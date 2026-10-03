"""Statistiche d'errore della rete rispetto ai target su un campione casuale dell'INTERO dataset
(include posizioni viste in training); per il solo split di validazione vedi heldout_by_phase.py.
Salva l'istogramma degli errori in analysis/error_distribution_nnue.png.
"""
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import torch
from tqdm import tqdm

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # moduli condivisi in NNUE/
from halfkp import DATA_DIR, RUNS_DIR, load_dataset
from model import load_model, prob_to_cp, to_tensors

torch.set_float32_matmul_precision('high')


def run_statistical_analysis(weights_path, bin_path, num_samples=500_000, batch_size=16384):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"--- AVVIO ANALISI STATISTICA SU {device.type.upper()} ---")
    
    model = load_model(weights_path, device)
    dataset = load_dataset(bin_path)
    
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
            us_tensor, them_tensor, real_prob = to_tensors(dataset[batch_idx], device)
            
            # Previsione WDL della rete
            pred_prob = model(us_tensor, them_tensor).squeeze()
            
            # Calcolo Errori WDL (Probabilità pura)
            wdl_error = torch.abs(real_prob - pred_prob)
            mae_wdl_sum += torch.sum(wdl_error).item()
            
            # Calcolo Errori CP
            real_cp = prob_to_cp(real_prob)
            pred_cp = prob_to_cp(pred_prob)
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
    out_png = Path(__file__).resolve().parent / "error_distribution_nnue.png"
    plt.savefig(out_png, dpi=300)
    print(f"\nGrafico della distribuzione salvato in {out_png}.")
    plt.show()

if __name__ == "__main__":
    WEIGHTS = RUNS_DIR / "run_v6/checkpoints/halfkp_epoch_18_final.pth"  # = nnue_weights_v3-6.bin (2338 Elo)
    BIN_DATASET = DATA_DIR / "clean_db_halfkp_balanced.bin"

    run_statistical_analysis(WEIGHTS, BIN_DATASET, num_samples=5_000_000)