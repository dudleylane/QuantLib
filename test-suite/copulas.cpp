/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 dudleylane

 This file is new in the dudleylane fork of QuantLib and is licensed
 under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
 See the LICENSE file in the repository root for the full license text.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "toplevelfixture.hpp"
#include <ql/experimental/math/claytoncopularng.hpp>
#include <ql/experimental/math/farliegumbelmorgensterncopularng.hpp>
#include <ql/experimental/math/frankcopularng.hpp>
#include <ql/math/copulas/claytoncopula.hpp>
#include <ql/math/copulas/farliegumbelmorgensterncopula.hpp>
#include <ql/math/copulas/frankcopula.hpp>
#include <ql/math/copulas/gaussiancopula.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <cmath>
#include <vector>

using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CopulaTests)

namespace
{
    // Compares the samples' empirical copula with the analytic one on a grid of the unit square.
    // The grid's last row and column are the marginals, which must come out uniform.
    template <class Generator, class Copula>
    void checkSamples(const Generator& generator, const Copula& copula)
    {
        const Size samples = 100000, gridPoints = 6;

        std::vector<std::vector<Size>> below(gridPoints, std::vector<Size>(gridPoints, 0));
        Size outside = 0;
        for (Size k = 0; k < samples; ++k)
        {
            const std::vector<Real> sample = generator.next().value;
            if (!(sample[0] >= 0.0 && sample[0] <= 1.0 && sample[1] >= 0.0 && sample[1] <= 1.0))
                ++outside;
            for (Size i = 0; i < gridPoints; ++i)
            {
                for (Size j = 0; j < gridPoints; ++j)
                {
                    if (sample[0] <= (i + 1.0) / gridPoints && sample[1] <= (j + 1.0) / gridPoints)
                        ++below[i][j];
                }
            }
        }
        BOOST_CHECK_EQUAL(outside, Size(0));

        for (Size i = 0; i < gridPoints; ++i)
        {
            for (Size j = 0; j < gridPoints; ++j)
            {
                BOOST_TEST_CONTEXT("at (" << i + 1 << ", " << j + 1 << ") / " << gridPoints)
                {
                    const Real expected = copula((i + 1.0) / gridPoints, (j + 1.0) / gridPoints);
                    // Four standard errors, with a floor for a point where the copula is 0 or 1.
                    const Real variance = std::max(expected * (1.0 - expected), 1.0 / samples);
                    BOOST_CHECK_SMALL(Real(below[i][j]) / samples - expected, 4.0 * std::sqrt(variance / samples));
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testFarlieGumbelMorgensternCopulaRng)
{
    BOOST_TEST_MESSAGE("Testing the Farlie-Gumbel-Morgenstern copula's random-number generator...");

    for (Real theta : {-1.0, -0.5, 0.0, 0.5, 1.0})
    {
        BOOST_TEST_CONTEXT("theta = " << theta)
        {
            const FarlieGumbelMorgensternCopulaRng<MersenneTwisterUniformRng> generator(MersenneTwisterUniformRng(42),
                                                                                        theta);
            checkSamples(generator, FarlieGumbelMorgensternCopula(theta));
        }
    }
}

BOOST_AUTO_TEST_CASE(testClaytonCopulaRng)
{
    BOOST_TEST_MESSAGE("Testing the Clayton copula's random-number generator...");

    for (Real theta : {-1.0, -0.9, -0.5, -0.25, -0.1, 0.5, 2.0, 5.0})
    {
        BOOST_TEST_CONTEXT("theta = " << theta)
        {
            const ClaytonCopulaRng<MersenneTwisterUniformRng> generator(MersenneTwisterUniformRng(42), theta);
            checkSamples(generator, ClaytonCopula(theta));
        }
    }
}

BOOST_AUTO_TEST_CASE(testFrankCopulaRng)
{
    BOOST_TEST_MESSAGE("Testing the Frank copula's random-number generator...");

    for (Real theta : {-5.0, -1.0, 1.0, 5.0, 30.0})
    {
        BOOST_TEST_CONTEXT("theta = " << theta)
        {
            const FrankCopulaRng<MersenneTwisterUniformRng> generator(MersenneTwisterUniformRng(42), theta);
            checkSamples(generator, FrankCopula(theta));
        }
    }
}

BOOST_AUTO_TEST_CASE(testClaytonCopulaAtNegativeTheta)
{
    BOOST_TEST_MESSAGE("Testing the Clayton copula at negative theta...");

    // Below the curve x^-theta + y^-theta = 1 the copula is 0.  There, the power of a negative
    // number would give a NaN, or a positive number when the power is an even integer.
    for (Real theta : {-0.9, -0.5, -0.25, -0.1})
    {
        const ClaytonCopula copula(theta);
        for (Real x : {1e-9, 1e-6})
        {
            for (Real y : {1e-9, 1e-6})
            {
                BOOST_TEST_CONTEXT("theta = " << theta << ", x = " << x << ", y = " << y)
                {
                    BOOST_CHECK_EQUAL(copula(x, y), 0.0);
                }
            }
        }
    }

    // Above it, the formula holds, and the marginals are uniform.
    const ClaytonCopula copula(-0.5);
    BOOST_CHECK_SMALL(copula(0.5, 0.75) - std::pow(std::sqrt(0.5) + std::sqrt(0.75) - 1.0, 2.0), 1e-15);
    BOOST_CHECK_SMALL(copula(0.2, 1.0) - 0.2, 1e-15);
    BOOST_CHECK_SMALL(copula(1.0, 0.2) - 0.2, 1e-15);

    // The two limits of the range, where the power is 1 and where the base can't be negative.
    const ClaytonCopula lowerBound(-1.0), positive(2.0);
    BOOST_CHECK_EQUAL(lowerBound(0.25, 0.5), 0.0);
    BOOST_CHECK_SMALL(lowerBound(0.75, 0.5) - 0.25, 1e-15);
    BOOST_CHECK_SMALL(positive(0.5, 0.5) - 1.0 / std::sqrt(7.0), 1e-15);
    BOOST_CHECK_EQUAL(positive(0.0, 0.5), 0.0);
}

BOOST_AUTO_TEST_CASE(testGaussianCopulaOnBoundary)
{
    BOOST_TEST_MESSAGE("Testing the Gaussian copula on the boundary of the unit square...");

    // C(0, v) = C(v, 0) = 0, and C(1, v) = C(v, 1) = v. The inverse normal takes 0 and 1 to -QL_MAX_REAL and
    // QL_MAX_REAL, and is accurate to about 1e-9 in between.
    for (Real rho : {-0.99, -0.5, 0.0, 0.5, 0.95, 0.99})
    {
        const GaussianCopula copula(rho);
        for (Real v : {0.0, 0.3, 1.0})
        {
            BOOST_TEST_CONTEXT("rho = " << rho << ", v = " << v)
            {
                BOOST_CHECK_SMALL(copula(0.0, v), 1e-15);
                BOOST_CHECK_SMALL(copula(v, 0.0), 1e-15);
                BOOST_CHECK_SMALL(copula(1.0, v) - v, 1e-9);
                BOOST_CHECK_SMALL(copula(v, 1.0) - v, 1e-9);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
