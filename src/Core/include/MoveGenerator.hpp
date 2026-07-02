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
    // Generate all pseudo-legal moves
    int GenerateAllMoves(const Board& board, MoveList& moveList);
    
    // Single functions
    void GeneratePawnMoves(const Board& board, MoveList& moveList, Color us);
    void GenerateKnightMoves(const Board& board, MoveList& moveList, Color us);
    void GenerateKingMoves(const Board& board, MoveList& moveList, Color us);
    void GenerateSlidingMoves(const Board& board, MoveList& moveList, Color us, PieceType piece);

    // Helper
    uint64_t ComputeRay(int direction, uint64_t edge, int square, uint64_t totalOccupation);
}

#endif