#include "NnueNetwork.hpp"

#include "cblas.h"
#include <fstream>
#include <cstring>

bool NnueNetwork::InitializeFromFile(std::string path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.read(reinterpret_cast<char*>(L0Bias), M * sizeof(double));
    file.read(reinterpret_cast<char*>(L0), NUM_FEATURES * M * sizeof(double));
    file.read(reinterpret_cast<char*>(L1Bias), K * sizeof(double));
    file.read(reinterpret_cast<char*>(L1), (2 * M) * K * sizeof(double));
    file.read(reinterpret_cast<char*>(&L2Bias), sizeof(double));    
    file.read(reinterpret_cast<char*>(L2), K * sizeof(double));
    
    char check;
    file.read(&check, 1);
    if (!file.eof()) {
        return false;
    }

    file.close();
    return true;
}

void NnueNetwork::ActivationFunction(double* arr, int len) {
    // To change and optimize
    for (int i = 0; i < len; i++) {
        arr[i] = std::min(1.0, std::max(0.0, arr[i]));
    }
}

void NnueNetwork::DenseLayerLinear(const double* input, const double* weights, const double* bias, double* output, int rows, int cols) {
    // y = A * x + b
    // - rows: Numero di neuroni di output dello strato (K = 32)
    // - cols: Numero di neuroni di input dello strato (2 * M = 512)
    std::memcpy(output, bias, rows * sizeof(double));
    cblas_dgemv(CblasRowMajor, CblasNoTrans, rows, cols, 1.0, weights, cols, input, 1, 1.0, output, 1);
}

double NnueNetwork::OutputLayerDotProduct(const double* input, const double* weights, double bias, int len) {
    // score = w \dot x + bias
    double dot = cblas_ddot(len, input, 1, weights, 1);
    return dot + bias;
}

int NnueNetwork::EvaluateNnue(double* smt, double* nsmt){
    
    std::memcpy(inputBuffer, smt, M * sizeof(double));
    std::memcpy(inputBuffer + M, nsmt, M * sizeof(double));
    
    ActivationFunction(inputBuffer, 2*M);    
    DenseLayerLinear(inputBuffer, &L1[0][0], L1Bias, outputBuffer, K, 2*M);
    ActivationFunction(outputBuffer, K);
    double finalScore = OutputLayerDotProduct(outputBuffer, L2, L2Bias, K);

    return static_cast<int>(finalScore * 100.0);
}

