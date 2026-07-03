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
    int us = ColorInt(board.GetSideToMove());

    for (int i = 0; i < nMoves; i++) {
        /* ********** ON DEMAND SELECTION SORT ******************************************/
        int bestIndex = i;
        for (int j = i + 1; j < nMoves; j++) {
            if (scoredMoves[j].score > scoredMoves[bestIndex].score) bestIndex = j;
        }
        if (bestIndex != i) std::swap(scoredMoves[i], scoredMoves[bestIndex]);
        /********************************************************************************/
        
        Move move = scoredMoves[i].move;
        int flags = getMoveFlags(move);
        bool isQuiet = (flags == FlagMap::MOVE || flags == FlagMap::DMOVE || flags == FlagMap::CASTLING);
        
        if (board.MakeMove(move)) {
            legalMovesCount++; 
            
            evaluator->OnMakeMove(board, move);
            int currentScore = -AlphaBeta(board, depth - 1, -beta, -alpha, ply + 1); 
            evaluator->OnUnmakeMove();
            board.UnmakeMove(move);
            
            if (currentScore >= beta) {
                stats.betaCutoffs++;
                if (legalMovesCount == 1) stats.firstMoveCutoffs++;
                
                if (isQuiet) {
                    // 1. Aggiorna le Killer Moves
                    if (ply < MAX_PLY) {
                        if (move != killerMoves[ply][0]) {
                            killerMoves[ply][1] = killerMoves[ply][0];
                            killerMoves[ply][0] = move;               
                        }
                    }

                    // 2. HISTORY HEURISTIC: BONUS
                    int from = getMoveFrom(move);
                    int to = getMoveTo(move);
                    historyTable[us][from][to] += (depth * depth);

                    // Normalizzazione per evitare overflow e appiattimento dei punteggi
                    if (historyTable[us][from][to] > 10000) {
                        for (int c = 0; c < 2; c++) {
                            for (int f = 0; f < 64; f++) {
                                for (int t = 0; t < 64; t++) {
                                    historyTable[c][f][t] /= 2;
                                }
                            }
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
            } else {
                // HISTORY HEURISTIC: PENALITÀ
                // Se la mossa silenziosa è stata cercata ma non ha migliorato alpha, perde punti
                if (isQuiet) {
                    int from = getMoveFrom(move);
                    int to = getMoveTo(move);
                    historyTable[us][from][to] -= depth;
                    if (historyTable[us][from][to] < 0) {
                        historyTable[us][from][to] = 0;
                    }
                }
            }
        }
    }
    
    if (legalMovesCount == 0) {
        Color myColor = board.GetSideToMove();
        Color them = !myColor;
        
        uint64_t myKingBB = board.GetBitBoard(myColor, PieceType::KING);
        if (myKingBB == 0ULL) return 0; // Fallback

        uint64_t myKing = __builtin_ctzll(myKingBB);

        if (board.IsSquareAttacked(myKing, them)) {
            return -(999999 - depth); // Checkmate
        } else {
            return 0; // Stalemate
        }
    }

    // --- 2. RECORD TT ENTRY (EXACT o ALPHA) ---
    if (bestMoveInThisPosition == 0) {
        bestMoveInThisPosition = ttMove;
    }
    
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
        return beta;
    }
    if (alpha < stand_pat) alpha = stand_pat;

    MoveList moveList;
    MoveGen::GenerateCaptureMoves(board, moveList);
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
                evaluator->OnMakeMove(board, move);

                int score = -QuiescenceSearch(board, -beta, -alpha, ply);
                
                evaluator->OnUnmakeMove();
                board.UnmakeMove(move);

                if (score >= beta){
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
    ClearHistory();
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
    if (move == ttMove) return 2000000; // Priorità Assoluta alla Transposition Table

    int score = 0;
    int flags = getMoveFlags(move);
    int from = getMoveFrom(move);
    int to = getMoveTo(move);

    // --- Catture (MVV - LVA) ---
    if (flags == FlagMap::CAPTURE || flags == FlagMap::PRCAPQUEEN || flags == FlagMap::ENPASS) {
        PieceType attacker = board.GetPieceOnSquare(from);
        PieceType victim = (flags == FlagMap::ENPASS) ? PieceType::PAWN : board.GetPieceOnSquare(to);
        
        if (attacker != PieceType::NONE && victim != PieceType::NONE) {
            // Partiamo da 1.000.000 per garantire che siano valutate prima delle mosse silenziose
            score = 1000000 + (LookupTables::pieceValues[PieceInt(victim)] * 10) - LookupTables::pieceValues[PieceInt(attacker)];
        }
    } 
    // --- Promotions (not captures) ---
    else if (flags == FlagMap::PRQUEEN) {
        score = 900000; 
    } 
    else if (flags == FlagMap::PRROOK || flags == FlagMap::PRBISHOP || flags == FlagMap::PRKNIGHT) {
        score = 800000;
    }
    // --- Normal moves (Quiet) ---
    else {
        if (ply < MAX_PLY && move == killerMoves[ply][0]) {
            score = 50000; // Killer move primaria
        } 
        else if (ply < MAX_PLY && move == killerMoves[ply][1]) {
            score = 40000; // Killer move secondaria
        } 
        else if (flags == FlagMap::CASTLING) {
            score = 30000; 
        } 
        else {
            // NUOVO: Lettura History
            int us = ColorInt(board.GetSideToMove());
            score = historyTable[us][from][to];
            
            // Cappiamo a 20.000 per assicurarci che non superino MAI le killer/castling/captures
            if (score > 20000) score = 20000;     
        }
    }

    return score;
}

void Engine::ClearHistory() {
    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                historyTable[c][i][j] = 0;
            }
        }
    }
}

void Engine::CheckTime() {
    if ((stats.nodesEvaluated & 2047) != 0) return;

    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = now - startTime;

    if (elapsed.count() >= timeLimitMs) {
        timeIsUp = true;
    }
}