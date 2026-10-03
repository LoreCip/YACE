# YACE — NNUE (dataset, training, analisi)

Pipeline Python che produce i pesi della rete HalfKP usata dal motore
(`src/Evaluation/.../NnueNetwork.cpp`, `NnueEvaluator.cpp`).

```
NNUE/
├── halfkp.py            formato del dataset, codifica HalfKP, percorsi, split di validazione
├── model.py             rete QAT (PyTorch), scale di quantizzazione, caricamento checkpoint
├── dataset/
│   ├── build_dataset.py     dump Lichess (.jsonl.zst) → data/clean_db_halfkp.bin
│   ├── balance_dataset.py   → data/clean_db_halfkp_balanced.bin (sfoltito e rimescolato)
│   ├── plot_dataset.py      statistiche e grafici di un dataset
│   └── build_stats.log      esito dell'ultima build_dataset
├── training/
│   ├── train.py             training → runs/<run>/checkpoints/halfkp_epoch_N_final.pth
│   └── export_weights.py    checkpoint → .bin quantizzato per il motore
├── analysis/            valutazione dei modelli e studio sui finali (endgame_weakness_analysis.md)
├── data/                dataset (non in git) + descrizione del dump Lichess
└── runs/                checkpoint (non in git) e log delle run di training
```

Gli script si lanciano da qualsiasi cartella, per esempio `python NNUE/training/train.py`.

## Pipeline

1. `python dataset/build_dataset.py` — filtra il dump (profondità ≥ 15, |cp| ≤ 1000, niente
   scacchi, mate, catture/promozioni come mossa migliore) e scrive una riga per posizione.
2. `python dataset/balance_dataset.py` — sfoltisce le posizioni vicine alla patta e quelle
   affollate, poi rimescola (lo stream Lichess non è i.i.d.).
3. `python training/train.py` — imposta `run_dir` e i parametri nel `__main__`.
4. `python training/export_weights.py runs/<run>/checkpoints/<epoca>.pth ../parameters/weights/<nome>.bin`
5. Analisi: `analysis/evaluate_model.py` (campione dell'intero dataset), `heldout_by_phase.py`,
   `endgame_by_material.py`, `one_piece_positions.py` (solo split di validazione).

## Formato e codifica (`halfkp.py`)

- Riga del dataset (132 byte): `us[32]`, `them[32]` (uint16, indici HalfKP, padding 40960)
  e `target` (float32) = `sigmoid(cp / 400)` con `cp` dal punto di vista di **chi muove**.
- `indice = casa + (tipo*2 + colore_relativo + casa_re*10) * 64`, tipo 0..4 = P N B R Q,
  scacchiera ribaltata (`^ 56`) per la prospettiva del nero. Al massimo 30 pezzi non-re.
- Split di validazione: 2% del dataset, `default_rng(42)`; lo stesso in training e analisi.

## Rete e corrispondenza con il C++

`40960 → 256` per prospettiva → ClippedReLU → concat 512 → `32` → ClippedReLU → `32` →
ClippedReLU → `1` (logit; cp = logit × 400).

| Pesi | Tipo nel `.bin` | Scala di export |
|---|---|---|
| L0, L0Bias | int16 | 127 |
| L1 / L2 | int8 | 64 (bias int32: 127×64) |
| L3 | int16 | 127 (bias int32: 127×127) |

Verifica fatta il 2026-10-03 su 2522 posizioni da partite del gauntlet:
- codifica Python + emulazione intera del `.bin` = valutazione del motore C++ in **tutte** le posizioni;
- rete PyTorch vs motore: errore medio 2.5 cp (max 18.5): è il costo della quantizzazione,
  anche perché il training simula L0 a scala 255 e l'export usa 127.

## Miglioramenti futuri

| ID | Descrizione |
|---|---|
| F1 | Feature set HalfKAv2, HalfKAv2_hm, Full_Threats |
| F2 | Aggiornamento accumulatore lazy (flag dirty + stack) |
| F3 | Architetture parametrizzate via template/enum |
| F4 | Caricamento formato `.nnue` nativo Stockfish |
| F6 | Rilevamento runtime VNNI |
| F8 | Multiple PSQT outputs + subnetworks (SFNNv4+) |
| F9 | Feature factorization per il training (P, K, HalfRelativeKP) |
