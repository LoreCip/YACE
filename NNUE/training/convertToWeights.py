import torch
import numpy as np
import os

def export_to_raw_binary(checkpoint_path, output_bin_path):
    if not os.path.exists(checkpoint_path):
        print(f"Errore: Il file di origine '{checkpoint_path}' non esiste.")
        return

    print(f"Caricamento del checkpoint: {checkpoint_path}")
    checkpoint = torch.load(checkpoint_path, map_location="cpu", weights_only=False)

    # Estrazione dello state_dict
    if isinstance(checkpoint, dict) and "model_state_dict" in checkpoint:
        state_dict = checkpoint["model_state_dict"]
        print(f"-> Checkpoint completo rilevato (Epoca {checkpoint.get('epoch', 'N/D')}).")
    else:
        state_dict = checkpoint
        print("-> File di soli pesi rilevato.")

    # Pulizia dai prefissi di torch.compile
    clean_state_dict = {}
    for key, value in state_dict.items():
        clean_key = key.replace("_orig_mod.", "")
        clean_state_dict[clean_key] = value

    try:
        # 1. L0 Bias (Accumulator Bias) -> [256]
        l0_bias = clean_state_dict["accumulator_bias"].numpy().astype(np.float64)
        
        # 2. L0 Weights (Accumulator/Embedding) -> [40960, 256]
        # Prendiamo solo i primi 40960 elementi, escludendo l'indice di padding
        l0_weights = clean_state_dict["accumulator.weight"][:40960, :].numpy().astype(np.float64)
        
        # 3. L1 Bias -> [32]
        l1_bias = clean_state_dict["layer1.bias"].numpy().astype(np.float64)
        
        # 4. L1 Weights -> [32, 512]
        l1_weights = clean_state_dict["layer1.weight"].numpy().astype(np.float64)
        
        # 5. L2 Bias -> [1] (Scalare)
        l2_bias = clean_state_dict["output_layer.bias"].numpy().astype(np.float64)
        
        # 6. L2 Weights -> [32]
        # PyTorch lo salva come [1, 32], lo appiattiamo a 1D
        l2_weights = clean_state_dict["output_layer.weight"].flatten().numpy().astype(np.float64)

        # --- SCRITTURA DEL FILE BINARIO ---
        print(f"\nScrittura del file binario crudo in corso...")
        with open(output_bin_path, 'wb') as f:
            f.write(l0_bias.tobytes())
            f.write(l0_weights.tobytes())
            f.write(l1_bias.tobytes())
            f.write(l1_weights.tobytes())
            f.write(l2_bias.tobytes())
            f.write(l2_weights.tobytes())

        # Verifica finale delle dimensioni
        expected_bytes = (256 + (40960 * 256) + 32 + (32 * 512) + 1 + 32) * 8
        actual_bytes = os.path.getsize(output_bin_path)

        print("\nEsportazione completata con successo!")
        print(f"Dimensioni attese: {expected_bytes / (1024*1024):.2f} MB")
        print(f"Dimensioni reali:  {actual_bytes / (1024*1024):.2f} MB")
        
        if expected_bytes == actual_bytes:
            print("-> I byte scritti corrispondono alle specifiche del C++.")
        else:
            print("-> ATTENZIONE: Disallineamento nelle dimensioni del file generato!")

    except KeyError as e:
        print(f"\nErrore: Non riesco a trovare il tensore {e} nel checkpoint.")
        print("I tensori disponibili sono:", list(clean_state_dict.keys()))

if __name__ == "__main__":
    CHECKPOINT_COMPLETO = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/training/weights/halfkp_epoch15_v1.pth"
    OUTPUT_BINARIO = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/training/weights/nnue_weights_v1.bin" 
    export_to_raw_binary(CHECKPOINT_COMPLETO, OUTPUT_BINARIO)