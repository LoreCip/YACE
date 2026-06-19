#include <iostream>
#include "Board.hpp"
#include "Engine.hpp"
#include "Move.hpp"
#include "LookupTables.hpp"



std::string moveToString(Move move) {
    if (move == 0) return "NESSUNA MOSSA"; // Gestione mossa nulla o di errore
    
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

    // 3. Estrazione del contesto della scacchiera
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

    std::string moveStr;
    // Legge le mosse passate da Python tramite Standard Input
    while (std::cin >> moveStr) {
        if (moveStr == "go") break; // Segnale per iniziare a pensare
        Move m = stringToMove(board, moveStr);
        board.MakeMove(m);
    }

    // Calcola e stampa la mossa migliore
    Move bestMove = engine.GetBestMove(board, 5); // Profondità 4
    std::cout << moveToString(bestMove) << std::endl;

    return 0;
}