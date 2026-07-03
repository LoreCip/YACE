#include "Engine.hpp"
#include "LookupTables.hpp"
#include "Types.hpp"
#include <algorithm>

int Engine::AlphaBeta(Board& board, int depth, int alpha, int beta, int ply) {
    if (timeIsUp) return 0;
    CheckTime();

    if (board.GetHalfMoveClock() >= 100) return 0;
    if (board.IsRepetition()) return 0;

    stats.nodesEvaluated++;
    if (depth == 0) return QuiescenceSearch(board, alpha, beta, ply);

    // --- 1. PROBE TRANSPOSITION TABLE ---
    uint64_t hashKey = board.GetHash();
    Move ttMove = 0;
    int ttScore = 0;
    if (tt.Probe(hashKey, depth, alpha, beta, ttScore, ttMove)) {
        stats.ttHits++;
        return ttScore;
    }

    MoveList moveList;
    MoveGen::GenerateAllMoves(board, moveList); 
    int nMoves = moveList.count;
    int legalMovesCount = 0; 

    // --- ORDERING WITH TT-MOVE ---
    struct ScoredMove {
        Move move;
        int score;
    };

    ScoredMove scoredMoves[256];
    for (int i = 0; i < nMoves; i++) {
        scoredMoves[i].move = moveList.moves[i];
        scoredMoves[i].score = ScoreMove(board, moveList.moves[i], ttMove, ply);
    }

    Move bestMoveInThisPosition = 0;
    int originalAlpha = alpha; 

    for (int i = 0; i < nMoves; i++) {
        /* ********** ON DEMAND SELECTION SORT ******************************************/
        int bestIndex = i;
        for (int j = i + 1; j < nMoves; j++) {
            if (scoredMoves[j].score > scoredMoves[bestIndex].score) bestIndex = j;
        }
        if (bestIndex != i) std::swap(scoredMoves[i], scoredMoves[bestIndex]);
        /********************************************************************************/
        
        Move move = scoredMoves[i].move;
        
        if (board.MakeMove(move)) {
            legalMovesCount++; 
            
            evaluator->OnMakeMove(board, move);

            int currentScore = -AlphaBeta(board, depth - 1, -beta, -alpha, ply + 1); 
            
            evaluator->OnUnmakeMove();
            board.UnmakeMove(move);
            
            if (currentScore >= beta) {
                stats.betaCutoffs++;
                if (legalMovesCount == 1) stats.firstMoveCutoffs++;
                
                int fflag = getMoveFlags(move);
                if (fflag == FlagMap::MOVE || fflag == FlagMap::DMOVE || fflag == FlagMap::CASTLING) {
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
        Color us = board.GetSideToMove();
        Color them = !us;
        
        uint64_t myKingBB = board.GetBitBoard(us, PieceType::KING);
        if (myKingBB == 0ULL) return 0; // Fallback

        uint64_t myKing = __builtin_ctzll(myKingBB);

        if (board.IsSquareAttacked(myKing, them)) {
            return -(999999 - depth); // Checkmate
        } else {
            return 0; // Stalemate
        }
    }

    // --- 2. RECORD TT ENTRY (EXACT o ALPHA) ---
    TTFlag flag = (alpha <= originalAlpha) ? ALPHA : EXACT;
    tt.Record(hashKey, bestMoveInThisPosition, depth, alpha, flag);

    return alpha;
}

int Engine::QuiescenceSearch(Board& board, int alpha, int beta, int ply) {
    if (timeIsUp) return 0;
    CheckTime();

    stats.qNodesEvaluated++;
    if (ply > stats.selDepthReached) stats.selDepthReached = ply;

    int stand_pat = evaluator->Evaluate(board);
    if (board.GetSideToMove() == Color::BLACK) {
        stand_pat = -stand_pat;
    }
    
    if (stand_pat >= beta){
        stats.betaCutoffs++;
        return beta;
    }
    if (alpha < stand_pat) alpha = stand_pat;

    MoveList moveList;
    MoveGen::GenerateAllMoves(board, moveList);
    int nMoves = moveList.count;

    struct ScoredMove {
        Move move;
        int score;
    };

    ScoredMove scoredMoves[256];
    for (int i = 0; i < nMoves; i++) {
        scoredMoves[i].move = moveList.moves[i];
        scoredMoves[i].score = ScoreMove(board, moveList.moves[i], (Move)0, 0); 
    }

    int qLegalMoves = 0;
    for (int i = 0; i < nMoves; i++) {
        /* ********** ON DEMAND SELECTION SORT ******************************************/
        int bestIndex = i;
        for (int j = i + 1; j < nMoves; j++) {
            if (scoredMoves[j].score > scoredMoves[bestIndex].score) bestIndex = j;
        }
        if (bestIndex != i) std::swap(scoredMoves[i], scoredMoves[bestIndex]);
        /********************************************************************************/
        
        Move move = scoredMoves[i].move;
        int flag = getMoveFlags(move);
        
        if (flag == FlagMap::CAPTURE || flag == FlagMap::ENPASS || 
            flag == FlagMap::PRCAPQUEEN || flag == FlagMap::PRCAPROOK || 
            flag == FlagMap::PRCAPBISHOP || flag == FlagMap::PRCAPKNIGHT) {
            
            if (board.MakeMove(move)) {
                qLegalMoves++;
                evaluator->OnMakeMove(board, move);

                int score = -QuiescenceSearch(board, -beta, -alpha, ply);
                
                evaluator->OnUnmakeMove();
                board.UnmakeMove(move);

                if (score >= beta){
                    stats.betaCutoffs++;
                    if (qLegalMoves == 1) stats.firstMoveCutoffs++;
                    return beta;
                }
                if (score > alpha) alpha = score;
            }
        }
    }
    return alpha;
}

Move Engine::GetBestMove(Board& board, int maxDepth, double allocatedTimeMs, InfoReporter callback) {
    stats.ResetForNewMove();
    startTime = std::chrono::high_resolution_clock::now();
    timeLimitMs = allocatedTimeMs;
    timeIsUp = false;
    
    MoveList moveList;
    MoveGen::GenerateAllMoves(board, moveList);    
    int nMoves = moveList.count;
    if (nMoves == 0) return 0;
    
    Move mossaMiglioreAssoluta = 0;
    for (int i = 0; i < nMoves; i++) {
        if (board.MakeMove(moveList.moves[i])) {
            mossaMiglioreAssoluta = moveList.moves[i];
            board.UnmakeMove(moveList.moves[i]);
            break; 
        }
    }
    if (mossaMiglioreAssoluta == 0) return 0; 

    // --- 2. ROOT SEARCH WITH ITERATIVE DEEPENING ---
    for (int d = 1; d <= maxDepth; d++) {
        stats.maxDepthReached = d;
        int alpha = -999999;
        int beta  = 999999;
        Move bestMoveCurrentDepth = mossaMiglioreAssoluta;

        struct ScoredMove {
            Move move;
            int score;
        };
        ScoredMove scoredMoves[256];
        for (int i = 0; i < nMoves; i++) {
            scoredMoves[i].move = moveList.moves[i];
            scoredMoves[i].score = ScoreMove(board, moveList.moves[i], mossaMiglioreAssoluta, 0); 
        }

        std::sort(scoredMoves, scoredMoves + nMoves, [](const ScoredMove& a, const ScoredMove& b) {
            return a.score > b.score;
        });

    
        for (int i = 0; i < nMoves; i++) {
            Move move = scoredMoves[i].move; 
            
            if (board.MakeMove(move)) {
                evaluator->OnMakeMove(board, move);

                int score = -AlphaBeta(board, d - 1, -beta, -alpha, 1);
                
                evaluator->OnUnmakeMove();
                board.UnmakeMove(move);
                
                if (timeIsUp) break;

                if (score > alpha) {
                    alpha = score;
                    bestMoveCurrentDepth = move;
                }
            }
        }
        
        if (timeIsUp) break;
        auto now = std::chrono::high_resolution_clock::now();
        stats.lastMoveTimeMs = std::chrono::duration<double, std::milli>(now - startTime).count();
        if (callback) callback(d, alpha, stats, bestMoveCurrentDepth);
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
    if (move == ttMove) return 1000000;

    int score = 0;
    int flags = getMoveFlags(move);
    int from = getMoveFrom(move);
    int to = getMoveTo(move);

    // --- Catture (MVV - LVA) ---
    if (flags == FlagMap::CAPTURE || flags == FlagMap::PRCAPQUEEN || flags == FlagMap::ENPASS) {
        Color us = board.GetSideToMove();
        Color them = !us;

        PieceType attacker = PieceType::NONE;
        for (int p = 0; p < 6; p++) {
            if (getBit(board.GetBitBoard(us, static_cast<PieceType>(p)), from)) {
                attacker = static_cast<PieceType>(p);
                break;
            }
        }

        attacker = board.GetPieceOnSquare(from);
        PieceType victim;
        if (flags == FlagMap::ENPASS) {
            victim = PieceType::PAWN;
        } else {
            victim = board.GetPieceOnSquare(to);
        }
        
        if (attacker != PieceType::NONE && victim != PieceType::NONE) {
            score = 100000 + (LookupTables::pieceValues[PieceInt(victim)] * 10) - LookupTables::pieceValues[PieceInt(attacker)];
        }
    } 
    // --- Promotions (not captures) ---
    else if (flags == FlagMap::PRQUEEN) {
        score = 90000; 
    } 
    else if (flags == FlagMap::PRROOK || flags == FlagMap::PRBISHOP || flags == FlagMap::PRKNIGHT) {
        score = 80000;
    }
    // --- Normal moves ---
    else {
        if (ply < MAX_PLY && move == killerMoves[ply][0]) {
            score = 90000; 
        } 
        else if (ply < MAX_PLY && move == killerMoves[ply][1]) {
            score = 80000; 
        } 
        else if (flags == FlagMap::CASTLING) {
            score = 50000; 
        } 
        else {
            score = 0;     
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