import torch
import torch.nn as nn
import torch.nn.functional as F
import numpy as np
import math
import os

from NNUE import FakeQuantizeSTE, HalfKP_NNUE_QAT
from tqdm import tqdm

torch.set_float32_matmul_precision('high')

# --- 2. FUNZIONI DI SUPPORTO ---
def prob_to_cp(p):
    """Converte la probabilità di vittoria (0-1) in Centipawns (CP)."""
    p = max(1e-6, min(1.0 - 1e-6, p))
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

    def get_p_idx(char, perspective_is_white):
        is_piece_white = char.isupper()
        pt_char = char.lower()
        nnue_pt = {'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4}[pt_char]
        relative_color = 0 if (is_piece_white == perspective_is_white) else 1
        return nnue_pt * 2 + relative_color
        
    us_indices = []
    them_indices = []
    
    for char, sq in pieces:
        p_idx_w = get_p_idx(char, perspective_is_white=True)
        w_idx = sq + p_idx_w * 64 + wk_sq * 640
        
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
            
    while len(us_indices) < 32: us_indices.append(PAD_VAL)
    while len(them_indices) < 32: them_indices.append(PAD_VAL)
    
    return torch.tensor([us_indices], dtype=torch.long), torch.tensor([them_indices], dtype=torch.long)


def prob_to_cp_tensor(p_tensor):
    """Versione vettorizzata (veloce) per convertire interi tensori da WDL a CP."""
    p_clamped = torch.clamp(p_tensor, 1e-6, 1.0 - 1e-6)
    logit = torch.log(p_clamped / (1.0 - p_clamped))
    return logit * 100.0

def run_statistical_analysis(weights_path, bin_path, num_samples=500_000, batch_size=16384):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"--- AVVIO ANALISI STATISTICA SU {device.type.upper()} ---")
    
    # 1. Caricamento Modello
    model = HalfKP_NNUE_QAT().to(device)
    checkpoint = torch.load(weights_path, map_location=device, weights_only=False)
    
    state_dict = checkpoint.get("model_state_dict", checkpoint)
    clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in state_dict.items()}
    model.load_state_dict(clean_state_dict)
    model.eval()

    # 2. Mappatura Dataset
    struttura_dati = np.dtype([('us', np.uint16, 32), ('them', np.uint16, 32), ('target', np.float32)])
    dataset = np.memmap(bin_path, dtype=struttura_dati, mode='r')
    
    total_available = len(dataset)
    num_samples = min(num_samples, total_available)
    print(f"Dataset totale: {total_available:,} posizioni.")
    print(f"Estrazione casuale di {num_samples:,} posizioni per il test...\n")

    # Selezioniamo indici casuali univoci
    random_indices = np.random.choice(total_available, num_samples, replace=False)
    
    # Metriche
    all_errors_cp = []
    mae_wdl_sum = 0.0
    mae_cp_sum = 0.0
    sign_correct = 0

    # 3. Inferenza a Batch
    with torch.no_grad():
        for i in tqdm(range(0, num_samples, batch_size), desc="Valutazione"):
            batch_idx = random_indices[i : i + batch_size]
            raw_pos = dataset[batch_idx]
            
            us_tensor = torch.from_numpy(raw_pos['us'].astype(np.int64)).to(device)
            them_tensor = torch.from_numpy(raw_pos['them'].astype(np.int64)).to(device)
            real_prob = torch.from_numpy(raw_pos['target'].astype(np.float32)).to(device)
            
            # Previsione WDL della rete
            pred_prob = model(us_tensor, them_tensor).squeeze()
            
            # Calcolo Errori WDL (Probabilità pura)
            wdl_error = torch.abs(real_prob - pred_prob)
            mae_wdl_sum += torch.sum(wdl_error).item()
            
            # Calcolo Errori CP
            real_cp = prob_to_cp_tensor(real_prob)
            pred_cp = prob_to_cp_tensor(pred_prob)
            cp_error = torch.abs(real_cp - pred_cp)
            mae_cp_sum += torch.sum(cp_error).item()
            
            # Salvataggio errori per l'istogramma (spostati su CPU)
            all_errors_cp.extend((pred_cp - real_cp).cpu().numpy())
            
            # Sign Accuracy: La rete capisce chi è in vantaggio?
            # Consideriamo "vantaggio" se il CP > 0 (ovvero prob > 0.5)
            real_sign = (real_prob > 0.5)
            pred_sign = (pred_prob > 0.5)
            sign_correct += torch.sum(real_sign == pred_sign).item()

    # 4. Calcolo Statistiche Finali
    mae_wdl = mae_wdl_sum / num_samples
    mae_cp = mae_cp_sum / num_samples
    sign_accuracy = (sign_correct / num_samples) * 100.0
    
    errors_array = np.array(all_errors_cp)
    std_dev_cp = np.std(errors_array)

    print("\n" + "="*50)
    print(f" RISULTATI STATISTICI SU {num_samples:,} POSIZIONI")
    print("="*50)
    print(f"MAE WDL (Probabilità):  {mae_wdl:.4f} (Errore medio % di vittoria)")
    print(f"MAE Centipawns (CP):    {mae_cp:.2f} pedoni (Errore medio assoluto)")
    print(f"Deviazione Standard CP: {std_dev_cp:.2f} CP")
    print(f"Sign Accuracy:          {sign_accuracy:.2f}% (Indovina chi è in vantaggio)")
    print("="*50)

    # 5. Visualizzazione (Campana di Gauss degli errori)
    plt.figure(figsize=(10, 6))
    
    filtered_errors = errors_array[(errors_array > -1300) & (errors_array < 1300)]
    
    plt.hist(filtered_errors, bins=100, color='skyblue', edgecolor='black', alpha=0.7)
    plt.axvline(0, color='red', linestyle='dashed', linewidth=2, label='Errore Zero')
    plt.axvline(np.mean(filtered_errors), color='green', linestyle='dashed', linewidth=2, label='Media Errore')

    plt.yscale("log")
    
    plt.title("Distribuzione dell'Errore della Rete (Predizione CP - Target CP)")
    plt.xlabel("Delta Errore (Centipawns)")
    plt.ylabel("Frequenza (N. di Posizioni)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig("error_distribution_nnue.png", dpi=300)
    print("\nGrafico della distribuzione salvato come 'error_distribution_nnue.png'.")
    plt.show()

if __name__ == "__main__":
    WEIGHTS = "./checkpoints/halfkp_epoch_35_final.pth" 
    BIN_DATASET = "../data/clean_db_halfkp_balanced.bin"
    
    run_statistical_analysis(WEIGHTS, BIN_DATASET, num_samples=15_000_000)