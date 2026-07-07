import os
import queue
import threading
import concurrent.futures
import subprocess
import time
import random
import math

import chess
import chess.pgn
import chess.engine

# --- CONFIGURAZIONE MOTORI ---
BASEPATH    = "/home/lorenzo/Scrivania/Projects/YACE/gauntlet/versions/"
ENGINE_YACE = BASEPATH + "engine_v5"
ENGINE_SF   = "stockfish" 

VA = "YACE_v5"
VB = "Stockfish"
ENGINE_A = ENGINE_YACE
ENGINE_B = ENGINE_SF
NUM_GAMES  = 100
MAX_WORKERS = 1 

# --- CONFIGURAZIONE ELO STOCKFISH ---
LIMIT_STOCKFISH = True
TARGET_ELO = 2000

LOG_A = False
LOG_B = False
DEBUG_COMM = False 

START_TIME_MS = 5000   
INCREMENT_MS  = 100    

# ---------------------------------------------------------------------------
# Utilità Matematica per ELO
# ---------------------------------------------------------------------------
def calculate_elo_difference(wins: int, losses: int, draws: int) -> float:
    total = wins + losses + draws
    if total == 0:
        return 0.0

    score = wins + (0.5 * draws)
    p = score / total

    p = max(0.001, min(0.999, p))
    elo_diff = -400 * math.log10((1 / p) - 1)
    return elo_diff

# ---------------------------------------------------------------------------
# Funzioni di I/O con Intercettazione
# ---------------------------------------------------------------------------
def send_command(engine, command, engine_name="Engine"):
    clean_cmd = command.strip()
    if DEBUG_COMM:
        print(f"\n\033[94m[GAUNTLET -> {engine_name}]\033[0m {clean_cmd}") 
    engine.stdin.write(clean_cmd + "\n")
    engine.stdin.flush()

def read_line(engine, engine_name="Engine"):
    line = engine.stdout.readline()
    if line and DEBUG_COMM:
        clean_line = line.strip()
        print(f"\033[92m[{engine_name} -> GAUNTLET]\033[0m {clean_line}") 
    return line

# ---------------------------------------------------------------------------
# Gestione processi persistenti
# ---------------------------------------------------------------------------
def start_engine(path: str) -> subprocess.Popen:
    return subprocess.Popen(
        [path],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, bufsize=1,
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
    if "stockfish" not in engine_name.lower():
        send_command(proc, "move " + move_uci, engine_name)

def request_move(proc: subprocess.Popen, go_command: str, timeout: float, engine_name="Engine") -> str | None:
    result_q: queue.Queue[str | None] = queue.Queue()

    def _reader():
        try:
            while True:
                line = read_line(proc, engine_name)
                if not line:
                    result_q.put(None)
                    break
                clean_line = line.strip()
                if clean_line.startswith("bestmove"):
                    result_q.put(clean_line)
                    break
        except Exception:
            result_q.put(None)

    send_command(proc, go_command, engine_name)
    t = threading.Thread(target=_reader, daemon=True)
    t.start()

    try:
        return result_q.get(timeout=timeout)
    except queue.Empty:
        return None
    
# ---------------------------------------------------------------------------
# Logica di partita
# ---------------------------------------------------------------------------
def setup_engine_position(proc, fen, engine_name):
    if "stockfish" in engine_name.lower():
        send_command(proc, f"position fen {fen}", engine_name)
    else:
        send_command(proc, f"fen {fen}", engine_name)

def play_game(white_path: str, black_path: str, start_fen: str) -> tuple[str, chess.pgn.Game]:
    board = chess.Board(start_fen)

    white_name = VB if "stockfish" in white_path else VA
    black_name = VB if "stockfish" in black_path else VA

    game = chess.pgn.Game()
    game.setup(board)
    game.headers["Event"] = "YACE Gauntlet"
    game.headers["White"] = white_name
    game.headers["Black"] = black_name
    game.headers["FEN"] = start_fen
    
    wtime = float(START_TIME_MS)
    btime = float(START_TIME_MS)
    node = game

    proc_w = start_engine(white_path)
    proc_b = start_engine(black_path)

    try:
        if "stockfish" in white_name.lower():
            send_command(proc_w, "uci", white_name)
            if LIMIT_STOCKFISH:
                send_command(proc_w, "setoption name UCI_LimitStrength value true", white_name)
                send_command(proc_w, f"setoption name UCI_Elo value {TARGET_ELO}", white_name)
            send_command(proc_w, "ucinewgame", white_name)

        if "stockfish" in black_name.lower():
            send_command(proc_b, "uci", black_name)
            if LIMIT_STOCKFISH:
                send_command(proc_b, "setoption name UCI_LimitStrength value true", black_name)
                send_command(proc_b, f"setoption name UCI_Elo value {TARGET_ELO}", black_name)
            send_command(proc_b, "ucinewgame", black_name)

        while not board.is_game_over():
            is_white_turn = (board.turn == chess.WHITE)
            
            active, passive = (proc_w, proc_b) if is_white_turn else (proc_b, proc_w)
            active_name = white_name if is_white_turn else black_name
            passive_name = black_name if is_white_turn else white_name

            setup_engine_position(active, board.fen(), active_name)
            go_cmd = f"go wtime {int(wtime)} btime {int(btime)}"
            
            my_time_sec = (wtime if is_white_turn else btime) / 1000.0
            safety_timeout = max(5.0, my_time_sec + 2.0) 

            t0 = time.time()
            response = request_move(active, go_cmd, timeout=safety_timeout, engine_name=active_name)
            t1 = time.time()

            elapsed_ms = (t1 - t0) * 1000.0
            if is_white_turn:
                wtime -= elapsed_ms
                if wtime < 0: return "0-1 (Timeout)", game
                wtime += INCREMENT_MS
            else:
                btime -= elapsed_ms
                if btime < 0: return "1-0 (Timeout)", game
                btime += INCREMENT_MS

            if not response:
                return ("1-0 (Crash)" if not is_white_turn else "0-1 (Crash)"), game

            move_uci = response.split()[1] if response.startswith("bestmove") else response.strip()

            try:
                move = chess.Move.from_uci(move_uci)
            except ValueError:
                return "Errore Formato", game

            if move not in board.legal_moves:
                return ("1-0 (Illegale)" if not is_white_turn else "0-1 (Illegale)"), game

            board.push(move)
            node = node.add_variation(move)
            send_move(passive, move_uci, engine_name=passive_name)

    finally:
        stop_engine(proc_w, white_name)
        stop_engine(proc_b, black_name)

    result = board.result()
    game.headers["Result"] = result
    return result, game

def generate_random_opening_fen() -> str:
    board = chess.Board()
    with chess.engine.SimpleEngine.popen_uci("stockfish") as sf:
        for _ in range(4):
            if board.is_game_over(): break
            move = random.choice(list(board.legal_moves))
            board.push(move)
        for _ in range(2):
            if board.is_game_over(): break
            result = sf.play(board, chess.engine.Limit(time=0.1))
            if result.move:
                board.push(result.move)
    return board.fen()

# ---------------------------------------------------------------------------
# Task parallelo 
# ---------------------------------------------------------------------------
def run_match_task(match_id: int) -> dict:
    if match_id % 2 == 0:
        white, black = ENGINE_A, ENGINE_B
        white_name, black_name = VA, VB
    else:
        white, black = ENGINE_B, ENGINE_A
        white_name, black_name = VB, VA

    start_fen = generate_random_opening_fen()
    t0 = time.time()
    result, game = play_game(white, black, start_fen) 
    
    return {
        "id":         match_id + 1,
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
        if LIMIT_STOCKFISH:
            print(f"Forza {VB} limitata a: {TARGET_ELO} Elo")
        print(f"Partite: {NUM_GAMES}  |  Thread: {MAX_WORKERS}\n")

    score = {VA: 0, VB: 0, "Draws": 0}

    # Stampa iniziale del contatore vuoto
    if not DEBUG_COMM:
        print(f"\r[Progresso: 0/{NUM_GAMES}] {VA}: 0 | {VB}: 0 | Patte: 0", end="", flush=True)

    with open("gauntlet_results.pgn", "w", encoding="utf-8") as pgn_file:
        with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
            futures = [ex.submit(run_match_task, i) for i in range(NUM_GAMES)]

            for future in concurrent.futures.as_completed(futures):
                res = future.result()
                r   = res["result"]

                # Scrittura silenziosa nel file PGN
                print(res["game"], file=pgn_file, end="\n\n")
                pgn_file.flush()

                # Aggiornamento punteggio
                if r == "1-0":
                    score[res["white_name"]] += 1
                elif r == "0-1":
                    score[res["black_name"]] += 1
                elif r == "1/2-1/2":
                    score["Draws"] += 1

                # Aggiornamento riga nel terminale (sovrascrive la precedente)
                if not DEBUG_COMM:
                    giocate = score[VA] + score[VB] + score["Draws"]
                    # \033[K pulisce la riga fino alla fine, utile se la stringa si accorcia
                    print(f"\r\033[K[Progresso: {giocate}/{NUM_GAMES}] {VA}: {score[VA]} | {VB}: {score[VB]} | Patte: {score['Draws']}", end="", flush=True)

    # Vai a capo una volta finito il ciclo
    if not DEBUG_COMM:
        print("\n")

    # Calcolo dei totali
    yace_wins = score[VA]
    sf_wins = score[VB]
    draws = score["Draws"]
    total_games = yace_wins + sf_wins + draws
    
    if total_games > 0:
        win_a_pct = (yace_wins / total_games) * 100
        win_b_pct = (sf_wins / total_games) * 100
        draw_pct  = (draws / total_games) * 100
        
        # Calcolo Elo Diff
        elo_diff = calculate_elo_difference(yace_wins, sf_wins, draws)
        yace_estimated_elo = TARGET_ELO + elo_diff if LIMIT_STOCKFISH else 3600 + elo_diff
    else:
        win_a_pct = win_b_pct = draw_pct = elo_diff = yace_estimated_elo = 0.0

    print("="*45)
    print(f"RISULTATI FINALI")
    print("-" * 45)
    print(f"{VA:<15} : {yace_wins:>3} vittorie ({win_a_pct:>5.1f}%)")
    print(f"{VB:<15} : {sf_wins:>3} vittorie ({win_b_pct:>5.1f}%)")
    print(f"{'Patte':<15} : {draws:>3} patte    ({draw_pct:>5.1f}%)")
    print("-" * 45)
    
    if score[VA] == 0 or score[VB] == 0:
        print("ATTENZIONE: Uno dei due motori ha 0 vittorie.")
        print("Il calcolo dell'Elo non è accurato (tagliato al limite matematico).")
        print("Modifica TARGET_ELO per bilanciare i due motori.")
        
    print(f"Diff. Elo       : {'+' if elo_diff > 0 else ''}{elo_diff:.1f}")
    if LIMIT_STOCKFISH:
        print(f"Elo stimato YACE: {yace_estimated_elo:.0f} (Basato su SF a {TARGET_ELO})")
        
    print("="*45)

if __name__ == "__main__":
    main()