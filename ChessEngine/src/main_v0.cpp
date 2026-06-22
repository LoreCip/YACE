#include <iostream>
#include <cstdint>
#include <sys/types.h>
#include <vector>

#include "BitOperations.hpp"
#include "LookupTables.hpp"
#include "Pieces.hpp"
#include "Board.hpp"
#include "Engine.hpp"

/**
 * La casella 0 (A1) è in basso a sinistra, la casella 63 (H8) è in alto a destra.
 */
void printBitboard(uint64_t bitboard, int idx = -1) {
    std::cout << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        std::cout << rank + 1 << "  "; // Stampa il numero della traversa (1-8)
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            if (getBit(bitboard, square)) {
                std::cout << "1 ";
            } else if (square == idx) {
                std::cout << "x ";
            } else {
                std::cout << ". ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n   a b c d e f g h\n\n";
}

void printMoveList(const std::vector<Move>& moveList) {
    std::cout << "Mosse generate (" << moveList.size() << "):\n";
    for (const auto& move : moveList) {
        int from = getMoveFrom(move);
        int to = getMoveTo(move);
        int flag = getMoveFlags(move);
        
        std::cout << "  Da: " << from << " A: " << to << " [Flag: " << flag << "]\n";
    }
}


int main() {

    int testSquare = 28; // e4

    // ==========================================
    // 1. TEST BIT OPERATIONS
    // ==========================================
    std::cout << "=== 1. TEST BIT OPERATIONS ===\n";
    uint64_t board = 0ULL;
    
    std::cout << "Imposto il bit sulla casella e4 (indice 28):";
    board = setBit(board, testSquare);
    printBitboard(board);

    std::cout << "Pulisco il bit su e4 (indice 28):";
    board = clearBit(board, testSquare);
    printBitboard(board);


    // ==========================================
    // 2. TEST LOOKUP TABLES
    // ==========================================
    std::cout << "=== 2. TEST LOOKUP TABLES ===\n";
    LookupTables::init();

    std::cout << "Attacchi del Cavallo da e4 (indice 28):";
    printBitboard(LookupTables::knightAttacks[testSquare], testSquare);

    std::cout << "Attacchi del Re da e4 (indice 28):";
    printBitboard(LookupTables::kingAttacks[testSquare], testSquare);

    std::cout << "Attacchi del pedone bianco da e4 (indice 28):";
    printBitboard(LookupTables::pawnAttacks[0][testSquare], testSquare);

    std::cout << "Attacchi del pedone nero da e4 (indice 28):";
    printBitboard(LookupTables::pawnAttacks[1][testSquare], testSquare);


    // ==========================================
    // 3. TEST CLASSE BOARD
    // ==========================================
    std::cout << "=== 3. TEST INIZIALIZZAZIONE BOARD ===\n";
    
    Board myBoard;
    myBoard.InitializeBoard();

    std::cout << "Occupazione Pezzi Bianchi (Colore 0):";
    // Si aspetta di vedere la traversa 1 (pezzi) e traversa 2 (pedoni) piene
    printBitboard(myBoard.GetColorOccupation(0));

    std::cout << "Occupazione Pezzi Neri (Colore 1):";
    // Si aspetta di vedere la traversa 8 (pezzi) e traversa 7 (pedoni) piene
    printBitboard(myBoard.GetColorOccupation(1));

    std::cout << "Occupazione Totale (Tutti i pezzi):";
    printBitboard(myBoard.GetTotalOccupation());

    std::cout << "Celle Libere (Vuote):";
    // Si aspetta di vedere le traverse centrali (3, 4, 5, 6) piene di '1'
    printBitboard(myBoard.GetFreeCells());


    // ==========================================
    // 4. TEST GENERAZIONE MOSSE (BOARD)
    // ==========================================
    std::cout << "=== 4. TEST GENERAZIONE MOSSE ===\n";

    // ------------------------------------------
    // Test A: Spinta dei Pedoni a inizio partita
    // ------------------------------------------
    std::cout << "--- Test A: Spinta Pedoni Bianchi a inizio partita ---\n";
    // Creiamo un bitboard che contiene solo i pedoni bianchi sulla seconda traversa (A2-H2)
    uint64_t whitePawnsStart = 0x000000000000FF00ULL; 
    
    // Generiamo le mosse con GetGeneratedMoves
    uint64_t pawnMoves = myBoard.GetGeneratedMoves(Color::WHITE, whitePawnsStart, PiecesEnum::PAWNS);
    std::cout << "I pedoni in seconda traversa dovrebbero poter fare sia 1 che 2 passi avanti (Traverse 3 e 4 piene):";
    printBitboard(pawnMoves);


    std::cout << "--- Test A2: Spinta Pedoni Neri a inizio partita ---\n";
    // Creiamo un bitboard che contiene solo i pedoni bianchi sulla seconda traversa (A2-H2)
    uint64_t blackPawnsStart = 0x00FF000000000000ULL; 
    
    // Generiamo le mosse con GetGeneratedMoves
    pawnMoves = myBoard.GetGeneratedMoves(Color::BLACK, blackPawnsStart, PiecesEnum::PAWNS);
    std::cout << "I pedoni in seconda traversa dovrebbero poter fare sia 1 che 2 passi avanti (Traverse 6 e 7 piene):";
    printBitboard(pawnMoves);

    // ------------------------------------------
    // Test B: Mosse del Cavallo con ostacoli alleati
    // ------------------------------------------
    std::cout << "--- Test B: Cavallo Bianco in e4 (indice 28) ---\n";
    // Creiamo un bitboard fittizio contenente solo il nostro cavallo in e4
    uint64_t customKnightBoard = 0ULL;
    customKnightBoard = setBit(customKnightBoard, testSquare); // e4

    // Chiediamo alla board di generare le mosse per il Cavallo (indice 28)
    // Nota: Siccome la board è in configurazione iniziale, le case d2 e f2 contengono già dei pedoni alleati bianchi!
    // Di conseguenza, i salti all'indietro verso d2 e f2 dovrebbero venire filtrati e bloccati.
    uint64_t knightMoves = myBoard.GetGeneratedMoves(Color::WHITE, customKnightBoard, PiecesEnum::KNIGHTS);
    std::cout << "Mosse del cavallo da e4 (notare come d2 e f2 non hanno '1' perché occupate da alleati):";
    printBitboard(knightMoves, testSquare);


    // ------------------------------------------
    // Test C: Catture dei Pedoni (Simulazione)
    // ------------------------------------------
    std::cout << "--- Test C: Cattura di un Pedone Bianco ---\n";
    // Mettiamo un singolo pedone bianco in e4 (indice 28)
    uint64_t singlePawnBoard = 0ULL;
    singlePawnBoard = setBit(singlePawnBoard, testSquare);

    // In questo momento la scacchiera iniziale ha i pezzi neri solo sulla 7° e 8° traversa.
    // Un pedone in e4 non attacca nessuno. Verifichiamo che possa solo avanzare in e5.
    uint64_t pawnNoCapture = myBoard.GetGeneratedMoves(Color::WHITE, singlePawnBoard, PiecesEnum::PAWNS);
    std::cout << "Pedone bianco in e4 senza nemici vicini (può solo andare in e5):";
    printBitboard(pawnNoCapture, testSquare);

    // Avanziamo il pedone alla 6° traversa
    singlePawnBoard = clearBit(singlePawnBoard, testSquare);
    singlePawnBoard = setBit(singlePawnBoard, 47);
    uint64_t pawnCapture = myBoard.GetGeneratedMoves(Color::WHITE, singlePawnBoard, PiecesEnum::PAWNS);
    printBitboard(pawnCapture, 47);

    std::cout << "--- Test C2: Cattura di un Pedone Nero ---\n";
    // Mettiamo un singolo pedone bianco in e4 (indice 28)
    singlePawnBoard = 0ULL;
    singlePawnBoard = setBit(singlePawnBoard, testSquare);

    // In questo momento la scacchiera iniziale ha i pezzi bianchi solo sulla 1° e 2° traversa.
    // Un pedone in e4 non attacca nessuno. Verifichiamo che possa solo avanzare in e3.
    pawnNoCapture = myBoard.GetGeneratedMoves(Color::BLACK, singlePawnBoard, PiecesEnum::PAWNS);
    std::cout << "Pedone nero in e4 senza nemici vicini (può solo andare in e3):";
    printBitboard(pawnNoCapture, testSquare);

    // Avanziamo il pedone alla 6° traversa
    singlePawnBoard = clearBit(singlePawnBoard, testSquare);
    singlePawnBoard = setBit(singlePawnBoard, 16);
    pawnCapture = myBoard.GetGeneratedMoves(Color::BLACK, singlePawnBoard, PiecesEnum::PAWNS);
    printBitboard(pawnCapture, 16);


    // ------------------------------------------
    // Test D: Mosse del Re
    // ------------------------------------------
    std::cout << "--- Test D: Movimento del Re ---\n";
    // Mettiamo un singolo Re bianco in d3 (indice 20)
    uint64_t singleKingBoard = 0ULL;
    singleKingBoard = setBit(singleKingBoard, testSquare - 9);
    
    uint64_t kingMoves = myBoard.GetGeneratedMoves(Color::WHITE, singleKingBoard, PiecesEnum::KING);
    std::cout << "Il movimento del Re è bloccato dietro dai pedoni alleati:";
    printBitboard(kingMoves, testSquare - 9);

    // ------------------------------------------
    // Test E: Mosse dei Pezzi Scorrevoli (Sliders)
    // ------------------------------------------
    std::cout << "=== 5. TEST GENERAZIONE MOSSE PEZZI SCORREVOLI ===\n";

    // --- TEST ALFIERE ---
    std::cout << "--- Test E1: Alfiere Bianco in e4 (indice 28) ---\n";
    uint64_t customBishopBoard = 0ULL;
    customBishopBoard = setBit(customBishopBoard, testSquare); // e4

    // Nella configurazione iniziale, l'Alfiere in e4 incontrerà:
    // - Verso Sud-Ovest: l'ostacolo in c2 (Pedone alleato bianco), quindi si ferma a d3 e NON cattura c2.
    // - Verso Sud-Est: l'ostacolo in g2 (Pedone alleato bianco), quindi si ferma a f3 e NON cattura g2.
    // - Verso Nord-Ovest: l'ostacolo in b7 (Pedone nemico nero), quindi si ferma a b7 INCLUDENDOLO come cattura.
    // - Verso Nord-Est: l'ostacolo in h7 (Pedone nemico nero), quindi si ferma a h7 INCLUDENDOLO come cattura.
    uint64_t bishopMoves = myBoard.GetGeneratedMoves(Color::WHITE, customBishopBoard, PiecesEnum::BISHOPS);
    std::cout << "Mosse Alfiere da e4 (Cattura su b7 e h7, bloccato da alleati prima di c2 e g2):";
    printBitboard(bishopMoves, testSquare);


    // --- TEST TORRE ---
    std::cout << "--- Test E2: Torre Bianca in e4 (indice 28) ---\n";
    uint64_t customRookBoard = 0ULL;
    customRookBoard = setBit(customRookBoard, testSquare); // e4

    // Nella configurazione iniziale, la Torre in e4 incontrerà:
    // - Verso Nord: l'ostacolo in e7 (Pedone nemico), si ferma su e7 includendolo (cattura).
    // - Verso Sud: l'ostacolo in e2 (Pedone alleato), si ferma a e3 e NON include e2.
    // - Verso Ovest/Est: campo libero fino ai bordi della scacchiera (traversa 4 vuota).
    uint64_t rookMoves = myBoard.GetGeneratedMoves(Color::WHITE, customRookBoard, PiecesEnum::ROOKS);
    std::cout << "Mosse Torre da e4 (Cattura su e7, bloccata a e3 da alleato, traversa 4 libera):";
    printBitboard(rookMoves, testSquare);


    // --- TEST DONNA ---
    std::cout << "--- Test E3: Donna Bianca in e4 (indice 28) ---\n";
    uint64_t customQueenBoard = 0ULL;
    customQueenBoard = setBit(customQueenBoard, testSquare); // e4

    // La Donna deve combinare esattamente le mosse dell'Alfiere e della Torre calcolate sopra.
    uint64_t queenMoves = myBoard.GetGeneratedMoves(Color::WHITE, customQueenBoard, PiecesEnum::QUEEN);
    std::cout << "Mosse Donna da e4 (Unione completa di diagonali e ortogonali):";
    printBitboard(queenMoves, testSquare);

    // ==========================================
    // 6. TEST CICLO DI STATO (MAKE/UNMAKE & RADAR)
    // ==========================================
    std::cout << "=== 6. TEST STATE MACHINE & RADAR ===\n";

    // Test B1: Spostamento Semplice (e2 -> e4)
    std::cout << "--- Test B1: Esecuzione Mossa (e2 -> e4) ---\n";
    int e2 = 12;
    int e4 = 28;
    // Creiamo la mossa: from=12, to=28, flag=0 (Normale)
    Move pawnPushMove = createMove(e2, e4, 0); 
    
    bool isLegal = myBoard.MakeMove(pawnPushMove);
    std::cout << "Mossa eseguita. E' legale? " << (isLegal ? "SI" : "NO") << "\n";
    std::cout << "Occupazione Totale DOPO la mossa (nota il bit spostato da e2 a e4):";
    printBitboard(myBoard.GetTotalOccupation());

    // Dopo e2-e4, il pedone bianco in e4 attacca d5 (35) e f5 (37).
    // Verifichiamo se d5 è considerata attaccata dal Bianco!
    int d5 = 35;
    bool isD5Attacked = myBoard.IsSquareAttacked(d5, Color::WHITE);
    std::cout << "La casa d5 e' attaccata dal Bianco? " << (isD5Attacked ? "SI" : "NO") << " (Dovrebbe essere SI per via del pedone in e4)\n";

    // Verifichiamo una casa sicura (es. h5)
    int h5 = 39;
    bool isH5Attacked = myBoard.IsSquareAttacked(h5, Color::WHITE);
    std::cout << "La casa h5 e' attaccata dal Bianco? " << (isH5Attacked ? "SI" : "NO") << " (Dovrebbe essere SI, per la regina in d1)\n";

    int h6 = 47;
    bool isH6Attacked = myBoard.IsSquareAttacked(h6, Color::WHITE);
    std::cout << "La casa h6 e' attaccata dal Bianco? " << (isH6Attacked ? "SI" : "NO") << " (Dovrebbe essere NO)\n";

    // Test B2: Annullamento della Mossa
    std::cout << "--- Test B2: Rollback (UnmakeMove) ---\n";
    myBoard.UnmakeMove(pawnPushMove);
    std::cout << "Occupazione Totale DOPO UnmakeMove (deve essere identica all'inizio partita, pedone tornato in e2):";
    printBitboard(myBoard.GetTotalOccupation());

    // ==========================================
    // 7. TEST COMPLETI DI INTEGRITÀ DELL'ENGINE
    // ==========================================
    std::cout << "=== 7. TEST COMPLETI DI INTEGRITÀ DELL'ENGINE ===\n";

    Engine myEngine;
    // -----------------------------------------------------------------
    // Test E1: Il Rilevamento dell'Inchiodatura (Pin)
    // -----------------------------------------------------------------
    std::cout << "--- Test E1: Verifica pezzo inchiodato (Pin) ---\n";
    Board pinBoard;

    // Setup manuale di una posizione di inchiodatura:
    // Re Bianco in e1 (4), Alfiere Bianco in e2 (12), Torre Nera in e8 (60).
    // L'Alfiere in e2 è pseudo-legalmente libero di muoversi sulle diagonali,
    // ma se lo fa, lascia il Re sotto il raggio della Torre nera. Deve essere illegale.
    pinBoard.sides[Color::WHITE][PiecesEnum::KING] = setBit(0ULL, 4);
    pinBoard.sides[Color::WHITE][PiecesEnum::BISHOPS] = setBit(0ULL, 12);
    pinBoard.sides[Color::BLACK][PiecesEnum::ROOKS] = setBit(0ULL, 60);
    pinBoard.sideToMove = Color::WHITE;

    pinBoard.UpdateGlobalBoardState();

    // Generiamo tutte le mosse pseudo-legali
    std::vector<Move> pseudoMoves = myEngine.GenerateAllMoves(pinBoard);
    printMoveList(pseudoMoves);
    int legalMovesCount = 0;

    std::cout << "Tentiamo di eseguire le mosse dell'Alfiere inchiodato:\n";
    for (const auto& move : pseudoMoves) {
        if (getMoveFrom(move) == 12) { // Se la mossa parte dall'Alfiere in e2
            bool success = pinBoard.MakeMove(move);
            if (success) {
                std::cout << "  [ERRORE] L'Alfiere si e' mosso in " << getMoveTo(move) << " lasciando il Re sotto scacco!\n";
                pinBoard.UnmakeMove(move);
                legalMovesCount++;
            } else {
                std::cout << "  [OK] Mossa verso " << getMoveTo(move) << " rifiutata correttamente da MakeMove.\n";
            }
        }
    }

    // -----------------------------------------------------------------
    // Test E2: Fuga dallo Scacco
    // -----------------------------------------------------------------
    std::cout << "--- Test E2: Risposta obbligatoria a uno Scacco ---\n";
    Board checkBoard;
    // Re Bianco in e1 (4), Cavallo Bianco in b1 (1), Torre Nera in e8 (60) [Re sotto scacco dritto].
    // Il Cavallo in b1 ha mosse pseudo-legali (es. in c3 o a3), ma nessuna di queste blocca lo scacco.
    // Solo le mosse del Re (es. d1, f1, d2, f2) o un pezzo che si interpone in e2/e3... possono essere legali.
    
    checkBoard.sides[Color::WHITE][PiecesEnum::KING] = setBit(0ULL, 4);
    checkBoard.sides[Color::WHITE][PiecesEnum::KNIGHTS] = setBit(0ULL, 1);
    checkBoard.sides[Color::BLACK][PiecesEnum::ROOKS] = setBit(0ULL, 60);
    checkBoard.sideToMove = Color::WHITE;
    checkBoard.UpdateGlobalBoardState();

    std::vector<Move> checkMoves = myEngine.GenerateAllMoves(checkBoard);
    int validMovesForKnight = 0;
    printMoveList(checkMoves);

    for (const auto& move : checkMoves) {
        if (getMoveFrom(move) == 1) {
            if (checkBoard.MakeMove(move)) {
                validMovesForKnight++;
                checkBoard.UnmakeMove(move);
            }
        }
    }
    std::cout << "Mosse legali permesse al Cavallo mentre il Re e' sotto scacco: " << validMovesForKnight << " (Deve essere 0)\n";


    // -----------------------------------------------------------------
    // Test E3: Scacco Doppio
    // -----------------------------------------------------------------
    std::cout << "--- Test E3: Scacco Doppio (Solo il Re puo' muoversi) ---\n";
    Board doubleCheckBoard;
    // Re Bianco in e1 (4), Torre Bianca in e2 (12), Torre Nera in e8 (60) [Scacco di Torre], Cavallo Nero in c2 (10) [Scacco di Cavallo].
    // Il Re è sotto scacco da DUE pezzi contemporaneamente. 
    // La Torre in e2 non può interparsi o catturare perché non può eliminare entrambe le minacce in una mossa.
    
    doubleCheckBoard.sides[Color::WHITE][PiecesEnum::KING] = setBit(0ULL, 4);
    doubleCheckBoard.sides[Color::WHITE][PiecesEnum::ROOKS] = setBit(0ULL, 12);
    doubleCheckBoard.sides[Color::BLACK][PiecesEnum::ROOKS] = setBit(0ULL, 60);
    doubleCheckBoard.sides[Color::BLACK][PiecesEnum::KNIGHTS] = setBit(0ULL, 10);
    doubleCheckBoard.sideToMove = Color::WHITE;
    doubleCheckBoard.UpdateGlobalBoardState();

    std::vector<Move> doubleCheckMoves = myEngine.GenerateAllMoves(doubleCheckBoard);
    printMoveList(doubleCheckMoves);
    int legalRookMoves = 0;
    int legalKingMoves = 0;

    for (const auto& move : doubleCheckMoves) {
        std::cout << "0:";
        if (doubleCheckBoard.MakeMove(move)) {
            std::cout << "1:";
            if (getMoveFrom(move) == 12) legalRookMoves++;
            std::cout << "2:";
            if (getMoveFrom(move) == 4)  legalKingMoves++;
            std::cout << "3:";
            doubleCheckBoard.UnmakeMove(move);
            std::cout << "4\n";
        }
    }
    std::cout << "In scacco doppio:\n";
    std::cout << "  Mosse legali della Torre: " << legalRookMoves << " (Deve essere 0)\n";
    std::cout << "  Mosse legali di fuga del Re: " << legalKingMoves << " (Deve essere maggiore di 0)\n";

    return 0;
}