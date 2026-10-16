#ifndef SEARCH_STATS_HPP
#define SEARCH_STATS_HPP

struct SearchStats {
    // Move
    long long nodesEvaluated = 0;
    long long qNodesEvaluated = 0;
    long long betaCutoffs = 0;
    long long firstMoveCutoffs = 0;
    long long ttHits = 0;          
    int selDepthReached = 0;       
    
    double lastMoveTimeMs = 0.0;
    double hashFullness = 0.0;

    // --- Helpers ---
    long long GetTotalNodes() const { 
        return nodesEvaluated + qNodesEvaluated; 
    }
    
    long long GetNPS() const {
        if (lastMoveTimeMs <= 0.001) return 0;
        return static_cast<long long>((GetTotalNodes() / lastMoveTimeMs) * 1000.0);
    }

    double GetMoveOrderingQuality() const {
        if (betaCutoffs == 0) return 0.0;
        return (static_cast<double>(firstMoveCutoffs) / betaCutoffs) * 100.0;
    }
};

#endif
