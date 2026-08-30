## Legenda
Il formato è **V[Motore]-[Valutazione]**:

* **V[n]-[a,b,...]**: Valutazione basata su logiche algoritmiche/manuali.
* **V[n]-[1,2,...]**: Valutazione basata su modelli addestrati.

---

## Serie V1 (Architettura Base)
*Motore: Alpha-beta pruning, QuiescenceSearch, Killer Moves, Transposition tables.*

- **V1-a**: Posizionale.
- **V1-b**: Posizionale + Simple PSTs (Piece-Square Tables).
- **V1-0**: Pesi casuali (double).
- **V1-1**: Architettura `256x2 -> 32 -> 1` (double).

---

## Serie V2 (Architettura Ottimizzata)
*Motore: Architettura V1 + On-demand selection sort, Efficient summary bitboard, Incremental Zobrist, Null Pruning, PVS, LMR, Profiling, Delta Pruning, Magic bitboards, Lazy SMP.*

### Modelli Addestrati
| Versione | Architettura | Precisione | Note |
| :--- | :--- | :--- | :--- |
| **V2-2** | 256x2 -> 32 -> 1 | `int` | Epoca 35 |
| **V2-3** | 256x2 -> 32 -> 32 -> 1 | `int` | Epoca 38 |
| **V3-3** | 256x2 -> 32 -> 32 -> 1 | `int` | Epoca 38 |

## Serie V3 (Architettura Ottimizzata)
*Motore: Architettura V2 + NNUE parallelization, SEE in Quiescence Search*

### Modelli Addestrati
| Versione | Architettura | Precisione | Note |
| :--- | :--- | :--- | :--- |
| **V3-3** | 256x2 -> 32 -> 32 -> 1 | `int` | Epoca 38 |
| **V3-3.1** | 256x2 -> 32 -> 32 -> 1 | `int` | Stessi pesi di V3-3 — fix del bug di scala nell'output NNUE lato Motore. |
| **V3-3.2** | 256x2 -> 32 -> 32 -> 1 | `int` | Stessi pesi di V3-4 — fix bug di segno nell'integrazione Ricerca/NNUE + fix export quantizzazione. |
| **V3-6** | 256x2 -> 32 -> 32 -> 1 | `int` | Nuovo training con fix dell'inversione dei label; ri-esportato dopo il fix del bug delle mosse di re (vedi sotto). **Modello di riferimento attuale.** |

### V3-3.1 — Fix bug di scala nell'output NNUE (2026-08-29)
`NnueNetwork::OutputLayerDotProduct` (Motore, C++) restituiva `dot >> SHIFT_OUT` come punteggio
"cp" finale — il logit grezzo quantizzato, senza alcun fattore di conversione in centipawns e
con uno shift (`÷16384`) che non corrisponde né alla vera scala di esportazione dei pesi
(`SCALE_ACT*SCALE_W_OUT=16129` in `convertToWeights.py`) né alla scala del target di training
(`sigmoid(cp/400)` in `cleanDataset.py`). Risultato: i punteggi NNUE che arrivavano alla ricerca
erano ordini di grandezza troppo piccoli (es. `score cp 1` per una posizione di donna in più,
invece di ~600). Corretto con una divisione esatta e arrotondata sulla scala reale
(`OUTPUT_SCALE=16129`, `CP_SCALE=400`); verificato via UCI e per confronto diretto con l'output
del modello Python (`prob_to_cp`, anch'esso corretto alla stessa scala) sulle stesse FEN. Stessi
pesi di V3-3, nessun retraining — è una correzione del Motore, non una nuova Valutazione.

### V3-3.2 — Fix bug di segno nell'integrazione Ricerca/NNUE + fix export quantizzazione (2026-08-29)

Analisi approfondita dell'intera sequenza di valutazione (Motore + pipeline dati) alla ricerca
di ulteriori bug oltre a quello già corretto in V3-3.1. Trovati tre problemi indipendenti:

1. **Inversione del segno nei label di training** (`NNUE/data/scripts/cleanDataset.py`): il `cp`
   del dump Lichess è relativo al Bianco, ma le feature HalfKP (`us`/`them`) sono relative a chi
   deve muovere. Senza un flip esplicito, il target di training era invertito per ~metà del
   dataset (tutte le posizioni col Nero di turno) — confermato empiricamente decodificando gli
   indici `us`/`them` reali dal file binario e confrontando il bilancio di materiale lato-di-mossa
   col segno del target: correlazione quasi nulla (0.041), accordo di segno 51.3% (livello del
   caso). Questo era il vero collo di bottiglia dietro il plateau delle versioni precedenti al
   ~63% di sign accuracy — non gli iperparametri di training, non lo sbilanciamento del dataset.
2. **Bug di segno nell'integrazione Ricerca/NNUE** (`NnueEvaluator::Evaluate`, Motore C++):
   `network.EvaluateNnue()` restituisce un punteggio relativo a chi deve muovere (convenzione
   standard `us`/`them`), ma il contratto di `IEvaluator::Evaluate()` stabilito da
   `ClassicalEvaluator` (e assunto da `SearchWorker::QuiescenceSearch`, che nega solo se tocca al
   Nero) è relativo al Bianco. Senza una conversione esplicita, ogni valutazione NNUE con il Nero
   di turno in quiescence veniva negata due volte, invertendo il segno per metà di tutte le
   posizioni valutate durante la ricerca. Corretto aggiungendo la conversione mancante; verificato
   con un harness standalone che valuta direttamente `Evaluate()` su posizioni speculari
   colore-invertite (simmetria di specchio confermata).
3. **Due bug di scala nell'export dei pesi** (`NNUE/training/convertToWeights.py`): `L0`/`L0Bias`
   (accumulatore) venivano quantizzati a `SCALE_W_ACC=255` invece di `SCALE_ACT=127` — la scala
   che il clamp `QA_BOUND=127` della Clipped ReLU lato C++ applica realmente alla somma grezza
   dell'accumulatore, senza alcuna riscala intermedia. Questo causava un troncamento indebito
   confermato su circa il 5% delle attivazioni. `L1Bias` aveva lo stesso problema
   (`SCALE_W_ACC*SCALE_W_L1=16320` invece di `SCALE_ACT*SCALE_W_L1=8128`, la scala reale
   dell'input di L1 dopo l'attivazione). Corretti entrambi; `L2Bias`/`L3` erano già corretti.

Il fix (1) richiede un retraining completo (vedi V3-6 sotto). I fix (2) e (3) sono lato
Motore/export e si applicano a qualunque checkpoint esistente senza retraining — da qui la nuova
sotto-versione **V3-3.2**, nessuna nuova Valutazione.

### V3-6 — Nuovo training con fix dell'inversione dei label (2026-08-29)

Rigenerata l'intera pipeline dati con il fix (1) di V3-3.2: `cleanDataset.py` (388,458,657 righe
grezze del dump Lichess → 233,951,854 posizioni filtrate, 60.23%) → `balanceDataset.py` (shuffle
+ sfoltimento Lorentziano originale, `mu=0.5, depth=0.85, gamma=0.02` → 91,248,622 posizioni,
39.0%) → retraining (stessa architettura e iperparametri di base: Adam, lr=0.001, batch=16384,
split di validazione 2% casuale mai visto in training, scheduler `OneCycleLR`, loss combinata
50% WDL + 50% logit-space).

**Sorveglianza overfitting**: un watchdog automatico ha monitorato `MAE_CP` sul validation set
dopo ogni epoca, con l'obiettivo di fermare il training se il segnale di generalizzazione si
fosse fermato pur avendo un checkpoint salvato a ogni epoca. Il training è stato arrestato
all'epoca 22/40: la loss di training continuava a scendere monotonicamente (`WDL: 0.0090→0.0080`
tra le epoche 18 e 21) mentre le metriche di validazione si erano appiattite/leggermente
peggiorate (`SignAcc` 83.60%→83.48%, `MAE_CP` non più in miglioramento netto) — la firma classica
di un training che continua a fittare rumore specifico del training set senza più generalizzare,
per quanto di ampiezza contenuta. Selezionato come modello finale il checkpoint dell'**epoca 18**
(miglior compromesso `SignAcc`/`MAE_CP` tra gli ultimi minimi validi).

> *Modello NNUE V3-6, epoca 18 (1.8M posizioni, split di validazione mai visto in training):*
> - **MAE (WDL):** 0.0697
> - **MAE (CP):** 130.61 pedoni
> - **Sign Accuracy:** 83.60%
>
> Salto netto rispetto alle versioni precedenti (MAE_WDL 0.1214, MAE_CP 221.20, Sign Accuracy
> 62.87%): il tasso di errore sul segno si è più che dimezzato (37.13%→16.40%). Conferma diretta
> che il collo di bottiglia era il bug di inversione dei label, non la capacità del modello o gli
> iperparametri di training.

**Release**: `make release V=3-6` — Motore invariato rispetto a V3-3.2 (solo refactoring interno
non funzionale di `main_game.cpp`, verificato via diff), nuovi pesi `nnue_weights_v3-6.bin`.

### Fix bug accumulatore mosse di re (2026-08-30)

Analisi in vista di un'ottimizzazione delle prestazioni ha trovato un bug di correttezza
indipendente: `NnueEvaluator::IncrementalUpdate` trattava ogni mossa di re come una feature
HalfKP normale. Siccome il re non è mai codificato come feature (`PIECE_TO_NNUE_IDX[KING] ==
-1`), questo produceva un indice fuori scala — o, per `king_sq` diverso da zero, un indice
*in scala ma sbagliato*, alias di un'altra riga dei pesi — corrompendo silenziosamente
l'accumulatore della prospettiva avversaria a ogni mossa di re. Corretto: una mossa di re non
aggiunge/rimuove più alcuna feature per la prospettiva altrui (mentre catture e torri
d'arrocco, che sono features reali, restano gestite normalmente).

Verificato con: una nuova modalità di build `NNUE_VALIDATE` che ricalcola da zero ogni
accumulatore e lo confronta byte-per-byte con quello incrementale (crash immediato sul bug,
zero fallimenti dopo il fix su 11 posizioni variate); simmetria speculare esatta
(`eval(pos) == -eval(mirror)`) su 5 coppie di posizioni; accordo con Stockfish su 48 posizioni
(68.8% col fix, 62.5% senza, 10/48 posizioni con mossa scelta diversa). Gauntlet di
ri-baseline: **2196 → 2258 Elo** (+62, dentro il rumore statistico di un campione da 100
partite ma coerente in direzione). Pesi V3-6 ri-esportati e binario ricostruito via
`make release V=3-6`.

### Fix bug di lettura fuori dai limiti in SEE (2026-08-30)

Costruendo lo strumento di misura per l'ottimizzazione della velocità (comando `bench`, vedi
sotto), è emerso che il motore **non era deterministico**: la stessa identica posizione, alla
stessa profondità, a thread singolo, con budget di tempo virtualmente illimitato (quindi senza
possibilità che scada il thread del timer), produceva risultati diversi tra un'esecuzione e
l'altra in proporzioni vicine al 50/50 — riproducibile anche con la valutazione classica (HCE),
quindi non legato all'NNUE. Diagnosticato con `AddressSanitizer`/`UndefinedBehaviorSanitizer`:
`SearchWorker::SEE` (usata per filtrare le catture in quiescence) assumeva che ogni mossa
generata da `GenerateCaptureMoves` catturasse un pezzo, ma quella funzione include anche le
**promozioni silenziose** (`PRQUEEN` su casa vuota) — per queste, `capturedPiece` è
`PieceType::NONE` (valore enum 6), e `pieceValues[6]` è una lettura fuori dai limiti su un
array di 6 elementi (indici 0-5). In produzione questo non andava in crash ma restituiva byte
arbitrari adiacenti in memoria, con un valore diverso a ogni esecuzione — da cui la
valutazione SEE (e quindi le decisioni di potatura in quiescence) casuale.

**Fix**: gain iniziale nullo per una promozione silenziosa (`capturedPiece == NONE`), invece di
leggere fuori dai limiti dell'array. Verificato: build ASan+UBSan pulita, e **10/10 esecuzioni
identiche** (nodi, punteggio, firma) sulla posizione che prima divergeva quasi a caso, sia per
NNUE sia per HCE. Gauntlet di ri-baseline: **2258 → 2330 Elo** (+72, un guadagno reale — questo
bug falsava la potatura in quiescence per qualunque posizione con pedoni prossimi alla
promozione, molto più comune del bug delle mosse di re).

### Statistiche di Performance
> *Modello NNUE V2 (15M posizioni, campionate dal file di training):*
> - **MAE (WDL):** 0.1251
> - **MAE (CP):** 57.12 pedoni
> - **Deviazione Standard (CP):** 78.09 CP
> - **Sign Accuracy:** 64.00%

> *Modello NNUE V3 / V3-3.1 (15M posizioni, campionate dal file di training):*
> - **MAE (WDL):** 0.1222
> - **MAE (CP):** 55.78 pedoni
> - **Deviazione Standard (CP):** 76.65 CP
> - **Sign Accuracy:** 64.53%
>
> **NOTA sulla scala CP:** le misure V2/V3 sopra usano la vecchia conversione `logit*100`; da
> V3-3.2/V3-6 in poi si usa la scala corretta `logit*400` (coerente col target di training
> `sigmoid(cp/400)`, vedi V3-3.1) — per un confronto diretto, i MAE(CP) di V2/V3 vanno
> moltiplicati per 4 (~228 e ~223 pedoni).

---

## Stima ELO

### YACE_v3-3 vs Stockfish
*Condizioni del match: 100 partite. Valutazione: HCE (classica). Forza Stockfish limitata a 2000 Elo.*

#### Esito Finale
| Risultato | Totale | Percentuale |
| :--- | :--- | :--- |
| **Vittorie YACE_v3-3** | 30 | 30.0% |
| **Vittorie Stockfish** | 53 | 53.0% |
| **Patte** | 17 | 17.0% |

#### Valutazione Elo
> - **Differenza Elo:** -81.4
> - **Elo stimato YACE_v3-3:** 1919 *(Basato su SF a 2000)*

### YACE_v3-6 vs Stockfish
*Condizioni del match: 100 partite. Valutazione: NNUE (V3-6, fix inversione label + fix Motore V3-3.2). Forza Stockfish limitata a 2000 Elo.*

#### Esito Finale
| Risultato | Totale | Percentuale |
| :--- | :--- | :--- |
| **Vittorie YACE_v3-6** | 70 | 70.0% |
| **Vittorie Stockfish** | 19 | 19.0% |
| **Patte** | 11 | 11.0% |

#### Valutazione Elo
> - **Differenza Elo:** +195.5
> - **Elo stimato YACE_v3-6:** 2196 *(Basato su SF a 2000)*
>
> Primo modello NNUE a superare nettamente sia le versioni precedenti sia la valutazione classica
> HCE (V3-3: 1919) — un salto di +277 Elo su HCE. Conferma che il salto nella sign accuracy
> offline (63%→83.6%, vedi sopra) si traduce direttamente in forza di gioco: il collo di
> bottiglia era davvero il bug di inversione dei label più il bug di segno nell'integrazione
> Ricerca/NNUE, non l'architettura o gli iperparametri di training.

---

## Costo computazionale: HCE vs NNUE (NPS)

*Misurato con `go nodes 5000000`, singolo thread (`-c 1`), mediana di 3 run per posizione/motore
(V3-4, `nnue_weights_v3-4.bin`).*

> **Nota di correzione (2026-08-30)**: il parser UCI (`src/Core/src/Uci.cpp`) non ha mai
> interpretato il token `nodes` di `go` (riconosce solo `movetime`, `wtime`/`btime`, `depth`) —
> la misura sopra non era quindi vincolata a 5.000.000 nodi come descritto, ma al comportamento
> di default (`depth=100`, `movetime=1000ms`). Il rapporto HCE/NNUE riportato resta comunque
> indicativo (entrambi i motori misurati con lo stesso metodo, quindi il confronto relativo
> tiene), ma per misure riproducibili e vincolate a una profondità fissa usare da ora il comando
> `bench [depth]` (vedi `gauntlet/optimization_log.md`), che gioca un set fisso di posizioni a
> profondità fissa con tempo illimitato — nessuna dipendenza dal parsing di `nodes`.

| Posizione | NPS (HCE) | NPS (NNUE) | Rapporto |
| :--- | :--- | :--- | :--- |
| Startpos (apertura) | 11.936M | 4.469M | 2.67x |
| Mediogioco tattico | 14.771M | 5.802M | 2.55x |
| Finale (pochi pezzi) | 18.261M | 6.298M | 2.90x |

NNUE gira a circa 1/2.5-1/2.9 della velocità di HCE su tutte le fasi di gioco. Questo è
coerente con la struttura di costo della NNUE: l'aggiornamento dell'accumulatore
(`OnMakeMove`/`OnUnmakeMove`, AVX-512) viene pagato su **il 100% dei nodi** dell'albero, non
solo ai nodi foglia — mentre HCE non ha alcun costo incrementale per mossa. Il rapporto è più
alto nel finale (2.90x) perché con meno pezzi il costo per nodo di HCE scala verso il basso
(meno bit da iterare per popcount/PST), mentre il costo dell'accumulatore NNUE resta
relativamente fisso per mossa.

---

## Ottimizzazione di velocità NNUE (2026-08-30)

Obiettivo: ridurre il costo per nodo della valutazione NNUE (senza ritoccare pesi o
architettura) per recuperare profondità di ricerca a parità di tempo. Un cambiamento per
iterazione, ciascuno misurato e verificato bit-per-bit contro una firma di regressione
(comando `bench`, nuovo — vedi sotto) prima di procedere al successivo. Log completo,
comprese le misure `perf stat` e i risultati negativi, in `gauntlet/optimization_log.md`.

**Due bug di correttezza trovati e corretti per primi** (non ottimizzazioni — cambiano
l'output, quindi testati e ri-baselinati separatamente prima di qualunque lavoro sulla
velocità):
1. **Bug accumulatore mosse di re**: `IncrementalUpdate` trattava il re come una feature
   HalfKP normale, corrompendo silenziosamente l'accumulatore avversario a ogni mossa di re.
2. **Bug di lettura fuori dai limiti in SEE**: `GenerateCaptureMoves` include le promozioni
   silenziose (nessuna cattura), per cui `SEE` indicizzava `pieceValues[6]` (fuori da un array
   di 6 elementi) — un vero bug di sicurezza della memoria (non solo di logica), che rendeva
   **l'intero motore non deterministico** (stesso identico binario, stessa posizione, stessa
   profondità → risultati diversi ~50/50 tra un'esecuzione e l'altra). Diagnosticato con
   AddressSanitizer dopo aver notato che disattivare l'ASLR (`setarch -R`) rendeva tutto
   deterministico.

| Fix | Elo (100 partite vs SF@2000) |
| :--- | ---: |
| Baseline (V3-6 come misurato in precedenza) | 2196 |
| + fix accumulatore mosse di re | 2258 (+62) |
| + fix lettura fuori dai limiti in SEE | 2330 (+72) |

**Ottimizzazioni di velocità** (bit-identiche per costruzione — stessa firma `bench` prima e
dopo ogni step; dettagli e numeri `perf stat` completi in `optimization_log.md`):

| Step | Cambiamento | Effetto misurato |
| :--- | :--- | :--- |
| Bench harness | Comando `bench [depth]`, firma di regressione FNV-1a | (strumento di misura) |
| Rete condivisa | Un solo caricamento dei pesi condiviso tra thread invece di N riletture/copie | -53% latenza per `go`, -61% RSS di picco (8 thread) |
| Reset worker | `Reset()` dell'accumulatore alla costruzione del worker | correttezza/igiene, prerequisito per accumulatore pigro |
| Flag di build | `-lcblas -lblas` rimossi, requisiti AVX-512 espliciti, `-DNDEBUG` | portabilità (nessun guadagno di velocità misurabile) |
| PGO | `make pgo`: build strumentata → carico `bench` → rebuild finale | -1.4% istruzioni, -1.3% cicli |
| Allineamento a 64B | `alignas(64)` su L0/L1/accumulatori, load/store allineati dove sicuro | -7.4% cicli, -13.3% L1-dcache-load-misses |
| Riduzione ad albero (haddx4) | Sostituita la riduzione seriale in `DenseLayerLinear` | -6.3% istruzioni, -6.2% cicli |
| Rimozione memcpy | Copia+aggiornamento dell'accumulatore fusi in un solo passaggio | -3.1% istruzioni, -2.4% cicli |
| Accumulatore pigro | **Tentato, risultato negativo (~0%), scartato** — vedi sotto | — |

**Risultato complessivo**: dalla baseline dello Step 2 (39.648 miliardi di cicli, `bench 12`) al
risultato finale (~33.5 miliardi di cicli): **~-15.5% cicli complessivi**, NPS da ~4.25-4.33M a
~4.9-5.1M (**+~15-17%**). Validazione finale: 100 partite vs Stockfish@2000, **2338 Elo** —
coerente (entro il rumore statistico di n=100) con il 2330 misurato dopo i soli fix di
correttezza, a conferma che il lavoro sulla velocità non ha introdotto regressioni.

---

## Tentativi non riusciti / non conclusivi

- **V3-3.1 in partita (1469 Elo)** — bug di segno nell'integrazione Ricerca/NNUE (vedi V3-3.2)
  ancora presente: metà delle valutazioni NNUE in quiescence erano invertite.
- **V3-4 — nuovo training (scheduler `OneCycleLR`, split di validazione casuale, loss combinata
  WDL+logit)**: 1324 Elo in gauntlet, nessun miglioramento (dati di training ancora con label
  invertiti, non diagnosticato a quel punto). I fix di metodologia sono comunque stati mantenuti
  nella pipeline e riusati per V3-6.
- **V3-5 — allargamento dello sfoltimento Lorentziano** (`gamma` 0.02→0.08, `depth` 0.85→0.98) per
  ridurre la concentrazione di posizioni vicine al pareggio: risultato nettamente peggiore (Sign
  Accuracy 59.97% vs 64% baseline, errore relativo 0.816 vs 0.644) — probabilmente per sottrazione
  di volume dati e sovra-rappresentazione delle posizioni decisive. Isolato con due probe da 8
  epoche: la causa era interamente lo sfoltimento, non lo shuffle introdotto nello stesso passaggio
  (che è stato mantenuto). Parametri Lorentziani ripristinati agli originali.
- **Accumulatore NNUE pigro** (rimandare l'aggiornamento dell'accumulatore da `OnMakeMove` a
  `Evaluate()`, così solo i nodi davvero valutati ne pagano il costo): corretto bit-per-bit
  (firma `bench` invariata, `NNUE_VALIDATE` pulito, lunghezza media della catena di replay
  1.182) ma nessun guadagno di velocità misurabile (+0.09% istruzioni, sostanzialmente
  invariato). Diagnosticato con un conteggio diretto delle chiamate: l'ordinamento delle mosse
  di questo motore (PVS/LMR) fa sì che la maggior parte delle mosse tentate raggiunga comunque
  la quiescence, quindi c'è molto meno "lavoro eager sprecato" da eliminare di quanto stimato a
  priori. Scartato, tornati alla versione eager con copia+aggiornamento fusi (vedi sopra).
