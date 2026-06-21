#ifndef _MINIMAX
#define _MINIMAX

#include <chrono>

#include "Board.hpp"
#include "Move.hpp"

struct SearchStats {
    long long nodesEvaluated = 0;
    long long qNodesEvaluated = 0;
    long long betaCutoffs = 0;
    double lastMoveTimeMs = 0.0;

    long long totalNodesSession = 0;
    double totalTimeMs = 0.0;
    double minTimeMs = 999999999.0;
    double maxTimeMs = 0.0;
    int movesPlayed = 0;

    void ResetForNewMove() {
        nodesEvaluated = 0;
        qNodesEvaluated = 0;
        betaCutoffs = 0;
        lastMoveTimeMs = 0.0;
    }
};

class Engine {
private:
    SearchStats stats;

    int AlphaBeta(Board& board, int depth, int alpha, int beta);
    int QuiescenceSearch(Board& board, int alpha, int beta);
    void GeneratePawnMoves(Board& board, Move* moveList, int& moveCount);

public:
    Move GetBestMove(Board& board, int depth);
    int GenerateAllMoves(Board& board, Move* moveList);

    const SearchStats& GetStats() const { return stats; }
};

#endif