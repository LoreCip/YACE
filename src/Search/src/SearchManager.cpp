#include "SearchManager.hpp"

Move SearchManager::Search(const Board& rootBoard, int maxDepth, double allocatedTimeMs, InfoReporter callback) {
    globalTimeIsUp.store(false, std::memory_order_relaxed);

    std::vector<std::unique_ptr<SearchWorker>> workers;
    for (int i = 0; i < numThreads; ++i) {
        bool isMainThread = (i == 0); 
        workers.push_back(std::make_unique<SearchWorker>(
            i, isMainThread, rootBoard, tt, globalTimeIsUp, evalFactory()
        ));
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    InfoReporter aggregatedCallback = [&](int depth, int score, const SearchStats& t0_stats, Move bestMove) {
        SearchStats aggStats = t0_stats;
        
        aggStats.nodesEvaluated = 0;
        aggStats.qNodesEvaluated = 0;
        aggStats.ttHits = 0;

        for (const auto& w : workers) {
            aggStats.nodesEvaluated += w->GetStats().nodesEvaluated;
            aggStats.qNodesEvaluated += w->GetStats().qNodesEvaluated;
            aggStats.ttHits += w->GetStats().ttHits;
        }

        if (callback) {
            callback(depth, score, aggStats, bestMove);
        }
    };

    std::thread timerThread([&]() {
        while (!globalTimeIsUp.load(std::memory_order_relaxed)) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = now - startTime;
            
            if (elapsed.count() >= allocatedTimeMs) {
                globalTimeIsUp.store(true, std::memory_order_relaxed);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    std::vector<std::thread> helperThreads;
    for (int i = 1; i < numThreads; ++i) {
        helperThreads.emplace_back(&SearchWorker::Search, workers[i].get(), maxDepth, nullptr);
    }

    workers[0]->Search(maxDepth, aggregatedCallback);

    globalTimeIsUp.store(true, std::memory_order_relaxed);

    if (timerThread.joinable()) timerThread.join();
    for (auto& t : helperThreads) {
        if (t.joinable()) t.join();
    }

    return workers[0]->bestRootMove;
}