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
ENGINE_B   = BASEPATH + "engine_v3"
NUM_GAMES  = 1
MAX_WORKERS = min(8, os.cpu_count() or 1, NUM_GAMES)

LOG_A = True
LOG_B = False

# --- CONFIGURAZIONE TEMPO DI GIOCO ---
START_TIME_MS = 50000   # Tempo iniziale per giocatore in ms
INCREMENT_MS  = 100    # Incremento a ogni mossa in ms


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
        bufsize=1,
    )

def stop_engine(proc: subprocess.Popen) -> None:
    try:
        proc.stdin.write("quit\n")
        proc.stdin.flush()
        proc.wait(timeout=2)
    except Exception:
        proc.kill()

def send_move(proc: subprocess.Popen, move_uci: str) -> None:
    proc.stdin.write("move " + move_uci + "\n")
    proc.stdin.flush()

def request_move(proc: subprocess.Popen, go_command: str, timeout: float) -> str | None:
    """
    Invia il comando 'go' (con i tempi logici) e attende la risposta con un timeout di sicurezza.
    """
    result_q: queue.Queue[str | None] = queue.Queue()

    def _reader():
        try:
            line = proc.stdout.readline()
            result_q.put(line.strip() if line else None)
        except Exception:
            result_q.put(None)

    proc.stdin.write(go_command + "\n")
    proc.stdin.flush()

    t = threading.Thread(target=_reader, daemon=True)
    t.start()

    try:
        return result_q.get(timeout=timeout)
    except queue.Empty:
        print("  [WARN] Timeout assoluto (Il motore si è bloccato e non ha rispettato il wtime/btime)!")
        return None


# ---------------------------------------------------------------------------
# Logica di partita
# ---------------------------------------------------------------------------
def play_game(white_path: str, black_path: str, fen: str) -> tuple[str, chess.pgn.Game]:
    board = chess.Board(fen)

    game = chess.pgn.Game()
    game.setup(board)
    game.headers["Event"] = "YACE Gauntlet"
    game.headers["White"] = white_path.split("/")[-1]
    game.headers["Black"] = black_path.split("/")[-1]
    game.headers["FEN"] = fen
    
    # Inizializza gli orologi
    wtime = float(START_TIME_MS)
    btime = float(START_TIME_MS)
    
    node = game

    proc_w = start_engine(white_path)
    proc_b = start_engine(black_path)

    termination_reason = "Normal"

    try:
        if LOG_A: proc_w.stdin.write("log on\n")
        if LOG_B: proc_b.stdin.write("log on\n")

        proc_w.stdin.write(f"fen {fen}\n")
        proc_b.stdin.write(f"fen {fen}\n")
        proc_w.stdin.flush()
        proc_b.stdin.flush()

        while not board.is_game_over():
            is_white_turn = (board.turn == chess.WHITE)
            active, passive = (proc_w, proc_b) if is_white_turn else (proc_b, proc_w)

            # Costruisci il comando UCI con il tempo rimanente
            go_cmd = f"go wtime {int(wtime)} btime {int(btime)}"
            
            my_time_sec = (wtime if is_white_turn else btime) / 1000.0
            safety_timeout = max(5.0, my_time_sec + 2.0) 

            t0 = time.time()
            response = request_move(active, go_cmd, timeout=safety_timeout)
            t1 = time.time()

            # Scaliamo il tempo consumato
            elapsed_ms = (t1 - t0) * 1000.0
            if is_white_turn:
                wtime -= elapsed_ms
                if wtime < 0:
                    game.headers["Result"] = "0-1"
                    game.headers["Termination"] = "Time forfeiture"
                    node.comment = "White loses on time"
                    return "0-1 (Timeout)", game
                wtime += INCREMENT_MS
            else:
                btime -= elapsed_ms
                if btime < 0:
                    game.headers["Result"] = "1-0"
                    game.headers["Termination"] = "Time forfeiture"
                    node.comment = "Black loses on time"
                    return "1-0 (Timeout)", game
                btime += INCREMENT_MS

            if not response:
                result = "1-0" if not is_white_turn else "0-1"
                game.headers["Result"] = result
                game.headers["Termination"] = "Rules infraction"
                node.comment = f"{'White' if is_white_turn else 'Black'} engine crashed"
                return result + " (Crash)", game

            # Estrai la mossa vera e propria 
            move_uci = response.split()[1] if response.startswith("bestmove") else response.strip()

            try:
                move = chess.Move.from_uci(move_uci)
            except ValueError:
                game.headers["Termination"] = "Abandoned"
                node.comment = f"Format error from {'White' if is_white_turn else 'Black'}"
                return "Errore Formato", game

            if move not in board.legal_moves:
                print(f"\n  [DEBUG] Mossa ILLEGALE: '{move_uci}'")
                result = "1-0" if not is_white_turn else "0-1"
                game.headers["Result"] = result
                game.headers["Termination"] = "Rules infraction"
                node.comment = f"Illegal move played by {'White' if is_white_turn else 'Black'}: {move_uci}"
                return result + " (Illegale)", game

            board.push(move)
            node = node.add_variation(move)

            send_move(passive, move_uci)

    finally:
        stop_engine(proc_w)
        stop_engine(proc_b)

    # Gestione dei motivi di fine partita regolamentari (Scacco matto, Patta, ecc.)
    result = board.result()
    game.headers["Result"] = result

    if board.is_checkmate():
        game.headers["Termination"] = "Mate"
        node.comment = "Checkmate"
    elif board.is_stalemate():
        game.headers["Termination"] = "Stalemate"
        node.comment = "Stalemate"
    elif board.is_insufficient_material():
        game.headers["Termination"] = "Insufficient material"
        node.comment = "Draw by insufficient material"
    elif board.is_fivefold_repetition() or board.is_repetition(3):
        game.headers["Termination"] = "Repetition"
        node.comment = "Draw by repetition"
    elif board.is_fifty_moves():
        game.headers["Termination"] = "50-move rule"
        node.comment = "Draw by 50-move rule"
    else:
        game.headers["Termination"] = "Normal"

    return result, game

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