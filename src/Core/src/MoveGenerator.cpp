#include "MoveGenerator.hpp"
#include "LookupTables.hpp"
#include "BitOperations.hpp"
#include "Board.hpp"
#include "Types.hpp"

namespace MoveGen {

namespace {

    // Una mossa da `from` verso ogni casa di `targets`: CAPTURE se la casa e' nemica.
    inline void PushMoves(MoveList& moveList, int from, uint64_t targets, uint64_t enemies) {
        while (targets != 0ULL) {
            int to = __builtin_ctzll(targets);
            int flag = (setBit(0ULL, to) & enemies) ? FlagMap::CAPTURE : FlagMap::MOVE;
            moveList.push(createMove(from, to, flag));
            targets = clearBit(targets, to);
        }
    }

    inline uint64_t PieceAttacks(PieceType piece, int square, uint64_t occupancy) {
        switch (piece) {
            case PieceType::KNIGHT: return AttackTables::knightAttacks[square];
            case PieceType::KING:   return AttackTables::kingAttacks[square];
            case PieceType::BISHOP: return MagicBitboards::GetBishopAttacks(square, occupancy);
            case PieceType::ROOK:   return MagicBitboards::GetRookAttacks(square, occupancy);
            case PieceType::QUEEN:  return MagicBitboards::GetRookAttacks(square, occupancy) |
                                           MagicBitboards::GetBishopAttacks(square, occupancy);
            default:                return 0ULL;
        }
    }

    // Mosse di cavallo/re/alfiere/torre/donna verso le case di `targetMask` (arrocco escluso).
    inline void GeneratePieceMoves(const Board& board, MoveList& moveList, Color us, PieceType piece, uint64_t targetMask) {
        uint64_t enemies = board.GetColorOccupation(!us);
        uint64_t occupancy = board.GetTotalOccupation();
        uint64_t pieces = board.GetBitBoard(us, piece);
        while (pieces != 0ULL) {
            int from = __builtin_ctzll(pieces);
            PushMoves(moveList, from, PieceAttacks(piece, from, occupancy) & targetMask, enemies);
            pieces = clearBit(pieces, from);
        }
    }

    void GenerateCastling(const Board& board, MoveList& moveList, Color us) {
        uint64_t king = board.GetBitBoard(us, PieceType::KING);
        if (king == 0ULL) return;
        int from = __builtin_ctzll(king);
        if (board.IsSquareAttacked(from, !us)) return;

        uint8_t castlingRights = board.GetCastlingRights();
        uint64_t occupancy = board.GetTotalOccupation();

        // Le case tra re e torre devono essere libere, quelle attraversate dal re non attaccate
        auto tryCastle = [&](uint8_t right, uint64_t emptyMask, int transit, int kingTo) {
            if ((castlingRights & right) && !(occupancy & emptyMask) &&
                !board.IsSquareAttacked(transit, !us) && !board.IsSquareAttacked(kingTo, !us)) {
                moveList.push(createMove(from, kingTo, FlagMap::CASTLING));
            }
        };
        if (us == Color::WHITE) {
            tryCastle(CastlingRights::WK, AttackTables::maskWK, 5, 6);
            tryCastle(CastlingRights::WQ, AttackTables::maskWQ, 3, 2);
        } else {
            tryCastle(CastlingRights::BK, AttackTables::maskBK, 61, 62);
            tryCastle(CastlingRights::BQ, AttackTables::maskBQ, 59, 58);
        }
    }

    void GeneratePawnMoves(const Board& board, MoveList& moveList, Color us) {
        uint64_t myPawns = board.GetBitBoard(us, PieceType::PAWN);
        uint64_t emptySquares = board.GetFreeCells();
        uint64_t enemies = board.GetColorOccupation(!us);
        int epSquare = board.GetEnPassantSquare();

        int direction      = (us == Color::WHITE) ? 8 : -8;
        int startRankFirst = (us == Color::WHITE) ? 8 : 48;
        int promoRankFirst = (us == Color::WHITE) ? 56 : 0;
        auto onStartRank = [&](int sq) { return sq >= startRankFirst && sq < startRankFirst + 8; };
        auto onPromoRank = [&](int sq) { return sq >= promoRankFirst && sq < promoRankFirst + 8; };

        auto pushPromotions = [&](int from, int to, bool isCapture) {
            moveList.push(createMove(from, to, isCapture ? FlagMap::PRCAPQUEEN : FlagMap::PRQUEEN));
            moveList.push(createMove(from, to, isCapture ? FlagMap::PRCAPROOK : FlagMap::PRROOK));
            moveList.push(createMove(from, to, isCapture ? FlagMap::PRCAPBISHOP : FlagMap::PRBISHOP));
            moveList.push(createMove(from, to, isCapture ? FlagMap::PRCAPKNIGHT : FlagMap::PRKNIGHT));
        };

        while (myPawns != 0ULL) {
            int from = __builtin_ctzll(myPawns);

            // Spinta singola e doppia
            int toSingle = from + direction;
            if (setBit(0ULL, toSingle) & emptySquares) {
                if (onPromoRank(toSingle)) {
                    pushPromotions(from, toSingle, false);
                } else {
                    moveList.push(createMove(from, toSingle, FlagMap::MOVE));
                    int toDouble = toSingle + direction;
                    if (onStartRank(from) && (setBit(0ULL, toDouble) & emptySquares)) {
                        moveList.push(createMove(from, toDouble, FlagMap::DMOVE));
                    }
                }
            }

            // Catture
            uint64_t attacks = AttackTables::pawnAttacks[ColorInt(us)][from];
            uint64_t targets = attacks & enemies;
            while (targets != 0ULL) {
                int to = __builtin_ctzll(targets);
                if (onPromoRank(to)) pushPromotions(from, to, true);
                else moveList.push(createMove(from, to, FlagMap::CAPTURE));
                targets = clearBit(targets, to);
            }

            // En passant
            if (epSquare != NO_SQUARE && (attacks & setBit(0ULL, epSquare))) {
                moveList.push(createMove(from, epSquare, FlagMap::ENPASS));
            }
            myPawns = clearBit(myPawns, from);
        }
    }

    // Quiescenza: catture, en passant e solo promozioni a donna. Tenuta separata da
    // GeneratePawnMoves: la versione unificata con un flag misurava ~1% in meno di bench HCE (build PGO).
    void GeneratePawnCaptures(const Board& board, MoveList& moveList, Color us) {
        uint64_t myPawns = board.GetBitBoard(us, PieceType::PAWN);
        uint64_t emptySquares = board.GetFreeCells();
        uint64_t enemies = board.GetColorOccupation(!us);
        int epSquare = board.GetEnPassantSquare();

        int direction      = (us == Color::WHITE) ? 8 : -8;
        int promoRankFirst = (us == Color::WHITE) ? 56 : 0;

        while (myPawns != 0ULL) {
            int from = __builtin_ctzll(myPawns);

            int toSingle = from + direction;
            if ((setBit(0ULL, toSingle) & emptySquares) && toSingle >= promoRankFirst && toSingle < promoRankFirst + 8) {
                moveList.push(createMove(from, toSingle, FlagMap::PRQUEEN));
            }

            uint64_t attacks = AttackTables::pawnAttacks[ColorInt(us)][from];
            uint64_t targets = attacks & enemies;
            while (targets != 0ULL) {
                int to = __builtin_ctzll(targets);
                bool promotes = to >= promoRankFirst && to < promoRankFirst + 8;
                moveList.push(createMove(from, to, promotes ? FlagMap::PRCAPQUEEN : FlagMap::CAPTURE));
                targets = clearBit(targets, to);
            }

            if (epSquare != NO_SQUARE && (attacks & setBit(0ULL, epSquare))) {
                moveList.push(createMove(from, epSquare, FlagMap::ENPASS));
            }
            myPawns = clearBit(myPawns, from);
        }
    }

} // namespace

    int GenerateAllMoves(const Board& board, MoveList& moveList) {
        Color us = board.GetSideToMove();
        uint64_t notAllies = ~board.GetColorOccupation(us);

        GeneratePieceMoves(board, moveList, us, PieceType::KNIGHT, notAllies);
        GeneratePieceMoves(board, moveList, us, PieceType::KING, notAllies);
        GenerateCastling(board, moveList, us);
        GeneratePieceMoves(board, moveList, us, PieceType::BISHOP, notAllies);
        GeneratePieceMoves(board, moveList, us, PieceType::ROOK, notAllies);
        GeneratePieceMoves(board, moveList, us, PieceType::QUEEN, notAllies);
        GeneratePawnMoves(board, moveList, us);
        return moveList.count;
    }

    int GenerateCaptureMoves(const Board& board, MoveList& moveList) {
        Color us = board.GetSideToMove();
        uint64_t enemies = board.GetColorOccupation(!us);

        GeneratePieceMoves(board, moveList, us, PieceType::KNIGHT, enemies);
        GeneratePieceMoves(board, moveList, us, PieceType::KING, enemies);
        GeneratePieceMoves(board, moveList, us, PieceType::BISHOP, enemies);
        GeneratePieceMoves(board, moveList, us, PieceType::ROOK, enemies);
        GeneratePieceMoves(board, moveList, us, PieceType::QUEEN, enemies);
        GeneratePawnCaptures(board, moveList, us);
        return moveList.count;
    }

} // namespace MoveGen
