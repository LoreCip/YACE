#ifndef NNUE_NETWORK_HPP
#define NNUE_NETWORK_HPP

#include "NnueConstants.hpp"

#include <cstdint>
#include <memory>
#include <string>

// File pesi (NNUE/training/export_weights.py): little-endian, senza header, in quest'ordine:
//   L0Bias int16[M], L0 int16[NUM_FEATURES][M], L1Bias int32[K], L1 int8[K][2*M],
//   L2Bias int32[K], L2 int8[K][K], L3Bias int32, L3 int16[K]

// alignas(64): le righe di L0 e L1 (512B) restano allineate → load/store AVX-512 allineati.
class alignas(64) NnueNetwork {
public:
    // Quantizzazione
    static constexpr int QA_BOUND = 127;  // tetto della Clipped ReLU
    static constexpr int SHIFT_L1 = 6;    // de-quantizzazione L1 (/64)
    static constexpr int SHIFT_L2 = 6;    // de-quantizzazione L2 (/64)

    // Scala dell'output L3 = 127*127 (export_weights.py): divisione esatta, uno shift da 2^14 sbaglia dell'1.6%.
    static constexpr int64_t OUTPUT_SCALE = 16129;
    // Il target di training e' sigmoid(cp/400) (halfkp.py, model.py): l'output della rete e' cp/400.
    static constexpr int64_t CP_SCALE = 400;

    // Solo L0 e L1 allineati: L2 e' 1KB con righe da 32B, non ne trae vantaggio.
    alignas(64) int16_t L0[NUM_FEATURES][M];
    alignas(64) int16_t L0Bias[M];
    alignas(64) int8_t L1[K][2 * M];
    alignas(64) int32_t L1Bias[K];
    int8_t L2[K][K];
    int32_t L2Bias[K];
    int16_t L3[K];
    int32_t L3Bias;

    bool InitializeFromFile(const std::string& path);
    // const e senza stato mutabile: un'unica istanza e' condivisa da tutti i thread.
    int EvaluateNnue(const int16_t* smt, const int16_t* nsmt) const;

private:
    void DenseLayerLinear(const int8_t* input, const int8_t* weights, const int32_t* bias, int32_t* output, int rows, int cols, int shift) const;
    int32_t OutputLayerDotProduct(const int8_t* input, const int16_t* weights, int32_t bias, int len) const;

    void ActivationFunction(const int16_t* in, int8_t* out, int len) const;
    void ActivationFunction(const int32_t* in, int8_t* out, int len) const;

};

// Rete in sola lettura condivisa da tutti i thread; nullptr se il file manca o e' incompleto.
std::shared_ptr<const NnueNetwork> LoadNnueNetwork(const std::string& path);

#endif


