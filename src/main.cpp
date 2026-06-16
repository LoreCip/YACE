#include <iostream>
#include <cstdint>

#include "BitOperations.hpp"
#include "LookupTables.hpp"
#include "Pieces.hpp"
#include "Board.hpp"

/**
 * La casella 0 (A1) è in basso a sinistra, la casella 63 (H8) è in alto a destra.
 */
void printBitboard(uint64_t bitboard, int idx = -1);
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

    return 0;
}