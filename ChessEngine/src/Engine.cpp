#include <algorithm>

#include "Engine.hpp"
#include "LookupTables.hpp"

/* PRIVATE */

int Engine::AlphaBeta(Board& board, int depth, int alpha, int beta, int ply) {
    if (timeIsUp) return 0;
    CheckTime();

    if (board.GetHalfMoveClock() >= 100) return 0;
    if (board.IsRepetition()) return 0;

    stats.nodesEvaluated++;
    if (depth == 0) return QuiescenceSearch(board, alpha, beta);

    // --- 1. PROBE TRANSPOSITION TABLE ---
    uint64_t hashKey = board.GetHash();
    Move ttMove = 0;
    int ttScore = 0;
    if (tt.Probe(hashKey, depth, alpha, beta, ttScore, ttMove)) {
        return ttScore;
    }

    Move moveList[256];
    int n_moves = GenerateAllMoves(board, moveList); 
    int legalMovesCount = 0; 

    // --- ORDERING WITH TT-MOVE ---
    struct ScoredMove {
        Move move;
        int score;
    };

    ScoredMove scoredMoves[256];
    for (int i = 0; i < n_moves; i++) {
        scoredMoves[i].move = moveList[i];
        scoredMoves[i].score = ScoreMove(board, moveList[i], ttMove, ply);
    }

    std::sort(scoredMoves, scoredMoves + n_moves, [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    Move bestMoveInThisPosition = 0;
    int originalAlpha = alpha; 

    for (int i = 0; i < n_moves; i++) {
        Move move = scoredMoves[i].move;
        if (board.MakeMove(move)) {
            legalMovesCount++; 

            int currentScore = -AlphaBeta(board, depth - 1, -beta, -alpha, ply + 1); 
            board.UnmakeMove(move);
            
            if (currentScore >= beta) {
                stats.betaCutoffs++;
                
                int fflag = getMoveFlags(move);
                if (fflag != FlagMap::MOVE && fflag != FlagMap::DMOVE && fflag != FlagMap::CASTLING){
                    if (ply < MAX_PLY) {
                        if (move != killerMoves[ply][0]) {
                            killerMoves[ply][1] = killerMoves[ply][0];
                            killerMoves[ply][0] = move;               
                        }
                    }
                }


                // --- RECORD BETA CUTOFF ---
                tt.Record(hashKey, move, depth, beta, BETA);
                return beta; 
            }
            if (currentScore > alpha) {
                alpha = currentScore; 
                bestMoveInThisPosition = move;
            }
        }
    }
    
    if (legalMovesCount == 0) {
        int us = board.GetSideToMove();
        int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;
        uint64_t myKing = __builtin_ctzll(board.GetBitBoard(us, PiecesEnum::KING));

        if (board.IsSquareAttacked(myKing, them)) {
            return -(999999 - depth); 
        } else {
            return 0; 
        }
    }

    // --- 2. RECORD TT ENTRY (EXACT o ALPHA) ---
    TTFlag flag = (alpha <= originalAlpha) ? ALPHA : EXACT;
    tt.Record(hashKey, bestMoveInThisPosition, depth, alpha, flag);

    return alpha;
}

int Engine::QuiescenceSearch(Board& board, int alpha, int beta) {
    if (timeIsUp) return 0;
    CheckTime();

    stats.qNodesEvaluated++;

    int stand_pat = board.Evaluate();
    
    if (stand_pat >= beta){
        stats.betaCutoffs++;
        return beta;
    }
    if (alpha < stand_pat) alpha = stand_pat;

    Move moveList[256];
    int n_moves = GenerateAllMoves(board, moveList);

    struct ScoredMove {
        Move move;
        int score;
    };

    ScoredMove scoredMoves[256];
    for (int i = 0; i < n_moves; i++) {
        scoredMoves[i].move = moveList[i];
        scoredMoves[i].score = ScoreMove(board, moveList[i], (Move)0, 0); // Or, pass ply to QuiescenceSearch and add it here
    }

    std::sort(scoredMoves, scoredMoves + n_moves, [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
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

        uint64_t bitboard = board.GetBitBoard(us, piece);

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
    return moveCount; 
}

void Engine::GeneratePawnMoves(Board& board, Move* moveList, int& moveCount) {
    int us = board.GetSideToMove();
    int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;
    
    uint64_t myPawns = board.GetBitBoard(us, PiecesEnum::PAWNS);
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

Move Engine::GetBestMove(Board& board, int maxDepth, double allocatedTimeMs) {
    stats.ResetForNewMove();
    startTime = std::chrono::high_resolution_clock::now();
    timeLimitMs = allocatedTimeMs;
    timeIsUp = false;
    
    Move moveList[256];
    int n_moves = GenerateAllMoves(board, moveList);    
    if (n_moves == 0) return 0;
    
    Move mossaMiglioreAssoluta = 0;
    for (int i = 0; i < n_moves; i++) {
        if (board.MakeMove(moveList[i])) {
            mossaMiglioreAssoluta = moveList[i];
            board.UnmakeMove(moveList[i]);
            break; 
        }
    }
    if (mossaMiglioreAssoluta == 0) return 0; 

    // --- 2. ROOT SEARCH WITH ITERATIVE DEEPENING ---
    for (int d = 1; d <= maxDepth; d++) {
        int alpha = -999999;
        int beta  = 999999;
        Move bestMoveCurrentDepth = mossaMiglioreAssoluta;

        struct ScoredMove {
            Move move;
            int score;
        };
        ScoredMove scoredMoves[256];
        for (int i = 0; i < n_moves; i++) {
            scoredMoves[i].move = moveList[i];
            // Passiamo "mossaMiglioreAssoluta" come ttMove per darle priorità 1.000.000
            scoredMoves[i].score = ScoreMove(board, moveList[i], mossaMiglioreAssoluta, 0); 
        }

        std::sort(scoredMoves, scoredMoves + n_moves, [](const ScoredMove& a, const ScoredMove& b) {
            return a.score > b.score;
        });

    
        for (int i = 0; i < n_moves; i++) {
            Move move = scoredMoves[i].move; 
            
            if (board.MakeMove(move)) {
                int score = -AlphaBeta(board, d - 1, -beta, -alpha, 1);
                board.UnmakeMove(move);
                
                if (timeIsUp) {
                    break;
                }

                if (score > alpha) {
                    alpha = score;
                    bestMoveCurrentDepth = move;
                }
            }
        }

        if (timeIsUp) {
            break; 
        }
        
        mossaMiglioreAssoluta = bestMoveCurrentDepth;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
    
    stats.lastMoveTimeMs = elapsed.count();
    stats.totalTimeMs += stats.lastMoveTimeMs;
    stats.movesPlayed++;
    stats.totalNodesSession += (stats.nodesEvaluated + stats.qNodesEvaluated);
    if (stats.lastMoveTimeMs < stats.minTimeMs) stats.minTimeMs = stats.lastMoveTimeMs;
    if (stats.lastMoveTimeMs > stats.maxTimeMs) stats.maxTimeMs = stats.lastMoveTimeMs;
    
    stats.hashFullness = tt.GetFullnessPercentage();
    
    return mossaMiglioreAssoluta;
}

int Engine::ScoreMove(Board& board, Move move, Move ttMove, int ply) {
    if (move == ttMove) {
        return 1000000; // Priorità assoluta alla mossa della TT
    }

    int score = 0;
    int flags = getMoveFlags(move);
    int from = getMoveFrom(move);
    int to = getMoveTo(move);

    // --- Catture (MVV - LVA) ---
    if (flags == FlagMap::CAPTURE || flags == FlagMap::PRCAPQUEEN || /* altri flag di cattura */ flags == FlagMap::ENPASS) {
        int us = board.GetSideToMove();
        int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;

        // Trova il pezzo attaccante
        PiecesEnum::Type attacker = PiecesEnum::NONE;
        for (const auto piece : PiecesEnum::All) {
            if (getBit(board.GetBitBoard(us, piece), from)) {
                attacker = piece;
                break;
            }
        }

        // Trova il pezzo vittima
        PiecesEnum::Type victim = PiecesEnum::NONE;
        if (flags == FlagMap::ENPASS) {
            victim = PiecesEnum::PAWNS;
        } else {
            for (const auto piece : PiecesEnum::All) {
                if (getBit(board.GetBitBoard(them, piece), to)) {
                    victim = piece;
                    break;
                }
            }
        }

        // Formula MVV-LVA: Vittima * 10 - Attaccante
        if (attacker != PiecesEnum::NONE && victim != PiecesEnum::NONE) {
            score = 100000 + (PiecesEnum::pieceValues[victim] * 10) - PiecesEnum::pieceValues[attacker];
        }
    } 
    // --- Promozioni (non di cattura) ---
    else if (flags == FlagMap::PRQUEEN) {
        score = 90000; 
    } 
    else if (flags == FlagMap::PRROOK || flags == FlagMap::PRBISHOP || flags == FlagMap::PRKNIGHT) {
        score = 80000;
    }
    // --- Mosse normali ---
    else {
        if (ply < MAX_PLY && move == killerMoves[ply][0]) {
            score = 90000; // Killer Primaria: Altissima priorità tra le silenziose
        } 
        else if (ply < MAX_PLY && move == killerMoves[ply][1]) {
            score = 80000; // Killer Secondaria: Alta priorità
        } 
        else if (flags == FlagMap::CASTLING) {
            score = 50000; // Arrocco
        } 
        else {
            score = 0;     // Mossa normale ignorata dalle euristiche
        }
    }

    return score;
}


void Engine::CheckTime() {
    if ((stats.nodesEvaluated & 2047) != 0) return;

    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = now - startTime;

    if (elapsed.count() >= timeLimitMs) {
        timeIsUp = true;
    }
}