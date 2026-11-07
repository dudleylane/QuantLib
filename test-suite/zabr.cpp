/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2026 Aaditya Panikath

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

#include "preconditions.hpp"
#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/termstructures/volatility/sabrsmilesection.hpp>
#include <ql/termstructures/volatility/zabr.hpp>
#include <ql/termstructures/volatility/zabrsmilesection.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ZabrTests)

BOOST_AUTO_TEST_CASE(testConsistency, *precondition(if_speed(Slow)))
{
    BOOST_TEST_MESSAGE("Testing the consistency of ZABR interpolation...");

    Real tol = 1E-4;

    Real alpha = 0.08;
    Real beta = 0.70;
    Real nu = 0.20;
    Real rho = -0.30;
    Real tau = 5.0;
    Real forward = 0.03;

    SabrSmileSection sabr(tau, forward, {alpha, beta, nu, rho});

    ZabrSmileSection<ZabrShortMaturityLognormal> zabr0(tau, forward, {alpha, beta, nu, rho, 1.0});

    ZabrSmileSection<ZabrShortMaturityNormal> zabr1(tau, forward, {alpha, beta, nu, rho, 1.0});

    ZabrSmileSection<ZabrLocalVolatility> zabr2(tau, forward, {alpha, beta, nu, rho, 1.0});

    // for full finite prices reduce the number of intermediate points here
    // below the recommended value to speed up the test
    ZabrSmileSection<ZabrFullFd> zabr3(tau, forward, {alpha, beta, nu, rho, 1.0}, std::vector<Real>(), 2);

    Real k = 0.0001;
    while (k <= 0.70)
    {
        Real c0 = sabr.optionPrice(k);
        Real z0 = zabr0.optionPrice(k);
        Real z1 = zabr1.optionPrice(k);
        Real z2 = zabr2.optionPrice(k);
        Real z3 = zabr3.optionPrice(k);
        if (std::fabs(z0 - c0) > tol)
            BOOST_ERROR("Zabr short maturity lognormal expansion price "
                        "("
                        << z0
                        << ") deviates from Sabr Hagan 2002 price "
                           "by "
                        << (z0 - c0));
        if (std::fabs(z1 - c0) > tol)
            BOOST_ERROR("Zabr short maturity normal expansion price "
                        "("
                        << z1
                        << ") deviates from Sabr Hagan 2002 price "
                           "by "
                        << (z1 - c0));
        if (std::fabs(z2 - c0) > tol)
            BOOST_ERROR("Zabr local volatility price "
                        "("
                        << z2
                        << ") deviates from Sabr Hagan 2002 price "
                           "by "
                        << (z2 - c0));
        if (std::fabs(z3 - c0) > tol)
            BOOST_ERROR("Zabr full finite difference price "
                        "("
                        << z3
                        << ") deviates from Sabr Hagan 2002 price "
                           "by "
                        << (z3 - c0));
        k += 0.0001;
    }
}

BOOST_AUTO_TEST_CASE(testEmptyStrikesAreRefused)
{
    BOOST_TEST_MESSAGE("Testing that ZABR refuses an empty list of strikes...");

    const std::vector<Real> none;

    // A gamma of one has a closed form, and any other walks from the forward.
    for (Real gamma : {1.0, 0.8})
    {
        ZabrModel model(1.0, 0.03, 0.08, 0.7, 0.2, -0.3, gamma);

        BOOST_CHECK_EXCEPTION(model.fdPrice(none), Error, ExpectedErrorMessage("no strikes given"));
        BOOST_CHECK_EXCEPTION(model.localVolatility(none), Error, ExpectedErrorMessage("no strikes given"));
        BOOST_CHECK_EXCEPTION(model.lognormalVolatility(none), Error, ExpectedErrorMessage("no strikes given"));
        BOOST_CHECK_EXCEPTION(model.normalVolatility(none), Error, ExpectedErrorMessage("no strikes given"));
    }

    // A section keeps only the positive strikes, so these leave it none.
    const std::vector<Real> params = {0.08, 0.7, 0.2, -0.3, 0.8};
    const std::vector<Real> zeroMoneyness = {0.0};

    BOOST_CHECK_EXCEPTION(ZabrSmileSection<ZabrLocalVolatility>(1.0, 0.0, params), Error,
                          ExpectedErrorMessage("no strikes given"));
    BOOST_CHECK_EXCEPTION(ZabrSmileSection<ZabrLocalVolatility>(1.0, 0.03, params, zeroMoneyness), Error,
                          ExpectedErrorMessage("no strikes given"));
}

BOOST_AUTO_TEST_CASE(testUnorderedStrikesAreRefused)
{
    BOOST_TEST_MESSAGE("Testing that ZABR refuses strikes out of order...");

    const ExpectedErrorMessage outOfOrder("strikes must be strictly ascending");

    // These would price on a narrower grid than their largest strike calls for, or off it,
    // and the volatilities walk from one strike to the next.
    ZabrModel model(1.0, 0.03, 0.08, 0.7, 0.2, -0.3, 0.8);
    const std::vector<std::vector<Real>> strikeLists = {{0.12, 0.09}, {0.2, 0.04},        {0.04, 0.02},
                                                        {0.02, 0.02}, {0.01, 0.03, 0.02}, {0.02, 0.03, 0.03, 0.05}};
    for (const auto& strikes : strikeLists)
    {
        BOOST_CHECK_EXCEPTION(model.fdPrice(strikes), Error, outOfOrder);
        BOOST_CHECK_EXCEPTION(model.localVolatility(strikes), Error, outOfOrder);
        BOOST_CHECK_EXCEPTION(model.lognormalVolatility(strikes), Error, outOfOrder);
        BOOST_CHECK_EXCEPTION(model.normalVolatility(strikes), Error, outOfOrder);
    }

    // A section interpolates over its strikes, which is wrong in any other order. Neither of
    // these evaluations prices anything before refusing.
    const std::vector<Real> params = {0.08, 0.7, 0.2, -0.3, 0.8};
    const std::vector<std::vector<Real>> moneynessLists = {{3.0, 2.0, 1.5, 1.0, 0.5, 0.25},
                                                           {1.0, 0.25, 3.0, 0.5, 2.0, 1.5},
                                                           {0.25, 0.5, 1.5, 1.0, 2.0, 3.0},
                                                           {0.25, 0.5, 1.0, 1.0, 2.0, 3.0}};
    for (const auto& moneyness : moneynessLists)
    {
        BOOST_CHECK_EXCEPTION(ZabrSmileSection<ZabrLocalVolatility>(1.0, 0.03, params, moneyness), Error, outOfOrder);
        BOOST_CHECK_EXCEPTION(ZabrSmileSection<ZabrFullFd>(1.0, 0.03, params, moneyness), Error, outOfOrder);
        // With refinement there are strikes between them, which changes nothing.
        BOOST_CHECK_EXCEPTION(ZabrSmileSection<ZabrLocalVolatility>(1.0, 0.03, params, moneyness, 3), Error,
                              outOfOrder);
    }
}

BOOST_AUTO_TEST_CASE(testSectionSkipsNonPositiveMoneyness)
{
    BOOST_TEST_MESSAGE("Testing that a ZABR section skips non-positive moneyness in any position...");

    const std::vector<Real> params = {0.08, 0.7, 0.2, -0.3, 0.8};
    const std::vector<Real> moneyness = {0.25, 0.5, 1.0, 1.5, 2.0, 3.0};
    const std::vector<Real> withOthers = {-2.0, 0.0, 0.25, 0.5, -1.0, 1.0, 1.5, 0.0, 2.0, 3.0, -3.0};

    ZabrSmileSection<ZabrLocalVolatility> section(1.0, 0.03, params, moneyness);
    ZabrSmileSection<ZabrLocalVolatility> sectionWithOthers(1.0, 0.03, params, withOthers);

    for (Real strike : {0.015, 0.03, 0.045, 0.06})
    {
        BOOST_TEST_CONTEXT("strike " << strike)
        {
            BOOST_CHECK_EQUAL(section.optionPrice(strike), sectionWithOthers.optionPrice(strike));
        }
    }
}

BOOST_AUTO_TEST_CASE(testFdPricesOfOrderedStrikes)
{
    BOOST_TEST_MESSAGE("Testing that ZABR's fd prices of ordered strikes come from one grid...");

    ZabrModel model(1.0, 0.03, 0.08, 0.7, 0.2, -0.3, 0.8);

    // Strikes this low leave the grid where it is by default, so each one prices as it does alone.
    const std::vector<Real> strikes = {0.01, 0.02, 0.03, 0.04, 0.05};
    const std::vector<Real> prices = model.fdPrice(strikes);

    BOOST_REQUIRE_EQUAL(prices.size(), strikes.size());
    for (Size i = 0; i < strikes.size(); ++i)
    {
        BOOST_TEST_CONTEXT("strike " << strikes[i])
        {
            BOOST_CHECK_EQUAL(prices[i], model.fdPrice(strikes[i]));
        }
    }

    // And a call is worth less the higher its strike.
    for (Size i = 1; i < prices.size(); ++i)
    {
        BOOST_CHECK(prices[i] < prices[i - 1]);
    }
}

BOOST_AUTO_TEST_CASE(testVolatilitiesOnOneSideOfTheForward)
{
    BOOST_TEST_MESSAGE("Testing ZABR volatilities from strikes on one side of the forward...");

    // The walk goes out from the forward in both directions, whichever strikes it's given, and
    // these lists leave nothing to do in one of them.
    ZabrModel model(1.0, 0.03, 0.08, 0.7, 0.2, -0.3, 0.8);

    const std::vector<Real> around = {0.01, 0.02, 0.03, 0.04, 0.05};
    const std::vector<Real> lognormalAround = model.lognormalVolatility(around);
    const std::vector<Real> normalAround = model.normalVolatility(around);

    const std::vector<std::vector<Size>> subsets = {{0, 1}, {3, 4}, {1, 2}, {2, 3}, {0}, {1}, {2}, {3}, {4}};
    for (const auto& subset : subsets)
    {
        std::vector<Real> strikes;
        for (Size i : subset)
        {
            strikes.push_back(around[i]);
        }

        const std::vector<Real> lognormal = model.lognormalVolatility(strikes);
        const std::vector<Real> normal = model.normalVolatility(strikes);

        for (Size j = 0; j < subset.size(); ++j)
        {
            BOOST_TEST_CONTEXT("strike " << strikes[j] << " of a list of " << strikes.size())
            {
                BOOST_CHECK_SMALL(lognormal[j] - lognormalAround[subset[j]], 1e-9);
                BOOST_CHECK_SMALL(normal[j] - normalAround[subset[j]], 1e-9);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
