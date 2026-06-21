import os
import queue
import threading
import concurrent.futures
import subprocess

import time
import random

import chess
import chess.pgn
import chess.engine

BASEPATH   = "./versions/"
ENGINE_A   = BASEPATH + "engine_v2"
ENGINE_B   = BASEPATH + "engine_v2"
NUM_GAMES  = 1
MAX_WORKERS = min(8, os.cpu_count() or 1, NUM_GAMES)

LOG_A = True
LOG_B = False

# ---------------------------------------------------------------------------
# Gestione processi persistenti
# ---------------------------------------------------------------------------

def start_engine(path: str) -> subprocess.Popen:
    return subprocess.Popen(
        [path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,          # line-buffered: ogni \n svuota il buffer lato Python
    )

def stop_engine(proc: subprocess.Popen) -> None:
    try:
        proc.stdin.write("quit\n")
        proc.stdin.flush()
        proc.wait(timeout=2)
    except Exception:
        proc.kill()

def send_move(proc: subprocess.Popen, move_uci: str) -> None:
    """Invia una mossa al processo (aggiorna il suo stato interno)."""
    proc.stdin.write(move_uci + "\n")
    proc.stdin.flush()

def request_move(proc: subprocess.Popen, timeout: float = 30.0) -> str | None:
    """
    Invia 'go' e attende la risposta con un timeout.
    La lettura avviene in un thread separato per non bloccare il chiamante.
    """
    result_q: queue.Queue[str | None] = queue.Queue()

    def _reader():
        try:
            line = proc.stdout.readline()
            result_q.put(line.strip() if line else None)
        except Exception:
            result_q.put(None)

    proc.stdin.write("go\n")
    proc.stdin.flush()

    t = threading.Thread(target=_reader, daemon=True)
    t.start()

    try:
        return result_q.get(timeout=timeout)
    except queue.Empty:
        print("  [WARN] Timeout del motore!")
        return None


# ---------------------------------------------------------------------------
# Logica di partita
# ---------------------------------------------------------------------------

def play_game(white_path: str, black_path: str, fen: str) -> tuple[str, chess.pgn.Game]:
    board = chess.Board(fen)

    game = chess.pgn.Game()
    game.setup(board) # Configura il PGN per partire da questa posizione FEN
    game.headers["Event"] = "YACE Gauntlet"
    game.headers["White"] = white_path.split("/")[-1]
    game.headers["Black"] = black_path.split("/")[-1]
    game.headers["FEN"] = fen # Salva il FEN nei metadati del PGN
    node = game

    # Avvia i processi dei motori
    proc_w = start_engine(white_path)
    proc_b = start_engine(black_path)

    try:
        # --- ATTIVAZIONE LOG (Opzionale) ---
        if LOG_A:
            proc_w.stdin.write("log on\n")
        if LOG_B:
            proc_b.stdin.write("log on\n")

        # --- SINCRONIZZAZIONE ENGINES ---
        proc_w.stdin.write(f"fen {fen}\n")
        proc_b.stdin.write(f"fen {fen}\n")
        proc_w.stdin.flush()
        proc_b.stdin.flush()

        while not board.is_game_over():
            active, passive = (proc_w, proc_b) if board.turn == chess.WHITE else (proc_b, proc_w)

            move_uci = request_move(active)
            if not move_uci:
                result = "1-0" if board.turn == chess.BLACK else "0-1"
                game.headers["Result"] = result
                return result, game

            try:
                move = chess.Move.from_uci(move_uci)
            except ValueError:
                return "Errore Formato", game

            if move not in board.legal_moves:
                print(f"\n  [DEBUG] Mossa ILLEGALE: '{move_uci}'")
                print(f"  [DEBUG] Legali: {[m.uci() for m in board.legal_moves]}")
                result = "1-0" if board.turn == chess.BLACK else "0-1"
                game.headers["Result"] = result
                return result, game

            board.push(move)
            node = node.add_variation(move)

            send_move(proc_w, move_uci)
            send_move(proc_b, move_uci)

    finally:
        stop_engine(proc_w)
        stop_engine(proc_b)

    game.headers["Result"] = board.result()
    return board.result(), game

def generate_random_opening_fen() -> str:
    board = chess.Board()
    
    with chess.engine.SimpleEngine.popen_uci("stockfish") as sf:
        for _ in range(4):
            if board.is_game_over():
                break
            move = random.choice(list(board.legal_moves))
            board.push(move)
            
        for _ in range(2):
            if board.is_game_over():
                break
            result = sf.play(board, chess.engine.Limit(time=0.1))
            if result.move:
                board.push(result.move)
                
    return board.fen()

# ---------------------------------------------------------------------------
# Task parallelo (un thread = una partita completa)
# ---------------------------------------------------------------------------

def run_match_task(match_id: int) -> dict:
    if match_id % 2 == 0:
        white, black = ENGINE_A, ENGINE_B
        white_name, black_name = "Engine A", "Engine B"
    else:
        white, black = ENGINE_B, ENGINE_A
        white_name, black_name = "Engine B", "Engine A"

    start_fen = generate_random_opening_fen()

    t0 = time.time()
    result, game = play_game(white, black, start_fen) 
    
    return {
        "id":         match_id + 1,
        "white":      white,
        "black":      black,
        "white_name": white_name,
        "black_name": black_name,
        "result":     result,
        "game":       game,
        "duration":   time.time() - t0,
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print(f"--- ARENA: {ENGINE_A} vs {ENGINE_B} ---")
    print(f"Partite: {NUM_GAMES}  |  Thread: {MAX_WORKERS}\n")

    score = {"Engine A": 0, "Engine B": 0, "Draws": 0}

    with open("gauntlet_results.pgn", "w", encoding="utf-8") as pgn_file:
        with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
            futures = [ex.submit(run_match_task, i) for i in range(NUM_GAMES)]

            for future in concurrent.futures.as_completed(futures):
                res = future.result()
                r   = res["result"]

                print(f"Partita {res['id']:>2}/{NUM_GAMES}: "
                      f"{res['white_name']} (B) vs {res['black_name']} (N) — ", end="", flush=True)

                print(res["game"], file=pgn_file, end="\n\n")
                pgn_file.flush()

                if r == "1-0":
                    score[res["white_name"]] += 1
                    print(f"Vittoria BIANCO  ({res['duration']:.1f}s)")
                elif r == "0-1":
                    score[res["black_name"]] += 1
                    print(f"Vittoria NERO    ({res['duration']:.1f}s)")
                elif r == "1/2-1/2":
                    score["Draws"] += 1
                    print(f"Patta            ({res['duration']:.1f}s)")

    total_games = score["Engine A"] + score["Engine B"] + score["Draws"]
    
    if total_games > 0:
        win_a_pct = (score["Engine A"] / total_games) * 100
        win_b_pct = (score["Engine B"] / total_games) * 100
        draw_pct  = (score["Draws"] / total_games) * 100
    else:
        win_a_pct = win_b_pct = draw_pct = 0.0

    print("\n" + "=" * 45)
    print("RISULTATI FINALI")
    print("=" * 45)
    print(f"{'Engine A':<15} : {score['Engine A']:>3} vittorie ({win_a_pct:>5.1f}%)")
    print(f"{'Engine B':<15} : {score['Engine B']:>3} vittorie ({win_b_pct:>5.1f}%)")
    print(f"{'Patte':<15} : {score['Draws']:>3} patte    ({draw_pct:>5.1f}%)")
    print("-" * 45)
    print(f"{'Totale partite':<15} : {total_games}")
    print("=" * 45)

if __name__ == "__main__":
    main()