import json
from typing import Any, Dict, Optional
import zstandard as zstd
from tqdm import tqdm
import chess

MIN_DEPTH = 15
CP_LIM = 1000  # 10 pedoni

FULL_DATASET = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/lichess_db_eval.jsonl.zst"
CLEAN_DATASET = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/data/clean_db_eval.jsonl.zst"

def readFen(fen: str) -> float:
    sp = fen.split(" ")
    color = sp[1]
    if color == "w":
        return 1
    return -1

def depthFilter(data: Dict[str, Any]) -> bool:
    return data["depth"] < MIN_DEPTH

def checkMateFilter(data: Dict[str, Any]) -> bool:
    return data["mate"] is not None

def scoreFilter(data: Dict[str, Any]) -> bool:
    return data["cp"] < -CP_LIM or data["cp"] > CP_LIM

def checkFilter(board: chess.Board) -> bool:
    return board.is_check()

def quiescenceFilter(board: chess.Board, data: Dict[str, Any]) -> bool:
    uciMove = data["line"].split()[0]
    move = chess.Move.from_uci(uciMove)
    is_capture = board.is_capture(move)
    is_promotion = move.promotion is not None
    return is_capture or is_promotion

def getRejectionReason(data: Dict[str, Any]) -> str:
    if depthFilter(data): return "rejected_depth"
    if checkMateFilter(data): return "rejected_mate"
    if scoreFilter(data): return "rejected_score"

    board = chess.Board(data["fen"])
    if checkFilter(board): return "rejected_check"
    if quiescenceFilter(board, data): return "rejected_quiescence"
    
    return "kept"

def extractData(line: str) -> Optional[Dict[str, Any]]:
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
        "knodes": bestEval["knodes"],
        "depth": bestEval["depth"],
        "cp": cp_val if cp_val is not None else 0,
        "mate": mate_val,
        "line": pv.get("line", "")
    }

def print_statistics(stats: Dict[str, int]):
    total = stats["total"]
    if total == 0:
        print("\nNessuna riga processata.")
        return

    print("\n" + "="*50)
    print(" 📊 REPORT ESTRAZIONE DATASET NNUE")
    print("="*50)
    print(f"Totale posizioni lette:    {total:,}".replace(",", "."))
    print(f"Posizioni salvate (KEPT):  {stats['kept']:,} ({stats['kept']/total*100:.2f}%)".replace(",", "."))
    print("-" * 50)
    print("Motivi di scarto:")
    
    print(f" - Dati mancanti/invalidi: {stats['missing_data']:,} ({stats['missing_data']/total*100:.2f}%)".replace(",", "."))
    
    reasons = [
        ("Profondità < 15", "rejected_depth"),
        ("Matto forzato", "rejected_mate"),
        ("Score estremo (>10)", "rejected_score"),
        ("Re sotto scacco", "rejected_check"),
        ("Tattica (Catture/Prom)", "rejected_quiescence")
    ]
    
    for label, key in reasons:
        count = stats[key]
        print(f" - {label:<22}: {count:,} ({count/total*100:.2f}%)".replace(",", "."))
    print("="*50 + "\n")


def main():
    stats = {
        "total": 0,
        "kept": 0,
        "missing_data": 0,
        "rejected_depth": 0,
        "rejected_mate": 0,
        "rejected_score": 0,
        "rejected_check": 0,
        "rejected_quiescence": 0
    }

    with open(FULL_DATASET, 'rb') as fh_r, open(CLEAN_DATASET, 'wb') as fh_w:
        dctx = zstd.ZstdDecompressor()
        cctx = zstd.ZstdCompressor()
        
        with dctx.stream_reader(fh_r) as reader, cctx.stream_writer(fh_w) as writer:
            import io
            text_reader = io.TextIOWrapper(reader, encoding='utf-8')
            
            for line in tqdm(text_reader, desc="Processando le posizioni"):
                stats["total"] += 1
                
                data = extractData(line)
                if data is None:
                    stats["missing_data"] += 1
                    continue
                    
                status = getRejectionReason(data)                
                stats[status] += 1
                
                if status == "kept":
                    sign = readFen(data["fen"])
                    out_dict = {"fen": data["fen"], "cp": int(sign * data["cp"])}
                    out_line = (json.dumps(out_dict) + "\n").encode('utf-8')
                    writer.write(out_line)

    print_statistics(stats)

if __name__ == "__main__":
    main()