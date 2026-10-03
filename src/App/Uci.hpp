#ifndef UCI_HPP
#define UCI_HPP

#include <string>
#include <functional>
#include "Move.hpp"
#include "Board.hpp"
#include "SearchWorker.hpp"

namespace UCI {
    void Initialize();

    using GoCallback = std::function<Move(int targetDepth, double allocatedTimeMs, InfoReporter reporter)>;
    using PositionLoadedCallback = std::function<void()>;

    void Loop(Board& board, GoCallback onGoCommand, PositionLoadedCallback onPositionLoaded);

    void ReportDebugString(const std::string& message);

    std::string MoveToString(Move move);
    // Ritorna 0 se la stringa non corrisponde a nessuna mossa pseudo-legale della posizione.
    Move StringToMove(const Board& board, const std::string& moveStr);
}

#endif
