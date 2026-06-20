#ifndef _LOOKUPTABLES
#define _LOOKUPTABLES

#include <cstdint>

namespace LookupTables{
    extern uint64_t knightAttacks[64];
    extern uint64_t kingAttacks[64];
    extern uint64_t pawnAttacks[2][64];

    // Zobrist Keys
    extern uint64_t pieceKeys[2][6][64];
    extern uint64_t sideKey;

    // Edge bitboard
    inline constexpr uint64_t notColumnA       = 0xFEFEFEFEFEFEFEFE; // 11111110 (Azzera la colonna A)
    inline constexpr uint64_t notColumnH       = 0x7F7F7F7F7F7F7F7F; // 01111111 (Azzera la colonna H)
    inline constexpr uint64_t notColumnADouble = 0xFCFCFCFCFCFCFCFC; // 11111100 (Azzera le colonne A e B)
    inline constexpr uint64_t notColumnHDouble = 0x3F3F3F3F3F3F3F3F; // 00111111 (Azzera le colonne G e H)
    inline constexpr uint64_t nullEdge = 0xFFFFFFFFFFFFFFFF;

    inline constexpr uint64_t thirdRow = 0x0000000000FF0000;
    inline constexpr uint64_t sixthRow = 0x0000FF0000000000;


    // Esempio di Piece-Square Table per i Cavalli (incentiva il centro)
    const int knightPST[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };

    // Esempio di PST per i pedoni (incentiva l'avanzamento, lato Bianco)
    const int pawnPST[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
        5,  5, 10, 25, 25, 10,  5,  5,
        0,  0,  0, 20, 20,  0,  0,  0,
        5, -5,-10,  0,  0,-10, -5,  5,
        5, 10, 10,-20,-20, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    };

    // PST per gli Alfieri: incentiva le diagonali attive, il controllo del centro e penalizza i bordi/angoli
    const int bishopPST[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };

    // PST per le Torri: incentiva la permanenza sulle colonne centrali (d, e) e premia moltissimo l'invasione della 7ª traversa
    const int rookPST[64] = {
        0,  0,  0,  5,  5,  0,  0,  0,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        5, 10, 10, 10, 10, 10, 10,  5,
        0,  0,  0,  0,  0,  0,  0,  0
    };

    // PST per la Donna: incentiva la centralizzazione prudente, scoraggia l'uscita prematura sui bordi in apertura
    const int queenPST[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -5,  0,  5,  5,  5,  5,  0, -5,
        0,  0,  5,  5,  5,  5,  0, -5,
        -10,  0,  5,  5,  5,  5,  0,-10,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };

    // PST per il Re (Medio Gioco): punisce severamente il Re al centro o esposto, premia l'arrocco (case g1/c1)
    const int kingMiddlePST[64] = {
        20, 30, 10,  0,  0, 10, 30, 20,
        20, 20,  0,  0,  0,  0, 20, 20,
        -10,-20,-20,-20,-20,-20,-20,-10,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30
    };

    void init();
}

#endif