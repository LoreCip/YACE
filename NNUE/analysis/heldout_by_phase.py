"""
Analisi approfondita del modello V3-6 sul VERO split di validazione (mai visto in training,
stesso seed/metodo di training/train.py) — confronto con le valutazioni di Stockfish
(il target del dataset e' sigmoid(cp/400), dove cp viene dalle analisi Stockfish del dump
Lichess, vedi dataset/build_dataset.py). Stratifica l'errore per fase di gioco (materiale non-Re) e
per quanto la posizione sia decisiva (|cp| del target), per capire DOVE la rete diverge di
piu' dal giudizio di Stockfish, non solo di quanto in media.
"""
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # moduli condivisi in NNUE/
from halfkp import DATA_DIR, PAD_IDX, RUNS_DIR, load_dataset, validation_split
from model import load_model, prob_to_cp, to_tensors

WEIGHTS = RUNS_DIR / "run_v6/checkpoints/halfkp_epoch_18_final.pth"  # = nnue_weights_v3-6.bin
BIN_DATASET = DATA_DIR / "clean_db_halfkp_balanced.bin"

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
print(f"Device: {device}")

model = load_model(WEIGHTS, device)
dataset = load_dataset(BIN_DATASET)
num_positions = len(dataset)
print(f"Dataset totale: {num_positions:,} posizioni.")

val_indices = validation_split(num_positions)
num_val = len(val_indices)
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

        all_nonking_pieces.append(np.sum(raw['us'] != PAD_IDX, axis=1))

        us_t, them_t, target_prob = to_tensors(raw, device)

        pred_prob = model(us_t, them_t).squeeze()

        pred_cp = prob_to_cp(pred_prob)
        target_cp = prob_to_cp(target_prob)

        all_pred_cp.append(pred_cp.cpu().numpy())
        all_target_cp.append(target_cp.cpu().numpy())

pred_cp = np.concatenate(all_pred_cp)
target_cp = np.concatenate(all_target_cp)
nonking_pieces = np.concatenate(all_nonking_pieces)

err = pred_cp - target_cp
abs_err = np.abs(err)
# Nessuna scappatoia per i target esattamente 0.5 (cp=0, ~3.8% del dataset): stesso identico
# criterio usato in training/train.py:evaluate_holdout (sign(pred-0.5)==sign(target-0.5)),
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
