#include "Uci.hpp"
#include "MoveGenerator.hpp"
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace UCI {

    static const char* const BENCH_FENS[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 4 5",
        "rnbqkb1r/pp1p1ppp/4pn2/2p5/2PP4/5N2/PP2PPPP/RNBQKB1R w KQkq - 0 4",
        "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 6 8",
        "2rq1rk1/pb1n1ppp/1p2pn2/8/2BP4/2N1PN2/PP3PPP/2RQ1RK1 w - - 0 14",
        "r1b1kb1r/pp3ppp/1qn1pn2/2ppN3/3P4/2N1P3/PPP1BPPP/R1BQK2R w KQkq - 0 8",
        "rnb1kb1r/ppp1pppp/5n2/3q4/8/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 2 5",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "2kr3r/pp1n1ppp/2p1p3/q7/1b1P4/2N1PN2/PPQ2PPP/2KR1B1R w - - 0 13",
        "5rk1/1p3ppp/p1p5/3Nn3/4P3/1P6/P4PPP/3R2K1 w - - 0 22",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "8/8/8/8/8/5k2/8/4K2R w K - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
        "4k3/8/8/8/8/8/8/4K2R w K - 0 1",
        "8/P7/8/8/8/8/p7/K6k w - - 0 1",
        "6k1/5p2/6p1/8/7P/6P1/5PK1/8 w - - 0 1",
    };

    static constexpr double INFINITE_TIME_MS = 99999999.0;
    static constexpr int MAX_SEARCH_DEPTH = 100;
    static constexpr int DEFAULT_BENCH_DEPTH = 8;

    static std::string MoveToString(Move move) {
        if (move == 0) return "0000"; // Mossa nulla

        int from = getMoveFrom(move);
        int to = getMoveTo(move);

        std::string moveStr = {char('a' + from % 8), char('1' + from / 8),
                               char('a' + to % 8),   char('1' + to / 8)};

        PieceType promo = promotionPieceFromFlags(getMoveFlags(move));
        if (promo != PieceType::NONE) moveStr += PIECE_CHARS[PieceInt(promo)];

        return moveStr;
    }

    // Ritorna 0 se la stringa non corrisponde a nessuna mossa pseudo-legale della posizione.
    static Move StringToMove(const Board& board, const std::string& moveStr) {
        MoveList moveList;
        MoveGen::GenerateAllMoves(board, moveList);
        for (int i = 0; i < moveList.count; i++) {
            if (MoveToString(moveList.moves[i]) == moveStr) return moveList.moves[i];
        }
        return 0;
    }

    void Initialize() {
        std::cout << "id name YACE v1.0\n";
        std::cout << "id author LoreChip\n";
    }

    void ReportDebugString(const std::string& message) {
        std::cout << "info string " << message << "\n" << std::flush;
    }

    static void ReportSearchInfo(int depth, int score, const SearchStats& stats, Move bestMove) {
        std::cout << "info "
                  << "depth " << depth << " "
                  << "seldepth " << stats.selDepthReached << " "
                  << "score cp " << score << " "
                  << "time " << static_cast<long long>(stats.lastMoveTimeMs) << " "
                  << "nodes " << stats.GetTotalNodes() << " "
                  << "nps " << stats.GetNPS() << " "
                  << "hashfull " << static_cast<int>(stats.hashFullness * 10) << " "
                  << "pv " << MoveToString(bestMove)
                  << "\n";

        std::cout << "info string "
                  << "MoveOrder: " << std::fixed << std::setprecision(1) << stats.GetMoveOrderingQuality() << "% | "
                  << "TTHits: " << stats.ttHits << " | "
                  << "QNodes: " << stats.qNodesEvaluated
                  << "\n" << std::flush;
    }

    // TT vuota a ogni nuova posizione: `bench` riproducibile, indipendente dall'ordine.
    static void LoadPosition(Board& board, SearchManager& search, const std::string& fen) {
        board.InitializeFromFEN(fen);
        search.ClearTT();
    }

    static void HandleFen(Board& board, SearchManager& search) {
        std::string fen;
        std::getline(std::cin >> std::ws, fen);
        LoadPosition(board, search, fen);
    }

    // go [movetime ms] [wtime ms btime ms] [depth n]
    static void HandleGo(Board& board, SearchManager& search) {
        std::string line;
        std::getline(std::cin, line);
        std::stringstream ss(line);
        std::string arg;

        double allocatedTimeMs = 1000.0;
        int wtime = -1, btime = -1, depth = MAX_SEARCH_DEPTH;

        while (ss >> arg) {
            if (arg == "movetime") ss >> allocatedTimeMs;
            else if (arg == "wtime") ss >> wtime;
            else if (arg == "btime") ss >> btime;
            else if (arg == "depth") ss >> depth;
        }

        if (wtime != -1 && btime != -1) {
            int myTime = (board.GetSideToMove() == Color::WHITE) ? wtime : btime;
            allocatedTimeMs = std::max(myTime / 30.0, 50.0);
        } else if (allocatedTimeMs == -1.0) {
            allocatedTimeMs = INFINITE_TIME_MS;
        }

        Move best = search.Search(board, depth, allocatedTimeMs, ReportSearchInfo);
        if (best != 0) board.MakeMove(best);

        std::cout << "bestmove " << MoveToString(best) << "\n" << std::flush;
    }

    // FNV-1a a 64 bit, byte per byte.
    static void FoldFnv1a(uint64_t& hash, long long value) {
        for (int b = 0; b < 8; b++) {
            hash ^= (static_cast<uint64_t>(value) >> (b * 8)) & 0xFFULL;
            hash *= 1099511628211ULL;
        }
    }

    // bench [depth]: nodi, nps e firma di (profondita', score, bestmove) su BENCH_FENS.
    // Un'ottimizzazione di velocita' non deve cambiare la firma.
    static void HandleBench(Board& board, SearchManager& search) {
        std::string line;
        std::getline(std::cin, line);
        std::stringstream ss(line);
        int depth = DEFAULT_BENCH_DEPTH;
        ss >> depth;
        if (depth <= 0) depth = DEFAULT_BENCH_DEPTH;

        long long totalNodes = 0;
        double totalTimeMs = 0.0;
        uint64_t signature = 14695981039346656037ULL; // offset basis FNV-1a

        for (const char* fen : BENCH_FENS) {
            LoadPosition(board, search, fen);

            long long lastNodes = 0, lastTimeMs = 0;
            int lastScore = 0;
            auto capture = [&](int, int score, const SearchStats& stats, Move) {
                lastScore = score;
                lastNodes = stats.GetTotalNodes();
                lastTimeMs = static_cast<long long>(stats.lastMoveTimeMs);
            };
            Move best = search.Search(board, depth, INFINITE_TIME_MS, capture);

            totalNodes += lastNodes;
            totalTimeMs += lastTimeMs;

            FoldFnv1a(signature, depth);
            FoldFnv1a(signature, lastScore);
            FoldFnv1a(signature, best);
        }

        long long nps = totalTimeMs > 0.0 ? static_cast<long long>(totalNodes / totalTimeMs * 1000.0) : 0;

        std::cout << "bench: " << totalNodes << " nodes  " << totalTimeMs << " ms  " << nps << " nps\n";
        std::cout << "bench signature: " << std::hex << signature << std::dec << "\n" << std::flush;
    }

    static void HandleMove(Board& board) {
        std::string moveStr;
        std::cin >> moveStr;
        Move move = StringToMove(board, moveStr);
        if (move == 0) {
            ReportDebugString("move not valid: " + moveStr);
        } else if (!board.MakeMove(move)) {
            ReportDebugString("illegal move");
        }
    }

    void Loop(Board& board, SearchManager& search) {
        std::string token;
        while (std::cin >> token && token != "quit") {
            if      (token == "fen")   HandleFen(board, search);
            else if (token == "go")    HandleGo(board, search);
            else if (token == "bench") HandleBench(board, search);
            else if (token == "move")  HandleMove(board);
            else if (token == "state") std::cout << "info fen: " << board.GetFEN() << "\n";
            else ReportDebugString("command not recognised, no action taken");
        }
    }
}
