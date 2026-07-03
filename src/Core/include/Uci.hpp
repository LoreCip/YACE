#ifndef UCI_HPP
#define UCI_HPP

#include <string>
#include <functional>
#include "Move.hpp"
#include "Board.hpp"

namespace UCI {
    void Initialize();

    using LoggerCallback = std::function<void(int depth, int selDepth, int score, 
                                              long long timeMs, long long nodes, long long nps, 
                                              int hashFull, const std::string& pvMove,
                                              double moveOrdering, long long ttHits, long long qNodes)>;

    using GoCallback = std::function<Move(int targetDepth, double allocatedTimeMs, LoggerCallback logger)>;
    using PositionLoadedCallback = std::function<void()>;
    using MoveMadeCallback = std::function<void(Move)>;

    void Loop(Board& board, 
              GoCallback onGoCommand, 
              PositionLoadedCallback onPositionLoaded, 
              MoveMadeCallback onMoveMade);

    void ReportSearchInfo(int depth, int selDepth, int score, 
                          double timeMs, long long nodes, long long nps, 
                          int hashFull, const std::string& pvMove,
                          double moveOrdering, long long ttHits, long long qNodes);

    void ReportDebugString(const std::string& message);

    std::string MoveToString(Move move);
    Move StringToMove(const Board& board, const std::string& moveStr);
}

#endif