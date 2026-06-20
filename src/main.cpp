#include <cstdint>
#include <iostream>
#include <string>

#include "BitOperations.hpp"
#include "LookupTables.hpp"
#include "Move.hpp"
#include "Pieces.hpp"
#include "Board.hpp"
#include "Engine.hpp"

void printChessBoard(Board& board) {
    std::cout << "\n";

    for (int rank = 7; rank >= 0; rank--) {
        std::cout << rank + 1 << "  "; // Stampa il numero della traversa
        
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            char pieceChar = '.'; // Punto se la casa è vuota

            if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::PAWNS), square)) pieceChar = 'P';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::KNIGHTS), square)) pieceChar = 'N';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::BISHOPS), square)) pieceChar = 'B';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::ROOKS), square)) pieceChar = 'R';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::QUEEN), square)) pieceChar = 'Q';
            else if (getBit(board.GetPieceBitBoard(Color::WHITE, PiecesEnum::KING), square)) pieceChar = 'K';
            
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::PAWNS), square)) pieceChar = 'p';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::KNIGHTS), square)) pieceChar = 'n';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::BISHOPS), square)) pieceChar = 'b';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::ROOKS), square)) pieceChar = 'r';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::QUEEN), square)) pieceChar = 'q';
            else if (getBit(board.GetPieceBitBoard(Color::BLACK, PiecesEnum::KING), square)) pieceChar = 'k';

            std::cout << pieceChar << " ";
        }
        std::cout << "\n";
    }
    std::cout << "   a b c d e f g h\n\n";
}

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

std::string moveToAlgebraic(Move move) {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    
    char fromFile = 'a' + (from % 8);
    char fromRank = '1' + (from / 8);
    char toFile   = 'a' + (to % 8);
    char toRank   = '1' + (to / 8);
    
    std::string moveStr = "";
    moveStr += fromFile;
    moveStr += fromRank;
    moveStr += toFile;
    moveStr += toRank;
    
    return moveStr;
}

uint64_t Perft(Engine& engine, Board& board, int depth) {
    std::vector<Move> moveList = engine.GenerateAllMoves(board);
    int n_moves = moveList.size();
    uint64_t nodes = 0;

    if (depth == 0) return 1ULL;

    for (int i = 0; i < n_moves; i++) {
        if (!board.MakeMove(moveList[i])) {
            continue;
        }
        nodes += Perft(engine, board, depth - 1);
        board.UnmakeMove(moveList[i]);
    }
    return nodes;
} 

uint64_t PerftDivide(Engine& engine, Board& board, int depth) {
    if (depth == 0) return 1ULL;

    std::vector<Move> moveList = engine.GenerateAllMoves(board);
    uint64_t totalNodes = 0;

    std::cout << "\n--- PERFT DIVIDE (Depth " << depth << ") ---" << std::endl;

    for (const auto& move : moveList) {
        if (!board.MakeMove(move)) {
            continue; 
        }

        uint64_t nodesForThisMove = Perft(engine, board, depth - 1);
        
        totalNodes += nodesForThisMove;

        std::cout << moveToAlgebraic(move) << ": " << nodesForThisMove << std::endl;

        board.UnmakeMove(move);
    }

    std::cout << "\nNodi Totali generati dal Divide: " << totalNodes << std::endl;
    std::cout << "---------------------------------" << std::endl;

    return totalNodes;
}



int main() {
    LookupTables::init();
    Engine myEngine;
    Board gameBoard;
    gameBoard.InitializeBoard();

    for (int i = 0; i < 7; i++)
        PerftDivide(myEngine, gameBoard, i);

    return 0;
}