#include "NnueNetwork.hpp"
#include <fstream>
#include <algorithm>
#include <immintrin.h>   // AVX-512 intrinsics

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

// ---------------------------------------------------------------------------
// Somma orizzontale di 8 int32 in un __m256i -> singolo int32 (usata solo
// nella coda a 256 bit di DenseLayerLinear quando cols%64 è tra 32 e 63).
// ---------------------------------------------------------------------------
static inline int32_t hsum_epi32_256(__m256i v) {
    __m128i lo = _mm256_castsi256_si128(v);
    __m128i hi = _mm256_extracti128_si256(v, 1);
    lo = _mm_add_epi32(lo, hi);
    __m128i shuf = _mm_shuffle_epi32(lo, _MM_SHUFFLE(1, 0, 3, 2));
    lo = _mm_add_epi32(lo, shuf);
    shuf = _mm_shuffle_epi32(lo, _MM_SHUFFLE(0, 1, 0, 1));
    lo = _mm_add_epi32(lo, shuf);
    return _mm_cvtsi128_si32(lo);
}

// ---------------------------------------------------------------------------
// Clipped ReLU: int16_t -> int8_t, clamp [0, QA_BOUND]
// _mm512_cvtepi16_epi8 fa il narrow con saturazione IN ORDINE, senza bisogno
// di permute per rimettere a posto le corsie (a differenza di packs_epi16/AVX2).
// 32 elementi per iterazione con un solo registro/load invece di due.
// ---------------------------------------------------------------------------
void NnueNetwork::ActivationFunction(const int16_t* in, int8_t* out, int len) {
    const __m512i zero  = _mm512_setzero_si512();
    const __m512i bound = _mm512_set1_epi16(QA_BOUND);

    int i = 0;
    for (; i + 32 <= len; i += 32) {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(in + i));
        v = _mm512_max_epi16(_mm512_min_epi16(v, bound), zero);
        __m256i packed = _mm512_cvtepi16_epi8(v);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), packed);
    }
    for (; i < len; i++) {
        out[i] = static_cast<int8_t>(std::clamp<int16_t>(in[i], 0, QA_BOUND));
    }
}

// ---------------------------------------------------------------------------
// Clipped ReLU: int32_t -> int8_t, clamp [0, QA_BOUND]
// _mm512_cvtepi32_epi8: stesso discorso, narrow diretto senza permute.
// 16 elementi per iterazione (un solo registro __m512i di int32).
// ---------------------------------------------------------------------------
void NnueNetwork::ActivationFunction(const int32_t* in, int8_t* out, int len) {
    const __m512i zero  = _mm512_setzero_si512();
    const __m512i bound = _mm512_set1_epi32(QA_BOUND);

    int i = 0;
    for (; i + 16 <= len; i += 16) {
        __m512i v = _mm512_loadu_si512(reinterpret_cast<const void*>(in + i));
        v = _mm512_max_epi32(_mm512_min_epi32(v, bound), zero);
        __m128i packed = _mm512_cvtepi32_epi8(v);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), packed);
    }
    for (; i < len; i++) {
        out[i] = static_cast<int8_t>(std::clamp<int32_t>(in[i], 0, QA_BOUND));
    }
}

// ---------------------------------------------------------------------------
// Layer denso: output[i] = (bias[i] + sum_j input[j]*weights[i*cols+j]) >> shift
// VPDPBUSD (AVX512VNNI): moltiplica-somma-accumula u8 x s8 -> i32 in UNA
// istruzione fusa, 64 byte per volta. Niente stadio intermedio a 16 bit
// (più sicuro anche dal punto di vista della saturazione rispetto a
// maddubs+madd), niente add esplicito (l'accumulo è già incluso).
// 4 righe di output per volta per tenere occupate più catene indipendenti.
// ---------------------------------------------------------------------------
void NnueNetwork::DenseLayerLinear(const int8_t* input, const int8_t* weights, const int32_t* bias, int32_t* output, int rows, int cols, int shift) {
    int i = 0;
    for (; i + 4 <= rows; i += 4) {
        __m512i acc0 = _mm512_setzero_si512();
        __m512i acc1 = _mm512_setzero_si512();
        __m512i acc2 = _mm512_setzero_si512();
        __m512i acc3 = _mm512_setzero_si512();
        const int8_t* w0 = weights + static_cast<size_t>(i + 0) * cols;
        const int8_t* w1 = weights + static_cast<size_t>(i + 1) * cols;
        const int8_t* w2 = weights + static_cast<size_t>(i + 2) * cols;
        const int8_t* w3 = weights + static_cast<size_t>(i + 3) * cols;

        int j = 0;
        for (; j + 64 <= cols; j += 64) {
            __m512i in_vec = _mm512_loadu_si512(reinterpret_cast<const void*>(input + j));
            acc0 = _mm512_dpbusd_epi32(acc0, in_vec, _mm512_loadu_si512(reinterpret_cast<const void*>(w0 + j)));
            acc1 = _mm512_dpbusd_epi32(acc1, in_vec, _mm512_loadu_si512(reinterpret_cast<const void*>(w1 + j)));
            acc2 = _mm512_dpbusd_epi32(acc2, in_vec, _mm512_loadu_si512(reinterpret_cast<const void*>(w2 + j)));
            acc3 = _mm512_dpbusd_epi32(acc3, in_vec, _mm512_loadu_si512(reinterpret_cast<const void*>(w3 + j)));
        }
        int32_t s0 = _mm512_reduce_add_epi32(acc0) + bias[i + 0];
        int32_t s1 = _mm512_reduce_add_epi32(acc1) + bias[i + 1];
        int32_t s2 = _mm512_reduce_add_epi32(acc2) + bias[i + 2];
        int32_t s3 = _mm512_reduce_add_epi32(acc3) + bias[i + 3];

        // coda a 32 byte con VNNI a 256 bit (AVX512VL) se resta spazio prima dello scalare
        for (; j + 32 <= cols; j += 32) {
            __m256i in_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + j));
            s0 += hsum_epi32_256(_mm256_dpbusd_epi32(_mm256_setzero_si256(), in_vec, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w0 + j))));
            s1 += hsum_epi32_256(_mm256_dpbusd_epi32(_mm256_setzero_si256(), in_vec, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w1 + j))));
            s2 += hsum_epi32_256(_mm256_dpbusd_epi32(_mm256_setzero_si256(), in_vec, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w2 + j))));
            s3 += hsum_epi32_256(_mm256_dpbusd_epi32(_mm256_setzero_si256(), in_vec, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w3 + j))));
        }
        // coda scalare finale (cols non multiplo di 32)
        for (; j < cols; j++) {
            s0 += input[j] * w0[j]; s1 += input[j] * w1[j];
            s2 += input[j] * w2[j]; s3 += input[j] * w3[j];
        }

        output[i + 0] = s0 >> shift; output[i + 1] = s1 >> shift;
        output[i + 2] = s2 >> shift; output[i + 3] = s3 >> shift;
    }

    // righe rimanenti (rows non multiplo di 4)
    for (; i < rows; i++) {
        __m512i acc = _mm512_setzero_si512();
        const int8_t* w = weights + static_cast<size_t>(i) * cols;

        int j = 0;
        for (; j + 64 <= cols; j += 64)
            acc = _mm512_dpbusd_epi32(acc, _mm512_loadu_si512(reinterpret_cast<const void*>(input + j)), _mm512_loadu_si512(reinterpret_cast<const void*>(w + j)));

        int32_t sum = _mm512_reduce_add_epi32(acc) + bias[i];

        for (; j + 32 <= cols; j += 32)
            sum += hsum_epi32_256(_mm256_dpbusd_epi32(_mm256_setzero_si256(), _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + j)), _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w + j))));

        for (; j < cols; j++) sum += input[j] * w[j];

        output[i] = sum >> shift;
    }
}

// ---------------------------------------------------------------------------
// Prodotto scalare finale: input int8_t (0..127) x weights int16_t
// K=32 sta esattamente in un solo registro __m512i (32 int16): un solo
// giro di estensione + madd + riduzione, niente loop.
// ---------------------------------------------------------------------------
int32_t NnueNetwork::OutputLayerDotProduct(const int8_t* input, const int16_t* weights, int32_t bias, int len) {
    __m512i acc = _mm512_setzero_si512();

    int i = 0;
    for (; i + 32 <= len; i += 32) {
        __m256i in8  = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i));
        __m512i in16 = _mm512_cvtepi8_epi16(in8); // sign-extend a 16 bit
        __m512i w16  = _mm512_loadu_si512(reinterpret_cast<const void*>(weights + i));

        acc = _mm512_add_epi32(acc, _mm512_madd_epi16(in16, w16));
    }

    int32_t dot = bias + _mm512_reduce_add_epi32(acc);
    for (; i < len; i++) dot += input[i] * weights[i];

    return dot >> SHIFT_OUT;
}

int NnueNetwork::EvaluateNnue(int16_t* smt, int16_t* nsmt) {
    ActivationFunction(smt, inputBuffer, M);
    ActivationFunction(nsmt, inputBuffer + M, M);
    
    // Passaggio L1
    int32_t tempHidden[K];
    DenseLayerLinear(inputBuffer, &L1[0][0], L1Bias, tempHidden, K, 2 * M, SHIFT_L1);
    ActivationFunction(tempHidden, outputBuffer, K);

    // Passaggio L2
    int32_t tempHidden2[K];
    int8_t outputBuffer2[K];
    DenseLayerLinear(outputBuffer, &L2[0][0], L2Bias, tempHidden2, K, K, SHIFT_L2);
    ActivationFunction(tempHidden2, outputBuffer2, K);
    
    // Output (L3)
    return OutputLayerDotProduct(outputBuffer2, L3, L3Bias, K);
}