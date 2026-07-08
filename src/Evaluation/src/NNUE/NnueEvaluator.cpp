#include "NnueEvaluator.hpp"
#include "NnueConstants.hpp"
#include "Types.hpp"
#include "Move.hpp"
#include <immintrin.h>   // AVX-512 intrinsics

// ============================================================================
// Richiede compilazione con -mavx512f -mavx512bw (stesso requisito di
// NnueNetwork.cpp, che in più usa -mavx512dq -mavx512vl -mavx512vnni).
// ============================================================================

NnueEvaluator::NnueEvaluator() : eager(true), currentPly(0) {
    // Inizializzazione pulita
    for (int i = 0; i < MAX_HISTORY_PLY; i++) {
        nnueStack[i].computed[0] = false;
        nnueStack[i].computed[1] = false;
    }
}

bool NnueEvaluator::Initialize(const std::string& weightsPath) {
    return network.InitializeFromFile(weightsPath); 
}

void NnueEvaluator::Reset(const Board& board) {
    currentPly = 0;
    nnueStack[0].computed[ColorInt(Color::WHITE)] = false;
    nnueStack[0].computed[ColorInt(Color::BLACK)] = false;
    RefreshAccumulator(board, ColorInt(Color::WHITE));
    RefreshAccumulator(board, ColorInt(Color::BLACK));
}

int NnueEvaluator::Evaluate(const Board& board) {
    if (!eager) {
        if (!nnueStack[currentPly].computed[ColorInt(Color::WHITE)]) 
            RefreshAccumulator(board, ColorInt(Color::WHITE));
        if (!nnueStack[currentPly].computed[ColorInt(Color::BLACK)]) 
            RefreshAccumulator(board, ColorInt(Color::BLACK));
    }
    
    int16_t *smt, *nsmt;
    if (board.GetSideToMove() == ColorInt(Color::WHITE)) {
        smt = nnueStack[currentPly].accumulator[ColorInt(Color::WHITE)];
        nsmt = nnueStack[currentPly].accumulator[ColorInt(Color::BLACK)];
    } else {
        smt = nnueStack[currentPly].accumulator[ColorInt(Color::BLACK)];
        nsmt = nnueStack[currentPly].accumulator[ColorInt(Color::WHITE)];
    }
    
    return network.EvaluateNnue(smt, nsmt);
}

void NnueEvaluator::OnMakeMove(const Board& board, Move move) {
    int prevPly = currentPly;
    currentPly++;
    
    if (currentPly >= MAX_HISTORY_PLY) return;

    nnueStack[currentPly] = nnueStack[prevPly];
    
    int movedColor = (board.GetSideToMove() == ColorInt(Color::WHITE)) 
                     ? ColorInt(Color::BLACK) 
                     : ColorInt(Color::WHITE);
                     
    int boardHistoryIdx = board.GetHistoryPly() - 1;
    
    // Se è stato mosso il re, tutta la prospettiva cambia e dobbiamo ricalcolare da zero
    if (board.GetHistory(boardHistoryIdx).movedPiece == PieceType::KING) {
        nnueStack[currentPly].computed[movedColor] = false;
    }

    // Aggiornamento prospettiva BIANCO
    if (nnueStack[currentPly].computed[ColorInt(Color::WHITE)]) {
        IncrementalUpdate(board, move, ColorInt(Color::WHITE));
    } else if (eager) {
        RefreshAccumulator(board, ColorInt(Color::WHITE));
    }

    // Aggiornamento prospettiva NERO
    if (nnueStack[currentPly].computed[ColorInt(Color::BLACK)]) {
        IncrementalUpdate(board, move, ColorInt(Color::BLACK));
    } else if (eager) {
        RefreshAccumulator(board, ColorInt(Color::BLACK));
    }
}

void NnueEvaluator::OnUnmakeMove() {
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
    int i = 0;
    for (; i + 32 <= M; i += 32) {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(base + i));
        for (int f = 0; f < count; f++) {
            v = _mm512_add_epi16(v, _mm512_loadu_si512(reinterpret_cast<const void*>(cols[f] + i)));
        }
        _mm512_storeu_si512(reinterpret_cast<void*>(acc + i), v);
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
        for (int i = 0; i < M; i++) acc[i] = network.L0Bias[i];
        nnueStack[currentPly].computed[perspective] = true;
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
                    activeCols[activeCount++] = network.L0[fIdx];
                }
            }
        }
    }

    // Passo 2: un solo passaggio vettoriale bias + tutte le colonne
    AccumulateColumnsInto(acc, network.L0Bias, activeCols, activeCount);

    nnueStack[currentPly].computed[perspective] = true;
}

int NnueEvaluator::MakeFeatureIndex(int square, PieceType pieceType, int pieceColor, int kingSq, int perspective) const {
    int finalSquare = (perspective == ColorInt(Color::BLACK)) ? FlipSquareVertical(square) : square;
    int finalKingSq = (perspective == ColorInt(Color::BLACK)) ? FlipSquareVertical(kingSq) : kingSq;
    
    int nnuePt = PIECE_TO_NNUE_IDX[PieceInt(pieceType)];
    int relativeColor = (pieceColor != perspective) ? 1 : 0;
    int pIdx = nnuePt * 2 + relativeColor;
    
    return finalSquare + (pIdx + finalKingSq * 10) * 64; 
}

void NnueEvaluator::IncrementalUpdate(const Board& board, Move move, int perspective) {
    int16_t* acc = nnueStack[currentPly].accumulator[perspective];

    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);
    Color us = (board.GetSideToMove() == Color::WHITE) ? Color::BLACK : Color::WHITE;
    Color them = !us;

    int boardHistoryIdx = board.GetHistoryPly() - 1;
    PieceType movedPiece = static_cast<PieceType>(board.GetHistory(boardHistoryIdx).movedPiece);
    PieceType capturedPiece = static_cast<PieceType>(board.GetHistory(boardHistoryIdx).capturedPiece);

    uint64_t king_bb = board.GetBitBoard(perspective, PieceType::KING);
    int king_sq = __builtin_ctzll(king_bb);

    const int16_t* removedCols[3];
    const int16_t* addedCols[3];
    int removedCount = 0;
    int addedCount = 0;

    removedCols[removedCount++] = network.L0[MakeFeatureIndex(from, movedPiece, ColorInt(us), king_sq, perspective)];

    if (capturedPiece != PieceType::NONE) {
        int actualCaptureSq = to;
        if (flags == FlagMap::ENPASS) {
            actualCaptureSq = (us == ColorInt(Color::WHITE)) ? to - 8 : to + 8;
        }
        removedCols[removedCount++] = network.L0[MakeFeatureIndex(actualCaptureSq, capturedPiece, ColorInt(them), king_sq, perspective)];
    }

    // Identifica la promozione
    PieceType promoPiece = getMovePromotionPiece(flags);
    
    if (promoPiece != PieceType::NONE) {
        addedCols[addedCount++] = network.L0[MakeFeatureIndex(to, promoPiece, ColorInt(us), king_sq, perspective)];
    } else {
        addedCols[addedCount++] = network.L0[MakeFeatureIndex(to, movedPiece, ColorInt(us), king_sq, perspective)];
    }

    // Gestione Arrocco per le Torri
    if (flags == FlagMap::CASTLING) {
        int rookFrom = 0;
        int rookTo = 0;
        
        if (to == 6)        { rookFrom = 7;  rookTo = 5;  } 
        else if (to == 2)   { rookFrom = 0;  rookTo = 3;  } 
        else if (to == 62)  { rookFrom = 63; rookTo = 61; } 
        else if (to == 58)  { rookFrom = 56; rookTo = 59; } 
        
        removedCols[removedCount++] = network.L0[MakeFeatureIndex(rookFrom, PieceType::ROOK, ColorInt(us), king_sq, perspective)];
        addedCols[addedCount++] = network.L0[MakeFeatureIndex(rookTo, PieceType::ROOK, ColorInt(us), king_sq, perspective)];
    }

    // Un solo passaggio vettoriale: sottrai tutte le rimosse, poi somma tutte le aggiunte
    int i = 0;
    for (; i + 32 <= M; i += 32) {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(acc + i));
        for (int r = 0; r < removedCount; r++)
            v = _mm512_sub_epi16(v, _mm512_loadu_si512(reinterpret_cast<const void*>(removedCols[r] + i)));
        for (int a = 0; a < addedCount; a++)
            v = _mm512_add_epi16(v, _mm512_loadu_si512(reinterpret_cast<const void*>(addedCols[a] + i)));
        _mm512_storeu_si512(reinterpret_cast<void*>(acc + i), v);
    }
    for (; i < M; i++) {
        int32_t s = acc[i];
        for (int r = 0; r < removedCount; r++) s -= removedCols[r][i];
        for (int a = 0; a < addedCount; a++) s += addedCols[a][i];
        acc[i] = static_cast<int16_t>(s);
    }
}

int NnueEvaluator::FlipSquareVertical(int square) const {
    return square ^ 56;
}