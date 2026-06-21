#ifndef _TRANSPOSITIONTABLE
#define _TRANSPOSITIONTABLE

#include <vector>

#include "Move.hpp"

enum TTFlag { ALPHA, BETA, EXACT };

struct TTEntry {
    uint64_t key;      // La chiave di Zobrist completa (per verificare le collisioni)
    Move bestMove;     // La mossa migliore trovata in questa posizione
    int depth;         // A che profondità era stata cercata
    int score;         // Il punteggio di valutazione
    TTFlag flag;       // Tipo di bound (Esatto, Limite Inferiore, Limite Superiore)
};

class TranspositionTable {
private:
    size_t numEntries;
    std::vector<TTEntry> table;
    bool active = true;

    const int MATE_SCORE = 990000;

public:
    TranspositionTable(size_t megaBytes) 
        : numEntries((megaBytes * 1024 * 1024) / sizeof(TTEntry)),
          table(numEntries, TTEntry{0, 0, 0, 0, EXACT})
    {}

    void Clear() {
        std::fill(table.begin(), table.end(), TTEntry{0, 0, 0, 0, EXACT});
    }

    bool Probe(uint64_t key, int depth, int alpha, int beta, int& score, Move& bestMove);
    void Record(uint64_t key, Move bestMove, int depth, int score, TTFlag flag);

    double GetFullnessPercentage();
    
    void SetTTActive(bool state) { active = state; }

    size_t GetNumEntries() {
        return numEntries;
    }

    Move GetStoredMove(uint64_t key);
};


#endif