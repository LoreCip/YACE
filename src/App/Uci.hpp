#ifndef UCI_HPP
#define UCI_HPP

#include <string>
#include "Board.hpp"
#include "SearchManager.hpp"

namespace UCI {
    void Initialize();
    void Loop(Board& board, SearchManager& search);
    void ReportDebugString(const std::string& message);
}

#endif
