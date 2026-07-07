#include "NnueNetwork.hpp"
#include <fstream>
#include <algorithm>

bool NnueNetwork::InitializeFromFile(std::string path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.read(reinterpret_cast<char*>(L0Bias), M * sizeof(int16_t));
    file.read(reinterpret_cast<char*>(L0), NUM_FEATURES * M * sizeof(int16_t));
    file.read(reinterpret_cast<char*>(L1Bias), K * sizeof(int32_t));
    file.read(reinterpret_cast<char*>(L1), (2 * M) * K * sizeof(int8_t));
    file.read(reinterpret_cast<char*>(L2Bias), K * sizeof(int32_t));    
    file.read(reinterpret_cast<char*>(L2), K * K * sizeof(int8_t));
    file.read(reinterpret_cast<char*>(&L3Bias), sizeof(int32_t));    
    file.read(reinterpret_cast<char*>(L3), K * sizeof(int16_t));
    
    char check;
    file.read(&check, 1);
    if (!file.eof()) {
        return false;
    }

    file.close();
    return true;
}

void NnueNetwork::ActivationFunction(const int16_t* in, int8_t* out, int len) {
    for (int i = 0; i < len; i++) {
        out[i] = static_cast<int8_t>(std::clamp<int16_t>(in[i], 0, QA_BOUND));
    }
}

void NnueNetwork::ActivationFunction(const int32_t* in, int8_t* out, int len) {
    for (int i = 0; i < len; i++) {
        out[i] = static_cast<int8_t>(std::clamp<int32_t>(in[i], 0, QA_BOUND));
    }
}

void NnueNetwork::DenseLayerLinear(const int8_t* input, const int8_t* weights, const int32_t* bias, int32_t* output, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        int32_t sum = bias[i];
        for (int j = 0; j < cols; j++) {
            sum += input[j] * weights[i * cols + j];
        }
        output[i] = sum >> SHIFT_L1; 
    }
}

int32_t NnueNetwork::OutputLayerDotProduct(const int8_t* input, const int16_t* weights, int32_t bias, int len) {
    int32_t dot = bias;
    for (int i = 0; i < len; i++) {
        dot += input[i] * weights[i];
    }
    return dot >> SHIFT_OUT; 
}

int NnueNetwork::EvaluateNnue(int16_t* smt, int16_t* nsmt) {
    ActivationFunction(smt, inputBuffer, M);
    ActivationFunction(nsmt, inputBuffer + M, M);
    
    // Passaggio L1
    int32_t tempHidden[K];
    DenseLayerLinear(inputBuffer, &L1[0][0], L1Bias, tempHidden, K, 2 * M);
    ActivationFunction(tempHidden, outputBuffer, K);

    // Passaggio L2
    int32_t tempHidden2[K];
    int8_t outputBuffer2[K];
    DenseLayerLinear(outputBuffer, &L2[0][0], L2Bias, tempHidden2, K, K);
    ActivationFunction(tempHidden2, outputBuffer2, K);
    
    // Output (L3)
    return OutputLayerDotProduct(outputBuffer2, L3, L3Bias, K);
}