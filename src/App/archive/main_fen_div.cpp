#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <omp.h>

#include "LookupTables.hpp"
#include "Move.hpp"
#include "Board.hpp"
#include "MoveGenerator.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <omp.h>

#include "LookupTables.hpp"
#include "Move.hpp"
#include "Board.hpp"
#include "MoveGenerator.hpp"


uint64_t Perft(Board& board, int depth) {
    if (depth == 0) return 1ULL;

    MoveList moveList;
    MoveGen::GenerateAllMoves(board, moveList);
    uint64_t nodes = 0;

    for (int i = 0; i < moveList.count; i++) {
        if (!board.MakeMove(moveList.moves[i])) {
            continue;
        }
        nodes += Perft(board, depth - 1);
        board.UnmakeMove(moveList.moves[i]);
    }
    return nodes;
} 


void PerftDivide(Board& rootBoard, int depth) {
    if (depth == 0) return;

    MoveList moveList;
    MoveGen::GenerateAllMoves(rootBoard, moveList);
    uint64_t totalNodes = 0;

    std::cout << "--- PERFT DIVIDE (Depth " << depth << ") ---\n";

    for (int i = 0; i < moveList.count; i++) {
        Board threadBoard = rootBoard;
        Move move = moveList.moves[i];

        if (threadBoard.MakeMove(move)) {
            uint64_t nodesForThisMove = Perft(threadBoard, depth - 1);
            
            // Decodifica la mossa per stamparla (es. e2e4)
            int from = getMoveFrom(move); // Usando le tue funzioni o macro esistenti
            int to = getMoveTo(move);
            std::string moveName = "";
            moveName += (char)('a' + (from % 8));
            moveName += (char)('1' + (from / 8));
            moveName += (char)('a' + (to % 8));
            moveName += (char)('1' + (to / 8));
            
            // Se hai la promozione, andrebbe aggiunta la lettera, ma per la posizione iniziale non serve.

            std::cout << moveName << ": " << nodesForThisMove << std::endl;
            totalNodes += nodesForThisMove;
        }
    }

    std::cout << "---------------------------\n";
    std::cout << "NODI TOTALI: " << totalNodes << std::endl;
}


bool assertUint64ArrayEqual(uint64_t* a, uint64_t* b, int len) {
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

int main() {
    LookupTables::init("/home/lorenzo/Scrivania/Projects/YACE/parameters/evals.txt"); 
    Board gameBoard;

    // Carichiamo la posizione iniziale
    gameBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    // Lanciamo il Divide a profondità 4
    PerftDivide(gameBoard, 4);

    return 0;
}