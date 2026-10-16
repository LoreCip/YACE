#ifndef SEARCH_MANAGER_HPP
#define SEARCH_MANAGER_HPP

#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>

#include "Board.hpp"
#include "TranspositionTable.hpp"
#include "SearchWorker.hpp"
#include "NnueNetwork.hpp"

class SearchManager {
private:
    TranspositionTable tt;
    std::atomic<bool> globalTimeIsUp{false};
    int numThreads;
    // Condivisa in sola lettura da tutti i thread; nullptr = valutazione classica (HCE).
    std::shared_ptr<const NnueNetwork> network;

public:
    SearchManager(size_t ttMegaBytes, int threads, std::shared_ptr<const NnueNetwork> network)
        : tt(ttMegaBytes), numThreads(threads), network(std::move(network)) {}

    void ClearTT() { tt.Clear(); }

    Move Search(const Board& rootBoard, int maxDepth, double allocatedTimeMs, InfoReporter callback);
};

#endif