#ifndef NNUE_EVALUATOR_HPP
#define NNUE_EVALUATOR_HPP

#include <string>
#include <memory>
#include <cstdint>

#include "IEvaluator.hpp"
#include "NnueNetwork.hpp"
#include "NnueConstants.hpp"
#include "Types.hpp"


// Tutte le informazioni per trasformare l'accumulatore del ply precedente in quello di questo
// ply, calcolate al momento di OnMakeMove (quando la board e' ancora nello stato corretto per
// leggere re/pezzi) e memorizzate come INDICI DI FEATURE FINALI — cosi' il "replay" (ApplyDelta)
// non ha bisogno di alcun accesso alla board, solo di questi indici e dei pesi L0.
// nAdded/nRemoved sono condivisi tra le due prospettive per costruzione (l'insieme di eventi —
// quale pezzo si muove, se c'e' una cattura, se e' arrocco — e' lo stesso; cambia solo l'indice
// di feature risultante). Una mossa di re non produce mai eventi per la prospettiva propria
// (quella prospettiva prende sempre un refresh completo, vedi OnMakeMove) — per la prospettiva
// altrui puo' ancora produrre una rimozione (cattura) o un arrocco (torre), ma mai un evento sul
// re stesso (il re non e' una feature HalfKP).
struct AccDelta {
    uint32_t added[2][2];    // [prospettiva][slot]: mossa/promozione + torre d'arrocco
    uint32_t removed[2][3];  // [prospettiva][slot]: from + cattura + torre d'arrocco
    uint8_t nAdded = 0;
    uint8_t nRemoved = 0;
};

// alignas(64) sulla struct (non solo sul membro) arrotonda sizeof(NnueState) a un multiplo di
// 64: cosi' OGNI slot di nnueStack[MAX_HISTORY_PLY] e' 64-allineato, non solo il primo — prima
// lo stride era 1026B (non multiplo di 64), quindi solo il ply 0 era per caso ben allineato e i
// successivi progressivamente disallineati.
struct alignas(64) NnueState {
    alignas(64) int16_t accumulator[2][ACC_SIZE];
    bool computed[2];
    AccDelta delta; // trasforma accumulator del ply precedente in quello di questo ply
};

class NnueEvaluator : public IEvaluator {
private:
    NnueState nnueStack[MAX_HISTORY_PLY];
    int currentPly;
    // Conta le OnMakeMove oltre MAX_HISTORY_PLY (praticamente irraggiungibile: 512 plies e'
    // gia' 8x MAX_PLY della ricerca, vedi SearchWorker) invece di lasciar avanzare currentPly
    // oltre i limiti dell'array. OnUnmakeMove deve srotolare PRIMA questo contatore e toccare
    // currentPly solo quando torna a 0 — altrimenti make/unmake si desincronizzerebbero in modo
    // permanente per il resto della partita.
    int overflowDepth = 0;
    // Rete condivisa in sola lettura tra tutti i thread di ricerca: prima ogni NnueEvaluator
    // possedeva la propria copia da 20MB, riletta da disco a ogni `go` per ogni thread (N
    // thread = N riletture + N copie in RAM). `networkOwner` mantiene in vita l'oggetto
    // condiviso; `net` e' il puntatore usato sul percorso caldo (evita la dereferenziazione
    // dello shared_ptr — con solo un refcount atomico toccato una volta alla costruzione,
    // mai nei loop di ricerca). Sicuro perche' NnueNetwork e' immutabile dopo il caricamento
    // (EvaluateNnue e i suoi metodi privati sono tutti `const`, nessuno stato mutabile).
    std::shared_ptr<const NnueNetwork> networkOwner;
    const NnueNetwork* net = nullptr;

    int FlipSquareVertical(int square) const;
    void RefreshAccumulator(const Board& board, int perspective);
    // Calcola gli indici di feature da aggiungere/rimuovere per ENTRAMBE le prospettive in un
    // solo passaggio (sostituisce IncrementalUpdate) — chiamata da OnMakeMove quando la board
    // e' ancora nello stato corretto. Non tocca gli accumulatori.
    void BuildDelta(const Board& board, Move move, PieceType movedPiece, PieceType capturedPiece,
                     int movedColor, AccDelta& delta) const;
    // Trasforma nnueStack[ply-1].accumulator[perspective] in nnueStack[ply].accumulator[
    // perspective] usando nnueStack[ply].delta — copia e aggiornamento fusi in un solo
    // passaggio (niente memcpy separato). `ply` e' esplicito (non sempre currentPly) perche'
    // la stessa funzione serve sia per l'applicazione eager in OnMakeMove sia per il replay
    // in avanti durante la ricerca all'indietro in Evaluate() (vedi Step 8d).
    void ApplyDelta(int perspective, int ply);
    int MakeFeatureIndex(int square, PieceType pieceType, int pieceColor, int kingSq, int perspective) const;

#ifdef NNUE_VALIDATE
    // Build di test: ricalcola da zero (RefreshAccumulator) ogni accumulatore gia' "computed"
    // e verifica che coincida byte per byte con quello mantenuto incrementalmente, poi lo
    // ripristina — pura osservazione, non altera il comportamento. Troppo lento per il build
    // di produzione (un refresh completo ad ogni Evaluate()); usato solo per validare i fix
    // dell'aggiornamento incrementale (es. il bug delle mosse di re).
    void ValidateAccumulators(const Board& board);
#endif

public:
    NnueEvaluator();
    // Costruttore preferito: condivide una rete gia' caricata (vedi LoadNnueNetwork in
    // NnueNetwork.hpp) invece di caricarla di nuovo da disco per ogni istanza/thread.
    explicit NnueEvaluator(std::shared_ptr<const NnueNetwork> sharedNetwork);
    ~NnueEvaluator() override = default;

    // Shim di compatibilita': carica i pesi da disco in una rete privata di questa istanza
    // (non condivisa). Preferire il costruttore con shared_ptr quando si creano piu' istanze
    // per la stessa sessione di ricerca (thread multipli).
    bool Initialize(const std::string& weightsPath);
    
    // Inizializza lo stack per una nuova posizione/partita
    void Reset(const Board& board) override; 

    // --- Implementazione dell'interfaccia IEvaluator ---
    int Evaluate(const Board& board) override;
    void OnMakeMove(const Board& board, Move move) override;
    void OnUnmakeMove() override;
};

#endif