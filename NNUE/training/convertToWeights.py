import torch
import struct
import numpy as np
import os

def export_to_cpp_binary(pth_path, base_path, bin_path):
    print(f"Caricamento pesi da: {pth_path}")
    
    checkpoint = torch.load(pth_path, map_location='cpu', weights_only=False)
    state_dict = checkpoint.get("model_state_dict", checkpoint)
    clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in state_dict.items()}

    # --- SCALE (Devono combaciare con quelle usate nella classe QAT) ---
    SCALE_W_ACC = 255.0
    SCALE_ACT   = 127.0
    SCALE_W_L1  = 64.0
    SCALE_W_L2  = 64.0
    SCALE_W_OUT = 127.0

    print("Estrazione e quantizzazione dei tensori...")

    # 1. Accumulatore (L0) - int16_t
    # shape: (40961, 256)
    l0_weight_float = clean_state_dict['accumulator.weight'][:40960, :].numpy()
    l0_bias_float   = clean_state_dict['accumulator_bias'].numpy()
    
    L0      = np.clip(np.round(l0_weight_float * SCALE_W_ACC), -32768, 32767).astype(np.int16)
    L0Bias  = np.clip(np.round(l0_bias_float * SCALE_W_ACC), -32768, 32767).astype(np.int16)

    # 2. Layer 1 (L1) - pesi int8_t, bias int32_t
    l1_weight_float = clean_state_dict['layer1.weight'].numpy()
    l1_bias_float   = clean_state_dict['layer1.bias'].numpy()
    
    L1      = np.clip(np.round(l1_weight_float * SCALE_W_L1), -128, 127).astype(np.int8)
    L1Bias  = np.round(l1_bias_float * (SCALE_W_ACC * SCALE_W_L1)).astype(np.int32)

    # 3. Layer 2 (L2) - pesi int8_t, bias int32_t
    l2_weight_float = clean_state_dict['layer2.weight'].numpy()
    l2_bias_float   = clean_state_dict['layer2.bias'].numpy()
    
    L2      = np.clip(np.round(l2_weight_float * SCALE_W_L2), -128, 127).astype(np.int8)
    L2Bias  = np.round(l2_bias_float * (SCALE_ACT * SCALE_W_L2)).astype(np.int32)

    # 4. Layer Output (L3) - pesi int16_t, bias int32_t (CORRETTO: usa le variabili l3_)
    l3_weight_float = clean_state_dict['output_layer.weight'].numpy().flatten()
    l3_bias_float   = clean_state_dict['output_layer.bias'].numpy()
    
    L3      = np.clip(np.round(l3_weight_float * SCALE_W_OUT), -32768, 32767).astype(np.int16)
    L3Bias  = np.round(l3_bias_float * (SCALE_ACT * SCALE_W_OUT)).astype(np.int32)
    
    print(f"Scrittura del file binario: {bin_path}")
    
    with open(bin_path, 'wb') as f:
        
        # 1. L0Bias (256 * int16 = 512 bytes)
        f.write(L0Bias.tobytes())
        
        # 2. L0 Pesi (40960 * 256 * int16 = 20,971,520 bytes)
        f.write(L0.tobytes())
        
        # 3. L1Bias (32 * int32 = 128 bytes)
        f.write(L1Bias.tobytes())
        
        # 4. L1 Pesi (32 * 512 * int8 = 16,384 bytes)
        f.write(L1.tobytes())
        
        # 5. L2Bias (32 * int32 = 128 bytes)
        f.write(L2Bias.tobytes())
        
        # 6. L2 Pesi (32 * 32 * int8 = 1024 bytes)
        f.write(L2.tobytes())

        # 7. L3Bias (1 * int32 = 4 bytes)
        f.write(L3Bias.tobytes())

        # 8. L3 Pesi (32 * int16 = 64 bytes)
        f.write(L3.tobytes())

if __name__ == "__main__":
    # Inserisci i tuoi percorsi
    PTH_FILE = "./checkpoints/halfkp_epoch_35_final.pth"
    BASE_PATH = "../../parameters/weights/"
    BIN_FILE = BASE_PATH + "nnue_weights_v3.bin"
    export_to_cpp_binary(PTH_FILE, BASE_PATH, BIN_FILE)