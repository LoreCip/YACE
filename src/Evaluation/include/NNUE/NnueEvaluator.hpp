#ifndef NNUE_EVALUATOR_HPP
#define NNUE_EVALUATOR_HPP

#include <string>

#include "IEvaluator.hpp"
#include "NnueNetwork.hpp"
#include "NnueConstants.hpp"
#include "Types.hpp"


struct NnueState {
    int16_t accumulator[2][ACC_SIZE];
    bool computed[2];
};

class NnueEvaluator : public IEvaluator {
private:
    bool eager;
    NnueState nnueStack[MAX_HISTORY_PLY];
    int currentPly;
    NnueNetwork network;

    int FlipSquareVertical(int square) const;
    void RefreshAccumulator(const Board& board, int perspective);
    void IncrementalUpdate(const Board& board, Move move, int perspective);
    int MakeFeatureIndex(int square, PieceType pieceType, int pieceColor, int kingSq, int perspective) const;

public:
    NnueEvaluator();
    ~NnueEvaluator() override = default;

    // Carica i pesi dal disco
    bool Initialize(const std::string& weightsPath);
    
    // Inizializza lo stack per una nuova posizione/partita
    void Reset(const Board& board) override; 

    // --- Implementazione dell'interfaccia IEvaluator ---
    int Evaluate(const Board& board) override;
    void OnMakeMove(const Board& board, Move move) override;
    void OnUnmakeMove() override;

    void SetEager(bool var) { eager = var; }
    bool GetEager() const { return eager; }
};

#endif