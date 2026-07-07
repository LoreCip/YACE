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
| **V2-3** | 256x2 -> 32 -> 32 -> 1 | `int` | Epoca 35 |

### Statistiche di Performance (Test su 15M posizioni)
> *Dati riferiti al modello V2-2:*
> - **MAE (WDL):** 0.1251
> - **MAE (CP):** 57.12 pedoni
> - **Deviazione Standard (CP):** 78.09 CP
> - **Sign Accuracy:** 64.00%

---

## Stima ELO

### YACE_v2-2 vs Stockfish
*Condizioni del match: 100 partite. Forza Stockfish limitata a 2000 Elo.*

#### Esito Finale
| Risultato | Totale | Percentuale |
| :--- | :--- | :--- |
| **Vittorie YACE_v5** | 30 | 30.0% |
| **Vittorie Stockfish** | 53 | 53.0% |
| **Patte** | 17 | 17.0% |

#### Valutazione Elo
> - **Differenza Elo:** -81.4
> - **Elo stimato YACE_v5:** 1919 *(Basato su SF a 2000)*