#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <omp.h>
#include "LookupTables.hpp"
#include "Move.hpp"
#include "Board.hpp"
#include "Engine.hpp"

uint64_t Perft(Engine& engine, Board& board, int depth) {
    if (depth == 0) return 1ULL;

    Move moveList[256];
    int n_moves = engine.GenerateAllMoves(board, moveList);
    uint64_t nodes = 0;

    for (int i = 0; i < n_moves; i++) {
        if (!board.MakeMove(moveList[i])) {
            continue;
        }
        nodes += Perft(engine, board, depth - 1);
        board.UnmakeMove(moveList[i]);
    }
    return nodes;
} 

uint64_t PerftParallel(Engine& engine, Board& rootBoard, int depth) {
    if (depth == 0) return 1ULL;

    Move moveList[256];
    int n_moves = engine.GenerateAllMoves(rootBoard, moveList);
    uint64_t totalNodes = 0;

    #pragma omp parallel for reduction(+:totalNodes) schedule(dynamic)
    for (int i = 0; i < n_moves; i++) {
        Board threadBoard = rootBoard; 
        Move move = moveList[i];

        if (threadBoard.MakeMove(move)) {
            uint64_t nodesForThisMove = Perft(engine, threadBoard, depth - 1);
            totalNodes += nodesForThisMove;
        }
    }

    return totalNodes;
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

uint64_t PerftDivide(Engine& engine, Board& board, int depth) {
    if (depth == 0) return 1ULL;

    Move moveList[256];
    // Recuperiamo il numero esatto di mosse scritte nell'array
    int n_moves = engine.GenerateAllMoves(board, moveList); 
    uint64_t totalNodes = 0;

    std::cout << "\n--- PERFT DIVIDE (Depth " << depth << ") ---" << std::endl;

    for (int i = 0; i < n_moves; i++) {
        Move move = moveList[i];

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

bool assertUint64ArrayEqual(uint64_t* a, uint64_t* b, int len) {
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

int main() {
    LookupTables::init();
    Engine myEngine;
    Board gameBoard;

    gameBoard.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");

    PerftDivide(myEngine, gameBoard, 5);

    return 0;
}