import io
import json
import math
import struct
import multiprocessing
import zstandard as zstd
from tqdm import tqdm
import chess

# ==========================================
# CONFIGURAZIONE
# ==========================================
MIN_DEPTH = 15
CP_LIM = 1000  
NROWS = 388_458_657
CHUNK_SIZE = 10000

FULL_DATASET = "/home/lorenzo/Dati/LichessDatasets/lichess_db_eval.jsonl.zst"
OUTPUT_BINARY = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_halfkp.bin"

SENTINEL = "END_OF_WORK"

# Struttura Binaria HalfKP: 32 indici US (uint16) + 32 indici THEM (uint16) + 1 Target (float32)
STRUCT_FORMAT = '<64Hf' 
PAD_VAL = 40960 

PIECE_MAP = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4,
    'p': 5, 'n': 6, 'b': 7, 'r': 8, 'q': 9
}

# ==========================================
# FILTRI DI PRE-ELABORAZIONE
# ==========================================
def extractData(line: str):
    try:
        data = json.loads(line)
        fen = data["fen"]
        
        bestEval = None
        for ev in data["evals"]:
            if bestEval is None:
                bestEval = ev
                continue
            if (ev["depth"] > bestEval["depth"]) or \
               ((ev["depth"] == bestEval["depth"]) and (ev["knodes"] > bestEval["knodes"])):
                bestEval = ev

        if bestEval is None or "pvs" not in bestEval or len(bestEval["pvs"]) == 0:
            return None

        pv = bestEval["pvs"][0]
        mate_val = pv.get("mate", None)
        cp_val = pv.get("cp", None)
        
        if cp_val is None and mate_val is None:
            return None

        return {
            "fen": fen,
            "depth": bestEval["depth"],
            "cp": cp_val if cp_val is not None else 0,
            "mate": mate_val,
            "line": pv.get("line", "")
        }
    except Exception:
        return None

def is_valid_position(data: dict, board: chess.Board) -> bool:
    if data["depth"] < MIN_DEPTH or data["mate"] is not None: return False
    if data["cp"] < -CP_LIM or data["cp"] > CP_LIM: return False
    if board.is_check(): return False
    
    if not data["line"] or len(data["line"].split()) == 0: return False
    uciMove = data["line"].split()[0]
    try:
        move = chess.Move.from_uci(uciMove)
        if move in board.legal_moves and (board.is_capture(move) or move.promotion is not None):
            return False
    except Exception:
        return False
        
    return True

# ==========================================
# TRASFORMAZIONE IN HALFKP BINARIO
# ==========================================
def fen_to_halfkp_bytes(fen: str, cp: int) -> bytes:
    parts = fen.split(' ')
    board_str = parts[0]
    is_white_turn = (parts[1] == 'w')
    
    wk_sq, bk_sq = -1, -1
    rank, file = 7, 0
    pieces = []
    
    for char in board_str:
        if char == '/':
            rank -= 1
            file = 0
        elif char.isdigit():
            file += int(char)
        else:
            sq = rank * 8 + file
            if char == 'K': wk_sq = sq
            elif char == 'k': bk_sq = sq
            else: pieces.append((PIECE_MAP[char], sq))
            file += 1
            
    if wk_sq == -1 or bk_sq == -1 or len(pieces) > 30:
        return b''
        
    white_perspectives = []
    black_perspectives = []
    
    for p_idx, sq in pieces:
        # Prospettiva Bianco
        w_idx = p_idx * 4096 + wk_sq * 64 + sq
        white_perspectives.append(w_idx)
        
        # Prospettiva Nero
        mirrored_bk = bk_sq ^ 56
        mirrored_sq = sq ^ 56
        p_idx_b = (p_idx + 5) % 10
        b_idx = p_idx_b * 4096 + mirrored_bk * 64 + mirrored_sq
        black_perspectives.append(b_idx)
        
    if is_white_turn:
        us_indices = white_perspectives
        them_indices = black_perspectives
    else:
        us_indices = black_perspectives
        them_indices = white_perspectives
            
    while len(us_indices) < 32: us_indices.append(PAD_VAL)
    while len(them_indices) < 32: them_indices.append(PAD_VAL)
    
    # Il cp di Lichess è già dal punto di vista di chi ha la mossa! Nessuna moltiplicazione.
    target = 1.0 / (1.0 + math.exp(-cp / 400.0))
    
    valori = us_indices + them_indices + [target]
    return struct.pack(STRUCT_FORMAT, *valori)

# ==========================================
# PIPELINE MULTIPROCESSING
# ==========================================
def reader_process(input_queue: multiprocessing.Queue, filepath: str, num_workers: int):
    dctx = zstd.ZstdDecompressor()
    with open(filepath, 'rb') as fh_r:
        with dctx.stream_reader(fh_r) as reader:
            text_reader = io.TextIOWrapper(reader, encoding='utf-8')
            chunk = []
            for line in text_reader:
                chunk.append(line)
                if len(chunk) >= CHUNK_SIZE:
                    input_queue.put(chunk)
                    chunk = []
            if chunk:
                input_queue.put(chunk)
                
    for _ in range(num_workers):
        input_queue.put(SENTINEL)

def worker_process(input_queue: multiprocessing.Queue, output_queue: multiprocessing.Queue):
    while True:
        chunk = input_queue.get()
        if chunk == SENTINEL:
            output_queue.put(SENTINEL)
            break
            
        byte_chunks = []
        total_read = 0
        kept_count = 0
        
        for line in chunk:
            total_read += 1
            data = extractData(line)
            if data is None: continue
                
            board = chess.Board(data["fen"])
            if not is_valid_position(data, board): continue
                
            # Trasformazione diretta in binario HalfKP
            bin_data = fen_to_halfkp_bytes(data["fen"], data["cp"])
            if bin_data:
                byte_chunks.append(bin_data)
                kept_count += 1
                
        output_queue.put((b''.join(byte_chunks), total_read, kept_count))

def writer_process(output_queue: multiprocessing.Queue, filepath: str, num_workers: int):
    total_pos_read = 0
    total_pos_kept = 0
    workers_finished = 0
    
    with open(filepath, 'wb') as f_out:
        with tqdm(total=NROWS, desc="Compilazione Binaria Diretta", unit=" pos") as pbar:
            while workers_finished < num_workers:
                result = output_queue.get()
                if result == SENTINEL:
                    workers_finished += 1
                    continue
                    
                raw_bytes, read_cnt, kept_cnt = result
                total_pos_read += read_cnt
                total_pos_kept += kept_cnt
                
                if raw_bytes:
                    f_out.write(raw_bytes)
                    
                pbar.update(read_cnt)
                pbar.set_postfix({"Kept": f"{total_pos_kept/total_pos_read*100:.2f}%"})
                
    print(f"\n[FINISH] Elaborazione completata!")
    print(f"Posizioni totali analizzate: {total_pos_read:,}")
    print(f"Posizioni scritte nel file .bin finale: {total_pos_kept:,} ({total_pos_kept/total_pos_read*100:.2f}%)")

def main():
    input_queue = multiprocessing.Queue(maxsize=30)
    output_queue = multiprocessing.Queue(maxsize=30)
    
    num_workers = max(1, multiprocessing.cpu_count() - 2)
    print(f"Avvio pipeline di compilazione diretta con {num_workers} worker CPU...")
    
    reader = multiprocessing.Process(target=reader_process, args=(input_queue, FULL_DATASET, num_workers))
    writer = multiprocessing.Process(target=writer_process, args=(output_queue, OUTPUT_BINARY, num_workers))
    
    workers = [multiprocessing.Process(target=worker_process, args=(input_queue, output_queue)) for _ in range(num_workers)]
    
    writer.start()
    reader.start()
    for w in workers: w.start()
        
    reader.join()
    for w in workers: w.join()
    writer.join()

if __name__ == "__main__":
    main()