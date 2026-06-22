import torch
import torch.nn as nn
from torch.utils.data import IterableDataset, DataLoader
import torch.optim as optim
from tqdm import tqdm

import chess
import math
import json
import io
import random
import zstandard as zstd

CLEAN_DATASET = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_eval.jsonl.zst"

class StreamingChessDataset(IterableDataset):
    def __init__(self, filepath, buffer_size=1000000):
        self.filepath = filepath
        self.buffer_size = buffer_size
        self.piece_map = {
            'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
            'p': 6, 'n': 7, 'b': 8, 'r': 9, 'q': 10, 'k': 11
        }

    def __iter__(self):
        buffer = []
        
        with open(self.filepath, 'rb') as fh:
            dctx = zstd.ZstdDecompressor()
            with dctx.stream_reader(fh) as reader:
                text_reader = io.TextIOWrapper(reader, encoding='utf-8')
                
                for line in text_reader:
                    data = json.loads(line)
                    
                    us_features, them_features = self.fen_to_dual_tensor(data["fen"])
                    cp = data["cp"]
                    win_prob = 1.0 / (1.0 + math.exp(-cp / 400.0))
                    target = torch.tensor([win_prob], dtype=torch.float32)
                    
                    item = (us_features, them_features, target)

                    if len(buffer) < self.buffer_size:
                        buffer.append(item)
                    else:
                        random_index = random.randint(0, self.buffer_size - 1)
                        yield buffer[random_index]
                        buffer[random_index] = item

                random.shuffle(buffer)
                for item in buffer:
                    yield item

    def get_single_tensor(self, board):
        """Funzione d'appoggio: calcola l'array 768 per una singola board"""
        tensor = torch.zeros(768, dtype=torch.float32)
        for square in chess.SQUARES:
            piece = board.piece_at(square)
            if piece:
                p_idx = self.piece_map[piece.symbol()]
                # Il dizionario copre già 0-11, non serve moltiplicare il colore
                index = (p_idx * 64) + square
                tensor[index] = 1.0
        return tensor

    def fen_to_dual_tensor(self, fen):
        board = chess.Board(fen)
        
        # Calcoliamo la prospettiva del Bianco (Scacchiera normale)
        white_tensor = self.get_single_tensor(board)
        
        # Calcoliamo la prospettiva del Nero (Scacchiera capovolta e colori invertiti)
        black_tensor = self.get_single_tensor(board.mirror())
        
        # Se tocca al Bianco, 'us' è white. Se tocca al Nero, 'us' è black.
        if board.turn == chess.WHITE:
            return white_tensor, black_tensor
        else:
            return black_tensor, white_tensor


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
        x = self.clipped_relu(self.layer1(x))
        x = self.clipped_relu(self.layer2(x))
        x = self.output_layer(x)
        
        return torch.sigmoid(x)


def train_nnue(dataset_path, resume_from=None, num_epochs=100, steps_per_epoch=5000, batch_size=16384, learning_rate=0.001):
    
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Sto addestrando su: {device}")

    dataset = StreamingChessDataset(dataset_path)
    dataloader = DataLoader(dataset, batch_size=batch_size)

    model = BasicNNUE().to(device)
    
    if resume_from is not None and os.path.exists(resume_from):
        print(f"\n[!] Trovato checkpoint. Ripristino i pesi da: {resume_from}")
        model.load_state_dict(torch.load(resume_from, map_location=device))
        print("[!] Pesi caricati con successo. Riprendo l'addestramento...\n")
    elif resume_from is not None:
        print(f"\n[ATTENZIONE] Il file {resume_from} non esiste. Parto da zero.\n")
    
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)

    data_iterator = iter(dataloader)

    print(f"Inizio addestramento ({steps_per_epoch} steps per epoca)...")
    for epoch in range(num_epochs):
        model.train()
        running_loss = 0.0
        
        progress_bar = tqdm(range(steps_per_epoch), desc=f"Epoca {epoch+1}/{num_epochs}")
        
        for step in progress_bar:
            try:
                us_features, them_features, targets = next(data_iterator)
            except StopIteration:
                print("\n[Info] Fine del file raggiunta. Ricomincio il dataset dall'inizio...")
                data_iterator = iter(dataloader)
                us_features, them_features, targets = next(data_iterator)
            
            us_features = us_features.to(device)
            them_features = them_features.to(device)
            targets = targets.to(device)

            optimizer.zero_grad()
            outputs = model(us_features, them_features)
            loss = criterion(outputs, targets)
            
            loss.backward()
            optimizer.step()

            running_loss += loss.item()
            progress_bar.set_postfix({'Loss': f"{loss.item():.4f}"})

        avg_loss = running_loss / steps_per_epoch
        print(f"-> Fine Epoca {epoch+1} | Loss Media: {avg_loss:.4f}\n")

        checkpoint_path = f"basic_nnue_epoch_{epoch+1}.pth"
        torch.save(model.state_dict(), checkpoint_path)

    torch.save(model.state_dict(), "basic_nnue_weights_final.pth")
    print("Addestramento completato! Pesi salvati.")

if __name__ == "__main__":
    train_nnue(dataset_path = CLEAN_DATASET, 
               resume_from=None,
               num_epochs=100, 
               steps_per_epoch=5000, 
               batch_size=32768, 
               learning_rate=0.001)
