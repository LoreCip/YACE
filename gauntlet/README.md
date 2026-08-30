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
| **V3-3.1** | 256x2 -> 32 -> 32 -> 1 | `int` | Stessi pesi di V3-3 — fix del bug di scala nell'output NNUE lato Motore (vedi sotto), nessun retraining. |
| **V3-4** | 256x2 -> 32 -> 32 -> 1 | `int` | Nuovo training: fix scheduler LR, split train/validation, loss combinata WDL+logit-space. |
| **V3-5** | 256x2 -> 32 -> 32 -> 1 | `int` | Esperimento sul dataset (vedi sotto) — **risultato negativo, pesi non conservati.** |
| **V3-3.2** | 256x2 -> 32 -> 32 -> 1 | `int` | Stessi pesi di V3-4 — fix bug di segno nell'integrazione Ricerca/NNUE + fix export quantizzazione (vedi sotto), nessun retraining. |
| **V3-6** | 256x2 -> 32 -> 32 -> 1 | `int` | Nuovo training sul dataset con fix dell'inversione dei label (vedi sotto). **Modello di riferimento attuale.** |

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

### V3-4 — Nuovo training (2026-08-29)
`NNUE/training/trainingNNUE_HalfKP.py`: scheduler `OneCycleLR` dimensionato correttamente sulle
epoche rimanenti (con stato ripristinabile da checkpoint), split train/validation con un
sottoinsieme casuale (seed fisso) mai visto in training, e loss combinata 50% WDL
(probability-space) + 50% logit-space — quest'ultima evita la saturazione del sigmoide sulle
posizioni nettamente vinte/perse. Stessa architettura e stessi iperparametri di base (Adam,
lr=0.001, batch=16384, 40 epoche) di V3-3. Le statistiche sotto sono misurate su un vero split
di validazione (mai visto in training), non contaminate dal training set.

### V3-5 — Esperimento sul dataset di training (2026-08-29, risultato negativo)

Indagine sulla qualità del dataset `NNUE/data/clean_db_halfkp_balanced.bin` (91.2M posizioni,
derivate dal dump pubblico Lichess). Trovati due problemi reali nella pipeline
(`NNUE/data/scripts/cleanDataset.py` → `balanceDataset.py`, nessuna modifica al Motore):

1. **Nessuno shuffle in nessuno stadio della pipeline** — l'ordine delle righe rispecchia
   l'ordine originale dello stream Lichess. Un'analisi a blocchi ha trovato tratti di milioni
   di righe consecutive con statistiche diverse dal resto del file (es. l'ultimo ~7% aveva una
   varianza del target sistematicamente più stretta). Non influenzava la correttezza del
   training attuale (che già campiona indici casuali su tutto il file), ma è un rischio latente
   per qualunque consumo futuro sequenziale/a stream.
2. **Distribuzione dei target ancora molto concentrata vicino al pareggio** — nonostante lo
   sfoltimento Lorentziano esistente, il ~45% delle posizioni cadeva in `[0.4,0.6)`.

**Fix applicato — shuffle:** `balanceDataset.py` ora smista ogni riga tenuta in uno di 64 shard
temporanei durante la scansione a blocchi, poi permuta ogni shard e lo riaccoda al file finale
(due sole passate sull'intero dataset, nessuna modifica alla logica di filtro esistente).
Verificato: le statistiche per-segmento del file rigenerato sono ora identiche su tutto il file
(prima: deviazione standard target 0.144 in coda vs 0.189 altrove; dopo: 0.2297-0.2298 ovunque).

**Tentativo — allargare lo sfoltimento Lorentziano:** `gamma` 0.02→0.08, `depth` 0.85→0.98, per
appiattire la concentrazione in `[0.4,0.6)` (45%→21.5%). Retraining completo (40 epoche, stessa
pipeline di V3-4) su questo dataset ribilanciato (91.2M→55.3M righe, -40% volume):

> - **MAE (WDL):** 0.1882 — **Sign Accuracy:** 59.97% (split di validazione, mai visto in training)
> - **Errore relativo (MAE/std del target):** 0.816, contro **0.644 di V3-4** — un modello
>   nettamente peggiore, non solo per l'aumento naturale di scala della varianza del target.

**Diagnosi:** lo sfoltimento aggressivo ha probabilmente sottratto troppo volume di dati totale
e sovra-rappresentato le posizioni decisive rispetto alla reale distribuzione di gioco (dove le
posizioni vicine alla parità sono la maggioranza) — la concentrazione vicino a 0.5 non era
necessariamente un difetto da correggere. Confermato isolando le due variabili con due probe
brevi (8 epoche, stesso codice): un dataset shuffled-ma-non-ribilanciato (91.3M righe, stessa
distribuzione di V3-4) ha dato risultati statisticamente identici a un dataset non-shuffled con
lo stesso identico contenuto (MAE_WDL 0.1408 vs 0.1407, Sign Accuracy 57.98% vs 57.81%) — lo
shuffle da solo non altera la qualità del training con l'attuale pipeline (che già campiona
indici casuali indipendentemente dall'ordine fisico del file), quindi la regressione di V3-5 è
interamente attribuibile allo sfoltimento, non allo shuffle.

**Esito:** `balanceDataset.py` mantiene il fix dello shuffle (corretto, senza controindicazioni,
utile per il futuro) ma i parametri Lorentziani sono stati riportati ai valori originali
(`gamma=0.02, depth=0.85`, vedi il file per la nota). **V3-4 resta il modello di riferimento** —
non è stato eseguito un retraining completo sul dataset shuffled-non-ribilanciato perché i due
probe da 8 epoche già dimostrano che sarebbe statisticamente equivalente a V3-4. Nessun ulteriore
tentativo di ribilanciamento su questa stessa fonte dati (Lichess) è pianificato: la prossima leva
concreta per la qualità della rete è la generazione di dati via self-play (costosa, ore/giorni),
da valutare solo dopo aver esplorato altre opzioni di miglioramento.

### V3-3.2 — Fix bug di segno nell'integrazione Ricerca/NNUE + fix export quantizzazione (2026-08-29)

Analisi approfondita dell'intera sequenza di valutazione (Motore + pipeline dati) alla ricerca
di ulteriori bug oltre a quelli già corretti in V3-3.1. Trovati tre problemi indipendenti:

1. **Inversione del segno nei label di training** (`NNUE/data/scripts/cleanDataset.py`): il `cp`
   del dump Lichess è relativo al Bianco, ma le feature HalfKP (`us`/`them`) sono relative a chi
   deve muovere. Senza un flip esplicito, il target di training era invertito per ~metà del
   dataset (tutte le posizioni col Nero di turno) — confermato empiricamente decodificando gli
   indici `us`/`them` reali dal file binario e confrontando il bilancio di materiale lato-di-mossa
   col segno del target: correlazione quasi nulla (0.041), accordo di segno 51.3% (livello del
   caso). Questo era il vero collo di bottiglia dietro il plateau di V3-4/V3-5 al ~63% di sign
   accuracy — non gli iperparametri di training, non lo sbilanciamento del dataset.
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
sotto-versione **V3-3.2**, stessi pesi di V3-4, nessuna nuova Valutazione.

### V3-6 — Nuovo training con fix dell'inversione dei label (2026-08-29)

Rigenerata l'intera pipeline dati con il fix (1) sopra: `cleanDataset.py` (388,458,657 righe
grezze del dump Lichess → 233,951,854 posizioni filtrate, 60.23%) → `balanceDataset.py` (shuffle
+ sfoltimento Lorentziano originale, `mu=0.5, depth=0.85, gamma=0.02` → 91,248,622 posizioni,
39.0%) → retraining (stessa architettura e iperparametri di base di V3-4: Adam, lr=0.001,
batch=16384, split di validazione 2% casuale mai visto in training).

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
> Salto netto rispetto a V3-4 (MAE_WDL 0.1214, MAE_CP 221.20, Sign Accuracy 62.87%): il tasso di
> errore sul segno si è più che dimezzato (37.13%→16.40%). Conferma diretta che il collo di
> bottiglia era il bug di inversione dei label, non la capacità del modello o gli iperparametri
> di training.

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

> *Modello NNUE V3-4 (1.8M posizioni, split di validazione mai visto in training):*
> - **MAE (WDL):** 0.1214
> - **MAE (CP):** 221.20 pedoni
> - **Sign Accuracy:** 62.87%
>
> **NOTA sulla scala CP:** le misure V2/V3 sopra usano la vecchia conversione `logit*100`; da
> V3-4 in poi si usa la scala corretta `logit*400` (coerente col target di training
> `sigmoid(cp/400)`, vedi V3-3.1) — per un confronto diretto, i MAE(CP) di V2/V3 vanno
> moltiplicati per 4 (~228 e ~223 pedoni). Su questa base, V3-4 è sostanzialmente allineato a
> V3-3.1 in termini di accuratezza pura del modello: i fix di training (scheduler, loss
> combinata) hanno prodotto una misura finalmente affidabile, non un salto netto di qualità —
> il collo di bottiglia resta la capacità del modello/dati più che gli iperparametri di training.
---

## Stima ELO

### YACE_v3-3 vs Stockfish
*Condizioni del match: 100 partite. Valutazione: HCE (classica). Forza Stockfish limitata a 2000 Elo.*

#### Esito Finale
| Risultato | Totale | Percentuale |
| :--- | :--- | :--- |
| **Vittorie YACE_v5** | 30 | 30.0% |
| **Vittorie Stockfish** | 53 | 53.0% |
| **Patte** | 17 | 17.0% |

#### Valutazione Elo
> - **Differenza Elo:** -81.4
> - **Elo stimato YACE_v5:** 1919 *(Basato su SF a 2000)*

### YACE_v3-3.1 vs Stockfish
*Condizioni del match: 100 partite. Valutazione: NNUE. Forza Stockfish limitata a 2000 Elo.*

#### Esito Finale
| Risultato | Totale | Percentuale |
| :--- | :--- | :--- |
| **Vittorie YACE_v3-3.1** | 4 | 4.0% |
| **Vittorie Stockfish** | 95 | 95.0% |
| **Patte** | 1 | 1.0% |

#### Valutazione Elo
> - **Differenza Elo:** -530.7
> - **Elo stimato YACE_v3-3.1:** 1469 *(Basato su SF a 2000)*

### YACE_v3-4 vs Stockfish
*Condizioni del match: 100 partite. Valutazione: NNUE. Forza Stockfish limitata a 2000 Elo.*

#### Esito Finale
| Risultato | Totale | Percentuale |
| :--- | :--- | :--- |
| **Vittorie YACE_v3-4** | 2 | 2.0% |
| **Vittorie Stockfish** | 98 | 98.0% |
| **Patte** | 0 | 0.0% |

#### Valutazione Elo
> - **Differenza Elo:** -676.1
> - **Elo stimato YACE_v3-4:** 1324 *(Basato su SF a 2000)*
>
> Risultato coerente (entro il rumore statistico di un campione di 100 partite) con V3-3.1 —
> la qualità del modello (sign accuracy ~63% su dati mai visti in training) è comparabile, non
> è il salto netto sperato dai soli fix di training (scheduler, loss combinata). Conferma che
> il collo di bottiglia principale resta la capacità del modello/dati, non gli iperparametri.

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
> Primo modello NNUE a superare nettamente sia le versioni precedenti (V3-4: 1324, V3-3.1: 1469)
> sia la valutazione classica HCE (V3-3: 1919) — un salto di +277 Elo su HCE e di +727/+872 Elo
> sulle precedenti versioni NNUE. Conferma che il salto nella sign accuracy offline (63%→83.6%,
> vedi sopra) si traduce direttamente in forza di gioco: il collo di bottiglia era davvero il bug
> di inversione dei label più il bug di segno nell'integrazione Ricerca/NNUE, non l'architettura
> o gli iperparametri di training.

---

## Costo computazionale: HCE vs NNUE (NPS)

*Misurato con `go nodes 5000000`, singolo thread (`-c 1`), mediana di 3 run per posizione/motore
(V3-4, `nnue_weights_v3-4.bin`).*

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
relativamente fisso per mossa. Questo costo per-nodo, sommato al fatto che la rete non è
ancora più forte di HCE in termini di qualità (vedi sopra), significa che a parità di tempo di
calcolo la ricerca con NNUE esplora un albero più piccolo per un guadagno di valutazione che
al momento non compensa — un argomento in più, insieme ai dati di allenamento ancora sbilanciati
(vedi sotto), per non investire ancora in un ampliamento della rete (M/K più grandi) prima di
aver risolto la qualità dei dati.