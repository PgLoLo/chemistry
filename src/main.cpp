#include "helper.h"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <Eigen/LU>

#include "linearAlgebraUtils.h"
#include "producers/producers.h"
#include "PythongraphicsFramework.h"

#include "optimization/optimizations.h"
#include "optimization/optimizeOnSphere.h"
#include "InputOutputUtils.h"

using namespace std;
using namespace boost;
using namespace optimization;

constexpr double EPS = 1e-7;

constexpr double calculate_delta(double min, double max, int n)
{
    return (max - min) / (n - 1);
}

constexpr double MAX_VAL = 1;
constexpr double MIN_X = -MAX_VAL;
constexpr double MAX_X = MAX_VAL;
constexpr double MIN_Y = -MAX_VAL;
constexpr double MAX_Y = MAX_VAL;

constexpr size_t N = 250;
constexpr double DX = calculate_delta(MIN_X, MAX_X, N);
constexpr double DY = calculate_delta(MIN_Y, MAX_Y, N);

constexpr size_t PHI = 1000;

vect getRandomPoint(vect const& lowerBound, vect const& upperBound)
{
    auto p = makeRandomVect(lowerBound.rows());
    return lowerBound.array() + p.array() * (upperBound.array() - lowerBound.array());
}

template<typename T>
auto optimize(T& func, vect const& x, bool deltaHistory = false)
{
    if (deltaHistory) {
        auto optimizer = makeSecondGradientDescent(HessianDeltaStrategy(),
                                                   makeHistoryStrategy(makeStandardAtomicStopStrategy(func)));
        auto path = optimizer(func, x);
        return path;
    } else {
        return makeSecondGradientDescent(makeRepeatDeltaStrategy(HessianDeltaStrategy()),
                                         makeStandardAtomicStopStrategy(func))(func, x);
    }
};

void optimizeStructure()
{
//    ifstream input("H2O");
    ifstream input("C2H4");

    vector<size_t> charges;
    vect initState;
    tie(charges, initState) = readChemcraft(input);

    initState = rotateToFix(initState);
    auto molecule = GaussianProducer(charges);
    auto prepared = fixAtomSymmetry(makeAffineTransfomation(molecule, initState));

    LOG_INFO("nDims = {}", molecule.nDims);

    auto optimized = optimize(prepared, makeConstantVect(prepared.nDims, 0), true).back();

    cout << toChemcraftCoords(molecule.getCharges(), prepared.transform(optimized)) << endl;
}

vector<vect> parseLogForStartingPoints(string const& file, size_t nDims)
{
    vector<vect> result;

    ifstream log(file);
    string s;
    while (getline(log, s))
        if (s.find("initial polar Direction") != string::npos) {
            stringstream ss(s);
            for (int i = 0; i < 5; i++)
                ss >> s;

            vect v(nDims);
            for (int i = 0; i < v.rows(); i++)
                ss >> v(i);
            result.push_back(v);

//            stringstream ss;
//
//            getline(log, s);
//            for (int i = 0; i < 6; i++) {
//                getline(log, s);
//                ss << s << endl;
//            }
//
//            vector<size_t> charges;
//            vect initState;
//            tie(charges, initState) = readMolecule(ss);
//
//            result.push_back(initState);
        }

    return result;
}


vector<vector<double>> calcPairwiseDists(vect v)
{
    assert(v.rows() % 3 == 0);

    size_t n = (size_t) v.rows() / 3;
    vector<vector<double>> dists(n);
    for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < n; j++)
            dists[i].push_back((v.block(i * 3, 0, 3, 1) - v.block(j * 3, 0, 3, 1)).norm());

    return dists;
}

double calcDist(vect v1, vect v2)
{
    auto d1 = calcPairwiseDists(v1);
    auto d2 = calcPairwiseDists(v2);

    vector<size_t> permut;
    for (size_t i = 0; i < d1.size(); i++)
        permut.push_back(i);

    double minMse = -1;

    do {
        double mse = 0;
        for (size_t i = 0; i < d1.size(); i++)
            for (size_t j = 0; j < d1.size(); j++)
                mse += sqr(d1[i][j] - d2[permut[i]][permut[j]]);

        if (minMse < 0 || mse < minMse)
            minMse = mse;
    } while (next_permutation(permut.begin(), permut.end()));

    return minMse;
}

void filterPolarDirectionsLogFile()
{
    ifstream input("C2H4");
    auto charges = readCharges(input);
    auto equilStruct = readVect(input);

    auto molecule = GaussianProducer(charges);
    auto fixedSym = fixAtomSymmetry(molecule);
    equilStruct = fixedSym.backTransform(equilStruct);

    auto linearHessian = normalizeForPolar(fixedSym, equilStruct);

    auto ps = parseLogForStartingPoints("./logs/log_2017-05-31_13-38", 12);
    cout << ps.size() << endl;

    vector<bool> used(ps.size());
    for (size_t i = 0; i < ps.size(); i++)
        for (size_t j = i + 1; j < ps.size(); j++)
            if (calcDist(linearHessian.fullTransform(ps[i]), linearHessian.fullTransform(ps[j])) < 1e-1)
                used[j] = true;

    size_t cnt = 0;
    for (size_t i = 0; i < ps.size(); i++)
        if (!used[i])
            cnt++;
    cout << cnt << endl;

    for (auto val : used)
        cout << val;
    cout << endl << endl;

    cout.precision(10);
    for (size_t i = 0; i < ps.size(); i++)
        if (!used[i]) {
            cout << ps[i].rows() << endl;
            for (int j = 0; j < ps[i].rows(); j++)
                cout << fixed << ps[i](j) << ' ';
            cout << endl;
        }
    cout << endl << endl << endl;

    cout << ps.size() << endl;
    for (auto const& p : ps)
        cout << p.transpose() << endl;
    cout << endl << endl;

    cout.precision(2);
    for (size_t i = 0; i < ps.size(); i++) {
        auto const& p1 = ps[i];
        if (used[i])
            continue;

        vector<double> xs;

        for (auto const& p2 : ps) {
//            auto dist = (p1 - p2).norm();
            auto dist = calcDist(linearHessian.fullTransform(p1), linearHessian.fullTransform(p2));

            cout << fixed << dist << ' ';
            xs.push_back(dist);
        }
        cout << endl;

        sort(xs.begin(), xs.end());
        framework.plot(framework.newPlot(), xs);
    }
}

vector<vect> fromCartesianToPositions(vect v)
{
    assert(v.rows() % 3 == 0);

    vector<vect> rs;
    for (size_t i = 0; i < (size_t) v.rows(); i += 3)
        rs.push_back(v.block(i, 0, 3, 1));
    return rs;
}

vect centerOfMass(vector<vect> const& rs)
{
    auto p = makeConstantVect(3, 0);
    for (auto r : rs)
        p += r;
    return p;
}

matrix tensorOfInertia(vector<vect> const& rs)
{
    auto J = makeConstantMatrix(3, 3, 0);
    for (auto r : rs)
        J += identity(3) * r.dot(r) + r * r.transpose();
    return J;
}

vect solveEquations(vect v)
{
    auto withZeros = makeConstantVect((size_t) v.rows() + 6, 0);
    for (size_t i = 0, j = 0; i < (size_t) withZeros.rows(); i++)
        if (i < 7 && i != 5)
            withZeros(i) = 0;
        else
            withZeros(i) = v(j++);
    LOG_INFO("\nv = {}\nw = {}", v.transpose(), withZeros.transpose());
    v = withZeros;

    auto rs = fromCartesianToPositions(v);
    auto p = centerOfMass(rs);
    auto J = tensorOfInertia(rs);

    LOG_INFO("before:\np: {}\nJ:\n{}", p.transpose(), J);

    double A = -p(0);
    double B = -p(1);
    double C = -p(2);
    double D = -J(0, 1);
    double E = -J(0, 2);
    double F = -J(1, 2);

    LOG_INFO("values: {} {} {} {} {} {}", A, B, C, D, E, F);

    {
        LOG_INFO("Linear Equasions #2: x: {}", C);
        v(2) = C;
    }

    {
        Eigen::Matrix2d m;
        m << 1, 1, v(2), v(5);
        Eigen::Vector2d b;
        b << B, F;
        Eigen::Vector2d x = m.colPivHouseholderQr().solve(b);
        LOG_INFO("Linear Equasions #2:\nm:\n{}\nb: {}\nx: {}", m, b.transpose(), x.transpose());
        v(1) = x(0);
        v(4) = x(1);
    }

    {
        Eigen::Matrix3d m;
        m << 1, 1, 1, v(1), v(4), v(7), v(2), v(5), v(8);
        Eigen::Vector3d b;
        b << A, D, E;
        Eigen::Vector3d x = m.colPivHouseholderQr().solve(b);
        LOG_INFO("Linear Equasions #3:\nm:\n{}\nb: {}\nx: {}", m, b.transpose(), x.transpose());
        v(0) = x(0);
        v(3) = x(1);
        v(6) = x(2);
    }

    rs = fromCartesianToPositions(v);
    p = centerOfMass(rs);
    J = tensorOfInertia(rs);
    LOG_INFO("after:\np: {}\nJ:\n{}", p.transpose(), J);
    LOG_INFO("result: {}", v.transpose());

    return v;
}

void analizeFolder()
{
    vector<double> energies;
    vector<vect> states;
    for (size_t i = 0; i < 200; i++) {
        vector<size_t> charges;
        vect state;
        tie(charges, state) = readChemcraft(ifstream(str(boost::format("./2/%1%.xyz") % i)));

        GaussianProducer molecule(charges);
        auto fixed = fixAtomSymmetry(molecule);
        state = fixed.backTransform(state);

        auto hess = fixed.hess(state);
        auto grad = fixed.grad(state);
        auto energy = fixed(state);

        energies.push_back(energy);
        states.push_back(state);

        LOG_INFO("State #{}: {}\n\tenergy = {}\n\tgradient = {} [{}]\n\thess values = {}\nchemcraft coords:\n{}", i,
                 state.transpose(), energy, grad.norm(), grad.transpose(), singularValues(hess),
                 toChemcraftCoords(charges, fixed.fullTransform(state)));
    }

    framework.plot(framework.newPlot(), energies);

    for (size_t i = 0; i < energies.size(); i++) {
        LOG_INFO("#{}: {}, {}", i, i == 0 || energies[i - 1] < energies[i] ? '+' : '-',
                 i + 1 == energies.size() || energies[i] < energies[i + 1] ? '+' : '-');
    }
}

void drawTrajectories()
{
    RandomProjection proj(15);
    vector<size_t> quantities = {335, 318, 218, 43};
    auto axis = framework.newPlot();
    for (size_t i = 0; i < quantities.size(); i++) {
        vector<double> xs, ys;
        for (size_t j = 0; j < quantities[i]; j++) {
            vector<size_t> charges;
            vect state;
            tie(charges, state) = readChemcraft(ifstream(str(format("./%1%/%2%.xyz") % i % j)));
            auto cur = proj(toDistanceSpace(state, false));
            xs.push_back(cur(0));
            ys.push_back(cur(1));
        }

        framework.plot(axis, xs, ys);
    }
}

template<typename FuncT>
void logFunctionInfo(string const& title, FuncT& func, vect const& p)
{
    auto hess = func.hess(p);
    auto grad = func.grad(p);
    auto value = func(p);

    LOG_INFO("{}\n\tposition: {}\n\tenergy: {}\n\tgradient: {} [{}]\n\thessian: {}\n\n",
             title, p.transpose(), value, grad.norm(), grad.transpose(), singularValues(hess));
}

template<typename FuncT>
void logFunctionPolarInfo(string const& title, FuncT& func, vect const& p, double r)
{
    auto polar = makePolarWithDirection(func, r, p);

    auto valueGradHess = polar.valueGradHess(makeConstantVect(polar.nDims, M_PI / 2));
    auto value = get<0>(valueGradHess);
    auto grad = get<1>(valueGradHess);
    auto hess = get<2>(valueGradHess);

    LOG_INFO("{}\n\tposition: {}\n\tenergy: {}\n\tgradient: {} [{}]\n\thessian: {}\n\n",
             title, p.transpose(), value, grad.norm(), grad.transpose(), singularValues(hess));
}

vector<vect> filterByDistance(vector<vect> const& vs, double r)
{
    vector<vect> result;

    for (size_t i = 0; i < vs.size(); i++) {
        bool flag = true;
        for (size_t j = 0; j < i; j++)
            if ((vs[i] - vs[j]).norm() < r)
                flag = false;
        if (flag)
            result.push_back(vs[i]);
    }
    return result;
}

template<typename FuncT>
vector<vect> filterBySingularValues(vector<vect> const& vs, FuncT& func)
{
    vector<vect> result;

    for (auto const& v : vs) {
        auto polar = makePolarWithDirection(func, 0.1, v);
        auto sValues = singularValues(polar.hess(makeConstantVect(polar.nDims, M_PI / 2)));

        bool flag = true;
        for (size_t i = 0; flag && i < sValues.size(); i++)
            if (sValues(i) < 0)
                flag = false;

        if (flag)
            result.push_back(v);
    }

    return result;
}


void analizeMinsOnSphere()
{

    ifstream input("./C2H4");
    auto charges = readCharges(input);
    auto equilStruct = readVect(input);

    auto molecule = fixAtomSymmetry(GaussianProducer(charges, 1));
    equilStruct = molecule.backTransform(equilStruct);
    auto normalized = normalizeForPolar(molecule, equilStruct);

    logFunctionInfo("normalized energy for equil structure", normalized, makeConstantVect(normalized.nDims, 0));

    ifstream mins("./mins_on_sphere");
    size_t cnt;
    mins >> cnt;
    vector<vect> vs;
    for (size_t i = 0; i < cnt; i++)
        vs.push_back(readVect(mins));

    vs = filterByDistance(vs, .0001);
    LOG_INFO("{} remained after filtering", vs.size());

    RandomProjection proj((size_t) vs.back().size());
    vector<double> xs, ys;
    for (auto const& v : vs) {
        auto projection = proj(v);
        xs.push_back(projection(0));
        ys.push_back(projection(1));
    }
    framework.scatter(framework.newPlot(), xs, ys);

    ofstream mins2("./mins_on_sphere_filtered");
    mins2.precision(30);
    mins2 << vs.size() << endl;
    for (auto const& v : vs)
        mins2 << v.size() << endl << fixed << v.transpose() << endl;

    for (auto const& v : vs) {
        auto polar = makePolarWithDirection(normalized, .1, v);
        logFunctionInfo("", polar, makeConstantVect(polar.nDims, M_PI / 2));
    }
}


void shs()
{
    ifstream input("./C2H4");
    auto charges = readCharges(input);
    auto equilStruct = readVect(input);

    auto molecule = fixAtomSymmetry(GaussianProducer(charges, 3));
    equilStruct = molecule.backTransform(equilStruct);
    auto normalized = normalizeForPolar(molecule, equilStruct);

    logFunctionInfo("normalized energy for equil structure", normalized, makeConstantVect(normalized.nDims, 0));

    ifstream minsOnSphere("./mins_on_sphere_filtered");
    size_t cnt;
    minsOnSphere >> cnt;
    vector<vect> vs;
    for (size_t i = 0; i < cnt; i++)
        vs.push_back(readVect(minsOnSphere));

    double const firstR = 0.1;
    double const deltaR = 0.01;

    auto const stopStrategy = makeHistoryStrategy(StopStrategy(5e-4, 5e-4));

#pragma omp parallel for
    for (size_t i = 0; i < cnt; i++) {
        auto direction = vs[i];
        LOG_INFO("Path #{}. Initial direction: {}", i, direction.transpose());

        system(str(boost::format("mkdir %1%") % i).c_str());
        double value = normalized(direction);

        size_t j = 0;
        for (j = 0; j < 600; j++) {
            if (!j) {
                auto polar = makePolarWithDirection(normalized, .1, direction);
                logFunctionInfo(str(boost::format("Paht %1% initial direction info") % i), polar, makeConstantVect(polar.nDims, M_PI / 2));
            }

            double r = firstR + deltaR * j;

            vect prev = direction;
            direction = direction / direction.norm() * r;
            try {
                vector<vect> path;
                if (!tryToConverge(stopStrategy, normalized, direction, r, path)) {
                    LOG_INFO("Path #{}. did not conveged", i);
                    break;
                }
                direction = path.back();

                double newValue = normalized(direction);
                LOG_INFO("New {} point in path {}:\n\tvalue = {:.13f}\n\tdelta norm = {:.13f}\n\t{}\nchemcraft coords:\n{}", j, i,
                         newValue, (direction / direction.norm() - prev / prev.norm()).norm(), direction.transpose(),
                         toChemcraftCoords(charges, normalized.fullTransform(direction)));

                ofstream output(str(boost::format("./%1%/%2%.xyz") % i % j));
                output << toChemcraftCoords(charges, normalized.fullTransform(direction)) << endl;

                if (newValue < value) {
                    LOG_ERROR("newValue < value [{:.13f} < {:.13f}]. Stopping", newValue, value);
                    //break;
                }

                value = newValue;
            } catch (GaussianException const &exc) {
                break;
            }
        }

        LOG_INFO("Path #{} finished with {} iterations", i, j);
    }
}

template<typename FuncT>
void findInitialPolarDirections(FuncT& func, double r)
{
    auto const axis = framework.newPlot();
    RandomProjection const projection(func.nDims);
    StopStrategy const stopStrategy(1e-7, 1e-7);

    ofstream output("./mins_on_sphere");
    output.precision(30);

    #pragma omp parallel for
    for (size_t i = 0; i < 2 * func.nDims; i++)
    {
        vect pos = r * eye(func.nDims, i / 2);
        if (i % 2)
            pos *= -1;

        auto path = optimizeOnSphere(stopStrategy, func, pos, r, 50);
        if (path.empty())
            continue;

        #pragma omp critical
        {
            vect p = path.back();
            output << p.size() << endl << fixed << p << endl;

            vector<double> xs, ys;
            for (auto const& p : path) {
                vect proj = projection(p);
                xs.push_back(proj(0));
                ys.push_back(proj(1));
            }

            framework.plot(axis, xs, ys);
            framework.scatter(axis, xs, ys);

            auto polar = makePolarWithDirection(func, r, path.back());
            logFunctionInfo(str(format("new direction (%1%)") % path.back().transpose()), polar, makeConstantVect(polar.nDims, M_PI / 2));
        }
    }
}

void minimaElimination()
{
    initializeLogger();

    ifstream input("./C2H4");
    auto charges = readCharges(input);
    auto equilStruct = readVect(input);

    auto molecule = fixAtomSymmetry(GaussianProducer(charges, 3));
    equilStruct = molecule.backTransform(equilStruct);
    auto normalized = normalizeForPolar(molecule, equilStruct);
    auto zeroEnergy = normalized(makeConstantVect(normalized.nDims, 0));

    double const r = .05;
    vector<double> values;
    vector<vect> directions;

    auto const axis = framework.newPlot();
    RandomProjection const projection(normalized.nDims);
    auto stopStrategy = makeHistoryStrategy(StopStrategy(1e-4 * r, 1e-4 * r));

    bool firstEpochFinished = false;
    size_t lastSuccessIter = (size_t) -1;

//    while (!firstEpochFinished) {
//        for (size_t iter = 0; iter < (normalized.nDims - 2) * 2; iter++) {
//            if (iter == lastSuccessIter) {
//                firstEpochFinished = true;
//                break;
//            }
//
////            Cosine3OnSPhereInterpolation supplement(normalized.nDims, values, directions);
////            LargestCosine3OnSphere supplement(normalized.nDims, values, directions);
//            Cosine3OnSPhereInterpolation supplement(normalized.nDims, values, directions);
//            auto func = normalized + supplement;
//
//            int sign = 2 * ((int) iter % 2) - 1;
//            vect startingDirection = r * sign * eye(normalized.nDims, iter / 2);
//            auto path = optimizeOnSphere(stopStrategy, func, startingDirection, r, 50);
//            auto direction = path.back();
//
//            logFunctionPolarInfo("func in new direction", func, direction, r);
//            logFunctionPolarInfo("normalized in new direction", normalized, direction, r);
//
//            stringstream distances;
//            double minAngle = 0;
//            for (auto const& prevDir : directions) {
//                minAngle = max(minAngle, angleCosine(direction, prevDir));
//                distances << boost::format("[%1%, %2%]") % distance(direction, prevDir) % angleCosine(direction, prevDir);
//            }
//            LOG_ERROR("Distances from previous {} directons [dist, cos(angle)]:\n{}\nmin angle = {}", directions.size(),
//                      distances.str(), minAngle);
//
//            bool needToAssert = false;
//
//            vector<vect> supplePath;
//            if (tryToConverge(stopStrategy, normalized, direction, r, supplePath, 10)) {
//                LOG_ERROR("second optimization converged for {} steps", supplePath.size());
//            } else {
//                LOG_ERROR("second optimization did not converged with hessian update. Tryin standard optimization");
//                supplePath = optimizeOnSphere(stopStrategy, normalized, direction, r, 50);
//                needToAssert = true;
//            }
//
//            path.insert(path.end(), supplePath.begin(), supplePath.end());
//            auto oldDirection = direction;
//            direction = path.back();
//            LOG_ERROR("cos(oldDirection, direction) = {} after second optimization", angleCosine(oldDirection, direction));
//
//            logFunctionPolarInfo("normalized after additional optimization", normalized, direction, r);
//
//            distances = stringstream();
//            minAngle = 0;
//            for (auto const& prevDir : directions) {
//                minAngle = max(minAngle, angleCosine(direction, prevDir));
//                distances << boost::format("[%1%, %2%]") % distance(direction, prevDir) % angleCosine(direction, prevDir);
//            }
//            LOG_ERROR("Distances from previous {} directons [dist, cos(angle)]:\n{}\nmin angle = {}", directions.size(),
//                      distances.str(), minAngle);
//
//            if (minAngle < .975) {
//                values.push_back((sqr(r) / 2 - (normalized(direction) - zeroEnergy)) / r / r / r);
//                //            values.push_back((sqr(r) / 2 - (func(direction) - zeroEnergy)) / r / r / r);
//                directions.push_back(direction);
//
//                ofstream mins("./mins_on_sphere");
//                mins.precision(21);
//
//                mins << directions.size() << endl;
//                for (auto const& dir : directions) {
//                    mins << dir.size() << endl << fixed << dir.transpose() << endl;
//                }
//
//                lastSuccessIter = iter;
//
//                assert(!needToAssert);
//            } else {
//                LOG_ERROR("min angle is too large: {}", minAngle);
//            }
//        }
//    }

    {
        ifstream mins("./data/mins_on_sphere");
        size_t cnt;
        mins >> cnt;

        for (size_t i = 0; i < cnt; i++) {
            auto direction = readVect(mins);

            logFunctionPolarInfo("", normalized, direction, r);

            directions.push_back(direction);
//            values.push_back((sqr(r) / 2 - (normalized(direction) - zeroEnergy)) / r / r / r);
            values.push_back(sqr(r) / 2 - (normalized(direction) - zeroEnergy));
        }
    }

    while (true) {
//        Cosine3OnSphereInterpolation supplement(normalized.nDims, values, directions);
        CleverCosine3OnSphereInterpolation supplement(normalized.nDims, values, directions);
        auto func = normalized + supplement;

        auto path = optimizeOnSphere(stopStrategy, func, r * randomVectOnSphere(normalized.nDims), r, 50, 5);
        auto direction = path.back();

        logFunctionPolarInfo("func in new direction", func, direction, r);
        logFunctionPolarInfo("normalized in new direction", normalized, direction, r);

        stringstream distances;
        double minAngle = 0;
        for (auto const& prevDir : directions) {
            minAngle = max(minAngle, angleCosine(direction, prevDir));
            distances << boost::format("[%1%, %2%]") % distance(direction, prevDir) % angleCosine(direction, prevDir);
        }
        LOG_ERROR("Distances from previous {} directons [dist, cos(angle)]:\n{}\nmin angle = {}", directions.size(),
                  distances.str(), minAngle);

        size_t const N = 15;
        auto directionMem = direction;

        for (size_t i = 0; i < N; i++) {
            double alpha = (double) (i + 1) / N;
            auto linearComb = alpha * normalized + (1 - alpha) * func;

            auto supplePath = optimizeOnSphere(stopStrategy, linearComb, direction, r, 50, 10);
            LOG_ERROR("experimental iteration {}: converged for {} steps", i + 1, supplePath.size());

            path.insert(path.end(), supplePath.begin(), supplePath.end());
            direction = path.back();
        }

        LOG_ERROR("Experimental convergence result:cos(angle) = {}", angleCosine(direction, directionMem));

        logFunctionPolarInfo("normalized after additional optimization", normalized, direction, r);

        distances = stringstream();
        minAngle = 0;
        for (auto const& prevDir : directions) {
            minAngle = max(minAngle, angleCosine(direction, prevDir));
            distances
               << boost::format("[%1%, %2%]") % distance(direction, prevDir) % angleCosine(direction, prevDir);
        }
        LOG_ERROR("Distances from previous {} directons [dist, cos(angle)]:\n{}\nmin angle = {}", directions.size(),
                  distances.str(), minAngle);

        if (minAngle < .975) {
//            values.push_back((sqr(r) / 2 - (normalized(direction) - zeroEnergy)) / r / r / r);
//            values.push_back((sqr(r) / 2 - (func(direction) - zeroEnergy)) / r / r / r);
            values.push_back(sqr(r) / 2 - (func(direction) - zeroEnergy));
            directions.push_back(direction);

            ofstream mins("./mins_on_sphere");
            mins.precision(21);

            mins << directions.size() << endl;
            for (auto const& dir : directions) {
                mins << dir.size() << endl << fixed << dir.transpose() << endl;
            }

        } else {
            LOG_ERROR("min angle is too large: {}", minAngle);
        }
    }
}

int main()
{
    initializeLogger();

    minimaElimination();

    ifstream input("./C2H4");
    auto charges = readCharges(input);
    auto equilStruct = readVect(input);

    auto molecule = fixAtomSymmetry(GaussianProducer(charges, 3));
    equilStruct = molecule.backTransform(equilStruct);
    auto normalized = normalizeForPolar(molecule, equilStruct);
    auto zeroEnergy = normalized(makeConstantVect(normalized.nDims, 0));

    auto const axis = framework.newPlot();
    auto stopStrategy = makeHistoryStrategy(StopStrategy(1e-7, 1e-7));

    double const r = .05;

    vector<double> values;
    vector<vect> directions;
    {
        ifstream mins("./data/mins_on_sphere");
        size_t cnt;
        mins >> cnt;

        for (size_t i = 0; i < cnt; i++) {
            auto direction = readVect(mins);

//            logFunctionPolarInfo("", normalized, direction, r);

            directions.push_back(direction);
            values.push_back(sqr(r) / 2 - (normalized(direction) - zeroEnergy));
        }
    }

    CleverCosine3OnSphereInterpolation func(normalized.nDims, values, directions);

    for (size_t i = 0; i < directions.size(); i++) {
        LOG_INFO("{} {}", zeroEnergy + sqr(r) / 2, normalized(directions[i]) + func(directions[i]));
    }

    return 0;

    while (true) {
        auto path = optimizeOnSphere(stopStrategy, normalized, randomVectOnSphere(normalized.nDims, r), r, 50, 5);
        LOG_WARN("finished, press key");
        char c;
        cin >> c;
    }
    //    for (size_t i = 0; i < 2 * func.nDims; i++)
//    {
//        vect pos = r * eye(func.nDims, i / 2);
//        if (i % 2)
//            pos *= -1;
//
//        auto path = optimizeOnSphere(stopStrategy, func, pos, r, 50);
//        if (path.empty())
//            continue;
//
//        {
//            vect p = path.back();
//            output << p.size() << endl << fixed << p << endl;
//
//            vector<double> xs, ys;
//            for (auto const& p : path) {
//                vect proj = projection(p);
//                xs.push_back(proj(0));
//                ys.push_back(proj(1));
//            }
//
//            framework.plot(axis, xs, ys);
//            framework.scatter(axis, xs, ys);
//
//            auto polar = makePolarWithDirection(func, r, path.back());
//            logFunctionInfo(str(format("new direction (%1%)") % path.back().transpose()), polar, makeConstantVect(polar.nDims, M_PI / 2));
//        }
//    }

//    StopStrategy const stopStrategy(1e-7, 1e-7);
//    #pragma omp parallel for
//    for (size_t i = 0; i < vs.size(); i++) {
//        vector<vect> path;
//        assert(tryToConverge(stopStrategy, normalized, vs[i], .1, path));
//        vs[i] = path.back();
//        LOG_INFO("{}: {} iters to converge");
//    }
//
//    vs = filterBySingularValues(vs, normalized);
//    LOG_INFO("{}", vs.size());


//    auto p = vs[8];
//
//    auto stopStrategy = makeHistoryStrategy(StopStrategy(1e-4, 1e-4));
//    double const r = .1;
//    auto const theta = makeConstantVect(normalized.nDims - 1, M_PI / 2);
//
//    vect momentum;
//    vector<vect> path;
//
//    for (size_t iter = 0; ; iter++) {
//        if (iter % 50 == 0 && tryToConverge(stopStrategy, normalized, p, r, path, iter)) {
//            LOG_ERROR("breaked here");
//            break;
//        }
//
//        auto polar = makePolarWithDirection(normalized, r, p);
//
//        auto valueGrad = polar.valueGrad(theta);
//        auto value = get<0>(valueGrad);
//        auto grad = get<1>(valueGrad);
//
//        if (iter)
//            momentum = max(0., momentum.dot(grad) / (grad.norm() * momentum.norm())) * momentum + grad;
//        else
//            momentum = grad;
//
//        auto lastP = p;
//        p = polar.getInnerFunction().transform(polar.transform(theta - momentum));
//        path.push_back(p);
//
//        stopStrategy(iter, p, value, grad, momentum);
//    }

//    shs();

//    auto startTime = chrono::system_clock::now();
//    auto result = findInitialPolarDirections(normalized, 0.1);
//    LOG_INFO("time passed: {}s", chrono::duration<double>(chrono::system_clock::now() - startTime).count());
//
//    ofstream output("./mins_on_sphere");
//    output.precision(30);
//    for (auto& v : result)
//        output << v.rows() << endl << fixed << v.transpose() << endl;
//
//    ofstream output2("./mins_on_sphere2");
//    output2.precision(30);
//    for (auto v : result) {
//        v = normalized.fullTransform(v);
//        output2 << v.rows() << endl << fixed << v.transpose() << endl;
//    }

    return 0;
}