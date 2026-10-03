"""Statistiche e grafici di un dataset HalfKP (distribuzione dei target e dei pezzi)."""
import os
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # moduli condivisi in NNUE/
from halfkp import DATA_DIR, PAD_IDX, load_dataset

def analizza_dataset_nnue(bin_path, sample_size=1_000_000):
    if not os.path.exists(bin_path):
        print(f"Errore: Il file {bin_path} non esiste.")
        return

    print("--- 1. LETTURA DEL DATASET ---")
    dataset = load_dataset(bin_path)
    num_positions = len(dataset)
    print(f"Posizioni totali nel file .bin: {num_positions:,}")

    # Estraiamo un campione casuale per l'analisi statistica e il plotting
    print(f"Estrazione di un campione casuale di {sample_size:,} posizioni per l'analisi...")
    indices = np.random.choice(num_positions, size=min(sample_size, num_positions), replace=False)
    sample = dataset[indices]

    targets = sample['target']
    us_indices = sample['us']
    them_indices = sample['them']

    # --- 2. CALCOLO STATISTICHE SUI PEZZI ---
    # Ogni indice diverso dal padding e' un pezzo (non-re) sulla scacchiera
    pezzi_us = np.sum(us_indices < PAD_IDX, axis=1)
    pezzi_them = np.sum(them_indices < PAD_IDX, axis=1)
    totale_pezzi = pezzi_us

    print("\n--- 2. STATISTICHE DESCRITTIVE ---")
    print(f"Target -> Min: {targets.min():.4f} | Max: {targets.max():.4f} | Media: {targets.mean():.4f} | Dev.Std: {targets.std():.4f}")
    print(f"Pezzi Attivi Totali -> Min: {totale_pezzi.min()} | Max: {totale_pezzi.max()} | Media: {totale_pezzi.mean():.2f}")

    # --- 3. PLOTTING ---
    print("\nGenerazione dei grafici in corso...")
    sns.set_theme(style="whitegrid")
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f"Analisi Dataset NNUE: {os.path.basename(bin_path)}", fontsize=18, fontweight='bold')

    # Grafico 1: Distribuzione dei Target
    sns.histplot(targets, bins=100, kde=True, ax=axes[0, 0], color="skyblue")
    axes[0, 0].set_title("Distribuzione dei Target (Valutazioni/Risultati)", fontsize=14)
    axes[0, 0].set_xlabel("Target Value")
    axes[0, 0].set_ylabel("Frequenza")

    # Grafico 2: Distribuzione del numero di pezzi attivi
    sns.histplot(totale_pezzi, bins=np.arange(totale_pezzi.min(), totale_pezzi.max() + 2) - 0.5, 
                 kde=False, ax=axes[0, 1], color="salmon", discrete=True)
    axes[0, 1].set_title("Distribuzione del Numero Totale di Pezzi per Posizione", fontsize=14)
    axes[0, 1].set_xlabel("Numero di Pezzi (Us + Them)")
    axes[0, 1].set_ylabel("Frequenza")

    # Grafico 3: Boxplot Target diviso per Numero di Pezzi
    # Filtriamo per non avere troppe categorie vuote nei grafici
    mask = (totale_pezzi >= 4) & (totale_pezzi <= 32)
    sns.boxplot(x=totale_pezzi[mask], y=targets[mask], ax=axes[1, 0], palette="Spectral")
    axes[1, 0].set_title("Distribuzione del Target in base al Numero di Pezzi", fontsize=14)
    axes[1, 0].set_xlabel("Numero Totale di Pezzi")
    axes[1, 0].set_ylabel("Target")
    axes[1, 0].tick_params(axis='x', rotation=45)

    # Grafico 4: Bilanciamento Us vs Them (Feature Attive)
    sns.scatterplot(x=pezzi_us, y=pezzi_them, alpha=0.1, ax=axes[1, 1], color="purple")
    axes[1, 1].set_title("Bilanciamento Pezzi: Us vs Them", fontsize=14)
    axes[1, 1].set_xlabel("Pezzi Attivi 'Us'")
    axes[1, 1].set_ylabel("Pezzi Attivi 'Them'")

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    
    plt.show()

if __name__ == "__main__":
    PATH_FILE = DATA_DIR / "clean_db_halfkp_balanced.bin"
    analizza_dataset_nnue(PATH_FILE)