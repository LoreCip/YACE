# Pesi NNUE

## Attuale

- **`nnue_weights_v3-6.bin`** — modello di riferimento attuale (Valutazione V3-6). Prodotto da
  `NNUE/training/run_v6/checkpoints/halfkp_epoch_18_final.pth` via `convertToWeights.py`.
  Risultati: Sign Accuracy 83.60% (held-out), 2196 Elo in gauntlet vs Stockfish. Dettagli
  completi in `gauntlet/README.md` (sezioni V3-3.2 e V3-6).

## Archivio (`archive/`)

Pesi superati da versioni successive o prodotti da esperimenti non riusciti — conservati per
tracciabilità, non più usati da alcun codice. Ogni cartella corrisponde a una Valutazione della
`gauntlet/README.md`; il checkpoint `.pth` grezzo di training (quando presente) è incluso insieme
al `.bin` quantizzato già esportato.

| Cartella | File | Origine (training) | Esito / motivo archiviazione |
| :--- | :--- | :--- | :--- |
| `v1/` | `nnue_weights_v1.bin`, `.pth` | Serie V1 (architettura `256x2->32->1`, double) | Architettura superata, nessun retraining conservato oltre il primo. |
| `v2/` | `nnue_weights_v2.bin`, `.pth`, `.png` | Serie V2 (V2-2/V2-3, `256x2->32->32->1`) | Superato da V3. Il `.png` è il grafico della loss di training. |
| `v3-3/` | `nnue_weights_v3.bin`, `nnue_weigths_v3.pth`, `.png` | V3-3 (prima del fix di scala) | Superato da V3-3.1 (fix bug di scala nell'output NNUE, stessi pesi — vedi `gauntlet/README.md`). |
| `v3-4/` | `nnue_weights_v3-4.bin` | `NNUE/training/run_v4/` (V3-4: fix scheduler LR, split validazione, loss combinata) | Superato da V3-6: dati di training ancora con label invertiti (bug risolto solo in V3-3.2/V3-6), nessun guadagno di Elo (1324). |
| `v3-4/` | `nnue_weights_v3-4.2.bin` | Stesso checkpoint di V3-4, ri-esportato con il fix di quantizzazione L0/L1Bias (vedi V3-3.2) | Test isolato del solo fix di export (senza il fix dei label) — superato da V3-6, che unisce entrambi i fix. |

Non è presente un archivio per **V3-5** (esperimento di ribilanciamento del dataset): il
retraining con parametri Lorentziani più aggressivi ha dato un risultato nettamente peggiore
(Sign Accuracy 59.97% vs 64%, vedi `gauntlet/README.md`) e i pesi risultanti non sono mai stati
salvati.

I checkpoint completi di ogni epoca (non solo quello finale esportato) restano nelle rispettive
cartelle `NNUE/training/run_v4/checkpoints/` e `run_v6/checkpoints/` — non duplicati qui per
motivi di spazio (centinaia di MB per epoca).
