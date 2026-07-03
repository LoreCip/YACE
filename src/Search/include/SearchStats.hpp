struct SearchStats {
    // Move
    long long nodesEvaluated = 0;
    long long qNodesEvaluated = 0;
    long long betaCutoffs = 0;
    long long firstMoveCutoffs = 0;
    long long ttHits = 0;          
    int maxDepthReached = 0;       
    int selDepthReached = 0;       
    
    double lastMoveTimeMs = 0.0;
    double hashFullness = 0.0;

    // Session
    long long totalNodesSession = 0;
    double totalTimeMs = 0.0;
    double minTimeMs = 999999999.0;
    double maxTimeMs = 0.0;
    int movesPlayed = 0;

    void ResetForNewMove() {
        nodesEvaluated = 0;
        qNodesEvaluated = 0;
        betaCutoffs = 0;
        firstMoveCutoffs = 0;
        ttHits = 0;
        maxDepthReached = 0;
        selDepthReached = 0;
        lastMoveTimeMs = 0.0;
    }

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
