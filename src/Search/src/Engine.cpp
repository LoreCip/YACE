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
    
    if (depth <= 0) return QuiescenceSearch(board, alpha, beta, ply);

    // --- 1. TRANSPOSITION TABLE PROBE ---
    uint64_t hashKey = board.GetHash();
    Move ttMove = 0;
    int ttScore = 0;
    if (tt.Probe(hashKey, depth, alpha, beta, ttScore, ttMove)) {
        stats.ttHits++;
        return ttScore;
    }

    int originalAlpha = alpha;
    Move bestMoveInThisPosition = 0;
    
    // Pre-calcolo dello stato di Scacco (usato da NMP, LMR e test finale)
    Color us = board.GetSideToMove();
    uint64_t myKingBB = board.GetBitBoard(us, PieceType::KING);
    bool inCheck = false;
    if (myKingBB != 0ULL) {
        inCheck = board.IsSquareAttacked(__builtin_ctzll(myKingBB), !us);
    }

    // --- 2. NULL MOVE PRUNING (NMP) ---
    bool isNullMoveAllowed = (board.GetHistoryPly() > 0 && 
                              board.GetHistory(board.GetHistoryPly() - 1).movedPiece != PieceType::NONE);
    
    if (!inCheck && depth >= 3 && ply > 0 && isNullMoveAllowed) {
        // Verifica materiale (Anti-Zugzwang)
        uint64_t nonPawnMaterial = board.GetColorOccupation(us) 
                                 ^ board.GetBitBoard(us, PieceType::KING) 
                                 ^ board.GetBitBoard(us, PieceType::PAWN);
        
        if (nonPawnMaterial != 0ULL) {
            board.MakeNullMove();
            int R = 2; // Taglio di 2 profondità
            int nullScore = -AlphaBeta(board, depth - 1 - R, -beta, -beta + 1, ply + 1);
            board.UnmakeNullMove();
            
            if (timeIsUp) return 0;
            if (nullScore >= beta) return beta; 
        }
    }

    // --- 3. GENERAZIONE ED ORDINAMENTO MOSSE ---
    MoveList moveList;
    MoveGen::GenerateAllMoves(board, moveList); 
    int nMoves = moveList.count;
    int legalMovesCount = 0; 

    struct ScoredMove {
        Move move;
        int score;
    };
    ScoredMove scoredMoves[256];
    for (int i = 0; i < nMoves; i++) {
        scoredMoves[i].move = moveList.moves[i];
        scoredMoves[i].score = ScoreMove(board, moveList.moves[i], ttMove, ply);
    }

    // --- 4. CICLO PRINCIPALE DELLE MOSSE (PVS + LMR) ---
    for (int i = 0; i < nMoves; i++) {
        
        // On-Demand Selection Sort
        int bestIndex = i;
        for (int j = i + 1; j < nMoves; j++) {
            if (scoredMoves[j].score > scoredMoves[bestIndex].score) bestIndex = j;
        }
        if (bestIndex != i) std::swap(scoredMoves[i], scoredMoves[bestIndex]);
        
        Move move = scoredMoves[i].move;
        int flags = getMoveFlags(move);
        bool isQuiet = (flags == FlagMap::MOVE || flags == FlagMap::DMOVE || flags == FlagMap::CASTLING);
        
        if (board.MakeMove(move)) {
            legalMovesCount++; 
            evaluator->OnMakeMove(board, move);

            int currentScore;
            
            if (legalMovesCount == 1) {
                // A. Principal Variation: Prima mossa testata a finestra completa
                currentScore = -AlphaBeta(board, depth - 1, -beta, -alpha, ply + 1);
            } else {
                // B. Late Move Reductions (LMR)
                bool canReduce = isQuiet && !inCheck && depth >= 3 && legalMovesCount >= 4;
                bool needsFullSearch = true;

                if (canReduce) {
                    int R = (legalMovesCount > 6 && depth > 3) ? 2 : 1; 
                    currentScore = -AlphaBeta(board, depth - 1 - R, -alpha - 1, -alpha, ply + 1);
                    if (currentScore <= alpha) {
                        needsFullSearch = false; // LMR convalidata, salta il resto
                    }
                }

                // C. Principal Variation Search (PVS)
                if (needsFullSearch) {
                    // Zero-Window Search a profondità piena
                    currentScore = -AlphaBeta(board, depth - 1, -alpha - 1, -alpha, ply + 1);
                    
                    // Re-Search (Finestra Completa) se la mossa ha superato le aspettative
                    if (currentScore > alpha && currentScore < beta) {
                        currentScore = -AlphaBeta(board, depth - 1, -beta, -alpha, ply + 1);
                    }
                }
            }

            evaluator->OnUnmakeMove();
            board.UnmakeMove(move);
            
            // --- 5. BETA CUTOFF (Fail-High) ---
            if (currentScore >= beta) {
                stats.betaCutoffs++;
                if (legalMovesCount == 1) stats.firstMoveCutoffs++;
                
                if (isQuiet) {
                    // Killer Moves
                    if (ply < MAX_PLY) {
                        if (move != killerMoves[ply][0]) {
                            killerMoves[ply][1] = killerMoves[ply][0];
                            killerMoves[ply][0] = move;               
                        }
                    }
                    // History Heuristic (Bonus e Normalizzazione)
                    int from = getMoveFrom(move);
                    int to = getMoveTo(move);
                    historyTable[ColorInt(us)][from][to] += (depth * depth);
                    if (historyTable[ColorInt(us)][from][to] > 10000) {
                        for (int c = 0; c < 2; c++) {
                            for (int f = 0; f < 64; f++) {
                                for (int t = 0; t < 64; t++) {
                                    historyTable[c][f][t] /= 2;
                                }
                            }
                        }
                    }
                }

                if (bestMoveInThisPosition == 0) bestMoveInThisPosition = move;
                tt.Record(hashKey, move, depth, beta, BETA);
                return beta; 
            }
            
            // --- 6. AGGIORNAMENTO ALPHA (PV Node) e PENALITÀ HISTORY ---
            if (currentScore > alpha) {
                alpha = currentScore; 
                bestMoveInThisPosition = move;
            } else {
                // Penalizza le mosse silenziose esplorate che hanno fallito in basso
                if (isQuiet) {
                    int from = getMoveFrom(move);
                    int to = getMoveTo(move);
                    historyTable[ColorInt(us)][from][to] -= depth;
                    if (historyTable[ColorInt(us)][from][to] < 0) historyTable[ColorInt(us)][from][to] = 0;
                }
            }
        }
    }
    
    // --- 7. VERIFICA MATTO / STALLO ---
    if (legalMovesCount == 0) {
        if (inCheck) {
            return -(999999 - ply); // Valuta la profondità del matto
        } else {
            return 0; // Stallo
        }
    }

    // --- 8. SALVATAGGIO TRANSPOSITION TABLE ---
    if (bestMoveInThisPosition == 0) {
        bestMoveInThisPosition = ttMove; // Evita di corrompere la TT
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

    /********** DELTA PRUNING ***************/
    const int DELTA_MARGIN = 90000 + 2000; 
    if (stand_pat + DELTA_MARGIN < alpha) {
        return alpha;
    }
    /****************************************/

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
                   
        if (board.MakeMove(move)) {
            evaluator->OnMakeMove(board, move);

            int score = -QuiescenceSearch(board, -beta, -alpha, ply);
            
            evaluator->OnUnmakeMove();
            board.UnmakeMove(move);

            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
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
    if (move == ttMove) return 2000000; // Massima priorità alla mossa della TT

    int score = 0;
    int flags = getMoveFlags(move);
    int from = getMoveFrom(move);
    int to = getMoveTo(move);

    // 1. Catture (MVV-LVA)
    if (flags == FlagMap::CAPTURE || flags == FlagMap::PRCAPQUEEN || flags == FlagMap::ENPASS) {
        PieceType attacker = board.GetPieceOnSquare(from);
        PieceType victim = (flags == FlagMap::ENPASS) ? PieceType::PAWN : board.GetPieceOnSquare(to);
        if (attacker != PieceType::NONE && victim != PieceType::NONE) {
            score = 1000000 + (LookupTables::pieceValues[PieceInt(victim)] * 10) - LookupTables::pieceValues[PieceInt(attacker)];
        }
    } 
    // 2. Promozioni Silenziose
    else if (flags == FlagMap::PRQUEEN) {
        score = 900000; 
    } 
    else if (flags == FlagMap::PRROOK || flags == FlagMap::PRBISHOP || flags == FlagMap::PRKNIGHT) {
        score = 800000;
    }
    // 3. Mosse Silenziose
    else {
        if (ply < MAX_PLY && move == killerMoves[ply][0]) {
            score = 50000; 
        } 
        else if (ply < MAX_PLY && move == killerMoves[ply][1]) {
            score = 40000; 
        } 
        else if (flags == FlagMap::CASTLING) {
            score = 30000; 
        } 
        else {
            int us = ColorInt(board.GetSideToMove());
            score = historyTable[us][from][to];
            if (score > 20000) score = 20000; // Limite superiore per la History
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