import torch
import torch.nn as nn
import numpy as np
import math
import os

torch.set_float32_matmul_precision('high')

# --- 1. DEFINIZIONE DELLA RETE CON EMBEDDING (Allineata al C++) ---
class HalfKP_NNUE(nn.Module):
    def __init__(self, acc_size=256, layer_1=32):
        super(HalfKP_NNUE, self).__init__()
        self.accumulator = nn.Embedding(40961, acc_size, padding_idx=40960)
        self.accumulator_bias = nn.Parameter(torch.zeros(acc_size))
        
        self.layer1 = nn.Linear(acc_size * 2, layer_1)
        self.output_layer = nn.Linear(layer_1, 1)

    def clipped_relu(self, x):
        return torch.clamp(x, 0.0, 1.0)

    def forward(self, us_indices, them_indices):
        acc_us = self.accumulator(us_indices).sum(dim=1) + self.accumulator_bias
        acc_them = self.accumulator(them_indices).sum(dim=1) + self.accumulator_bias
        
        # L0: Clipped ReLU
        acc_us = self.clipped_relu(acc_us)
        acc_them = self.clipped_relu(acc_them)
        
        x = torch.cat([acc_us, acc_them], dim=1)
        
        # L1: Clipped ReLU (come in NnueNetwork.cpp -> ActivationFunction)
        x = self.clipped_relu(self.layer1(x))
        
        # L2: Output non attivato (Sigmoid solo per convertire in probabilità per il test)
        x = self.output_layer(x)
        return torch.sigmoid(x)

# --- 2. FUNZIONI DI SUPPORTO ---
def prob_to_cp(p):
    """Converte la probabilità di vittoria (0-1) in Centipawns (CP)."""
    p = max(1e-6, min(1.0 - 1e-6, p))
    # Estraiamo il Logit e lo scaliamo per 100, speculare a EvaluateNnue C++
    logit = math.log(p / (1.0 - p))
    return logit * 100.0

def fen_to_halfkp_indices(fen):
    """Converte un FEN in due liste di 32 indici HalfKP speculari al C++."""
    PAD_VAL = 40960
    
    parts = fen.split(' ', 2)
    board = parts[0]
    is_white_turn = (parts[1] == 'w')
    
    wk_sq = -1
    bk_sq = -1
    rank, file = 7, 0
    pieces = []
    
    # 1. Localizza i Re e salva i pezzi
    for char in board:
        if char == '/':
            rank -= 1
            file = 0
        elif char.isdigit():
            file += int(char)
        else:
            sq = rank * 8 + file
            if char == 'K':
                wk_sq = sq
            elif char == 'k':
                bk_sq = sq
            else:
                pieces.append((char, sq))
            file += 1
            
    if wk_sq == -1 or bk_sq == -1:
        raise ValueError("Il FEN fornito non contiene entrambi i Re!")

    # 2. Helper per replicare la logica: pIdx = nnuePt * 2 + relativeColor
    def get_p_idx(char, perspective_is_white):
        is_piece_white = char.isupper()
        pt_char = char.lower()
        # Mappatura PIECE_TO_NNUE_IDX del tuo file Constants.hpp
        nnue_pt = {'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4}[pt_char]
        relative_color = 0 if (is_piece_white == perspective_is_white) else 1
        return nnue_pt * 2 + relative_color
        
    us_indices = []
    them_indices = []
    
    # 3. Costruzione Indici per entrambe le prospettive
    for char, sq in pieces:
        # Prospettiva Bianco
        p_idx_w = get_p_idx(char, perspective_is_white=True)
        w_idx = sq + p_idx_w * 64 + wk_sq * 640
        
        # Prospettiva Nero (Flippata in verticale -> sq ^ 56)
        p_idx_b = get_p_idx(char, perspective_is_white=False)
        flipped_sq = sq ^ 56
        flipped_bk_sq = bk_sq ^ 56
        b_idx = flipped_sq + p_idx_b * 64 + flipped_bk_sq * 640
        
        if is_white_turn:
            us_indices.append(w_idx)
            them_indices.append(b_idx)
        else:
            us_indices.append(b_idx)
            them_indices.append(w_idx)
            
    # Padding per arrivare alla shape fissa di 32
    while len(us_indices) < 32: us_indices.append(PAD_VAL)
    while len(them_indices) < 32: them_indices.append(PAD_VAL)
    
    return torch.tensor([us_indices], dtype=torch.long), torch.tensor([them_indices], dtype=torch.long)

# --- 3. SCRIPT DI TEST PRINCIPALE ---
def test_model(weights_path, bin_path=None):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Caricamento modello HalfKP su {device}...")
    
    model = HalfKP_NNUE().to(device)
    if not os.path.exists(weights_path):
        print(f" Errore: File dei pesi '{weights_path}' non trovato!")
        return
        
    checkpoint = torch.load(weights_path, map_location=device, weights_only=False)
    if "model_state_dict" in checkpoint:
        model.load_state_dict(checkpoint["model_state_dict"])
    else:
        clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in checkpoint.items()}
        model.load_state_dict(clean_state_dict)
        
    model.eval()
    print(" Modello caricato con successo!\n")

    # ====================================================
    # FASE 1: TEST RANDOM DAL DATASET BINARIO
    # ====================================================
    if bin_path and os.path.exists(bin_path):
        print("--- Estrazione 10 posizioni casuali dal Dataset Binario ---")
        struttura_dati = np.dtype([('us', np.uint16, 32), ('them', np.uint16, 32), ('target', np.float32)])
        dataset = np.memmap(bin_path, dtype=struttura_dati, mode='r')
        
        random_indices = np.random.randint(0, len(dataset), 10)
        
        with torch.no_grad():
            for i, idx in enumerate(random_indices):
                raw_pos = dataset[idx]
                
                us_tensor = torch.from_numpy(raw_pos['us'].astype(np.int64)).unsqueeze(0).to(device)
                them_tensor = torch.from_numpy(raw_pos['them'].astype(np.int64)).unsqueeze(0).to(device)
                
                pred_prob = model(us_tensor, them_tensor).item()
                real_prob = raw_pos['target']
                
                pred_cp = prob_to_cp(pred_prob)
                real_cp = prob_to_cp(real_prob)
                
                print(f"Posizione {i+1}:")
                print(f"  -> Reale (Target): {real_cp:+.2f} CP  (Win Prob: {real_prob:.3f})")
                print(f"  -> Previsione:     {pred_cp:+.2f} CP  (Win Prob: {pred_prob:.3f})")
                print(f"  -> Delta Errore:   {abs(real_cp - pred_cp):.2f} CP\n")

    # ====================================================
    # FASE 2: TEST MANUALE INTERATTIVO DA STRINGA FEN
    # ====================================================
    print("-" * 50)
    print(" MODALITÀ INTERATTIVA YACE HALF-KP NNUE")
    print("Incolla un FEN valido per intero (Es: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1').")
    print("Scrivi 'q' per uscire.")
    print("-" * 50)
    
    with torch.no_grad():
        while True:
            fen = input("\nFEN > ").strip()
            if fen.lower() in ['q', 'quit', 'exit']:
                break
            if not fen:
                continue
                
            try:
                us_tensor, them_tensor = fen_to_halfkp_indices(fen)
                us_tensor = us_tensor.to(device)
                them_tensor = them_tensor.to(device)
                
                output_prob = model(us_tensor, them_tensor).item()
                
                turn = fen.split(' ')[1]
                player = "Bianco" if turn == 'w' else "Nero"
                
                cp_relative = prob_to_cp(output_prob)
                cp_absolute = cp_relative if turn == 'w' else -cp_relative
                
                print(f"[{player} al tratto]")
                print(f"Valutazione Assoluta (Bianco): {cp_absolute / 100.0:+.2f} pedoni (CP: {cp_absolute:+.1f})")
                print(f"Prob. di vittoria del {player}: {output_prob * 100:.1f}%")
                
            except Exception as e:
                print(f" Errore durante l'analisi: {e}")

if __name__ == "__main__":
    WEIGHTS = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/training/weights/halfkp_epoch15_v1.pth" 
    BIN_DATASET = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp.bin"
    
    test_model(WEIGHTS, BIN_DATASET)