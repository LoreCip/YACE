#include "TranspositionTable.hpp"

bool TranspositionTable::Probe(uint64_t key, int depth, int alpha, int beta, int& score, Move& bestMove, int ply) {
    if (!active || !table) return false;

    size_t index = key & (numEntries - 1);
    TTEntry& entry = table[index];

    uint64_t stored_data = entry.data.load(std::memory_order_relaxed);
    uint64_t stored_key = entry.key.load(std::memory_order_relaxed);

    if ((stored_key ^ stored_data) == key) {
        Move ttMove;
        int ttDepth;
        int ttScore;
        TTFlag ttFlag;
        
        UnpackData(stored_data, ttMove, ttDepth, ttScore, ttFlag);
        bestMove = ttMove;

        if (ttDepth >= depth) {
            if (ttFlag == EXACT) {
                int returnedScore = ttScore;
                if (returnedScore > MATE_SCORE)  returnedScore -= ply; 
                if (returnedScore < -MATE_SCORE) returnedScore += ply;
                score = returnedScore;
                return true;
            }
            if (ttFlag == ALPHA && ttScore <= alpha) {
                score = alpha;
                return true;
            }
            if (ttFlag == BETA && ttScore >= beta) {
                score = beta;
                return true;
            }
        }
    }
    return false;
}

void TranspositionTable::Record(uint64_t key, Move bestMove, int depth, int score, TTFlag flag, int ply) {
    if (!active || !table) return;
    size_t index = key & (numEntries - 1);
    TTEntry& entry = table[index];
    
    int scoreToSave = score;
    if (score > MATE_SCORE)  scoreToSave += ply;
    if (score < -MATE_SCORE) scoreToSave -= ply;

    uint64_t old_data = entry.data.load(std::memory_order_relaxed);
    uint64_t old_key = entry.key.load(std::memory_order_relaxed);

    if (bestMove == 0 && (old_key ^ old_data) == key) {
        bestMove = static_cast<Move>(old_data & 0xFFFFULL);
    }

    if (old_key == 0) occupiedEntries.fetch_add(1, std::memory_order_relaxed);

    uint64_t packed_data = PackData(bestMove, depth, scoreToSave, flag);
    uint64_t signature = key ^ packed_data;

    int old_depth = static_cast<int>(static_cast<int8_t>((old_data >> 16) & 0xFFULL));
    bool is_same_pos = (old_key ^ old_data) == key;

    if (old_key == 0 || is_same_pos || old_depth <= depth) {
        entry.data.store(packed_data, std::memory_order_relaxed);
        entry.key.store(signature, std::memory_order_relaxed);
    }
}

double TranspositionTable::GetFullnessPercentage() {
    if (numEntries == 0) return -1.0;  
    return (static_cast<double>(occupiedEntries.load(std::memory_order_relaxed)) / numEntries) * 100.0;
}

Move TranspositionTable::GetStoredMove(uint64_t key) {
    if (!table) return 0;
    size_t index = key & (numEntries - 1);
    uint64_t stored_data = table[index].data.load(std::memory_order_relaxed);
    uint64_t stored_key = table[index].key.load(std::memory_order_relaxed);
    
    if ((stored_key ^ stored_data) == key) {
        return static_cast<Move>(stored_data & 0xFFFFULL);
    }
    return 0;
}