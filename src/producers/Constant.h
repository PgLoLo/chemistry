#pragma once

#include "helper.h"

#include "FunctionProducer.h"

class Constant : public FunctionProducer
{
public:
    Constant(size_t nDims, double value);

    double operator()(vect const& x);
    vect grad(vect const& x);
    matrix hess(vect const& x);
    tuple<double, vect> valueGrad(vect const& x);
    tuple<double, vect, matrix> valueGradHess(vect const& x);

private:
    double mValue;
};
