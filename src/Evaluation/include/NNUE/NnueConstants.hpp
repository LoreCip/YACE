#ifndef NNUE_CONSTANTS_HPP
#define NNUE_CONSTANTS_HPP

constexpr int NUM_FEATURES = 40960; // Number of inputs
constexpr int M = 256; // Accumulator size 
constexpr int K = 32;  // First dense layer

constexpr int MAX_ACTIVE_FEATURES = 30;
// 512, non 1024: SearchWorker::MAX_PLY e' 64 e i worker ora fanno Reset() alla costruzione
// (vedi Step 4), quindi un worker di ricerca non si avvicina mai a questo limite — solo il
// root evaluator accumula plies reali di partita (un game "normale" resta ben sotto 512*2
// mezze-mosse). Dimezza lo stack per-thread (utile ora che ogni ply porta anche un AccDelta,
// vedi lo Step 8 dell'ottimizzazione di velocita').
constexpr int MAX_HISTORY_PLY = 512;
constexpr int ACC_SIZE = 256; 

// Translates PiecesEnum::Type to  NNUE index (0=Pawn,1=Knight,2=Bishop,3=Rook,4=Queen)
constexpr int PIECE_TO_NNUE_IDX[6] = {
    0,   // PAWNS   → 0
    3,   // ROOKS   → 3
    1,   // KNIGHTS → 1
    2,   // BISHOPS → 2
    4,   // QUEEN   → 4
    -1,  // KING    → not used
};

constexpr int PROMOTION_ENUM_TO_NNUE_PIECE_IDX[4] = {
    3,   // PRBISHOPS → 2
    2,   // PRKNIGHT  → 1
    1,   // PRROOK    → 3
    4,   // PRQUEEN   → 4
};


#endif