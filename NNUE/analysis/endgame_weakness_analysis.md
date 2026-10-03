# Analisi: debolezza della NNUE (V3-6) nei finali con poco materiale

Studio comparativo tra le predizioni della rete V3-6 (`run_v6/checkpoints/halfkp_epoch_18_final.pth`,
lo stesso checkpoint da cui e' esportato `nnue_weights_v3-6.bin`) e le valutazioni di Stockfish
(il target del dataset, `sigmoid(cp/400)` con `cp` dalle analisi Stockfish del dump Lichess —
vedi `dataset/build_dataset.py`). Misurato sul VERO split di validazione (`seed=42`, 2% del dataset,
1.824.972 posizioni mai viste in training — stesso metodo di `evaluate_holdout` in
`training/train.py`).

Script usati: `evaluate_model.py` (base), `heldout_by_phase.py`, `endgame_by_material.py`,
`one_piece_positions.py` (tutti in questa cartella).

## 1. Il sintomo iniziale

Stratificando l'errore per fase di gioco (numero di pezzi non-Re attivi):

| Fase | MAE (cp) | Sign Accuracy |
| :--- | ---: | ---: |
| Apertura (>=24 pezzi) | 72.9 | 82.9% |
| Mediogioco (14-23 pezzi) | 129.8 | 83.2% |
| Finale (6-13 pezzi) | 172.8 | **86.0%** |
| Finale avanzato (<6 pezzi) | 174.1 | **73.2%** |

La sign accuracy migliora progressivamente nel finale (86.0%) ma poi crolla nei finali con meno
di 6 pezzi (73.2%) — peggio del mediogioco, nonostante ci sia molto meno da valutare. Il resto
del documento scompone questo fenomeno per capire cosa manca esattamente.

## 2. Scomposizione per tipo di materiale — due problemi distinti

Decodificando il tipo di pezzo direttamente dagli indici HalfKP (`endgame_by_material.py`,
`one_piece_positions.py`) emergono **due cause completamente diverse**, non una sola:

### 2a. K+pezzo-minore-solo vs K (Cavallo o Alfiere): vuoto di dati, non un problema di training

Su un campione di 10.000.000 di posizioni dell'intero dataset, le posizioni con esattamente 1
pezzo non-Re si distribuiscono cosi':

| Pezzo | Occorrenze | % dei casi a 1 pezzo |
| :--- | ---: | ---: |
| Pedone | 1213 | 92.60% |
| Torre | 94 | 7.18% |
| Donna | 3 | 0.23% |
| **Cavallo** | **0** | **0.00%** |
| **Alfiere** | **0** | **0.00%** |

**Zero occorrenze**, non semplicemente rare. Motivo strutturale: K+pezzo-minore-solo vs K e' un
pareggio teorico noto (materiale insufficiente per dare scacco matto) — nella pratica reale
nessuno fa analizzare a Stockfish una posizione palesemente patta con un solo cavallo o alfiere
in campo, quindi il dump Lichess (da cui deriva tutto il dataset) non puo' contenere questi
casi, indipendentemente da come si costruisce la pipeline dati a valle.

**Questo non e' un pattern che si possa imparare dai dati** — e' una regola fissa degli scacchi
("Re + al massimo un pezzo minore, niente pedoni = patta"), quindi cercare di insegnarlo alla
rete tramite piu' dati sarebbe uno spreco. La soluzione corretta e' un controllo lato codice
(motore), non lato rete: rilevare "materiale insufficiente" prima di chiamare l'evaluator (NNUE
o HCE) e ritornare direttamente il punteggio di patta, esattamente come gia' avviene per la
regola delle 50 mosse e la ripetizione (`SearchWorker.cpp`). **Verificato: questo controllo non
esiste da nessuna parte nel codice attuale** (`grep -rn "insufficient\|InsufficientMaterial"
src/` non trova nulla).

Il crollo di sign accuracy all'8.98% osservato inizialmente sul bucket "esattamente 1 pezzo" e'
in realta' quasi interamente un artefatto della metrica: e' dominato da posizioni K+Pedone vs K
dove il target di Stockfish e' quasi sempre **esattamente 0** (patta) — con un target a zero,
qualunque predizione non-zero ha il 50% di probabilita' di "sbagliare segno" per puro caso,
senza che sia un vero errore di direzione (le magnitudini predette, tipicamente 10-40cp, sono
in realta' ragionevoli per posizioni oggettivamente quasi patte).

### 2b. K+Torre vs K, K+Donna vs K (e altri finali sparsi ma DECISIVI): vero problema di calibrazione

A differenza del caso precedente, questi NON sono patte automatiche — Stockfish li valuta
correttamente come nettamente vinti, e la rete **indovina quasi sempre la direzione giusta**
(K+Torre vs K: 100% sign accuracy nel campione osservato, n=16) — il problema e' che
**sottostima sistematicamente quanto sia vinta la posizione**, e questa sottostima peggiora
proprio quando il vantaggio reale e' piu' grande, SOLO nei finali sparsi:

Pendenza di regressione (predizione/target, senza intercetta) per fascia di vantaggio:

| \|target cp\| | Finale avanzato (<6 pezzi) | Resto della partita (>=6 pezzi) |
| :--- | ---: | ---: |
| 200-400 | 0.879 | 0.698 |
| 400-700 | 0.725 | 0.741 |
| >=700 | **0.582** | 0.683 |

Nel resto della partita la sottoconfidenza (un effetto noto di saturazione del training con
target `sigmoid(cp/400)`) resta stabile intorno a 0.68-0.74 indipendentemente da quanto sia
grande il vantaggio reale. Nei finali sparsi, invece, la pendenza **continua a scendere** man
mano che il vantaggio cresce — a vantaggi schiaccianti (>=700cp, tipicamente K+Torre/Donna vs
K) la rete esprime solo il 58% della magnitudine che dovrebbe, contro il 68% altrove.

**Causa: scarsita' di dati, non un limite dell'architettura.** Le posizioni con <6 pezzi sono
solo il 4.3% del dataset; al loro interno, le posizioni con vantaggio >=700cp E poco materiale
sono ancora piu' rare, perche' i dati vengono da PARTITE REALI (dump Lichess) — chi si trova
con un Re nudo contro Re+Torre di solito si arrende o subisce matto rapidamente, quindi poche
di queste posizioni specifiche vengono sottoposte ad analisi Stockfish profonda e finiscono nel
dataset. La rete ha visto abbastanza esempi per imparare la DIREZIONE (segno) ma non abbastanza
per calibrare correttamente la MAGNITUDINE in questo regime specifico.

## 2c. Conferma: apertura e mediogioco NON mostrano questo problema

Stessa pendenza di regressione, calcolata per apertura e mediogioco:

| \|target cp\| | Apertura (>=24 pezzi) | Mediogioco (14-23 pezzi) |
| :--- | ---: | ---: |
| 200-400 | 0.609 | 0.654 |
| 400-700 | 0.686 | 0.707 |
| >=700 | 0.636 | 0.653 |

Nessun trend discendente: la pendenza resta piatta (0.61-0.71) indipendentemente da quanto sia
grande il vantaggio, esattamente come nel "resto della partita" del punto 2b. La sottoconfidenza
uniforme (~30-40%) presente ovunque e' il normale effetto di saturazione del training con
target sigmoide — innocuo perche' costante e quindi prevedibile. **Il problema di calibrazione
che peggiora progressivamente (punto 2b) e' specifico ed esclusivo dei finali con poco
materiale**, non un difetto generale della rete. Apertura e mediogioco non necessitano di
alcun intervento.

## 3. Le due soluzioni (non ancora implementate su richiesta)

### Fix 1 — Controllo materiale insufficiente (codice, non training)
Aggiungere un controllo prima della chiamata all'evaluator (in `SearchWorker.cpp`, accanto ai
controlli esistenti per 50 mosse/ripetizione) che rileva "Re contro Re + al massimo un pezzo
minore, nessun pedone" e ritorna direttamente 0 (patta), bypassando NNUE e HCE. Piccolo,
sicuro, nessun retraining necessario, elimina completamente questo blind spot.

### Fix 2 — Aumento/ribilanciamento dei dati per finali sparsi e decisivi (training)
Per il problema di calibrazione (2b), che riguarda posizioni REALMENTE decisive (non patte),
serve insegnare alla rete la magnitudine corretta:
1. Generare posizioni sintetiche di finale sparso (K+Torre/Donna/pezzo-minore vs K, finali di
   soli pedoni) con etichette ESATTE da tablebase Syzygy (WDL/DTZ), non dall'euristica di
   Stockfish — ground truth perfetta proprio dove serve di piu'.
2. In alternativa/aggiunta, aumentare il peso delle posizioni sparse gia' esistenti in
   `dataset/balance_dataset.py`: lo sfoltimento Lorentziano attuale assottiglia solo le posizioni vicine
   alla patta, non ha alcun termine che compensi la sotto-rappresentazione delle posizioni con
   pochi pezzi.
3. Retraining completo sul dataset aumentato/ribilanciato, poi ripetere questa stessa analisi
   per confermare che la pendenza torni stabile anche nei finali sparsi a vantaggio >=700cp.

Progetto di piu' ore (richiede installare le tablebase Syzygy, scrivere un generatore di
posizioni, integrarlo nella pipeline esistente, poi un retraining completo) — non iniziato,
in attesa di indicazioni.
