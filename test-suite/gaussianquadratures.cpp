/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2005, 2016 Klaus Spanderen

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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/experimental/math/gaussiannoncentralchisquaredpolynomial.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/integrals/gaussianquadratures.hpp>
#include <ql/math/integrals/gausslaguerrecosinepolynomial.hpp>
#include <ql/math/integrals/momentbasedgaussianpolynomial.hpp>
#include <ql/math/matrix.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/types.hpp>
#include <boost/math/distributions/non_central_chi_squared.hpp>

#ifndef TEST_BOOST_MULTIPRECISION_GAUSSIAN_QUADRATURE
// #define TEST_BOOST_MULTIPRECISION_GAUSSIAN_QUADRATURE
#endif

#ifdef TEST_BOOST_MULTIPRECISION_GAUSSIAN_QUADRATURE
#    if BOOST_VERSION < 105300
#        error This boost version is too old to support boost multi precision
#    endif

#    include <boost/multiprecision/cpp_dec_float.hpp>
#endif

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(GaussianQuadraturesTests)

template <class T>
void testSingle(const T& I, const std::string& tag, const boost::function<Real(Real)>& f, Real expected)
{
    Real calculated = I(f);
    if (std::fabs(calculated - expected) > 1.0e-4)
    {
        BOOST_ERROR("integrating" << tag << "\n"
                                  << "    calculated: " << calculated << "\n"
                                  << "    expected:   " << expected);
    }
}

// test functions

Real inv_exp(Real x)
{
    return std::exp(-x);
}

Real x_inv_exp(Real x)
{
    return x * std::exp(-x);
}

Real x_normaldistribution(Real x)
{
    return x * NormalDistribution()(x);
}

Real x_x_normaldistribution(Real x)
{
    return x * x * NormalDistribution()(x);
}

Real inv_cosh(Real x)
{
    return 1 / std::cosh(x);
}

Real x_inv_cosh(Real x)
{
    return x / std::cosh(x);
}

Real x_x_nonCentralChiSquared(Real x)
{
    return x * x * boost::math::pdf(boost::math::non_central_chi_squared_distribution<Real>(4.0, 1.0), x);
}

Real x_sin_exp_nonCentralChiSquared(Real x)
{
    return x * std::sin(0.1 * x) * std::exp(0.3 * x) *
           boost::math::pdf(boost::math::non_central_chi_squared_distribution<Real>(1.0, 1.0), x);
}

template <class T>
void testSingleJacobi(const T& I)
{
    testSingle(I, "f(x) = 1", [](Real x) -> Real { return 1.0; }, 2.0);
    testSingle(I, "f(x) = x", [](Real x) -> Real { return x; }, 0.0);
    testSingle(I, "f(x) = x^2", [](Real x) -> Real { return x * x; }, 2 / 3.);
    testSingle(I, "f(x) = sin(x)", [](Real x) -> Real { return std::sin(x); }, 0.0);
    testSingle(I, "f(x) = cos(x)", [](Real x) -> Real { return std::cos(x); }, std::sin(1.0) - std::sin(-1.0));
    testSingle(I, "f(x) = Gaussian(x)", NormalDistribution(),
               CumulativeNormalDistribution()(1.0) - CumulativeNormalDistribution()(-1.0));
}

template <class T>
void testSingleLaguerre(const T& I)
{
    testSingle(I, "f(x) = exp(-x)", inv_exp, 1.0);
    testSingle(I, "f(x) = x*exp(-x)", x_inv_exp, 1.0);
    testSingle(I, "f(x) = Gaussian(x)", NormalDistribution(), 0.5);
}

void testSingleTabulated(const boost::function<Real(Real)>& f, const std::string& tag, Real expected, Real tolerance)
{
    const Size order[] = {6, 7, 12, 20};
    TabulatedGaussLegendre quad;
    for (unsigned long i : order)
    {
        quad.order(i);
        Real realised = quad(f);
        if (std::fabs(realised - expected) > tolerance)
        {
            BOOST_ERROR(" integrating " << tag << "\n"
                                        << "    order " << i << "\n"
                                        << "    realised: " << realised << "\n"
                                        << "    expected: " << expected);
        }
    }
}

template <class mp_float>
class MomentBasedGaussLaguerrePolynomial : public MomentBasedGaussianPolynomial<mp_float>
{
  public:
    mp_float moment(Size i) const override
    {
        if (i == 0)
            return mp_float(1.0);
        else
            return mp_float(i) * moment(i - 1);
    }

    Real w(Real x) const override { return std::exp(-x); }
};


BOOST_AUTO_TEST_CASE(testJacobi)
{
    BOOST_TEST_MESSAGE("Testing Gauss-Jacobi integration...");

    testSingleJacobi(GaussLegendreIntegration(16));
    testSingleJacobi(GaussChebyshevIntegration(130));
    testSingleJacobi(GaussChebyshev2ndIntegration(130));
    testSingleJacobi(GaussGegenbauerIntegration(50, 0.55));
}

BOOST_AUTO_TEST_CASE(testLaguerre)
{
    BOOST_TEST_MESSAGE("Testing Gauss-Laguerre integration...");

    testSingleLaguerre(GaussLaguerreIntegration(16));
    testSingleLaguerre(GaussLaguerreIntegration(150, 0.01));

    testSingle(GaussLaguerreIntegration(16, 1.0), "f(x) = x*exp(-x)", x_inv_exp, 1.0);
    testSingle(GaussLaguerreIntegration(32, 0.9), "f(x) = x*exp(-x)", x_inv_exp, 1.0);
}

BOOST_AUTO_TEST_CASE(testHermite)
{
    BOOST_TEST_MESSAGE("Testing Gauss-Hermite integration...");

    testSingle(GaussHermiteIntegration(16), "f(x) = Gaussian(x)", NormalDistribution(), 1.0);
    testSingle(GaussHermiteIntegration(16, 0.5), "f(x) = x*Gaussian(x)", x_normaldistribution, 0.0);
    testSingle(GaussHermiteIntegration(64, 0.9), "f(x) = x*x*Gaussian(x)", x_x_normaldistribution, 1.0);
}

BOOST_AUTO_TEST_CASE(testHyperbolic)
{
    BOOST_TEST_MESSAGE("Testing Gauss hyperbolic integration...");

    testSingle(GaussHyperbolicIntegration(16), "f(x) = 1/cosh(x)", inv_cosh, M_PI);
    testSingle(GaussHyperbolicIntegration(16), "f(x) = x/cosh(x)", x_inv_cosh, 0.0);
}

BOOST_AUTO_TEST_CASE(testTabulated)
{
    BOOST_TEST_MESSAGE("Testing tabulated Gauss-Laguerre integration...");

    testSingleTabulated([](Real x) -> Real { return x; }, "f(x) = x", 0.0, 1.0e-13);
    testSingleTabulated([](Real x) -> Real { return x * x; }, "f(x) = x^2", (2.0 / 3.0), 1.0e-13);
    testSingleTabulated([](Real x) -> Real { return x * x * x; }, "f(x) = x^3", 0.0, 1.0e-13);
    testSingleTabulated([](Real x) -> Real { return x * x * x * x; }, "f(x) = x^4", (2.0 / 5.0), 1.0e-13);
}

BOOST_AUTO_TEST_CASE(testMomentBasedGaussianPolynomial)
{
    BOOST_TEST_MESSAGE("Testing moment-based Gaussian polynomials...");

    GaussLaguerrePolynomial g;

    std::vector<ext::shared_ptr<GaussianOrthogonalPolynomial>> ml;
    ml.push_back(ext::make_shared<MomentBasedGaussLaguerrePolynomial<Real>>());

#ifdef TEST_BOOST_MULTIPRECISION_GAUSSIAN_QUADRATURE
    ml.push_back(ext::make_shared<MomentBasedGaussLaguerrePolynomial<
                     boost::multiprecision::number<boost::multiprecision::cpp_dec_float<20>>>>());
#endif

    const Real tol = 1e-12;
    for (auto& k : ml)
    {

        for (Size i = 0; i < 10; ++i)
        {
            const Real diffAlpha = std::fabs(k->alpha(i) - g.alpha(i));
            const Real diffBeta = std::fabs(k->beta(i) - g.beta(i));

            if (diffAlpha > tol)
            {
                BOOST_ERROR("failed to reproduce alpha for Laguerre quadrature" << "\n    calculated: " << k->alpha(i)
                                                                                << "\n    expected  : " << g.alpha(i)
                                                                                << "\n    diff      : " << diffAlpha);
            }
            if (i > 0 && diffBeta > tol)
            {
                BOOST_ERROR("failed to reproduce beta for Laguerre quadrature" << "\n    calculated: " << k->beta(i)
                                                                               << "\n    expected  : " << g.beta(i)
                                                                               << "\n    diff      : " << diffBeta);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testGaussLaguerreCosinePolynomial)
{
    BOOST_TEST_MESSAGE("Testing Gauss-Laguerre-Cosine quadrature...");

    const GaussianQuadrature quadCosine(16, GaussLaguerreCosinePolynomial<Real>(0.2));

    testSingle(quadCosine, "f(x) = exp(-x)", inv_exp, 1.0);
    testSingle(quadCosine, "f(x) = x*exp(-x)", x_inv_exp, 1.0);

    const GaussianQuadrature quadSine(16, GaussLaguerreSinePolynomial<Real>(0.2));

    testSingle(quadSine, "f(x) = exp(-x)", inv_exp, 1.0);
    testSingle(quadSine, "f(x) = x*exp(-x)", x_inv_exp, 1.0);
}

BOOST_AUTO_TEST_CASE(testNonCentralChiSquared)
{
    BOOST_TEST_MESSAGE("Testing Gauss non-central chi-squared integration...");

    testSingle(GaussianQuadrature(2, GaussNonCentralChiSquaredPolynomial(4.0, 1.0)),
               "f(x) = x^2 * nonCentralChiSquared(4, 1)(x)", x_x_nonCentralChiSquared, 37.0);

    testSingle(GaussianQuadrature(14, GaussNonCentralChiSquaredPolynomial(1.0, 1.0)),
               "f(x) = x * sin(0.1*x)*exp(0.3*x)*nonCentralChiSquared(1, 1)(x)", x_sin_exp_nonCentralChiSquared,
               17.408092);
}

BOOST_AUTO_TEST_CASE(testNonCentralChiSquaredSumOfNodes)
{
    BOOST_TEST_MESSAGE("Testing Gauss non-central chi-squared sum of nodes...");

    // Walter Gautschi, How and How not to check Gaussian Quadrature Formulae
    // https://www.cs.purdue.edu/homes/wxg/selected_works/section_08/084.pdf

    // Expected results have been calculated with a multi precision library
    // following the description of test #4 in the paper above.
    // Using QuantLib's own determinant function will not work here
    // as it supports only double precision.

    const Real expected[] = {47.53491786730293,   70.6103295419633383, 98.0593406849441607,
                             129.853401537905341, 165.96963582663912,  206.389183233992043};

    const Real nu = 4.0;
    const Real lambda = 1.0;
    const GaussNonCentralChiSquaredPolynomial orthPoly(nu, lambda);

    const Real tol = 1e-5;

    for (Size n = 4; n < 10; ++n)
    {
        const Array x = GaussianQuadrature(n, orthPoly).x();
        const Real calculated = std::accumulate(x.begin(), x.end(), Real(0.0));


        if (std::fabs(calculated - expected[n - 4]) > tol)
        {
            BOOST_ERROR("failed to reproduce rule of sum" << "\n    calculated: " << calculated
                                                          << "\n    expected:   " << expected[n - 4]
                                                          << "\n    diff    :   " << calculated - expected[n - 4]);
        }
    }
}


BOOST_AUTO_TEST_CASE(testMultiDimensionalGaussIntegration)
{
    BOOST_TEST_MESSAGE("Testing multi-dimensional Gaussian quadrature...");

    const auto normal = [](const Array& x) -> Real { return std::exp(-DotProduct(x, x)); };

    for (Size n = 1; n < 5; ++n)
    {
        std::vector<Size> ns(n);
        std::iota(ns.begin(), ns.end(), Size(1));

        MultiDimGaussianIntegration quad(ns, [](const Size n) { return ext::make_shared<GaussHermiteIntegration>(n); });

        constexpr double tol = 1e4 * QL_EPSILON;

        const Real calculated = quad(normal);
        const Real expected = std::sqrt(std::pow(M_PI, Real(n)));
        const Real diff = std::abs(expected - calculated);
        if (diff > tol)
        {
            BOOST_ERROR("failed to reproduce multi dimensional Gaussian quadrature"
                        << std::setprecision(12) << "\n    calculated: " << calculated
                        << "\n    expected:   " << expected << "\n    diff:       " << diff);
        }
    }

    // testing some Gaussian Integrals
    // https://en.wikipedia.org/wiki/Gaussian_integral
    MersenneTwisterUniformRng rng(1234);
    const std::vector<Size> ns = {20, 28, 16, 22};
    const std::vector<Real> tols = {1e-8, 1e-6, 1e-2, 5e-2};
    for (Size n = 1; n < 5; ++n)
    {
        // create symmetric positive-definite matrix
        Matrix a(n, n);
        for (Size i = 0; i < n; ++i)
            for (Size j = 0; j < n; ++j)
                a[i][j] = (i == j) ? (i + 1) : rng.nextReal();

        const Matrix A = a * transpose(a);
        const Matrix invA = inverse(A);
        const Real det_2piA = std::sqrt(determinant(M_TWOPI * invA));

        const MultiDimGaussianIntegration quad(std::vector<Size>(ns.begin(), ns.begin() + n), [](const Size n)
                                               { return ext::make_shared<GaussHermiteIntegration>(n); });

        const Real calculated = quad([&A](const Array& x) -> Real { return std::exp(-0.5 * DotProduct(x, A * x)); });

        const Real expected = det_2piA;
        const Real diff = std::abs(calculated - expected);
        if (diff > tols[n - 1])
        {
            BOOST_ERROR("failed to reproduce multi dimensional Gaussian quadrature"
                        << "\n    dimensions: " << n << std::setprecision(12) << "\n    calculated: " << calculated
                        << "\n    expected:   " << expected << "\n    diff:       " << diff
                        << "\n    tolerance:  " << tols[n - 1]);
        }
    }


    Matrix a(3, 3);
    for (Size i = 0; i < 3; ++i)
        for (Size j = 0; j < 3; ++j)
            a[i][j] = (i == j) ? (i + 1) : rng.nextReal();

    const Matrix A = a * transpose(a);
    const Matrix invA = inverse(A);
    const Real sqrt_det_2piA = std::sqrt(determinant(M_TWOPI * invA));

    const MultiDimGaussianIntegration quadHigh(std::vector<Size>({22, 18, 26}), [](const Size n)
                                               { return ext::make_shared<GaussHermiteIntegration>(n); });
    const MultiDimGaussianIntegration quad2(std::vector<Size>(3, 2),
                                            [](const Size n) { return ext::make_shared<GaussHermiteIntegration>(n); });

    for (Size i = 0; i < 3; ++i)
        for (Size j = 0; j < 3; ++j)
        {
            const Real expected = sqrt_det_2piA * invA[i][j];

            Real calculated = quadHigh([&A, i, j](const Array& x) -> Real
                                       { return x[i] * x[j] * std::exp(-0.5 * DotProduct(x, A * x)); });

            Real diff = std::abs(calculated - expected);
            Real tol = 1e-4;
            if (diff > tol)
            {
                BOOST_ERROR("failed to reproduce multi dimensional Gaussian quadrature"
                            << std::setprecision(12) << "\n    calculated: " << calculated << "\n    expected:   "
                            << expected << "\n    diff:       " << diff << "\n    tolerance:  " << tol);
            }

            Matrix inva = inverse(transpose(a));
            calculated = quad2(
                [&inva, i, j](const Array& x) -> Real
                {
                    const Array f = M_SQRT2 * inva * x;
                    return f[i] * f[j] * std::exp(-DotProduct(x, x));
                });

            calculated *= determinant(M_SQRT2 * inva);
            diff = std::abs(calculated - expected);
            tol = QL_EPSILON * 1e4;
            if (diff > tol)
            {
                BOOST_ERROR("failed to reproduce multi dimensional Gaussian quadrature"
                            << std::setprecision(12) << "\n    calculated: " << calculated << "\n    expected:   "
                            << expected << "\n    diff:       " << diff << "\n    tolerance:  " << tol);
            }
        }
}


BOOST_AUTO_TEST_CASE(testJacobiRecurrenceNearRemovableSingularities)
{
    BOOST_TEST_MESSAGE("Testing the Gauss-Jacobi recurrence near its removable singularities...");

    // The expected values are a_0 = (beta - alpha) / (alpha + beta + 2) and
    // b_1 = 4 (1 + alpha) (1 + beta) / ((2 + alpha + beta)^2 (3 + alpha + beta)), at 40 digits.
    const Real tolerance = 1e-10; // percentage

    // With alpha + beta at -1, the general form of b_1 is 0 / 0, and it used to throw.
    const GaussJacobiPolynomial p1(-0.9999999998999999, -1.0e-10);
    BOOST_CHECK_CLOSE(p1.beta(1), 2.0000023857267904134e-10, tolerance);
    BOOST_CHECK_NO_THROW(GaussJacobiIntegration(10, -0.9999999998999999, -1.0e-10));

    // And near it, it lost its accuracy without saying so.
    const GaussJacobiPolynomial p2(-0.3, -0.6999999999999);
    BOOST_CHECK_CLOSE(p2.beta(1), 0.42000000000003498257, tolerance);

    // With alpha + beta at 0, the same goes for a_0.
    const GaussJacobiPolynomial p3(0.3, -0.30000000000000004);
    BOOST_CHECK_CLOSE(p3.alpha(0), -0.30000000000000002498, tolerance);

    const GaussJacobiPolynomial p4(0.3, -0.2999999999999);
    BOOST_CHECK_CLOSE(p4.alpha(0), -0.29999999999993500477, tolerance);

    // a_0 is also the first moment of the weight, which the quadrature should reproduce.
    const GaussJacobiIntegration integration(10, 0.3, -0.30000000000000004);
    const Real firstMoment = integration([&](Real x) { return x * p3.w(x); });
    BOOST_CHECK_CLOSE(firstMoment, p3.mu_0() * -0.30000000000000002498, 1e-8);
}

void checkOutermostWeight(GaussianQuadrature rule, const std::string& tag, Size i, Real expected)
{
    BOOST_TEST_CONTEXT(tag << " of order " << rule.order() << ", weight " << i)
    {
        BOOST_CHECK_SMALL(rule.weights()[i] / expected - 1.0, 1e-11);
    }
}

BOOST_AUTO_TEST_CASE(testWeightsAtOutermostNodes)
{
    BOOST_TEST_MESSAGE("Testing Gauss weights at the outermost nodes...");

    // From the recurrence's polynomials at 60 digits, at nodes solved at 60 digits.  These
    // are the weights divided by w(x), so they're about the spacing of the nodes there.

    // w(x) subnormal or zero, and the eigenvector's element subnormal or zero.
    checkOutermostWeight(GaussLaguerreIntegration(192), "Laguerre", 0, 29.06462660358607644319);
    checkOutermostWeight(GaussLaguerreIntegration(200), "Laguerre", 0, 29.47615809012518755906);
    checkOutermostWeight(GaussLaguerreIntegration(200), "Laguerre", 1, 22.17180934283598679925);
    checkOutermostWeight(GaussLaguerreIntegration(400), "Laguerre", 0, 37.36445005600186383036);
    checkOutermostWeight(GaussLaguerreIntegration(400), "Laguerre", 1, 28.24824698712899783148);
    checkOutermostWeight(GaussLaguerreIntegration(400), "Laguerre", 3, 21.67597013882301521147);
    checkOutermostWeight(GaussLaguerreIntegration(500), "Laguerre", 0, 40.30760586638187670701);
    checkOutermostWeight(GaussLaguerreIntegration(300, 0.5), "Laguerre, s = 0.5", 0, 33.88302940824677870193);

    // The eigenvector's element, about 1e-17 where it should be far smaller, swamps these.
    checkOutermostWeight(GaussHermiteIntegration(48), "Hermite", 0, 0.7668763684952382747464);
    checkOutermostWeight(GaussHermiteIntegration(64), "Hermite", 0, 0.7289272086904484277421);
    checkOutermostWeight(GaussHermiteIntegration(128), "Hermite", 0, 0.6461910523324689081628);
    checkOutermostWeight(GaussHermiteIntegration(368), "Hermite", 0, 0.5395610833771907419823);
    checkOutermostWeight(GaussHermiteIntegration(400), "Hermite", 0, 0.5319914447749765434198);
    checkOutermostWeight(GaussHermiteIntegration(500), "Hermite", 0, 0.5122820090549542695078);
    checkOutermostWeight(GaussHermiteIntegration(128, 0.5), "Hermite, mu = 0.5", 0, 0.6457557386166697057286);
    checkOutermostWeight(GaussHermiteIntegration(400, 0.5), "Hermite, mu = 0.5", 0, 0.5318787903434990044248);
    checkOutermostWeight(GaussHyperbolicIntegration(32), "hyperbolic", 0, 15.17568887357144891249);
    checkOutermostWeight(GaussHyperbolicIntegration(64), "hyperbolic", 0, 19.54308387526667472721);
    checkOutermostWeight(GaussHyperbolicIntegration(128), "hyperbolic", 0, 24.96276698863168448828);
    checkOutermostWeight(GaussHyperbolicIntegration(200), "hyperbolic", 0, 29.14137780384266234157);
    checkOutermostWeight(GaussHyperbolicIntegration(256), "hyperbolic", 0, 31.72426990941596923171);
    checkOutermostWeight(GaussHyperbolicIntegration(500), "hyperbolic", 0, 39.86680921400362637881);

    // Further in, where the element is about 1e-14 and the same noise moves the weight by 12 to 23 %.
    checkOutermostWeight(GaussHermiteIntegration(368), "Hermite", 117, 0.1208222198846529959514);
    checkOutermostWeight(GaussHyperbolicIntegration(300), "hyperbolic", 121, 2.919906888434212445108);
}

BOOST_AUTO_TEST_CASE(testHighOrdersOnFunctionsDecayingLikeTheWeight)
{
    BOOST_TEST_MESSAGE("Testing high-order Gauss rules on functions decaying like their weights...");

    // A single NaN weight is enough to make these NaN, whatever the function at that node.
    for (Size n : {184, 192, 200, 232, 256, 300, 368, 400, 500})
    {
        BOOST_TEST_CONTEXT("order " << n)
        {
            const GaussLaguerreIntegration laguerre(n);
            BOOST_CHECK_SMALL(laguerre([](Real x) { return std::exp(-x); }) - 1.0, 1e-12);
            BOOST_CHECK_SMALL(laguerre([](Real x) { return std::pow(x, 8) * std::exp(-x); }) / 40320.0 - 1.0, 1e-12);

            const GaussHermiteIntegration hermite(n);
            BOOST_CHECK_SMALL(hermite([](Real x) { return std::exp(-x * x); }) - std::sqrt(M_PI), 1e-12);

            const GaussHyperbolicIntegration hyperbolic(n);
            BOOST_CHECK_SMALL(hyperbolic([](Real x) { return 1 / std::cosh(x); }) - M_PI, 1e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testFunctionsDecayingSlowerThanTheWeight)
{
    BOOST_TEST_MESSAGE("Testing Gauss rules on functions decaying slower than their weights...");

    // These are what the outermost weights matter for: the function doesn't make up for a
    // wrong weight by vanishing there.  The bounds on the slowest functions are the rules' own
    // errors, which fall about as 1/n, with exact weights.
    const auto lorentzian = [](Real x) { return 1 / (1 + x * x); };

    for (Size n : {32, 48, 64, 80, 96, 128, 192, 256, 368, 400, 500})
    {
        BOOST_TEST_CONTEXT("Hermite of order " << n)
        {
            const GaussHermiteIntegration rule(n);
            BOOST_CHECK_SMALL(rule([](Real x) { return std::exp(-x * x / 2); }) - std::sqrt(2 * M_PI), 1e-12);
            BOOST_CHECK_SMALL(rule(lorentzian) - M_PI, 0.3);
        }
    }

    for (Size n : {32, 48, 64, 128, 192, 256, 300, 400, 500})
    {
        BOOST_TEST_CONTEXT("hyperbolic of order " << n)
        {
            const GaussHyperbolicIntegration rule(n);
            BOOST_CHECK_SMALL(rule(lorentzian) - M_PI, 0.2);
            BOOST_CHECK_SMALL(rule([](Real x) { return 1 / (std::cosh(x) * std::cosh(x)); }) - 2.0, 0.07);
        }
    }

    for (Size n : {64, 128, 192, 200, 300, 400, 500})
    {
        BOOST_TEST_CONTEXT("Laguerre of order " << n)
        {
            const GaussLaguerreIntegration rule(n);
            BOOST_CHECK_SMALL(rule([](Real x) { return std::exp(-x / 2); }) - 2.0, 1e-12);
            BOOST_CHECK_SMALL(rule([](Real x) { return 1 / ((1 + x) * (1 + x)); }) - 1.0, 0.01);
        }
    }

    // Slow enough for the outermost nodes of these to count: leaving out those whose
    // eigenvector elements underflow gets the second wrong by 3.3e-6.
    const auto slowExponential = [](Real x) { return std::exp(-x / 100); };
    BOOST_CHECK_SMALL(GaussLaguerreIntegration(400)(slowExponential) / 100.0 - 1.0, 1e-6);
    BOOST_CHECK_SMALL(GaussLaguerreIntegration(500)(slowExponential) / 100.0 - 1.0, 1e-7);
}

BOOST_AUTO_TEST_CASE(testLogWeightFunctions)
{
    BOOST_TEST_MESSAGE("Testing the logarithms of the Gauss weight functions...");

    const GaussLaguerrePolynomial laguerre, laguerreWithS(0.5);
    const GaussHermitePolynomial hermite, hermiteWithMu(0.5);
    const GaussHyperbolicPolynomial hyperbolic;
    const GaussLegendrePolynomial legendre;

    // Where w(x) can still be told from zero.
    for (Real x : {0.25, 1.0, 5.0, 20.0})
    {
        BOOST_TEST_CONTEXT("x = " << x)
        {
            BOOST_CHECK_SMALL(laguerre.logW(x) - std::log(laguerre.w(x)), 1e-12);
            BOOST_CHECK_SMALL(laguerreWithS.logW(x) - std::log(laguerreWithS.w(x)), 1e-12);
            BOOST_CHECK_SMALL(hermite.logW(x) - std::log(hermite.w(x)), 1e-12);
            BOOST_CHECK_SMALL(hermite.logW(-x) - std::log(hermite.w(-x)), 1e-12);
            BOOST_CHECK_SMALL(hermiteWithMu.logW(-x) - std::log(hermiteWithMu.w(-x)), 1e-12);
            BOOST_CHECK_SMALL(hyperbolic.logW(x) - std::log(hyperbolic.w(x)), 1e-12);
            BOOST_CHECK_SMALL(hyperbolic.logW(-x) - std::log(hyperbolic.w(-x)), 1e-12);
        }
    }
    BOOST_CHECK_SMALL(legendre.logW(0.5), 1e-15);

    // w(0) is 1 for these, and a parameter of zero mustn't multiply the log of zero.
    BOOST_CHECK_EQUAL(laguerre.logW(0.0), 0.0);
    BOOST_CHECK_EQUAL(hermite.logW(0.0), 0.0);
    BOOST_CHECK_EQUAL(hyperbolic.logW(0.0), 0.0);

    // Where w(x) has underflowed.
    BOOST_CHECK_CLOSE(laguerre.logW(2000.0), -2000.0, 1e-12);
    BOOST_CHECK_CLOSE(laguerreWithS.logW(2000.0), 0.5 * std::log(2000.0) - 2000.0, 1e-12);
    BOOST_CHECK_CLOSE(hermite.logW(40.0), -1600.0, 1e-12);
    BOOST_CHECK_CLOSE(hermiteWithMu.logW(40.0), std::log(40.0) - 1600.0, 1e-12);
    BOOST_CHECK_CLOSE(hyperbolic.logW(2000.0), M_LN2 - 2000.0, 1e-12);
    BOOST_CHECK_CLOSE(hyperbolic.logW(-2000.0), M_LN2 - 2000.0, 1e-12);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
