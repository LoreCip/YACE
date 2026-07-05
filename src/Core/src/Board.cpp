    #include <cstdint>
    #include <sys/types.h>
    #include <string>
    #include <sstream>

    #include "Board.hpp"
    #include "BitOperations.hpp"
    #include "LookupTables.hpp"
    #include "Move.hpp"
    #include "Types.hpp"
    #include "MoveGenerator.hpp"


    /* PUBLIC  */

    void Board::InitializeBoard(){
        
        for (int c = 0; c < NUM_COLORS; c++) {
            for (int p = 0; p < NUM_PIECES; p++) {
                sides[c][p] = 0ULL;
            }
        }
        zobristHash = 0;
        historyPly = 0;
        halfMoveClock = 0;
        enPassantSquare = 64;
        castlingRights = 15; 
        
        sideToMove = Color::WHITE; 
        
        for (int color = 0; color < NUM_COLORS; color++){
            int startPos = color == Color::WHITE ? 8 : 48;
            for (int nPawn = 0; nPawn < 8; nPawn++){
                AddPiece(color, PieceType::PAWN, startPos);
                startPos++;
            }

            startPos = color == Color::WHITE ? 0 : 56;
            AddPiece(color, PieceType::ROOK, startPos);
            AddPiece(color, PieceType::ROOK, startPos + 7);
            AddPiece(color, PieceType::KNIGHT, startPos + 1);
            AddPiece(color, PieceType::KNIGHT, startPos + 6);
            AddPiece(color, PieceType::BISHOP, startPos + 2);
            AddPiece(color, PieceType::BISHOP, startPos + 5);
            AddPiece(color, PieceType::QUEEN, startPos + 3);
            AddPiece(color, PieceType::KING, startPos + 4);
        }
        
        for (int color = 0; color < NUM_COLORS; color++){
            colorOccupation[color] = 0;
            for (const auto piece : Pieces::All) {
                colorOccupation[color] |= GetBitBoard(color, piece);
            }
        }
        totalOccupation = colorOccupation[0] | colorOccupation[1];
        freeCells = ~totalOccupation;

        zobristHash ^= LookupTables::castlingKeys[castlingRights];
        
    }

    void Board::InitializeFromFEN(const std::string& fen) {
        // 1. Board reset, to be safe
        for (int c = 0; c < 2; c++) {
            for (int p = 0; p < NUM_PIECES; p++) {
                sides[c][p] = 0ULL;
            }
            colorOccupation[c] = 0ULL;
        }
        for(int i = 0; i < 64; i++) {
            board[i].piece = PieceType::NONE;
        }
        
        historyPly = 0;
        halfMoveClock = 0;
        enPassantSquare = 64;
        castlingRights = 0;
        
        // Hash reset
        zobristHash = 0;

        std::stringstream ss(fen);
        std::string pieces, activeColor, castling, enPassant, halfMove, fullMove;
        ss >> pieces >> activeColor >> castling >> enPassant >> halfMove >> fullMove;

        // 2. Load pieces
        int rank = 7;
        int file = 0;
        for (char c : pieces) {
            if (c == '/') { rank--; file = 0; }
            else if (std::isdigit(c)) { file += (c - '0'); }
            else {
                Color color = std::isupper(c) ? Color::WHITE : Color::BLACK;
                PieceType p = PieceType::NONE;
                char l = std::tolower(c);
                if (l == 'p') p = PieceType::PAWN;
                else if (l == 'n') p = PieceType::KNIGHT;
                else if (l == 'b') p = PieceType::BISHOP;
                else if (l == 'r') p = PieceType::ROOK;
                else if (l == 'q') p = PieceType::QUEEN;
                else if (l == 'k') p = PieceType::KING;

                if (p != PieceType::NONE) AddPiece(color, p, rank * 8 + file);
                file++;
            }
        }

        // 3. Update Zobrist Key
        sideToMove = (activeColor == "w") ? Color::WHITE : Color::BLACK;
        if (sideToMove == Color::BLACK) {
            zobristHash ^= LookupTables::sideKey;
        }

        // Castling
        if (castling != "-") {
            for (char c : castling) {
                if (c == 'K') castlingRights |= CastlingRights::WK;
                if (c == 'Q') castlingRights |= CastlingRights::WQ;
                if (c == 'k') castlingRights |= CastlingRights::BK;
                if (c == 'q') castlingRights |= CastlingRights::BQ;
            }
        }
        zobristHash ^= LookupTables::castlingKeys[castlingRights];

        // En Passant
        if (enPassant != "-") {
            int epFile = enPassant[0] - 'a';
            int epRank = enPassant[1] - '1';
            enPassantSquare = epRank * 8 + epFile;
            zobristHash ^= LookupTables::enPassantKeys[enPassantSquare];
        }

        if (!halfMove.empty()) halfMoveClock = std::stoi(halfMove);

        totalOccupation = colorOccupation[0] | colorOccupation[1];
        freeCells = ~totalOccupation;
        
    }

    bool Board::MakeMove(Move move) {
        int from = getMoveFrom(move);
        int to = getMoveTo(move);
        int flags = getMoveFlags(move);

        Color us = sideToMove;
        Color them = !us;
        PieceType pieceType = GetPieceOnSquare(from);

        if (pieceType == PieceType::NONE) return false;

        // --- 1. Zobrist Hash: Turn, En Passant & Castling ---
        zobristHash ^= LookupTables::sideKey;
        zobristHash ^= LookupTables::castlingKeys[castlingRights];
        if (enPassantSquare != 64) zobristHash ^= LookupTables::enPassantKeys[enPassantSquare];

        // --- 2. Captures & En Passant ---
        bool captured = false;
        PieceType capturedPieceType = PieceType::NONE;
        int captureSquare = to; 

        if (isMoveEnPassant(flags)) {
            captured = true;
            capturedPieceType = PieceType::PAWN;
            captureSquare = (us == Color::WHITE) ? to - 8 : to + 8;
            RemovePiece(them, capturedPieceType, captureSquare);
        } else if (isMoveCapture(flags)) {
            captured = true;
            for (const auto piece : Pieces::All) {
                if (getBit(GetBitBoard(them, piece), to)) {
                    capturedPieceType = piece;
                    break;
                }
            }
            
            // BUG FIX 2: Previene la rimozione di un pezzo inesistente se il flag è errato
            if (capturedPieceType != PieceType::NONE) {
                RemovePiece(them, capturedPieceType, to);
            } else {
                // Ripristina l'hash prima di ritornare (poiché l'abbiamo modificato al punto 1)
                zobristHash ^= LookupTables::sideKey;
                zobristHash ^= LookupTables::castlingKeys[castlingRights];
                if (enPassantSquare != 64) zobristHash ^= LookupTables::enPassantKeys[enPassantSquare];
                return false; 
            }
        }
        
        // --- 3. History ---
        positionHistory[historyPly] = GetHash();
        history[historyPly].movedPiece = pieceType;
        history[historyPly].capturedPiece = capturedPieceType;
        history[historyPly].enPassantSquare = enPassantSquare;
        history[historyPly].castlingRights = castlingRights;
        history[historyPly].halfMoveClock = halfMoveClock;
        historyPly++;

        // --- 4. Update State ---
        halfMoveClock = (pieceType == PieceType::PAWN || captured) ? 0 : halfMoveClock + 1;
        enPassantSquare = (pieceType == PieceType::PAWN && std::abs(from - to) == 16) ? (from + to) / 2 : 64;
        castlingRights &= (LookupTables::castlingMasks[from] & LookupTables::castlingMasks[to]);

        // --- 5. Zobrist Hash ---
        zobristHash ^= LookupTables::castlingKeys[castlingRights];
        if (enPassantSquare != 64) zobristHash ^= LookupTables::enPassantKeys[enPassantSquare];

        // --- 6. Move Pieces ---
        PieceType promoPiece = getMovePromotionPiece(flags);
        if (promoPiece != PieceType::NONE) {
            RemovePiece(us, pieceType, from);
            AddPiece(us, promoPiece, to);    
        } else {
            MovePiece(us, pieceType, from, to); // Mossa normale
        }

        bool isCastle = (pieceType == PieceType::KING && std::abs(from - to) == 2);
        int intermediateSquare = isCastle ? (from + to) / 2 : -1;

        if (isCastle) {
            if (to == 6)  MovePiece(us, PieceType::ROOK, 7, 5);
            else if (to == 2)  MovePiece(us, PieceType::ROOK, 0, 3);
            else if (to == 62) MovePiece(us, PieceType::ROOK, 63, 61);
            else if (to == 58) MovePiece(us, PieceType::ROOK, 56, 59);
        }

        totalOccupation = colorOccupation[0] | colorOccupation[1];
        freeCells = ~totalOccupation;
        sideToMove = them;

        if (isCastle && (IsSquareAttacked(from, them) || IsSquareAttacked(intermediateSquare, them))) {
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
        
        uint64_t pawnTriggers = LookupTables::pawnAttacks[ColorInt(us)][square];
        if (pawnTriggers & GetBitBoard(attackingColor, PieceType::PAWN)) return true;

        uint64_t knightTriggers = LookupTables::knightAttacks[square];
        if (knightTriggers & GetBitBoard(attackingColor, PieceType::KNIGHT)) return true;

        uint64_t kingTriggers = LookupTables::kingAttacks[square];
        if (kingTriggers & GetBitBoard(attackingColor, PieceType::KING)) return true;

        uint64_t bishopTriggers = LookupTables::GetBishopAttacks(square, totalOccupation);
        uint64_t diagonalThreats = GetBitBoard(attackingColor, PieceType::BISHOP) | GetBitBoard(attackingColor, PieceType::QUEEN);
        if (bishopTriggers & diagonalThreats) return true;

        uint64_t rookTriggers = LookupTables::GetRookAttacks(square, totalOccupation);
        uint64_t orthogonalThreats = GetBitBoard(attackingColor, PieceType::ROOK) | GetBitBoard(attackingColor, PieceType::QUEEN);
        if (rookTriggers & orthogonalThreats) return true;

        return false;
    }

    void Board::UnmakeMove(Move move) {
        int from = getMoveFrom(move);
        int to = getMoveTo(move);
        int flags = getMoveFlags(move);

        // 1. Restore turn
        sideToMove = !sideToMove;
        Color us = sideToMove;
        Color them = !us;

        // 2. Restore data from history
        historyPly--;
        UndoState lastState = history[historyPly];
        
        // 3. Update Zobrist Key (Remove current state)
        zobristHash ^= LookupTables::sideKey;
        zobristHash ^= LookupTables::castlingKeys[castlingRights];
        if (enPassantSquare != 64) zobristHash ^= LookupTables::enPassantKeys[enPassantSquare];

        // 4. Restore Variables
        enPassantSquare = lastState.enPassantSquare;
        castlingRights = lastState.castlingRights;
        halfMoveClock = lastState.halfMoveClock;

        // 5. Update Zobrist Key (Add restored state)
        zobristHash ^= LookupTables::castlingKeys[castlingRights];
        if (enPassantSquare != 64) zobristHash ^= LookupTables::enPassantKeys[enPassantSquare];

        // --- 6. Restore Pieces ---
        PieceType promoPiece = getMovePromotionPiece(flags);
        if (promoPiece != PieceType::NONE) {
            RemovePiece(us, promoPiece, to);
            AddPiece(us, PieceType::PAWN, to);
            MovePiece(us, PieceType::PAWN, to, from);
        } else {
            MovePiece(us, lastState.movedPiece, to, from);
        }

        if (lastState.capturedPiece != PieceType::NONE) {
            int captureSquare = (flags == FlagMap::ENPASS) ? ((us == Color::WHITE) ? to - 8 : to + 8) : to;
            AddPiece(them, lastState.capturedPiece, captureSquare);
        }

        // 7. Castling
        if (lastState.movedPiece == PieceType::KING && std::abs(from - to) == 2) {
            if (to == 6)  MovePiece(us, PieceType::ROOK, 5, 7);
            else if (to == 2)  MovePiece(us, PieceType::ROOK, 3, 0);
            else if (to == 62) MovePiece(us, PieceType::ROOK, 61, 63);
            else if (to == 58) MovePiece(us, PieceType::ROOK, 59, 56);
        }

        totalOccupation = colorOccupation[0] | colorOccupation[1];
        freeCells = ~totalOccupation;

        
    }

    void Board::MakeNullMove() {
        positionHistory[historyPly] = GetHash();
        
        history[historyPly].movedPiece = PieceType::NONE;
        history[historyPly].capturedPiece = PieceType::NONE;
        history[historyPly].enPassantSquare = enPassantSquare;
        history[historyPly].castlingRights = castlingRights;
        history[historyPly].halfMoveClock = halfMoveClock;
        historyPly++;

        zobristHash ^= LookupTables::sideKey;
        if (enPassantSquare != 64) zobristHash ^= LookupTables::enPassantKeys[enPassantSquare];
        
        enPassantSquare = 64;
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
        uint64_t currentHash = GetHash();
        if (historyPly < 4) return false; 

        for (int i = historyPly - 4; i >= historyPly - halfMoveClock; i -= 2){
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
                
                int foundColor = -1;
                PieceType foundPiece = PieceType::NONE;

                for (int color = 0; color < 2; color++) {
                    for (const auto piece : Pieces::All) {
                        if (getBit(GetBitBoard(color, piece), square)) {
                            foundColor = color;
                            foundPiece = piece;
                            break;
                        }
                    }
                    if (foundPiece != PieceType::NONE) break;
                }

                if (foundPiece != PieceType::NONE) {
                    if (emptySquares > 0) {
                        fen << emptySquares;
                        emptySquares = 0;
                    }
                    
                    char pieceChar = ' ';
                    if (foundPiece == PieceType::PAWN)   pieceChar = 'p';
                    else if (foundPiece == PieceType::KNIGHT) pieceChar = 'n';
                    else if (foundPiece == PieceType::BISHOP) pieceChar = 'b';
                    else if (foundPiece == PieceType::ROOK)   pieceChar = 'r';
                    else if (foundPiece == PieceType::QUEEN)   pieceChar = 'q';
                    else if (foundPiece == PieceType::KING)    pieceChar = 'k';

                    if (foundColor == Color::WHITE) {
                        pieceChar = std::toupper(pieceChar);
                    }
                    fen << pieceChar;
                } else {
                    emptySquares++;
                }
            }

            if (emptySquares > 0) {
                fen << emptySquares;
            }

            if (rank > 0) {
                fen << "/";
            }
        }

        fen << (sideToMove == Color::WHITE ? " w " : " b ");

        if (castlingRights == 0) {
            fen << "-";
        } else {
            if (castlingRights & CastlingRights::WK) fen << "K";
            if (castlingRights & CastlingRights::WQ) fen << "Q";
            if (castlingRights & CastlingRights::BK) fen << "k";
            if (castlingRights & CastlingRights::BQ) fen << "q";
        }

        fen << " ";
        if (enPassantSquare == 64) {
            fen << "-";
        } else {
            Color us = sideToMove;
            uint64_t myPawns = GetBitBoard(us, PieceType::PAWN);
            
            Color opponentColor = !us;
            uint64_t attackers = LookupTables::pawnAttacks[ColorInt(opponentColor)][enPassantSquare] & myPawns;

            if (attackers != 0ULL) {
                char epFile = 'a' + (enPassantSquare % 8);
                char epRank = '1' + (enPassantSquare / 8);
                fen << epFile << epRank;
            } else {
                fen << "-";
            }
        }

        // 5. Halfmove clock
        fen << " " << (int)halfMoveClock;
        fen << " " << (historyPly / 2 + 1);

        return fen.str();
    }