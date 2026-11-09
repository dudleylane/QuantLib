/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2005 Klaus Spanderen
 Copyright (C) 2005 Gary Kennedy

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file gaussianquadratures.hpp
    \brief Integral of a 1-dimensional function using the Gauss quadratures
*/

#include <ql/math/integrals/gaussianquadratures.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/math/matrixutilities/tqreigendecomposition.hpp>
#include <ql/utilities/null.hpp>
#include <map>

namespace QuantLib
{

    namespace
    {
        // log(p_0(x)^2 + ... + p_{n-1}(x)^2), with p_k the recurrence's orthonormal polynomials and
        // p_0 = 1.  They can grow past what a double holds, so the sum is scaled down as it grows.
        Real logSumOfSquaredPolynomials(Real x, const Array& alpha, const Array& sqrtBeta)
        {
            const Real scale = 1e100;
            Real previous = 0.0, current = 1.0, sum = 1.0, logScaling = 0.0;
            for (Size k = 0; k < sqrtBeta.size(); ++k)
            {
                const Real next =
                    ((x - alpha[k]) * current - (k > 0 ? Real(sqrtBeta[k - 1] * previous) : Real(0.0))) / sqrtBeta[k];
                previous = current;
                current = next;
                sum += current * current;
                if (sum > scale * scale)
                {
                    previous /= scale;
                    current /= scale;
                    sum /= scale * scale;
                    logScaling += 2 * std::log(scale);
                }
            }
            return std::log(sum) + logScaling;
        }
    }

    GaussianQuadrature::GaussianQuadrature(Size n, const GaussianOrthogonalPolynomial& orthPoly) : x_(n), w_(n)
    {

        // set-up matrix to compute the roots and the weights
        Array e(n - 1);

        Size i;
        for (i = 1; i < n; ++i)
        {
            x_[i] = orthPoly.alpha(i);
            e[i - 1] = std::sqrt(orthPoly.beta(i));
        }
        x_[0] = orthPoly.alpha(0);
        const Array alpha = x_;

        TqrEigenDecomposition tqr(x_, e, TqrEigenDecomposition::OnlyFirstRowEigenVector,
                                  TqrEigenDecomposition::Overrelaxation);

        x_ = tqr.eigenvalues();
        const Matrix& ev = tqr.eigenvectors();

        // The eigenvectors' first row has absolute precision at best: where an element should
        // be 1e-150 it can come out at 1e-17, or underflow.  The polynomials give those weights
        // instead, since mu_0 / sum_k p_k(x)^2 is the same weight, and in logs, since w(x) can
        // underflow where the weight itself doesn't.
        const Real smallestElement = std::sqrt(QL_EPSILON);
        Real mu_0 = orthPoly.mu_0();
        for (i = 0; i < n; ++i)
        {
            if (std::fabs(ev[0][i]) > smallestElement)
                w_[i] = mu_0 * ev[0][i] * ev[0][i] / orthPoly.w(x_[i]);
            else
                w_[i] = std::exp(std::log(mu_0) - logSumOfSquaredPolynomials(x_[i], alpha, e) - orthPoly.logW(x_[i]));
        }
    }


    MultiDimGaussianIntegration::MultiDimGaussianIntegration(
        const std::vector<Size>& ns, const std::function<ext::shared_ptr<GaussianQuadrature>(Size)>& genQuad)
    : weights_(std::accumulate(ns.begin(), ns.end(), Size(1), std::multiplies<>()), 1.0),
      x_(weights_.size(), Array(ns.size()))
    {

        const Size m = ns.size();
        const Size n = x_.size();

        std::vector<Size> spacing(m);
        spacing[0] = 1;
        std::partial_sum(ns.begin(), ns.end() - 1, spacing.begin() + 1, std::multiplies<>());

        std::map<Size, Array> n2weights, n2x;
        for (auto order : ns)
        {
            if (n2x.find(order) == n2x.end())
            {
                const ext::shared_ptr<GaussianQuadrature> quad = genQuad(order);
                n2x[order] = quad->x();
                n2weights[order] = quad->weights();
            }
        }

        for (Size i = 0; i < n; ++i)
        {
            for (Size j = 0; j < m; ++j)
            {
                const Size order = ns[j];
                const Size nx = (i / spacing[j]) % ns[j];
                weights_[i] *= n2weights[order][nx];
                x_[i][j] = n2x[order][nx];
            }
        }
    }

    Real MultiDimGaussianIntegration::operator()(const std::function<Real(Array)>& f) const
    {
        Real s = 0.0;
        const Size n = x_.size();
        for (Size i = 0; i < n; ++i)
            s += weights_[i] * f(x_[i]);

        return s;
    }


    namespace detail
    {
        template <class Integration>
        GaussianQuadratureIntegrator<Integration>::GaussianQuadratureIntegrator(Size n)
        : Integrator(Null<Real>(), n), integration_(ext::make_shared<Integration>(n))
        {
        }

        template <class Integration>
        Real
        GaussianQuadratureIntegrator<Integration>::integrate(const std::function<Real(Real)>& f, Real a, Real b) const
        {

            const Real c1 = 0.5 * (b - a);
            const Real c2 = 0.5 * (a + b);

            return c1 * integration_->operator()([c1, c2, f](Real x) { return f(c1 * x + c2); });
        }

        template class GaussianQuadratureIntegrator<GaussLegendreIntegration>;
        template class GaussianQuadratureIntegrator<GaussChebyshevIntegration>;
        template class GaussianQuadratureIntegrator<GaussChebyshev2ndIntegration>;
    }

    void TabulatedGaussLegendre::order(Size order)
    {
        switch (order)
        {
            case (6):
                order_ = order;
                x_ = x6;
                w_ = w6;
                n_ = n6;
                break;
            case (7):
                order_ = order;
                x_ = x7;
                w_ = w7;
                n_ = n7;
                break;
            case (12):
                order_ = order;
                x_ = x12;
                w_ = w12;
                n_ = n12;
                break;
            case (20):
                order_ = order;
                x_ = x20;
                w_ = w20;
                n_ = n20;
                break;
            default:
                QL_FAIL("order " << order << " not supported");
        }
    }


    // Abscissas and weights: the zeros of the Legendre polynomials, and 2 / ((1 - x^2) P_n'(x)^2) at each, computed
    // at 60 digits and given to 20 significant digits, which the compiler rounds to the nearest double.

    /* order 6 */
    const Real TabulatedGaussLegendre::x6[3] = {0.23861918608319690863, 0.66120938646626451366, 0.93246951420315202781};

    const Real TabulatedGaussLegendre::w6[3] = {0.46791393457269104739, 0.36076157304813860757, 0.17132449237917034504};

    const Size TabulatedGaussLegendre::n6 = 3;

    /* order 7 */
    const Real TabulatedGaussLegendre::x7[4] = {0.0, 0.40584515137739716691, 0.74153118559939443986,
                                                0.94910791234275852453};

    const Real TabulatedGaussLegendre::w7[4] = {0.41795918367346938776, 0.38183005050511894495, 0.27970539148927666790,
                                                0.12948496616886969327};

    const Size TabulatedGaussLegendre::n7 = 4;

    /* order 12 */
    const Real TabulatedGaussLegendre::x12[6] = {0.12523340851146891547, 0.36783149899818019375,
                                                 0.58731795428661744730, 0.76990267419430468704,
                                                 0.90411725637047485668, 0.98156063424671925069};

    const Real TabulatedGaussLegendre::w12[6] = {0.24914704581340278500, 0.23349253653835480876,
                                                 0.20316742672306592175, 0.16007832854334622633,
                                                 0.10693932599531843096, 0.047175336386511827195};

    const Size TabulatedGaussLegendre::n12 = 6;

    /* order 20 */
    const Real TabulatedGaussLegendre::x20[10] = {
        0.076526521133497333755, 0.22778585114164507808, 0.37370608871541956067, 0.51086700195082709800,
        0.63605368072651502545,  0.74633190646015079261, 0.83911697182221882339, 0.91223442825132590587,
        0.96397192727791379127,  0.99312859918509492479};

    const Real TabulatedGaussLegendre::w20[10] = {
        0.15275338713072585070,  0.14917298647260374679, 0.14209610931838205133,  0.13168863844917662690,
        0.11819453196151841731,  0.10193011981724043504, 0.083276741576704748725, 0.062672048334109063570,
        0.040601429800386941331, 0.017614007139152118312};

    const Size TabulatedGaussLegendre::n20 = 10;

}
