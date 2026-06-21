#include <iostream>
#include <string>
#include <fstream>

#include "Board.hpp"
#include "Engine.hpp"
#include "Move.hpp"
#include "LookupTables.hpp"

std::string moveToString(Move move) {
    if (move == 0) return "0000"; // Ritorna formato standard per mossa nulla
    
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
    
    // CORREZIONE: Gestione esplicita di tutti i tipi di promozione
    int flag = getMoveFlags(move);
    if (flag == FlagMap::PRQUEEN || flag == FlagMap::PRCAPQUEEN) s += "q";
    else if (flag == FlagMap::PRROOK || flag == FlagMap::PRCAPROOK) s += "r";
    else if (flag == FlagMap::PRBISHOP || flag == FlagMap::PRCAPBISHOP) s += "b";
    else if (flag == FlagMap::PRKNIGHT || flag == FlagMap::PRCAPKNIGHT) s += "n";
    
    return s;
}

Move stringToMove(Board& board, std::string moveStr) {
    if (moveStr.length() < 4) return 0; 

    int fromFile = moveStr[0] - 'a'; // 'a' -> 0, 'h' -> 7
    int fromRank = moveStr[1] - '1'; // '1' -> 0, '8' -> 7
    int toFile   = moveStr[2] - 'a';
    int toRank   = moveStr[3] - '1';

    int from = fromRank * 8 + fromFile;
    int to   = toRank * 8 + toFile;

    int us = board.GetSideToMove();
    int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;

    PiecesEnum::Type movingPiece = PiecesEnum::NONE;
    for (const auto piece : PiecesEnum::All) {
        if (getBit(board.GetPieceBitBoard(us, piece), from)) {
            movingPiece = piece;
            break;
        }
    }

    bool isCapture = getBit(board.GetColorOccupation(them), to);

    int flag = FlagMap::MOVE;

    if (movingPiece == PiecesEnum::PAWNS) {
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
    else if (movingPiece == PiecesEnum::KING) {
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
    LookupTables::init();
    Board board;
    board.InitializeBoard();
    Engine engine;

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
        } 
        else if (token == "go") {
            Move best = engine.GetBestMove(board, 4);
            
            // 3. Scrittura del log condizionale
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

                // Apre il file in modalità append (aggiunge in coda senza sovrascrivere)
                std::ofstream logFile("engine_stats.log", std::ios::app);
                if (logFile.is_open()) {
                    logFile << "info time " << (int)stats.lastMoveTimeMs 
                            << " nodes " << totalNodes 
                            << " nps " << nps 
                            << " cutoffs " << stats.betaCutoffs 
                            << " (" << cutoffPct << "%)" << std::endl;
                }
            }

            std::cout << moveToString(best) << "\n";
            std::cout.flush();          
        } 
        else {
            board.MakeMove(stringToMove(board, token));
        }
    }
    return 0;
}