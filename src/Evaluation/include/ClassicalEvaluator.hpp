#ifndef CLASSICAL_EVALUATOR_HPP
#define CLASSICAL_EVALUATOR_HPP

#include "IEvaluator.hpp"

class ClassicalEvaluator : public IEvaluator {
public:
    int Evaluate(const Board& board) override;
    void Reset(const Board&) override {}
    void OnMakeMove(const Board&, Move) override {}
    void OnUnmakeMove() override {}
};

#endif