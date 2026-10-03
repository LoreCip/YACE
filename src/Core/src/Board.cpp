#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "Board.hpp"
#include "BitOperations.hpp"
#include "Move.hpp"
#include "Types.hpp"

static const char* const START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Casa del pedone catturato en passant: una traversa dietro la casa di arrivo.
static int EnPassantVictimSquare(int to, Color us) {
    return (us == Color::WHITE) ? to - 8 : to + 8;
}

void Board::InitializeBoard() {
    InitializeFromFEN(START_FEN);
}

void Board::InitializeFromFEN(const std::string& fen) {
    *this = Board{};

    std::stringstream ss(fen);
    std::string pieces, activeColor, castling, enPassant, halfMove, fullMove;
    ss >> pieces >> activeColor >> castling >> enPassant >> halfMove >> fullMove;

    int rank = 7;
    int file = 0;
    for (char c : pieces) {
        if (c == '/') { rank--; file = 0; }
        else if (std::isdigit(c)) { file += (c - '0'); }
        else {
            Color color = std::isupper(c) ? Color::WHITE : Color::BLACK;
            const char* p = std::strchr(PIECE_CHARS, std::tolower(c));
            if (p) AddPiece(color, static_cast<PieceType>(p - PIECE_CHARS), rank * 8 + file);
            file++;
        }
    }

    sideToMove = (activeColor == "w") ? Color::WHITE : Color::BLACK;
    if (sideToMove == Color::BLACK) {
        zobristHash ^= Zobrist::sideKey;
    }

    castlingRights = CastlingRights::NONE;
    for (char c : castling) {
        if (c == 'K') castlingRights |= CastlingRights::WK;
        if (c == 'Q') castlingRights |= CastlingRights::WQ;
        if (c == 'k') castlingRights |= CastlingRights::BK;
        if (c == 'q') castlingRights |= CastlingRights::BQ;
    }

    if (enPassant != "-") {
        int epFile = enPassant[0] - 'a';
        int epRank = enPassant[1] - '1';
        enPassantSquare = epRank * 8 + epFile;
    }
    HashCastlingAndEnPassant();

    if (!halfMove.empty()) halfMoveClock = std::stoi(halfMove);

    UpdateOccupation();
}

bool Board::MakeMove(Move move) {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    Color us = sideToMove;
    Color them = !us;
    PieceType pieceType = GetPieceOnSquare(from);
    if (pieceType == PieceType::NONE) return false;
    uint64_t hashBefore = zobristHash;

    // --- 1. Togli turno/arrocco/ep correnti dall'hash ---
    zobristHash ^= Zobrist::sideKey;
    HashCastlingAndEnPassant();

    // --- 2. Cattura ---
    PieceType capturedPieceType = PieceType::NONE;
    if (isMoveEnPassant(flags)) {
        capturedPieceType = PieceType::PAWN;
        RemovePiece(them, PieceType::PAWN, EnPassantVictimSquare(to, us));
    } else if (isMoveCapture(flags)) {
        for (const auto piece : Pieces::All) {
            if (getBit(GetBitBoard(them, piece), to)) {
                capturedPieceType = piece;
                break;
            }
        }
        if (capturedPieceType == PieceType::NONE) {
            // Flag di cattura su una casa vuota: annulla il punto 1 e rifiuta la mossa
            zobristHash ^= Zobrist::sideKey;
            HashCastlingAndEnPassant();
            return false;
        }
        RemovePiece(them, capturedPieceType, to);
    }
    bool captured = (capturedPieceType != PieceType::NONE);
    PushHistory(pieceType, capturedPieceType, hashBefore);

    // --- 3. Nuovo stato ---
    halfMoveClock = (pieceType == PieceType::PAWN || captured) ? 0 : halfMoveClock + 1;
    enPassantSquare = (pieceType == PieceType::PAWN && std::abs(from - to) == 16) ? (from + to) / 2 : NO_SQUARE;
    castlingRights &= (AttackTables::castlingMasks[from] & AttackTables::castlingMasks[to]);
    HashCastlingAndEnPassant();

    // --- 4. Pezzi ---
    PieceType promoPiece = getMovePromotionPiece(flags);
    if (promoPiece != PieceType::NONE) {
        RemovePiece(us, pieceType, from);
        AddPiece(us, promoPiece, to);
    } else {
        MovePiece(us, pieceType, from, to);
    }

    bool isCastle = (pieceType == PieceType::KING && std::abs(from - to) == 2);
    if (isCastle) {
        int rookFrom, rookTo;
        castlingRookSquares(to, rookFrom, rookTo);
        MovePiece(us, PieceType::ROOK, rookFrom, rookTo);
    }

    UpdateOccupation();
    sideToMove = them;

    // --- 5. Legalità: il re (e, per l'arrocco, la casa di partenza e di transito) non sotto attacco ---
    if (isCastle && (IsSquareAttacked(from, them) || IsSquareAttacked((from + to) / 2, them))) {
        UnmakeMove(move);
        return false;
    }
    if (IsSquareAttacked(__builtin_ctzll(GetBitBoard(us, PieceType::KING)), them)) {
        UnmakeMove(move);
        return false;
    }
    return true;
}

bool Board::IsSquareAttacked(int square, Color attackingColor) const {
    Color us = !attackingColor;

    uint64_t pawnTriggers = AttackTables::pawnAttacks[ColorInt(us)][square];
    if (pawnTriggers & GetBitBoard(attackingColor, PieceType::PAWN)) return true;

    uint64_t knightTriggers = AttackTables::knightAttacks[square];
    if (knightTriggers & GetBitBoard(attackingColor, PieceType::KNIGHT)) return true;

    uint64_t kingTriggers = AttackTables::kingAttacks[square];
    if (kingTriggers & GetBitBoard(attackingColor, PieceType::KING)) return true;

    uint64_t bishopTriggers = MagicBitboards::GetBishopAttacks(square, totalOccupation);
    uint64_t diagonalThreats = GetBitBoard(attackingColor, PieceType::BISHOP) | GetBitBoard(attackingColor, PieceType::QUEEN);
    if (bishopTriggers & diagonalThreats) return true;

    uint64_t rookTriggers = MagicBitboards::GetRookAttacks(square, totalOccupation);
    uint64_t orthogonalThreats = GetBitBoard(attackingColor, PieceType::ROOK) | GetBitBoard(attackingColor, PieceType::QUEEN);
    if (rookTriggers & orthogonalThreats) return true;

    return false;
}

void Board::UnmakeMove(Move move) {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    sideToMove = !sideToMove;
    Color us = sideToMove;
    Color them = !us;

    // Copia, non riferimento: con un riferimento il compilatore rilegge i campi dopo ogni
    // scrittura sulle bitboard (misurato: ~5% di perft in meno).
    UndoState lastState = history[--historyPly];

    // Stato: togli quello corrente dall'hash, ripristina, rimetti quello vecchio
    zobristHash ^= Zobrist::sideKey;
    HashCastlingAndEnPassant();
    enPassantSquare = lastState.enPassantSquare;
    castlingRights = lastState.castlingRights;
    halfMoveClock = lastState.halfMoveClock;
    HashCastlingAndEnPassant();

    // Pezzi
    PieceType promoPiece = getMovePromotionPiece(flags);
    if (promoPiece != PieceType::NONE) {
        RemovePiece(us, promoPiece, to);
        AddPiece(us, PieceType::PAWN, from);
    } else {
        MovePiece(us, lastState.movedPiece, to, from);
    }

    if (lastState.capturedPiece != PieceType::NONE) {
        int captureSquare = isMoveEnPassant(flags) ? EnPassantVictimSquare(to, us) : to;
        AddPiece(them, lastState.capturedPiece, captureSquare);
    }

    if (lastState.movedPiece == PieceType::KING && std::abs(from - to) == 2) {
        int rookFrom, rookTo;
        castlingRookSquares(to, rookFrom, rookTo);
        MovePiece(us, PieceType::ROOK, rookTo, rookFrom);
    }

    UpdateOccupation();
}

void Board::MakeNullMove() {
    PushHistory(PieceType::NONE, PieceType::NONE, zobristHash);

    zobristHash ^= Zobrist::sideKey;
    if (enPassantSquare != NO_SQUARE) zobristHash ^= Zobrist::enPassantKeys[enPassantSquare];

    enPassantSquare = NO_SQUARE;
    sideToMove = !sideToMove;
    halfMoveClock++;
}

void Board::UnmakeNullMove() {
    historyPly--;
    sideToMove = !sideToMove;

    enPassantSquare = history[historyPly].enPassantSquare;
    castlingRights = history[historyPly].castlingRights;
    halfMoveClock = history[historyPly].halfMoveClock;

    zobristHash = positionHistory[historyPly];
}

bool Board::IsRepetition() const {
    // Solo le posizioni con lo stesso giocatore al tratto, entro l'ultima mossa irreversibile
    // e non prima dell'inizio della storia (la FEN puo' dare un halfmove clock > historyPly).
    uint64_t currentHash = GetHash();
    int oldest = std::max(0, historyPly - halfMoveClock);
    for (int i = historyPly - 4; i >= oldest; i -= 2) {
        if (positionHistory[i] == currentHash) {
            return true;
        }
    }
    return false;
}

std::string Board::GetFEN() const {
    std::stringstream fen;

    for (int rank = 7; rank >= 0; rank--) {
        int emptySquares = 0;
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            PieceType piece = GetPieceOnSquare(square);
            if (piece == PieceType::NONE) {
                emptySquares++;
                continue;
            }
            if (emptySquares > 0) {
                fen << emptySquares;
                emptySquares = 0;
            }
            char pieceChar = PIECE_CHARS[PieceInt(piece)];
            bool isWhite = getBit(colorOccupation[ColorInt(Color::WHITE)], square);
            fen << (isWhite ? static_cast<char>(std::toupper(pieceChar)) : pieceChar);
        }
        if (emptySquares > 0) fen << emptySquares;
        if (rank > 0) fen << "/";
    }

    fen << (sideToMove == Color::WHITE ? " w " : " b ");

    if (castlingRights == CastlingRights::NONE) {
        fen << "-";
    } else {
        if (castlingRights & CastlingRights::WK) fen << "K";
        if (castlingRights & CastlingRights::WQ) fen << "Q";
        if (castlingRights & CastlingRights::BK) fen << "k";
        if (castlingRights & CastlingRights::BQ) fen << "q";
    }

    // La casa en passant si scrive solo se un nostro pedone puo' davvero catturare
    fen << " ";
    uint64_t epAttackers = (enPassantSquare == NO_SQUARE) ? 0ULL
        : AttackTables::pawnAttacks[ColorInt(!sideToMove)][enPassantSquare] & GetBitBoard(sideToMove, PieceType::PAWN);
    if (epAttackers != 0ULL) {
        fen << static_cast<char>('a' + (enPassantSquare % 8)) << static_cast<char>('1' + (enPassantSquare / 8));
    } else {
        fen << "-";
    }

    fen << " " << (int)halfMoveClock;
    fen << " " << (historyPly / 2 + 1);

    return fen.str();
}
