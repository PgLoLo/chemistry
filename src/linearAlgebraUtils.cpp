#include "linearAlgebraUtils.h"

matrix makeRandomMatrix(size_t rows, size_t cols)
{
    matrix matr(rows, cols);
    matr.setRandom();
    return matr;
};

vect makeRandomVect(size_t n)
{
    vect v(n);
    v.setRandom();
    return v;
};

vect makeRandomVect(vect const& lowerBound, vect const& upperBound)
{
    return lowerBound + (0.5 * (1 + makeRandomVect(lowerBound.rows()).array()) * (upperBound - lowerBound).array()).matrix();
}

vect makeConstantVect(size_t n, double constant)
{
    vect v(n);
    v.setConstant(constant);
    return v;
}

vect eye(size_t n, size_t i)
{
    vect result(n);
    result.setZero();
    result(i) = 1.;

    return result;
};

matrix makeConstantMatrix(size_t rows, size_t cols, double constant)
{
    matrix m(rows, cols);
    m.setConstant(constant);
    return m;
}

matrix identity(size_t nDims)
{
    return identity(nDims, nDims);
};

matrix identity(size_t rows, size_t cols)
{
    matrix result(rows, cols);
    result.setIdentity();
    return result;
};


matrix isqrt(matrix m)
{
    for (int i = 0; i < m.rows(); i++)
        m(i, i) = 1. / sqrt(abs(m(i, i)));
    return m;
};

matrix linearization(matrix m)
{
    Eigen::JacobiSVD<matrix> d(m, Eigen::ComputeFullU | Eigen::ComputeFullV);
    LOG_INFO("singular values: {}", d.singularValues().transpose());

    return d.matrixU();
}

matrix linearizationNormalization(matrix m)
{
    Eigen::JacobiSVD<matrix> d(m, Eigen::ComputeFullU | Eigen::ComputeFullV);
    return d.matrixU() * isqrt(matrix(d.singularValues().asDiagonal()));
};

matrix rotationMatrix(vect u, vect v, double alpha)
{
    return identity((size_t) u.rows()) + sin(alpha) * (u * v.transpose() - v * u.transpose()) +
           (cos(alpha) - 1) * (u * u.transpose() + v * v.transpose());
}

matrix rotationMatrix(vect from, vect to)
{
    vect v = to / to.norm();
    vect u = (from - v * from.dot(v));
    u /= u.norm();

    double alpha = acos(from.dot(to) / from.norm() / to.norm());

    auto M = rotationMatrix(v, u, alpha);
    LOG_ERROR("vectors [alpha = {}]\nv = {}\nu = {}\nto = {}\nM * from = {}\nM' * from = {}\n{}",
              alpha, v.transpose(), u.transpose(), to.transpose(), (M * from).transpose(), (rotationMatrix(v, u, -alpha) * from).transpose(), M);
    return M;
}

vect randomVectOnSphere(size_t nDims, double r)
{
    vect v(nDims);
    normal_distribution<double> distribution(.0, 1.);
    for (size_t i = 0; i < nDims; i++)
        v(i) = distribution(randomGen);
    return v / v.norm() * r;
}

vect projection(vect wich, vect to)
{
    to /= to.norm();
    return wich.dot(to) * to;
}

matrix singularValues(matrix m)
{
    return Eigen::JacobiSVD<matrix>(m).singularValues().transpose();
}