#include "NnueEvaluator.hpp"
#include "NnueConstants.hpp"
#include "Types.hpp"

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
    
    double *smt, *nsmt;
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

void NnueEvaluator::RefreshAccumulator(const Board& board, int perspective) {
    double* acc = nnueStack[currentPly].accumulator[perspective];

    for (int i = 0; i < M; i++) {
        acc[i] = network.L0Bias[i]; 
    }

    uint64_t kingBb = board.GetBitBoard(perspective, PieceType::KING);
    if (kingBb == 0) {
        nnueStack[currentPly].computed[perspective] = true;
        return;
    }
    
    int kingSq = __builtin_ctzll(kingBb);

    for (int p = 0; p < 6; p++) {
        PieceType pt = static_cast<PieceType>(p);
        if (pt == PieceType::KING || pt == PieceType::NONE) continue;

        for (int color = 0; color <= 1; color++) {
            uint64_t bb = board.GetBitBoard(color, pt);

            while (bb != 0) {
                int pieceSq = __builtin_ctzll(bb);
                bb &= bb - 1; // Rimuove il LSB
                
                int fIdx = MakeFeatureIndex(pieceSq, pt, color, kingSq, perspective);
                for (int i = 0; i < M; i++) {
                    acc[i] += network.L0[fIdx][i];
                }
            }
        }
    }

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
    double* acc = nnueStack[currentPly].accumulator[perspective];

    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);
    Color us = (board.GetSideToMove() == Color::WHITE) ? !board.GetSideToMove() : board.GetSideToMove();
    Color them = !us;

    int boardHistoryIdx = board.GetHistoryPly() - 1;
    PieceType movedPiece = static_cast<PieceType>(board.GetHistory(boardHistoryIdx).movedPiece);
    PieceType capturedPiece = static_cast<PieceType>(board.GetHistory(boardHistoryIdx).capturedPiece);

    uint64_t king_bb = board.GetBitBoard(perspective, PieceType::KING);
    int king_sq = __builtin_ctzll(king_bb);

    int removedFeatures[3];
    int addedFeatures[3];
    int removedCount = 0;
    int addedCount = 0;

    removedFeatures[removedCount++] = MakeFeatureIndex(from, movedPiece, ColorInt(us), king_sq, perspective);

    if (capturedPiece != PieceType::NONE) {
        int actualCaptureSq = to;
        if (flags == FlagMap::ENPASS) {
            actualCaptureSq = (us == ColorInt(Color::WHITE)) ? to - 8 : to + 8;
        }
        removedFeatures[removedCount++] = MakeFeatureIndex(actualCaptureSq, capturedPiece, ColorInt(them), king_sq, perspective);
    }

    // Identifica la promozione
    PieceType promoPiece = PieceType::NONE;
    if (flags == FlagMap::PRQUEEN || flags == FlagMap::PRCAPQUEEN) promoPiece = PieceType::QUEEN;
    else if (flags == FlagMap::PRROOK || flags == FlagMap::PRCAPROOK) promoPiece = PieceType::ROOK;
    else if (flags == FlagMap::PRBISHOP || flags == FlagMap::PRCAPBISHOP) promoPiece = PieceType::BISHOP;
    else if (flags == FlagMap::PRKNIGHT || flags == FlagMap::PRCAPKNIGHT) promoPiece = PieceType::KNIGHT;
    
    if (promoPiece != PieceType::NONE) {
        addedFeatures[addedCount++] = MakeFeatureIndex(to, promoPiece, ColorInt(us), king_sq, perspective);
    } else {
        addedFeatures[addedCount++] = MakeFeatureIndex(to, movedPiece, ColorInt(us), king_sq, perspective);
    }

    // Gestione Arrocco per le Torri
    if (flags == FlagMap::CASTLING) {
        int rookFrom = 0;
        int rookTo = 0;
        
        if (to == 6)        { rookFrom = 7;  rookTo = 5;  } 
        else if (to == 2)   { rookFrom = 0;  rookTo = 3;  } 
        else if (to == 62)  { rookFrom = 63; rookTo = 61; } 
        else if (to == 58)  { rookFrom = 56; rookTo = 59; } 
        
        removedFeatures[removedCount++] = MakeFeatureIndex(rookFrom, PieceType::ROOK, ColorInt(us), king_sq, perspective);
        addedFeatures[addedCount++] = MakeFeatureIndex(rookTo, PieceType::ROOK, ColorInt(us), king_sq, perspective);
    }

    for (int i = 0; i < removedCount; i++) {
        int fIdx = removedFeatures[i];
        for (int j = 0; j < M; j++) {
            acc[j] -= network.L0[fIdx][j];
        }
    }

    for (int i = 0; i < addedCount; i++) {
        int fIdx = addedFeatures[i];
        for (int j = 0; j < M; j++) {
            acc[j] += network.L0[fIdx][j];
        }
    }
}

int NnueEvaluator::FlipSquareVertical(int square) const {
    return square ^ 56;
}