#ifndef NNUEADAPTER
#define NNUEADAPTER

#include "Constants.hpp"
#include "NnueNetwork.hpp"
#include "Board.hpp"
#include "Pieces.hpp"
#include "Move.hpp"
#include <string>
#include <cstdint>

inline int pop_lsb(uint64_t& bb) {
    int sq = __builtin_ctzll(bb);  // indice del bit meno significativo
    bb &= bb - 1;                  // rimuove quel bit
    return sq;
}

struct NnueState {
    double accumulator[2][ACC_SIZE];
    bool computed[2];
};

class NnueAdapter {
private:
    static bool disableForPerft;
    static bool eager;
    static NnueState nnueStack[MAX_HISTORY_PLY];
    static int currentPly;
    static NnueNetwork network;

    static int ClampOrComputeFeatureIndex(int kingSq, int pieceSq, int pieceType, int pieceColor, int perspective);
    static inline int FlipSquareVertical(int square);
    static void RefreshAccumulator(Board& board, int perspective);
    static void IncrementalUpdate(Board& board, Move move, int perspective);
    static int MakeFeatureIndex(int square, PiecesEnum::Type pieceType, int pieceColor, int kingSq, int perspective);

public:
    static bool Initialize(const std::string& weightsPath);
    static void Reset(Board& board);
    static int NnueEvaluate(Board& board);

    static void OnMakeMove(Board& board, Move move);
    static void OnUnmakeMove();

    static void SetEager(bool var){
        eager = var;
    }
    static bool GetEarger(){
        return eager;
    }
};

#endif