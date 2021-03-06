#pragma once

#include <boost/algorithm/string/predicate.hpp>

#include "FunctionProducer.h"
#include "inputOutputUtils.h"

extern string const GAUSSIAN_HEADER;
extern string const SCF_METHOD;
extern string const FORCE_METHOD;
extern string const HESS_METHOD;
extern string const OPT_METHOD;

class GaussianException : public exception
{
public:
    explicit GaussianException(std::thread::id threadId)
            : mThreadId(threadId), mThreadHash(std::hash<thread::id>()(mThreadId))
    { }

private:
    std::thread::id const mThreadId;
    size_t const mThreadHash;
};

class GaussianProducer : public FunctionProducer {
public:
    static constexpr double MAGIC_CONSTANT = 1.88972585931612435672;

    explicit GaussianProducer(vector<size_t> charges, size_t nProc = 1, size_t mem = 1000);

    double operator()(vect const &x) override;
    vect grad(vect const &x) override;
    matrix hess(vect const &x) override;
    tuple<double, vect> valueGrad(vect const& x) override;
    tuple<double, vect, matrix> valueGradHess(vect const& x) override;

    vect optimize(vect const&) const;

    vector<size_t> const& getCharges() const;
    vect transform(vect from) const;
    vect fullTransform(vect from) const;
    GaussianProducer const& getFullInnerFunction() const;
    GaussianProducer& getFullInnerFunction();

    void setGaussianNProc(size_t nProc);
    void setGaussianMem(size_t mem);

private:
    size_t mNProc;
    size_t mMem;

    vector<size_t> mCharges;

    ifstream runGaussian(vect const& x, string const& method) const;
    string createInputFile(vect const &x, string const& method) const;
    double parseValue(ifstream& input) const;
    vect parseGrad(ifstream& input) const;
    matrix parseHess(ifstream& input) const;
    vect parseStructure(ifstream& input) const;
};

