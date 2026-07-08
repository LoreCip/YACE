#ifndef SEARCH_WORKER_HPP
#define SEARCH_WORKER_HPP

#include <atomic>
#include <memory>
#include <functional> 
#include "Board.hpp"
#include "TranspositionTable.hpp"
#include "SearchStats.hpp"
#include "IEvaluator.hpp"
#include "Move.hpp"

using InfoReporter = std::function<void(int depth, int score, const SearchStats& stats, Move bestMove)>;

class SearchWorker {
private:
    static const int MAX_PLY = 64;

    int threadId;
    bool isMainThread;

    Board board; 
    SearchStats stats;
    int historyTable[2][64][64] = {{{0}}};
    Move killerMoves[MAX_PLY][2] = {{0}};

    TranspositionTable& tt;               
    std::atomic<bool>& globalTimeIsUp;    
    std::unique_ptr<IEvaluator> evaluator;

    // Funzioni di ricerca
    int AlphaBeta(int depth, int alpha, int beta, int ply);
    int QuiescenceSearch(int alpha, int beta, int ply);
    int ScoreMove(Move move, Move ttMove, int ply);
    void ClearHistory();

    bool SEE(Move move, int threshold = 0);
    uint64_t GetLeastValuableAttacker(int square, Color color, uint64_t occupancy, PieceType& outPiece);

public:
    Move bestRootMove = 0;

    SearchWorker(int id, bool mainThread, const Board& rootBoard, 
                 TranspositionTable& globalTT, std::atomic<bool>& stopFlag,
                 std::unique_ptr<IEvaluator> eval)
        : threadId(id), 
          isMainThread(mainThread), 
          board(rootBoard),        
          tt(globalTT), 
          globalTimeIsUp(stopFlag),
          evaluator(std::move(eval)) {}

    void Search(int maxDepth, InfoReporter callback = nullptr);
    const SearchStats& GetStats() const { return stats; }
};

#endif