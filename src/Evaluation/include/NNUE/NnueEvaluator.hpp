#ifndef NNUE_EVALUATOR_HPP
#define NNUE_EVALUATOR_HPP

#include <string>
#include <memory>
#include <cstdint>

#include "IEvaluator.hpp"
#include "NnueNetwork.hpp"
#include "NnueConstants.hpp"
#include "Types.hpp"


// Feature (righe di L0) da togliere/aggiungere all'accumulatore del ply precedente, per prospettiva.
// Una mossa di re non ne genera per la propria prospettiva, che viene ricalcolata da zero.
struct AccDelta {
    uint32_t added[2][2];    // [prospettiva][slot]: mossa/promozione + torre d'arrocco
    uint32_t removed[2][3];  // [prospettiva][slot]: from + cattura + torre d'arrocco
    uint8_t nAdded = 0;
    uint8_t nRemoved = 0;
};

// alignas(64): ogni slot di nnueStack resta allineato per load/store AVX-512.
struct alignas(64) NnueState {
    alignas(64) int16_t accumulator[2][M];  // [prospettiva][neurone]
    AccDelta delta;                         // trasforma l'accumulatore del ply precedente in questo
};

class NnueEvaluator : public IEvaluator {
private:
    NnueState nnueStack[MAX_HISTORY_PLY];
    int currentPly = 0;
    // Plies oltre MAX_HISTORY_PLY (in pratica mai): contati invece di uscire dall'array.
    int overflowDepth = 0;
    // `networkOwner` tiene in vita la rete condivisa, `net` la usa sul percorso caldo.
    std::shared_ptr<const NnueNetwork> networkOwner;
    const NnueNetwork* net = nullptr;

    void RefreshAccumulator(const Board& board, int perspective);
    void BuildDelta(const Board& board, Move move, PieceType movedPiece, PieceType capturedPiece,
                    AccDelta& delta) const;
    // nnueStack[ply-1].accumulator[perspective] + nnueStack[ply].delta → nnueStack[ply].accumulator[perspective]
    void ApplyDelta(int perspective, int ply);
    int MakeFeatureIndex(int square, PieceType pieceType, int pieceColor, int kingSq, int perspective) const;

#ifdef NNUE_VALIDATE
    // Build di test: accumulatore incrementale == refresh completo?
    void ValidateAccumulators(const Board& board);
#endif

public:
    explicit NnueEvaluator(std::shared_ptr<const NnueNetwork> sharedNetwork);

    void Reset(const Board& board) override;
    int Evaluate(const Board& board) override;
    void OnMakeMove(const Board& board, Move move) override;
    void OnUnmakeMove() override;
};

#endif