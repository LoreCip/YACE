"""
Scompone la debolezza negli 'finali avanzati' (<6 pezzi non-Re) trovata da heldout_by_phase.py
per TIPO di materiale (pedoni, torri, pezzi minori, donne) — per capire ESATTAMENTE cosa
manca alla rete, non solo che il fenomeno esiste.

Decodifica il tipo di pezzo direttamente dagli indici HalfKP in 'us' (stessa formula di
MakeFeatureIndex in C++ / halfkp.feature_index):
    idx = square + (pIdx + kingSq*10) * 64
    pIdx = nnuePt*2 + relColor      (nnuePt: 0=pedone,1=cavallo,2=alfiere,3=torre,4=donna)
"""
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # moduli condivisi in NNUE/
from halfkp import DATA_DIR, PAD_IDX, RUNS_DIR, decode_feature, load_dataset, validation_split
from model import load_model, prob_to_cp, to_tensors

WEIGHTS = RUNS_DIR / "run_v6/checkpoints/halfkp_epoch_18_final.pth"  # = nnue_weights_v3-6.bin
BIN_DATASET = DATA_DIR / "clean_db_halfkp_balanced.bin"


def piece_type_counts(us):
    """[N, 5] numero di pezzi per tipo (pedone..donna, entrambi i colori) dagli indici 'us'."""
    _, piece_type, _, _ = decode_feature(us.astype(np.int64))
    piece_type = np.where(us != PAD_IDX, piece_type, -1)
    return np.stack([np.sum(piece_type == pt, axis=1) for pt in range(5)], axis=1).astype(np.int32)


device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
model = load_model(WEIGHTS, device)
dataset = load_dataset(BIN_DATASET)
num_positions = len(dataset)

rng = np.random.default_rng(42)  # condiviso con il campione di training piu' sotto
val_indices = np.sort(validation_split(num_positions, rng))
num_val = len(val_indices)

batch_size = 65536
all_pred_cp, all_target_cp = [], []
all_piece_counts = []  # [N,5] conteggio per tipo di pezzo (entrambi i colori sommati)

with torch.no_grad():
    for i in range(0, num_val, batch_size):
        idx = val_indices[i:i + batch_size]
        raw = dataset[idx]
        all_piece_counts.append(piece_type_counts(raw['us']))

        us_t, them_t, target_prob = to_tensors(raw, device)
        pred_prob = model(us_t, them_t).squeeze()

        all_pred_cp.append(prob_to_cp(pred_prob).cpu().numpy())
        all_target_cp.append(prob_to_cp(target_prob).cpu().numpy())

pred_cp = np.concatenate(all_pred_cp)
target_cp = np.concatenate(all_target_cp)
piece_counts = np.concatenate(all_piece_counts)  # colonne: pedone,cavallo,alfiere,torre,donna

total_pieces = piece_counts.sum(axis=1)
err = pred_cp - target_cp
abs_err = np.abs(err)
sign_match = (np.sign(pred_cp) == np.sign(target_cp))

sparse = total_pieces < 6
print(f"Posizioni con <6 pezzi non-Re nel held-out: {sparse.sum():,} "
      f"({sparse.sum()/len(sparse)*100:.2f}% del totale)\n")

def report(mask, label):
    n = mask.sum()
    if n < 50:
        print(f"  {label:<32} n={n:>7,}  (campione troppo piccolo, ignorato)")
        return
    print(f"  {label:<32} n={n:>7,}  MAE={abs_err[mask].mean():>7.2f}  "
          f"bias={err[mask].mean():>+7.2f}  SignAcc={sign_match[mask].mean()*100:>6.2f}%")

pawn, knight, bishop, rook, queen = (piece_counts[:, i] for i in range(5))
minors = knight + bishop

print("--- Finali avanzati (<6 pezzi) per TIPO di materiale presente ---")
report(sparse & (rook == 0) & (queen == 0) & (minors == 0) & (pawn > 0), "Solo pedoni (K+P vs K+P)")
report(sparse & (rook == 0) & (queen == 0) & (minors == 0) & (pawn == 0), "Solo Re (K vs K, teoricamente 0cp)")
report(sparse & (rook == 0) & (queen == 0) & (minors > 0), "Pezzi minori (+ eventuali pedoni)")
report(sparse & (rook > 0) & (queen == 0), "Torri (+ eventuali pedoni/minori)")
report(sparse & (queen > 0), "Donne in campo")

print("\n--- Stesse categorie, ma nel resto della partita (>=6 pezzi, per confronto) ---")
dense = ~sparse
report(dense & (rook == 0) & (queen == 0) & (minors == 0) & (pawn > 0), "Solo pedoni")
report(dense & (rook == 0) & (queen == 0) & (minors > 0) & (rook==0), "Pezzi minori dominanti (no torri/donne)")
report(dense & (rook > 0) & (queen == 0), "Torri (no donne)")
report(dense & (queen > 0), "Donne in campo")

print("\n--- Finali avanzati per NUMERO TOTALE di pezzi (granularita' fine) ---")
for n_pieces in range(0, 6):
    report(sparse & (total_pieces == n_pieces), f"Esattamente {n_pieces} pezzi non-Re")

print("\n--- Volume di dati di TRAINING per queste categorie (non solo held-out) ---")
print("(ri-uso la stessa decodifica su un campione ampio dell'intero dataset)")
sample_idx = np.sort(rng.choice(num_positions, 3_000_000, replace=False))
big_pawn = big_minor = big_rook = big_queen = big_kk = 0
for i in range(0, len(sample_idx), batch_size):
    idx = sample_idx[i:i + batch_size]
    counts = piece_type_counts(dataset[idx]['us'])
    tot = counts.sum(axis=1)
    sp = tot < 6
    pawn_, knight_, bishop_, rook_, queen_ = (counts[:, k] for k in range(5))
    minors_ = knight_ + bishop_
    big_pawn += np.sum(sp & (rook_==0) & (queen_==0) & (minors_==0) & (pawn_>0))
    big_kk += np.sum(sp & (rook_==0) & (queen_==0) & (minors_==0) & (pawn_==0))
    big_minor += np.sum(sp & (rook_==0) & (queen_==0) & (minors_>0))
    big_rook += np.sum(sp & (rook_>0) & (queen_==0))
    big_queen += np.sum(sp & (queen_>0))

total_sample = len(sample_idx)
print(f"  Campione: {total_sample:,} posizioni dall'intero dataset")
print(f"  Solo pedoni (finale avanzato):   {big_pawn:>8,} ({big_pawn/total_sample*100:.4f}%)")
print(f"  K vs K:                          {big_kk:>8,} ({big_kk/total_sample*100:.4f}%)")
print(f"  Pezzi minori (finale avanzato):  {big_minor:>8,} ({big_minor/total_sample*100:.4f}%)")
print(f"  Torri (finale avanzato):         {big_rook:>8,} ({big_rook/total_sample*100:.4f}%)")
print(f"  Donne (finale avanzato):         {big_queen:>8,} ({big_queen/total_sample*100:.4f}%)")
