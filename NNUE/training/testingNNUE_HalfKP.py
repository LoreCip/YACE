import torch
import torch.nn as nn
import numpy as np
import math
import os

# --- 1. DEFINIZIONE DELLA RETE CON EMBEDDING (Identica al training) ---
class HalfKP_NNUE(nn.Module):
    def __init__(self, acc_size=256, layer_1=32, layer_2=32):
        super(HalfKP_NNUE, self).__init__()
        self.accumulator = nn.Embedding(40961, acc_size, padding_idx=40960)
        self.accumulator_bias = nn.Parameter(torch.zeros(acc_size))
        self.layer1 = nn.Linear(acc_size * 2, layer_1)
        self.layer2 = nn.Linear(layer_1, layer_2)
        self.output_layer = nn.Linear(layer_2, 1)

    def clipped_relu(self, x):
        return torch.clamp(x, 0.0, 1.0)

    def forward(self, us_indices, them_indices):
        acc_us = self.accumulator(us_indices).sum(dim=1) + self.accumulator_bias
        acc_them = self.accumulator(them_indices).sum(dim=1) + self.accumulator_bias
        
        acc_us = self.clipped_relu(acc_us)
        acc_them = self.clipped_relu(acc_them)
        
        x = torch.cat([acc_us, acc_them], dim=1)
        x = torch.relu(self.layer1(x))
        x = torch.relu(self.layer2(x))
        x = self.output_layer(x)
        return torch.sigmoid(x)

# --- 2. FUNZIONI DI SUPPORTO ---
def prob_to_cp(p):
    """Converte la probabilità di vittoria (0-1) in Centipawns (CP)."""
    p = max(1e-6, min(1.0 - 1e-6, p))
    return -400.0 * math.log((1.0 / p) - 1.0)

def fen_to_halfkp_indices(fen):
    """Converte un FEN in due liste di 32 indici HalfKP per l'inferenza."""
    piece_map = {
        'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4,
        'p': 5, 'n': 6, 'b': 7, 'r': 8, 'q': 9
    }
    PAD_VAL = 40960
    
    parts = fen.split(' ', 2)
    board = parts[0]
    is_white_turn = (parts[1] == 'w')
    
    wk_sq = -1
    bk_sq = -1
    rank, file = 7, 0
    pieces = []
    
    # Primo passaggio: localizza i Re e mappa i pezzi rimasti
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
                pieces.append((piece_map[char], sq))
            file += 1
            
    if wk_sq == -1 or bk_sq == -1:
        raise ValueError("Il FEN fornito non contiene entrambi i Re!")
        
    us_indices = []
    them_indices = []
    
    # Secondo passaggio: calcola gli indici relativi basati sul Re
    for p_idx, sq in pieces:
        w_idx = p_idx * 4096 + wk_sq * 64 + sq
        
        mirrored_bk = bk_sq ^ 56
        mirrored_sq = sq ^ 56
        p_idx_b = (p_idx + 5) % 10
        b_idx = p_idx_b * 4096 + mirrored_bk * 64 + mirrored_sq
        
        if is_white_turn:
            us_indices.append(w_idx)
            them_indices.append(b_idx)
        else:
            us_indices.append(b_idx)
            them_indices.append(w_idx)
            
    # Riempimento obbligatorio (Padding) per arrivare a forma [1, 32]
    while len(us_indices) < 32: us_indices.append(PAD_VAL)
    while len(them_indices) < 32: them_indices.append(PAD_VAL)
    
    # Ritorniamo i tensori con dimensione Batch aggiunta (1, 32)
    return torch.tensor([us_indices], dtype=torch.long), torch.tensor([them_indices], dtype=torch.long)

# --- 3. SCRIPT DI TEST PRINCIPALE ---
def test_model(weights_path, bin_path=None):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Caricamento modello HalfKP su {device}...")
    
    model = HalfKP_NNUE().to(device)
    if not os.path.exists(weights_path):
        print(f" Errore: File dei pesi '{weights_path}' non trovato!")
        return
        
    # Risoluzione compatibilità pesi compilati o completi
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
        print("--- Estrazione 20 posizioni casuali dal Dataset Binario HalfKP ---")
        # NOTA: Usiamo np.uint16 coerentemente con il nuovo file binario
        struttura_dati = np.dtype([('us', np.uint16, 32), ('them', np.uint16, 32), ('target', np.float32)])
        dataset = np.memmap(bin_path, dtype=struttura_dati, mode='r')
        
        random_indices = np.random.randint(0, len(dataset), 20)
        
        with torch.no_grad():
            for i, idx in enumerate(random_indices):
                raw_pos = dataset[idx]
                
                # Convertiamo direttamente in tensori int64 per l'embedding
                us_tensor = torch.from_numpy(raw_pos['us'].astype(np.int64)).unsqueeze(0).to(device)
                them_tensor = torch.from_numpy(raw_pos['them'].astype(np.int64)).unsqueeze(0).to(device)
                
                pred_prob = model(us_tensor, them_tensor).item()
                real_prob = raw_pos['target']
                
                pred_cp = prob_to_cp(pred_prob)
                real_cp = prob_to_cp(real_prob)
                
                print(f"Posizione {i+1}:")
                print(f"  -> Reale: {real_cp:+.2f} CP  (Win Prob: {real_prob:.3f})")
                print(f"  -> Rete:  {pred_cp:+.2f} CP  (Win Prob: {pred_prob:.3f})")
                print(f"  -> Errore: {abs(real_cp - pred_cp):.2f} CP\n")

    # ====================================================
    # FASE 2: TEST MANUALE INTERATTIVO DA STRINGA FEN
    # ====================================================
    print("-" * 50)
    print(" MODALITÀ INTERATTIVA YACE HALF-KP NNUE")
    print("Incolla un FEN valido per intero.")
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
    WEIGHTS = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/checkpoints/V2_HalfKP/Run_2/halfkp_epoch_3_final.pth" 
    BIN_DATASET = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp.bin"
    
    test_model(WEIGHTS, BIN_DATASET)