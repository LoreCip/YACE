#ifndef ATTACKTABLES_HPP
#define ATTACKTABLES_HPP

#include <cstdint>

// Tabelle di attacco precalcolate per i pezzi "non scorrevoli"
// (cavallo, re, pedone), piu' i dati statici necessari per l'arrocco.
namespace AttackTables {

    // Maschere di colonna, usate per evitare il wraparound quando si shiftano le bitboard
    extern const uint64_t notColumnA;
    extern const uint64_t notColumnB;
    extern const uint64_t notColumnG;
    extern const uint64_t notColumnH;
    extern const uint64_t notColumnADouble;
    extern const uint64_t notColumnHDouble;
    extern const uint64_t nullEdge;

    // Caselle che devono essere libere per ciascun tipo di arrocco.
    extern const uint64_t maskWK;
    extern const uint64_t maskWQ;
    extern const uint64_t maskBK;
    extern const uint64_t maskBQ;

    // Maschera per-casa usata per aggiornare i diritti di arrocco
    extern const uint8_t castlingMasks[64];

    extern uint64_t knightAttacks[64];
    extern uint64_t kingAttacks[64];
    extern uint64_t pawnAttacks[2][64];

    // Popola knightAttacks / kingAttacks / pawnAttacks a runtime.
    void Init();
}

#endif
