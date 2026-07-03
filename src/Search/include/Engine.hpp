#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <functional>
#include <chrono>

#include "Board.hpp"
#include "Move.hpp"
#include "TranspositionTable.hpp"
#include "IEvaluator.hpp"
#include "MoveGenerator.hpp"
#include "SearchStats.hpp"

using InfoReporter = std::function<void(int depth, int score, const SearchStats& stats, Move bestMove)>;

class Engine {
private:
    TranspositionTable& tt;
    IEvaluator* evaluator;
    SearchStats stats;

    InfoReporter reportCallback;

    static const int MAX_PLY = 64;
    Move killerMoves[MAX_PLY][2];

    int AlphaBeta(Board& board, int depth, int alpha, int beta, int ply);
    int QuiescenceSearch(Board& board, int alpha, int beta, int ply);
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

    Move GetBestMove(Board& board, int maxDepth, double allocatedTimeMs, InfoReporter callback = nullptr);

    const SearchStats& GetStats() const { return stats; }
};

#endif