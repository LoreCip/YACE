#ifndef _MOVE
#define _MOVE

#include <cstdint>

using Move = uint16_t;
/*
15 14 13 12 | 11 10  9  8  7  6 |  5  4  3  2  1  0
   Flags    |    To Square      |    From Square
   (4 bit)  |     (6 bit)       |     (6 bit)
*/

inline Move createMove(int from, int to, int flags);
Move createMove(int from, int to, int flags) {
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

#endif