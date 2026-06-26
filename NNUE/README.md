# Piano di Sviluppo NNUE in C++

> Basato su: `official-stockfish/nnue-pytorch` — `docs/nnue.md`
> Linguaggio: C++17 o superiore
> Obiettivo: implementazione incrementale, partendo da una baseline float e arrivando a un'implementazione quantizzata ottimizzata

---

## Struttura del progetto

```
nnue/
├── src/
│   ├── features/
│   │   ├── feature_set.h          // interfaccia base per i feature set
│   │   ├── half_kp.h              // implementazione HalfKP
│   │   └── [future] half_ka_v2.h
│   ├── layers/
│   │   ├── linear.h               // strato lineare (dense)
│   │   ├── clipped_relu.h         // attivazione ClippedReLU
│   │   └── [future] quantmoid4.h
│   ├── network/
│   │   ├── accumulator.h          // struttura dell'accumulatore
│   │   ├── nnue.h                 // rete completa, forward pass
│   │   └── [future] arch/         // architetture diverse (SFNNv1..v13)
│   ├── training/
│   │   └── [stub] data_loader.h   // da implementare: lettura file dati
│   ├── quantization/
│   │   └── [future] quantize.h    // conversione float → int
│   └── simd/
│       └── [future] avx2.h        // intrinsics AVX2
├── tests/
│   ├── test_features.cpp
│   ├── test_forward.cpp
│   └── test_accumulator.cpp
└── CMakeLists.txt
```

---

## Fase 0 — Fondamenta (prerequisiti)

### 0.1 Algebra lineare di base
- Definire i tipi di dato: `float`, poi in seguito `int8_t`, `int16_t`, `int32_t`
- Convenzione layout pesi: **column-major** per il primo strato (sparse), **row-major** per gli strati densi successivi
  - Motivazione da doc: la moltiplicazione sparsa aggiunge colonne; la densa beneficia dal row-major
- Macro o `constexpr` per le dimensioni: `NUM_FEATURES`, `M`, `K` (strati nascosti)

### 0.2 Struttura dati posizione (stub)
- `struct Position` minimale: solo ciò che serve per estrarre le feature
  - lista di pezzi (tipo, colore, casa)
  - re bianco e nero (casa)
  - lato in mossa (`Color side_to_move`)
- **Non implementare la logica scacchistica ora** — il motore è fuori scope

---

## Fase 1 — Feature Set: HalfKP (versione base)

### 1.1 Indice delle feature
- Feature = `(king_square, piece_square, piece_type, piece_color)`
- `piece_type` **esclude** il re (5 tipi × 2 colori × 64 case pezzo × 64 case re = **40.960 feature**)
- Formula indice:
  ```
  p_idx = piece_type * 2 + piece_color
  halfkp_idx = piece_square + (p_idx + king_square * 10) * 64
  ```
- Funzione `get_active_features(Color perspective, const Position&) → vector<int>`
  - Itera su tutti i pezzi (no re)
  - Applica flip verticale per la prospettiva nera (orientamento coerente)

### 1.2 Aggiornamento incrementale delle feature
- Funzione `get_added_features(...)` e `get_removed_features(...)`
  - Caso mossa normale: 1 rimossa, 1 aggiunta
  - Caso cattura: 1 rimossa (pezzo catturato) + 1 rimossa + 1 aggiunta
  - Caso arrocco: 4 feature cambiano
  - **Caso re che si muove**: tutte le feature cambiano → refresh completo
- Flag `bool needs_refresh[2]` per ogni prospettiva

> **Improvement futuro [F1]**: aggiungere `HalfKAv2` (include il re come pezzo, 64×64×10×2 = 81.920 feature) e `HalfKAv2_hm` (con mirror orizzontale per dimezzare i pesi).

---

## Fase 2 — Accumulatore

### 2.1 Struttura
```cpp
// Versione float (Fase 2)
struct NnueAccumulator {
    float v[2][M];            // [prospettiva][neuroni]
    bool computed[2];         // flag: è aggiornato?
    float* operator[](Color c) { return v[c]; }
};
```
- L'accumulatore vive sullo **stack di ricerca** della posizione
- Non viene mai aggiornato all'undo: si restaura il valore precedente

### 2.2 Refresh completo
```
refresh_accumulator(L0, new_acc, active_features, perspective):
    copia bias → acc
    per ogni feature attiva: acc += weights[feature]
```

### 2.3 Aggiornamento incrementale
```
update_accumulator(L0, new_acc, prev_acc, removed, added, perspective):
    copia prev_acc → new_acc
    per ogni rimossa: new_acc -= weights[feature]
    per ogni aggiunta: new_acc += weights[feature]
```

> **Improvement futuro [F2]**: aggiornamento **lazy** (solo quando serve la valutazione, non ad ogni mossa). Richiede un flag `is_dirty` e la propagazione dello stack.

---

## Fase 3 — Strati della rete

### 3.1 Struttura `LinearLayer`
```cpp
struct LinearLayer {
    int num_inputs;
    int num_outputs;
    float* weights;   // layout: [num_inputs][num_outputs] (column-major per L0)
    float* bias;
};
```
- I pesi vengono allocati e caricati dal file al bootstrap
- Tenere separato `L0` (feature transformer) dagli altri (`L1`, `L2`)

### 3.2 `linear()` — moltiplicazione densa
```
linear(layer, output, input):
    output = bias
    per ogni i in num_inputs:
        per ogni j in num_outputs:
            output[j] += input[i] * weights[i][j]
    return output + num_outputs
```
- Usato per `L1` (dimensione `2M → K`) e `L2` (dimensione `K → 1`)

### 3.3 `clipped_relu()` — attivazione
```
crelu(size, output, input):
    per ogni i: output[i] = clamp(input[i], 0.0f, 1.0f)
    return output + size
```

---

## Fase 4 — Architettura di rete e forward pass

### 4.1 Architettura target (base)
```
HalfKP[40960] → M×2 → K → 1
```
Concretamente per il prototipo: `M=256`, `K=32`
Quindi: `40960 → 256×2 → 32 → 1`

Strati:
- `L0`: `LinearLayer` con 40960 input, M output (usato nell'accumulatore)
- `C0`: ClippedReLU su `2M` valori (concatenazione prospettiva STM + non-STM)
- `L1`: `LinearLayer` con `2M` input, `K` output
- `C1`: ClippedReLU su `K` valori
- `L2`: `LinearLayer` con `K` input, 1 output

### 4.2 Ordine delle prospettive nell'input a `L1`
```
input[0..M-1]   = accumulator[side_to_move]
input[M..2M-1]  = accumulator[!side_to_move]
```
- Questo permette alla rete di conoscere il **tempo** (chi è in mossa)

### 4.3 `nnue_evaluate(const Position&)`
```
nnue_evaluate(pos):
    aggiorna/refresh gli accumulatori se necessario
    assembla input[2M] concatenando STM e non-STM
    curr = crelu(2M, buf, input)
    curr = linear(L1, buf, curr)
    curr = crelu(K, buf, curr)
    curr = linear(L2, buf, curr)
    return *curr   // scalare = valutazione
```

> **Improvement futuro [F3]**: supporto per architetture multiple (enum `NetworkArch`) per poter passare facilmente tra SFNNv1, v4, v6 ecc. Parametrizzare `M` e `K` come template.

---

## Fase 5 — Caricamento pesi da file (stub)

### 5.1 Interfaccia `DataLoader` (stub, da completare)
```cpp
// src/training/data_loader.h
struct WeightFile {
    // da definire in base al formato scelto (binario raw, .nnue di Stockfish, ecc.)
};

bool load_network(const std::string& path, LinearLayer& L0, LinearLayer& L1, LinearLayer& L2);
```

### 5.2 Formato file suggerito (versione semplice)
- Header: magic number + versione + dimensioni (`num_features`, `M`, `K`)
- Dati in sequenza: `L0.bias`, `L0.weights`, `L1.bias`, `L1.weights`, `L2.bias`, `L2.weights`
- Tutto in `float32` little-endian nella baseline

> **Improvement futuro [F4]**: supporto al formato `.nnue` nativo di Stockfish (header con hash dell'architettura, pesi già quantizzati in int16/int8). Utile per caricare reti pre-addestrate ufficiali.

---

## Fase 6 — Test e validazione

### 6.1 Test del feature set
- Data una posizione nota, verificare che `get_active_features()` restituisca gli indici attesi
- Verificare simmetria: posizione speculare → feature speculari
- Verificare il caso di refresh sul movimento del re

### 6.2 Test dell'accumulatore
- Applicare `refresh_accumulator` e `update_accumulator` sulla stessa posizione
- Verificare che il risultato coincida entro epsilon float
- Testare catture, arrocco, promozione

### 6.3 Test del forward pass
- Con pesi noti (es. tutti 0 tranne un bias), verificare che l'output sia deterministico
- Test di regressione: stessa posizione → stesso output

---

## Fase 7 — Quantizzazione (futura)

> Questa fase va implementata **solo dopo** che la versione float è stabile e testata.

### Schema Stockfish (da `nnue.md` §Quantization)

| Strato | Tipo input | Tipo pesi | Tipo accumulatore |
|---|---|---|---|
| Feature Transformer | — | `int16_t` | `int16_t` |
| ClippedReLU (post-FT) | `int16_t` | — | `int8_t` (0..127) |
| Linear L1/L2 | `int8_t` | `int8_t` | `int32_t` |
| ClippedReLU (post-L1) | `int32_t` | — | `int8_t` |

### Fattori di scala
- `s_A = 127` — range attivazione (0..127 invece di 0..1)
- `s_W = 64` — scala pesi (peso max = 127/64 ≈ 1.98 in float)
- `s_O` — scala output finale (es. 400, allinea con centipawn)

### Operazioni di conversione al caricamento
- FT: `weight_int16 = round(weight_float * 127)`
- L1/L2: `weight_int8 = round(weight_float * s_W)`, clamp in `[-128, 127]`
- Bias FT: `bias_int16 = round(bias_float * 127)`
- Bias L1: `bias_int32 = round(bias_float * 127 * s_W)`
- Bias L2: `bias_int32 = round(bias_float * s_W * s_O)`

> **Improvement futuro [F5]**: implementare `quantize.h` con funzioni `float_to_quantized_network()` e `save_quantized()`.

---

## Fase 8 — Ottimizzazioni SIMD (futura)

> Da affrontare solo dopo che la quantizzazione è corretta e verificata.

### 8.1 Feature Transformer con AVX2
- Accumulatore allineato a 64 byte (`alignas(64)`)
- Registro `__m256i` per 16 valori `int16_t` alla volta
- `_mm256_add_epi16` / `_mm256_sub_epi16` per add/sub delle colonne
- Minimizzare i write in memoria: operare sui registri e scrivere solo alla fine

### 8.2 Linear layer denso con AVX2
- `m256_add_dpbusd_epi32`: moltiplica `int8 × int8`, accumula in `int32` (32 elementi per registro)
- `m256_haddx4`: riduzione orizzontale di 4 somme → 4 `int32` + bias
- Unroll ×4 sulle righe di output per riutilizzare gli stessi input
- Output scalato con `_mm_srai_epi32(..., log2(s_W))`

### 8.3 Linear layer con input sparso
- Dopo ClippedReLU il primo strato denso ha input con densità ≤15%
- Trovare gli indici non-zero (`lsb()` su bitmask)
- Processare solo le colonne attive dei pesi (trasposti a `int16_t`)
- Usare `m256_process_chunk` per coppie di colonne

> **Improvement futuro [F6]**: rilevamento runtime del supporto VNNI (`_mm256_dpbusd_epi32`) per sostituire la sequenza `maddubs + madd` con una sola istruzione.

> **Improvement futuro [F7]**: versione con AVX-512 per processare 32 int16 o 64 int8 alla volta.

---

## Roadmap sintetica

```
[Fase 0]  Tipi, layout, Position stub
    ↓
[Fase 1]  HalfKP: indici feature, flip prospettiva, get_active/added/removed
    ↓
[Fase 2]  Accumulatore: refresh + update incrementale
    ↓
[Fase 3]  Strati: LinearLayer (float), ClippedReLU (float)
    ↓
[Fase 4]  Forward pass completo in float — architettura 40960→256×2→32→1
    ↓
[Fase 5]  Stub DataLoader — caricamento pesi da file binario semplice
    ↓
[Fase 6]  Test e validazione
    ↓
[Fase 7]  Quantizzazione int8/int16 (schema Stockfish)
    ↓
[Fase 8]  Ottimizzazioni AVX2 (FT, dense, sparse)
```

---

## Riferimenti ai simboli del documento

| Simbolo | Significato |
|---|---|
| `M` | dimensione accumulatore (output di L0 per prospettiva) |
| `K` | dimensione primo strato denso |
| `N` | numero feature (40960 per HalfKP) |
| `s_A` | fattore scala attivazione (127 in quantizzazione) |
| `s_W` | fattore scala pesi (64 tipicamente) |
| `s_O` | fattore scala output finale |
| `STM` | Side To Move (lato in mossa) |
| `FT` | Feature Transformer (= L0 + accumulatore) |

---

## Improvements futuri — indice

| ID | Descrizione |
|---|---|
| F1 | Feature set HalfKAv2, HalfKAv2_hm, Full_Threats |
| F2 | Aggiornamento accumulatore lazy (flag dirty + stack) |
| F3 | Architetture parametrizzate via template/enum |
| F4 | Caricamento formato `.nnue` nativo Stockfish |
| F5 | Tool di quantizzazione float→int e salvataggio |
| F6 | Rilevamento runtime VNNI |
| F7 | Implementazione AVX-512 |
| F8 | Multiple PSQT outputs + subnetworks (SFNNv4+) |
| F9 | Feature factorization per il training (P, K, HalfRelativeKP) |