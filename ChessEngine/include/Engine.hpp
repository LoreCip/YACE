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

    static const int MAX_PLY = 64;
    Move killerMoves[MAX_PLY][2];

    int AlphaBeta(Board& board, int depth, int alpha, int beta, int ply);
    int QuiescenceSearch(Board& board, int alpha, int beta);
    void GeneratePawnMoves(Board& board, Move* moveList, int& moveCount);
    int ScoreMove(Board& board, Move move, Move ttMove, int ply);

    Move GetStoredMove(uint64_t key);

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    double timeLimitMs;   // Tempo massimo per questa mossa
    bool timeIsUp;        // L'interruttore di emergenza

    void CheckTime();     // Funzione per controllare l'orologio

public:
    Engine(): tt(64){}

    Move GetBestMove(Board& board, int maxDepth, double allocatedTimeMs);
    int GenerateAllMoves(Board& board, Move* moveList);

    const SearchStats& GetStats() const { return stats; }
    void SetTTActive(bool state) { tt.SetTTActive(state); }
    
};

#endif