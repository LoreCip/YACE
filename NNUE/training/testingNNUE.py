import torch
import torch.nn as nn
import numpy as np
import math
import os

# --- 1. DEFINIZIONE DELLA RETE (Deve essere identica al training) ---
class BasicNNUE(nn.Module):
    def __init__(self, acc_size=256, layer_1=32, layer_2=32):
        super(BasicNNUE, self).__init__()
        self.accumulator = nn.Linear(768, acc_size)
        self.layer1 = nn.Linear(acc_size * 2, layer_1)
        self.layer2 = nn.Linear(layer_1, layer_2)
        self.output_layer = nn.Linear(layer_2, 1)

    def clipped_relu(self, x):
        return torch.clamp(x, 0.0, 1.0)

    def forward(self, us_features, them_features):
        acc_us = self.clipped_relu(self.accumulator(us_features))
        acc_them = self.clipped_relu(self.accumulator(them_features))
        
        x = torch.cat([acc_us, acc_them], dim=1)
        x = self.clipped_relu(self.layer1(x))
        x = self.clipped_relu(self.layer2(x))
        x = self.output_layer(x)
        
        return torch.sigmoid(x)

# --- 2. FUNZIONI DI SUPPORTO ---
def prob_to_cp(p):
    """Converte la probabilità di vittoria (0-1) in Centipawns (CP)."""
    # Evitiamo divisioni per zero o logaritmi di zero clampando i valori
    p = max(1e-6, min(1.0 - 1e-6, p))
    return -400.0 * math.log((1.0 / p) - 1.0)

def fen_to_tensors(fen):
    """Converte un FEN testuale in due tensori 1x768 per l'inferenza singola."""
    piece_map = {
        'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
        'p': 6, 'n': 7, 'b': 8, 'r': 9, 'q': 10, 'k': 11
    }
    us_tensor = torch.zeros(1, 768)
    them_tensor = torch.zeros(1, 768)
    
    parts = fen.split(' ', 2)
    board = parts[0]
    is_white_turn = (parts[1] == 'w')
    
    rank, file = 7, 0
    for char in board:
        if char == '/':
            rank -= 1
            file = 0
        elif char.isdigit():
            file += int(char)
        else:
            sq = rank * 8 + file
            p_idx = piece_map[char]
            
            us_idx = p_idx * 64 + sq
            mirrored_sq = sq ^ 56
            mirrored_p_idx = p_idx + 6 if p_idx < 6 else p_idx - 6
            them_idx = mirrored_p_idx * 64 + mirrored_sq
            
            if is_white_turn:
                us_tensor[0, us_idx] = 1.0
                them_tensor[0, them_idx] = 1.0
            else:
                them_tensor[0, us_idx] = 1.0
                us_tensor[0, them_idx] = 1.0
            
            file += 1
            
    return us_tensor, them_tensor

# --- 3. SCRIPT DI TEST PRINCIPALE ---
def test_model(weights_path, bin_path=None):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Caricamento modello su {device}...")
    
    model = BasicNNUE().to(device)
    if not os.path.exists(weights_path):
        print(f" Errore: File dei pesi '{weights_path}' non trovato!")
        return
        
    checkpoint = torch.load(weights_path, map_location=device, weights_only=False)
    if "model_state_dict" in checkpoint:
        model.load_state_dict(checkpoint["model_state_dict"])
    else:
        state_dict = checkpoint
        clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in state_dict.items()}
        model.load_state_dict(clean_state_dict)
        
    model.eval()
    print(" Modello caricato con successo!\n")

    # ====================================================
    # FASE 1: TEST RANDOM DAL FILE BINARIO (Se fornito)
    # ====================================================
    if bin_path and os.path.exists(bin_path):
        print("--- Estrazione 5 posizioni casuali dal Dataset Binario ---")
        struttura_dati = np.dtype([('us', np.int16, 32), ('them', np.int16, 32), ('target', np.float32)])
        dataset = np.memmap(bin_path, dtype=struttura_dati, mode='r')
        
        random_indices = np.random.randint(0, len(dataset), 5)
        
        with torch.no_grad(): # Nessun gradiente = calcoli velocissimi
            for i, idx in enumerate(random_indices):
                raw_pos = dataset[idx]
                
                us_idx = [x for x in raw_pos['us'] if x < 768]
                them_idx = [x for x in raw_pos['them'] if x < 768]
                
                us_tensor = torch.zeros(1, 768, device=device)
                them_tensor = torch.zeros(1, 768, device=device)
                
                for u in us_idx: us_tensor[0, u] = 1.0
                for t in them_idx: them_tensor[0, t] = 1.0
                
                pred_prob = model(us_tensor, them_tensor).item()
                real_prob = raw_pos['target']
                
                pred_cp = prob_to_cp(pred_prob)
                real_cp = prob_to_cp(real_prob)
                
                print(f"Posizione {i+1}:")
                print(f"  -> Reale: {real_cp:+.2f} CP  (Win Prob: {real_prob:.3f})")
                print(f"  -> Rete:  {pred_cp:+.2f} CP  (Win Prob: {pred_prob:.3f})")
                print(f"  -> Errore: {abs(real_cp - pred_cp):.2f} CP\n")

    # ====================================================
    # FASE 2: TEST MANUALE INTERATTIVO
    # ====================================================
    print("-" * 50)
    print("MODALITÀ INTERATTIVA YACE NNUE")
    print("Incolla un FEN (es: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1)")
    print("Scrivi 'q' o 'quit' per uscire.")
    print("-" * 50)
    
    with torch.no_grad():
        while True:
            fen = input("\nFEN > ").strip()
            if fen.lower() in ['q', 'quit', 'exit']:
                break
            if not fen:
                continue
                
            try:
                us_tensor, them_tensor = fen_to_tensors(fen)
                us_tensor = us_tensor.to(device)
                them_tensor = them_tensor.to(device)
                
                output_prob = model(us_tensor, them_tensor).item()
                
                turn = fen.split(' ')[1]
                player = "Bianco" if turn == 'w' else "Nero"
                
                cp_relative = prob_to_cp(output_prob)
                cp_absolute = cp_relative if turn == 'w' else -cp_relative
                
                print(f"[{player} al tratto]")
                print(f"Vantaggio (Bianco): {cp_absolute / 100.0:+.2f} pedoni (CP: {cp_absolute:+.1f})")
                print(f"Prob. di vittoria ({player}): {output_prob * 100:.1f}%")
                
            except Exception as e:
                print(f"❌ Errore nel parsing del FEN: {e}")

if __name__ == "__main__":
    WEIGHTS = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/checkpoints/V1_Base/Run_3/nnue_latest_weights.pth" 
    BIN_DATASET = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_eval.bin"
    
    test_model(WEIGHTS, BIN_DATASET)