#include "newDegreeDelition.h"

#include "helper.h"

#include "linearAlgebraUtils.h"
#include "InputOutputUtils.h"
#include "producers/producers.h"

template<typename FuncT>
void logFunctionInfo(string const& title, FuncT& func, vect const& p)
{
    auto hess = func.hess(p);
    auto grad = func.grad(p);
    auto value = func(p);

    LOG_INFO("{}\n\tposition: {}\n\tenergy: {}\n\tgradient: {} [{}]\n\thessian: {}\n\n",
             title, p.transpose(), value, grad.norm(), grad.transpose(), singularValues(hess));
}

void testDegreesDelition()
{
    ifstream input("./C2H4");
//    ifstream input("./H2O");
    auto charges = readCharges(input);
    auto equilStruct = readVect(input);

    auto U = makeConstantMatrix(charges.size() * 3, charges.size() * 3 - 3);
    for (size_t i = 0, num1 = 0; num1 < 3; num1++)
        for (size_t num2 = 1; num2 < charges.size(); num2++, i++) {
            U(num1, i) = 1. / sqrt(2);
            U(num2 * 3 + num1, i) = -1. / sqrt(2);
        }

    auto molecule = makeAffineTransfomation(GaussianProducer(charges), equilStruct);
    auto fixed = makeAffineTransfomation(molecule, U);

    logFunctionInfo("not fixed", molecule, makeConstantVect(molecule.nDims));
    logFunctionInfo("fixed", fixed, makeConstantVect(fixed.nDims));
};