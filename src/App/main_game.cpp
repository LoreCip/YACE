#include <iostream>
#include <string>
#include <fstream>
#include <memory>
#include <functional>
#include <thread>

#include "LookupTables.hpp"
#include "Board.hpp"
#include "ClassicalEvaluator.hpp"
#include "NnueEvaluator.hpp"
#include "SearchManager.hpp"
#include "MoveGenerator.hpp"
#include "Uci.hpp"

int main(int argc, char** argv) {
    // Defaults
    std::string evalType = "hce";
    std::string weightsPath = "/home/lorenzo/Scrivania/Projects/YACE/parameters/weights/nnue_weights_v2.bin";
    std::string evalsPath = "/home/lorenzo/Scrivania/Projects/YACE/parameters/evals.txt";
    int ttSize = 64;
    
    int numThreads = 1; 
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads > 0) numThreads = hardware_threads / 2;

    // IO parser
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << "YACE - Yet Another Chess Engine \n";
            std::cout << "Usage: \n";
            std::cout << argv[0] << " [-e hce|nnue] [-p pst_path] [-w weights_path] [-t tt_size_mb] [-c threads]\n";
            return 0;
        } else if (arg == "-e" && i + 1 < argc) {
            evalType = argv[++i];
        } else if  (arg == "-p" && i + 1 < argc) {
            evalsPath = argv[++i];
        } else if (arg == "-w" && i + 1 < argc) {
            weightsPath = argv[++i];
        } else if (arg == "-t" && i + 1 < argc) {
            ttSize = std::stoi(argv[++i]);
        } else if (arg == "-c" && i + 1 < argc) {
            numThreads = std::stoi(argv[++i]);
        }
    }

    std::cout << std::unitbuf;

    auto board = std::make_unique<Board>();
    
    UCI::Initialize();
    LookupTables::init(evalsPath);
    board->InitializeBoard();

    std::unique_ptr<IEvaluator> rootEvaluator;
    bool useNnue = (evalType == "nnue");

    if (useNnue) {
        auto tempNnue = std::make_unique<NnueEvaluator>();
        if (tempNnue->Initialize(weightsPath)) {
            UCI::ReportDebugString("NNUE caricata con successo.");
            rootEvaluator = std::move(tempNnue);
        } else {
            UCI::ReportDebugString("NNUE non trovata.");
            return 1;
        }
    } else {
        UCI::ReportDebugString("Valutazione Classica (HCE).");
        rootEvaluator = std::make_unique<ClassicalEvaluator>();
    }

    rootEvaluator->Reset(*board);

    auto evalFactory = [&]() -> std::unique_ptr<IEvaluator> {
        if (useNnue) {
            auto nnue = std::make_unique<NnueEvaluator>();
            nnue->Initialize(weightsPath); // Carica i pesi per la rete del thread locale
            return nnue;
        }
        return std::make_unique<ClassicalEvaluator>();
    };

    UCI::ReportDebugString("Avvio motore con " + std::to_string(numThreads) + " threads e " + std::to_string(ttSize) + "MB di TT.");
    SearchManager searchManager(ttSize, numThreads, evalFactory);
    
    // --- Callbacks UCI ---
    auto handlePositionLoaded = [&]() {
        rootEvaluator->Reset(*board);
    };

    auto handleMoveMade = [&](Move move) {
        rootEvaluator->OnMakeMove(*board, move);
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

        Move best = searchManager.Search(*board, targetDepth, allocatedTimeMs, engineReporter);
        return best;
    };

    UCI::Loop(*board, handleGoCommand, handlePositionLoaded, handleMoveMade);

    return 0;
}