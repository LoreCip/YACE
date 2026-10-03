#ifndef NNUE_EVALUATOR_HPP
#define NNUE_EVALUATOR_HPP

#include <string>
#include <memory>
#include <cstdint>

#include "IEvaluator.hpp"
#include "NnueNetwork.hpp"
#include "NnueConstants.hpp"
#include "Types.hpp"


// Come passare dall'accumulatore del ply precedente a quello di questo ply: indici di feature
// (righe di L0) da sottrarre/aggiungere, per entrambe le prospettive. Calcolato in OnMakeMove,
// quando la board e' gia' nello stato dopo la mossa. Una mossa di re non genera eventi per la
// prospettiva propria (che fa sempre un refresh completo) e il re non e' mai una feature HalfKP.
struct AccDelta {
    uint32_t added[2][2];    // [prospettiva][slot]: mossa/promozione + torre d'arrocco
    uint32_t removed[2][3];  // [prospettiva][slot]: from + cattura + torre d'arrocco
    uint8_t nAdded = 0;
    uint8_t nRemoved = 0;
};

// alignas(64) sulla struct arrotonda sizeof a un multiplo di 64: cosi' ogni slot di nnueStack
// e' allineato e gli accumulatori si leggono/scrivono con load/store AVX-512 allineati.
struct alignas(64) NnueState {
    alignas(64) int16_t accumulator[2][M];  // [prospettiva][neurone]
    AccDelta delta;                         // trasforma l'accumulatore del ply precedente in questo
};

class NnueEvaluator : public IEvaluator {
private:
    NnueState nnueStack[MAX_HISTORY_PLY];
    int currentPly = 0;
    // OnMakeMove oltre MAX_HISTORY_PLY (in pratica irraggiungibile): si contano invece di far
    // uscire currentPly dall'array, e OnUnmakeMove li srotola per primi.
    int overflowDepth = 0;
    // Rete condivisa in sola lettura tra tutti i thread; `net` e' il puntatore grezzo usato sul
    // percorso caldo, `networkOwner` la tiene in vita.
    std::shared_ptr<const NnueNetwork> networkOwner;
    const NnueNetwork* net = nullptr;

    void RefreshAccumulator(const Board& board, int perspective);
    void BuildDelta(const Board& board, Move move, PieceType movedPiece, PieceType capturedPiece,
                    AccDelta& delta) const;
    // nnueStack[ply-1].accumulator[perspective] + nnueStack[ply].delta → nnueStack[ply].accumulator[perspective]
    void ApplyDelta(int perspective, int ply);
    int MakeFeatureIndex(int square, PieceType pieceType, int pieceColor, int kingSq, int perspective) const;

#ifdef NNUE_VALIDATE
    // Build di test: confronta ogni accumulatore incrementale con un refresh completo.
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