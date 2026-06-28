#include <iostream>
#include <cmath>
#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>
// ---------------------------------------------------------
// HACK PER UNIT TESTING: Esponiamo i membri privati
// ---------------------------------------------------------
#define private public
#include "NnueAdapter.hpp"
#include "NnueNetwork.hpp"
#include "Board.hpp"
#undef private

#include "Move.hpp"
#include "Pieces.hpp"
#include "LookupTables.hpp"
// ---------------------------------------------------------
// UTILS PER I TEST
// ---------------------------------------------------------
void AssertFloatEq(double a, double b, const std::string& msg, double epsilon = 1e-5) {
    if (std::abs(a - b) > epsilon) {
        std::cerr << "[FALLITO] " << msg << " | Atteso: " << a << ", Ottenuto: " << b << "\n";
        exit(1);
    }
}

// ---------------------------------------------------------
// 6.1 TEST DEL FEATURE SET
// ---------------------------------------------------------
void TestFeatureSet() {
    std::cout << "--- Esecuzione 6.1: Test del Feature Set ---\n";
    std::unique_ptr<NnueNetwork> testNet = std::make_unique<NnueNetwork>();
    // 1. Verificare gli indici attesi
    // Pedone bianco (PAWNS = 0), colore WHITE, Re in E1 (4), Prospettiva WHITE
    int fIndexWhite = NnueAdapter::MakeFeatureIndex(12, PiecesEnum::PAWNS, WHITE, 4, WHITE);
    assert(fIndexWhite == 2572 && "Indice feature pedone bianco errato");

    // 2. Verificare la simmetria
    // Pedone nero (PAWNS = 0) in E7 (52), Re in E8 (60)
    // Prospettiva BLACK flippa verticalmente la casa e il re
    int fIndexBlack = NnueAdapter::MakeFeatureIndex(52, PiecesEnum::PAWNS, BLACK, 60, BLACK); 
    assert(fIndexWhite == fIndexBlack && "Simmetria feature non rispettata tra Bianco e Nero");

    // 3. Verificare il refresh sul movimento del Re
    Board board;
    board.InitializeBoard(); 
    NnueAdapter::Reset(board);
    assert(NnueAdapter::nnueStack[0].computed[WHITE] == true && "Accumulatore base non calcolato");
    
    Move kMove = createMove(4, 12, FlagMap::MOVE); // e1 -> e2
    bool isValid = board.MakeMove(kMove, true);
    assert(isValid && "ATTENZIONE: La mossa del re doveva essere legale!");
    
    NnueAdapter::OnMakeMove(board, kMove); 
    
    // Avendo giocato 3 mosse (e4, e5, Ke2), il ply dell'adapter ora è a 3!
    assert(NnueAdapter::nnueStack[3].computed[WHITE] == false && "Il movimento del re non ha invalidato l'accumulatore");

    NnueAdapter::SetEager(true);

    std::cout << "[OK] Test Feature Set completato.\n\n";
}

// ---------------------------------------------------------
// 6.2 TEST DELL'ACCUMULATORE
// ---------------------------------------------------------
void TestAccumulator() {
    std::cout << "--- Esecuzione 6.2: Test dell'Accumulatore ---\n";

    // Impostiamo pesi dummy casuali ma deterministici sulla rete reale
    for(int i = 0; i < NUM_FEATURES; i++) {
        for(int j = 0; j < M; j++) {
            NnueAdapter::network.L0[i][j] = (i + j) * 0.0001; 
        }
    }

    Board board;
    board.InitializeBoard();
    NnueAdapter::Reset(board);

    // Simuliamo una spinta doppia di pedone (e2-e4 -> da 12 a 28)
    Move e2e4 = createMove(12, 28, FlagMap::DMOVE);
    board.MakeMove(e2e4, true);
    NnueAdapter::OnMakeMove(board, e2e4);

    // Salviamo lo stato dell'accumulatore post-aggiornamento incrementale
    double accIncremental[M];
    std::memcpy(accIncremental, NnueAdapter::nnueStack[1].accumulator[WHITE], sizeof(double) * M);

    // Forziamo un refresh totale invalidando il computed flag
    NnueAdapter::nnueStack[1].computed[WHITE] = false; 
    NnueAdapter::RefreshAccumulator(board, WHITE); 
    
    // Confrontiamo i due risultati: Aggiornamento Incrementale vs Ricalcolo da zero
    for (int i = 0; i < M; i++) {
        AssertFloatEq(accIncremental[i], NnueAdapter::nnueStack[1].accumulator[WHITE][i], "Mismatch Accumulatore in pos " + std::to_string(i));
    }

    std::cout << "[OK] Test Accumulatore completato.\n\n";
}

// ---------------------------------------------------------
// 6.3 TEST DEL FORWARD PASS
// ---------------------------------------------------------
void TestForwardPass() {
    std::cout << "--- Esecuzione 6.3: Test del Forward Pass ---\n";

    std::unique_ptr<NnueNetwork> testNet = std::make_unique<NnueNetwork>();

    std::memset(testNet->L0, 0, sizeof(testNet->L0));
    std::memset(testNet->L0Bias, 0, sizeof(testNet->L0Bias));
    std::memset(testNet->L1, 0, sizeof(testNet->L1));
    std::memset(testNet->L1Bias, 0, sizeof(testNet->L1Bias));
    std::memset(testNet->L2, 0, sizeof(testNet->L2));
    testNet->L2Bias = 50.0; 

    double smt[M] = {0};
    double nsmt[M] = {0};

    // Forward pass
    int score = testNet->EvaluateNnue(smt, nsmt);
    
    int expectedScore = static_cast<int>(50.0 * 100.0);
    assert(score == expectedScore && "L'output del forward pass non corrisponde al bias atteso");

    // Test di determinismo sulla rete fissa
    int score2 = testNet->EvaluateNnue(smt, nsmt);
    assert(score == score2 && "L'inferenza NNUE non e' deterministica");

    std::cout << "[OK] Test Forward Pass completato.\n\n";
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main() {
    std::cout << "=== Avvio Test Suite NNUE (Integrazione Engine) ===\n\n";

    // Inizializza le maschere e le LookupTables necessarie al funzionamento della Board
    LookupTables::init();

    // Sblocchiamo l'esecuzione rimuovendo il disabilita per Perft
    NnueAdapter::disableForPerft = false;
    NnueAdapter::SetEager(true);

    TestFeatureSet();
    TestAccumulator();
    TestForwardPass();

    std::cout << "=== Tutti i test sono passati con successo! ===\n";
    return 0;
}