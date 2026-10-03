#include "NnueEvaluator.hpp"
#include "NnueConstants.hpp"
#include "Types.hpp"
#include "Move.hpp"
#include <immintrin.h>   // AVX-512 intrinsics
#include <cassert>
#include <cstring>
#include <cstdint>

// ============================================================================
// Richiede compilazione con -mavx512f -mavx512bw (stesso requisito di
// NnueNetwork.cpp, che in più usa -mavx512dq -mavx512vl -mavx512vnni).
// ============================================================================

NnueEvaluator::NnueEvaluator(std::shared_ptr<const NnueNetwork> sharedNetwork)
    : networkOwner(std::move(sharedNetwork)), net(networkOwner.get()) {}

void NnueEvaluator::Reset(const Board& board) {
    currentPly = 0;
    RefreshAccumulator(board, ColorInt(Color::WHITE));
    RefreshAccumulator(board, ColorInt(Color::BLACK));
}

#ifdef NNUE_VALIDATE
void NnueEvaluator::ValidateAccumulators(const Board& board) {
    for (int p = 0; p <= 1; p++) {
        int16_t saved[M];
        std::memcpy(saved, nnueStack[currentPly].accumulator[p], sizeof(saved));

        RefreshAccumulator(board, p); // sovrascrive nnueStack[currentPly].accumulator[p]

        bool match = (std::memcmp(saved, nnueStack[currentPly].accumulator[p], sizeof(saved)) == 0);

        // Ripristina l'accumulatore incrementale originale: la validazione deve essere pura
        // osservazione, la ricerca continua a usare esattamente cio' che avrebbe usato senza
        // NNUE_VALIDATE.
        std::memcpy(nnueStack[currentPly].accumulator[p], saved, sizeof(saved));

        assert(match && "NNUE_VALIDATE: accumulatore incrementale diverge dal refresh completo");
    }
}
#endif

int NnueEvaluator::Evaluate(const Board& board) {
#ifdef NNUE_VALIDATE
    ValidateAccumulators(board);
#endif

    // La rete valuta dal punto di vista di chi muove: il suo accumulatore va per primo
    int stm = ColorInt(board.GetSideToMove());
    return net->EvaluateNnue(nnueStack[currentPly].accumulator[stm], nnueStack[currentPly].accumulator[stm ^ 1]);
}

void NnueEvaluator::OnMakeMove(const Board& board, Move move) {
    if (currentPly + 1 >= MAX_HISTORY_PLY) {
        overflowDepth++;
        return;
    }
    currentPly++;

    // La board ha gia' eseguito la mossa: chi ha mosso e' ora l'avversario di chi muove
    int movedColor = ColorInt(!board.GetSideToMove());
    const UndoState& undo = board.GetHistory(board.GetHistoryPly() - 1);
    bool isKingMove = (undo.movedPiece == PieceType::KING);

    BuildDelta(board, move, undo.movedPiece, undo.capturedPiece, nnueStack[currentPly].delta);

    // HalfKP: se il re si muove cambiano tutte le feature della sua prospettiva → refresh completo
    for (int perspective = 0; perspective < NUM_COLORS; perspective++) {
        if (isKingMove && perspective == movedColor) RefreshAccumulator(board, perspective);
        else ApplyDelta(perspective, currentPly);
    }
}

void NnueEvaluator::OnUnmakeMove() {
    if (overflowDepth > 0) {
        overflowDepth--;
        return;
    }
    currentPly--;
    if (currentPly < 0) currentPly = 0;
}

// ---------------------------------------------------------------------------
// Somma vettoriale fusa: acc[0..M) = base[0..M) + sum_f cols[f][0..M)
// Un solo passaggio di lettura/scrittura su acc invece di uno per feature.
// M=256 è multiplo esatto di 16 (int16 per registro AVX2), ma la coda
// scalare resta per sicurezza/robustezza se M dovesse cambiare.
// ---------------------------------------------------------------------------
static inline void AccumulateColumnsInto(int16_t* acc, const int16_t* base, const int16_t* const* cols, int count) {
    assert((reinterpret_cast<uintptr_t>(acc) & 63) == 0);
    assert((reinterpret_cast<uintptr_t>(base) & 63) == 0);
    int i = 0;
    for (; i + 32 <= M; i += 32) {
        __m512i v = _mm512_load_si512(reinterpret_cast<const void*>(base + i));
        for (int f = 0; f < count; f++) {
            assert((reinterpret_cast<uintptr_t>(cols[f]) & 63) == 0);
            v = _mm512_add_epi16(v, _mm512_load_si512(reinterpret_cast<const void*>(cols[f] + i)));
        }
        _mm512_store_si512(reinterpret_cast<void*>(acc + i), v);
    }
    for (; i < M; i++) {
        int32_t s = base[i];
        for (int f = 0; f < count; f++) s += cols[f][i];
        acc[i] = static_cast<int16_t>(s);
    }
}

void NnueEvaluator::RefreshAccumulator(const Board& board, int perspective) {
    int16_t* acc = nnueStack[currentPly].accumulator[perspective];

    uint64_t kingBb = board.GetBitBoard(perspective, PieceType::KING);
    if (kingBb == 0) {
        for (int i = 0; i < M; i++) acc[i] = net->L0Bias[i];
        return;
    }
    
    int kingSq = __builtin_ctzll(kingBb);

    // Passo 1: raccogli tutte le feature attive (max teorico 30 pezzi non-re, vedi MAX_ACTIVE_FEATURES)
    const int16_t* activeCols[MAX_ACTIVE_FEATURES];
    int activeCount = 0;

    for (int p = 0; p < 6; p++) {
        PieceType pt = static_cast<PieceType>(p);
        if (pt == PieceType::KING || pt == PieceType::NONE) continue;

        for (int color = 0; color <= 1; color++) {
            uint64_t bb = board.GetBitBoard(color, pt);

            while (bb != 0) {
                int pieceSq = __builtin_ctzll(bb);
                bb &= bb - 1; // Rimuove il LSB
                
                int fIdx = MakeFeatureIndex(pieceSq, pt, color, kingSq, perspective);
                if (activeCount < MAX_ACTIVE_FEATURES) {
                    activeCols[activeCount++] = net->L0[fIdx];
                }
            }
        }
    }

    // Passo 2: un solo passaggio vettoriale bias + tutte le colonne
    AccumulateColumnsInto(acc, net->L0Bias, activeCols, activeCount);
}

int NnueEvaluator::MakeFeatureIndex(int square, PieceType pieceType, int pieceColor, int kingSq, int perspective) const {
    assert(pieceType != PieceType::KING && "MakeFeatureIndex chiamata con KING: il re non e' una feature HalfKP");

    // La prospettiva nera vede la scacchiera ribaltata verticalmente (sq ^ 56)
    int finalSquare = (perspective == ColorInt(Color::BLACK)) ? square ^ 56 : square;
    int finalKingSq = (perspective == ColorInt(Color::BLACK)) ? kingSq ^ 56 : kingSq;

    int nnuePt = PIECE_TO_NNUE_IDX[PieceInt(pieceType)];
    int relativeColor = (pieceColor != perspective) ? 1 : 0;
    int pIdx = nnuePt * 2 + relativeColor;
    int idx = finalSquare + (pIdx + finalKingSq * 10) * 64;

    assert(idx >= 0 && idx < NUM_FEATURES && "MakeFeatureIndex ha prodotto un indice fuori scala per L0");
    return idx;
}

void NnueEvaluator::BuildDelta(const Board& board, Move move, PieceType movedPiece, PieceType capturedPiece,
                                AccDelta& delta) const {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);
    Color us = !board.GetSideToMove();  // chi ha appena mosso
    Color them = !us;

    bool isKingMove = (movedPiece == PieceType::KING);
    PieceType promoPiece = promotionPieceFromFlags(flags);

    int actualCaptureSq = to;
    if (flags == FlagMap::ENPASS) {
        actualCaptureSq = (us == ColorInt(Color::WHITE)) ? to - 8 : to + 8;
    }

    int rookFrom = -1, rookTo = -1;
    if (flags == FlagMap::CASTLING) castlingRookSquares(to, rookFrom, rookTo);

    for (int perspective = 0; perspective <= 1; perspective++) {
        uint64_t king_bb = board.GetBitBoard(perspective, PieceType::KING);
        int king_sq = __builtin_ctzll(king_bb);

        int rCount = 0, aCount = 0;

        if (!isKingMove) {
            delta.removed[perspective][rCount++] =
                static_cast<uint32_t>(MakeFeatureIndex(from, movedPiece, ColorInt(us), king_sq, perspective));
        }
        if (capturedPiece != PieceType::NONE) {
            delta.removed[perspective][rCount++] =
                static_cast<uint32_t>(MakeFeatureIndex(actualCaptureSq, capturedPiece, ColorInt(them), king_sq, perspective));
        }
        if (!isKingMove) {
            PieceType addPiece = (promoPiece != PieceType::NONE) ? promoPiece : movedPiece;
            delta.added[perspective][aCount++] =
                static_cast<uint32_t>(MakeFeatureIndex(to, addPiece, ColorInt(us), king_sq, perspective));
        }
        if (flags == FlagMap::CASTLING) {
            delta.removed[perspective][rCount++] =
                static_cast<uint32_t>(MakeFeatureIndex(rookFrom, PieceType::ROOK, ColorInt(us), king_sq, perspective));
            delta.added[perspective][aCount++] =
                static_cast<uint32_t>(MakeFeatureIndex(rookTo, PieceType::ROOK, ColorInt(us), king_sq, perspective));
        }

        delta.nRemoved = static_cast<uint8_t>(rCount);
        delta.nAdded = static_cast<uint8_t>(aCount);
    }
}

void NnueEvaluator::ApplyDelta(int perspective, int ply) {
    const int16_t* src = nnueStack[ply - 1].accumulator[perspective];
    int16_t* dst = nnueStack[ply].accumulator[perspective];
    const AccDelta& d = nnueStack[ply].delta;

    const int16_t* removedCols[3];
    const int16_t* addedCols[2];
    for (int r = 0; r < d.nRemoved; r++) removedCols[r] = net->L0[d.removed[perspective][r]];
    for (int a = 0; a < d.nAdded; a++) addedCols[a] = net->L0[d.added[perspective][a]];

    assert((reinterpret_cast<uintptr_t>(src) & 63) == 0);
    assert((reinterpret_cast<uintptr_t>(dst) & 63) == 0);
    int i = 0;
    for (; i + 32 <= M; i += 32) {
        __m512i v = _mm512_load_si512(reinterpret_cast<const void*>(src + i));
        for (int r = 0; r < d.nRemoved; r++) {
            assert((reinterpret_cast<uintptr_t>(removedCols[r]) & 63) == 0);
            v = _mm512_sub_epi16(v, _mm512_load_si512(reinterpret_cast<const void*>(removedCols[r] + i)));
        }
        for (int a = 0; a < d.nAdded; a++) {
            assert((reinterpret_cast<uintptr_t>(addedCols[a]) & 63) == 0);
            v = _mm512_add_epi16(v, _mm512_load_si512(reinterpret_cast<const void*>(addedCols[a] + i)));
        }
        _mm512_store_si512(reinterpret_cast<void*>(dst + i), v);
    }
    for (; i < M; i++) {
        int32_t s = src[i];
        for (int r = 0; r < d.nRemoved; r++) s -= removedCols[r][i];
        for (int a = 0; a < d.nAdded; a++) s += addedCols[a][i];
        dst[i] = static_cast<int16_t>(s);
    }
}

