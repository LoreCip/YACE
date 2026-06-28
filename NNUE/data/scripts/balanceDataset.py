import numpy as np
import os
from tqdm import tqdm

def bilancia_dataset_nnue(input_path, output_path, chunk_size=5_000_000):
    if not os.path.exists(input_path):
        print(f"Errore: Il file {input_path} non esiste.")
        return

    # 1. Definizione della struttura dati e parametri
    struttura_dati = np.dtype([
        ('us', np.uint16, 32),    
        ('them', np.uint16, 32),  
        ('target', np.float32)   
    ])
    PADDING_VAL = 40960

    # Calcolo della dimensione del file per iterare a blocchi
    file_size_bytes = os.path.getsize(input_path)
    bytes_per_row = struttura_dati.itemsize
    total_positions = file_size_bytes // bytes_per_row
    num_chunks = int(np.ceil(total_positions / chunk_size))

    print(f"--- INIZIO BILANCIAMENTO DATASET ---")
    print(f"File di input:  {input_path}")
    print(f"File di output: {output_path}")
    print(f"Posizioni totali: {total_positions:,}")
    print(f"Elaborazione in {num_chunks} blocchi da {chunk_size:,} posizioni.\n")

    # Se il file di output esiste già, lo eliminiamo per evitare di accodare dati vecchi
    if os.path.exists(output_path):
        os.remove(output_path)

    posizioni_scritte = 0
    posizioni_scartate = 0

    # 2. Lettura e filtraggio a blocchi (memmap per efficienza)
    dataset_in = np.memmap(input_path, dtype=struttura_dati, mode='r')

    with open(output_path, 'ab') as f_out:
        for i in tqdm(range(num_chunks), desc="Avanzamento Globale"):
            start_idx = i * chunk_size
            end_idx = min((i + 1) * chunk_size, total_positions)
            
            # Estrazione del blocco corrente in memoria
            chunk = np.array(dataset_in[start_idx:end_idx])
            
            targets = chunk['target']
            pezzi_attivi = np.sum(chunk['us'] < PADDING_VAL, axis=1)

            # Partiamo col 100% di probabilità di tenere ogni posizione
            keep_probs = np.ones(len(chunk), dtype=np.float32)

            # ====================================================
            # PENALITÀ 1: Sfoltimento Lorentziano per le patte
            # ====================================================
            mu = 0.5       # Centro del picco
            depth = 0.85   # Percentuale massima di sfoltimento (0.85 = teniamo il 15% al centro)
            gamma = 0.02   # Larghezza della campana
            
            # Calcolo vettorializzato della penalità Lorentziana
            lorentz_penalty = depth * ( (gamma**2) / ((targets - mu)**2 + gamma**2) )
            
            # Sottraiamo la penalità alla probabilità base (1.0)
            keep_probs *= (1.0 - lorentz_penalty)

            # ====================================================
            # PENALITÀ 2: Sbilanciamento Mediogioco
            # ====================================================
            # Teniamo il 100% dei finali (<= 12 pezzi)
            midgame_mask = (pezzi_attivi > 12) & (pezzi_attivi <= 22)
            keep_probs[midgame_mask] *= 0.60 

            crowded_mask = (pezzi_attivi > 22)
            keep_probs[crowded_mask] *= 0.35 

            # 4. Applicazione del filtro tramite valori casuali
            random_vals = np.random.rand(len(chunk))
            keep_mask = random_vals < keep_probs

            filtered_chunk = chunk[keep_mask]
            
            # Scrittura su disco del blocco filtrato
            f_out.write(filtered_chunk.tobytes())
            
            posizioni_scritte += len(filtered_chunk)
            posizioni_scartate += (len(chunk) - len(filtered_chunk))

    # 5. Report Finale
    print("\n--- OPERAZIONE COMPLETATA ---")
    print(f"Posizioni originali: {total_positions:,}")
    print(f"Posizioni mantenute: {posizioni_scritte:,} ({(posizioni_scritte/total_positions)*100:.1f}%)")
    print(f"Posizioni scartate:  {posizioni_scartate:,}")
    
    old_size = file_size_bytes / (1024**3)
    new_size = os.path.getsize(output_path) / (1024**3)
    print(f"\nDimensione su disco ridotta da {old_size:.2f} GB a {new_size:.2f} GB.")


if __name__ == "__main__":
    INPUT_BIN = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp.bin"
    OUTPUT_BIN = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp_balanced.bin"
    
    bilancia_dataset_nnue(INPUT_BIN, OUTPUT_BIN)