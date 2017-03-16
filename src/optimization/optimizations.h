#pragma once

#include "GradientDescent.h"
#include "MomentumGradientDescent.h"
#include "NesterovGradientDescent.h"
#include "Adagrad.h"
#include "Adadelta.h"
#include "Adam.h"

#include "History.h"
#include "GradientLengthStopCriteria.h"

#include "FollowGradientDeltaStrategy.h"

#include "AtomicStopStrategy.h"
#include "DeltaNormStopStrategy.h"
#include "GradientNormStopStrategy.h"
#include "HistoryStrategyWrapper.h"

namespace optimization
{
    template<template<int, typename> typename OptimizerT, typename CriteriaT>
    OptimizerT<CriteriaT::N, CriteriaT> make_optimizer_with_criteria(CriteriaT const &criteria)
    {
        return OptimizerT<CriteriaT::N, CriteriaT>(criteria);
    }
}