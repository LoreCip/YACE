# Registro ottimizzazioni NNUE (velocità, senza retraining)

Un cambiamento per iterazione. Ogni riga registra: cosa è cambiato, come è stato misurato, e
il verdetto. Vedi il piano approvato per la disciplina di lavoro completa.

Macchina: i7-11800H, 8C/16T, L3 24 MiB, AVX-512 F/BW/DQ/VL/VNNI. `DefaultThreadCount()` = 8.

---

## Step 1 — Fix bug accumulatore mosse di re (correttezza, non velocità)

**Cambiamento**: `NnueEvaluator::IncrementalUpdate` (`src/Evaluation/src/NNUE/NnueEvaluator.cpp`)
trattava ogni mossa di re come se il re fosse una feature HalfKP normale. Siccome
`PIECE_TO_NNUE_IDX[KING] == -1`, questo produceva un indice fuori scala (o, per `king_sq`
diverso da zero, un indice *in scala ma sbagliato*, alias di un'altra riga di `L0`) e
corrompeva silenziosamente l'accumulatore della prospettiva avversaria a ogni mossa di re.
Fix: quando `movedPiece == KING`, la prospettiva non-movente salta l'add/remove del re stesso
(nessuna feature coinvolta) ma applica comunque normalmente la rimozione di un'eventuale
cattura e l'aggiornamento della torre in caso di arrocco (features reali, non toccate dal bug).
Aggiunto anche un `assert` in `MakeFeatureIndex` che verifica `pieceType != KING` e che
l'indice prodotto sia sempre in `[0, NUM_FEATURES)`.

**File**: `src/Evaluation/src/NNUE/NnueEvaluator.cpp`, `src/Evaluation/include/NNUE/NnueEvaluator.hpp`.

### Test eseguiti

**a. Validator `NNUE_VALIDATE`** — nuova modalità di build (`#ifdef NNUE_VALIDATE`) che dopo
ogni `Evaluate()` ricalcola da zero (`RefreshAccumulator`) ogni accumulatore già "computed",
verifica che coincida byte-per-byte con quello mantenuto incrementalmente, poi lo ripristina
(pura osservazione). Verificato che il validator FUNZIONA: costruita una copia di controllo
con il fix revertito (`isKingMove` rimosso) — crash immediato (`assert` fallito) al primo
`Evaluate()` dopo una mossa di re, sulla posizione `r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1`, a
profondità 1. Con il fix applicato: **11 posizioni variate** (arrocco entrambi i lati/colori,
finali di soli re, promozioni, mediogioco) a profondità 7, **zero fallimenti** dell'assert.

**b. Simmetria speculare** — harness standalone (`/tmp/eval_probe.cpp`, chiama `Evaluate()`
direttamente, bypassando la ricerca) su 5 coppie di posizioni speculari (scambio colori +
flip verticale + cambio lato di mossa): tutte e 5 mostrano `eval(pos) == -eval(mirror)` esatto.

**c. Accordo con Stockfish** (`/tmp/sf_agreement.py`, python-chess) — 48 posizioni (15 scelte a
mano pesate su re attivi/arrocco/finali + 35 generate con random walk da 10-40 mezze mosse),
YACE a profondità 7 vs Stockfish 18 a profondità 14:
  - **Fixed**: 33/48 accordo con Stockfish (68.8%)
  - **Pre-fix (bug presente)**: 30/48 accordo con Stockfish (62.5%)
  - **10/48 posizioni** mostrano una mossa scelta diversa tra fixed e pre-fix — evidenza diretta
    che il bug altera scelte di gioco reali, non solo un dettaglio numerico interno.

**d. Gauntlet di ri-baseline** (100 partite vs Stockfish, TARGET_ELO=2000, stesse impostazioni
della misura V3-6 originale = 2196 Elo):

> - 77 vittorie, 14 sconfitte, 9 patte (77.0%)
> - Differenza Elo: +257.6 — **Elo stimato: 2258** (era 2196 pre-fix)
> - +62 Elo, dentro la banda di rumore statistico di un campione da 100 partite ma coerente in
>   direzione con il miglioramento di accordo con Stockfish misurato al punto (c) — non un
>   salto enorme (il bug colpiva solo le mosse di re, una minoranza dei nodi), ma un
>   miglioramento reale e nella direzione attesa.

**Verdetto**: KEPT. Bug reale confermato e corretto; **nuova baseline per tutti gli step
successivi: bench signature dopo questo fix, 2258 Elo.**

---

## Step 1bis — Fix bug di lettura fuori dai limiti in SEE (correttezza, scoperto costruendo il bench)

**Contesto**: costruendo lo Step 2 (comando `bench`, la strumentazione di misura richiesta da
tutti gli step successivi), il segnale atteso deterministico non lo era: lo stesso identico
binario, sulla stessa posizione, alla stessa profondità, a thread singolo, con budget di tempo
enorme (quindi il thread del timer non può mai scadere), produceva **due risposte diverse in
proporzioni vicine al 50/50** su ripetizioni consecutive. Bug pre-esistente, indipendente da
tutte le modifiche di questa sessione (riprodotto anche con `-e hce`).

**Diagnosi**: `setarch -R` (ASLR disattivato) rendeva il comportamento perfettamente
deterministico → puntava a una lettura di memoria non inizializzata/fuori dai limiti, non a una
race condition. Confermato disabilitando del tutto la logica del thread del timer (nessun
cambiamento) e poi con una build strumentata AddressSanitizer + UndefinedBehaviorSanitizer, che
ha isolato l'errore esatto:

```
SearchWorker.cpp:407: gain[0] = LookupTables::pieceValues[PieceInt(capturedPiece)];
AddressSanitizer: global-buffer-overflow — index 6 su un array int[6]
```

`GenerateCaptureMoves` (usata in quiescence) include anche le **promozioni silenziose**
(`PRQUEEN` su casa vuota, non solo `PRCAPQUEEN`), per cui `capturedPiece =
board.GetPieceOnSquare(to)` può essere `PieceType::NONE` (valore enum 6). `pieceValues[]` ha
solo 6 elementi (indici 0-5, PAWN..KING) — leggere l'indice 6 e' una lettura fuori dai limiti su
una variabile globale, contenuto indeterminato, che nella build di produzione (non
strumentata) restituiva byte adiacenti arbitrari invece di un crash — da cui la "casualità"
apparente della valutazione SEE (quindi delle decisioni di potatura in quiescence) a ogni
esecuzione.

**Fix**: `gain[0] = (capturedPiece != PieceType::NONE) ? pieceValues[PieceInt(capturedPiece)] : 0;`
— una promozione silenziosa non cattura nulla, il bonus di promozione (regina - pedone) resta
gestito correttamente dal blocco `if` successivo, invariato.

**File**: `src/Search/src/SearchWorker.cpp` (`SearchWorker::SEE`).

**Verifica**: build strumentata ASan+UBSan pulita sulla stessa posizione che prima falliva
immediatamente. Build di produzione: **10/10 run identici** (nodi, punteggio, firma) sulla
posizione che prima divergeva quasi a caso, e sull'intero set di bench (8/8 run identici, sia
NNUE sia HCE, profondità 10-12).

**Gauntlet di ri-baseline** (100 partite vs Stockfish, TARGET_ELO=2000, stesse impostazioni
delle misure precedenti = 2258 Elo dopo il fix delle mosse di re):

> - 84 vittorie, 10 sconfitte, 6 patte (84.0%)
> - Differenza Elo: +330.2 — **Elo stimato: 2330** (era 2258 prima di questo fix, 2196 prima di
>   entrambi i fix di correttezza)
> - +72 Elo, un miglioramento reale e sostanziale: la valutazione SEE errata falsava le
>   decisioni di potatura in quiescence per qualunque posizione con pedoni prossimi alla
>   promozione — molto più comune del bug delle mosse di re, da cui il guadagno maggiore.

**Verdetto**: KEPT. Bug reale di sicurezza della memoria (non solo di logica), confermato e
corretto. **Nuova baseline definitiva per gli step di ottimizzazione della velocità**: il motore
ora produce risultati bit-per-bit deterministici a thread singolo, requisito indispensabile
perché la firma del bench possa fare da cancello di correttezza per tutti gli step successivi.

---

## Step 2 — Comando `bench` (strumento di misura) + profilo di riferimento

**Cambiamento**: aggiunto il token `bench [depth]` a `UCI::Loop` (`src/Core/src/Uci.cpp`) — 16
posizioni fisse (apertura, mediogiochi, finali, arrocco, promozioni, re in angolo), ciascuna
gioca `go depth <N> movetime -1` con TT svuotata prima di ogni posizione
(`searchManager.ClearTT()` aggiunto a `handlePositionLoaded` in `main_game.cpp`, insieme al
`Reset()` dell'accumulatore già presente), senza mai giocare la bestmove sulla board. Stampa
nodi totali, tempo, NPS, e una firma FNV-1a a 64 bit su `(depth, score finale, bestmove)` di
ogni posizione — il cancello di correttezza per ogni step successivo.

**Bug trovato e corretto durante la costruzione** (vedi voce "Step 1bis" sopra): il comando
`bench` chiamava `onGoCommand` direttamente, saltando la traduzione `movetime -1 -> tempo
illimitato` che avviene solo nel parser del token `go` — passava `-1.0` invece di
`99999999.0`, facendo scadere il thread del timer quasi subito. Corretto passando il valore già
tradotto. Questo bug, comunque, ha portato a scoprire il bug ben più serio di SEE (Step 1bis).

**Profilo di riferimento** (`perf record -g`, `bench 12`, `-c 1`, `taskset -c 2`, dopo entrambi
i fix di correttezza — build deterministica):

| Funzione | % self time |
| :--- | ---: |
| `NnueNetwork::EvaluateNnue` (dense layers + attivazioni + output, tutto inlineato da LTO) | **39.42%** |
| `SearchWorker::AlphaBeta` (scheletro di ricerca) | 18.36% |
| `NnueEvaluator::IncrementalUpdate` | 11.38% |
| `SearchWorker::QuiescenceSearch` | 6.91% |
| `Board::MakeMove` | 5.79% |
| `MoveGen::GenerateAllMoves` | 3.50% |
| `Board::UnmakeMove` | 3.22% |
| `NnueEvaluator::RefreshAccumulator` (2 specializzazioni) | 2.58% + 1.83% |
| `NnueEvaluator::OnMakeMove` (wrapper sottile) | 0.95% |

**Implicazione per l'ordine degli step successivi**: `EvaluateNnue` da sola (~39%) supera
nettamente l'intero costo di manutenzione dell'accumulatore (`IncrementalUpdate` + `OnMakeMove`
+ `RefreshAccumulator` ≈ 16.7%). Gli Step 6 (allineamento) e 7 (haddx4) colpiscono direttamente
il pool più grande (39%); lo Step 8 (accumulatore pigro) colpisce il pool più piccolo (16.7%) —
resta comunque utile, ma il profilo conferma che il forward pass denso è la leva più grande su
questo hardware, non la manutenzione dell'accumulatore come inizialmente stimato. L'ordine già
pianificato (6 e 7 prima di 8) resta corretto, nessuna riordinazione necessaria.

**Baseline per gli step successivi** (bench 12, `-c 1`, `taskset -c 2`, mediana di 3, dopo
entrambi i fix di correttezza):

| Metrica | Valore |
| :--- | ---: |
| Nodi totali | 41.578.225 |
| Tempo | ~9.6-9.9 s |
| NPS | ~4.25-4.33M |
| Firma bench | `86e814edf18440a2` |
| Istruzioni totali | 82.092.484.014 |
| Cicli totali | 39.648.462.460 |
| L1-dcache-load-misses | 1.974.319.881 |
| LLC-load-misses | 6.054.881 |
| Istruzioni/nodo | ~1974 |

**Verdetto**: KEPT (harness) + KEPT (fix scoperto). Ogni step da qui in avanti deve lasciare la
firma bench invariata e sarà confrontato contro questa baseline.

---

## Step 3 — Rete NNUE condivisa tra i thread (invece di ricaricata/duplicata per ognuno)

**Cambiamento**: `NnueNetwork` era un membro *by-value* (~20MB, dominato da `L0`) di ogni
`NnueEvaluator`, e `MakeEvaluatorFactory` (`main_game.cpp`) rileggeva il file dei pesi da disco
per ogni thread a ogni comando `go` (`SearchManager::Search` ricostruisce il vettore di worker
a ogni chiamata). Con `DefaultThreadCount()=8` su questa macchina: 8x lettura di un file da
~21MB + 8 allocazioni/copie in RAM, in serie, prima che un solo nodo venga cercato — a ogni
mossa.

Fix (nessuna semantica di ricerca toccata, `EvaluateNnue` e i suoi metodi privati sono ora
tutti `const`, `inputBuffer`/`outputBuffer` (uniche scritture mutabili) spostati da membri
condivisi a variabili locali sullo stack di `EvaluateNnue`):
1. `NnueNetwork::EvaluateNnue(...)` e i metodi privati diventano `const`; `inputBuffer`/
   `outputBuffer` rimossi come membri, ora locali (832B di stack, nessun impatto misurabile).
2. Nuova funzione libera `LoadNnueNetwork(path) -> std::shared_ptr<const NnueNetwork>`
   (`NnueNetwork.hpp/.cpp`) — carica una volta, alloca con `new` (non `make_shared`, per
   compatibilita' futura con `alignas(64)`, vedi Step 6).
3. `NnueEvaluator` tiene `std::shared_ptr<const NnueNetwork> networkOwner` (proprieta') +
   `const NnueNetwork* net` (accesso sul percorso caldo, nessun refcount toccato nei loop di
   ricerca) — nuovo costruttore che accetta la rete condivisa; `Initialize(path)` resta come
   shim di compatibilita' (rete privata, non condivisa) per chi non vuole condividere.
4. `main_game.cpp`: `main()` carica la rete UNA volta, la passa sia a `CreateRootEvaluator` sia
   a `MakeEvaluatorFactory` (catturata per valore nella lambda — lifetime indipendente
   dall'ordine di distruzione).

**File**: `src/Evaluation/include/NNUE/NnueNetwork.hpp`, `src/Evaluation/src/NNUE/NnueNetwork.cpp`,
`src/Evaluation/include/NNUE/NnueEvaluator.hpp`, `src/Evaluation/src/NNUE/NnueEvaluator.cpp`,
`src/App/main_game.cpp`.

**Sicurezza dei thread**: l'oggetto e' scritto completamente da un thread solo prima che
qualunque worker esista; tutti gli accessi successivi sono tramite `const NnueNetwork*`; la
costruzione di uno `std::thread` e' un punto di sincronizzazione (happens-before), quindi le
scritture di `InitializeFromFile` sono visibili senza fence aggiuntivi.

### Test eseguiti

**Correttezza**: `bench 12` (`-c 1`, `taskset -c 2`) — firma **invariata**
(`86e814edf18440a2`), nodi totali identici (41.578.225) rispetto alla baseline dello Step 2.
Nessun impatto sui valori di valutazione, come atteso (cambio puramente strutturale).

**Prestazioni** (metodo: controllo pre-Step-3 ricostruito da un `git stash` temporaneo dei soli
file toccati, confrontato binario-a-binario con lo stesso comando; mediana/media su 5 run,
`-c 8`, `DefaultThreadCount()`):

| Metrica (8 thread) | Prima | Dopo | Variazione |
| :--- | ---: | ---: | ---: |
| Latenza `go depth 1` (avvio processo incluso) | 44.7 ms (mediana) | 21.1 ms (mediana) | **-53%** |
| RSS di picco (`go depth 8`) | 267.152 KB | 103.168 KB | **-61%** |

Coerente con la stima del piano (~100-150ms di latenza seriale eliminata, RSS atteso
168MB→21MB per la sola quota NNUE — il delta osservato, ~164MB, combacia da vicino tenendo
conto dell'overhead fisso della TT da 64MB e di altre allocazioni). Questo guadagno non è
visibile nel bench a profondità fissa (misura solo il costo per nodo a thread singolo): si
manifesta nel gioco a tempo reale con più thread, dove riduce il tempo "sprecato" prima che la
ricerca vera e propria inizi.

**Verdetto**: KEPT.

---

## Step 4 — Reset() dell'accumulatore del worker alla costruzione

**Cambiamento**: `Reset()` veniva chiamato solo sul root evaluator (`main_game.cpp`), mai sui
worker creati da `SearchManager::Search()`. Il ply 0 di ogni worker restava con
`computed[]=false` per l'intera vita del worker (nulla lo corregge mai), quindi OGNI mossa di
radice, a OGNI profondità dell'iterative deepening, copiava quel `false` al ply 1 e innescava
un `RefreshAccumulator` completo invece del percorso incrementale — per l'intera durata della
ricerca, non solo all'inizio. Fix: `if (evaluator) evaluator->Reset(board);` nel costruttore di
`SearchWorker` (`SearchWorker.hpp`), cosi' il ply 0 e' marcato `computed[]=true` fin da subito.

**File**: `src/Search/include/SearchWorker.hpp`.

**Test**: `bench 12` — firma invariata (`86e814edf18440a2`), nodi identici. Istruzioni totali
sostanzialmente invariate (82.158B vs 82.092B di baseline, dentro il rumore di misura) — il
refresh completo del solo ply di radice costa poche centinaia di cicli per occorrenza, una
frazione trascurabile dei ~39.6 miliardi di cicli totali del bench. Nessun guadagno misurabile
in isolamento, come previsto dal piano.

**Verdetto**: KEPT — non per il guadagno diretto (trascurabile), ma perché è un prerequisito
necessario per l'accumulatore pigro (Step 8): la ricerca all'indietro deve poter contare su un
ply 0 già calcolato per terminare, e perché lascia il comportamento oggettivamente più corretto
(l'accumulatore di ogni worker è coerente fin dal primo utilizzo).

---

## Step 5a — Pulizia flag di build

**Cambiamento** (`makefile`): rimossi `-lcblas -lblas` (nessun simbolo BLAS mai usato in `src/`
— tutta la matematica NNUE è intrinsics AVX-512 scritti a mano); `-fopenmp` spostato solo sul
target `main_fen` (unico file che usa OpenMP — il motore UCI usa `std::thread` per il Lazy
SMP); aggiunte `-mavx512f -mavx512bw -mavx512dq -mavx512vl -mavx512vnni` esplicite (prima
ottenute solo implicitamente da `-march=native` — ora un errore di compilazione chiaro su una
macchina senza AVX-512/VNNI, non un'istruzione illegale a runtime); `-DNDEBUG` (gli `assert()`
di validazione aggiunti in questa sessione non costano nulla in produzione);
`-mprefer-vector-width=512`.

**Test**: `bench 12` — firma invariata, `main_fen` (perft) — tutte le 6 posizioni di test
passate a tutte le profondità. Confronto `perf stat` controllato (build precedente vs nuova,
stesso comando): istruzioni 82.158B→82.079B (-0.1%), cicli 39.274B→39.468B (+0.5%) — un pareggio
dentro il rumore di misura su questo laptop. Nessun guadagno di velocità misurabile da questi
flag specifici su questo carico di lavoro (`-march=native` già forniva implicitamente le
estensioni AVX-512; gli assert rimossi erano già economici).

**Verdetto**: KEPT — non per la velocità (nessun guadagno misurabile), ma per portabilità/igiene:
dipendenze di link morte rimosse, requisiti di CPU resi espliciti invece che impliciti
(fallimento chiaro in compilazione anziché istruzione illegale a runtime su un'altra macchina).

---

## Step 5b — Profile-Guided Optimization (PGO)

**Cambiamento**: nuovo target `make pgo` (`makefile`) — tre fasi automatizzate: (1) build
strumentata con `-fprofile-generate`, (2) esecuzione di `bench 9` come carico di allenamento
(sia NNUE sia HCE, cosi' entrambi i percorsi sono rappresentati nei dati di profilo), (3)
rebuild finale con `-fprofile-use -fprofile-correction -Wno-missing-profile`. Il flag
`PGO_FLAGS` (vuoto di default, valorizzato da riga di comando nelle chiamate ricorsive a
`$(MAKE)`) si propaga a `CXXFLAGS`/`CXXFLAGS_FEN`/`LDFLAGS`/`LDFLAGS_FEN` in quanto variabili
ricorsive del makefile.

**Test**: `bench 12` — firma invariata, `main_fen` (perft) — tutte le posizioni passate.
`perf stat` (stesso comando, confronto diretto con la build post-Step-5a):

| Metrica | Prima (post 5a) | Dopo (PGO) | Variazione |
| :--- | ---: | ---: | ---: |
| Istruzioni totali | 82.078.791.201 | 80.925.852.735 | **-1.4%** |
| Cicli totali | 39.467.863.307 | 38.956.243.318 | **-1.3%** |

Guadagno reale ma più piccolo della stima iniziale del piano (5-15%, una stima ragionata, non
misurata) — plausibile: quella stima era tipica di codice generico ricco di branch, mentre qui
il percorso caldo (`EvaluateNnue`, ~39% del tempo, vedi Step 2) è già una sequenza fissa di
intrinsics AVX-512 scritti a mano — la leva principale del PGO (previsione dei branch, euristiche
di inlining) ha meno margine su codice già esplicitamente vettorizzato e già aggressivamente
inlineato da `-O3 -flto`.

**Verdetto**: KEPT — piccolo ma gratuito e reale, nessun rischio, nessun cambio di semantica.

---

## Step 6 — Allineamento a 64 byte (L0, L1, stack degli accumulatori)

**Cambiamento**: `alignas(64)` su `NnueNetwork` (classe + membri `L0`, `L0Bias`, `L1`,
`L1Bias` — righe da 512B, multiplo esatto di 64) e su `NnueState` (struct + `accumulator` —
arrotonda anche lo stride di `nnueStack[MAX_HISTORY_PLY]` a un multiplo di 64, cosi' OGNI ply
e' allineato, non solo il primo). `L2`/`L2Bias`/`L3` restano non allineati (righe da 32B, non
multiplo di 64 — il layer intero e' 1KB, non e' dove va il tempo, vedi profilo Step 2).
`loadu`/`storeu` → `load`/`store` **solo dove provabilmente sicuro**: `AccumulateColumnsInto`,
`IncrementalUpdate` (`NnueEvaluator.cpp`), `ActivationFunction(const int16_t*, ...)` e il ramo a
64 byte di `DenseLayerLinear` (`NnueNetwork.cpp`) — quest'ultimo esegue solo per la chiamata L1
(cols=512): per L2 (cols=32) la condizione del loop e' sempre falsa, quindi e' codice morto per
L2 e gli assert di allineamento non vengono mai valutati su dati non allineati. Aggiunto
`assert((ptr & 63) == 0)` (gratis sotto `-DNDEBUG`) a ogni sito convertito.

**File**: `src/Evaluation/include/NNUE/NnueNetwork.hpp`, `src/Evaluation/src/NNUE/NnueNetwork.cpp`,
`src/Evaluation/include/NNUE/NnueEvaluator.hpp`, `src/Evaluation/src/NNUE/NnueEvaluator.cpp`.

**Verifica dell'allineamento** (il passo "rumoroso" per definizione — un'ipotesi sbagliata è un
`#GP`/SIGSEGV immediato, non un errore silenzioso): build di debug SENZA `-DNDEBUG` (assert
attivi) — `bench 10` completo, **zero fallimenti** su tutte le 16 posizioni. Solo dopo,
passaggio alla build di produzione.

**Test**: `bench 12` — firma invariata (`86e814edf18440a2`), nodi identici. `perf stat`
(confronto diretto, stessa configurazione non-PGO prima/dopo, isolando l'effetto):

| Metrica | Prima (post 5a, non allineato) | Dopo (Step 6) | Variazione |
| :--- | ---: | ---: | ---: |
| Istruzioni totali | 82.078.791.201 | 82.052.183.193 | ~0% (atteso: l'allineamento non riduce il conteggio di istruzioni) |
| Cicli totali | 39.467.863.307 | 36.564.705.590 | **-7.4%** |
| L1-dcache-load-misses | 1.974.319.881 (baseline Step 2) | 1.711.652.649 | **-13.3%** |

Esattamente l'effetto atteso: stesso numero di istruzioni ma molto meno costose (niente più
load "split" su righe da 512B che attraversavano un confine di cache line), dentro la stima
del piano (3-8%). NPS della release finale (PGO + allineamento): ~4.65-4.74M, contro il
~4.25-4.33M della baseline dello Step 2 — un ~10% complessivo da Step 5b+6 insieme.

**Verdetto**: KEPT.

---

## Step 7 — Riduzione ad albero (haddx4) in DenseLayerLinear

**Cambiamento**: `_mm512_reduce_add_epi32` e' una sequenza di ~7 istruzioni con una catena di
dipendenza seriale di ~14 cicli; il blocco a 4 righe di `DenseLayerLinear` la chiamava 4 volte
(una per accumulatore) — per L1 (8 gruppi di 4 righe = 32 chiamate a `_mm512_reduce_add_epi32`
per ogni `EvaluateNnue`) il costo della riduzione e' paragonabile al lavoro moltiplica-accumula
che dovrebbe solo totalizzare. Aggiunta `haddx4_epi32` (sequenza standard "affine_transform"
in stile Stockfish: quattro `_mm512_unpacklo/hi_epi32` → due `_mm512_add_epi32` →
`_mm512_unpacklo/hi_epi64` → `_mm512_add_epi32` → fold cross-lane a 256 e 128 bit) — 11
istruzioni per 4 somme contro ~28, nessuna catena seriale lunga (gli unpack sono in-lane, le 4
somme parziali restano separate per corsia fino alle due fold finali).

Attivata SOLO quando il ramo a 64 byte ha consumato tutte le colonne (`j == cols`, vero per L1
dove cols=512=multiplo esatto di 64; per L2, cols=32, quel ramo non esegue mai — j resta 0,
quindi si cade nel percorso esistente invariato, nessun rischio per L2). Bit-identico per
costruzione: l'addizione tra interi e' associativa ed esatta, qualunque albero di somma da' lo
stesso `int32`; i parziali di L1 sono limitati da 512×127×127≈8.3M, tre ordini di grandezza
sotto l'overflow di int32; `_mm_srai_epi32` e' uno shift aritmetico, coerente con `>>` su
interi con segno in GCC.

**File**: `src/Evaluation/src/NNUE/NnueNetwork.cpp` (`DenseLayerLinear`, nuova `haddx4_epi32`).

**Test**: `bench 12` — firma invariata, nodi identici; `main_fen` (perft) — tutto passato.
`perf stat` (stesso comando, confronto diretto col post-Step-6):

| Metrica | Prima (post Step 6) | Dopo (Step 7) | Variazione |
| :--- | ---: | ---: | ---: |
| Istruzioni totali | 82.052.183.193 | 76.884.276.662 | **-6.3%** |
| Cicli totali | 36.564.705.590 | 34.286.412.079 | **-6.2%** |

Guadagno superiore alla stima del piano (4-6%) — probabilmente perche' oltre a rimuovere le
4 riduzioni seriali, il `continue` post-haddx4 salta interamente il codice delle code a 32
byte/scalari per L1 (che con cols=512 non eseguivano mai comunque, ma il branch/i controlli di
loop restavano nel percorso di esecuzione prima).

**Verdetto**: KEPT.

---

## Step 8a — Fix overflow ply + MAX_HISTORY_PLY 1024→512

**Cambiamento**: `OnMakeMove` incrementava `currentPly` PRIMA di controllare i limiti — se mai
raggiunto, `currentPly` restava oltre `MAX_HISTORY_PLY` per sempre (UB su un array fuori
limiti), perché `OnUnmakeMove` lo decrementa incondizionatamente. Corretto: il controllo dei
limiti avviene prima dell'incremento; un nuovo contatore `overflowDepth` traccia le
make/unmake oltre il limite senza mai toccare `currentPly`, cosi' `OnUnmakeMove` lo srotola
correttamente prima di riprendere a decrementare `currentPly`. `MAX_HISTORY_PLY`
(`NnueConstants.hpp`) ridotto 1024→512 — prerequisito diretto per lo Step 8 completo (ogni ply
porterà anche un `AccDelta`), sicuro perché `SearchWorker::MAX_PLY` è 64 e i worker fanno
`Reset()` alla costruzione (Step 4): nessun worker si avvicina mai a questo limite.

**File**: `src/Evaluation/include/NNUE/NnueEvaluator.hpp`, `src/Evaluation/src/NNUE/NnueEvaluator.cpp`,
`src/Evaluation/include/NNUE/NnueConstants.hpp`.

**Test**: `bench 12` — firma invariata, nodi identici; `main_fen` — tutto passato; build
`NNUE_VALIDATE` — nessun fallimento. Nessun cambiamento di prestazioni misurabile (atteso — è
un fix di correttezza/bookkeeping su un caso limite praticamente irraggiungibile, non un
cambio del percorso caldo).

**Verdetto**: KEPT.

---

## Step 8b — Introduzione di AccDelta/BuildDelta (ancora eager)

**Cambiamento**: sostituita `IncrementalUpdate` (calcolava indici E aggiornava l'accumulatore
in un'unica funzione, chiamata separatamente per ciascuna prospettiva) con due funzioni: 
`BuildDelta` (nuova — calcola gli indici di feature da aggiungere/rimuovere per ENTRAMBE le
prospettive in un solo passaggio, usando gli stessi identici calcoli di prima) e `ApplyDelta`
(applica un `AccDelta` gia' calcolato a una prospettiva, stesso ciclo SIMD di prima). `OnMakeMove`
ora chiama `BuildDelta` una volta poi `ApplyDelta` per ciascuna prospettiva "computed" — ANCORA
eager (l'accumulatore sorgente e destinazione sono lo stesso, il memcpy `nnueStack[currentPly] =
nnueStack[prevPly]` resta presente). Scopo di questo step: dimostrare in isolamento che
`BuildDelta`+`ApplyDelta` riproduce ESATTAMENTE `IncrementalUpdate`, prima di rimuovere il
memcpy (Step 8c) e di andare pigri (Step 8d). Approfittando del passaggio, anche
`Board::GetHistory` e' ora per const-reference (`Board.hpp`) invece che per valore — una sola
fetch invece di due (controllo mossa-di-re + dentro `IncrementalUpdate`, ora dentro `OnMakeMove`
una volta sola).

**File**: `src/Evaluation/include/NNUE/NnueEvaluator.hpp`, `src/Evaluation/src/NNUE/NnueEvaluator.cpp`,
`src/Core/include/Board.hpp`.

**Test**: `bench 12` — firma invariata, nodi identici (prova diretta che `BuildDelta`+
`ApplyDelta` ≡ `IncrementalUpdate` bit-per-bit); `main_fen` — tutto passato; build
`NNUE_VALIDATE` su 5 posizioni mirate (arrocco entrambi i lati/colori, promozione, re-e-torre) —
zero fallimenti.

**Verdetto**: KEPT — nessun guadagno di velocità atteso o misurato a questo stadio (ancora
eager, stesso lavoro), è un passo di isolamento/verifica prima delle modifiche che contano.

---

## Step 8c — Rimozione del memcpy (copia e aggiornamento fusi)

**Cambiamento**: rimosso `nnueStack[currentPly] = nnueStack[prevPly]` (copiava l'intero
`NnueState` — 1088B con `alignas(64)` — a ogni `OnMakeMove`, per essere poi immediatamente
sovrascritto). `ApplyDelta` ora prende un parametro `ply` esplicito e legge la SORGENTE da
`nnueStack[ply-1]` scrivendo la DESTINAZIONE in `nnueStack[ply]` — un solo passaggio di
lettura+scrittura invece di copia (1024B lette + 1024B scritte) seguita da un secondo
aggiornamento (512B RMW). `OnMakeMove` ricostruito di conseguenza: ogni ply riparte con
`computed[]=false` esplicito (non più ereditato dal memcpy), e la decisione
ApplyDelta-vs-refresh guarda `nnueStack[prevPly].computed[]` (la sorgente reale) invece di
`nnueStack[currentPly].computed[]` (che prima ereditava lo stesso valore solo grazie al
memcpy ora rimosso). Il parametro `ply` esplicito in `ApplyDelta` è pensato per essere
riusato tale e quale nel replay in avanti dello Step 8d.

**File**: `src/Evaluation/include/NNUE/NnueEvaluator.hpp`, `src/Evaluation/src/NNUE/NnueEvaluator.cpp`.

**Test**: `bench 12` — firma invariata, nodi identici; `main_fen` — tutto passato; build
`NNUE_VALIDATE` (`bench 10` completo) — zero fallimenti. `perf stat` (confronto diretto col
post-Step-8b):

| Metrica | Prima (post 8b) | Dopo (Step 8c) | Variazione |
| :--- | ---: | ---: | ---: |
| Istruzioni totali | 76.884.276.662 | 74.528.000.409 | **-3.1%** |
| Cicli totali | 34.286.412.079 | 33.461.721.631 | **-2.4%** |

**Verdetto**: KEPT.

---

## Step 8d — Accumulatore pigro (TENTATO, risultato negativo, scartato)

**Cambiamento tentato**: rimandare l'applicazione del delta da `OnMakeMove` (eager) a
`Evaluate()`, tramite un nuovo `EnsureComputed(board, perspective)` che cammina all'indietro
fino al più vicino antenato "computed" e riproduce in avanti un `ApplyDelta` per ply — così
solo i ply DAVVERO valutati (i nodi di quiescence, ~54% del totale per il profilo dello Step 2)
pagano il costo dell'aggiornamento dell'accumulatore, non ogni `OnMakeMove`. Le mosse di re
continuano a rinfrescare eagerly (obbligatorio: il bucket king-relative cambia, non è
replayabile da un delta).

**Verifica di correttezza (superata)**: `bench 12` — firma invariata, nodi identici; build
`NNUE_VALIDATE` (bench 10 completo, ogni singola valutazione ri-verificata contro un refresh
completo indipendente) — zero fallimenti, il che valida implicitamente anche l'interazione con
le mosse nulle (NMP si attiva pesantemente a profondità 10). Lunghezza media della catena di
replay misurata empiricamente: **1.182** (massimo 11, su 40M+ chiamate) — ben sotto la soglia
di 3 del piano.

**Risultato di prestazioni (negativo)**: `perf stat`, confronto diretto con lo Step 8c:

| Metrica | Prima (post 8c) | Dopo (Step 8d) | Variazione |
| :--- | ---: | ---: | ---: |
| Istruzioni totali | 74.528.000.409 | 74.598.750.532 | +0.09% (sostanzialmente invariato) |
| Cicli totali | 33.461.721.631 | 33.577.075.602 | +0.3% (sostanzialmente invariato) |

Diagnosticato il perché con un conteggio diretto delle chiamate (build strumentata a parte):
`OnMakeMove`=26.422.794, `EnsureComputed`=41.676.434, `ApplyDelta`=47.309.647. La stima del
piano assumeva che il numero di `OnMakeMove` fosse vicino al numero totale di nodi (quindi che
l'eager avrebbe sprecato ~2×nodi chiamate ad `ApplyDelta` su rami mai valutati) — ma
`OnMakeMove` è risultato molto più raro di quanto stimato, e il limite superiore eager
(2×OnMakeMove≈52.8M, assumendo poche mosse di re) è solo ~10% sopra le 47.3M chiamate
effettive della versione pigra. La ragione: con l'ordinamento delle mosse di questo motore
(PVS/LMR), la maggior parte delle mosse tentate raggiunge comunque la quiescence prima di
tornare indietro — c'è molto meno "lavoro eager sprecato" da eliminare di quanto ipotizzato a
priori, quindi il guadagno teorico non si materializza.

**Decisione**: **SCARTATO, tornati allo stato dello Step 8c.** Per la disciplina di lavoro
("se un cambiamento non paga, ripristinalo e registralo") — nessun guadagno misurabile a
fronte di complessità reale aggiunta (nuova funzione `EnsureComputed`, ragionamento più
sottile sull'invariante "computed"). Rimossa `EnsureComputed` dal codice; `Evaluate()` e
`OnMakeMove` tornano alla forma eager dello Step 8c. Il parametro `ply` esplicito aggiunto ad
`ApplyDelta` nello Step 8c resta (innocuo, non aggiunge complessità reale).

**Verdetto**: SCARTATO (esito negativo onestamente registrato, come richiesto).

---

<!-- Righe successive: una per iterazione, aggiunte da questo punto in poi -->
