import subprocess
import chess
import chess.pgn
import time

BASEPATH = "./versions/"
# Percorsi agli eseguibili C++ compilati
ENGINE_A = BASEPATH + "engine_v1"
ENGINE_B = BASEPATH + "engine_v1"

NUM_GAMES = 10

def get_engine_move(engine_path, move_history):
    """
    Invia lo storico delle mosse all'eseguibile C++ e recupera la risposta.
    """
    # Prepara l'input: es. "e2e4 e7e5 g1f3 go\n"
    input_data = " ".join(move_history) + (" " if move_history else "") + "go\n"
    
    try:
        # Avvia il processo C++
        process = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Invia le mosse e aspetta l'output
        stdout, stderr = process.communicate(input=input_data, timeout=10) # Timeout 10 sec
        
        # L'ultima riga non vuota stampata dovrebbe essere la mossa (es. "e2e4")
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
    
    # --- NUOVO: Inizializza il PGN ---
    game = chess.pgn.Game()
    game.headers["Event"] = "YACE Gauntlet Match"
    game.headers["White"] = white_engine.split("/")[-1] # Prende solo il nome del file
    game.headers["Black"] = black_engine.split("/")[-1]
    node = game
    # ---------------------------------
    
    while not board.is_game_over():
        current_engine = white_engine if board.turn == chess.WHITE else black_engine
        move_str = get_engine_move(current_engine, move_history)
        
        if not move_str:
            return "Errore Motore", game # Ritorna anche il game

        try:
            move = chess.Move.from_uci(move_str) 
            if move in board.legal_moves:
                board.push(move)
                move_history.append(move_str)
                
                # --- NUOVO: Aggiunge la mossa al PGN ---
                node = node.add_variation(move)
                # ---------------------------------------
            else:
                result = "1-0" if board.turn == chess.BLACK else "0-1"
                game.headers["Result"] = result
                return result, game
                
        except ValueError:
            return "Errore Formato", game

    game.headers["Result"] = board.result()
    return board.result(), game

def main():
    print(f"--- INIZIO ARENA: {ENGINE_A} vs {ENGINE_B} ---")
    print(f"Partite programmate: {NUM_GAMES}\n")
    
    score = {ENGINE_A: 0, ENGINE_B: 0, "Draws": 0}

    # Apriamo il file una sola volta per tutto il torneo
    with open("gauntlet_results.pgn", "w", encoding="utf-8") as pgn_file:
        
        # UN UNICO CICLO FOR
        for i in range(NUM_GAMES):
            # Alterniamo i colori a ogni partita
            if i % 2 == 0:
                white, black = ENGINE_A, ENGINE_B
                white_name, black_name = "Engine A", "Engine B"
            else:
                white, black = ENGINE_B, ENGINE_A
                white_name, black_name = "Engine B", "Engine A"
                
            print(f"Partita {i+1}/{NUM_GAMES}: {white_name} (Bianco) vs {black_name} (Nero)... ", end="", flush=True)
            
            start_time = time.time()
            
            # Giochiamo la partita
            match_result, game = play_game(white, black) 
            
            duration = time.time() - start_time
            
            # Salviamo nel PGN
            print(game, file=pgn_file, end="\n\n")
            
            # Aggiorniamo i punteggi e stampiamo il risultato in console
            if match_result == "1-0":
                score[white] += 1
                print(f"Vittoria BIANCO in {duration:.1f}s")
            elif match_result == "0-1":
                score[black] += 1
                print(f"Vittoria NERO in {duration:.1f}s")
            elif match_result == "1/2-1/2":
                score["Draws"] += 1
                print(f"PATTA in {duration:.1f}s")
            else:
                print(f"PARTITA ANNULLATA ({match_result})")

    # Report Finale (fuori dal 'with', il file ora è chiuso in modo sicuro)
    print("\n" + "="*30)
    print("RISULTATI FINALI")
    print("="*30)
    print(f"{ENGINE_A} : {score[ENGINE_A]} vittorie")
    print(f"{ENGINE_B} : {score[ENGINE_B]} vittorie")
    print(f"Patte      : {score['Draws']}")
    print("="*30)
    print("Analizza in https://lichess.org/paste")

if __name__ == "__main__":
    main()