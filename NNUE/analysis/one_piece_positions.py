"""
Guarda da vicino le posizioni con esattamente 1 pezzo non-Re nel held-out (K+X vs K) dove la
rete sbaglia platealmente il segno — dovrebbero essere casi quasi sempre elementari (K+Donna
o K+Torre vs K quasi sempre vinti, K+pezzo minore da solo spesso patta) eppure la sign accuracy
misurata e' 8.98%, molto peggio del caso. Decodifica square/colore/tipo per capire cosa succede
davvero.
"""
import sys
from pathlib import Path

import numpy as np
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # moduli condivisi in NNUE/
from halfkp import DATA_DIR, PAD_IDX, PIECE_NAMES, RUNS_DIR, decode_feature, load_dataset, validation_split
from model import load_model, prob_to_cp, to_tensors

WEIGHTS = RUNS_DIR / "run_v6/checkpoints/halfkp_epoch_18_final.pth"  # = nnue_weights_v3-6.bin
BIN_DATASET = DATA_DIR / "clean_db_halfkp_balanced.bin"

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
model = load_model(WEIGHTS, device)
dataset = load_dataset(BIN_DATASET)
val_indices = np.sort(validation_split(len(dataset)))

raw = dataset[val_indices]
us = raw['us']
them = raw['them']
active_us = (us != PAD_IDX)
total_us = active_us.sum(axis=1)

mask = (total_us == 1)
sel_idx = np.where(mask)[0]
print(f"Trovate {len(sel_idx)} posizioni con esattamente 1 pezzo non-Re (lato 'us').\n")

us_sel = us[sel_idx]
them_sel = them[sel_idx]
target_sel = raw['target'][sel_idx]

with torch.no_grad():
    us_t, them_t, _ = to_tensors(raw[sel_idx], device)
    pred_prob = model(us_t, them_t).squeeze()
    pred_cp = prob_to_cp(pred_prob).cpu().numpy()
target_cp = prob_to_cp(torch.from_numpy(target_sel)).numpy()

def sq_name(sq):
    return "abcdefgh"[sq % 8] + str(sq // 8 + 1)

wrong_sign = 0
print(f"{'us_piece':<20}{'us_relcolor':<12}{'pred_cp':>9}{'target_cp':>10}{'segno_ok':>10}")
for i in range(min(30, len(sel_idx))):
    row_us = us_sel[i]
    active = row_us[row_us != PAD_IDX]
    idx_val = int(active[0])
    square, nnue_pt, rel_color, king_sq = decode_feature(idx_val)

    p, t = pred_cp[i], target_cp[i]
    ok = (np.sign(p) == np.sign(t))
    if not ok:
        wrong_sign += 1
    piece_label = f"{PIECE_NAMES[nnue_pt]}@{sq_name(square)}"
    color_label = "stesso colore di chi muove" if rel_color == 0 else "colore avversario"
    print(f"{piece_label:<20}{color_label:<28}{p:>9.1f}{t:>10.1f}{'OK' if ok else 'NO':>6}")

print(f"\nSu {min(30,len(sel_idx))} esempi mostrati: {wrong_sign} segni sbagliati")

# Rottura per tipo di pezzo e colore relativo, sull'INTERO campione (non solo i primi 30)
print("\n--- Sign accuracy per (tipo di pezzo, colore relativo a chi muove) ---")
pt_arr = np.zeros(len(sel_idx), dtype=int)
color_arr = np.zeros(len(sel_idx), dtype=int)
for i in range(len(sel_idx)):
    active = us_sel[i][us_sel[i] != PAD_IDX]
    _, nnue_pt, rel_color, _ = decode_feature(int(active[0]))
    pt_arr[i] = nnue_pt
    color_arr[i] = rel_color

sign_ok = (np.sign(pred_cp) == np.sign(target_cp))
for pt in range(5):
    for rc in range(2):
        m = (pt_arr == pt) & (color_arr == rc)
        n = m.sum()
        if n == 0:
            continue
        label = f"{PIECE_NAMES[pt]}, {'stesso colore' if rc==0 else 'colore avversario'}"
        print(f"  {label:<38} n={n:>5}  SignAcc={sign_ok[m].mean()*100:>6.2f}%  "
              f"pred medio={pred_cp[m].mean():>8.1f}  target medio={target_cp[m].mean():>8.1f}")
