#ifndef IEVALUATOR_HPP
#define IEVALUATOR_HPP

#include "Board.hpp"
#include "Move.hpp"

class IEvaluator {
public:
    virtual ~IEvaluator() = default;

    virtual int Evaluate(const Board& board) = 0;
    virtual void Reset(const Board& board) = 0;
    
    virtual void OnMakeMove(const Board& board, Move move) = 0;
    virtual void OnUnmakeMove() = 0;
};

#endif