#ifndef _LOOKUPTABLES
#define _LOOKUPTABLES

#include <cstdint>

namespace LookupTables{
    extern uint64_t knightAttacks[64];
    extern uint64_t kingAttacks[64];
    extern uint64_t pawnAttacks[2][64];

    // Edge bitboard
    inline constexpr uint64_t notColumnA = 0x7F7F7F7F7F7F7F7F; // 01111111 ....
    inline constexpr uint64_t notColumnH = 0xFEFEFEFEFEFEFEFE; // 11111110 ....
    inline constexpr uint64_t notColumnADouble = 0x3F3F3F3F3F3F3F3F; // 00111111 ....
    inline constexpr uint64_t notColumnHDouble = 0xFCFCFCFCFCFCFCFC; // 11111100 ....
    inline constexpr uint64_t nullEdge = 0xFFFFFFFFFFFFFFFF;

    inline constexpr uint64_t thirdRow = 0x0000000000FF0000;
    inline constexpr uint64_t sixthRow = 0x0000FF0000000000;

    void init();
}

#endif