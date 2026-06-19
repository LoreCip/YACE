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
    inline constexpr uint64_t notColumnA = 0xFEFEFEFEFEFEFEFE;       // Blocca Colonna A
    inline constexpr uint64_t notColumnH = 0x7F7F7F7F7F7F7F7F;       // Blocca Colonna H
    inline constexpr uint64_t notColumnADouble = 0xFCFCFCFCFCFCFCFC; // Blocca Colonne A e B
    inline constexpr uint64_t notColumnHDouble = 0x3F3F3F3F3F3F3F3F; // Blocca Colonne G e H
    inline constexpr uint64_t nullEdge = 0xFFFFFFFFFFFFFFFF;

    inline constexpr uint64_t thirdRow = 0x0000000000FF0000;
    inline constexpr uint64_t sixthRow = 0x0000FF0000000000;

    void init();
}

#endif