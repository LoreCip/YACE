#include "Engine.hpp"
#include "Pieces.hpp"
#include "Move.hpp"
#include "BitOperations.hpp"
#include "LookupTables.hpp"

#include <cstdint>

/* PRIVATE */
int Engine::AlphaBeta(Board& board, int depth, int alpha, int beta) {
    // if (board.IsRepetition()) return 21999;
    if (depth == 0) return QuiescenceSearch(board, alpha, beta);

    int legalMovesCount = 0; 
    std::vector<Move> moveList = GenerateAllMoves(board); 

    for (Move move : moveList) {
        if (board.MakeMove(move)) {
            legalMovesCount++; 

            // RICORSIONE NEGAMAX: Invertiamo il segno, e passiamo -beta e -alpha
            int currentScore = -AlphaBeta(board, depth - 1, -beta, -alpha); 
            board.UnmakeMove(move);
            
            // TAGLIO BETA (Fail-Hard)
            if (currentScore >= beta) {
                return beta; 
            }

            // AGGIORNAMENTO ALPHA
            if (currentScore > alpha) {
                alpha = currentScore; 
            }
        }
    }
    
    // GESTIONE MATTO E STALLO
    if (legalMovesCount == 0) {
        int us = board.GetSideToMove();
        int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;
        uint64_t myKing = __builtin_ctzll(board.GetPieceBitBoard(us, PiecesEnum::KING));

        if (board.IsSquareAttacked(myKing, them)) {
            return -(999999 - depth);; 
        } else {
            return 0; 
        }
    }
    
    return alpha;
}

int Engine::QuiescenceSearch(Board& board, int alpha, int beta) {
    // Valutazione "stand pat": quanto vale la posizione se NON catturiamo nulla?
    int stand_pat = board.Evaluate();
    
    // Fail-hard beta cutoff
    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    std::vector<Move> moveList = GenerateAllMoves(board);

    for (Move move : moveList) {
        int flag = getMoveFlags(move);
        
        // ESPLORIAMO SOLO LE CATTURE! (Ignoriamo le mosse silenziose)
        if (flag == FlagMap::CAPTURE || flag == FlagMap::PRCAPQUEEN || 
            flag == FlagMap::PRCAPROOK || flag == FlagMap::PRCAPBISHOP || flag == FlagMap::PRCAPKNIGHT) {
            
            if (board.MakeMove(move)) {
                int score = -QuiescenceSearch(board, -beta, -alpha);
                board.UnmakeMove(move);

                if (score >= beta) return beta;
                if (score > alpha) alpha = score;
            }
        }
    }
    return alpha;
}

std::vector<Move> Engine::GenerateAllMoves(Board& board) {
    std::vector<Move> moveList;
    moveList.reserve(64); 

    int us = board.GetSideToMove();
    int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;
    
    uint64_t enemyOccupation = board.GetColorOccupation(them);

    for (const auto piece : PiecesEnum::All) {
        if (piece == PiecesEnum::PAWNS) continue;

        uint64_t bitboard = board.GetPieceBitBoard(us, piece);

        while (bitboard != (uint64_t)0) {
            int from = __builtin_ctzll(bitboard); 

            uint64_t singlePieceBB = setBit(0ULL, from); 
            uint64_t allTo = board.GetGeneratedMoves(us, singlePieceBB, piece);
            
            while (allTo != (uint64_t)0) {
                int to = __builtin_ctzll(allTo);
                int flag = FlagMap::MOVE;
                if (setBit(0ULL, to) & enemyOccupation) {
                    flag = FlagMap::CAPTURE;
                }

                Move move = createMove(from, to, flag);
                moveList.push_back(move);

                allTo = clearBit(allTo, to);
            }
            bitboard = clearBit(bitboard, from); 
        }
    }

    GeneratePawnMoves( board, moveList);
    return moveList;
}

void Engine::GeneratePawnMoves(Board& board, std::vector<Move>& moveList) {
    int us = board.GetSideToMove();
    int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;
    
    uint64_t myPawns = board.GetPieceBitBoard(us, PiecesEnum::PAWNS);
    uint64_t emptySquares = board.GetFreeCells();
    uint64_t enemyOccupation = board.GetColorOccupation(them);
    
    int direction = (us == Color::WHITE) ? 8 : -8;
    int startRankStart = (us == Color::WHITE) ? 8  : 48;
    int startRankEnd   = (us == Color::WHITE) ? 15 : 55;
    int promoRankStart = (us == Color::WHITE) ? 56 : 0; 
    int promoRankEnd   = (us == Color::WHITE) ? 63 : 7; 

    while (myPawns != 0ULL) {
        int from = __builtin_ctzll(myPawns);
        
        int toSingle = from + direction;
        
        if (setBit(0ULL, toSingle) & emptySquares) {
            
            if (toSingle >= promoRankStart && toSingle <= promoRankEnd) {
                moveList.push_back(createMove(from, toSingle, FlagMap::PRQUEEN));
                moveList.push_back(createMove(from, toSingle, FlagMap::PRROOK));
                moveList.push_back(createMove(from, toSingle, FlagMap::PRBISHOP));
                moveList.push_back(createMove(from, toSingle, FlagMap::PRKNIGHT));
            } 
            else {
                moveList.push_back(createMove(from, toSingle, FlagMap::MOVE));
                
                if (from >= startRankStart && from <= startRankEnd) {
                    int toDouble = from + (direction * 2);
                    if (setBit(0ULL, toDouble) & emptySquares) {
                        moveList.push_back(createMove(from, toDouble, FlagMap::DMOVE));
                    }
                }
            }
        }

        uint64_t targets = LookupTables::pawnAttacks[us][from] & enemyOccupation;
        
        while (targets != 0ULL) {
            int toCapture = __builtin_ctzll(targets);
            
            if (toCapture >= promoRankStart && toCapture <= promoRankEnd) {
                moveList.push_back(createMove(from, toCapture, FlagMap::PRCAPQUEEN));
                moveList.push_back(createMove(from, toCapture, FlagMap::PRCAPROOK));
                moveList.push_back(createMove(from, toCapture, FlagMap::PRCAPBISHOP));
                moveList.push_back(createMove(from, toCapture, FlagMap::PRCAPKNIGHT));
            } 
            else {
                moveList.push_back(createMove(from, toCapture, FlagMap::CAPTURE));
            }
            
            targets = clearBit(targets, toCapture);
        }

        int epSquare = board.GetEnPassantSquare();
        if (epSquare != 64) { // Se 64, non c'è En Passant attivo
            uint64_t epBB = 1ULL << epSquare;
            if (LookupTables::pawnAttacks[us][from] & epBB) {
                moveList.push_back(createMove(from, epSquare, FlagMap::ENPASS));
            }
        }

        myPawns = clearBit(myPawns, from);

        myPawns = clearBit(myPawns, from);
    }
}


/* PUBLIC */

Move Engine::GetBestMove(Board& board, int depth) {
    std::vector<Move> moveList = GenerateAllMoves(board);
    Move bestMove = 0;

    int alpha = -999999;
    int beta  =  999999;

    for (Move move : moveList) {
        if (board.MakeMove(move)) {
            int score = -AlphaBeta(board, depth - 1, -beta, -alpha);
            board.UnmakeMove(move);

            if (score > alpha) {
                alpha = score;
                bestMove = move;
            }
        }
    }

    return bestMove;
}