#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "Board.hpp"
#include "ClassicalEvaluator.hpp"
#include "Evaluation.hpp"
#include "LookupTables.hpp"
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

// Factory handed to SearchManager so each search thread gets its own evaluator
// instance — but ALL instances share the same read-only NnueNetwork (`sharedNetwork`),
// instead of each thread re-reading the ~21MB weights file from disk and holding its own
// private copy on every `go` command.
EvaluatorFactory MakeEvaluatorFactory(bool useNnue, 
                                      std::shared_ptr<const NnueNetwork> nnueNetwork) {
    return [useNnue, nnueNetwork = std::move(nnueNetwork)]()
               -> std::unique_ptr<IEvaluator>
    {
        if (useNnue) {
            return std::make_unique<NnueEvaluator>(nnueNetwork);
        }

        return std::make_unique<ClassicalEvaluator>();
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
  Evaluation::LoadFromFile(options.evalsPath);
  LookupTables::init();
  board->InitializeBoard();

  // Caricata UNA volta e condivisa da tutti i thread di ricerca invece di essere
  // riletta da disco e duplicata per ogni thread a ogni `go` (vedi MakeEvaluatorFactory).
  // std::shared_ptr<const NnueNetwork> nnueNetwork;
  const bool useNnue = options.evalType == "nnue";

  std::shared_ptr<const NnueNetwork> nnueNetwork;
  if (useNnue) {
    nnueNetwork = LoadNnueNetwork(options.weightsPath);
    if (!nnueNetwork) {
      UCI::ReportDebugString("NNUE non trovata.");
      return 1;
    }
    UCI::ReportDebugString("NNUE caricata con successo.");
  } else {
    UCI::ReportDebugString("Valutazione Classica (HCE).");
  }

  UCI::ReportDebugString("Avvio motore con " +
                         std::to_string(options.numThreads) + " threads e " +
                         std::to_string(options.ttSizeMb) + "MB di TT.");
  SearchManager searchManager(options.ttSizeMb, options.numThreads,
                              MakeEvaluatorFactory(useNnue, nnueNetwork));

  // Svuotare la TT a ogni nuova posizione rende `bench` riproducibile (indipendente
  // dall'ordine delle posizioni); innocuo per un `fen` normale.
  auto handlePositionLoaded = [&]() { searchManager.ClearTT(); };

  auto handleGoCommand = [&](int targetDepth, double allocatedTimeMs,
                             InfoReporter reporter) -> Move {
    return searchManager.Search(*board, targetDepth, allocatedTimeMs, reporter);
  };

  UCI::Loop(*board, handleGoCommand, handlePositionLoaded);

  return 0;
}
