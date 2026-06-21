#include "Engine.hpp"
#include <algorithm>

#include "LookupTables.hpp"

/* PRIVATE */

int Engine::AlphaBeta(Board& board, int depth, int alpha, int beta) {
    if (board.IsRepetition()) return -10; 
    
    stats.nodesEvaluated++;

    if (depth == 0) return QuiescenceSearch(board, alpha, beta);

    Move moveList[256];
    int n_moves = GenerateAllMoves(board, moveList); 
    int legalMovesCount = 0; 

    std::sort(moveList, moveList + n_moves, [](const Move& a, const Move& b) {
        return getMoveFlags(a) > getMoveFlags(b); 
    });

    for (int i = 0; i < n_moves; i++) {
        Move move = moveList[i];
        if (board.MakeMove(move)) {
            legalMovesCount++; 

            int currentScore = -AlphaBeta(board, depth - 1, -beta, -alpha); 
            board.UnmakeMove(move);
            
            if (currentScore >= beta) {
                stats.betaCutoffs++;
                return beta; 
            }
            if (currentScore > alpha) {
                alpha = currentScore; 
            }
        }
    }
    
    if (legalMovesCount == 0) {
        int us = board.GetSideToMove();
        int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;
        uint64_t myKing = __builtin_ctzll(board.GetPieceBitBoard(us, PiecesEnum::KING));

        if (board.IsSquareAttacked(myKing, them)) {
            return -(999999 - depth); 
        } else {
            return 0; 
        }
    }

    return alpha;
}

int Engine::QuiescenceSearch(Board& board, int alpha, int beta) {
    stats.qNodesEvaluated++;

    int stand_pat = board.Evaluate();
    
    if (stand_pat >= beta){
        stats.betaCutoffs++;
        return beta;
    }
    if (alpha < stand_pat) alpha = stand_pat;

    Move moveList[256];
    int n_moves = GenerateAllMoves(board, moveList);

    std::sort(moveList, moveList + n_moves, [](const Move& a, const Move& b) {
        return getMoveFlags(a) > getMoveFlags(b);
    });

    for (int i = 0; i < n_moves; i++) {
        Move move = moveList[i];
        int flag = getMoveFlags(move);
        
        if (flag == FlagMap::CAPTURE || flag == FlagMap::ENPASS || 
            flag == FlagMap::PRCAPQUEEN || flag == FlagMap::PRCAPROOK || 
            flag == FlagMap::PRCAPBISHOP || flag == FlagMap::PRCAPKNIGHT) {
            
            if (board.MakeMove(move)) {
                int score = -QuiescenceSearch(board, -beta, -alpha);
                board.UnmakeMove(move);

                if (score >= beta){
                    stats.betaCutoffs++;
                    return beta;
                }
                if (score > alpha) alpha = score;
            }
        }
    }
    return alpha;
}

int Engine::GenerateAllMoves(Board& board, Move* moveList) {
    int moveCount = 0;

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

                moveList[moveCount++] = createMove(from, to, flag);
                allTo = clearBit(allTo, to);
            }
            bitboard = clearBit(bitboard, from); 
        }
    }

    GeneratePawnMoves(board, moveList, moveCount);
    return moveCount; // Ritorna il numero totale di mosse scritte nell'array
}

void Engine::GeneratePawnMoves(Board& board, Move* moveList, int& moveCount) {
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
                moveList[moveCount++] = createMove(from, toSingle, FlagMap::PRQUEEN);
                moveList[moveCount++] = createMove(from, toSingle, FlagMap::PRROOK);
                moveList[moveCount++] = createMove(from, toSingle, FlagMap::PRBISHOP);
                moveList[moveCount++] = createMove(from, toSingle, FlagMap::PRKNIGHT);
            } 
            else {
                moveList[moveCount++] = createMove(from, toSingle, FlagMap::MOVE);
                
                if (from >= startRankStart && from <= startRankEnd) {
                    int toDouble = from + (direction * 2);
                    if (setBit(0ULL, toDouble) & emptySquares) {
                        moveList[moveCount++] = createMove(from, toDouble, FlagMap::DMOVE);
                    }
                }
            }
        }

        uint64_t targets = LookupTables::pawnAttacks[us][from] & enemyOccupation;
        while (targets != 0ULL) {
            int toCapture = __builtin_ctzll(targets);
            
            if (toCapture >= promoRankStart && toCapture <= promoRankEnd) {
                moveList[moveCount++] = createMove(from, toCapture, FlagMap::PRCAPQUEEN);
                moveList[moveCount++] = createMove(from, toCapture, FlagMap::PRCAPROOK);
                moveList[moveCount++] = createMove(from, toCapture, FlagMap::PRCAPBISHOP);
                moveList[moveCount++] = createMove(from, toCapture, FlagMap::PRCAPKNIGHT);
            } 
            else {
                moveList[moveCount++] = createMove(from, toCapture, FlagMap::CAPTURE);
            }
            targets = clearBit(targets, toCapture);
        }

        // En Passant
        int epSquare = board.GetEnPassantSquare();
        if (epSquare != 64) {
            uint64_t epBB = 1ULL << epSquare;
            if (LookupTables::pawnAttacks[us][from] & epBB) {
                moveList[moveCount++] = createMove(from, epSquare, FlagMap::ENPASS);
            }
        }

        myPawns = clearBit(myPawns, from);
    }
}

Move Engine::GetBestMove(Board& board, int depth) {
    stats.ResetForNewMove();
    auto startTime = std::chrono::high_resolution_clock::now();

    Move moveList[256];
    int n_moves = GenerateAllMoves(board, moveList);
    Move bestMove = 0;

    int alpha = -999999;
    int beta  =  999999;

    std::sort(moveList, moveList + n_moves, [](const Move& a, const Move& b) {
        return getMoveFlags(a) > getMoveFlags(b);
    });

    for (int i = 0; i < n_moves; i++) {
        Move move = moveList[i];
        if (board.MakeMove(move)) {
            int score = -AlphaBeta(board, depth - 1, -beta, -alpha);
            board.UnmakeMove(move);

            if (score > alpha) {
                alpha = score;
                bestMove = move;
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
    
    stats.lastMoveTimeMs = elapsed.count();
    
    stats.totalTimeMs += stats.lastMoveTimeMs;
    stats.movesPlayed++;
    stats.totalNodesSession += (stats.nodesEvaluated + stats.qNodesEvaluated);
    if (stats.lastMoveTimeMs < stats.minTimeMs) stats.minTimeMs = stats.lastMoveTimeMs;
    if (stats.lastMoveTimeMs > stats.maxTimeMs) stats.maxTimeMs = stats.lastMoveTimeMs;

    return bestMove;
}