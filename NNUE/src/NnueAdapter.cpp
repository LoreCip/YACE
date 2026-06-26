#include "NnueAdapter.hpp"
#include "Constants.hpp"
#include "Move.hpp"
#include "NnueNetwork.hpp"
#include "Pieces.hpp"

#ifdef _DEBUG
#include <assert.h>
#endif


NnueState NnueAdapter::nnueStack[MAX_HISTORY_PLY];
int NnueAdapter::currentPly = 0;
NnueNetwork NnueAdapter::network;
bool NnueAdapter::eager = true;
bool NnueAdapter::disableForPerft = true;


bool NnueAdapter::Initialize(const std::string& weightsPath) {
    return network.InitializeFromFile(weightsPath); 
}

void NnueAdapter::Reset(Board& board) {
    currentPly = 0;
    nnueStack[0].computed[WHITE] = false;
    nnueStack[0].computed[BLACK] = false;
    RefreshAccumulator(board, WHITE);
    RefreshAccumulator(board, BLACK);
}

void NnueAdapter::OnMakeMove(Board& board, Move move) {
    if (disableForPerft) return;

    int prevPly = currentPly;
    currentPly++;
    
    if (currentPly >= MAX_HISTORY_PLY) {
        return;
    }

    nnueStack[currentPly] = nnueStack[prevPly];
    int movedColor = board.GetSideToMove();
    int boardHistoryIdx = board.GetHistoryPly() - 1;
    if (board.GetHistory(boardHistoryIdx).movedPiece == PiecesEnum::KING) {
        nnueStack[currentPly].computed[movedColor] = false;
    }

    if (nnueStack[currentPly].computed[WHITE]) {
        IncrementalUpdate(board, move, WHITE);
    } else if (eager){
        RefreshAccumulator(board, WHITE);
    }

    if (nnueStack[currentPly].computed[BLACK]) {
        IncrementalUpdate(board, move, BLACK);
    } else if (eager){
        RefreshAccumulator(board, BLACK);
    }
}

void NnueAdapter::OnUnmakeMove() {
    currentPly--;
    if (currentPly < 0) {
        currentPly = 0;
    }
}

int NnueAdapter::NnueEvaluate(Board& board) {
    if (!eager) {
        if (!nnueStack[currentPly].computed[WHITE]) RefreshAccumulator(board, WHITE);
        if (!nnueStack[currentPly].computed[BLACK]) RefreshAccumulator(board, BLACK);
    }
    
    double *smt, *nsmt;
    if (board.GetSideToMove() == WHITE) {
        smt = nnueStack[currentPly].accumulator[WHITE];
        nsmt = nnueStack[currentPly].accumulator[BLACK];
    } else {
        smt = nnueStack[currentPly].accumulator[BLACK];
        nsmt = nnueStack[currentPly].accumulator[WHITE];
    }
    
    return network.EvaluateNnue(smt, nsmt);
}

void NnueAdapter::RefreshAccumulator(Board& board, int perspective) {
    double* acc = nnueStack[currentPly].accumulator[perspective];

    for (int i = 0; i < M; i++) {
        acc[i] = network.L0Bias[i]; 
    }

    uint64_t kingBb = board.GetBitBoard(perspective, PiecesEnum::KING);
    if (kingBb == 0) {
        nnueStack[currentPly].computed[perspective] = true;
        return;
    }
    
    int kingSq = __builtin_ctzll(kingBb);
    if (perspective == BLACK) kingSq = FlipSquareVertical(kingSq);

    for (PiecesEnum::Type pt : PiecesEnum::All) {
        if (pt == PiecesEnum::KING) continue;

        for (int color = WHITE; color <= BLACK; color++) {
            uint64_t bb = board.GetBitBoard(color, pt);

            while (bb != 0) {
                int pieceSq = pop_lsb(bb); 
                int fIdx = MakeFeatureIndex(pieceSq, pt, color, kingSq, perspective);
                for (int i = 0; i < M; i++) {
                    acc[i] += network.L0[fIdx][i];
                }
            }
        }
    }

    // 7. Sanciamo che l'accumulatore per questa prospettiva al ply corrente è valido e aggiornato
    nnueStack[currentPly].computed[perspective] = true;
}
int NnueAdapter::MakeFeatureIndex(int square, PiecesEnum::Type pieceType, int pieceColor, int kingSq, int perspective) {
    int finalSquare = (perspective == BLACK) ? FlipSquareVertical(square) : square;
    int nnuePt = PIECE_TO_NNUE_IDX[pieceType];
    int relativeColor = (pieceColor != perspective) ? 1 : 0;
    int pIdx = nnuePt * 2 + relativeColor;
    return finalSquare + (pIdx + kingSq * 10) * 64; 
}

void NnueAdapter::IncrementalUpdate(Board& board, Move move, int perspective) {
    double* acc = nnueStack[currentPly].accumulator[perspective];

    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);
    int us = board.GetSideToMove();
    int them = (us == WHITE) ? BLACK : WHITE;

    int boardHistoryIdx = board.GetHistoryPly() - 1;
    auto movedPiece = board.GetHistory(boardHistoryIdx).movedPiece;
    auto capturedPiece = board.GetHistory(boardHistoryIdx).capturedPiece;

    // Ricaviamo la casa del re per questa prospettiva (flippata all'origine se BLACK)
    uint64_t king_bb = board.GetBitBoard(perspective, PiecesEnum::KING);
    int king_sq = __builtin_ctzll(king_bb);
    if (perspective == BLACK) king_sq = FlipSquareVertical(king_sq);

    int removedFeatures[2];
    int addedFeatures[2];
    int removedCount = 0;
    int addedCount = 0;


    removedFeatures[removedCount++] = MakeFeatureIndex(from, movedPiece, us, king_sq, perspective);

    if (capturedPiece != PiecesEnum::NONE) {
        int actualCaptureSq = to;
        if (flags == FlagMap::ENPASS) {
            // Sincronizzato con il tuo MakeMove: il pedone catturato è dietro
            actualCaptureSq = (us == WHITE) ? to - 8 : to + 8;
        }
        removedFeatures[removedCount++] = MakeFeatureIndex(actualCaptureSq, capturedPiece, them, king_sq, perspective);
    }

    PiecesEnum::Type promoPiece = board.GetPromotionPiece(flags); 
    
    if (promoPiece != PiecesEnum::NONE) {
        addedFeatures[addedCount++] = MakeFeatureIndex(to, promoPiece, us, king_sq, perspective);
    } else {
        addedFeatures[addedCount++] = MakeFeatureIndex(to, movedPiece, us, king_sq, perspective);
    }

    if (flags == FlagMap::CASTLING) {
        int rookFrom = 0;
        int rookTo = 0;
        
        if (to == 6)        { rookFrom = 7;  rookTo = 5;  } // Bianco Corto
        else if (to == 2)   { rookFrom = 0;  rookTo = 3;  } // Bianco Lungo
        else if (to == 62)  { rookFrom = 63; rookTo = 61; } // Nero Corto
        else if (to == 58)  { rookFrom = 56; rookTo = 59; } // Nero Lungo
        
        removedFeatures[removedCount++] = MakeFeatureIndex(rookFrom, PiecesEnum::ROOKS, us, king_sq, perspective);
        addedFeatures[addedCount++] = MakeFeatureIndex(rookTo, PiecesEnum::ROOKS, us, king_sq, perspective);
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

int NnueAdapter::FlipSquareVertical(int square) {
    return square ^ 56;
}