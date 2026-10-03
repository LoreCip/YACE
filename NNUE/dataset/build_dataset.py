"""Dump Lichess delle valutazioni (jsonl.zst) → dataset binario HalfKP (vedi halfkp.DATASET_DTYPE)."""
import io
import json
import multiprocessing
import struct
import sys
from pathlib import Path

import chess
import zstandard as zstd
from tqdm import tqdm

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # moduli condivisi in NNUE/
from halfkp import DATA_DIR, cp_to_target, fen_to_features

# ==========================================
# CONFIGURAZIONE
# ==========================================
MIN_DEPTH = 15
CP_LIM = 1000
NROWS = 388_458_657
CHUNK_SIZE = 10000

FULL_DATASET = DATA_DIR / "lichess_db_eval.jsonl.zst"
OUTPUT_BINARY = DATA_DIR / "clean_db_halfkp.bin"

SENTINEL = "END_OF_WORK"

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
    features = fen_to_features(fen)
    if features is None:
        return b''
    us, them = features

    # Il cp del dump Lichess è relativo al BIANCO, ma le feature HalfKP (us/them) sono
    # relative a chi deve muovere. Senza questo flip il target sarebbe invertito per ogni
    # posizione con il nero di turno (~metà del dataset) — bug confermato empiricamente:
    # correlazione quasi nulla (0.04) tra il materiale lato-di-mossa e il target originale.
    if fen.split(' ')[1] != 'w':
        cp = -cp

    return struct.pack('<64Hf', *us, *them, cp_to_target(cp))

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