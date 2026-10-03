"""
Guarda da vicino le posizioni con esattamente 1 pezzo non-Re nel held-out (K+X vs K) dove la
rete sbaglia platealmente il segno — dovrebbero essere casi quasi sempre elementari (K+Donna
o K+Torre vs K quasi sempre vinti, K+pezzo minore da solo spesso patta) eppure la sign accuracy
misurata e' 8.98%, molto peggio del caso. Decodifica square/colore/tipo per capire cosa succede
davvero.
"""
import torch
import numpy as np

from NNUE import HalfKP_NNUE_QAT
from testingNNUE_HalfKP import prob_to_cp_tensor

WEIGHTS = "./run_v6/checkpoints/halfkp_epoch_18_final.pth"
BIN_DATASET = "../data/clean_db_halfkp_balanced.bin"
VAL_FRACTION = 0.02
PAD_VAL = 40960
PIECE_NAMES = ["Pedone", "Cavallo", "Alfiere", "Torre", "Donna"]

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
model = HalfKP_NNUE_QAT().to(device)
checkpoint = torch.load(WEIGHTS, map_location=device, weights_only=False)
state_dict = checkpoint.get("model_state_dict", checkpoint)
clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in state_dict.items()}
model.load_state_dict(clean_state_dict)
model.eval()

struttura_dati = np.dtype([('us', np.uint16, 32), ('them', np.uint16, 32), ('target', np.float32)])
dataset = np.memmap(BIN_DATASET, dtype=struttura_dati, mode='r')
num_positions = len(dataset)

rng = np.random.default_rng(42)
num_val = int(num_positions * VAL_FRACTION)
val_indices = np.sort(rng.choice(num_positions, num_val, replace=False))

raw = dataset[val_indices]
us = raw['us']
them = raw['them']
active_us = (us != PAD_VAL)
total_us = active_us.sum(axis=1)

mask = (total_us == 1)
sel_idx = np.where(mask)[0]
print(f"Trovate {len(sel_idx)} posizioni con esattamente 1 pezzo non-Re (lato 'us').\n")

us_sel = us[sel_idx]
them_sel = them[sel_idx]
target_sel = raw['target'][sel_idx]

with torch.no_grad():
    us_t = torch.from_numpy(us_sel.astype(np.int64)).to(device)
    them_t = torch.from_numpy(them_sel.astype(np.int64)).to(device)
    pred_prob = model(us_t, them_t).squeeze()
    pred_cp = prob_to_cp_tensor(pred_prob).cpu().numpy()
target_cp = np.array([np.log(max(1e-6, min(1-1e-6, t)) / (1 - max(1e-6, min(1-1e-6, t)))) * 400.0 for t in target_sel])

def decode_index(idx):
    """idx = square + (pIdx + kingSq*10)*64 ; pIdx = nnuePt*2 + relColor"""
    square = idx % 64
    rest = idx // 64
    p_idx = rest % 10
    king_sq = rest // 10
    nnue_pt = p_idx // 2
    rel_color = p_idx % 2
    return square, nnue_pt, rel_color, king_sq

def sq_name(sq):
    return "abcdefgh"[sq % 8] + str(sq // 8 + 1)

wrong_sign = 0
print(f"{'us_piece':<20}{'us_relcolor':<12}{'pred_cp':>9}{'target_cp':>10}{'segno_ok':>10}")
for i in range(min(30, len(sel_idx))):
    row_us = us_sel[i]
    active = row_us[row_us != PAD_VAL]
    idx_val = int(active[0])
    square, nnue_pt, rel_color, king_sq = decode_index(idx_val)

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
    active = us_sel[i][us_sel[i] != PAD_VAL]
    _, nnue_pt, rel_color, _ = decode_index(int(active[0]))
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
