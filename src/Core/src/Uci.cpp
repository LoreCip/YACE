#include "Uci.hpp"
#include <iostream>
#include <iomanip>
#include <cstdint>

namespace UCI {

    static const char* const BENCH_FENS[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 4 5",
        "rnbqkb1r/pp1p1ppp/4pn2/2p5/2PP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 0 4",
        "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 6 8",
        "2rq1rk1/pb1n1ppp/1p2pn2/8/2BP4/2N1PN2/PP3PPP/2RQ1RK1 w - - 0 14",
        "r1b1kb1r/pp3ppp/1qn1pn2/2ppN3/3P4/2N1P3/PPP1BPPP/R1BQK2R w KQkq - 0 8",
        "rnb1kb1r/ppp1pppp/5n2/3q4/8/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 2 5",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2kr3r/pp1n1ppp/2p1p3/q7/1b1P4/2N1PN2/PPQ2PPP/2KR1B1R w - - 0 13",
        "5rk1/1p3ppp/p1p5/3Nn3/4P3/1P6/P4PPP/3R2K1 w - - 0 22",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "8/8/8/8/8/5k2/8/4K2R w K - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
        "4k3/8/8/8/8/8/8/4K2R w K - 0 1",
        "8/P7/8/8/8/8/p7/K6k w - - 0 1",
        "6k1/5p2/6p1/8/7P/6P1/5PK1/8 w - - 0 1",
    };  
    static const int NUM_BENCH_FENS = sizeof(BENCH_FENS) / sizeof(BENCH_FENS[0]);

    void Initialize() {
        std::cout << "id name YACE v1.0\n";
        std::cout << "id author LoreChip\n";
    }

    void ReportSearchInfo(int depth, int selDepth, int score, 
                          double timeMs, long long nodes, long long nps, 
                          int hashFull, const std::string& pvMove,
                          double moveOrdering, long long ttHits, long long qNodes) {
        
        std::cout << "info "
                  << "depth " << depth << " "
                  << "seldepth " << selDepth << " "
                  << "score cp " << score << " " 
                  << "time " << timeMs << " "
                  << "nodes " << nodes << " "
                  << "nps " << nps << " "
                  << "hashfull " << hashFull << " " 
                  << "pv " << pvMove 
                  << "\n";
                  
        std::cout << "info string "
                  << "MoveOrder: " << std::fixed << std::setprecision(1) << moveOrdering << "% | "
                  << "TTHits: " << ttHits << " | "
                  << "QNodes: " << qNodes
                  << "\n" << std::flush;
    }

    void ReportDebugString(const std::string& message) {
        std::cout << "info string " << message << "\n" << std::flush;
    }

    void Loop(Board& board, GoCallback onGoCommand, PositionLoadedCallback onPositionLoaded, MoveMadeCallback onMoveMade) {
        std::string token;
        
        LoggerCallback uciLogger = [](int depth, int selDepth, int score, 
                                      long long timeMs, long long nodes, long long nps, 
                                      int hashFull, const std::string& pvMove,
                                      double moveOrdering, long long ttHits, long long qNodes) {
            ReportSearchInfo(depth, selDepth, score, timeMs, nodes, nps, hashFull, pvMove, moveOrdering, ttHits, qNodes);
        };

        while (std::cin >> token) {
            if (token == "quit") {
                break;
            } else if (token == "fen") {
                std::string fenLine;
                std::getline(std::cin, fenLine);
                if (!fenLine.empty() && fenLine[0] == ' ') fenLine.erase(0, 1);
                
                board.InitializeFromFEN(fenLine);
                if (onPositionLoaded) onPositionLoaded();
            } else if (token == "go") {
                std::string line;
                std::getline(std::cin, line);
                std::stringstream ss(line);
                std::string arg;

                double allocatedTimeMs = 1000.0;
                int wtime = -1, btime = -1, depth = -1;

                while (ss >> arg) {
                    if (arg == "movetime") ss >> allocatedTimeMs;
                    else if (arg == "wtime") ss >> wtime;
                    else if (arg == "btime") ss >> btime;
                    else if (arg == "depth") ss >> depth;
                }

                if (wtime != -1 && btime != -1) {
                    Color us = board.GetSideToMove();
                    int myTime = (us == Color::WHITE) ? wtime : btime;
                    allocatedTimeMs = myTime / 30.0; 
                    if (allocatedTimeMs < 50) allocatedTimeMs = 50; 
                } else if (allocatedTimeMs == -1.0) {
                    allocatedTimeMs = 99999999.0;
                }

                int targetDepth = (depth != -1) ? depth : 100;

                if (onGoCommand) {
                    Move best = onGoCommand(targetDepth, allocatedTimeMs, uciLogger);
                    
                    if (best != 0 && board.MakeMove(best)) {
                        if (onMoveMade) onMoveMade(best);
                    }

                    std::cout << "bestmove " << MoveToString(best) << "\n";
                    std::cout.flush();     
                }
            } else if (token == "bench") {
                std::string line;
                std::getline(std::cin, line);
                std::stringstream ss(line);
                int depth = 8;
                ss >> depth;
                if (depth <= 0) depth = 8;

                long long totalNodes = 0;
                double totalTimeMs = 0.0;
                // FNV-1a, offset basis a 64 bit: firma deterministica di (profondita', score
                // finale, bestmove) per ogni posizione — il cancello di correttezza per ogni
                // ottimizzazione di velocita' successiva (deve restare invariata).
                uint64_t signature = 14695981039346656037ULL;
                auto foldSig = [&signature](long long v) {
                    for (int b = 0; b < 8; b++) {
                        uint8_t byte = static_cast<uint8_t>((static_cast<uint64_t>(v) >> (b * 8)) & 0xFFULL);
                        signature ^= byte;
                        signature *= 1099511628211ULL;
                    }
                };

                for (int i = 0; i < NUM_BENCH_FENS; i++) {
                    board.InitializeFromFEN(BENCH_FENS[i]);
                    if (onPositionLoaded) onPositionLoaded();

                    long long lastNodes = 0, lastTimeMs = 0;
                    int lastScore = 0;
                    LoggerCallback capture = [&](int, int, int score, long long timeMs, long long nodes,
                                                 long long, int, const std::string&, double, long long, long long) {
                        lastScore = score;
                        lastNodes = nodes;
                        lastTimeMs = timeMs;
                    };
                    Move best = onGoCommand ? onGoCommand(depth, 99999999.0, capture) : 0;

                    totalNodes += lastNodes;
                    totalTimeMs += static_cast<double>(lastTimeMs);

                    foldSig(static_cast<long long>(depth));
                    foldSig(static_cast<long long>(lastScore));
                    foldSig(static_cast<long long>(best));
                }

                long long nps = (totalTimeMs > 0.0)
                    ? static_cast<long long>(totalNodes / totalTimeMs * 1000.0)
                    : 0;

                std::cout << "bench: " << totalNodes << " nodes  " << totalTimeMs << " ms  "
                          << nps << " nps\n";
                std::cout << "bench signature: " << std::hex << signature << std::dec << "\n";
                std::cout.flush();
            } else if (token == "state") {
                std::cout << "info fen: " << board.GetFEN() << "\n";
            } else if (token == "move") {
                std::string actualMove;
                std::cin >> actualMove;
                Move parsedMove = StringToMove(board, actualMove);
                if (parsedMove != 0) {
                    if (board.MakeMove(parsedMove)) {
                        if (onMoveMade) onMoveMade(parsedMove);
                    } else {
                        ReportDebugString("illegal move");
                    }
                } else {
                    ReportDebugString("format not valid: " + actualMove);
                }
            } else {
                ReportDebugString("command not recognised, no action taken");
            }
        }
    }

    std::string MoveToString(Move move) {
        if (move == 0) return "0000"; // Mossa nulla

        int from = getMoveFrom(move);
        int to = getMoveTo(move);
        
        char fileFrom = 'a' + (from % 8);
        char rankFrom = '1' + (from / 8);
        char fileTo = 'a' + (to % 8);
        char rankTo = '1' + (to / 8);

        std::string moveStr = "";
        moveStr += fileFrom; moveStr += rankFrom;
        moveStr += fileTo; moveStr += rankTo;

        if (isMovePromotion(move)) {
            PieceType promo = getMovePromotionPiece(move);
            if (promo == PieceType::QUEEN) moveStr += 'q';
            else if (promo == PieceType::ROOK) moveStr += 'r';
            else if (promo == PieceType::BISHOP) moveStr += 'b';
            else if (promo == PieceType::KNIGHT) moveStr += 'n';
        }

        return moveStr;
    }

    Move StringToMove(const Board& board, const std::string& moveStr) {
        if (moveStr.length() < 4 || moveStr.length() > 5) return 0; 
        if (moveStr[0] < 'a' || moveStr[0] > 'h' || moveStr[2] < 'a' || moveStr[2] > 'h') return 0;
        if (moveStr[1] < '1' || moveStr[1] > '8' || moveStr[3] < '1' || moveStr[3] > '8') return 0;

        int fromFile = moveStr[0] - 'a'; // 'a' -> 0, 'h' -> 7
        int fromRank = moveStr[1] - '1'; // '1' -> 0, '8' -> 7
        int toFile   = moveStr[2] - 'a';
        int toRank   = moveStr[3] - '1';

        int from = fromRank * 8 + fromFile;
        int to   = toRank * 8 + toFile;

        Color us = board.GetSideToMove();
        Color them = !us;

        PieceType movingPiece = PieceType::NONE;
        for (const auto piece : Pieces::All) {
            if (getBit(board.GetBitBoard(us, piece), from)) {
                movingPiece = piece;
                break;
            }
        }

        bool isCapture = getBit(board.GetColorOccupation(them), to);

        int flag = FlagMap::MOVE;

        if (movingPiece == PieceType::PAWN) {
            if (std::abs(to - from) == 16) {
                flag = FlagMap::DMOVE;
            }
            else if (fromFile != toFile && !isCapture) {
                flag = FlagMap::ENPASS;
            }
            else if (moveStr.length() == 5) {
                char promo = moveStr[4];
                if (isCapture) {
                    if (promo == 'q') flag = FlagMap::PRCAPQUEEN;
                    else if (promo == 'r') flag = FlagMap::PRCAPROOK;
                    else if (promo == 'b') flag = FlagMap::PRCAPBISHOP;
                    else if (promo == 'n') flag = FlagMap::PRCAPKNIGHT;
                } else {
                    if (promo == 'q') flag = FlagMap::PRQUEEN;
                    else if (promo == 'r') flag = FlagMap::PRROOK;
                    else if (promo == 'b') flag = FlagMap::PRBISHOP;
                    else if (promo == 'n') flag = FlagMap::PRKNIGHT;
                }
            } 
            else if (isCapture) {
                flag = FlagMap::CAPTURE;
            }
        } 
        else if (movingPiece == PieceType::KING) {
            if (std::abs(fromFile - toFile) == 2) {
                flag = FlagMap::CASTLING;
            } else if (isCapture) {
                flag = FlagMap::CAPTURE;
            }
        }
        else if (isCapture) {
            flag = FlagMap::CAPTURE;
        }

        return createMove(from, to, flag);
    }
}