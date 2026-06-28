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

BASEPATH   = "/home/lorenzo/Scrivania/Projects/YACE/gauntlet/versions/"
VA = "engine_v4"
VB = "engine_v1"
ENGINE_A   = BASEPATH + VA
ENGINE_B   = BASEPATH + VB
NUM_GAMES  = 100
MAX_WORKERS = 1 

LOG_A = False
LOG_B = False

# --- FLAG PER IL LOGGING DELLA COMUNICAZIONE ---
DEBUG_COMM = False 

# --- CONFIGURAZIONE TEMPO DI GIOCO ---
START_TIME_MS = 5000   # Tempo iniziale per giocatore in ms
INCREMENT_MS  = 100    # Incremento a ogni mossa in ms

# ---------------------------------------------------------------------------
# Funzioni di I/O con Intercettazione
# ---------------------------------------------------------------------------

def send_command(engine, command, engine_name="Engine"):
    """Invia un comando al motore e, se DEBUG_COMM è attivo, lo stampa a schermo."""
    clean_cmd = command.strip()
    if DEBUG_COMM:
        print(f"\033[94m[GAUNTLET -> {engine_name}]\033[0m {clean_cmd}") # Testo blu
    
    engine.stdin.write(clean_cmd + "\n")
    engine.stdin.flush()

def read_line(engine, engine_name="Engine"):
    """Legge una riga dal motore e, se DEBUG_COMM è attivo, la stampa a schermo."""
    line = engine.stdout.readline()
    if line and DEBUG_COMM:
        clean_line = line.strip()
        print(f"\033[92m[{engine_name} -> GAUNTLET]\033[0m {clean_line}") # Testo verde
    return line

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

def stop_engine(proc: subprocess.Popen, engine_name="Engine") -> None:
    try:
        if proc.poll() is None: 
            send_command(proc, "quit", engine_name)
            proc.wait(timeout=2)
    except Exception:
        pass
    finally:
        if proc.poll() is None:
            proc.kill()
        if proc.stdin: proc.stdin.close()
        if proc.stdout: proc.stdout.close()
        if proc.stderr: proc.stderr.close()

def send_move(proc: subprocess.Popen, move_uci: str, engine_name="Engine") -> None:
    send_command(proc, "move " + move_uci, engine_name)

def request_move(proc: subprocess.Popen, go_command: str, timeout: float, engine_name="Engine") -> str | None:
    """
    Invia il comando 'go' e attende la risposta con timeout, tracciando i log.
    """
    result_q: queue.Queue[str | None] = queue.Queue()

    def _reader():
        try:
            line = read_line(proc, engine_name)
            result_q.put(line.strip() if line else None)
        except Exception:
            result_q.put(None)

    send_command(proc, go_command, engine_name)

    t = threading.Thread(target=_reader, daemon=True)
    t.start()

    try:
        return result_q.get(timeout=timeout)
    except queue.Empty:
        print(f"  [WARN] Timeout assoluto (Il motore {engine_name} si è bloccato e non ha rispettato il tempo)!")
        return None
    
# ---------------------------------------------------------------------------
# Logica di partita
# ---------------------------------------------------------------------------
def play_game(white_path: str, black_path: str, fen: str) -> tuple[str, chess.pgn.Game]:
    board = chess.Board(fen)

    white_name = white_path.split("/")[-1]
    black_name = black_path.split("/")[-1]

    game = chess.pgn.Game()
    game.setup(board)
    game.headers["Event"] = "YACE Gauntlet"
    game.headers["White"] = white_name
    game.headers["Black"] = black_name
    game.headers["FEN"] = fen
    
    # Inizializza gli orologi
    wtime = float(START_TIME_MS)
    btime = float(START_TIME_MS)
    
    node = game

    proc_w = start_engine(white_path)
    proc_b = start_engine(black_path)

    try:
        if LOG_A: send_command(proc_w, "log on", white_name)
        if LOG_B: send_command(proc_b, "log on", black_name)

        send_command(proc_w, f"fen {fen}", white_name)
        send_command(proc_b, f"fen {fen}", black_name)

        while not board.is_game_over():
            is_white_turn = (board.turn == chess.WHITE)
            
            # Assegna il processo attivo/passivo e i rispettivi nomi per i log
            active, passive = (proc_w, proc_b) if is_white_turn else (proc_b, proc_w)
            active_name = white_name if is_white_turn else black_name
            passive_name = black_name if is_white_turn else white_name

            # Costruisci il comando UCI con il tempo rimanente
            go_cmd = f"go wtime {int(wtime)} btime {int(btime)}"
            
            my_time_sec = (wtime if is_white_turn else btime) / 1000.0
            safety_timeout = max(5.0, my_time_sec + 2.0) 

            t0 = time.time()
            response = request_move(active, go_cmd, timeout=safety_timeout, engine_name=active_name)
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
                node.comment = f"{active_name} engine crashed"
                return result + " (Crash)", game

            # Estrai la mossa vera e propria 
            move_uci = response.split()[1] if response.startswith("bestmove") else response.strip()

            try:
                move = chess.Move.from_uci(move_uci)
            except ValueError:
                game.headers["Termination"] = "Abandoned"
                node.comment = f"Format error from {active_name}"
                return "Errore Formato", game

            if move not in board.legal_moves:
                print(f"\n  [DEBUG] Mossa ILLEGALE: '{move_uci}' giocata da {active_name}")
                result = "1-0" if not is_white_turn else "0-1"
                game.headers["Result"] = result
                game.headers["Termination"] = "Rules infraction"
                node.comment = f"Illegal move played by {active_name}: {move_uci}"
                return result + " (Illegale)", game

            board.push(move)
            node = node.add_variation(move)

            # Invia la mossa all'avversario
            send_move(passive, move_uci, engine_name=passive_name)

    finally:
        stop_engine(proc_w, white_name)
        stop_engine(proc_b, black_name)

    # Gestione dei motivi di fine partita regolamentari
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

    if DEBUG_COMM:
        print(f"\n--- INIZIO PARTITA {match_id + 1} ---")

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
    if not DEBUG_COMM:
        print(f"--- ARENA: {VA} vs {VB} ---")
        print(f"Partite: {NUM_GAMES}  |  Thread: {MAX_WORKERS}\n")

    score = {"Engine A": 0, "Engine B": 0, "Draws": 0}

    with open("gauntlet_results.pgn", "w", encoding="utf-8") as pgn_file:
        with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
            futures = [ex.submit(run_match_task, i) for i in range(NUM_GAMES)]

            for future in concurrent.futures.as_completed(futures):
                res = future.result()
                r   = res["result"]

                if not DEBUG_COMM:
                    print(f"Partita {res['id']:>2}/{NUM_GAMES}: "
                          f"{res['white_name']} (B) vs {res['black_name']} (N) — ", end="", flush=True)

                print(res["game"], file=pgn_file, end="\n\n")
                pgn_file.flush()

                if r == "1-0":
                    score[res["white_name"]] += 1
                    if not DEBUG_COMM: print(f"Vittoria BIANCO  ({res['duration']:.1f}s)")
                elif r == "0-1":
                    score[res["black_name"]] += 1
                    if not DEBUG_COMM: print(f"Vittoria NERO    ({res['duration']:.1f}s)")
                elif r == "1/2-1/2":
                    score["Draws"] += 1
                    if not DEBUG_COMM: print(f"Patta            ({res['duration']:.1f}s)")
                
                if DEBUG_COMM:
                    print(f"--- FINE PARTITA {res['id']} (Risultato: {r}) ---")

    total_games = score["Engine A"] + score["Engine B"] + score["Draws"]
    
    if total_games > 0:
        win_a_pct = (score["Engine A"] / total_games) * 100
        win_b_pct = (score["Engine B"] / total_games) * 100
        draw_pct  = (score["Draws"] / total_games) * 100
    else:
        win_a_pct = win_b_pct = draw_pct = 0.0

    print("\n" + "="*45)
    print(f"{VA:<15} : {score['Engine A']:>3} vittorie ({win_a_pct:>5.1f}%)")
    print(f"{VB:<15} : {score['Engine B']:>3} vittorie ({win_b_pct:>5.1f}%)")
    print(f"{'Patte':<15} : {score['Draws']:>3} patte    ({draw_pct:>5.1f}%)")
    print("-" * 45)
    print(f"{'Totale partite':<15} : {total_games}")
    print("="*45)

if __name__ == "__main__":
    main()