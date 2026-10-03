#ifndef MOVE_HPP
#define MOVE_HPP

#include <cstdint>

#include "Types.hpp"

using Move = uint16_t;
/*
15 14 13 12 | 11 10  9  8  7  6 |  5  4  3  2  1  0
   Flags    |    To Square      |    From Square
   (4 bit)  |     (6 bit)       |     (6 bit)
*/

inline Move createMove(int from, int to, int flags) {
    return (Move)from | ((Move)to << 6) | ((Move)flags << 12);
}
inline int getMoveFrom(Move move){
    return move & (Move)0x3f; // 00111111
}
inline int getMoveTo(Move move){
    return (move >> 6) & (Move)0x3f; // 00111111
}
inline int getMoveFlags(Move move){
    return move >> 12;
}

enum FlagMap { 
    MOVE,        // Simple move
    DMOVE,       // Pawn double move
    CASTLING,    // Castling
    CAPTURE,     // Simple capture
    ENPASS,      // En passant
    PRBISHOP,    // Promotion to bishop
    PRKNIGHT,    // Promotion to knight 
    PRROOK,      // Promotion to rook
    PRQUEEN,     // Promotion to queen
    PRCAPBISHOP, // Capture + Promotion to bishop 
    PRCAPKNIGHT, // Capture + Promotion to knight
    PRCAPROOK,   // Capture + Promotion to rook
    PRCAPQUEEN   // Capture + Promotion to queen
};

// Le funzioni seguenti prendono i FLAG (getMoveFlags(move)), non la mossa intera.
inline PieceType promotionPieceFromFlags(int flags) {
    switch (flags) {
        case FlagMap::PRKNIGHT: case FlagMap::PRCAPKNIGHT:
            return PieceType::KNIGHT;
        case FlagMap::PRBISHOP: case FlagMap::PRCAPBISHOP:
            return PieceType::BISHOP;
        case FlagMap::PRROOK: case FlagMap::PRCAPROOK:
            return PieceType::ROOK;
        case FlagMap::PRQUEEN: case FlagMap::PRCAPQUEEN:
            return PieceType::QUEEN;
        default:
            return PieceType::NONE;   // Not a promotion Move
    }
}

inline bool isEnPassantFlag(int flags) {
    return flags == FlagMap::ENPASS;
}

inline bool isCaptureFlag(int flags) {
    return flags == FlagMap::CAPTURE || flags >= FlagMap::PRCAPBISHOP;
}

// Arrocco: case di partenza/arrivo della torre, dato l'arrivo del re (g1/c1/g8/c8).
inline void castlingRookSquares(int kingTo, int& rookFrom, int& rookTo) {
    bool kingSide = (kingTo % 8 == 6);
    rookFrom = kingSide ? kingTo + 1 : kingTo - 2;
    rookTo   = kingSide ? kingTo - 1 : kingTo + 1;
}

#endif