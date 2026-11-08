/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2024 Ralf Konrad Eckel

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
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/randomnumbers/xoshiro256starstaruniformrng.hpp>
#include <ql/math/randomnumbers/zigguratgaussianrng.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ZigguratGaussianTests)

namespace
{
    // Hands out the numbers it's given, so that a test can send a draw down a chosen path.
    struct ScriptedRng
    {
        std::vector<std::uint64_t> integers;
        std::vector<Real> reals;
        mutable Size integersUsed = 0, realsUsed = 0;

        std::uint64_t nextInt64() const { return integers.at(integersUsed++); }
        Real nextReal() const { return reals.at(realsUsed++); }
    };

    // The integer from which nextReal() takes the given layer and ordinate.
    std::uint64_t drawFor(int layer, Real u)
    {
        const auto bucket = static_cast<std::uint64_t>(std::llround((u + 1.0) / 2.0 * std::ldexp(1.0, 53) - 0.5));
        return (bucket << 11) | static_cast<std::uint64_t>(layer);
    }
}

BOOST_AUTO_TEST_CASE(testStatisticsOfNextReal)
{
    BOOST_TEST_MESSAGE("Testing ZigguratGaussianRng<Xoshiro256StarStarUniformRng>::nextReal() for "
                       "mean, variance, skewness and kurtosis...");
    auto seed = 42UL;
    auto uniformRandom = Xoshiro256StarStarUniformRng(seed);
    auto random = ZigguratGaussianRng<Xoshiro256StarStarUniformRng>(uniformRandom);

    auto randoms = IncrementalStatistics();

    auto iterations = 10'000'000;
    for (auto j = 0; j < iterations; ++j)
    {
        Real next = random.next().value;
        randoms.add(next);
    }

    auto mean = randoms.mean();
    auto variance = randoms.variance();
    auto skewness = randoms.skewness();
    auto kurtosis = randoms.kurtosis();

    if (std::abs(mean) > 0.001)
    {
        BOOST_ERROR("Mean " << mean << " for seed " << seed << " is not close to 0.");
    }
    if (std::abs(1.0 - variance) > 0.005)
    {
        BOOST_ERROR("Variance " << variance << " for seed " << seed << " is not close to 1.");
    }
    if (std::abs(skewness) > 0.001)
    {
        BOOST_ERROR("Skewness " << skewness << " for seed " << seed << " is not close to 0.");
    }
    if (std::abs(kurtosis) > 0.03)
    {
        BOOST_ERROR("Kurtosis " << kurtosis << " for seed " << seed << " is not close to 0.");
    }
}

BOOST_AUTO_TEST_CASE(testTailsOfNextReal)
{
    BOOST_TEST_MESSAGE("Testing the tails of ZigguratGaussianRng<Xoshiro256StarStarUniformRng>::nextReal()...");

    auto uniformRandom = Xoshiro256StarStarUniformRng(42UL);
    auto random = ZigguratGaussianRng<Xoshiro256StarStarUniformRng>(uniformRandom);

    const std::vector<Real> levels = {1.5, 2.0, 2.5, 3.0, 3.25, 3.5, 4.0};
    std::vector<Size> beyond(levels.size(), 0);

    const Size iterations = 5'000'000;
    for (Size j = 0; j < iterations; ++j)
    {
        const Real x = std::fabs(random.next().value);
        for (Size k = 0; k < levels.size(); ++k)
        {
            if (x > levels[k])
                ++beyond[k];
        }
    }

    const CumulativeNormalDistribution N;
    for (Size k = 0; k < levels.size(); ++k)
    {
        BOOST_TEST_CONTEXT("level " << levels[k])
        {
            const Real p = 2.0 * N(-levels[k]);
            const Real expected = p * iterations;
            BOOST_CHECK_SMALL(beyond[k] - expected, 4.0 * std::sqrt(expected * (1.0 - p)));
        }
    }
}

BOOST_AUTO_TEST_CASE(testWedgeAcceptance)
{
    BOOST_TEST_MESSAGE("Testing the wedge acceptance of ZigguratGaussianRng...");

    // Layer 1 runs from x = 3.654152885361008796, with a density of 0.001260285930498598 relative
    // to its peak, to x = 3.449278298560964462, with 0.002609072746106363.  So at x = 3.5, where the
    // density is 0.002187491118182885, a height drawn uniformly between the two falls below the
    // density for a uniform number above 0.31256.
    const Real wedgeX = 3.5;
    const auto drawWith = [=](Real uniform)
    {
        // If the first draw is rejected, the second is returned at once, as it's well inside layer 200.
        const ZigguratGaussianRng<ScriptedRng> random(
            ScriptedRng{{drawFor(1, wedgeX / 3.654152885361008796), drawFor(200, 0.1)}, {uniform}});
        return random.next().value;
    };

    for (Real accepted : {0.32, 0.5, 0.99})
    {
        BOOST_TEST_CONTEXT("uniform number " << accepted)
        {
            BOOST_CHECK_SMALL(drawWith(accepted) - wedgeX, 1e-12);
        }
    }
    for (Real rejected : {0.01, 0.31})
    {
        BOOST_TEST_CONTEXT("uniform number " << rejected)
        {
            BOOST_CHECK_LT(std::fabs(drawWith(rejected)), 1.0);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
