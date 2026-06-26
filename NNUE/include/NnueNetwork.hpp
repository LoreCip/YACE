#ifndef NNUENETWORK
#define NNUENETWORK

#include "Constants.hpp"
#include<string>
#include <cstring>

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

class NnueNetwork{
private:
    void ActivationFunction(double* arr, int len);
    void DenseLayerLinear(const double* input, const double* weights, const double* bias, double* output, int rows, int cols);
    double OutputLayerDotProduct(const double* input, const double* weights, double bias, int len);

public:
    double L0[NUM_FEATURES][M] = {{0}};
    double L0Bias[M] = {0};
    double L1[2*M][K] = {{0}};
    double L1Bias[K] = {0};
    double L2[K] = {0};
    double L2Bias = 0;

    double inputBuffer[2*M] = {0};
    double outputBuffer[K] = {0};

    bool InitializeFromFile(std::string path);
    int EvaluateNnue(double* smt, double* nsmt);

};


#endif