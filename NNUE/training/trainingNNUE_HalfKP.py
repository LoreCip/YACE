import torch
import torch.nn as nn
import torch.optim as optim
from tqdm import tqdm
import numpy as np
import os

torch.set_float32_matmul_precision('high')

class HalfKP_NNUE(nn.Module):
    def __init__(self, acc_size=256, layer_1=32, layer_2=32):
        super(HalfKP_NNUE, self).__init__()
        
        # padding_idx=40960 fa in modo che "PAD_VAL" valga sempre zero
        self.accumulator = nn.Embedding(40961, acc_size, padding_idx=40960)
        self.accumulator_bias = nn.Parameter(torch.zeros(acc_size)) # L'embedding non ha bias, lo aggiungiamo a mano
        
        self.layer1 = nn.Linear(acc_size * 2, layer_1)
        self.layer2 = nn.Linear(layer_1, layer_2)
        self.output_layer = nn.Linear(layer_2, 1)

    def clipped_relu(self, x):
        return torch.clamp(x, 0.0, 1.0)

    def forward(self, us_indices, them_indices):
        # Passiamo DIRETTAMENTE gli indici (Shape: Batch x 32)
        acc_us = self.accumulator(us_indices).sum(dim=1) + self.accumulator_bias
        acc_them = self.accumulator(them_indices).sum(dim=1) + self.accumulator_bias
        
        acc_us = self.clipped_relu(acc_us)
        acc_them = self.clipped_relu(acc_them)
        
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
        resume_from=None
    ):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Sto addestrando HalfKP su: {device}")
    
    struttura_dati = np.dtype([
        ('us', np.uint16, 32),    
        ('them', np.uint16, 32),  
        ('target', np.float32)   
    ])
    
    dataset = np.memmap(bin_path, dtype=struttura_dati, mode='r')
    num_positions = len(dataset)
    
    model = HalfKP_NNUE().to(device)
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
            
            optimizer.load_state_dict(checkpoint["optimizer_state_dict"]) 
            print("[INFO] Stato dell'ottimizzatore Adam ripristinato (Inerzia e momenti recuperati).")
            
            if "scheduler_state_dict" in checkpoint:
                # scheduler.load_state_dict(checkpoint["scheduler_state_dict"])
                print("[INFO] Stato del Learning Rate Scheduler ripristinato.")
            
            global_step = checkpoint.get("global_step", 0)
            start_epoch = checkpoint.get("epoch", 0)
            
        else:
            clean_state_dict = {k.replace('_orig_mod.', ''): v for k, v in checkpoint.items()}
            model.load_state_dict(clean_state_dict)
            print("[INFO] Pesi puri caricati. L'ottimizzatore Adam e lo Scheduler ripartono da zero.")
            
        print(f"-> Ripristinato con successo. Step iniziale: {global_step} | Epoca iniziale: {start_epoch + 1}")


    if device.type == 'cuda':
        model = torch.compile(model)

    os.makedirs("checkpoints", exist_ok=True)
    saved_checkpoints = [] 
    MAX_CHECKPOINTS = 3

    for epoch in range(start_epoch, num_epochs):
        model.train()
        print(f"\n--- Epoca {epoch+1}/{num_epochs} ---")
        
        shuffle_indices = torch.randperm(num_positions)
        running_loss = 0.0
        num_batches = num_positions // batch_size
        
        current_lr = optimizer.param_groups[0]['lr']
        progress_bar = tqdm(range(num_batches), desc=f"Training Epoca {epoch+1} | LR {current_lr:.6f}")
        for i in progress_bar:
            batch_idx = shuffle_indices[i * batch_size : (i+1) * batch_size].numpy()
            raw_batch = dataset[batch_idx]
            
            us_indices = torch.from_numpy(raw_batch['us'].astype(np.int64)).to(device)
            them_indices = torch.from_numpy(raw_batch['them'].astype(np.int64)).to(device)
            targets = torch.from_numpy(raw_batch['target'].astype(np.float32)).unsqueeze(1).to(device)
            
            optimizer.zero_grad(set_to_none=True)
            
            outputs = model(us_indices, them_indices)
            
            loss = criterion(outputs, targets)
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item()
            global_step += 1
            progress_bar.set_postfix({'Loss': f"{loss.item():.4f}", 'Step': global_step})
                
        avg_epoch_loss = running_loss / num_batches
        print(f"Loss media Epoca {epoch+1}: {avg_epoch_loss:.4f}")
        scheduler.step(avg_epoch_loss)
    
        # --- 4. CHECKPOINT FINE EPOCA (Aggiornato per salvare tutto) ---
        raw_model = model._orig_mod if hasattr(model, '_orig_mod') else model
        torch.save({
            'epoch': epoch + 1,
            'global_step': global_step,
            'model_state_dict': raw_model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'scheduler_state_dict': scheduler.state_dict(),
            'loss': avg_epoch_loss,
        }, f"checkpoints/halfkp_epoch_{epoch+1}_final.pth") # File completo per il resume
        print(f" Checkpoint completo e pesi di fine epoca salvati.")

if __name__ == "__main__":
    train_nnue(
        bin_path="/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp.bin", 
        num_epochs=15, 
        batch_size=16384, 
        learning_rate=0.001,
        resume_from="/home/lorenzo/Scrivania/Projects/YACE/NNUE/checkpoints/halfkp_epoch_6_final.pth"
    )