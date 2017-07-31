#pragma once

#include "helper.h"

#include "FunctionProducer.h"
#include "linearAlgebraUtils.h"

template<typename FuncT>
class AffineTransformation : public FunctionProducer
{
public:
    AffineTransformation(FuncT func, vect delta, matrix const& basis) : FunctionProducer((size_t) basis.cols()),
                                                                        mFunc(move(func)), mDelta(move(delta)),
                                                                        mBasis(basis), mBasisT(basis.transpose())
    {}

    virtual double operator()(vect const& x) override
    {
        assert((size_t) x.rows() == nDims);

        return mFunc(transform(x));
    }

    virtual vect grad(vect const& x) override
    {
        assert((size_t) x.rows() == nDims);

        return mBasisT * mFunc.grad(transform(x));
    }

    virtual matrix hess(vect const& x) override
    {
        assert((size_t) x.rows() == nDims);

        return mBasisT * mFunc.hess(transform(x)) * mBasis;
    }

    vect transform(vect const& x) const
    {
        assert((size_t) x.rows() == nDims);

        return mBasis * x + mDelta;
    }

    vect fullTransform(vect const& x) const
    {
        assert((size_t) x.rows() == nDims);

        return mFunc.fullTransform(transform(x));
    }

    vect backTransform(vect const& x) const
    {
        assert((size_t) x.rows() == nDims);

        return mBasis.inverse() * (x - mDelta);
    }

    FuncT const& getInnerFunction() const
    {
        return mFunc;
    }

private:
    FuncT mFunc;

    vect mDelta;
    matrix mBasis;
    matrix mBasisT;
};

template<typename FuncT>
auto makeAffineTransfomation(FuncT&& func, vect delta)
{
    return AffineTransformation<decay_t<FuncT>>(forward<FuncT>(func), move(delta), identity(func.nDims));
}

template<typename FuncT>
auto makeAffineTransfomation(FuncT&& func, vect delta, matrix const& A)
{
    return AffineTransformation<decay_t<FuncT>>(forward<FuncT>(func), move(delta), A);
}

template<typename FuncT>
auto makeAffineTransfomation(FuncT&& func, matrix const& A)
{
    return AffineTransformation<decay_t<FuncT>>(forward<FuncT>(func), makeConstantVect(A.rows(), 0), A);
}

template<typename FuncT>
auto prepareForPolar(FuncT&& func, vect const& v)
{
    auto A = linearizationNormalization(func.hess(v));
    LOG_INFO("{}", singularValues(func.hess(v)));
    return makeAffineTransfomation(forward<FuncT>(func), v, A);
}
