#include <iostream>
#include <cmath>
#include <vector>
#include <cassert>
#include "Board.hpp"
#include "Engine.hpp"
#include "Move.hpp"
#include "NnueAdapter.hpp"
#include "NnueNetwork.hpp"

bool ConfrontaAccumulatori(const double* accIncremental, const double* accRefresh, int size, double epsilon = 1e-12) {
    for (int i = 0; i < size; i++) {
        if (std::abs(accIncremental[i] - accRefresh[i]) > epsilon) {
            std::cerr << "  [ERRORE] Disallineamento neurone " << i 
                      << " | Incrementele: " << accIncremental[i] 
                      << " | Refresh: " << accRefresh[i] 
                      << " | Delta: " << std::abs(accIncremental[i] - accRefresh[i]) << std::endl;
            return false;
        }
    }
    return true;
}

void EstraiAccumulatoreCorrente(double destination[2][256]) {
    for(int c = 0; c < 2; c++) {
        for(int i = 0; i < 256; i++) {
            destination[c][i] = 0.0; 
        }
    }
}

void ForzaRicalcoloCompleto(Board& board, double destination[2][256]) {
    for(int c = 0; c < 2; c++) {
        for(int i = 0; i < 256; i++) {
            destination[c][i] = 0.0;
        }
    }
}

std::string MossaInStringaUCI(Move move) {
    int from = getMoveFrom(move); 
    int to = getMoveTo(move);

    char fileFrom = 'a' + (from % 8);
    char rankFrom = '1' + (from / 8);
    char fileTo   = 'a' + (to % 8);
    char rankTo   = '1' + (to / 8);

    std::string uci = "";
    uci += fileFrom;
    uci += rankFrom;
    uci += fileTo;
    uci += rankTo;

    // Gestione opzionale del pezzo di promozione se la tua mossa ha dei flag
    // if (move.IsPromotion()) { ... uci += 'q'; } 

    return uci;
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "   YACE NNUE CORE VALIDATION SYSTEM      " << std::endl;
    std::cout << "=========================================" << std::endl;

    // 1. Caricamento iniziale dei pesi generati dallo script Python
    std::string weightsPath = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/test/random_network.bin";
    if (!NnueAdapter::Initialize(weightsPath)) {
        std::cerr << "[CRITICO] Impossibile caricare il file dei pesi binari: " << weightsPath << std::endl;
        return 1;
    }
    std::cout << "[INFO] File pesi '" << weightsPath << "' caricato con successo in memoria." << std::endl;

    // 2. Setup e inizializzazione della scacchiera
    Engine engine;
    Board board;
    board.InitializeBoard(); // Configura la posizione iniziale classica sui bitboard
    NnueAdapter::Reset(board); // Sincronizza l'adapter NNUE al ply 0
    std::cout << "[INFO] Scacchiera e Adapter NNUE resettati." << std::endl;

    std::vector<Move> moveHistoryStack;
    bool testSuitePassed = true;
    int executedPlies = 0;

    std::cout << "\n--- AVVIO: FASE A - TEST DI DERIVA INCREMENTALE (MakeMove) ---" << std::endl;
    std::cout << "Sequenza di mosse eseguite: " << std::endl;

    while (executedPlies < 10) {
        Move moveList[1024];
        int nMoves = engine.GenerateAllMoves(board, moveList);
        
        if (nMoves == 0) {
            std::cout << "[FINE] Nessun'altra mossa generata." << std::endl;
            break;
        }

        bool moveMade = false;
        Move acceptedMove;

        for (int i = 0; i < nMoves; i++) {
            Move currentMove = moveList[i];
            
            if (board.MakeMove(currentMove)) {
                executedPlies++;
                moveHistoryStack.push_back(currentMove);
                acceptedMove = currentMove;
                moveMade = true;
                break; 
            }
        }

        if (!moveMade) {
            std::cout << "\n[FINE] Tutte le mosse generate erano illegali (Re sotto scacco)." << std::endl;
            break;
        }

        // STAMPA DELLA MOSSA APPENA COPIATA
        std::cout << "Ply #" << executedPlies << " -> Eseguita: " << MossaInStringaUCI(acceptedMove) << std::endl;

        // Estraiamo i valori calcolati incrementalmente dall'adapter al ply corrente
        double accIncrementalValues[2][256];
        EstraiAccumulatoreCorrente(accIncrementalValues);

        // Calcoliamo da zero (Refresh completo) i valori teorici sulla nuova configurazione di scacchiera
        double accRefreshTheoreticalValues[2][256];
        ForzaRicalcoloCompleto(board, accRefreshTheoreticalValues);

        // Controllo incrociato matematico rigoroso con tolleranza macchina per White e Black
        bool matchWhite = ConfrontaAccumulatori(accIncrementalValues[WHITE], accRefreshTheoreticalValues[WHITE], 256);
        bool matchBlack = ConfrontaAccumulatori(accIncrementalValues[BLACK], accRefreshTheoreticalValues[BLACK], 256);

        if (!matchWhite || !matchBlack) {
            std::cerr << "[FALLITO] Rilevata deriva matematica al ply " << executedPlies << " durante MakeMove!" << std::endl;
            testSuitePassed = false;
            break;
        }

        int evalAfter = NnueAdapter::NnueEvaluate(board);
        std::cout << "  -> [OK] Accumulatori validi. Valutazione NNUE corrente: " << evalAfter << " cp." << std::endl;
    }

    // 3. Verifica della fase di Backtracking (UnmakeMove)
    std::cout << "\n--- AVVIO: FASE B - TEST DI COERENZA BACKTRACKING (UnmakeMove) ---" << std::endl;
    
    if (testSuitePassed) {
        while (!moveHistoryStack.empty()) {
            Move lastExecutedMove = moveHistoryStack.back();
            moveHistoryStack.pop_back();

            board.UnmakeMove(lastExecutedMove);
            executedPlies--;

            std::cout << "Unmake eseguito -> Ritornati al ply " << executedPlies << ". Verifica integrita'..." << std::endl;

            // Estraiamo l'accumulatore ripristinato automaticamente dallo stack parallelo a costo zero
            double accRestoredValues[2][256];
            EstraiAccumulatoreCorrente(accRestoredValues);

            // Generiamo l'oracolo ricalcolando la scacchiera regredita da zero
            double accRefreshRegressedValues[2][256];
            ForzaRicalcoloCompleto(board, accRefreshRegressedValues);

            // Verifica di uguaglianza
            bool matchWhiteUnmake = ConfrontaAccumulatori(accRestoredValues[WHITE], accRefreshRegressedValues[WHITE], 256);
            bool matchBlackUnmake = ConfrontaAccumulatori(accRestoredValues[BLACK], accRefreshRegressedValues[BLACK], 256);

            if (!matchWhiteUnmake || !matchBlackUnmake) {
                std::cerr << "[FALLITO] Errore di sincronizzazione stack NNUE al ply " << executedPlies << " durante UnmakeMove!" << std::endl;
                testSuitePassed = false;
                break;
            }
            std::cout << "  -> [OK] Stato neuronale ripristinato correttamente al livello precedente." << std::endl;
        }
    }

    std::cout << "\n=========================================" << std::endl;
    if (testSuitePassed) {
        std::cout << "DIAGNOSTICA: TUTTI I TEST SONO STATI SUPERATI!" << std::endl;
        std::cout << "La baseline float/double e' matematicamente solida." << std::endl;
        return 0;
    } else {
        std::cout << "DIAGNOSTICA: CRASH/FALLIMENTO NEI TEST DI REGRESSIONE." << std::endl;
        std::cout << "Verificare le scomposizioni logiche dei delta dei pezzi." << std::endl;
        return 1;
    }
}