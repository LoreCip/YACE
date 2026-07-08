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

class NnueNetwork {
public:
    static constexpr int M = 256;
    static constexpr int K = 32;
    static constexpr int NUM_FEATURES = 40960;

    // --- Parametri di Quantizzazione (QAT) ---
    static constexpr int QA_BOUND = 127;  // Limite massimo per la Clipped ReLU (es. int8 max)
    static constexpr int SHIFT_L1 = 6;    // Bit-shift per de-quantizzare L1 (2^6 = 64)
    static constexpr int SHIFT_L2 = 6;    // Bit-shift per de-quantizzare L2 (2^6 = 64)
    static constexpr int SHIFT_OUT = 14;  // Bit-shift per de-quantizzare l'Output Finale

    // --- Pesi Quantizzati ---
    int16_t L0[NUM_FEATURES][M];
    int16_t L0Bias[M];
    int8_t L1[K][2 * M];
    int32_t L1Bias[K];     
    int8_t L2[K][K];
    int32_t L2Bias[K];
    int16_t L3[K];
    int32_t L3Bias;

    // --- Buffer di memoria interni ---
    int8_t inputBuffer[2 * M];
    int8_t outputBuffer[K];

    // --- Metodi e Helper (Struttura Originale) ---
    bool InitializeFromFile(std::string path);
    int EvaluateNnue(int16_t* smt, int16_t* nsmt);

private:
    void DenseLayerLinear(const int8_t* input, const int8_t* weights, const int32_t* bias, int32_t* output, int rows, int cols, int shift);    
    int32_t OutputLayerDotProduct(const int8_t* input, const int16_t* weights, int32_t bias, int len);
    
    void ActivationFunction(const int16_t* in, int8_t* out, int len);
    void ActivationFunction(const int32_t* in, int8_t* out, int len); // Overload per il Layer 1

};

#endif


