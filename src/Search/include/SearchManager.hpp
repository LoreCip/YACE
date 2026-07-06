#ifndef SEARCH_MANAGER_HPP
#define SEARCH_MANAGER_HPP

#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional> 

#include "Board.hpp"
#include "TranspositionTable.hpp"
#include "SearchWorker.hpp"
#include "IEvaluator.hpp"

using EvaluatorFactory = std::function<std::unique_ptr<IEvaluator>()>;

class SearchManager {
private:
    TranspositionTable tt;
    std::atomic<bool> globalTimeIsUp{false};
    int numThreads;
    EvaluatorFactory evalFactory;

public:
    SearchManager(size_t ttMegaBytes, int threads, EvaluatorFactory factory)
        : tt(ttMegaBytes), numThreads(threads), evalFactory(factory) {}

    void SetThreads(int threads) { 
        numThreads = threads > 0 ? threads : 1; 
    }
    
    void ClearTT() { tt.Clear(); }

    Move Search(const Board& rootBoard, int maxDepth, double allocatedTimeMs, InfoReporter callback);
};

#endif