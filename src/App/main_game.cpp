#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "Board.hpp"
#include "Evaluation.hpp"
#include "LookupTables.hpp"
#include "NnueNetwork.hpp"
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

// false = esci subito (-h/--help).
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

} // namespace

int main(int argc, char **argv) {
  EngineOptions options;
  options.numThreads = DefaultThreadCount();

  if (!ParseArgs(argc, argv, options)) {
    return 0;
  }

  std::cout << std::unitbuf;

  Board board;
  UCI::Initialize();
  Evaluation::LoadFromFile(options.evalsPath);
  LookupTables::init();
  board.InitializeBoard();

  // Caricata una volta e condivisa da tutti i thread; nullptr = HCE.
  std::shared_ptr<const NnueNetwork> nnueNetwork;
  if (options.evalType == "nnue") {
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
  SearchManager searchManager(options.ttSizeMb, options.numThreads, nnueNetwork);

  UCI::Loop(board, searchManager);

  return 0;
}
