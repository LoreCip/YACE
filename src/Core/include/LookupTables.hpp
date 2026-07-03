#ifndef LOOKUPTABLES_HPP
#define LOOKUPTABLES_HPP

#include <cstdint>
#include <fstream>

namespace LookupTables {
    extern const uint64_t notColumnA;
    extern const uint64_t notColumnB;
    extern const uint64_t notColumnG;
    extern const uint64_t notColumnH;
    extern const uint64_t notColumnADouble;
    extern const uint64_t notColumnHDouble;
    extern const uint64_t nullEdge;

    extern const uint64_t maskWK;
    extern const uint64_t maskWQ;
    extern const uint64_t maskBK;
    extern const uint64_t maskBQ;

    extern const uint64_t (&knightAttacks)[64];
    extern const uint64_t (&kingAttacks)[64];
    extern const uint64_t (&pawnAttacks)[2][64];

    extern const uint8_t (&castlingMasks)[64];

    // --- ZOBRIST KEYS ---
    extern const uint64_t (&castlingKeys)[16];
    extern const uint64_t (&enPassantKeys)[65];
    extern const uint64_t (&pieceKeys)[2][6][64];
    extern const uint64_t &sideKey;

    // --- PIECE-SQUARE TABLES (PST) ---
    extern int pstTables[6][64];
    extern int pieceValues[6];

    void init(const std::string& configFile);
    bool readArrayBlock(std::ifstream& file, int* targetArray, int expectedSize);
}

#endif