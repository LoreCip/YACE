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

uint64_t PerftParallel(Board& rootBoard, int depth) {
    if (depth == 0) return 1ULL;

    MoveList moveList;
    MoveGen::GenerateAllMoves(rootBoard, moveList);
    uint64_t totalNodes = 0;

    
    #pragma omp parallel for reduction(+:totalNodes) schedule(dynamic)
    for (int i = 0; i < moveList.count; i++) {
        Board threadBoard = rootBoard; 
        Move move = moveList.moves[i];

        if (threadBoard.MakeMove(move)) {
            uint64_t nodesForThisMove = Perft(threadBoard, depth - 1);
            totalNodes += nodesForThisMove;
        }
    }

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
    Board gameBoard;

    /* https://www.chessprogramming.org/Perft_Results */
    std::vector<std::string> fenList = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1 ",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1 ",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ",
    };

    std::vector<std::vector<uint64_t>> truthTable = {
        {20, 400, 8902, 197281, 4865609, 119060324, 3195901860ULL},
        {48, 2039, 97862, 4085603, 193690690, 8031647685ULL},
        {14, 191, 2812, 43238, 674624, 11030083, 178633661ULL},
        {6, 264, 9467, 422333, 15833292, 706045033ULL},
        {6, 264, 9467, 422333, 15833292, 706045033ULL},
        {44, 1486, 62379, 2103487, 89941194ULL},
        {46, 2079, 89890, 3894594, 164075551ULL}
    };


    std::cout << "--- AVVIO TEST DI VALIDAZIONE ENGINE ---\n" << std::endl;
    int failedTestsCount = 0;

    for (size_t f = 0; f < fenList.size(); f++) {
        std::string currentFen = fenList[f];
        std::cout << "Testando FEN [" << f << "]: " << currentFen << std::endl;

        gameBoard.InitializeFromFEN(currentFen);

        int maxDepths = truthTable[f].size();
        std::vector<uint64_t> currentCounts(maxDepths, 0);
        std::vector<uint64_t> expectedCounts(maxDepths, 0);

        for (int i = 0; i < maxDepths; i++) {
            int targetDepth = i + 1;
            currentCounts[i] = PerftParallel(gameBoard, targetDepth);
            expectedCounts[i] = truthTable[f][i];
            
            std::cout << "  > Depth " << targetDepth << " -> Risultato: " << currentCounts[i] 
                      << " | Atteso: " << expectedCounts[i] << std::endl;
        }

        if (assertUint64ArrayEqual(currentCounts.data(), expectedCounts.data(), maxDepths)) {
            std::cout << ">> STATO FEN [" << f << "]: PASSED\n" << std::endl;
        } else {
            std::cerr << ">> STATO FEN [" << f << "]: FAILED! Discrepanza nel conteggio dei nodi.\n" << std::endl;
            failedTestsCount++;
        }
    }

    std::cout << "--- RESOCONTO FINALE SUITE ---" << std::endl;
    if (failedTestsCount == 0) {
        std::cout << "TUTTI I TEST SONO PASSED CON SUCCESSO!" << std::endl;
    } else {
        std::cout << "Suite completata con " << failedTestsCount << " fallimenti su " << fenList.size() << " FEN testati." << std::endl;
    }

    return 0;
}