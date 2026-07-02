#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>

#include "LookupTables.hpp"
#include "Board.hpp"
#include "TranspositionTable.hpp"
#include "ClassicalEvaluator.hpp"
#include "NnueEvaluator.hpp"
#include "Engine.hpp"
#include "MoveGenerator.hpp"
#include "Types.hpp"

std::string moveToString(Move move) {
    if (move == 0) return "0000";
    
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    
    char fromFile = 'a' + (from % 8);
    char fromRank = '1' + (from / 8);
    char toFile = 'a' + (to % 8);
    char toRank = '1' + (to / 8);
    
    std::string s = "";
    s += fromFile;
    s += fromRank;
    s += toFile;
    s += toRank;
    
    int flag = getMoveFlags(move);
    if (flag == FlagMap::PRQUEEN || flag == FlagMap::PRCAPQUEEN) s += "q";
    else if (flag == FlagMap::PRROOK || flag == FlagMap::PRCAPROOK) s += "r";
    else if (flag == FlagMap::PRBISHOP || flag == FlagMap::PRCAPBISHOP) s += "b";
    else if (flag == FlagMap::PRKNIGHT || flag == FlagMap::PRCAPKNIGHT) s += "n";
    
    return s;
}

Move stringToMove(const Board& board, std::string moveStr) {
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


int main() {
    std::cout << std::unitbuf;
    LookupTables::init();
    Board board;
    board.InitializeBoard();

    TranspositionTable tt(64);
    ClassicalEvaluator hce;
    NnueEvaluator nnue;
    IEvaluator* activeEvaluator = nullptr;

    std::string weightsPath = "/home/lorenzo/Scrivania/Projects/YACE/NNUE/training/weights/nnue_weights_v1.bin";
    if (nnue.Initialize(weightsPath)) {
        std::cout << "info string NNUE caricata con successo (" << weightsPath << ")\n";
        nnue.Reset(board);
        activeEvaluator = &nnue;
    } else {
        std::cout << "info string Attenzione: NNUE non trovata. Fallback alla Valutazione Classica (HCE).\n";
        activeEvaluator = &hce;
    }

    Engine engine(tt, activeEvaluator);
    
    bool enableLogging = false; 

    std::string token;
    while (std::cin >> token) {
        if (token == "quit") {
            break;
        } 
        else if (token == "log") {
            std::string state;
            std::cin >> state;
            if (state == "on") {
                enableLogging = true;
            } else if (state == "off") {
                enableLogging = false;
            }
        }
        else if (token == "fen") {
            std::string fenLine;
            std::getline(std::cin, fenLine);
            if (!fenLine.empty() && fenLine[0] == ' ') {
                fenLine.erase(0, 1);
            }
            board.InitializeFromFEN(fenLine);
            
            // FONDAMENTALE: Quando forzi un FEN, la rete neurale deve resettare lo stack!
            if (activeEvaluator) activeEvaluator->Reset(board);
        } 
        else if (token == "go") {
            std::string line;
            std::getline(std::cin, line);
            std::stringstream ss(line);
            std::string arg;

            double allocatedTimeMs = 1000.0;
            int wtime = -1, btime = -1, depth = -1;

            while (ss >> arg) {
                if (arg == "movetime") {
                    ss >> allocatedTimeMs;
                } else if (arg == "wtime") {
                    ss >> wtime;
                } else if (arg == "btime") {
                    ss >> btime;
                } else if (arg == "depth") {
                    ss >> depth;
                }
            }

            if (wtime != -1 && btime != -1) {
                Color us = board.GetSideToMove();
                int myTime = (us == Color::WHITE) ? wtime : btime;
                
                allocatedTimeMs = myTime / 30.0; 
                
                if (allocatedTimeMs < 50) allocatedTimeMs = 50; 
            }

            int targetDepth = (depth != -1) ? depth : 100;

            Move best = engine.GetBestMove(board, targetDepth, allocatedTimeMs);
            
            if (enableLogging) {
                const SearchStats& stats = engine.GetStats();
                long long totalNodes = stats.nodesEvaluated + stats.qNodesEvaluated;
                long long nps = 0;
                if (stats.lastMoveTimeMs > 0) {
                    nps = (totalNodes * 1000) / stats.lastMoveTimeMs;
                }
                
                double cutoffPct = 0.0;
                if (totalNodes > 0) {
                    cutoffPct = ((double)stats.betaCutoffs / totalNodes) * 100.0;
                }

                std::ofstream logFile("engine_stats.log", std::ios::app);
                if (logFile.is_open()) {
                    logFile << "info time " << (int)stats.lastMoveTimeMs
                            << " nodes " << totalNodes
                            << " nps " << nps
                            << " cutoffs " << stats.betaCutoffs
                            << " (" << cutoffPct << "%)"
                            << " hash fullness " << stats.hashFullness << "%"
                            << std::endl;
                }
            }

            if (best != 0) {
                if (board.MakeMove(best)) {
                    if (activeEvaluator) activeEvaluator->OnMakeMove(board, best);
                }
            }

            std::cout << "bestmove " << moveToString(best) << "\n";
            std::cout.flush();     
        }
        else if (token == "move") {
            std::string actualMove;
            std::cin >> actualMove;
            Move parsedMove = stringToMove(board, actualMove);
            if (parsedMove != 0) {
                if (board.MakeMove(parsedMove)) {
                    if (activeEvaluator) activeEvaluator->OnMakeMove(board, parsedMove);
                } else {
                    std::cout << "info string Mossa illegale (lascia il Re sotto scacco)!\n";
                }
            }
        }
        else if (token == "state") {
            std::cout << "FEN attuale: " << board.GetFEN() << "\n";
        }
        else {
            // Gestione dei comandi "diretti" es. e2e4 senza "move" davanti
            Move parsedMove = stringToMove(board, token);
            if (parsedMove != 0) {
                if (board.MakeMove(parsedMove)) {
                    if (activeEvaluator) activeEvaluator->OnMakeMove(board, parsedMove);
                } else {
                    std::cout << "info string Mossa illegale!\n";
                }
            }
        }
    }
    return 0;
}