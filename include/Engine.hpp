#ifndef _MINIMAX
#define _MINIMAX

#include <vector>

#include "Board.hpp"
#include "Move.hpp"

class Engine {
private:
    int AlphaBeta(Board& board, int depth, int alpha, int beta);
    int QuiescenceSearch(Board& board, int alpha, int beta);
    void GeneratePawnMoves(Board& board, std::vector<Move>& moveList);

public:
    Move GetBestMove(Board& board, int depth);
    std::vector<Move> GenerateAllMoves(Board& board);

};

#endif