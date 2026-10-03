#ifndef MAGICBITBOARDS_HPP
#define MAGICBITBOARDS_HPP

#include <cstdint>

// Magic bitboards per i pezzi scorrevoli: tabelle di attacco precalcolate,
// indicizzate tramite un hash moltiplicativo dei blocchi presenti sulla riga/colonna/diagonale.
namespace MagicBitboards {
    uint64_t GetRookAttacks(int square, uint64_t occupancy);
    uint64_t GetBishopAttacks(int square, uint64_t occupancy);

    void InitMagicBitboards();
}

#endif
