#include <iostream>
#include <vector>
#include <string>

#include "BitOperations.hpp"
#include "LookupTables.hpp"
#include "Pieces.hpp"
#include "Board.hpp"
#include "Engine.hpp"

void printChessBoard(Board& board) {
    std::cout << "\n";

    for (int rank = 7; rank >= 0; rank--) {
        std::cout << rank + 1 << "  "; // Stampa il numero della traversa
        
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            char pieceChar = '.'; // Punto se la casa è vuota

            if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::PAWNS), square)) pieceChar = 'P';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::KNIGHTS), square)) pieceChar = 'N';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::BISHOPS), square)) pieceChar = 'B';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::ROOKS), square)) pieceChar = 'R';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::QUEEN), square)) pieceChar = 'Q';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::KING), square)) pieceChar = 'K';
            
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::PAWNS), square)) pieceChar = 'p';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::KNIGHTS), square)) pieceChar = 'n';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::BISHOPS), square)) pieceChar = 'b';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::ROOKS), square)) pieceChar = 'r';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::QUEEN), square)) pieceChar = 'q';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::KING), square)) pieceChar = 'k';

            std::cout << pieceChar << " ";
        }
        std::cout << "\n";
    }
    std::cout << "   a b c d e f g h\n\n";
}

std::string moveToString(Move move) {
    if (move == 0) return "NESSUNA MOSSA"; // Gestione mossa nulla o di errore
    
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    
    char fromFile = 'a' + (from % 8);
    char fromRank = '1' + (from / 8);
    char toFile = 'a' + (to % 8);
    char toRank = '1' + (to / 8);
    
    std::string s = "";
    s += fromFile;
    s += fromRank;
    s += toFile;
    s += toRank;
    
    int flag = getMoveFlags(move);
    if (flag == FlagMap::PRQUEEN || flag == FlagMap::PRCAPQUEEN) s += "q";
    
    return s;
}

int main() {
    // Inizializziamo le tabelle geometriche e i componenti
    LookupTables::init();
    Engine myEngine;
    Board gameBoard;
    
    // ==========================================
    // TEST 1: AUTO-PLAY DA POSIZIONE INIZIALE
    // ==========================================
    std::cout << "=== TEST 1: AUTO-PLAY (POSIZIONE INIZIALE) ===\n";
    gameBoard.InitializeBoard();
    
    // Mostriamo la scacchiera iniziale
    std::cout << "Posizione di Partenza:";
    printChessBoard(gameBoard);
    
    int profonditaRicerca = 3; 
    int mosseDaSimulare = 4;
    
    for (int i = 0; i < mosseDaSimulare; i++) {
        std::cout << "--- MOSSA " << i + 1 << " ---";
        std::cout << (gameBoard.GetSideToMove() == Color::WHITE ? " [Turno: BIANCO]" : " [Turno: NERO]") << "\n";
        
        Move bestMove = myEngine.GetBestMove(gameBoard, profonditaRicerca);
        
        if (bestMove == 0) {
            std::cout << "Il motore non ha trovato mosse legali!\n";
            break;
        }
        
        std::cout << "Il motore ha scelto la mossa: " << moveToString(bestMove) << "\n";
        
        gameBoard.MakeMove(bestMove);
        
        // STAMPIAMO LA SCACCHIERA DOPO LA MOSSA
        printChessBoard(gameBoard);
    }

    // ==========================================
    // TEST 2: RILEVAMENTO MATTO IN 1 MOSSA
    // ==========================================
    std::cout << "\n\n=== TEST 2: RILEVAMENTO MATTO IN 1 ===\n";
    Board mateBoard;
    mateBoard.InitializeBoard();
    
    // Svuotiamo la scacchiera (azzera tutti gli array a 0 se non hai un metodo Clear)
    for(int c=0; c<2; c++) for(int p=0; p<6; p++) mateBoard.SetPieceBitBoard(c, (PiecesEnum::Type)p, 0ULL);
    
    // Setup legale: Re in g6, Torre in a1, Re Nero intrappolato in h8
    mateBoard.AddPiece(Color::WHITE, PiecesEnum::KING, 12);
    mateBoard.AddPiece(Color::WHITE, PiecesEnum::QUEEN, 46);  // g6
    mateBoard.AddPiece(Color::WHITE, PiecesEnum::ROOKS, 0);  // a1
    mateBoard.AddPiece(Color::BLACK, PiecesEnum::KING, 63);  // h8
    
    mateBoard.SetSideToMove(Color::WHITE);
    mateBoard.UpdateGlobalBoardState();

    std::cout << "Posizione caricata. Regina Bianca in g6, Torre in a1, Re Nero in h8.\n";
    printChessBoard(mateBoard);
    
    // Mettiamo profondità 2 così vede la risposta del Nero
    Move mateMove = myEngine.GetBestMove(mateBoard, 2); 
    
    std::cout << "Mossa calcolata dal motore: " << moveToString(mateMove) << "\n";
    if (getMoveTo(mateMove) == 56 || getMoveTo(mateMove) == 7) {
        std::cout << "[OK] IL MOTORE HA TROVATO IL MATTO IN 1!\n";
    } else {
        std::cout << "[ERRORE] Il motore non ha visto il matto.\n";
    }

    std::cout << "\n\n=== TEST 3: EVITARE LO STALLO E TROVARE IL MATTO ===\n";
    Board stalemateBoard;
    stalemateBoard.InitializeBoard();
    
    // Svuotiamo la scacchiera
    for(int c=0; c<2; c++) {
        for(int p=0; p<6; p++) {
            stalemateBoard.SetPieceBitBoard(c, (PiecesEnum::Type)p, 0ULL);
        }
    }
    
    // Setup Posizione:
    // Re Nero in a8 (Casella 56: rank 8, colonna a)
    // Re Bianco in c6 (Casella 42: rank 6, colonna c)
    // Regina Bianca in h7 (Casella 55: rank 7, colonna h)
    stalemateBoard.AddPiece(Color::BLACK, PiecesEnum::KING, 56);
    stalemateBoard.AddPiece(Color::WHITE, PiecesEnum::KING, 42);
    stalemateBoard.AddPiece(Color::WHITE, PiecesEnum::QUEEN, 55);
    
    stalemateBoard.SetSideToMove(Color::WHITE);
    stalemateBoard.UpdateGlobalBoardState();

    std::cout << "Posizione caricata. Re Nero in a8, Re Bianco in c6, Regina in h7.\n";
    printChessBoard(stalemateBoard);
    
    // Mettiamo una profondità di almeno 2 per fargli vedere la risposta del Nero
    Move bestMove = myEngine.GetBestMove(stalemateBoard, 3); 
    
    std::cout << "Mossa calcolata dal motore: " << moveToString(bestMove) << "\n";
    
    // h7 è 55. b7 è 49 (Matto). c7 è 50 (Stallo).
    if (getMoveTo(bestMove) == 49) {
        std::cout << "[OK] IL MOTORE HA TROVATO IL MATTO IN 1 (h7b7) ED EVITATO LO STALLO!\n";
    } else if (getMoveTo(bestMove) == 50) {
        std::cout << "[ERRORE] Il motore ha giocato h7c7, causando lo STALLO e buttando la vittoria.\n";
    } else {
        std::cout << "[INFO] Il motore ha scelto una mossa diversa (" << moveToString(bestMove) << "), ma ha evitato lo stallo.\n";
    }

    return 0;
}