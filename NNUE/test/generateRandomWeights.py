import numpy as np

# Configurazione dell'architettura
NUM_FEATURES = 40960
M = 256
K = 32

def generate_and_save_weights(filepath):
    print("Generazione dei pesi casuali in corso...")
    
    np.random.seed(42)    
    # BLOCK 1: L0 Bias -> Array 1D di dimensione M
    l0_bias = np.random.uniform(-0.1, 0.1, size=(M,)).astype(np.float64)
    
    # BLOCK 2: L0 Weights -> Matrice [M, NUM_FEATURES] in PyTorch
    l0_weights_pytorch = np.random.uniform(-0.2, 0.2, size=(M, NUM_FEATURES)).astype(np.float64)
    l0_weights_cpp = l0_weights_pytorch.transpose()  # Diventa [NUM_FEATURES][M]
    
    # BLOCK 3: L1 Bias -> Array 1D di dimensione K
    l1_bias = np.random.uniform(-0.1, 0.1, size=(K,)).astype(np.float64)
    
    # BLOCK 4: L1 Weights -> Matrice [K, 2*M] in PyTorch (Già Row-Major)
    l1_weights = np.random.uniform(-0.1, 0.1, size=(K, 2 * M)).astype(np.float64)
    
    # BLOCK 5: L2 Bias -> Scalare singolo
    l2_bias = np.random.uniform(-0.05, 0.05, size=(1,)).astype(np.float64)
    
    # BLOCK 6: L2 Weights -> Array 1D di dimensione K
    l2_weights = np.random.uniform(-0.2, 0.2, size=(K,)).astype(np.float64)

    with open(filepath, 'wb') as f:
        f.write(l0_bias.tobytes())
        f.write(l0_weights_cpp.tobytes())
        f.write(l1_bias.tobytes())
        f.write(l1_weights.tobytes())
        f.write(l2_bias.tobytes())
        f.write(l2_weights.tobytes())
        
    expected_size = (M + (NUM_FEATURES * M) + K + (K * 2 * M) + 1 + K) * 8
    print(f"File binario scritto con successo: {filepath}")
    print(f"Dimensione prevista: {expected_size} byte.")

if __name__ == "__main__":
    generate_and_save_weights("random_network.bin")