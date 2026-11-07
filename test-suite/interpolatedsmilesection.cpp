/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 Paolo D'Elia

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
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <cmath>
#include <vector>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InterpolatedSmileSectionTests)

// Helper for linear interpolation/extrapolation between two points
static Real linearInterp(Real x, Real x1, Real y1, Real x2, Real y2)
{
    if (x2 == x1)
        return y1;
    return y1 + (y2 - y1) * ((x - x1) / (x2 - x1));
}

BOOST_AUTO_TEST_CASE(testInterpolationAndVariance)
{
    BOOST_TEST_MESSAGE("Testing basic behavior of linearly interpolated smile section...");
    // basic scenario: sorted strikes, constructor taking stdDevs (total std devs)
    Time expiry = 0.25; // 3 months
    Real sqrtT = std::sqrt(expiry);
    std::vector<Rate> strikes{90.0, 100.0, 110};
    // total std deviations (i.e., sigma * sqrt(T))
    std::vector<Real> stdDevs{0.20 * sqrtT, 0.15 * sqrtT, 0.18 * sqrtT};
    Real atmLevel = 95.0;

    auto section = ext::make_shared<InterpolatedSmileSection<Linear>>(expiry, strikes, stdDevs, atmLevel);

    // pick an interior strike 95 between 90 and 100
    Real strike = 95.0;

    Real v90 = stdDevs[0] / sqrtT;
    Real v100 = stdDevs[1] / sqrtT;
    Real expectedVol = linearInterp(strike, 90.0, v90, 100.0, v100);

    Real tol = 1e-12;
    QL_CHECK_CLOSE(section->volatilityImpl(strike), expectedVol, tol);

    // variance should be vol^2 * T
    Real expectedVar = expectedVol * expectedVol * expiry;
    QL_CHECK_CLOSE(section->varianceImpl(strike), expectedVar, tol);
}

BOOST_AUTO_TEST_CASE(testExtrapolationWhenAllowed)
{
    BOOST_TEST_MESSAGE("Testing extrapolation behavior of linearly interpolated smile section...");
    // test extrapolation behavior when flatStrikeExtrapolation=false
    Time expiry = 0.25;
    Real sqrtT = std::sqrt(expiry);
    std::vector<Rate> strikes{90.0, 100.0, 110};
    std::vector<Real> stdDevs{0.20 * sqrtT, 0.15 * sqrtT, 0.18 * sqrtT};
    Real atmLevel = 95.0;

    auto section = ext::make_shared<InterpolatedSmileSection<Linear>>(expiry, strikes, stdDevs, atmLevel);

    // compute expected extrapolated vol at strike 80 (below min strike 90)
    Real v90 = stdDevs[0] / sqrtT;
    Real v100 = stdDevs[1] / sqrtT;
    Real strikeLow = 80.0;
    Real expectedLow = linearInterp(strikeLow, 90.0, v90, 100.0, v100);

    Real tol = 1e-12;
    QL_CHECK_CLOSE(section->volatilityImpl(strikeLow), expectedLow, tol);

    // extrapolate above max strike, e.g., 120 using last segment 110-100
    Real v110 = stdDevs[2] / sqrtT;
    Real strikeHigh = 120.0;
    // use last two points (110, v110) and (100, v100) for extrapolation;
    // linearInterp works for extrapolation as well
    Real expectedHigh = linearInterp(strikeHigh, 110.0, v110, 100.0, v100);
    QL_CHECK_CLOSE(section->volatilityImpl(strikeHigh), expectedHigh, tol);
}

BOOST_AUTO_TEST_CASE(testHandlesUpdatePropagates)
{
    BOOST_TEST_MESSAGE("Testing that linearly interpolated smile section observes its quotes...");
    // construct via Quote handles and verify changing the underlying quote updates the section
    Time expiry = 0.25;
    Real sqrtT = std::sqrt(expiry);
    std::vector<Rate> strikes{80.0, 90.0, 100.0};

    // create SimpleQuote instances
    ext::shared_ptr<SimpleQuote> q0(new SimpleQuote(0.20 * sqrtT));
    ext::shared_ptr<SimpleQuote> q1(new SimpleQuote(0.15 * sqrtT));
    ext::shared_ptr<SimpleQuote> q2(new SimpleQuote(0.18 * sqrtT));
    std::vector<Handle<Quote>> stdDevHandles;
    stdDevHandles.emplace_back(q0);
    stdDevHandles.emplace_back(q1);
    stdDevHandles.emplace_back(q2);

    ext::shared_ptr<SimpleQuote> atm(new SimpleQuote(95.0));
    Handle<Quote> atmHandle(atm);

    auto section = ext::make_shared<InterpolatedSmileSection<Linear>>(expiry, strikes, stdDevHandles, atmHandle);

    // current vol at 95
    Real v90 = q1->value() / sqrtT;
    Real v100 = q2->value() / sqrtT;
    Real expectedBefore = linearInterp(95.0, 90.0, v90, 100.0, v100);

    Real tol = 1e-12;
    QL_CHECK_CLOSE(section->volatilityImpl(95.0), expectedBefore, tol);

    // now change the middle quote q1 from 0.15 to 0.20
    q1->setValue(0.20 * sqrtT);

    // after changing the quote, the section should reflect the new vol
    Real v90_after = q1->value() / sqrtT;
    Real expectedAfter = linearInterp(95.0, 90.0, v90_after, 100.0, v100);
    QL_CHECK_CLOSE(section->volatilityImpl(95.0), expectedAfter, tol);
}

BOOST_AUTO_TEST_CASE(testFlatStrikeExtrapolation)
{
    BOOST_TEST_MESSAGE("Testing flat strike extrapolation in interpolated smile section...");
    // construct via Quote handles and verify changing the underlying quote updates the section
    Time expiry = 0.25;
    Real sqrtT = std::sqrt(expiry);
    std::vector<Rate> strikes{90.0, 100.0, 110.0};

    // create SimpleQuote instances
    ext::shared_ptr<SimpleQuote> q0(new SimpleQuote(0.20 * sqrtT));
    ext::shared_ptr<SimpleQuote> q1(new SimpleQuote(0.15 * sqrtT));
    ext::shared_ptr<SimpleQuote> q2(new SimpleQuote(0.18 * sqrtT));
    std::vector<Handle<Quote>> stdDevHandles;
    stdDevHandles.emplace_back(q0);
    stdDevHandles.emplace_back(q1);
    stdDevHandles.emplace_back(q2);

    ext::shared_ptr<SimpleQuote> atm(new SimpleQuote(95.0));
    Handle<Quote> atmHandle(atm);

    auto section = ext::make_shared<InterpolatedSmileSection<Linear>>(
        expiry, strikes, stdDevHandles, atmHandle, Linear(), Actual365Fixed(), ShiftedLognormal, 0.0, true);

    // Check with strike lower than minStrike()
    Real v90 = q0->value() / sqrtT;
    Real strikeLow = 85.0;

    Real tol = 1e-12;
    QL_CHECK_CLOSE(section->volatilityImpl(strikeLow), v90, tol);

    // Check with strike higher than maxStrike()
    Real v110 = q2->value() / sqrtT;
    Real strikeHigh = 120.0;

    QL_CHECK_CLOSE(section->volatilityImpl(strikeHigh), v110, tol);

    // Change minStrike vol quote and check if out-of-bounds vol is the same
    q0->setValue(0.21 * sqrtT);
    Real v90_after = q0->value() / sqrtT;
    QL_CHECK_CLOSE(section->volatilityImpl(strikeLow), v90_after, tol);
}

BOOST_AUTO_TEST_CASE(testErrorThrowingWhenNonSortedStrikes)
{
    BOOST_TEST_MESSAGE("Testing that creation of interpolated smile section with non-sorted strikes throws...");
    // basic scenario: sorted strikes, constructor taking stdDevs (total std devs)
    Time expiry = 0.25; // 3 months
    Real sqrtT = std::sqrt(expiry);
    std::vector<Rate> strikes{90.0, 110.0, 100};
    // total std deviations (i.e., sigma * sqrt(T))
    std::vector<Real> stdDevs{0.20 * sqrtT, 0.15 * sqrtT, 0.18 * sqrtT};
    Real atmLevel = 95.0;

    BOOST_CHECK_THROW(auto section =
                          ext::make_shared<InterpolatedSmileSection<Linear>>(expiry, strikes, stdDevs, atmLevel),
                      QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testDigitalOptionPriceAtLargeLevels)
{
    BOOST_TEST_MESSAGE("Testing smile-section digitals at levels that a fixed gap is too small for...");

    const CumulativeNormalDistribution N;

    for (Real forward : {1.0e2, 1.0e3, 1.0e6, 1.0e8, 1.0e11})
    {
        for (Real stdDev : {0.001, 0.2, 1.0, 4.0})
        {
            const InterpolatedSmileSection<Linear> smile(1.0, {forward * 1e-6, forward, forward * 1e3},
                                                         {stdDev, stdDev, stdDev}, forward);
            for (Real moneyness : {0.1, 0.5, 0.9, 1.0, 1.1, 2.0, 10.0})
            {
                const Real strike = moneyness * forward;
                const Real d2 = -std::log(moneyness) / stdDev - 0.5 * stdDev;
                BOOST_TEST_CONTEXT("forward " << forward << ", standard deviation " << stdDev << ", moneyness "
                                              << moneyness)
                {
                    BOOST_CHECK_SMALL(smile.digitalOptionPrice(strike, Option::Call) - N(d2), 2e-9);
                    BOOST_CHECK_SMALL(smile.digitalOptionPrice(strike, Option::Put) - N(-d2), 2e-9);
                    BOOST_CHECK_SMALL(smile.digitalOptionPrice(strike, Option::Call, 0.9) - 0.9 * N(d2), 2e-9);
                }
            }
        }
    }

    // Normal volatilities, with both signs of the forward, and a shift.
    for (Real forward : {1.0e6, -1.0e6, 1.0e11, -1.0e11})
    {
        const Real level = std::fabs(forward);
        const Real stdDev = 0.2 * level;
        const InterpolatedSmileSection<Linear> smile(1.0, {forward - level, forward, forward + level},
                                                     {stdDev, stdDev, stdDev}, forward, Linear(), Actual365Fixed(),
                                                     VolatilityType::Normal);
        for (Real spread : {-0.5, -0.1, 0.0, 0.1, 0.5})
        {
            const Real strike = forward + spread * level;
            BOOST_TEST_CONTEXT("normal, forward " << forward << ", spread " << spread)
            {
                BOOST_CHECK_SMALL(smile.digitalOptionPrice(strike) - N((forward - strike) / stdDev), 2e-9);
            }
        }
    }

    // A strike or a forward far larger than the other, with either sign, sets the level.
    for (const auto& [forward, strike] : std::vector<std::pair<Real, Real>>{{0.0, -1.0e8}, {-1.0e8, -1.0e3}})
    {
        const Real stdDev = 1.0e8;
        const InterpolatedSmileSection<Linear> smile(1.0, {forward - stdDev, forward, forward + stdDev},
                                                     {stdDev, stdDev, stdDev}, forward, Linear(), Actual365Fixed(),
                                                     VolatilityType::Normal);
        BOOST_TEST_CONTEXT("normal, forward " << forward << ", strike " << strike)
        {
            BOOST_CHECK_SMALL(smile.digitalOptionPrice(strike) - N((forward - strike) / stdDev), 2e-9);
        }
    }

    const Real forward = 1.0e6, shift = 1.0e5, stdDev = 0.2;
    const InterpolatedSmileSection<Linear> smile(1.0, {forward * 1e-3, forward, forward * 1e3},
                                                 {stdDev, stdDev, stdDev}, forward, Linear(), Actual365Fixed(),
                                                 VolatilityType::ShiftedLognormal, shift);
    const Real strike = 1.1 * forward;
    const Real d2 = std::log((forward + shift) / (strike + shift)) / stdDev - 0.5 * stdDev;
    BOOST_CHECK_SMALL(smile.digitalOptionPrice(strike) - N(d2), 2e-9);
}

BOOST_AUTO_TEST_CASE(testDensityAtLargeLevels)
{
    BOOST_TEST_MESSAGE("Testing smile-section densities at levels that a fixed gap is too small for...");

    const NormalDistribution n;

    for (Real forward : {1.0e2, 2.5e3, 5.0e3, 3.8e4, 1.0e6, 1.0e8, 1.0e11})
    {
        for (Real stdDev : {0.001, 0.2, 1.0, 4.0})
        {
            const InterpolatedSmileSection<Linear> smile(1.0, {forward * 1e-6, forward, forward * 1e3},
                                                         {stdDev, stdDev, stdDev}, forward);
            const Real peak = n(0.0) / (forward * stdDev);
            for (Real moneyness : {0.1, 0.5, 0.9, 1.0, 1.1, 2.0, 10.0})
            {
                const Real strike = moneyness * forward;
                const Real d2 = -std::log(moneyness) / stdDev - 0.5 * stdDev;
                const Real expected = n(d2) / (strike * stdDev);
                BOOST_TEST_CONTEXT("forward " << forward << ", standard deviation " << stdDev << ", moneyness "
                                              << moneyness)
                {
                    BOOST_CHECK_SMALL((smile.density(strike) - expected) / peak, 1e-4);
                    BOOST_CHECK_SMALL((smile.density(strike, 0.9) - 0.9 * expected) / peak, 1e-4);
                }
            }
        }
    }

    for (Real forward : {1.0e6, -1.0e6, 1.0e11, -1.0e11})
    {
        const Real level = std::fabs(forward);
        const Real stdDev = 0.2 * level;
        const InterpolatedSmileSection<Linear> smile(1.0, {forward - level, forward, forward + level},
                                                     {stdDev, stdDev, stdDev}, forward, Linear(), Actual365Fixed(),
                                                     VolatilityType::Normal);
        const Real peak = n(0.0) / stdDev;
        for (Real spread : {-0.5, -0.1, 0.0, 0.1, 0.5})
        {
            const Real strike = forward + spread * level;
            BOOST_TEST_CONTEXT("normal, forward " << forward << ", spread " << spread)
            {
                BOOST_CHECK_SMALL((smile.density(strike) - n((forward - strike) / stdDev) / stdDev) / peak, 1e-4);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testGapsAtRateLevels)
{
    BOOST_TEST_MESSAGE("Testing that smile-section gaps stay the caller's at rate levels...");

    const Real digitalGap = 1e-5, densityGap = 1e-4;

    for (Real forward : {0.03, 1.0, 9.0})
    {
        const Real stdDev = 0.2;
        const InterpolatedSmileSection<Linear> smile(1.0, {forward * 1e-6, forward, forward * 1e3},
                                                     {stdDev, stdDev, stdDev}, forward);
        const auto digital = [&](Real strike, Real gap)
        {
            const Real kl = strike - gap / 2.0;
            return (smile.optionPrice(kl) - smile.optionPrice(kl + gap)) / gap;
        };
        for (Real moneyness : {0.5, 1.0, 1.1})
        {
            const Real strike = moneyness * forward;
            const Real kl = strike - densityGap / 2.0;
            BOOST_TEST_CONTEXT("forward " << forward << ", moneyness " << moneyness)
            {
                BOOST_CHECK_EQUAL(smile.digitalOptionPrice(strike), digital(strike, digitalGap));
                BOOST_CHECK_EQUAL(smile.density(strike),
                                  (digital(kl, densityGap) - digital(kl + densityGap, densityGap)) / densityGap);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testLargerGapsAreKept)
{
    BOOST_TEST_MESSAGE("Testing that smile-section gaps above the floors are kept...");

    const Real forward = 1.0e11, gap = 1.0e10, stdDev = 0.2;
    const InterpolatedSmileSection<Linear> smile(1.0, {forward * 1e-6, forward, forward * 1e3},
                                                 {stdDev, stdDev, stdDev}, forward);

    const Real low = smile.optionPrice(forward - gap), at = smile.optionPrice(forward),
               high = smile.optionPrice(forward + gap);

    BOOST_CHECK_SMALL(smile.density(forward, 1.0, gap) * gap * gap / (low - 2.0 * at + high) - 1.0, 1e-8);

    const Real expected = (smile.optionPrice(forward - gap / 2.0) - smile.optionPrice(forward + gap / 2.0)) / gap;
    BOOST_CHECK_SMALL(smile.digitalOptionPrice(forward, Option::Call, 1.0, gap) - expected, 1e-14);
}

BOOST_AUTO_TEST_CASE(testGapsWithoutAtmLevel)
{
    BOOST_TEST_MESSAGE("Testing smile-section gaps where the section has no ATM level...");

    // Its prices need no ATM level, so a missing one must not reach the gaps either.
    class SectionWithoutAtmLevel : public SmileSection
    {
      public:
        SectionWithoutAtmLevel() : SmileSection(1.0, Actual365Fixed()) {}
        Real minStrike() const override { return 0.0; }
        Real maxStrike() const override { return QL_MAX_REAL; }
        Real atmLevel() const override { return Null<Real>(); }
        Real optionPrice(Rate strike, Option::Type, Real) const override { return strike * strike; }

      protected:
        Volatility volatilityImpl(Rate) const override { return 0.2; }
    };

    const SectionWithoutAtmLevel section;

    BOOST_CHECK_SMALL(section.digitalOptionPrice(0.5) + 1.0, 1e-9);
    BOOST_CHECK_SMALL(section.density(0.5) - 2.0, 1e-6);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
