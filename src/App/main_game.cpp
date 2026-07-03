#include <iostream>
#include <string>
#include <fstream>
#include <memory>

#include "LookupTables.hpp"
#include "Board.hpp"
#include "TranspositionTable.hpp"
#include "ClassicalEvaluator.hpp"
#include "NnueEvaluator.hpp"
#include "Engine.hpp"
#include "MoveGenerator.hpp"
#include "Uci.hpp"

int main(int argc, char** argv) {
    // Defaults
    std::string evalType = "hce";
    std::string weightsPath = "/home/lorenzo/Scrivania/Projects/YACE/parameters/weights/nnue_weights_v1.bin";
    std::string evalsPath = "/home/lorenzo/Scrivania/Projects/YACE/parameters/evals.txt";
    int ttSize = 64;

    // IO parser
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << "YACE - Yet Another Chess Engine \n";
            std::cout << "Usage: \n";
            std::cout << argv[0] << " [-e hce|nnue] [-p pst_path] [-w weights_path] [-t tt_size_mb]\n";
            return 0;
        } else if (arg == "-e" && i + 1 < argc) {
            evalType = argv[++i];
        } else if  (arg == "-p" && i + 1 < argc) {
            evalsPath = argv[++i];
        } else if (arg == "-w" && i + 1 < argc) {
            weightsPath = argv[++i];
        } else if (arg == "-t" && i + 1 < argc) {
            ttSize = std::stoi(argv[++i]);
        }
    }

    std::cout << std::unitbuf;

    auto board = std::make_unique<Board>();
    auto tt = std::make_unique<TranspositionTable>(ttSize);
    auto hce = std::make_unique<ClassicalEvaluator>();
    auto nnue = std::make_unique<NnueEvaluator>();
    IEvaluator* activeEvaluator = nullptr;


    UCI::Initialize();
    LookupTables::init(evalsPath);
    board->InitializeBoard();


    if (evalType == "nnue") {
        if (nnue->Initialize(weightsPath)) {
            UCI::ReportDebugString("NNUE caricata con successo.");
            nnue->Reset(*board);
            activeEvaluator = nnue.get();
        } else {
            UCI::ReportDebugString("NNUE non trovata. Fallback alla Valutazione Classica (HCE).");
            activeEvaluator = hce.get();
        }
    } else {
        UCI::ReportDebugString("Valutazione Classica (HCE) forzata da riga di comando.");
        activeEvaluator = hce.get();
    }

    auto engine = std::make_unique<Engine>(*tt, activeEvaluator);
    bool enableLogging = true;
    
    auto handlePositionLoaded = [&]() {
        if (activeEvaluator) activeEvaluator->Reset(*board);
    };

    auto handleMoveMade = [&](Move move) {
        if (activeEvaluator) activeEvaluator->OnMakeMove(*board, move);
    };

    auto handleGoCommand = [&](int targetDepth, double allocatedTimeMs, UCI::LoggerCallback uciLogger) -> Move {
        
        InfoReporter engineReporter = [&](int depth, int score, const SearchStats& stats, Move bestMove) {
            std::string moveStr = UCI::MoveToString(bestMove);
            uciLogger(
                depth, stats.selDepthReached, score,
                stats.lastMoveTimeMs,
                stats.GetTotalNodes(), stats.GetNPS(),
                static_cast<int>(stats.hashFullness * 10), 
                moveStr,
                stats.GetMoveOrderingQuality(), 
                stats.ttHits, stats.qNodesEvaluated
            );
        };

        Move best = engine->GetBestMove(*board, targetDepth, allocatedTimeMs, engineReporter);
        return best;
    };

    UCI::Loop(*board, handleGoCommand, handlePositionLoaded, handleMoveMade);

    return 0;
}