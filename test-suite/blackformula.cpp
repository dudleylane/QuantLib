/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2013 Gary Kennedy
 Copyright (C) 2015, 2024 Peter Caspers
 Copyright (C) 2017 Klaus Spanderen
 Copyright (C) 2020 Marcin Rybacki

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
#include <ql/pricingengines/blackformula.hpp>
#include <cmath>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackFormulaTests)

BOOST_AUTO_TEST_CASE(testBachelierImpliedVol)
{

    BOOST_TEST_MESSAGE("Testing Bachelier implied vol...");

    Real forward = 1.0;
    Real bpvol = 0.01;
    Real tte = 10.0;
    Real stdDev = bpvol * std::sqrt(tte);
    Option::Type optionType = Option::Call;
    Real discount = 0.95;

    Real d[] = {-3.0, -2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 3.0};
    for (Real i : d)
    {


        Real strike = forward - i * bpvol * std::sqrt(tte);

        Real callPrem = bachelierBlackFormula(optionType, strike, forward, stdDev, discount);

        Real impliedBpVol = bachelierBlackFormulaImpliedVolChoi(optionType, strike, forward, tte, callPrem, discount);

        if (std::fabs(bpvol - impliedBpVol) > 1.0e-12)
        {
            BOOST_ERROR("Failed, expected " << bpvol << " realised " << impliedBpVol);
        }

        Real impliedBpVolExact = bachelierBlackFormulaImpliedVol(optionType, strike, forward, tte, callPrem, discount);

        if (std::fabs(bpvol - impliedBpVolExact) > 1.0e-15)
        {
            BOOST_ERROR("Failed, expected " << bpvol << " realised " << impliedBpVolExact);
        }
    }
}

BOOST_AUTO_TEST_CASE(testChambersImpliedVol)
{

    BOOST_TEST_MESSAGE("Testing Chambers-Nawalkha implied vol approximation...");

    Option::Type types[] = {Option::Call, Option::Put};
    Real displacements[] = {0.0000, 0.0010, 0.0050, 0.0100, 0.0200};
    Real forwards[] = {-0.0010, 0.0000, 0.0050, 0.0100, 0.0200, 0.0500};
    Real strikes[] = {-0.0100, -0.0050, -0.0010, 0.0000, 0.0010, 0.0050, 0.0100, 0.0200, 0.0500, 0.1000};
    Real stdDevs[] = {0.10, 0.15, 0.20, 0.30, 0.50, 0.60, 0.70, 0.80, 1.00, 1.50, 2.00};
    Real discounts[] = {1.00, 0.95, 0.80, 1.10};

    Real tol = 5.0E-4;

    for (auto& type : types)
    {
        for (Real& displacement : displacements)
        {
            for (Real& forward : forwards)
            {
                for (Real& strike : strikes)
                {
                    for (Real& stdDev : stdDevs)
                    {
                        for (Real& discount : discounts)
                        {
                            if (forward + displacement > 0.0 && strike + displacement > 0.0)
                            {
                                Real premium = blackFormula(type, strike, forward, stdDev, discount, displacement);
                                Real atmPremium = blackFormula(type, forward, forward, stdDev, discount, displacement);
                                Real iStdDev = blackFormulaImpliedStdDevChambers(type, strike, forward, premium,
                                                                                 atmPremium, discount, displacement);
                                Real moneyness = (strike + displacement) / (forward + displacement);
                                if (moneyness > 1.0)
                                    moneyness = 1.0 / moneyness;
                                Real error = (iStdDev - stdDev) / stdDev * moneyness;
                                if (error > tol)
                                    BOOST_ERROR("Failed to verify Chambers-Nawalkha "
                                                "approximation for "
                                                << type << " displacement=" << displacement << " forward=" << forward
                                                << " strike=" << strike << " discount=" << discount
                                                << " stddev=" << stdDev << " result=" << iStdDev
                                                << " exceeds maximum error tolerance");
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRadoicicStefanicaImpliedVol)
{

    BOOST_TEST_MESSAGE("Testing Radoicic-Stefanica implied vol approximation...");

    const Time T = 1.7;
    const Rate r = 0.1;
    const DiscountFactor df = std::exp(-r * T);

    const Real forward = 100;

    const Volatility vol = 0.3;
    const Real stdDev = vol * std::sqrt(T);

    const Option::Type types[] = {Option::Call, Option::Put};
    const Real strikes[] = {50, 60, 70, 80, 90, 100, 110, 125, 150, 200, 300};

    const Real tol = 0.02;

    for (Real strike : strikes)
    {
        for (auto type : types)
        {
            const ext::shared_ptr<PlainVanillaPayoff> payoff(ext::make_shared<PlainVanillaPayoff>(type, strike));

            const Real marketValue = blackFormula(payoff, forward, stdDev, df);

            const Real estVol =
                blackFormulaImpliedStdDevApproximationRS(payoff, forward, marketValue, df) / std::sqrt(T);

            const Real error = std::fabs(estVol - vol);
            if (error > tol)
            {
                BOOST_ERROR("Failed to verify Radoicic-Stefanica"
                            "approximation for "
                            << type << "\n forward     :" << forward << "\n strike      :" << strike
                            << "\n discount    :" << df << "\n implied vol :" << vol << "\n result      :" << estVol
                            << "\n error       :" << error << "\n tolerance   :" << tol);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRadoicicStefanicaLowerBound)
{

    BOOST_TEST_MESSAGE("Testing Radoicic-Stefanica lower bound...");

    // testing lower bound plot figure 3.1 from
    // "Tighter Bounds for Implied Volatility",
    // J. Gatheral, I. Matic, R. Radoicic, D. Stefanica
    // https://papers.ssrn.com/sol3/papers.cfm?abstract_id=2922742

    const Real forward = 1.0;
    const Real k = 1.2;

    for (Real s = 0.17; s < 2.9; s += 0.01)
    {
        const Real strike = std::exp(k) * forward;
        const Real c = blackFormula(Option::Call, strike, forward, s);
        const Real estimate = blackFormulaImpliedStdDevApproximationRS(Option::Call, strike, forward, c);

        const Real error = s - estimate;
        if (std::isnan(estimate) || std::fabs(error) > 0.05)
        {
            BOOST_ERROR("Failed to lower bound Radoicic-Stefanica"
                        "approximation for "
                        << "\n forward     :" << forward << "\n strike      :" << k << "\n stdDev      :" << s
                        << "\n result      :" << estimate << "\n error       :" << error);
        }

        if (c > 1e-6 && error < 0.0)
        {
            BOOST_ERROR("Failed to verify Radoicic-Stefanica is lower bound"
                        << "\n forward     :" << forward << "\n strike      :" << k << "\n stdDev      :" << s
                        << "\n result      :" << estimate << "\n error       :" << error);
        }
    }
}

BOOST_AUTO_TEST_CASE(testImpliedVolAdaptiveSuccessiveOverRelaxation)
{
    BOOST_TEST_MESSAGE("Testing implied volatility calculation via "
                       "adaptive successive over-relaxation...");

    const DayCounter dc = Actual365Fixed();
    const Date today = Date(12, July, 2017);
    Settings::instance().evaluationDate() = today;

    const Date exerciseDate = today + Period(15, Months);
    const Time exerciseTime = dc.yearFraction(today, exerciseDate);

    const ext::shared_ptr<YieldTermStructure> rTS = flatRate(0.10, dc);
    const ext::shared_ptr<YieldTermStructure> qTS = flatRate(0.06, dc);

    const DiscountFactor df = rTS->discount(exerciseDate);

    const Volatility vol = 0.20;
    const Real stdDev = vol * std::sqrt(exerciseTime);

    const Real s0 = 100;
    const Real forward = s0 * qTS->discount(exerciseDate) / df;

    const Option::Type types[] = {Option::Call, Option::Put};
    const Real strikes[] = {50, 60, 70, 80, 90, 100, 110, 125, 150, 200};
    const Real displacements[] = {0, 25, 50, 100};

    const Real tol = 1e-8;

    for (Real strike : strikes)
    {
        for (auto type : types)
        {
            const ext::shared_ptr<PlainVanillaPayoff> payoff(ext::make_shared<PlainVanillaPayoff>(type, strike));

            for (Real displacement : displacements)
            {

                const Real marketValue = blackFormula(payoff, forward, stdDev, df, displacement);

                const Real impliedStdDev = blackFormulaImpliedStdDevLiRS(payoff, forward, marketValue, df, displacement,
                                                                         Null<Real>(), 1.0, tol, 100);

                const Real error = std::fabs(impliedStdDev - stdDev);
                if (error > 10 * tol)
                {
                    BOOST_ERROR("Failed to calculated implied volatility"
                                " with adaptive successive over-relaxation"
                                << "\n forward     :" << forward << "\n strike      :" << strike << "\n stdDev      :"
                                << stdDev << "\n displacement:" << displacement << "\n result      :" << impliedStdDev
                                << "\n error       :" << error << "\n tolerance   :" << tol);
                }
            }
        }
    }
}

void assertBlackFormulaForwardDerivative(Option::Type optionType, const std::vector<Real>& strikes, Real bpvol)
{
    Real forward = 1.0;
    Real tte = 10.0;
    Real stdDev = bpvol * std::sqrt(tte);
    Real discount = 0.95;
    Real displacement = 0.01;
    Real bump = 0.0001;
    Real epsilon = 1.e-10;
    std::string type = optionType == Option::Call ? "Call" : "Put";

    for (Real strike : strikes)
    {
        Real delta = blackFormulaForwardDerivative(optionType, strike, forward, stdDev, discount, displacement);
        Real bumpedDelta =
            blackFormulaForwardDerivative(optionType, strike, forward + bump, stdDev, discount, displacement);

        Real basePremium = blackFormula(optionType, strike, forward, stdDev, discount, displacement);
        Real bumpedPremium = blackFormula(optionType, strike, forward + bump, stdDev, discount, displacement);
        Real deltaApprox = (bumpedPremium - basePremium) / bump;

        /*! Based on the Mean Value Theorem, the below inequality
            should hold for any function that is monotonic in the
            area of the bump.
         */
        bool success = (std::max(delta, bumpedDelta) + epsilon > deltaApprox) &&
                       (deltaApprox > std::min(delta, bumpedDelta) - epsilon);

        if (!success)
        {
            BOOST_ERROR("Failed to calculate the derivative of the"
                        " Black formula w.r.t. forward"
                        << "\n option type       :" << type << "\n forward           :" << forward
                        << "\n strike            :" << strike << "\n stdDev            :" << stdDev
                        << "\n displacement      :" << displacement << "\n analytical delta  :" << delta
                        << "\n approximated delta:" << deltaApprox);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBlackFormulaForwardDerivative)
{

    BOOST_TEST_MESSAGE("Testing forward derivative of the Black formula...");

    std::vector<Real> strikes;
    strikes.push_back(0.1);
    strikes.push_back(0.5);
    strikes.push_back(1.0);
    strikes.push_back(2.0);
    strikes.push_back(3.0);
    const Real vol = 0.1;
    assertBlackFormulaForwardDerivative(Option::Call, strikes, vol);
    assertBlackFormulaForwardDerivative(Option::Put, strikes, vol);
}

BOOST_AUTO_TEST_CASE(testBlackFormulaForwardDerivativeWithZeroStrike)
{

    BOOST_TEST_MESSAGE("Testing forward derivative of the Black formula "
                       "with zero strike...");

    std::vector<Real> strikes;
    strikes.push_back(0.0);
    const Real vol = 0.1;
    assertBlackFormulaForwardDerivative(Option::Call, strikes, vol);
    assertBlackFormulaForwardDerivative(Option::Put, strikes, vol);
}

BOOST_AUTO_TEST_CASE(testBlackFormulaForwardDerivativeWithZeroVolatility)
{

    BOOST_TEST_MESSAGE("Testing forward derivative of the Black formula "
                       "with zero volatility...");

    std::vector<Real> strikes;
    strikes.push_back(0.1);
    strikes.push_back(0.5);
    strikes.push_back(1.0);
    strikes.push_back(2.0);
    strikes.push_back(3.0);
    const Real vol = 0.0;
    assertBlackFormulaForwardDerivative(Option::Call, strikes, vol);
    assertBlackFormulaForwardDerivative(Option::Put, strikes, vol);
}

void assertBachelierBlackFormulaForwardDerivative(Option::Type optionType, const std::vector<Real>& strikes, Real bpvol)
{
    Real forward = 1.0;
    Real tte = 10.0;
    Real stdDev = bpvol * std::sqrt(tte);
    Real discount = 0.95;
    Real bump = 0.0001;
    Real epsilon = 1.e-10;
    std::string type = optionType == Option::Call ? "Call" : "Put";

    for (Real strike : strikes)
    {
        Real delta = bachelierBlackFormulaForwardDerivative(optionType, strike, forward, stdDev, discount);
        Real bumpedDelta = bachelierBlackFormulaForwardDerivative(optionType, strike, forward + bump, stdDev, discount);

        Real basePremium = bachelierBlackFormula(optionType, strike, forward, stdDev, discount);
        Real bumpedPremium = bachelierBlackFormula(optionType, strike, forward + bump, stdDev, discount);
        Real deltaApprox = (bumpedPremium - basePremium) / bump;

        /*! Based on the Mean Value Theorem, the below inequality
            should hold for any function that is monotonic in the
            area of the bump.
         */
        bool success = (std::max(delta, bumpedDelta) + epsilon > deltaApprox) &&
                       (deltaApprox > std::min(delta, bumpedDelta) - epsilon);

        if (!success)
        {
            BOOST_ERROR("Failed to calculate the derivative of the"
                        " Bachelier Black formula w.r.t. forward"
                        << "\n option type       :" << type << "\n forward           :" << forward
                        << "\n strike            :" << strike << "\n stdDev            :" << stdDev
                        << "\n analytical delta  :" << delta << "\n approximated delta:" << deltaApprox);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBachelierBlackFormulaForwardDerivative)
{

    BOOST_TEST_MESSAGE("Testing forward derivative of the "
                       "Bachelier Black formula...");

    std::vector<Real> strikes;
    strikes.push_back(-3.0);
    strikes.push_back(-2.0);
    strikes.push_back(-1.0);
    strikes.push_back(-0.5);
    strikes.push_back(0.0);
    strikes.push_back(0.5);
    strikes.push_back(1.0);
    strikes.push_back(2.0);
    strikes.push_back(3.0);
    const Real vol = 0.001;
    assertBachelierBlackFormulaForwardDerivative(Option::Call, strikes, vol);
    assertBachelierBlackFormulaForwardDerivative(Option::Put, strikes, vol);
}

BOOST_AUTO_TEST_CASE(testBachelierBlackFormulaForwardDerivativeWithZeroVolatility)
{

    BOOST_TEST_MESSAGE("Testing forward derivative of the Bachelier Black formula "
                       "with zero volatility...");

    std::vector<Real> strikes;
    strikes.push_back(-3.0);
    strikes.push_back(-2.0);
    strikes.push_back(-1.0);
    strikes.push_back(-0.5);
    strikes.push_back(0.0);
    strikes.push_back(0.5);
    strikes.push_back(1.0);
    strikes.push_back(2.0);
    strikes.push_back(3.0);
    const Real vol = 0.0;
    assertBachelierBlackFormulaForwardDerivative(Option::Call, strikes, vol);
    assertBachelierBlackFormulaForwardDerivative(Option::Put, strikes, vol);
}

BOOST_AUTO_TEST_CASE(testDeepInTheMoneyPricesAreNotBelowIntrinsic)
{
    BOOST_TEST_MESSAGE("Testing that deep in-the-money Black and Bachelier prices are not below intrinsic...");

    struct Case
    {
        Option::Type type;
        Real strike, forward;
    };
    const Case cases[] = {{Option::Call, 50.0, 100.0}, {Option::Put, 100.0, 50.0},     {Option::Call, 0.03, 0.05},
                          {Option::Put, 0.05, 0.03},   {Option::Call, 1000.0, 1234.5}, {Option::Put, 1234.5, 1000.0}};

    for (const Case& c : cases)
    {
        const Real moneyness = Integer(c.type) * (c.forward - c.strike);
        for (Real discount : {1.0, 0.97})
        {
            const Real intrinsic = discount * moneyness;

            // With both d1 and d2 above 20 in the first, and (forward - strike) / stdDev at 20 in the second.
            const Real black = blackFormula(c.type, c.strike, c.forward, 0.01, discount);
            const Real bachelier = bachelierBlackFormula(c.type, c.strike, c.forward, moneyness / 20.0, discount);
            BOOST_TEST_CONTEXT("strike " << c.strike << ", forward " << c.forward << ", discount " << discount)
            {
                BOOST_CHECK_MESSAGE(black >= intrinsic, "Black is below intrinsic by " << intrinsic - black);
                BOOST_CHECK_MESSAGE(bachelier >= intrinsic,
                                    "Bachelier is below intrinsic by " << intrinsic - bachelier);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testItmProbabilitiesWithZeroVolatility)
{
    BOOST_TEST_MESSAGE("Testing in-the-money probabilities with zero volatility...");

    // Without variance these are the limits of the formulas: 1 in the money and 0 out of it.
    struct Case
    {
        Option::Type type;
        Real strike, forward, displacement;
    };
    const Case cases[] = {{Option::Call, 0.03, 0.05, 0.0},   {Option::Call, 0.05, 0.03, 0.0},
                          {Option::Put, 0.03, 0.05, 0.0},    {Option::Put, 0.05, 0.03, 0.0},
                          {Option::Call, 90.0, 100.0, 0.0},  {Option::Put, 90.0, 100.0, 0.0},
                          {Option::Call, -0.01, 0.01, 0.02}, {Option::Put, -0.01, 0.01, 0.02}};

    const Real tinyStdDev = 1.0e-8;
    const Real tolerance = 1.0e-12;
    for (const Case& c : cases)
    {
        const Real expected = Integer(c.type) * (c.forward - c.strike) > 0.0 ? 1.0 : 0.0;
        BOOST_TEST_CONTEXT("type " << c.type << ", strike " << c.strike << ", forward " << c.forward)
        {
            BOOST_CHECK_SMALL(
                blackFormulaCashItmProbability(c.type, c.strike, c.forward, 0.0, c.displacement) - expected, tolerance);
            BOOST_CHECK_SMALL(blackFormulaAssetItmProbability(c.type, c.strike, c.forward, 0.0, c.displacement) -
                                  expected,
                              tolerance);
            BOOST_CHECK_SMALL(bachelierBlackFormulaAssetItmProbability(c.type, c.strike, c.forward, 0.0) - expected,
                              tolerance);

            BOOST_CHECK_SMALL(blackFormulaCashItmProbability(c.type, c.strike, c.forward, tinyStdDev, c.displacement) -
                                  expected,
                              tolerance);
            BOOST_CHECK_SMALL(blackFormulaAssetItmProbability(c.type, c.strike, c.forward, tinyStdDev, c.displacement) -
                                  expected,
                              tolerance);
            BOOST_CHECK_SMALL(bachelierBlackFormulaAssetItmProbability(c.type, c.strike, c.forward, tinyStdDev) -
                                  expected,
                              tolerance);
        }
    }

    // On the strike, all three take it as out of the money.
    for (auto type : {Option::Call, Option::Put})
    {
        BOOST_CHECK_EQUAL(blackFormulaCashItmProbability(type, 0.05, 0.05, 0.0), 0.0);
        BOOST_CHECK_EQUAL(blackFormulaAssetItmProbability(type, 0.05, 0.05, 0.0), 0.0);
        BOOST_CHECK_EQUAL(bachelierBlackFormulaAssetItmProbability(type, 0.05, 0.05, 0.0), 0.0);
    }
}

BOOST_AUTO_TEST_CASE(testBachelierImpliedVolAtIntrinsic)
{
    BOOST_TEST_MESSAGE("Testing Bachelier implied volatility for prices at intrinsic value...");

    // Far enough in the money, a price is its intrinsic value up to rounding, to either side of it.
    const Real tte = 0.01;
    const Real vol = 1.0e-5;

    for (Real forward : {-0.01, 0.005, 0.05, 1.0})
    {
        for (Real offset : {0.01, 0.03, 0.07})
        {
            for (auto type : {Option::Call, Option::Put})
            {
                const Real strike = (type == Option::Call) ? forward - offset : forward + offset;
                const Real intrinsic = Integer(type) * (forward - strike);

                for (Real discount : {1.0, 0.95, 0.5})
                {
                    BOOST_TEST_CONTEXT("type " << type << ", strike " << strike << ", forward " << forward
                                               << ", discount " << discount)
                    {
                        const Real price = bachelierBlackFormula(type, strike, forward, vol * std::sqrt(tte), discount);
                        BOOST_CHECK_GE(bachelierBlackFormulaImpliedVol(type, strike, forward, tte, price, discount),
                                       0.0);

                        for (Real ulps : {4.0, 16.0})
                        {
                            const Real below = intrinsic * (1.0 - ulps * QL_EPSILON) * discount;
                            BOOST_CHECK_EQUAL(
                                bachelierBlackFormulaImpliedVol(type, strike, forward, tte, below, discount), 0.0);
                        }
                    }
                }

                BOOST_TEST_CONTEXT("type " << type << ", strike " << strike << ", forward " << forward)
                {
                    BOOST_CHECK_EQUAL(bachelierBlackFormulaImpliedVol(type, strike, forward, tte, intrinsic), 0.0);
                    BOOST_CHECK_EQUAL(
                        bachelierBlackFormulaImpliedVol(type, strike, forward, tte, intrinsic * (1.0 - QL_EPSILON)),
                        0.0);

                    // Further below than rounding can explain, there's no volatility to find.
                    BOOST_CHECK_EXCEPTION(
                        bachelierBlackFormulaImpliedVol(type, strike, forward, tte, intrinsic - 1.0e-12), Error,
                        ExpectedErrorMessage("negative time value"));
                }
            }
        }
    }

    // Rounding goes with the forward and the strike, whatever the distance
    // between them: the next Real below a forward of 100 is 1.4e-14 away.
    const Real strike = 99.99, forward = 100.0;
    const Real price = std::nextafter(forward, 0.0) - strike;
    BOOST_CHECK_EQUAL(bachelierBlackFormulaImpliedVol(Option::Call, strike, forward, tte, price), 0.0);
}

BOOST_AUTO_TEST_CASE(testBachelierImpliedVolJustAboveIntrinsic)
{
    BOOST_TEST_MESSAGE("Testing Bachelier implied volatility for prices just above intrinsic value...");

    const Real tte = 1.0;
    const Real vol = 1.0;
    const Real stdDev = vol * std::sqrt(tte);
    const Real forward = 0.0;

    // Time values here are near the rounding of the price, and the
    // implied volatility moves much less than they do: at 7.6, they're
    // about 4e-15 on a price of 7.6, and the implied volatility is within 2%.
    Size inverted = 0;
    for (Size i = 0; i <= 12; ++i)
    {
        const Real distance = 7.0 + i * 0.05;
        for (auto type : {Option::Call, Option::Put})
        {
            const Real strike = (type == Option::Call) ? forward - distance : forward + distance;
            for (Real discount : {1.0, 0.99, 0.95, 0.9})
            {
                BOOST_TEST_CONTEXT("type " << type << ", distance " << distance << ", discount " << discount)
                {
                    const Real price = bachelierBlackFormula(type, strike, forward, stdDev, discount);
                    const Real implied = bachelierBlackFormulaImpliedVol(type, strike, forward, tte, price, discount);
                    if (price / discount > distance)
                    {
                        BOOST_CHECK_SMALL(implied / vol - 1.0, 0.03);
                        ++inverted;
                    }
                    else
                    {
                        BOOST_CHECK_EQUAL(implied, 0.0);
                    }
                }
            }
        }
    }
    BOOST_CHECK_GT(inverted, Size(0));
}

BOOST_AUTO_TEST_CASE(testBachelierImpliedVolFarOutOfTheMoney)
{
    BOOST_TEST_MESSAGE("Testing Bachelier implied volatility for very low prices...");

    // However low, the price of an out-of-the-money option says what its volatility is.
    for (Real forward : {-0.01, 0.05, 1.0})
    {
        for (Real tte : {0.01, 1.0})
        {
            for (Real vol : {1.0e-5, 0.01})
            {
                for (Real distance : {10.0, 15.0, 20.0, 30.0, 35.0})
                {
                    for (auto type : {Option::Call, Option::Put})
                    {
                        const Real stdDev = vol * std::sqrt(tte);
                        const Real strike = forward + Integer(type) * distance * stdDev;
                        for (Real discount : {1.0, 0.95})
                        {
                            BOOST_TEST_CONTEXT("type " << type << ", strike " << strike << ", forward " << forward
                                                       << ", stdDev " << stdDev << ", discount " << discount)
                            {
                                const Real price = bachelierBlackFormula(type, strike, forward, stdDev, discount);
                                BOOST_CHECK_SMALL(
                                    bachelierBlackFormulaImpliedVol(type, strike, forward, tte, price, discount) / vol -
                                        1.0,
                                    1.0e-9);
                            }
                        }
                    }
                }
            }
        }
    }

    // No price at all is no volatility, and neither is one that's a rounding below it.
    BOOST_CHECK_EQUAL(bachelierBlackFormulaImpliedVol(Option::Call, 1.01, 1.0, 1.0, 0.0), 0.0);
    BOOST_CHECK_EQUAL(bachelierBlackFormulaImpliedVol(Option::Call, 1.01, 1.0, 1.0, -1.0e-20), 0.0);
    BOOST_CHECK_EXCEPTION(bachelierBlackFormulaImpliedVol(Option::Call, 1.01, 1.0, 1.0, -1.0e-12), Error,
                          ExpectedErrorMessage("negative time value"));
}

BOOST_AUTO_TEST_CASE(testImpliedStdDevApproximationRSAgainstItsFormula)
{
    BOOST_TEST_MESSAGE("Testing the RS implied-standard-deviation approximation against its formula...");

    // The formula evaluated at 700 digits on these same doubles.
    struct Case
    {
        Option::Type type;
        Real strike, forward, price, discount, displacement, expected;
    };
    const Case cases[] = {
        {Option::Call, 1, 1, 3.9894228040143267e-09, 1, 0, 1e-08},
        {Option::Put, 1, 1, 3.9894228040143267e-09, 1, 0, 1e-08},
        {Option::Call, 1, 1, 3.9894228023520673e-05, 1, 0, 9.9999999998122072e-05},
        {Option::Call, 1, 1, 0.039877611676744924, 1, 0, 0.099998122259636515},
        {Option::Call, 1, 1, 0.51940803048395034, 0.94999999999999996, 0, 1.4938077345206533},
        {Option::Call, 1, 1, 0.98758066934844768, 1, 0, 4.822627038476881},
        {Option::Call, 0.99999999999900002, 1, 3.9899228128977036e-09, 1, 0, 9.9999999977465832e-09},
        {Option::Put, 0.99999999999900002, 1, 3.9889228350194237e-09, 1, 0, 9.9999999977465832e-09},
        {Option::Call, 1.0000000000010001, 1, 3.9893727997707974e-08, 1, 0, 9.9999999999774605e-08},
        {Option::Put, 0.99999999989999999, 1, 3.9889228237204362e-07, 1, 0, 9.999999997746462e-07},
        {Option::Call, 1.0000100000500001, 1, 7.4745976348756754e-31, 1, 0, 1.0741889306204562e-06},
        {Option::Call, 1.0001000050001667, 1, 7.4749339915241401e-30, 1, 0, 1.0741889306209454e-05},
        {Option::Put, 0.99990000499983334, 1, 7.4741865358177424e-30, 1, 0, 1.0741889306209151e-05},
        {Option::Call, 1.2840254166877414, 1, 1.3810607788827693e-141, 1, 0, 0.011143868220185562},
        {Option::Call, 2.7182818284590451, 1, 1.1290332270977463e-91, 1, 0, 0.055425652303931372},
        {Option::Call, 7.3890560989306504, 1, 3.7194507268047054e-91, 1, 0, 0.11084895914816804},
        {Option::Call, 20.085536923187668, 1, 7.3048061017546562e-200, 1, 0, 0.11179083744794927},
        {Option::Put, 0.049787068367863944, 1, 3.6024422405362772e-16, 1, 0, 0.42003994631215757},
        {Option::Put, -0.42531939744820407, 1, 5.4552732120310238e-201, 1, 0.5, 0.11179083744794927},
        {Option::Call, 0.36787944117144233, 1, 0.74998615718412287, 1, 0, 1.4799190359623655},
        {Option::Put, 0.36787944117144233, 1, 0.022082291977311399, 0.94999999999999996, 0, 0.77385818023638275},
        {Option::Call, 0.77880078307140488, 1, 0.27773748538173915, 1, 0, 0.39639866572116245},
        {Option::Put, 1.2840254166877414, 1, 0.29545084173928648, 1, 0, 0.19352225332913914},
        {Option::Call, 0.9999850000749998, 1, 0.00060594028638469267, 1, 0.5, 0.00099999774460664574},
        {Option::Call, 148.4131591025766, 1, 2.9548333577107236e-139, 1, 0, 0.22285180839078805},
        {Option::Call, 0.90483741803595952, 1, 0.98818827348728866, 1, 0, 4.8226256040435693},
        {Option::Put, 0.99900049983337502, 1, 7.4708238994857728e-29, 1, 0, 0.00010741889305968087},
        {Option::Call, 1.0000010000005, 1, 7.4745640208128288e-32, 1, 0, 1.0741889306184211e-07},
        {Option::Put, 0.99999900000050002, 1, 7.4745565293961694e-32, 1, 0, 1.0741889306200149e-07},
        // Very low prices on the money, where C E as a product underflows.
        {Option::Call, 1, 1, 3.9894228040143267e-61, 1, 0, 9.9999999999999997e-61},
        {Option::Put, 1, 1, 1.9947114020071634e-61, 0.5, 0, 9.9999999999999997e-61},
        {Option::Call, 1, 1, 3.9894228040143273e-71, 1, 0, 1.0000000000000002e-70},
        // The strike very far from the forward.
        {Option::Put, 2.0611536224385579e-09, 1, 7.7816898215502792e-117, 1, 0, 0.9924524989068878},
        {Option::Call, 485165195.40979028, 1, 3.7754050628908155e-108, 1, 0, 0.9924524989068878},
        {Option::Call, 10686474581524.463, 1, 2.2528111029279398e-160, 1, 0, 1.2197974543496031},
        {Option::Put, 4.2483542552915889e-18, 1, 6.0513847385228145e-230, 1, 0, 1.4110861237036965},
        {Option::Put, 3.9754497359086468e-31, 1, 4.7216512379786153e-114, 1, 0, 3.6670204038622751},
        {Option::Put, 3.7200759760208361e-44, 1, 1.2806657287815413e-161, 1, 0, 4.3944003680537742},
        {Option::Put, 1.3838965267367376e-87, 1, 7.0319292613061161e-100, 1, 0, 14.527690442609073},
        {Option::Put, 9.9295903962649796e-153, 1, 2.7757353114609385e-173, 1, 0, 19.292964757078},
    };
    // Where the two branches meet, gamma - |y| is nearly 0, so gamma's rounding shows through its square root.
    const Case atTheSwitch[] = {
        {Option::Call, 100.00000120874823, 100, 0.0062022700721393598, 1, 0, 0.00015548300342851077},
        {Option::Put, 0.99999969560725488, 1, 0.00031112134099592348, 1, 0, 0.0007802471255113515},
        {Option::Put, 1.0000000001, 1, 5.3598487656668984e-06, 0.94999999999999996, 0, 1.4142136208418913e-05},
        {Option::Call, 1.00000000015, 1, 8.039630648488557e-06, 0.94999999999999996, 0.5, 1.4142136208418913e-05},
        {Option::Put, 0.99999999984999999, 1, 8.0396306480865721e-06, 0.94999999999999996, 0.5, 1.4142136209126019e-05},
    };

    const auto check = [](const Case& c, Real tolerance)
    {
        BOOST_TEST_CONTEXT("type " << c.type << ", strike " << c.strike << ", forward " << c.forward << ", price "
                                   << c.price << ", discount " << c.discount << ", displacement " << c.displacement)
        {
            const Real approx = blackFormulaImpliedStdDevApproximationRS(c.type, c.strike, c.forward, c.price,
                                                                         c.discount, c.displacement);
            BOOST_CHECK_SMALL(approx / c.expected - 1.0, tolerance);
        }
    };
    for (const Case& c : cases)
        check(c, 1.0e-9);
    for (const Case& c : atTheSwitch)
        check(c, 1.0e-7);
}

BOOST_AUTO_TEST_CASE(testImpliedStdDevApproximationRSAcrossMoneyness)
{
    BOOST_TEST_MESSAGE("Testing the RS implied-standard-deviation approximation near and away from the money...");

    // The approximation's own error goes up to 12% far out of the money, and it holds that at any scale.
    for (Real forward : {1.0, 100.0, 0.03})
    {
        for (Real displacement : {0.0, 0.5})
        {
            for (Real discount : {1.0, 0.95})
            {
                for (Real stdDev : {1.0e-8, 1.0e-6, 1.0e-4, 1.0e-2, 0.1, 0.5})
                {
                    for (Real distance :
                         {30.0, 10.0, 3.0, 1.0, 0.3, 1.0e-3, 0.0, -1.0e-3, -0.3, -1.0, -3.0, -10.0, -30.0})
                    {
                        const Real logMoneyness = distance * stdDev;
                        const Real strike = (forward + displacement) * std::exp(-logMoneyness) - displacement;
                        for (auto type : {Option::Call, Option::Put})
                        {
                            // Further in the money, the price doesn't say what the standard deviation is.
                            if (Integer(type) * distance > 1.0)
                                continue;

                            BOOST_TEST_CONTEXT("type " << type << ", strike " << strike << ", forward " << forward
                                                       << ", stdDev " << stdDev << ", discount " << discount
                                                       << ", displacement " << displacement)
                            {
                                const Real price = blackFormula(type, strike, forward, stdDev, discount, displacement);
                                const Real approx = blackFormulaImpliedStdDevApproximationRS(
                                    type, strike, forward, price, discount, displacement);
                                BOOST_CHECK_SMALL(approx / stdDev - 1.0, 0.12);
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testImpliedStdDevApproximationRSAtItsLimits)
{
    BOOST_TEST_MESSAGE("Testing the RS implied-standard-deviation approximation at the ends of its range...");

    // In the money with a time value lost to rounding, and everywhere from far to near the strike.
    for (Real ratio : {1.0001, 1.01, 1.1, 1.5, 2.0, 3.0, 5.0, 10.0})
    {
        for (Real stdDev : {1.0e-8, 1.0e-4, 0.01, 0.05, 0.1, 0.2, 0.4, 0.8, 1.5})
        {
            for (auto type : {Option::Call, Option::Put})
            {
                for (Real strike : {1.0 / ratio, 1.0 * ratio})
                {
                    BOOST_TEST_CONTEXT("type " << type << ", strike " << strike << ", stdDev " << stdDev)
                    {
                        const Real price = blackFormula(type, strike, 1.0, stdDev);
                        const Real approx = blackFormulaImpliedStdDevApproximationRS(type, strike, 1.0, price);
                        BOOST_CHECK(std::isfinite(approx));
                        BOOST_CHECK_GE(approx, 0.0);
                    }
                }
            }
        }
    }

    for (auto type : {Option::Call, Option::Put})
    {
        // No price, on the money or off it, is no variance.
        BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(type, 1.0, 1.0, 0.0), 0.0);
        BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(type, 1.0 + 0.2 * Integer(type), 1.0, 0.0), 0.0);

        // So is an intrinsic value, or anything below it.
        const Real strike = 1.0 - 0.2 * Integer(type);
        const Real intrinsic = Integer(type) * (1.0 - strike);
        BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(type, strike, 1.0, intrinsic), 0.0);
        BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(type, strike, 1.0, 0.75 * intrinsic), 0.0);
        BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(type, strike, 1.0, 0.5 * intrinsic), 0.0);
        BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(type, strike, 1.0, 0.0), 0.0);
    }

    // A price too low to carry through the formula's squares is no variance, not a NaN.
    BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(Option::Call, 1.0, 1.0, 4.0e-101), 0.0);
    BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevApproximationRS(Option::Call, 1.0, 1.0, 4.0e-201), 0.0);

    // The most an option can be worth takes an infinite standard deviation, and so does anything above that.
    BOOST_CHECK(std::isinf(blackFormulaImpliedStdDevApproximationRS(Option::Call, 1.0, 1.0, 1.0)));
    BOOST_CHECK(std::isinf(blackFormulaImpliedStdDevApproximationRS(Option::Call, 1.0, 1.0, 1.5)));
    BOOST_CHECK(std::isinf(blackFormulaImpliedStdDevApproximationRS(Option::Put, 1.0, 1.0, 1.0)));
    BOOST_CHECK(std::isinf(blackFormulaImpliedStdDevApproximationRS(Option::Put, 2.0611536224385579e-09, 1.0, 1.0e-5)));
    BOOST_CHECK(std::isinf(blackFormulaImpliedStdDevApproximationRS(Option::Call, 485165195.40979028, 1.0, 1.5)));
    BOOST_CHECK(std::isinf(blackFormulaImpliedStdDevApproximationRS(Option::Call, 2.6881171418161356e+43, 1.0, 1.5)));
}

BOOST_AUTO_TEST_CASE(testImpliedStdDevAtIntrinsic)
{
    BOOST_TEST_MESSAGE("Testing implied standard deviations for prices at intrinsic value...");

    const auto liRS = [](Option::Type type, Real strike, Real forward, Real price, Real discount, Real displacement)
    { return blackFormulaImpliedStdDevLiRS(type, strike, forward, price, discount, displacement); };
    const auto newton = [](Option::Type type, Real strike, Real forward, Real price, Real discount, Real displacement)
    { return blackFormulaImpliedStdDev(type, strike, forward, price, discount, displacement); };

    // These strikes and displacements add up exactly, so the intrinsic value holds no rounding of its own.
    const Real forward = 1.0;
    for (Real discount : {1.0, 0.95, 0.5})
    {
        for (Real displacement : {0.0, 0.5})
        {
            for (Real strike : {0.25, 0.5, 0.75, 0.875, 1.125, 1.25, 1.5, 2.0, 4.0})
            {
                const Option::Type type = strike < forward ? Option::Call : Option::Put;
                const Real intrinsic = Integer(type) * (forward - strike);

                BOOST_TEST_CONTEXT("strike " << strike << ", discount " << discount << ", displacement "
                                             << displacement)
                {
                    // Below it by what rounding can explain.
                    for (Real ulps : {4.0, 16.0})
                    {
                        const Real price = intrinsic * (1.0 - ulps * QL_EPSILON) * discount;
                        BOOST_CHECK_EQUAL(liRS(type, strike, forward, price, discount, displacement), 0.0);
                        BOOST_CHECK_EQUAL(newton(type, strike, forward, price, discount, displacement), 0.0);
                    }

                    // Further below than that, there's no standard deviation to find.
                    const Real price = (intrinsic - 1.0e-12) * discount;
                    BOOST_CHECK_EXCEPTION(liRS(type, strike, forward, price, discount, displacement), Error,
                                          ExpectedErrorMessage("below its intrinsic value"));
                    BOOST_CHECK_EXCEPTION(newton(type, strike, forward, price, discount, displacement), Error,
                                          ExpectedErrorMessage("implied by put-call parity"));
                }

                if (discount == 1.0)
                {
                    BOOST_TEST_CONTEXT("strike " << strike << ", displacement " << displacement)
                    {
                        BOOST_CHECK_EQUAL(liRS(type, strike, forward, intrinsic, 1.0, displacement), 0.0);
                        BOOST_CHECK_EQUAL(newton(type, strike, forward, intrinsic, 1.0, displacement), 0.0);
                    }
                }
            }
        }
    }

    // Rounding goes with the forward and the strike, whatever the distance
    // between them: the next Real below a forward of 100 is 1.4e-14 away.
    const Real price = std::nextafter(100.0, 0.0) - 99.99;
    BOOST_CHECK_EQUAL(liRS(Option::Call, 99.99, 100.0, price, 1.0, 0.0), 0.0);
    BOOST_CHECK_EQUAL(newton(Option::Call, 99.99, 100.0, price, 1.0, 0.0), 0.0);

    // With a displacement, it goes with the sums, whatever the forward and the strike are themselves.
    const Real displaced = 0.001 - 1.0e-12;
    BOOST_CHECK_EQUAL(liRS(Option::Put, 0.002, 0.001, displaced, 1.0, 1000.0), 0.0);
    BOOST_CHECK_EQUAL(newton(Option::Put, 0.002, 0.001, displaced, 1.0, 1000.0), 0.0);

    // With a discount, it goes down with the price.
    BOOST_CHECK_EXCEPTION(newton(Option::Call, 0.5, 1.0, 0.005 - 1.0e-15, 0.01, 0.0), Error,
                          ExpectedErrorMessage("implied by put-call parity"));

    // No price at all, out of the money or on it.
    for (Real strike : {1.5, 1.0, 0.5})
    {
        const Option::Type type = strike >= forward ? Option::Call : Option::Put;
        BOOST_CHECK_EQUAL(liRS(type, strike, forward, 0.0, 1.0, 0.0), 0.0);
    }

    // In the money at low volatility, rounding puts the price on either side of its intrinsic value.
    for (Real ratio : {1.0001, 1.01, 1.5, 2.0, 5.0, 10.0})
    {
        for (Real stdDev : {1.0e-8, 1.0e-7, 1.0e-4, 0.01})
        {
            for (Real discount : {1.0, 0.95})
            {
                for (Real displacement : {0.0, 0.5})
                {
                    const Real low = (forward + displacement) / ratio - displacement;
                    const Real high = (forward + displacement) * ratio - displacement;
                    BOOST_TEST_CONTEXT("ratio " << ratio << ", stdDev " << stdDev << ", discount " << discount
                                                << ", displacement " << displacement)
                    {
                        const Real call = blackFormula(Option::Call, low, forward, stdDev, discount, displacement);
                        BOOST_CHECK_GE(liRS(Option::Call, low, forward, call, discount, displacement), 0.0);
                        BOOST_CHECK_GE(newton(Option::Call, low, forward, call, discount, displacement), 0.0);
                        const Real put = blackFormula(Option::Put, high, forward, stdDev, discount, displacement);
                        BOOST_CHECK_GE(liRS(Option::Put, high, forward, put, discount, displacement), 0.0);
                        BOOST_CHECK_GE(newton(Option::Put, high, forward, put, discount, displacement), 0.0);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testImpliedStdDevLiRSAcrossMoneyness)
{
    BOOST_TEST_MESSAGE("Testing Li-RS implied standard deviation near and away from the money...");

    for (Real forward : {1.0, 100.0, 0.03})
    {
        for (Real displacement : {0.0, 0.5})
        {
            for (Real discount : {1.0, 0.95})
            {
                for (Real stdDev : {1.0e-8, 1.0e-6, 1.0e-4, 1.0e-2, 0.1, 0.5})
                {
                    for (Real distance :
                         {30.0, 10.0, 3.0, 1.0, 0.3, 1.0e-3, 0.0, -1.0e-3, -0.3, -1.0, -3.0, -10.0, -30.0})
                    {
                        const Real logMoneyness = distance * stdDev;
                        const Real strike = (forward + displacement) * std::exp(-logMoneyness) - displacement;
                        for (auto type : {Option::Call, Option::Put})
                        {
                            // Further in the money, the price doesn't say what the standard deviation is.
                            if (Integer(type) * distance > 1.0)
                                continue;

                            BOOST_TEST_CONTEXT("type " << type << ", strike " << strike << ", forward " << forward
                                                       << ", stdDev " << stdDev << ", discount " << discount
                                                       << ", displacement " << displacement)
                            {
                                const Real price = blackFormula(type, strike, forward, stdDev, discount, displacement);
                                const Real calculated =
                                    blackFormulaImpliedStdDevLiRS(type, strike, forward, price, discount, displacement,
                                                                  Null<Real>(), 1.0, 1.0e-6 * stdDev, 100);
                                BOOST_CHECK_SMALL(calculated / stdDev - 1.0, 1.0e-5);
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testImpliedStdDevLiRSGuess)
{
    BOOST_TEST_MESSAGE("Testing the first guess of the Li-RS implied standard deviation...");

    // With an accuracy nothing can miss, the result is its first guess: RS's approximation, with the
    // same displacement it would get if called on its own.
    const Real strike = 1.1, forward = 1.0, discount = 0.95, stdDev = 0.3;
    for (Real displacement : {0.0, 0.5, 2.0})
    {
        const Real price = blackFormula(Option::Call, strike, forward, stdDev, discount, displacement);
        BOOST_TEST_CONTEXT("displacement " << displacement)
        {
            BOOST_CHECK_EQUAL(
                blackFormulaImpliedStdDevLiRS(Option::Call, strike, forward, price, discount, displacement,
                                              Null<Real>(), 1.0, 10.0, 1),
                blackFormulaImpliedStdDevApproximationRS(Option::Call, strike, forward, price, discount, displacement));

            // One of the caller's own is taken, and it comes to the same place.
            BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevLiRS(Option::Call, strike, forward, price, discount,
                                                            displacement, 0.1, 1.0, 10.0, 1),
                              0.1);
            BOOST_CHECK_SMALL(blackFormulaImpliedStdDevLiRS(Option::Call, strike, forward, price, discount,
                                                            displacement, 0.1, 1.0, 1.0e-10, 100) -
                                  stdDev,
                              1.0e-8);

            // The iteration can't start from 0.
            BOOST_CHECK_EXCEPTION(
                blackFormulaImpliedStdDevLiRS(Option::Call, strike, forward, price, discount, displacement, 0.0), Error,
                ExpectedErrorMessage("must be positive"));
        }
    }

    // No finite standard deviation gives the most an option can be worth, or more, whatever the guess.
    for (Real guess : {Real(Null<Real>()), 0.5})
    {
        for (Real discount : {1.0, 0.5})
        {
            BOOST_TEST_CONTEXT("guess " << guess << ", discount " << discount)
            {
                for (Real ratio : {1.0, 1.5})
                {
                    BOOST_CHECK_EXCEPTION(
                        blackFormulaImpliedStdDevLiRS(Option::Call, 1.5, 1.0, ratio * discount, discount, 0.0, guess),
                        Error, ExpectedErrorMessage("the most the option can be worth"));
                    BOOST_CHECK_EXCEPTION(
                        blackFormulaImpliedStdDevLiRS(Option::Put, 1.0, 1.5, ratio * discount, discount, 0.0, guess),
                        Error, ExpectedErrorMessage("the most the option can be worth"));
                }
            }
        }
    }

    // At intrinsic, though, there's nothing to iterate on and it doesn't matter.
    BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevLiRS(Option::Call, 0.5, 1.0, 0.5, 1.0, 0.0, 0.0), 0.0);

    // A step below the accuracy ends the iteration on its guess, which holds its digits at prices that
    // blackFormula can't tell from 0. Below what the guess can tell from 0, the answer is 0.
    for (Real low : {1.0e-16, 1.0e-30, 1.0e-70})
    {
        const Real price = low * M_SQRT1_2 * M_1_SQRTPI;
        BOOST_CHECK_SMALL(blackFormulaImpliedStdDevLiRS(Option::Call, 1.0, 1.0, price) / low - 1.0, 1.0e-13);
    }
    BOOST_CHECK_EQUAL(blackFormulaImpliedStdDevLiRS(Option::Call, 1.0, 1.0, 1.0e-100), 0.0);
}

BOOST_AUTO_TEST_CASE(testImpliedStdDevBelowTheForward)
{
    BOOST_TEST_MESSAGE("Testing implied standard deviations at strikes below the forward...");

    // Below the forward it solves for a put, whichever type is asked for, and it should converge
    // as it does for a call above the forward: well beyond the accuracy, in a few evaluations.
    const Real forward = 1.0, accuracy = 1.0e-6;
    const Natural maxEvaluations = 10;

    for (Real strike : {0.8, 0.9, 0.99, 1.01, 1.1, 1.2})
    {
        for (Real stdDev : {0.2, 0.5, 1.0})
        {
            for (Option::Type type : {Option::Call, Option::Put})
            {
                BOOST_TEST_CONTEXT(type << ", strike " << strike << ", standard deviation " << stdDev)
                {
                    const Real price = blackFormula(type, strike, forward, stdDev);
                    const Real implied = blackFormulaImpliedStdDev(type, strike, forward, price, 1.0, 0.0, Null<Real>(),
                                                                   accuracy, maxEvaluations);
                    BOOST_CHECK_SMALL(implied - stdDev, 1.0e-9);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
