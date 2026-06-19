import subprocess
import chess
import chess.pgn
import time
import concurrent.futures
import os

BASEPATH = "./versions/"
ENGINE_A = BASEPATH + "engine_v1"
ENGINE_B = BASEPATH + "engine_v2"

NUM_GAMES = 1
MAX_WORKERS = min(min(8, (os.cpu_count() or 1)), NUM_GAMES)

def get_engine_move(engine_path, move_history):
    input_data = " ".join(move_history) + (" " if move_history else "") + "go\n"
    
    try:
        process = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        stdout, stderr = process.communicate(input=input_data, timeout=10) 
        
        lines = stdout.strip().split('\n')
        best_move_str = lines[-1].strip()
        
        return best_move_str
        
    except subprocess.TimeoutExpired:
        process.kill()
        print(f"Errore: {engine_path} ha impiegato troppo tempo!")
        return None
    except Exception as e:
        print(f"Errore di esecuzione: {e}")
        return None

def play_game(white_engine, black_engine):
    board = chess.Board()
    move_history = []
    
    game = chess.pgn.Game()
    game.headers["Event"] = "YACE Gauntlet Match"
    game.headers["White"] = white_engine.split("/")[-1] 
    game.headers["Black"] = black_engine.split("/")[-1]
    node = game
    
    while not board.is_game_over():
        current_engine = white_engine if board.turn == chess.WHITE else black_engine
        move_str = get_engine_move(current_engine, move_history)
        
        if not move_str:
            return "Errore Motore", game

        try:
            move = chess.Move.from_uci(move_str) 
            if move in board.legal_moves:
                board.push(move)
                move_history.append(move_str)
                node = node.add_variation(move)
            else:
                print(f"\n[DEBUG] Mossa ILLEGALE ricevuta dal C++: '{move_str}'")
                print(f"[DEBUG] Mosse legali secondo Python: {[m.uci() for m in board.legal_moves]}")
                result = "1-0" if board.turn == chess.BLACK else "0-1"
                game.headers["Result"] = result
                return result, game
                
        except ValueError:
            return "Errore Formato", game

    game.headers["Result"] = board.result()
    return board.result(), game

def run_match_task(match_id):
    """
    Funzione wrapper (il "lavoro" del singolo thread).
    Gioca una singola partita e impacchetta tutti i risultati in un dizionario.
    """
    if match_id % 2 == 0:
        white, black = ENGINE_A, ENGINE_B
        white_name, black_name = "Engine A", "Engine B"
    else:
        white, black = ENGINE_B, ENGINE_A
        white_name, black_name = "Engine B", "Engine A"
        
    start_time = time.time()
    match_result, game = play_game(white, black)
    duration = time.time() - start_time
    
    return {
        "id": match_id + 1,
        "white": white,
        "black": black,
        "white_name": white_name,
        "black_name": black_name,
        "result": match_result,
        "game": game,
        "duration": duration
    }

def main():
    print(f"--- INIZIO ARENA: {ENGINE_A} vs {ENGINE_B} ---")
    print(f"Partite programmate: {NUM_GAMES}")
    print(f"Motori avviati in parallelo: {MAX_WORKERS}\n")
    
    score = {ENGINE_A: 0, ENGINE_B: 0, "Draws": 0}

    with open(BASEPATH+"gauntlet_results.pgn", "w", encoding="utf-8") as pgn_file:
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
            futures = [executor.submit(run_match_task, i) for i in range(NUM_GAMES)]
            for future in concurrent.futures.as_completed(futures):
                res = future.result() 
                
                print(f"Partita {res['id']}/{NUM_GAMES}: {res['white_name']} (B) vs {res['black_name']} (N)... ", end="", flush=True)
                print(res['game'], file=pgn_file, end="\n\n")
                pgn_file.flush() # Forza il salvataggio su disco immediato
                
                match_result = res['result']
                if match_result == "1-0":
                    score[res['white']] += 1
                    print(f"Vittoria BIANCO in {res['duration']:.1f}s")
                elif match_result == "0-1":
                    score[res['black']] += 1
                    print(f"Vittoria NERO in {res['duration']:.1f}s")
                elif match_result == "1/2-1/2":
                    score["Draws"] += 1
                    print(f"PATTA in {res['duration']:.1f}s")
                else:
                    print(f"PARTITA ANNULLATA ({match_result})")

    # Report Finale
    print("\n" + "="*30)
    print("RISULTATI FINALI")
    print("="*30)
    print(f"{ENGINE_A} : {score[ENGINE_A]} vittorie")
    print(f"{ENGINE_B} : {score[ENGINE_B]} vittorie")
    print(f"Patte      : {score['Draws']}")
    print("="*30)

if __name__ == "__main__":
    main()