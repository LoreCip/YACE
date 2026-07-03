#include <iostream>
#include <cmath>
#include <cassert>
#include <cstring>
#include <memory>
#include <sstream>

// ---------------------------------------------------------
// HACK PER UNIT TESTING: 
// ---------------------------------------------------------
#define private public
#include "NnueEvaluator.hpp" 
#include "NnueNetwork.hpp"
#include "Board.hpp"
#undef private

#include "Move.hpp"
#include "Types.hpp" 
#include "LookupTables.hpp"

const int WHITE_IDX = static_cast<int>(Color::WHITE);
const int BLACK_IDX = static_cast<int>(Color::BLACK);

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
    
    // Istanziamo l'oggetto al posto del vecchio Adapter statico
    NnueEvaluator nnue; 

    // 1. Verificare gli indici attesi
    // Pedone bianco, colore WHITE, Re in E1 (4), Prospettiva WHITE
    int fIndexWhite = nnue.MakeFeatureIndex(12, PieceType::PAWN, WHITE_IDX, 4, WHITE_IDX);
    assert(fIndexWhite == 2572 && "Indice feature pedone bianco errato");

    // 2. Verificare la simmetria
    // Pedone nero in E7 (52), Re in E8 (60), Prospettiva BLACK
    int fIndexBlack = nnue.MakeFeatureIndex(52, PieceType::PAWN, BLACK_IDX, 60, BLACK_IDX); 
    assert(fIndexWhite == fIndexBlack && "Simmetria feature non rispettata tra Bianco e Nero");

    // 3. Verificare il refresh sul movimento del Re
    Board board;
    board.InitializeBoard(); 
    nnue.Reset(board);
    assert(nnue.nnueStack[0].computed[WHITE_IDX] == true && "Accumulatore base non calcolato");
    
    // Forziamo una mossa del Re (e1 -> e2)
    Move kMove = createMove(4, 12, FlagMap::MOVE); 
    bool isValid = board.MakeMove(kMove); // Niente più 'true' come secondo parametro
    assert(isValid && "ATTENZIONE: La mossa del re doveva essere legale!");
    
    nnue.OnMakeMove(board, kMove); 
    
    // Avendo giocato 1 mossa (Ke2), il ply dell'adapter ora è a 1.
    assert(nnue.nnueStack[1].computed[WHITE_IDX] == false && "Il movimento del re non ha invalidato l'accumulatore");

    nnue.SetEager(true);

    std::cout << "[OK] Test Feature Set completato.\n\n";
}

// ---------------------------------------------------------
// 6.2 TEST DELL'ACCUMULATORE
// ---------------------------------------------------------
void TestAccumulator() {
    std::cout << "--- Esecuzione 6.2: Test dell'Accumulatore ---\n";

    NnueEvaluator nnue;

    // Impostiamo pesi dummy casuali ma deterministici sulla rete reale dell'oggetto
    for(int i = 0; i < NUM_FEATURES; i++) {
        for(int j = 0; j < M; j++) {
            nnue.network.L0[i][j] = (i + j) * 0.0001; 
        }
    }

    Board board;
    board.InitializeBoard();
    nnue.Reset(board);

    // Simuliamo una spinta doppia di pedone (e2-e4 -> da 12 a 28)
    Move e2e4 = createMove(12, 28, FlagMap::DMOVE);
    board.MakeMove(e2e4);
    nnue.OnMakeMove(board, e2e4);

    // Salviamo lo stato dell'accumulatore post-aggiornamento incrementale
    double accIncremental[M];
    std::memcpy(accIncremental, nnue.nnueStack[1].accumulator[WHITE_IDX], sizeof(double) * M);

    // Forziamo un refresh totale invalidando il computed flag
    nnue.nnueStack[1].computed[WHITE_IDX] = false; 
    nnue.RefreshAccumulator(board, WHITE_IDX); 
    
    // Confrontiamo i due risultati: Aggiornamento Incrementale vs Ricalcolo da zero
    for (int i = 0; i < M; i++) {
        AssertFloatEq(accIncremental[i], nnue.nnueStack[1].accumulator[WHITE_IDX][i], "Mismatch Accumulatore in pos " + std::to_string(i));
    }

    std::cout << "[OK] Test Accumulatore completato.\n\n";
}

// ---------------------------------------------------------
// 6.3 TEST DEL FORWARD PASS
// ---------------------------------------------------------
void TestForwardPass() {
    std::cout << "--- Esecuzione 6.3: Test del Forward Pass ---\n";

    NnueNetwork testNet; // Basta istanziare la rete nuda e cruda

    std::memset(testNet.L0, 0, sizeof(testNet.L0));
    std::memset(testNet.L0Bias, 0, sizeof(testNet.L0Bias));
    std::memset(testNet.L1, 0, sizeof(testNet.L1));
    std::memset(testNet.L1Bias, 0, sizeof(testNet.L1Bias));
    std::memset(testNet.L2, 0, sizeof(testNet.L2));
    testNet.L2Bias = 50.0; 

    double smt[M] = {0};
    double nsmt[M] = {0};

    // Forward pass
    int score = testNet.EvaluateNnue(smt, nsmt);
    
    int expectedScore = static_cast<int>(50.0 * 100.0);
    assert(score == expectedScore && "L'output del forward pass non corrisponde al bias atteso");

    // Test di determinismo sulla rete fissa
    int score2 = testNet.EvaluateNnue(smt, nsmt);
    assert(score == score2 && "L'inferenza NNUE non e' deterministica");

    std::cout << "[OK] Test Forward Pass completato.\n\n";
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main() {
    std::cout << "=== Avvio Test Suite NNUE (Integrazione Engine) ===\n\n";
    LookupTables::init("/home/lorenzo/Scrivania/Projects/YACE/parameters/evals.txt");

    TestFeatureSet();
    TestAccumulator();
    TestForwardPass();

    std::cout << "=== Tutti i test sono passati con successo! ===\n";
    return 0;
}