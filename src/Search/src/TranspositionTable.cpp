#include "TranspositionTable.hpp"


bool TranspositionTable::Probe(uint64_t key, int depth, int alpha, int beta, int& score, Move& bestMove) {
    if (!active) return false;

    size_t index = key & (numEntries - 1);
    TTEntry& entry = table[index];
    if (entry.key == key) {
        bestMove = entry.bestMove;
        if (entry.depth >= depth) {
            if (entry.flag == EXACT) {
                int returnedScore = entry.score;
                if (returnedScore > MATE_SCORE)  returnedScore -= depth; // Riconvertilo per la profondità attuale
                if (returnedScore < -MATE_SCORE) returnedScore += depth;
                score = returnedScore;
                return true;
            }
            if (entry.flag == ALPHA && entry.score <= alpha) {
                score = alpha;
                return true;
            }
            if (entry.flag == BETA && entry.score >= beta) {
                score = beta;
                return true;
            }
        }
    }
    return false;
}

void TranspositionTable::Record(uint64_t key, Move bestMove, int depth, int score, TTFlag flag) {
    if (!active) return;
    size_t index = key & (numEntries - 1);
    
    int scoreToSave = score;
    if (score > MATE_SCORE)  scoreToSave += depth;
    if (score < -MATE_SCORE) scoreToSave -= depth;

    if (bestMove == 0 && table[index].key == key) bestMove = table[index].bestMove;

    if (table[index].key == 0) occupiedEntries++;
    if (table[index].key == 0 || table[index].depth <= depth) {
        table[index] = { key, bestMove, depth, scoreToSave, flag };
    }
}


double TranspositionTable::GetFullnessPercentage() {
    if (numEntries == 0) return 0.0;  
    return (static_cast<double>(occupiedEntries) / numEntries) * 100.0;
}


Move TranspositionTable::GetStoredMove(uint64_t key) {
    size_t index = key & (numEntries - 1);
    if (table[index].key == key) {
        return table[index].bestMove;
    }
    return 0;
}