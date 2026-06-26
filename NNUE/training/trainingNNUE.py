import torch
import torch.nn as nn
import torch.optim as optim
from tqdm import tqdm
import numpy as np
import os

torch.set_float32_matmul_precision('high')

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
        
        x = torch.relu(self.layer1(x))
        x = torch.relu(self.layer2(x))
        
        x = self.output_layer(x)
        return torch.sigmoid(x)

def train_nnue(
        bin_path, 
        num_epochs=15, 
        batch_size=16384, 
        learning_rate=0.001,
        save_every_steps=5000,   
        resume_from=None
    ):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Sto addestrando su: {device}")
    
    struttura_dati = np.dtype([
        ('us', np.int16, 32),    
        ('them', np.int16, 32),  
        ('target', np.float32)   
    ])
    
    print("Mappatura del file binario in corso...")
    dataset = np.memmap(bin_path, dtype=struttura_dati, mode='r')
    num_positions = len(dataset)
    print(f"Dataset caricato: {num_positions:,} posizioni trovate.")
    
    model = BasicNNUE().to(device)
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)
    
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min', factor=0.5, patience=2)
    criterion = nn.MSELoss()
    
    # --- 2. GESTIONE RIPRISTINO CHECKPOINT ---
    global_step = 0
    start_epoch = 0
    
    if resume_from and os.path.exists(resume_from):
        print(f"\n[!] Ripristino lo stato dell'addestramento da: {resume_from}")
        checkpoint = torch.load(resume_from, map_location=device, weights_only=False)
        
        if "model_state_dict" in checkpoint:
            model.load_state_dict(checkpoint["model_state_dict"])
            
            print("[INFO] Reset dell'ottimizzatore Adam forzato (Momenti azzerati).")
            optimizer.load_state_dict(checkpoint["optimizer_state_dict"]) 
            
            global_step = checkpoint.get("global_step", 0)
            start_epoch = checkpoint.get("epoch", 0)
        else:
            clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in checkpoint.items()}
            model.load_state_dict(clean_state_dict)
            print("[INFO] Pesi puri caricati. Ottimizzatore Adam reinizializzato da zero.")
            
        print(f"-> Ripristinato con successo. Step iniziale: {global_step} | Epoca iniziale: {start_epoch + 1}")

    if device.type == 'cuda':
        print("Compilazione del modello in corso (torch.compile)...")
        model = torch.compile(model)

    os.makedirs("checkpoints", exist_ok=True)
    saved_checkpoints = [] 
    MAX_CHECKPOINTS = 3

    # --- LOOP DI TRAINING ---
    for epoch in range(start_epoch, num_epochs):
        model.train()
        print(f"\n--- Epoca {epoch+1}/{num_epochs} ---")
        
        # Shuffle istantaneo degli indici
        shuffle_indices = torch.randperm(num_positions)
        
        running_loss = 0.0
        num_batches = num_positions // batch_size
        
        progress_bar = tqdm(range(num_batches), desc=f"Training Epoca {epoch+1}")
        for i in progress_bar:
            batch_idx = shuffle_indices[i * batch_size : (i+1) * batch_size].numpy()
            
            raw_batch = dataset[batch_idx]
            
            us_indices = torch.from_numpy(raw_batch['us'].astype(np.int64)).to(device)
            them_indices = torch.from_numpy(raw_batch['them'].astype(np.int64)).to(device)
            targets = torch.from_numpy(raw_batch['target'].astype(np.float32)).unsqueeze(1).to(device)
            
            us_dense = torch.zeros(batch_size, 769, device=device)
            them_dense = torch.zeros(batch_size, 769, device=device)
            
            us_dense.scatter_(1, us_indices, 1.0)
            them_dense.scatter_(1, them_indices, 1.0)
            
            us_dense = us_dense[:, :768]
            them_dense = them_dense[:, :768]
            
            optimizer.zero_grad(set_to_none=True)
            outputs = model(us_dense, them_dense)
            loss = criterion(outputs, targets)
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item()
            global_step += 1
            
            progress_bar.set_postfix({'Loss': f"{loss.item():.4f}", 'Step': global_step})
            
            # --- 3. SALVATAGGIO CHECKPOINT INTERMEDIO ---
            if global_step % save_every_steps == 0:
                raw_model = model._orig_mod if hasattr(model, '_orig_mod') else model
                checkpoint_path = f"checkpoints/nnue_step_{global_step}.pth"
                
                torch.save({
                    'epoch': epoch + 1, # Salviamo l'epoca successiva per il resume corretto
                    'global_step': global_step,
                    'model_state_dict': raw_model.state_dict(),
                    'optimizer_state_dict': optimizer.state_dict(),
                    'loss': loss.item(),
                }, checkpoint_path)
                
                torch.save(raw_model.state_dict(), "checkpoints/nnue_latest_weights.pth")
                saved_checkpoints.append(checkpoint_path)
                
                # Elimina il più vecchio
                if len(saved_checkpoints) > MAX_CHECKPOINTS:
                    oldest_checkpoint = saved_checkpoints.pop(0) 
                    if os.path.exists(oldest_checkpoint):
                        os.remove(oldest_checkpoint) 
                
        avg_epoch_loss = running_loss / num_batches
        print(f"Loss media Epoca {epoch+1}: {avg_epoch_loss:.4f}")

        scheduler.step(avg_epoch_loss)
        
        # --- 4. CHECKPOINT FINE EPOCA ---
        raw_model = model._orig_mod if hasattr(model, '_orig_mod') else model
        torch.save(raw_model.state_dict(), f"checkpoints/nnue_epoch_{epoch+1}_final.pth")
        print(f"✅ Checkpoint di fine epoca salvato.")

if __name__ == "__main__":
    train_nnue(
        bin_path="/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_eval.bin", 
        num_epochs=15, 
        batch_size=16384, 
        learning_rate=0.001,
        save_every_steps=50000,
        resume_from="/home/lorenzo/Scrivania/Projects/YACE/NNUE/checkpoints/V1/Run_2/nnue_latest_weights.pth"
    )