"""Bilancia (sfoltisce patte e posizioni affollate) e rimescola il dataset HalfKP."""
import os
import shutil
import sys
from pathlib import Path

import numpy as np
from tqdm import tqdm

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # moduli condivisi in NNUE/
from halfkp import DATA_DIR, DATASET_DTYPE, PAD_IDX

N_SHARDS = 64


def bilancia_dataset_nnue(input_path, output_path, chunk_size=5_000_000, shard_dir=None):
    if not os.path.exists(input_path):
        print(f"Errore: Il file {input_path} non esiste.")
        return

    bytes_per_row = DATASET_DTYPE.itemsize
    file_size_bytes = os.path.getsize(input_path)
    total_positions = file_size_bytes // bytes_per_row
    num_chunks = int(np.ceil(total_positions / chunk_size))

    if shard_dir is None:
        shard_dir = output_path + ".shuffle_shards"
    if os.path.exists(shard_dir):
        shutil.rmtree(shard_dir)
    os.makedirs(shard_dir)

    print(f"--- INIZIO BILANCIAMENTO + SHUFFLE DATASET ---")
    print(f"File di input:  {input_path}  ({total_positions:,} posizioni)")
    print(f"File di output: {output_path}")
    print(f"Shard temporanei ({N_SHARDS}): {shard_dir}")

    # Non sovrascriviamo mai direttamente un output_path preesistente: se esiste già,
    # lo consideriamo la versione precedente e la lasciamo intatta finché il nuovo file
    # non e' stato scritto e verificato per intero (vedi rename finale).
    tmp_output_path = output_path + ".new"
    if os.path.exists(tmp_output_path):
        os.remove(tmp_output_path)

    # ------------------------------------------------------------------
    # PASSO 1: lettura a blocchi + filtro di bilanciamento (come prima) +
    # smistamento casuale di ogni riga tenuta in uno degli N_SHARDS file
    # temporanei, cosi' l'ordine originale dello stream Lichess (che non e'
    # i.i.d. per posizione nel file — blocchi da milioni di righe con
    # statistiche diverse tra loro) viene rimescolato invece di essere
    # preservato fino al file finale.
    # ------------------------------------------------------------------
    posizioni_lette = 0
    posizioni_scritte = 0

    dataset_in = np.memmap(input_path, dtype=DATASET_DTYPE, mode='r')
    shard_paths = [os.path.join(shard_dir, f"shard_{s:02d}.bin") for s in range(N_SHARDS)]
    shard_files = [open(p, 'ab') for p in shard_paths]

    try:
        for i in tqdm(range(num_chunks), desc="Passo 1/2: bilanciamento + smistamento"):
            start_idx = i * chunk_size
            end_idx = min((i + 1) * chunk_size, total_positions)

            chunk = np.array(dataset_in[start_idx:end_idx])

            targets = chunk['target']
            pezzi_attivi = np.sum(chunk['us'] < PAD_IDX, axis=1)

            keep_probs = np.ones(len(chunk), dtype=np.float32)

            # ====================================================
            # PENALITÀ 1: Sfoltimento Lorentziano per le patte/posizioni bilanciate
            # ====================================================
            # NOTA (2026-08-29): un tentativo di allargare molto questi parametri
            # (gamma=0.08, depth=0.98) per appiattire l'istogramma dei target ha
            # prodotto un modello PEGGIORE (errore relativo su held-out 0.816 contro
            # 0.644 di V3-4) — probabilmente perché il taglio troppo aggressivo ha
            # ridotto il volume totale di dati (~40%) e sovra-rappresentato le
            # posizioni decisive rispetto alla distribuzione reale di gioco (dove le
            # posizioni vicine alla parità sono la maggioranza). Ripristinati i valori
            # originali: la concentrazione vicino a 0.5 non è necessariamente un difetto
            # da correggere, potrebbe riflettere la realtà del gioco.
            mu = 0.5
            depth = 0.85
            gamma = 0.02

            lorentz_penalty = depth * ((gamma**2) / ((targets - mu)**2 + gamma**2))
            keep_probs *= (1.0 - lorentz_penalty)

            # ====================================================
            # PENALITÀ 2: Sbilanciamento Mediogioco
            # ====================================================
            midgame_mask = (pezzi_attivi > 12) & (pezzi_attivi <= 22)
            keep_probs[midgame_mask] *= 0.60

            crowded_mask = (pezzi_attivi > 22)
            keep_probs[crowded_mask] *= 0.35

            random_vals = np.random.rand(len(chunk))
            keep_mask = random_vals < keep_probs

            filtered_chunk = chunk[keep_mask]

            # Smistamento casuale nei shard: ogni shard riceve righe da OGNI chunk
            # originale, cosi' la concatenazione finale non ha lunghe sequenze con
            # statistiche diverse dal resto del file.
            shard_ids = np.random.randint(0, N_SHARDS, size=len(filtered_chunk))
            for s in range(N_SHARDS):
                shard_files[s].write(filtered_chunk[shard_ids == s].tobytes())

            posizioni_lette += len(chunk)
            posizioni_scritte += len(filtered_chunk)
    finally:
        for f in shard_files:
            f.close()

    # ------------------------------------------------------------------
    # PASSO 2: ogni shard (piccolo, sta comodamente in RAM) viene caricato,
    # permutato internamente, e accodato al file finale. Verifica del conteggio
    # totale prima di cancellare gli shard temporanei.
    # ------------------------------------------------------------------
    righe_finali = 0
    with open(tmp_output_path, 'wb') as f_out:
        for shard_path in tqdm(shard_paths, desc="Passo 2/2: shuffle finale degli shard"):
            shard_data = np.fromfile(shard_path, dtype=DATASET_DTYPE)
            if len(shard_data) == 0:
                continue
            perm = np.random.permutation(len(shard_data))
            f_out.write(shard_data[perm].tobytes())
            righe_finali += len(shard_data)

    assert righe_finali == posizioni_scritte, (
        f"Conteggio righe incoerente dopo lo shuffle: attese {posizioni_scritte:,}, "
        f"trovate {righe_finali:,}. File temporaneo NON promosso, shard NON cancellati."
    )

    shutil.rmtree(shard_dir)

    # Solo ora, con il nuovo file scritto e verificato, sostituiamo l'output finale.
    if os.path.exists(output_path):
        backup_path = output_path + ".bak"
        if os.path.exists(backup_path):
            os.remove(backup_path)
        os.rename(output_path, backup_path)
        print(f"Vecchio file di output conservato come backup: {backup_path}")
    os.rename(tmp_output_path, output_path)

    # 5. Report Finale
    print("\n--- OPERAZIONE COMPLETATA ---")
    print(f"Posizioni originali: {total_positions:,}")
    print(f"Posizioni mantenute: {posizioni_scritte:,} ({(posizioni_scritte/total_positions)*100:.1f}%)")

    old_size = file_size_bytes / (1024**3)
    new_size = os.path.getsize(output_path) / (1024**3)
    print(f"Dimensione su disco ridotta da {old_size:.2f} GB a {new_size:.2f} GB.")


if __name__ == "__main__":
    INPUT_BIN = str(DATA_DIR / "clean_db_halfkp.bin")
    OUTPUT_BIN = str(DATA_DIR / "clean_db_halfkp_balanced.bin")

    bilancia_dataset_nnue(INPUT_BIN, OUTPUT_BIN)