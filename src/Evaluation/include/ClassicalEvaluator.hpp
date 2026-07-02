#ifndef CLASSICAL_EVALUATOR_HPP
#define CLASSICAL_EVALUATOR_HPP

#include "IEvaluator.hpp"

class ClassicalEvaluator : public IEvaluator {
public:
    int Evaluate(const Board& board) override;
    void Reset(const Board& board) override {}
    
    void OnMakeMove(const Board& board, Move move) override {}
    void OnUnmakeMove() override {}
};

#endif