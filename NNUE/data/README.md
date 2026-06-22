# YACE -- NNUE


wget https://database.lichess.org/lichess_db_eval.jsonl.zst


## Format

Evaluations are formatted as JSON; one position per line.
The schema of a position looks like this:

{
  "fen":          // the position FEN only contains pieces, active color, castling rights, and en passant square.
  "evals": [      // a list of evaluations, ordered by number of PVs.
      "knodes":   // number of kilo-nodes searched by the engine
      "depth":    // depth reached by the engine
      "pvs": [    // list of principal variations
        "cp":     // centipawn evaluation. Omitted if mate is certain.
        "mate":   // mate evaluation. Omitted if mate is not certain.
        "line":   // principal variation, in UCI_Chess960 format.
}

Each position can have multiple evaluations, each with a different number of PVs. 