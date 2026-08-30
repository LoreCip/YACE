import os

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim

import numpy as np

from tqdm import tqdm

from NNUE import FakeQuantizeSTE, HalfKP_NNUE_QAT

torch.set_float32_matmul_precision('high')

# Target di training = sigmoid(cp/400) (vedi NNUE/data/scripts/cleanDataset.py), quindi
# l'inverso per ricavare il target in "logit-space" usa la stessa scala.
CP_SCALE = 400.0

# Peso della componente in logit-space nella loss combinata (il resto va alla componente
# in probability-space/WDL). Il logit-space evita la saturazione del sigmoide per le
# posizioni nettamente vinte/perse, dove la loss su probabilità pura ha gradiente quasi nullo.
LOGIT_LOSS_WEIGHT = 0.5

VAL_FRACTION = 0.02  # frazione del dataset riservata a validazione (mai vista in training)


def targets_to_logit(targets, eps=1e-6):
    t = torch.clamp(targets, eps, 1.0 - eps)
    return torch.log(t / (1.0 - t))


def blended_loss(pred_prob, pred_logit, targets):
    target_logit = targets_to_logit(targets)
    loss_wdl = F.mse_loss(pred_prob, targets)
    loss_logit = F.mse_loss(pred_logit, target_logit)
    return (1.0 - LOGIT_LOSS_WEIGHT) * loss_wdl + LOGIT_LOSS_WEIGHT * loss_logit, loss_wdl, loss_logit


@torch.no_grad()
def evaluate_holdout(model, dataset, val_indices, device, batch_size=16384, max_samples=500_000):
    model.eval()
    n = min(len(val_indices), max_samples)
    idx = np.random.choice(val_indices, n, replace=False)
    idx.sort()

    mae_wdl_sum = 0.0
    mae_cp_sum = 0.0
    sign_correct = 0

    for i in range(0, n, batch_size):
        batch_idx = idx[i:i + batch_size]
        raw = dataset[batch_idx]

        us = torch.from_numpy(raw['us'].astype(np.int64)).to(device)
        them = torch.from_numpy(raw['them'].astype(np.int64)).to(device)
        targets = torch.from_numpy(raw['target'].astype(np.float32)).to(device)

        pred_prob = model(us, them).squeeze(-1)

        mae_wdl_sum += torch.abs(pred_prob - targets).sum().item()

        pred_cp = targets_to_logit(pred_prob) * CP_SCALE
        real_cp = targets_to_logit(targets) * CP_SCALE
        mae_cp_sum += torch.abs(pred_cp - real_cp).sum().item()

        sign_correct += (torch.sign(pred_prob - 0.5) == torch.sign(targets - 0.5)).sum().item()

    model.train()
    return {
        "mae_wdl": mae_wdl_sum / n,
        "mae_cp": mae_cp_sum / n,
        "sign_accuracy": 100.0 * sign_correct / n,
        "n": n,
    }


def train_nnue(
        bin_path,
        num_epochs=15,
        batch_size=16384,
        learning_rate=0.001,
        resume_from=None,
        max_rows=None,
    ):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Sto addestrando HalfKP su: {device}")

    struttura_dati = np.dtype([
        ('us', np.uint16, 32),
        ('them', np.uint16, 32),
        ('target', np.float32)
    ])

    if max_rows is None:
        print(f"Caricamento del dataset in RAM da {bin_path}...")
        dataset = np.fromfile(bin_path, dtype=struttura_dati)
    else:
        # Modalita' "quick probe": carica solo un sottoinsieme casuale di max_rows righe
        # (via memmap, senza leggere l'intero file) per confrontare rapidamente varianti
        # del dataset (es. diversi parametri di bilanciamento) con un training breve,
        # prima di impegnarsi in una run completa su tutto il file.
        print(f"[QUICK PROBE] Caricamento di un sottoinsieme casuale di {max_rows:,} righe da {bin_path}...")
        mm = np.memmap(bin_path, dtype=struttura_dati, mode='r')
        probe_rng = np.random.default_rng(123)
        probe_idx = probe_rng.choice(len(mm), min(max_rows, len(mm)), replace=False)
        probe_idx.sort()
        dataset = np.array(mm[probe_idx])
    num_positions = len(dataset)

    # --- SPLIT TRAIN/VALIDATION ---
    # Riserviamo un sottoinsieme casuale (seed fisso, riproducibile) come validation set, mai
    # visto in training, cosi' le metriche riportate (qui e in testingNNUE_HalfKP.py) sono una
    # vera misura di generalizzazione e non contaminate dal training stesso.
    # NOTA: uno split "in coda" (prime N righe train, ultime M righe val) e' stato provato prima
    # ma scartato: il file non e' i.i.d. per posizione (cleanDataset.py/balanceDataset.py lo
    # processano preservando l'ordine originale dello stream Lichess), quindi la coda del file
    # e' una fetta con una distribuzione di target sistematicamente diversa (piu' stretta, target
    # piu' vicini a 0.5) dal resto — dava un sign-accuracy pessimistico e fuorviante (~54% invece
    # di ~63%) che sembrava un plateau/overfitting ma era solo un artefatto dello split.
    num_val = int(num_positions * VAL_FRACTION)
    rng = np.random.default_rng(42)
    val_indices = rng.choice(num_positions, num_val, replace=False)
    val_mask = np.zeros(num_positions, dtype=bool)
    val_mask[val_indices] = True
    train_indices = np.nonzero(~val_mask)[0]
    num_train = len(train_indices)

    num_batches = num_train // batch_size
    print(f"Dataset caricato: {num_positions:,} posizioni totali "
          f"({num_train:,} train / {num_val:,} validation, {VAL_FRACTION*100:.1f}%).")

    model = HalfKP_NNUE_QAT().to(device)
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)

    global_step = 0
    start_epoch = 0
    checkpoint = None

    if resume_from and os.path.exists(resume_from):
        checkpoint = torch.load(resume_from, map_location=device, weights_only=False)
        model.load_state_dict(checkpoint["model_state_dict"])
        optimizer.load_state_dict(checkpoint["optimizer_state_dict"])
        start_epoch = checkpoint.get("epoch", 0)
        global_step = checkpoint.get("global_step", 0)
        print(f"[INFO] Ripristinato: Epoca {start_epoch}, Step {global_step}")

    # Un solo scheduler OneCycleLR per l'intera run, dimensionato sulle epoche RIMANENTI cosi'
    # che il ciclo (warmup + anneal) si completi correttamente anche riprendendo da un
    # checkpoint, invece di essere tagliato o rifatto da zero (bug precedente: veniva sempre
    # dimensionato su num_epochs invece che su num_epochs - start_epoch, e lo stato non veniva
    # mai ripristinato da resume_from).
    remaining_epochs = max(1, num_epochs - start_epoch)
    scheduler = optim.lr_scheduler.OneCycleLR(
        optimizer,
        max_lr=learning_rate,
        steps_per_epoch=num_batches,
        epochs=remaining_epochs,
        pct_start=0.1,
        div_factor=10.0,
        final_div_factor=100.0
    )
    if checkpoint is not None and "scheduler_state_dict" in checkpoint:
        scheduler.load_state_dict(checkpoint["scheduler_state_dict"])

    if device.type == 'cuda':
        model = torch.compile(model)

    os.makedirs("checkpoints", exist_ok=True)

    for epoch in range(start_epoch, num_epochs):
        model.train()
        print(f"\n--- Epoca {epoch+1}/{num_epochs} ---")

        shuffle_indices = torch.randperm(num_train)
        running_loss = 0.0
        current_lr = optimizer.param_groups[0]['lr']

        progress_bar = tqdm(
            range(num_batches),
            desc=f"Epoca {epoch+1}/{num_epochs} [LR: {current_lr:.6f}]",
            dynamic_ncols=True,
            leave=True
        )

        for i in progress_bar:
            perm = shuffle_indices[i * batch_size : (i+1) * batch_size].numpy()
            batch_idx = train_indices[perm]
            raw_batch = dataset[batch_idx]

            us_indices = torch.from_numpy(raw_batch['us'].astype(np.int64)).to(device)
            them_indices = torch.from_numpy(raw_batch['them'].astype(np.int64)).to(device)
            targets = torch.from_numpy(raw_batch['target'].astype(np.float32)).to(device)

            optimizer.zero_grad(set_to_none=True)

            pred_prob, pred_logit = model(us_indices, them_indices, return_logits=True)
            loss, loss_wdl, loss_logit = blended_loss(pred_prob.squeeze(-1), pred_logit.squeeze(-1), targets)
            loss.backward()
            optimizer.step()
            scheduler.step()

            running_loss += loss.item()
            global_step += 1

            current_avg_loss = running_loss / (i + 1)

            progress_bar.set_postfix_str(
                f"Avg_Loss: {current_avg_loss:.4f} | WDL: {loss_wdl.item():.4f} | Logit: {loss_logit.item():.4f}"
            )

        avg_epoch_loss = running_loss / num_batches

        val_metrics = evaluate_holdout(model, dataset, val_indices, device, batch_size=batch_size)
        print(
            f" Val (held-out, mai vista in training): "
            f"MAE_WDL={val_metrics['mae_wdl']:.4f} | "
            f"MAE_CP={val_metrics['mae_cp']:.2f} | "
            f"SignAcc={val_metrics['sign_accuracy']:.2f}% "
            f"(su {val_metrics['n']:,} posizioni)"
        )

        # --- CHECKPOINT ---
        raw_model = model._orig_mod if hasattr(model, '_orig_mod') else model
        torch.save({
            'epoch': epoch + 1,
            'global_step': global_step,
            'model_state_dict': raw_model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'scheduler_state_dict': scheduler.state_dict(),
            'loss': avg_epoch_loss,
            'val_metrics': val_metrics,
        }, f"checkpoints/halfkp_epoch_{epoch+1}_final.pth")

        print(f" Checkpoint salvato.")

if __name__ == "__main__":
    train_nnue(
        bin_path="/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp_balanced.bin",
        num_epochs=40,
        batch_size=16384,
        learning_rate=0.001,
        resume_from=None
    )
