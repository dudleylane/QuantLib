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

    for (Real theta : {0.5, 2.0, 5.0})
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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
