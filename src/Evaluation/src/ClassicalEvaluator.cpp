#include "ClassicalEvaluator.hpp"
#include "Evaluation.hpp"
#include "Types.hpp"

int ClassicalEvaluator::Evaluate(const Board& board) {
    int scores[2] = {0, 0};

    for (int color = 0; color < 2; color++) {
        for (const auto piece : Pieces::All) {
            if (piece == PieceType::NONE) continue;

            uint64_t bitboard = board.GetBitBoard(color, piece);
            
            scores[color] += __builtin_popcountll(bitboard) * Evaluation::pieceValues[PieceInt(piece)];
            
            while (bitboard) {
                int sq = __builtin_ctzll(bitboard);
                int pstSquare = (color == static_cast<int>(Color::BLACK)) ? (sq ^ 56) : sq; 
                scores[color] += Evaluation::pstTables[PieceInt(piece)][pstSquare];
                
                bitboard &= bitboard - 1;
            }
        }
        
        if (__builtin_popcountll(board.GetBitBoard(color, PieceType::BISHOP)) >= 2) {
            scores[color] += 50;
        }
    }

    int stm = ColorInt(board.GetSideToMove());
    return scores[stm] - scores[stm ^ 1];
}