#ifndef NNUE_NETWORK_HPP
#define NNUE_NETWORK_HPP

#include "NnueConstants.hpp"
#include <string>

/*

NNUE BINARY WEIGHTS FILE FORMAT (BASELINE DOUBLE)

LOGICAL SPECIFICATIONS
    Element Type: 64-bit float (IEEE 754 Double Precision, C++ double)
    Endianness  : Little-Endian (x86_64 native)
    Layout L0   : Column-Major [NUM_FEATURES][M] (Contiguous neurons per feature)
    Layout L1/L2: Row-Major (Contiguous inputs per output neuron for BLAS)
    Dimensions  : NUM_FEATURES = 40960 | M = 256 | K = 32

BINARY FILE MAP (RAW SEQUENTIAL BYTES)
    The file contains NO header or metadata. It consists of 6 contiguous blocks:
    BLOCK 1:
        L0 Bias (Feature Transformer Bias)
        Type: 1D Array [M]
        Data: 256 doubles (2,048 bytes)
    BLOCK 2: 
        L0 Weights (Feature Transformer Weights)
        Type: 2D Matrix [NUM_FEATURES][M] (Column-Major)
        Data: 40.960 * 256 = 10.485.760 doubles (83.886.080 bytes)
        Order: 256 neurons for Feature 0, then 256 for Feature 1, up to 40959
    BLOCK 3: 
        L1 Bias (Hidden Layer Bias)
        Type: 1D Array [K]
        Data: 32 doubles (256 bytes)
    BLOCK 4: 
        L1 Weights (Hidden Layer Weights)
        Type: 2D Matrix [K][2*M] (Row-Major)
        Data: 32 * 512 = 16.384 doubles (131.072 bytes)
        Order: 512 inputs for Hidden Neuron 0, then 512 for Neuron 1, up to 31
    BLOCK 5:
        L2 Bias (Output Layer Bias)
        Type: Scalar
        Data: 1 double (8 bytes)
    BLOCK 6:
        L2 Weights (Output Layer Weights)
        Type: 1D Array [K]
        Data: 32 doubles (256 bytes)
*/

#pragma once

#include <string>
#include <cstdint>
#include <memory>

// alignas(64): tutti gli accessi SIMD a L0/L1 nel percorso caldo (AccumulateColumnsInto,
// IncrementalUpdate, RefreshAccumulator, DenseLayerLinear per L1) possono cosi' usare load/store
// ALLINEATI invece di loadu/storeu — le righe di L0 (512B) e L1 (512B) sono multipli esatti di
// 64B, quindi se l'array parte allineato ogni riga lo e' automaticamente. Senza allineamento
// esplicito il compilatore garantisce solo l'allineamento naturale (2/4 byte), quindi una riga
// da 512B poteva iniziare su un confine qualunque — fino a 9 cache line toccate invece di 8, con
// load "split" che costano un ciclo extra di porta L1 ciascuna sul percorso più caldo del motore.
class alignas(64) NnueNetwork {
public:
    static constexpr int M = 256;
    static constexpr int K = 32;
    static constexpr int NUM_FEATURES = 40960;

    // --- Parametri di Quantizzazione (QAT) ---
    static constexpr int QA_BOUND = 127;  // Limite massimo per la Clipped ReLU (es. int8 max)
    static constexpr int SHIFT_L1 = 6;    // Bit-shift per de-quantizzare L1 (2^6 = 64)
    static constexpr int SHIFT_L2 = 6;    // Bit-shift per de-quantizzare L2 (2^6 = 64)
    static constexpr int SHIFT_OUT = 14;  // (non usato per l'output finale, vedi OUTPUT_SCALE sotto)

    // Scala combinata reale dell'output L3 = SCALE_ACT * SCALE_W_OUT (127*127) in convertToWeights.py,
    // NON 2^SHIFT_OUT (16384): usare una divisione esatta, non uno shift, per evitare l'errore
    // sistematico del ~1.6% introdotto dall'approssimazione a potenza di 2.
    static constexpr int64_t OUTPUT_SCALE = 16129;
    // I "cp" del motore devono combaciare con la scala usata per generare il target di
    // training: cleanDataset.py definisce target = sigmoid(cp / 400) (non /100!), quindi il
    // logit imparato dalla rete approssima cp/400, e va ri-scalato di conseguenza qui e in
    // prob_to_cp()/prob_to_cp_tensor() in testingNNUE_HalfKP.py.
    static constexpr int64_t CP_SCALE = 400;

    // --- Pesi Quantizzati ---
    // Solo L0 e L1 sono allineati esplicitamente: le loro righe sono 512B (multiplo di 64),
    // quindi l'allineamento paga in aligned load/store nel percorso caldo (vedi sopra). L2 ha
    // righe da soli 32B (non multiplo di 64) — restano su load/store non allineati, l'intero
    // layer e' 1KB e non e' dove va il tempo (vedi profilo nello Step 2).
    alignas(64) int16_t L0[NUM_FEATURES][M];
    alignas(64) int16_t L0Bias[M];
    alignas(64) int8_t L1[K][2 * M];
    alignas(64) int32_t L1Bias[K];
    int8_t L2[K][K];
    int32_t L2Bias[K];
    int16_t L3[K];
    int32_t L3Bias;

    // --- Metodi e Helper (Struttura Originale) ---
    bool InitializeFromFile(std::string path);
    // const: nessuno stato mutabile nell'oggetto (inputBuffer/outputBuffer erano scratch
    // condivisi qui, ora locali allo stack di EvaluateNnue) — questo e' cio' che rende sicuro
    // condividere una singola istanza tra i thread di ricerca (vedi NnueEvaluator).
    int EvaluateNnue(const int16_t* smt, const int16_t* nsmt) const;

private:
    void DenseLayerLinear(const int8_t* input, const int8_t* weights, const int32_t* bias, int32_t* output, int rows, int cols, int shift) const;
    int32_t OutputLayerDotProduct(const int8_t* input, const int16_t* weights, int32_t bias, int len) const;

    void ActivationFunction(const int16_t* in, int8_t* out, int len) const;
    void ActivationFunction(const int32_t* in, int8_t* out, int len) const; // Overload per il Layer 1

};

// Carica una rete da disco in un oggetto condiviso e in sola lettura, da riusare per tutti i
// thread di ricerca di una sessione (invece di rileggere il file e duplicare i 20MB di L0 per
// ogni thread a ogni `go`). Ritorna nullptr se il file non puo' essere aperto/letto.
// Usa `new` invece di `std::make_shared` cosi' l'allocazione passa esplicitamente per
// `operator new(size_t, std::align_val_t)` quando NnueNetwork acquisira' un `alignas(64)` —
// `make_shared` dovrebbe allineare l'intera allocazione combinata oggetto+control-block, `new`
// rende la garanzia ovvia a chi legge il codice.
std::shared_ptr<const NnueNetwork> LoadNnueNetwork(const std::string& path);

#endif


