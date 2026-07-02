#include "MoveGenerator.hpp"
#include "LookupTables.hpp"
#include "BitOperations.hpp"
#include "Board.hpp"
#include "Types.hpp"

namespace MoveGen {

    int GenerateAllMoves(const Board& board, MoveList& moveList) {
        Color us = board.GetSideToMove();
        
        GenerateKnightMoves(board, moveList, us);
        GenerateKingMoves(board, moveList, us);
        GenerateSlidingMoves(board, moveList, us, PieceType::BISHOP);
        GenerateSlidingMoves(board, moveList, us, PieceType::ROOK);
        GenerateSlidingMoves(board, moveList, us, PieceType::QUEEN);
        GeneratePawnMoves(board, moveList, us);
        return moveList.count;
    }

    void GenerateKnightMoves(const Board& board, MoveList& moveList, Color us) {
        uint64_t bitboard = board.GetBitBoard(us, PieceType::KNIGHT);
        uint64_t allies = board.GetColorOccupation(us);
        uint64_t enemies = board.GetColorOccupation(!us);

        while (bitboard != 0ULL) {
            int from = __builtin_ctzll(bitboard);
            // Calcola gli attacchi validi escludendo le case occupate da alleati
            uint64_t attacks = LookupTables::knightAttacks[from] & (~allies);
            
            while (attacks != 0ULL) {
                int to = __builtin_ctzll(attacks);
                int flag = (setBit(0ULL, to) & enemies) ? FlagMap::CAPTURE : FlagMap::MOVE;
                moveList.push(createMove(from, to, flag));
                attacks = clearBit(attacks, to);
            }
            bitboard = clearBit(bitboard, from);
        }
    }

    void GenerateKingMoves(const Board& board, MoveList& moveList, Color us) {
        uint64_t bitboard = board.GetBitBoard(us, PieceType::KING);
        uint64_t allies = board.GetColorOccupation(us);
        uint64_t enemies = board.GetColorOccupation(!us);

        if (bitboard == 0ULL) return;
        int from = __builtin_ctzll(bitboard);
        
        // Normal moves
        uint64_t attacks = LookupTables::kingAttacks[from] & (~allies);
        while (attacks != 0ULL) {
            int to = __builtin_ctzll(attacks);
            int flag = (setBit(0ULL, to) & enemies) ? FlagMap::CAPTURE : FlagMap::MOVE;
            moveList.push(createMove(from, to, flag));
            attacks = clearBit(attacks, to);
        }

        // Castling
        uint8_t castlingRights = board.GetCastlingRights();
        uint64_t totalOcc = board.GetTotalOccupation();

        auto canCastle = [&](uint64_t emptyMask, int transit1, int transit2, int destSq) {
            if (!(totalOcc & emptyMask)) {
                if (!const_cast<Board&>(board).IsSquareAttacked(transit1, !us) && 
                    !const_cast<Board&>(board).IsSquareAttacked(transit2, !us)) {
                    moveList.push(createMove(from, destSq, FlagMap::CASTLING));
                }
            }
        };

        if (!const_cast<Board&>(board).IsSquareAttacked(from, !us)) {
            if (us == Color::WHITE) {
                if (castlingRights & CastlingRights::WK) canCastle(LookupTables::maskWK, 5, 6, 6);
                if (castlingRights & CastlingRights::WQ) canCastle(LookupTables::maskWQ, 3, 2, 2);
            } else {
                if (castlingRights & CastlingRights::BK) canCastle(LookupTables::maskBK, 61, 62, 62);
                if (castlingRights & CastlingRights::BQ) canCastle(LookupTables::maskBQ, 59, 58, 58);
            }
        }
    }

    void GenerateSlidingMoves(const Board& board, MoveList& moveList, Color us, PieceType piece) {
        uint64_t bitboard = board.GetBitBoard(us, piece);
        uint64_t allies = board.GetColorOccupation(us);
        uint64_t enemies = board.GetColorOccupation(!us);
        uint64_t totalOcc = board.GetTotalOccupation();

        while (bitboard != 0ULL) {
            int from = __builtin_ctzll(bitboard);
            uint64_t attacks = 0ULL;

            // Generazione dei raggi combinata
            if (piece == PieceType::BISHOP || piece == PieceType::QUEEN) {
                attacks |= ComputeRay(9, LookupTables::notColumnH, from, totalOcc);
                attacks |= ComputeRay(7, LookupTables::notColumnA, from, totalOcc);
                attacks |= ComputeRay(-9, LookupTables::notColumnA, from, totalOcc);
                attacks |= ComputeRay(-7, LookupTables::notColumnH, from, totalOcc);
            }
            if (piece == PieceType::ROOK || piece == PieceType::QUEEN) {
                attacks |= ComputeRay(-1, LookupTables::notColumnA, from, totalOcc);
                attacks |= ComputeRay(1, LookupTables::notColumnH, from, totalOcc);
                attacks |= ComputeRay(8, LookupTables::nullEdge, from, totalOcc);
                attacks |= ComputeRay(-8, LookupTables::nullEdge, from, totalOcc);
            }

            attacks &= (~allies);

            while (attacks != 0ULL) {
                int to = __builtin_ctzll(attacks);
                int flag = (setBit(0ULL, to) & enemies) ? FlagMap::CAPTURE : FlagMap::MOVE;
                moveList.push(createMove(from, to, flag));
                attacks = clearBit(attacks, to);
            }
            bitboard = clearBit(bitboard, from);
        }
    }

    void GeneratePawnMoves(const Board& board, MoveList& moveList, Color us) {
        
        uint64_t myPawns = board.GetBitBoard(us, PieceType::PAWN);
        uint64_t emptySquares = board.GetFreeCells();
        uint64_t enemyOccupation = board.GetColorOccupation(!us);
        
        int direction = (us == Color::WHITE) ? 8 : -8;
        int startRankStart = (us == Color::WHITE) ? 8  : 48;
        int startRankEnd   = (us == Color::WHITE) ? 15 : 55;
        int promoRankStart = (us == Color::WHITE) ? 56 : 0; 
        int promoRankEnd   = (us == Color::WHITE) ? 63 : 7; 

        while (myPawns != 0ULL) {
            int from = __builtin_ctzll(myPawns);
            int toSingle = from + direction;
            
            // Spinta singola e doppia
            if (setBit(0ULL, toSingle) & emptySquares) {
                if (toSingle >= promoRankStart && toSingle <= promoRankEnd) {
                    moveList.push(createMove(from, toSingle, FlagMap::PRQUEEN));
                    moveList.push(createMove(from, toSingle, FlagMap::PRROOK));
                    moveList.push(createMove(from, toSingle, FlagMap::PRBISHOP));
                    moveList.push(createMove(from, toSingle, FlagMap::PRKNIGHT));
                } else {
                    moveList.push(createMove(from, toSingle, FlagMap::MOVE));
                    if (from >= startRankStart && from <= startRankEnd) {
                        int toDouble = from + (direction * 2);
                        if (setBit(0ULL, toDouble) & emptySquares) {
                            moveList.push(createMove(from, toDouble, FlagMap::DMOVE));
                        }
                    }
                }
            }

            // Catture normali
            uint64_t targets = LookupTables::pawnAttacks[ColorInt(us)][from] & enemyOccupation;
            while (targets != 0ULL) {
                int toCapture = __builtin_ctzll(targets);
                if (toCapture >= promoRankStart && toCapture <= promoRankEnd) {
                    moveList.push(createMove(from, toCapture, FlagMap::PRCAPQUEEN));
                    moveList.push(createMove(from, toCapture, FlagMap::PRCAPROOK));
                    moveList.push(createMove(from, toCapture, FlagMap::PRCAPBISHOP));
                    moveList.push(createMove(from, toCapture, FlagMap::PRCAPKNIGHT));
                } else {
                    moveList.push(createMove(from, toCapture, FlagMap::CAPTURE));
                }
                targets = clearBit(targets, toCapture);
            }

            // En Passant
            int epSquare = board.GetEnPassantSquare();
            if (epSquare != 64) {
                uint64_t epBB = 1ULL << epSquare;
                if (LookupTables::pawnAttacks[ColorInt(us)][from] & epBB) {
                    moveList.push(createMove(from, epSquare, FlagMap::ENPASS));
                }
            }
            myPawns = clearBit(myPawns, from);
        }
    }

    uint64_t ComputeRay(int direction, uint64_t edge, int square, uint64_t totalOccupation) {
        uint64_t allMoves = 0ULL;
        uint64_t ray = 1ULL << square;
        while (true) {
            ray &= edge; 
            if (ray == 0ULL) break;
            ray = direction > 0 ? ray << direction : ray >> (-direction);
            if (ray == 0ULL) break;
            allMoves |= ray;
            if (ray & totalOccupation) break;
        }
        return allMoves;
    }

} // namespace MoveGen