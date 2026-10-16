#ifndef NNUE_CONSTANTS_HPP
#define NNUE_CONSTANTS_HPP

constexpr int NUM_FEATURES = 40960; // Number of inputs
constexpr int M = 256; // Accumulator size 
constexpr int K = 32;  // First dense layer

constexpr int MAX_ACTIVE_FEATURES = 30;
// Profondita' massima dello stack di accumulatori: ogni worker riparte da Reset(), 512 e' ampio.
constexpr int MAX_HISTORY_PLY = 512;

// PieceType → NNUE index (0=Pawn, 1=Knight, 2=Bishop, 3=Rook, 4=Queen)
constexpr int PIECE_TO_NNUE_IDX[6] = {
    0,   // PAWNS   → 0
    3,   // ROOKS   → 3
    1,   // KNIGHTS → 1
    2,   // BISHOPS → 2
    4,   // QUEEN   → 4
    -1,  // KING    → not used
};


#endif