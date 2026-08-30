#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "Board.hpp"
#include "ClassicalEvaluator.hpp"
#include "LookupTables.hpp"
#include "MoveGenerator.hpp"
#include "NnueEvaluator.hpp"
#include "SearchManager.hpp"
#include "Uci.hpp"

namespace {

struct EngineOptions {
  std::string evalType = "hce";
  std::string weightsPath;
  std::string evalsPath;
  int ttSizeMb = 64;
  int numThreads = 1;
};

int DefaultThreadCount() {
  unsigned int hardwareThreads = std::thread::hardware_concurrency();
  return hardwareThreads > 0 ? static_cast<int>(hardwareThreads / 2) : 1;
}

void PrintUsage(const char *programName) {
  std::cout << "YACE - Yet Another Chess Engine \n";
  std::cout << "Usage: \n";
  std::cout << programName
            << " [-e hce|nnue] [-p pst_path] [-w weights_path] [-t tt_size_mb] "
               "[-c threads]\n";
}

// Returns false if execution should stop immediately (e.g. -h/--help was
// requested).
bool ParseArgs(int argc, char **argv, EngineOptions &options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    bool hasValue = i + 1 < argc;

    if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return false;
    } else if (arg == "-e" && hasValue) {
      options.evalType = argv[++i];
    } else if (arg == "-p" && hasValue) {
      options.evalsPath = argv[++i];
    } else if (arg == "-w" && hasValue) {
      options.weightsPath = argv[++i];
    } else if (arg == "-t" && hasValue) {
      options.ttSizeMb = std::stoi(argv[++i]);
    } else if (arg == "-c" && hasValue) {
      options.numThreads = std::stoi(argv[++i]);
    }
  }
  return true;
}

// Builds and initializes the evaluator used for the main/root search thread.
// `sharedNetwork` is null when evalType != "nnue"; otherwise it must already be loaded
// (see main(): the network is loaded exactly once and shared with MakeEvaluatorFactory).
std::unique_ptr<IEvaluator> CreateRootEvaluator(
    const EngineOptions &options,
    const std::shared_ptr<const NnueNetwork> &sharedNetwork) {
  if (options.evalType != "nnue") {
    UCI::ReportDebugString("Valutazione Classica (HCE).");
    return std::make_unique<ClassicalEvaluator>();
  }
  return std::make_unique<NnueEvaluator>(sharedNetwork);
}

// Factory handed to SearchManager so each search thread gets its own evaluator
// instance — but ALL instances share the same read-only NnueNetwork (`sharedNetwork`),
// instead of each thread re-reading the ~21MB weights file from disk and holding its own
// private copy on every `go` command.
EvaluatorFactory MakeEvaluatorFactory(const EngineOptions &options,
                                      std::shared_ptr<const NnueNetwork> sharedNetwork) {
  bool useNnue = (options.evalType == "nnue");

  return [useNnue, sharedNetwork]() -> std::unique_ptr<IEvaluator> {
    if (!useNnue) {
      return std::make_unique<ClassicalEvaluator>();
    }
    return std::make_unique<NnueEvaluator>(sharedNetwork);
  };
}

// Wraps the search's raw stats into the shape UCI::LoggerCallback expects.
InfoReporter MakeUciInfoReporter(UCI::LoggerCallback uciLogger) {
  return [uciLogger](int depth, int score, const SearchStats &stats,
                     Move bestMove) {
    std::string moveStr = UCI::MoveToString(bestMove);
    uciLogger(depth, stats.selDepthReached, score, stats.lastMoveTimeMs,
              stats.GetTotalNodes(), stats.GetNPS(),
              static_cast<int>(stats.hashFullness * 10), moveStr,
              stats.GetMoveOrderingQuality(), stats.ttHits,
              stats.qNodesEvaluated);
  };
}

} // namespace

int main(int argc, char **argv) {
  EngineOptions options;
  options.numThreads = DefaultThreadCount();

  if (!ParseArgs(argc, argv, options)) {
    return 0;
  }

  std::cout << std::unitbuf;

  auto board = std::make_unique<Board>();
  UCI::Initialize();
  LookupTables::init(options.evalsPath);
  board->InitializeBoard();

  // Caricata UNA volta e condivisa (root + tutti i thread di ricerca) invece di essere
  // riletta da disco e duplicata per ogni thread a ogni `go` (vedi MakeEvaluatorFactory).
  std::shared_ptr<const NnueNetwork> sharedNetwork;
  if (options.evalType == "nnue") {
    sharedNetwork = LoadNnueNetwork(options.weightsPath);
    if (!sharedNetwork) {
      UCI::ReportDebugString("NNUE non trovata.");
      return 1;
    }
    UCI::ReportDebugString("NNUE caricata con successo.");
  }

  std::unique_ptr<IEvaluator> rootEvaluator =
      CreateRootEvaluator(options, sharedNetwork);
  rootEvaluator->Reset(*board);

  UCI::ReportDebugString("Avvio motore con " +
                         std::to_string(options.numThreads) + " threads e " +
                         std::to_string(options.ttSizeMb) + "MB di TT.");
  SearchManager searchManager(options.ttSizeMb, options.numThreads,
                              MakeEvaluatorFactory(options, sharedNetwork));

  // --- Callbacks UCI ---
  // Reset dell'accumulatore NNUE E svuotamento della TT: necessario per il comando `bench`
  // (misure riproducibili, indipendenti dall'ordine delle posizioni), innocuo per `fen` normale.
  auto handlePositionLoaded = [&]() {
    rootEvaluator->Reset(*board);
    searchManager.ClearTT();
  };

  auto handleMoveMade = [&](Move move) {
    rootEvaluator->OnMakeMove(*board, move);
  };

  auto handleGoCommand = [&](int targetDepth, double allocatedTimeMs,
                             UCI::LoggerCallback uciLogger) -> Move {
    InfoReporter engineReporter = MakeUciInfoReporter(uciLogger);
    return searchManager.Search(*board, targetDepth, allocatedTimeMs,
                                engineReporter);
  };

  UCI::Loop(*board, handleGoCommand, handlePositionLoaded, handleMoveMade);

  return 0;
}
