#ifndef _MINIMAX
#define _MINIMAX

#include <vector>

#include "Board.hpp"
#include "Move.hpp"

class Engine {
private:
    int Minimax(Board& board, int depth);
    std::vector<Move> GenerateAllMoves(Board& board);
    void GeneratePawnMoves(Board& board, std::vector<Move>& moveList);

public:
    Move GetBestMove(Board& board, int depth);

};

#endif