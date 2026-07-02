#ifndef ENGINE_HPP
#define ENGINE_HPP
    
#include <chrono>

#include "Board.hpp"
#include "Move.hpp"
#include "TranspositionTable.hpp"
#include "IEvaluator.hpp"
#include "MoveGenerator.hpp"

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

    double hashFullness = 0.0;

    void ResetForNewMove() {
        nodesEvaluated = 0;
        qNodesEvaluated = 0;
        betaCutoffs = 0;
        lastMoveTimeMs = 0.0;
    }
};

class Engine {
private:
    TranspositionTable& tt;
    IEvaluator* evaluator;
    SearchStats stats;

    static const int MAX_PLY = 64;
    Move killerMoves[MAX_PLY][2];

    int AlphaBeta(Board& board, int depth, int alpha, int beta, int ply);
    int QuiescenceSearch(Board& board, int alpha, int beta);
    int ScoreMove(Board& board, Move move, Move ttMove, int ply);

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    double timeLimitMs;   
    bool timeIsUp;        

    void CheckTime();

public:
    Engine(TranspositionTable& t, IEvaluator* eval) : tt(t), evaluator(eval) {
        for(int i = 0; i < MAX_PLY; ++i) {
            killerMoves[i][0] = 0;
            killerMoves[i][1] = 0;
        }
    }

    Move GetBestMove(Board& board, int maxDepth, double allocatedTimeMs);

    const SearchStats& GetStats() const { return stats; }
};

#endif