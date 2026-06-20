import subprocess
import chess
import chess.pgn
import time
import concurrent.futures
import os
import queue
import threading

BASEPATH   = "./versions/"
ENGINE_A   = BASEPATH + "engine_v1"
ENGINE_B   = BASEPATH + "engine_v2"
NUM_GAMES  = 10
MAX_WORKERS = min(8, os.cpu_count() or 1, NUM_GAMES)


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

def request_move(proc: subprocess.Popen, timeout: float = 10.0) -> str | None:
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

def play_game(white_path: str, black_path: str) -> tuple[str, chess.pgn.Game]:
    board = chess.Board()

    game = chess.pgn.Game()
    game.headers["Event"] = "YACE Gauntlet"
    game.headers["White"] = white_path.split("/")[-1]
    game.headers["Black"] = black_path.split("/")[-1]
    node = game

    # Un processo per colore, vivi per tutta la partita
    proc_w = start_engine(white_path)
    proc_b = start_engine(black_path)

    try:
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

            # Entrambi i processi devono aggiornare il loro stato:
            # - l'engine attivo non applica la mossa dopo 'go', va inviata esplicitamente
            # - l'engine passivo deve sapere cosa ha giocato l'avversario
            send_move(proc_w, move_uci)
            send_move(proc_b, move_uci)

    finally:
        stop_engine(proc_w)
        stop_engine(proc_b)

    game.headers["Result"] = board.result()
    return board.result(), game


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

    t0 = time.time()
    result, game = play_game(white, black)
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

    score = {ENGINE_A: 0, ENGINE_B: 0, "Draws": 0}

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
                    score[res["white"]] += 1
                    print(f"Vittoria BIANCO  ({res['duration']:.1f}s)")
                elif r == "0-1":
                    score[res["black"]] += 1
                    print(f"Vittoria NERO    ({res['duration']:.1f}s)")
                elif r == "1/2-1/2":
                    score["Draws"] += 1
                    print(f"Patta            ({res['duration']:.1f}s)")
                else:
                    print(f"Annullata ({r})")

    print("\n" + "=" * 35)
    print("RISULTATI FINALI")
    print("=" * 35)
    print(f"{ENGINE_A:<25}: {score[ENGINE_A]} vittorie")
    print(f"{ENGINE_B:<25}: {score[ENGINE_B]} vittorie")
    print(f"{'Patte':<25}: {score['Draws']}")
    print("=" * 35)


if __name__ == "__main__":
    main()