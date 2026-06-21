#ifndef _MINIMAX
#define _MINIMAX
    
#include <chrono>

#include "Board.hpp"
#include "Move.hpp"
#include "TranspositionTable.hpp"


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
    SearchStats stats;
    TranspositionTable tt;

    int AlphaBeta(Board& board, int depth, int alpha, int beta);
    int QuiescenceSearch(Board& board, int alpha, int beta);
    void GeneratePawnMoves(Board& board, Move* moveList, int& moveCount);
    int ScoreMove(Board& board, Move move, Move ttMove);

    Move GetStoredMove(uint64_t key);

public:
    Engine(): tt(64){}

    Move GetBestMove(Board& board, int depth);
    int GenerateAllMoves(Board& board, Move* moveList);

    const SearchStats& GetStats() const { return stats; }
    void SetTTActive(bool state) { tt.SetTTActive(state); }
    
};

#endif