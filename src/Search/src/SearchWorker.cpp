#include "SearchWorker.hpp"
#include "MoveGenerator.hpp"
#include "Evaluation.hpp"
#include "LookupTables.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>

void SearchWorker::Search(int maxDepth, InfoReporter callback) {
    stats = {};
    ClearHistory();
    bestRootMove = 0;

    auto startTime = std::chrono::high_resolution_clock::now();
    
    MoveList moveList;
    MoveGen::GenerateAllMoves(board, moveList);    
    int nMoves = moveList.count;
    if (nMoves == 0) return;
    
    Move bestMove = 0;
    for (int i = 0; i < nMoves; i++) {
        if (board.MakeMove(moveList.moves[i])) {
            bestMove = moveList.moves[i];
            board.UnmakeMove(moveList.moves[i]);
            break; 
        }
    }
    if (bestMove == 0) return;

    // Iterative deepening
    for (int d = 1; d <= maxDepth; d++) {
        if (globalTimeIsUp.load(std::memory_order_relaxed)) break;

        int alpha = -INFINITE_SCORE;
        int beta  = INFINITE_SCORE;
        Move bestMoveCurrentDepth = bestMove;

        ScoredMove scoredMoves[256];
        ScoreMoves(moveList, bestMove, 0, scoredMoves);

        std::sort(scoredMoves, scoredMoves + nMoves, [](const ScoredMove& a, const ScoredMove& b) {
            return a.score > b.score;
        });

        for (int i = 0; i < nMoves; i++) {
            Move move = scoredMoves[i].move; 
            
            if (board.MakeMove(move)) {
                evaluator->OnMakeMove(board, move);

                int score = -AlphaBeta(d - 1, -beta, -alpha, 1);
                
                evaluator->OnUnmakeMove();
                board.UnmakeMove(move);
                
                if (globalTimeIsUp.load(std::memory_order_relaxed)) break;

                if (score > alpha) {
                    alpha = score;
                    bestMoveCurrentDepth = move;
                }
            }
        }
        
        if (globalTimeIsUp.load(std::memory_order_relaxed)) break;
        
        bestMove = bestMoveCurrentDepth;
        bestRootMove = bestMove;

        auto now = std::chrono::high_resolution_clock::now();
        stats.lastMoveTimeMs = std::chrono::duration<double, std::milli>(now - startTime).count();

        if (callback) {
            stats.hashFullness = tt.GetFullnessPercentage();
            callback(d, alpha, stats, bestMoveCurrentDepth);
        }
    }
}

int SearchWorker::AlphaBeta(int depth, int alpha, int beta, int ply) {
    if (globalTimeIsUp.load(std::memory_order_relaxed)) return 0;

    if (board.GetHalfMoveClock() >= 100) return 0;
    if (board.IsRepetition()) return 0;

    stats.nodesEvaluated++;
    
    if (depth <= 0) return QuiescenceSearch(alpha, beta, ply);

    // Transposition table
    uint64_t hashKey = board.GetHash();
    Move ttMove = 0;
    int ttScore = 0;
    if (tt.Probe(hashKey, depth, alpha, beta, ttScore, ttMove, ply)) {
        stats.ttHits++;
        return ttScore;
    }

    int originalAlpha = alpha;
    Move bestMoveInThisPosition = 0;
    
    // Scacco: usato da NMP, LMR e matto/stallo
    Color us = board.GetSideToMove();
    uint64_t myKingBB = board.GetBitBoard(us, PieceType::KING);
    bool inCheck = false;
    if (myKingBB != 0ULL) {
        inCheck = board.IsSquareAttacked(__builtin_ctzll(myKingBB), !us);
    }

    // Null move pruning
    bool isNullMoveAllowed = (board.GetHistoryPly() > 0 && 
                              board.GetHistory(board.GetHistoryPly() - 1).movedPiece != PieceType::NONE);
    
    if (!inCheck && depth >= 3 && ply > 0 && isNullMoveAllowed) {
        // Solo con pezzi oltre ai pedoni (zugzwang)
        uint64_t nonPawnMaterial = board.GetColorOccupation(us) 
                                 ^ board.GetBitBoard(us, PieceType::KING) 
                                 ^ board.GetBitBoard(us, PieceType::PAWN);
        
        if (nonPawnMaterial != 0ULL) {
            board.MakeNullMove();
            int R = 2;
            int nullScore = -AlphaBeta(depth - 1 - R, -beta, -beta + 1, ply + 1);
            board.UnmakeNullMove();
            
            if (globalTimeIsUp.load(std::memory_order_relaxed)) return 0;
            if (nullScore >= beta) return beta; 
        }
    }

    MoveList moveList;
    MoveGen::GenerateAllMoves(board, moveList); 
    int nMoves = moveList.count;
    int legalMovesCount = 0; 

    ScoredMove scoredMoves[256];
    ScoreMoves(moveList, ttMove, ply, scoredMoves);

    // PVS + LMR
    for (int i = 0; i < nMoves; i++) {
        // Selection sort lazy (il cutoff arriva presto); inline: come helper costava ~1.5% di nps.
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
                // Prima mossa: finestra piena
                currentScore = -AlphaBeta(depth - 1, -beta, -alpha, ply + 1);
            } else {
                // LMR: mosse silenziose tardive a profondita' ridotta
                bool canReduce = isQuiet && !inCheck && depth >= 3 && legalMovesCount >= 4;
                bool needsFullSearch = true;

                if (canReduce) {
                    int R = (legalMovesCount > 6 && depth > 3) ? 2 : 1; 
                    currentScore = -AlphaBeta(depth - 1 - R, -alpha - 1, -alpha, ply + 1);
                    if (currentScore <= alpha) {
                        needsFullSearch = false;
                    }
                }

                // PVS: finestra nulla, finestra piena solo se la mossa sorprende
                if (needsFullSearch) {
                    currentScore = -AlphaBeta(depth - 1, -alpha - 1, -alpha, ply + 1);
                    
                    if (currentScore > alpha && currentScore < beta) {
                        currentScore = -AlphaBeta(depth - 1, -beta, -alpha, ply + 1);
                    }
                }
            }

            evaluator->OnUnmakeMove();
            board.UnmakeMove(move);
            
            // Beta cutoff
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
                    // History: bonus depth², tabella dimezzata oltre 10000
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
                tt.Record(hashKey, move, depth, beta, BETA, ply);
                return beta; 
            }
            
            if (currentScore > alpha) {
                alpha = currentScore; 
                bestMoveInThisPosition = move;
            } else {
                // Malus history alle mosse silenziose fallite in basso
                if (isQuiet) {
                    int from = getMoveFrom(move);
                    int to = getMoveTo(move);
                    historyTable[ColorInt(us)][from][to] -= depth;
                    if (historyTable[ColorInt(us)][from][to] < 0) historyTable[ColorInt(us)][from][to] = 0;
                }
            }
        }
    }
    
    // Nessuna mossa legale: matto o stallo
    if (legalMovesCount == 0) {
        if (inCheck) {
            return -(INFINITE_SCORE - ply); // matto: piu' vicino = peggiore
        } else {
            return 0;
        }
    }

    if (bestMoveInThisPosition == 0) {
        bestMoveInThisPosition = ttMove; // fail-low: conserva la mossa della TT
    }
    
    TTFlag flag = (alpha <= originalAlpha) ? ALPHA : EXACT;
    tt.Record(hashKey, bestMoveInThisPosition, depth, alpha, flag, ply);

    return alpha;
}

int SearchWorker::QuiescenceSearch(int alpha, int beta, int ply) {
    if (globalTimeIsUp.load(std::memory_order_relaxed)) return 0;

    stats.qNodesEvaluated++;
    if (ply > stats.selDepthReached) stats.selDepthReached = ply;

    int stand_pat = evaluator->Evaluate(board);
    
    if (stand_pat >= beta){
        return beta;
    }
    if (alpha < stand_pat) alpha = stand_pat;

    // Delta pruning (nodo): nemmeno prendere una donna (+ promozione, + margine) raggiunge alpha.
    const int DELTA_MARGIN = 200;
    const int* values = Evaluation::pieceValues;
    int promotionGain = values[PieceInt(PieceType::QUEEN)] - values[PieceInt(PieceType::PAWN)];
    int delta = values[PieceInt(PieceType::QUEEN)] + DELTA_MARGIN;
    Color us = board.GetSideToMove();
    uint64_t seventhRank = (us == Color::WHITE) ? 0x00FF000000000000ULL : 0x000000000000FF00ULL;
    if (board.GetBitBoard(us, PieceType::PAWN) & seventhRank) {
        delta += promotionGain;
    }
    if (stand_pat + delta < alpha) {
        return alpha;
    }

    MoveList moveList;
    MoveGen::GenerateCaptureMoves(board, moveList);
    int nMoves = moveList.count;

    ScoredMove scoredMoves[256];
    ScoreMoves(moveList, 0, 0, scoredMoves);

    for (int i = 0; i < nMoves; i++) {
        int bestIndex = i;
        for (int j = i + 1; j < nMoves; j++) {
            if (scoredMoves[j].score > scoredMoves[bestIndex].score) bestIndex = j;
        }
        if (bestIndex != i) std::swap(scoredMoves[i], scoredMoves[bestIndex]);
        Move move = scoredMoves[i].move;

        // Delta pruning (mossa): la vittima (+ promozione, + margine) non raggiunge alpha.
        int flags = getMoveFlags(move);
        int gain = 0;
        if (isEnPassantFlag(flags)) gain = values[PieceInt(PieceType::PAWN)];
        else if (isCaptureFlag(flags)) gain = values[PieceInt(board.GetPieceOnSquare(getMoveTo(move)))];
        if (promotionPieceFromFlags(flags) != PieceType::NONE) gain += promotionGain;
        if (stand_pat + gain + DELTA_MARGIN < alpha) continue;

        if (!SEE(move, 0)) continue;

        if (board.MakeMove(move)) {
            evaluator->OnMakeMove(board, move);

            int score = -QuiescenceSearch(-beta, -alpha, ply + 1);
            
            evaluator->OnUnmakeMove();
            board.UnmakeMove(move);

            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        }
    }
    return alpha;
}

void SearchWorker::ScoreMoves(const MoveList& moveList, Move ttMove, int ply, ScoredMove* scored) {
    for (int i = 0; i < moveList.count; i++) {
        scored[i] = {moveList.moves[i], ScoreMove(moveList.moves[i], ttMove, ply)};
    }
}

int SearchWorker::ScoreMove(Move move, Move ttMove, int ply) {
    if (move == ttMove) return 2000000;

    int score = 0;
    int flags = getMoveFlags(move);
    int from = getMoveFrom(move);
    int to = getMoveTo(move);

    // 1. Catture (MVV-LVA), comprese en passant e catture con promozione
    if (isCaptureFlag(flags) || isEnPassantFlag(flags)) {
        PieceType attacker = board.GetPieceOnSquare(from);
        PieceType victim = (flags == FlagMap::ENPASS) ? PieceType::PAWN : board.GetPieceOnSquare(to);
        if (attacker != PieceType::NONE && victim != PieceType::NONE) {
            score = 1000000 + (Evaluation::pieceValues[PieceInt(victim)] * 10) - Evaluation::pieceValues[PieceInt(attacker)];
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
            if (score > 20000) score = 20000;
        }
    }

    return score;
}

bool SearchWorker::SEE(Move move, int threshold) {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    PieceType movingPiece = board.GetPieceOnSquare(from);
    PieceType capturedPiece = board.GetPieceOnSquare(to);

    if (flags == FlagMap::ENPASS) {
        capturedPiece = PieceType::PAWN;
    }

    int gain[32]; // al piu' 32 catture su una casa
    int d = 0;
    gain[0] = (capturedPiece != PieceType::NONE) ? Evaluation::pieceValues[PieceInt(capturedPiece)] : 0;

    // Promozione: donna al posto del pedone
    if (flags == FlagMap::PRQUEEN || flags == FlagMap::PRCAPQUEEN) {
        gain[0] += Evaluation::pieceValues[PieceInt(PieceType::QUEEN)] - Evaluation::pieceValues[PieceInt(PieceType::PAWN)];
    }

    // Occupazione simulata: togliere gli attaccanti scopre i pezzi a raggi X
    uint64_t occupancy = board.GetColorOccupation(Color::WHITE) | board.GetColorOccupation(Color::BLACK);
    occupancy ^= (1ULL << from);
    
    if (flags == FlagMap::ENPASS) {
        // pedone catturato en passant
        int epOffset = (board.GetSideToMove() == Color::WHITE) ? -8 : 8; 
        occupancy ^= (1ULL << (to + epOffset));
    }

    Color nextColor = !board.GetSideToMove(); // ricattura l'avversario
    PieceType attackerPiece = movingPiece;

    // Ricatture alternate, sempre col pezzo meno pregiato
    while (true) {
        d++;
        gain[d] = Evaluation::pieceValues[PieceInt(attackerPiece)] - gain[d - 1];

        uint64_t nextAttackerBB = GetLeastValuableAttacker(to, nextColor, occupancy, attackerPiece);

        if (nextAttackerBB == 0ULL) {
            break;
        }

        occupancy ^= nextAttackerBB;
        nextColor = !nextColor;
    }

    // Minimax all'indietro: ognuno puo' smettere di ricatturare se non conviene.
    while (--d > 0) {
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
    }

    return gain[0] >= threshold;
}

uint64_t SearchWorker::GetLeastValuableAttacker(int sq, Color color, uint64_t occupancy, PieceType& outPiece) {
    uint64_t attackers = 0;

    // 1. Pedoni
    uint64_t pawns = board.GetBitBoard(color, PieceType::PAWN) & occupancy;
    if (pawns) {
        // pedoni che attaccano sq = attacchi di un pedone avversario da sq
        if ((attackers = AttackTables::pawnAttacks[ColorInt(!color)][sq] & pawns)) {
            outPiece = PieceType::PAWN;
            return attackers & -attackers; 
        }
    }

    // 2. Cavalli
    uint64_t knights = board.GetBitBoard(color, PieceType::KNIGHT) & occupancy;
    if ((attackers = AttackTables::knightAttacks[sq] & knights)) {
        outPiece = PieceType::KNIGHT;
        return attackers & -attackers;
    }

    // 3. Alfieri
    uint64_t bishops = board.GetBitBoard(color, PieceType::BISHOP) & occupancy;
    uint64_t bishopAttacks = MagicBitboards::GetBishopAttacks(sq, occupancy);
    if ((attackers = bishopAttacks & bishops)) {
        outPiece = PieceType::BISHOP;
        return attackers & -attackers;
    }

    // 4. Torri
    uint64_t rooks = board.GetBitBoard(color, PieceType::ROOK) & occupancy;
    uint64_t rookAttacks = MagicBitboards::GetRookAttacks(sq, occupancy);
    if ((attackers = rookAttacks & rooks)) {
        outPiece = PieceType::ROOK;
        return attackers & -attackers;
    }

    // 5. Regine
    uint64_t queens = board.GetBitBoard(color, PieceType::QUEEN) & occupancy;
    if ((attackers = (bishopAttacks | rookAttacks) & queens)) {
        outPiece = PieceType::QUEEN;
        return attackers & -attackers;
    }

    // 6. Re
    uint64_t kings = board.GetBitBoard(color, PieceType::KING) & occupancy;
    if ((attackers = AttackTables::kingAttacks[sq] & kings)) {
        outPiece = PieceType::KING;
        return attackers & -attackers;
    }

    outPiece = PieceType::NONE;
    return 0ULL;
}

void SearchWorker::ClearHistory() {
    std::memset(historyTable, 0, sizeof(historyTable));
}
