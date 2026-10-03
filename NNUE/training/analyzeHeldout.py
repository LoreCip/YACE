"""
Analisi approfondita del modello V3-6 sul VERO split di validazione (mai visto in training,
stesso seed/metodo di trainingNNUE_HalfKP.py) — confronto con le valutazioni di Stockfish
(il target del dataset e' sigmoid(cp/400), dove cp viene dalle analisi Stockfish del dump
Lichess, vedi cleanDataset.py). Stratifica l'errore per fase di gioco (materiale non-Re) e
per quanto la posizione sia decisiva (|cp| del target), per capire DOVE la rete diverge di
piu' dal giudizio di Stockfish, non solo di quanto in media.
"""
import torch
import numpy as np
import math

from NNUE import HalfKP_NNUE_QAT
from testingNNUE_HalfKP import prob_to_cp_tensor

WEIGHTS = "./run_v6/checkpoints/halfkp_epoch_18_final.pth"
BIN_DATASET = "../data/clean_db_halfkp_balanced.bin"
VAL_FRACTION = 0.02
PAD_VAL = 40960

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
print(f"Device: {device}")

model = HalfKP_NNUE_QAT().to(device)
checkpoint = torch.load(WEIGHTS, map_location=device, weights_only=False)
state_dict = checkpoint.get("model_state_dict", checkpoint)
clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in state_dict.items()}
model.load_state_dict(clean_state_dict)
model.eval()

struttura_dati = np.dtype([('us', np.uint16, 32), ('them', np.uint16, 32), ('target', np.float32)])
dataset = np.memmap(BIN_DATASET, dtype=struttura_dati, mode='r')
num_positions = len(dataset)
print(f"Dataset totale: {num_positions:,} posizioni.")

rng = np.random.default_rng(42)
num_val = int(num_positions * VAL_FRACTION)
val_indices = rng.choice(num_positions, num_val, replace=False)
print(f"Split di validazione (mai visto in training): {num_val:,} posizioni.")

batch_size = 65536
all_pred_cp = []
all_target_cp = []
all_nonking_pieces = []  # proxy di fase: n. di feature attive (non-PAD) in 'us'

with torch.no_grad():
    for i in range(0, num_val, batch_size):
        idx = val_indices[i:i + batch_size]
        idx_sorted = np.sort(idx)  # letture sequenziali piu' veloci sul memmap
        raw = dataset[idx_sorted]

        us = raw['us']
        nonking = np.sum(us != PAD_VAL, axis=1)
        all_nonking_pieces.append(nonking)

        us_t = torch.from_numpy(us.astype(np.int64)).to(device)
        them_t = torch.from_numpy(raw['them'].astype(np.int64)).to(device)
        target_prob = torch.from_numpy(raw['target'].astype(np.float32)).to(device)

        pred_prob = model(us_t, them_t).squeeze()

        pred_cp = prob_to_cp_tensor(pred_prob)
        target_cp = prob_to_cp_tensor(target_prob)

        all_pred_cp.append(pred_cp.cpu().numpy())
        all_target_cp.append(target_cp.cpu().numpy())

pred_cp = np.concatenate(all_pred_cp)
target_cp = np.concatenate(all_target_cp)
nonking_pieces = np.concatenate(all_nonking_pieces)

err = pred_cp - target_cp
abs_err = np.abs(err)
# Nessuna scappatoia per i target esattamente 0.5 (cp=0, ~3.8% del dataset): stesso identico
# criterio usato in trainingNNUE_HalfKP.py:evaluate_holdout (sign(pred-0.5)==sign(target-0.5)),
# equivalente a sign(pred_cp)==sign(target_cp) su float continui — un OR di "credito automatico"
# per i pareggi esatti gonfiava artificialmente la sign accuracy di ~3.7 punti percentuali.
sign_match = (np.sign(pred_cp) == np.sign(target_cp))

print("\n" + "=" * 60)
print(f" RISULTATI SUL VERO HELD-OUT ({num_val:,} posizioni, seed=42)")
print("=" * 60)
print(f"MAE CP:          {abs_err.mean():.2f}")
print(f"Bias medio (pred-target): {err.mean():+.2f} (vicino a 0 = nessuna distorsione sistematica)")
print(f"Std dev errore:  {err.std():.2f}")
print(f"Sign Accuracy:   {sign_match.mean()*100:.2f}%")
print(f"Correlazione di Pearson (pred vs target): {np.corrcoef(pred_cp, target_cp)[0,1]:.4f}")

def report_bucket(mask, label):
    n = mask.sum()
    if n == 0:
        return
    print(f"  {label:<28} n={n:>9,}  MAE={abs_err[mask].mean():>7.2f}  "
          f"bias={err[mask].mean():>+7.2f}  SignAcc={sign_match[mask].mean()*100:>6.2f}%")

print("\n--- Per fase di gioco (n. pezzi non-Re attivi su 30 max) ---")
report_bucket(nonking_pieces >= 24, "Apertura (>=24 pezzi)")
report_bucket((nonking_pieces >= 14) & (nonking_pieces < 24), "Mediogioco (14-23 pezzi)")
report_bucket((nonking_pieces >= 6) & (nonking_pieces < 14), "Finale (6-13 pezzi)")
report_bucket(nonking_pieces < 6, "Finale avanzato (<6 pezzi)")

print("\n--- Per quanto e' decisiva la posizione (|target cp|) ---")
abs_target = np.abs(target_cp)
report_bucket(abs_target < 50, "Quasi patta (<50cp)")
report_bucket((abs_target >= 50) & (abs_target < 150), "Leggero vantaggio (50-150cp)")
report_bucket((abs_target >= 150) & (abs_target < 400), "Vantaggio chiaro (150-400cp)")
report_bucket(abs_target >= 400, "Decisiva (>=400cp)")

print("\n--- Le posizioni con l'errore piu' grande (top 10) ---")
worst = np.argsort(-abs_err)[:10]
for i in worst:
    print(f"  pred={pred_cp[i]:>8.1f}  target={target_cp[i]:>8.1f}  err={err[i]:>8.1f}  "
          f"non-king pieces={nonking_pieces[i]}")
