#ifndef ZOBRIST_HPP
#define ZOBRIST_HPP

#include <cstdint>

// Chiavi Zobrist: permettono di calcolare in modo incrementale un hash
// (quasi) univoco della posizione (pezzi, arrocco, en passant, turno).
namespace Zobrist {

    extern uint64_t pieceKeys[2][6][64];
    extern uint64_t castlingKeys[16];
    extern uint64_t enPassantKeys[65];
    extern uint64_t sideKey;

    void Init();
}

#endif
