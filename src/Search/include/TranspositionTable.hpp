#ifndef TRANSPOSITIONTABLE_HPP
#define TRANSPOSITIONTABLE_HPP

#include <memory>
#include <atomic>
#include <cstdint>
#include "Move.hpp"

enum TTFlag { ALPHA, BETA, EXACT };

struct TTEntry {
    std::atomic<uint64_t> key;  
    std::atomic<uint64_t> data; 
};

class TranspositionTable {
private:
    size_t numEntries = 0;
    std::atomic<size_t> occupiedEntries{0};
    std::unique_ptr<TTEntry[]> table = nullptr;
    bool active = true;

    const int MATE_SCORE = 990000;

    /*
    0-15 (16 bit): bestMove (Move -> uint16_t
    16-23 (8 bit): depth
    24-31 (8 bit): flag
    32-63 (32 bit): score
    */
    inline uint64_t PackData(Move bestMove, int depth, int score, TTFlag flag) const {
        uint64_t m = static_cast<uint64_t>(bestMove) & 0xFFFFULL;
        uint64_t d = static_cast<uint64_t>(static_cast<int8_t>(depth)) & 0xFFULL;
        uint64_t f = static_cast<uint64_t>(static_cast<uint8_t>(flag)) & 0xFFULL;
        uint64_t s = static_cast<uint64_t>(static_cast<int32_t>(score)) & 0xFFFFFFFFULL;
        return m | (d << 16) | (f << 24) | (s << 32);
    }

    inline void UnpackData(uint64_t data, Move& bestMove, int& depth, int& score, TTFlag& flag) const {
        bestMove = static_cast<Move>(data & 0xFFFFULL);
        depth = static_cast<int>(static_cast<int8_t>((data >> 16) & 0xFFULL));
        flag = static_cast<TTFlag>((data >> 24) & 0xFFULL);
        score = static_cast<int>(static_cast<int32_t>((data >> 32) & 0xFFFFFFFFULL));
    }

public:
    TranspositionTable() : numEntries(0), table(nullptr) {}

    TranspositionTable(size_t megaBytes) {
        Resize(megaBytes);
    }

    void Resize(size_t megaBytes) {
        size_t targetEntries = (megaBytes * 1024 * 1024) / sizeof(TTEntry);
        numEntries = 1;
        while ((numEntries * 2) <= targetEntries) {
            numEntries *= 2;
        }
        table = std::make_unique<TTEntry[]>(numEntries);
        Clear();
    }

    void Clear() {
        if (!table) return;
        for (size_t i = 0; i < numEntries; ++i) {
            table[i].key.store(0, std::memory_order_relaxed);
            table[i].data.store(0, std::memory_order_relaxed);
        }
        occupiedEntries.store(0, std::memory_order_relaxed);
    }

    bool Probe(uint64_t key, int depth, int alpha, int beta, int& score, Move& bestMove, int ply);
    void Record(uint64_t key, Move bestMove, int depth, int score, TTFlag flag, int ply);
    double GetFullnessPercentage();
    Move GetStoredMove(uint64_t key);
    
    void SetTTActive(bool state) { active = state; }
    size_t GetNumEntries() { return numEntries; }
};

#endif