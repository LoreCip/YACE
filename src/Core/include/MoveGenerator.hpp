#ifndef MOVEGEN_HPP
#define MOVEGEN_HPP

#include "Move.hpp"
#include "Types.hpp"

class Board;

struct MoveList {
    Move moves[256];
    int count = 0;
    inline void push(Move m) { moves[count++] = m; }
};

namespace MoveGen {
    // Mosse pseudo-legali (la legalita' la verifica Board::MakeMove); ritornano moveList.count
    int GenerateAllMoves(const Board& board, MoveList& moveList);
    // Solo catture e promozioni a donna, per la quiescenza
    int GenerateCaptureMoves(const Board& board, MoveList& moveList);
}

#endif