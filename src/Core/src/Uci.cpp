#include "Uci.hpp"
#include <iostream>
#include <iomanip>

namespace UCI {

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