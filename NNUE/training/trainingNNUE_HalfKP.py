import torch
import torch.nn as nn
import torch.optim as optim
from tqdm import tqdm
import numpy as np
import os

torch.set_float32_matmul_precision('high')

class HalfKP_NNUE(nn.Module):
    def __init__(self, acc_size=256, layer_1=32):
        super(HalfKP_NNUE, self).__init__()
        
        self.accumulator = nn.Embedding(40961, acc_size, padding_idx=40960)
        self.accumulator_bias = nn.Parameter(torch.zeros(acc_size)) 
        
        self.layer1 = nn.Linear(acc_size * 2, layer_1)
        
        self.output_layer = nn.Linear(layer_1, 1)

    def clipped_relu(self, x):
        return torch.clamp(x, 0.0, 1.0)

    def forward(self, us_indices, them_indices):
        acc_us = self.accumulator(us_indices).sum(dim=1) + self.accumulator_bias
        acc_them = self.accumulator(them_indices).sum(dim=1) + self.accumulator_bias
        
        # L0: Attivazione Clipped ReLU (0.0 -> 1.0)
        acc_us = self.clipped_relu(acc_us)
        acc_them = self.clipped_relu(acc_them)        
        x = torch.cat([acc_us, acc_them], dim=1)
        
        # L1: Attivazione Clipped ReLU (0.0 -> 1.0)
        x = self.clipped_relu(self.layer1(x))
        
        # L2: Output
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
    
    print(f"Caricamento del dataset in RAM da {bin_path}...")
    dataset = np.fromfile(bin_path, dtype=struttura_dati)
    num_positions = len(dataset)
    num_batches = num_positions // batch_size
    print(f"Dataset caricato con successo: {num_positions:,} posizioni.")
    
    model = HalfKP_NNUE().to(device)
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)
    criterion = nn.MSELoss()
    
    global_step = 0
    start_epoch = 0

    if resume_from and os.path.exists(resume_from):
        checkpoint = torch.load(resume_from, map_location=device, weights_only=False)
        model.load_state_dict(checkpoint["model_state_dict"])
        optimizer.load_state_dict(checkpoint["optimizer_state_dict"])
        start_epoch = checkpoint.get("epoch", 0)
        global_step = checkpoint.get("global_step", 0)
        print(f"[INFO] Ripristinato: Epoca {start_epoch}, Step {global_step}")

    if start_epoch >= 15:
        scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=5, gamma=0.5)
    else:
        scheduler = optim.lr_scheduler.OneCycleLR(
                            optimizer,
                            max_lr=learning_rate,
                            steps_per_epoch=num_batches, 
                            epochs=num_epochs,           
                            pct_start=0.1,
                            div_factor=10.0,
                            final_div_factor=100.0
                        )

    if device.type == 'cuda':
        model = torch.compile(model)

    os.makedirs("checkpoints", exist_ok=True)

    for epoch in range(start_epoch, num_epochs):
        model.train()
        print(f"\n--- Epoca {epoch+1}/{num_epochs} ---")
        
        shuffle_indices = torch.randperm(num_positions)
        running_loss = 0.0
        current_lr = optimizer.param_groups[0]['lr']
        
        progress_bar = tqdm(
            range(num_batches), 
            desc=f"Epoca {epoch+1}/{num_epochs} [LR: {current_lr:.6f}]",
            dynamic_ncols=True,
            leave=True         
        )
        
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
            scheduler.step()
            
            running_loss += loss.item()
            global_step += 1
            
            current_avg_loss = running_loss / (i + 1)
            
            progress_bar.set_postfix_str(f"Avg_Loss: {current_avg_loss:.4f} | Batch: {loss.item():.4f}")
                
        avg_epoch_loss = running_loss / num_batches
        
    
        # --- CHECKPOINT ---
        raw_model = model._orig_mod if hasattr(model, '_orig_mod') else model
        torch.save({
            'epoch': epoch + 1,
            'global_step': global_step,
            'model_state_dict': raw_model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'scheduler_state_dict': scheduler.state_dict(),
            'loss': avg_epoch_loss,
        }, f"checkpoints/halfkp_epoch_{epoch+1}_final.pth")
        
        print(f" Checkpoint salvato.")

if __name__ == "__main__":
    train_nnue(
        bin_path="/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp_balanced.bin", 
        num_epochs=15, 
        batch_size=16384, 
        learning_rate=0.001,
        resume_from=None 
    )