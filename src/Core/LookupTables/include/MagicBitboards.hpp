#ifndef MAGICBITBOARDS_HPP
#define MAGICBITBOARDS_HPP

#include <cstdint>

// Magic bitboards per i pezzi scorrevoli: tabelle di attacco precalcolate,
// indicizzate tramite un hash moltiplicativo dei blocchi presenti sulla riga/colonna/diagonale.
namespace MagicBitboards {

    extern const uint64_t RookMagics[64];
    extern const uint64_t BishopMagics[64];
    extern uint64_t RookMasks[64];
    extern uint64_t BishopMasks[64];
    extern uint64_t RookAttacks[64][4096];
    extern uint64_t BishopAttacks[64][512];

    uint64_t ComputeRay(int direction, int square, uint64_t totalOccupation);

    uint64_t MaskRookAttacks(int square);
    uint64_t MaskBishopAttacks(int square);
    uint64_t SetOccupancy(int index, int bitsInMask, uint64_t attackMask);

    // Lookup a runtime: usati da MoveGenerator e da Board::IsSquareAttacked.
    uint64_t GetRookAttacks(int square, uint64_t occupancy);
    uint64_t GetBishopAttacks(int square, uint64_t occupancy);

    void InitMagicBitboards();
}

#endif
