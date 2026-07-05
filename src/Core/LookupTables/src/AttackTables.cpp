#include "AttackTables.hpp"
#include "BitOperations.hpp"

namespace AttackTables {

    const uint64_t notColumnA       = 0xFEFEFEFEFEFEFEFEULL;
    const uint64_t notColumnB       = 0xFDFDFDFDFDFDFDFDULL;
    const uint64_t notColumnG       = 0xBFBFBFBFBFBFBFBFULL;
    const uint64_t notColumnH       = 0x7F7F7F7F7F7F7F7FULL;
    const uint64_t notColumnADouble = 0xFCFCFCFCFCFCFCFCULL;
    const uint64_t notColumnHDouble = 0x3F3F3F3F3F3F3F3FULL;
    const uint64_t nullEdge         = 0xFFFFFFFFFFFFFFFFULL;

    const uint64_t maskWK = 0x60ULL;                 // Caselle f1, g1
    const uint64_t maskWQ = 0xEULL;                  // Caselle b1, c1, d1
    const uint64_t maskBK = 0x6000000000000000ULL;   // Caselle f8, g8
    const uint64_t maskBQ = 0x0E00000000000000ULL;   // Caselle b8, c8, d8

    const uint8_t castlingMasks[64] = {
        13, 15, 15, 15, 12, 15, 15, 14, // 0-7   (White: rook A1, king, rook H1)
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
         7, 15, 15, 15,  3, 15, 15, 11  // 56-63 (Black: rook A8, king, rook H8)
    };

    uint64_t knightAttacks[64]  = {0};
    uint64_t kingAttacks[64]    = {0};
    uint64_t pawnAttacks[2][64] = {{0}};

    void Init() {
        for (int i = 0; i < 64; i++) {

            // 1. Knights
            uint64_t bitboard = 0ULL;
            bitboard = setBit(bitboard, i);
            uint64_t attacks = 0ULL;

            attacks |= ((bitboard & notColumnH) << 17);
            attacks |= ((bitboard & notColumnHDouble) << 10);
            attacks |= ((bitboard & notColumnHDouble) >> 6);
            attacks |= ((bitboard & notColumnH) >> 15);

            attacks |= ((bitboard & notColumnA) >> 17);
            attacks |= ((bitboard & notColumnADouble) >> 10);
            attacks |= ((bitboard & notColumnADouble) << 6);
            attacks |= ((bitboard & notColumnA) << 15);

            knightAttacks[i] = attacks;

            // 2. King
            bitboard = 0ULL;
            bitboard = setBit(bitboard, i);
            attacks = 0ULL;

            attacks |= ((bitboard & notColumnA) >> 1);
            attacks |= ((bitboard & notColumnH) << 1);
            attacks |= (bitboard << 8);
            attacks |= (bitboard >> 8);

            attacks |= ((bitboard & notColumnH) << 9);
            attacks |= ((bitboard & notColumnA) << 7);
            attacks |= ((bitboard & notColumnH) >> 7);
            attacks |= ((bitboard & notColumnA) >> 9);

            kingAttacks[i] = attacks;

            // 3. Pawn Captures
            // White
            bitboard = 0ULL;
            bitboard = setBit(bitboard, i);
            attacks = 0ULL;
            attacks |= ((bitboard & notColumnA) << 7);
            attacks |= ((bitboard & notColumnH) << 9);
            pawnAttacks[0][i] = attacks;

            // Black
            bitboard = 0ULL;
            bitboard = setBit(bitboard, i);
            attacks = 0ULL;
            attacks |= ((bitboard & notColumnH) >> 7);
            attacks |= ((bitboard & notColumnA) >> 9);
            pawnAttacks[1][i] = attacks;
        }
    }
}
