"""Formato del dataset, codifica HalfKP e percorsi condivisi da dataset, training e analisi.

La codifica deve restare identica a quella del motore C++
(NnueEvaluator::MakeFeatureIndex in src/Evaluation/src/NNUE/NnueEvaluator.cpp):
    indice = casa + (tipo_pezzo*2 + colore_relativo + casa_re*10) * 64
con tipo_pezzo 0=pedone 1=cavallo 2=alfiere 3=torre 4=donna, colore_relativo 0 = stesso colore
della prospettiva, e scacchiera ribaltata (casa ^ 56) per la prospettiva del nero.
"""
import math
from pathlib import Path

import numpy as np

NNUE_DIR = Path(__file__).resolve().parent
DATA_DIR = NNUE_DIR / "data"
RUNS_DIR = NNUE_DIR / "runs"
WEIGHTS_DIR = NNUE_DIR.parent / "parameters" / "weights"

NUM_FEATURES = 40960      # 64 case del re x 10 (5 pezzi x 2 colori) x 64 case
PAD_IDX = NUM_FEATURES    # slot vuoto: riga extra dell'embedding, ignorata
SLOTS = 32                # indici per prospettiva in ogni riga del dataset
MAX_PIECES = 30           # pezzi non-re massimi (= MAX_ACTIVE_FEATURES in C++)
CP_SCALE = 400.0          # target = sigmoid(cp / CP_SCALE) (= NnueNetwork::CP_SCALE in C++)

# Una riga del dataset: indici della prospettiva di chi muove, dell'avversario, target in [0, 1]
DATASET_DTYPE = np.dtype([("us", np.uint16, SLOTS), ("them", np.uint16, SLOTS), ("target", np.float32)])

VAL_FRACTION = 0.02       # frazione del dataset tenuta fuori dal training
VAL_SEED = 42

PIECE_NAMES = ["Pedone", "Cavallo", "Alfiere", "Torre", "Donna"]
_NNUE_PIECE = {"p": 0, "n": 1, "b": 2, "r": 3, "q": 4}


def feature_index(square, piece_char, king_square, white_perspective):
    """Indice HalfKP di un pezzo (carattere FEN) vista dalla prospettiva indicata."""
    same_color = piece_char.isupper() == white_perspective
    p_idx = _NNUE_PIECE[piece_char.lower()] * 2 + (0 if same_color else 1)
    if not white_perspective:
        square, king_square = square ^ 56, king_square ^ 56
    return square + p_idx * 64 + king_square * 640


def decode_feature(idx):
    """Inverso di feature_index (anche su array numpy): (casa, tipo_pezzo, colore_relativo, casa_re)."""
    return idx % 64, (idx // 64) % 10 // 2, (idx // 64) % 2, idx // 640


def fen_to_features(fen):
    """Indici (us, them) di una FEN, ciascuno lungo SLOTS con padding; us = chi muove.
    None se manca un re o ci sono piu' di MAX_PIECES pezzi non-re."""
    board, side = fen.split(" ")[:2]
    kings, pieces = {}, []
    rank, file = 7, 0
    for char in board:
        if char == "/":
            rank, file = rank - 1, 0
        elif char.isdigit():
            file += int(char)
        else:
            if char in "Kk":
                kings[char] = rank * 8 + file
            else:
                pieces.append((char, rank * 8 + file))
            file += 1
    if len(kings) != 2 or len(pieces) > MAX_PIECES:
        return None

    white = [feature_index(sq, c, kings["K"], True) for c, sq in pieces]
    black = [feature_index(sq, c, kings["k"], False) for c, sq in pieces]
    us, them = (white, black) if side == "w" else (black, white)
    pad = [PAD_IDX] * (SLOTS - len(pieces))
    return us + pad, them + pad


def cp_to_target(cp):
    """Centipawn (punto di vista di chi muove) → target di training in [0, 1]."""
    return 1.0 / (1.0 + math.exp(-cp / CP_SCALE))


def load_dataset(path):
    return np.memmap(path, dtype=DATASET_DTYPE, mode="r")


def validation_split(num_positions, rng=None):
    """Indici dello split di validazione: sempre gli stessi per lo stesso dataset (seed fisso)."""
    rng = rng if rng is not None else np.random.default_rng(VAL_SEED)
    return rng.choice(num_positions, int(num_positions * VAL_FRACTION), replace=False)
