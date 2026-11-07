/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003, 2004 Ferdinando Ametrano
 Copyright (C) 2003 Neil Firth
 Copyright (C) 2003 RiskMap srl
 Copyright (C) 2007 StatPro Italia srl

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
#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/vanilla/analyticdigitalamericanengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/mcdigitalengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/shared_ptr.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <map>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DigitalOptionTests)

#undef REPORT_FAILURE
#define REPORT_FAILURE(greekName, payoff, exercise, s, q, r, today, v, expected, calculated, error, tolerance, \
                       knockin)                                                                                \
    BOOST_FAIL(exerciseTypeToString(exercise)                                                                  \
               << " " << payoff->optionType() << " option with " << payoffTypeToString(payoff) << " payoff:\n" \
               << "    spot value:       " << s << "\n"                                                        \
               << "    strike:           " << payoff->strike() << "\n"                                         \
               << "    dividend yield:   " << io::rate(q) << "\n"                                              \
               << "    risk-free rate:   " << io::rate(r) << "\n"                                              \
               << "    reference date:   " << today << "\n"                                                    \
               << "    maturity:         " << exercise->lastDate() << "\n"                                     \
               << "    volatility:       " << io::volatility(v) << "\n\n"                                      \
               << "    expected   " << greekName << ": " << expected << "\n"                                   \
               << "    calculated " << greekName << ": " << calculated << "\n"                                 \
               << "    error:            " << error << "\n"                                                    \
               << "    tolerance:        " << tolerance << "\n"                                                \
               << "    knock_in:         " << knockin);

struct DigitalOptionData
{
    Option::Type type;
    Real strike;
    Real s;       // spot
    Rate q;       // dividend
    Rate r;       // risk-free rate
    Time t;       // time to maturity
    Volatility v; // volatility
    Real result;  // expected result
    Real tol;     // tolerance
    bool knockin; // true if knock-in
};


BOOST_AUTO_TEST_CASE(testCashOrNothingEuropeanValues)
{

    BOOST_TEST_MESSAGE("Testing European cash-or-nothing digital option...");

    DigitalOptionData values[] = {// "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 88
                                  //        type, strike,  spot,    q,    r,    t,  vol,  value, tol
                                  {Option::Put, 80.00, 100.0, 0.06, 0.06, 0.75, 0.35, 2.6710, 1e-4, true}};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(value.type, value.strike, 10.0));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
        ext::shared_ptr<PricingEngine> engine(new AnalyticEuropeanEngine(stochProcess));

        VanillaOption opt(payoff, exercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, exercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAssetOrNothingEuropeanValues)
{

    BOOST_TEST_MESSAGE("Testing European asset-or-nothing digital option...");

    // "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 90
    DigitalOptionData values[] = {//        type, strike, spot,    q,    r,    t,  vol,   value, tol
                                  {Option::Put, 65.00, 70.0, 0.05, 0.07, 0.50, 0.27, 20.2069, 1e-4, true}};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new AssetOrNothingPayoff(value.type, value.strike));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
        ext::shared_ptr<PricingEngine> engine(new AnalyticEuropeanEngine(stochProcess));

        VanillaOption opt(payoff, exercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, exercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testGapEuropeanValues)
{

    BOOST_TEST_MESSAGE("Testing European gap digital option...");

    // "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 88
    DigitalOptionData values[] = {//        type, strike, spot,    q,    r,    t,  vol,   value, tol
                                  {Option::Call, 50.00, 50.0, 0.00, 0.09, 0.50, 0.20, -0.0053, 1e-4, true}};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new GapPayoff(value.type, value.strike, 57.00));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
        ext::shared_ptr<PricingEngine> engine(new AnalyticEuropeanEngine(stochProcess));

        VanillaOption opt(payoff, exercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, exercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtHitOrNothingAmericanValues)
{

    BOOST_TEST_MESSAGE("Testing American cash-(at-hit)-or-nothing "
                       "digital option...");

    DigitalOptionData values[] = {//        type, strike,   spot,    q,    r,   t,  vol,   value, tol
                                  // "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 95, case 1,2
                                  {Option::Put, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 9.7264, 1e-4, true},
                                  {Option::Call, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 11.6553, 1e-4, true},

                                  // the following cases are not taken from a reference paper or book
                                  // in the money options (guaranteed immediate payoff)
                                  {Option::Call, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 15.0000, 1e-16, true},
                                  {Option::Put, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 15.0000, 1e-16, true},
                                  // non null dividend (cross-tested with MC simulation)
                                  {Option::Put, 100.00, 105.00, 0.20, 0.10, 0.5, 0.20, 12.2715, 1e-4, true},
                                  {Option::Call, 100.00, 95.00, 0.20, 0.10, 0.5, 0.20, 8.9109, 1e-4, true},
                                  {Option::Call, 100.00, 105.00, 0.20, 0.10, 0.5, 0.20, 15.0000, 1e-16, true},
                                  {Option::Put, 100.00, 95.00, 0.20, 0.10, 0.5, 0.20, 15.0000, 1e-16, true}};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(value.type, value.strike, 15.00));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> amExercise(new AmericanExercise(today, exDate));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
        ext::shared_ptr<PricingEngine> engine(new AnalyticDigitalAmericanEngine(stochProcess));

        VanillaOption opt(payoff, amExercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, amExercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAssetAtHitOrNothingAmericanValues)
{

    BOOST_TEST_MESSAGE("Testing American asset-(at-hit)-or-nothing "
                       "digital option...");

    DigitalOptionData values[] = {//        type, strike,   spot,    q,    r,   t,  vol,   value, tol
                                  // "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 95, case 3,4
                                  {Option::Put, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 64.8426, 1e-04,
                                   true}, // Haug value is wrong here, Haug VBA code is right
                                  {Option::Call, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 77.7017, 1e-04,
                                   true}, // Haug value is wrong here, Haug VBA code is right
                                          // data from Haug VBA code results
                                  {Option::Put, 100.00, 105.00, 0.01, 0.10, 0.5, 0.20, 65.7811, 1e-04, true},
                                  {Option::Call, 100.00, 95.00, 0.01, 0.10, 0.5, 0.20, 76.8858, 1e-04, true},
                                  // in the money options  (guaranteed immediate payoff = spot)
                                  {Option::Call, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 105.0000, 1e-16, true},
                                  {Option::Put, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 95.0000, 1e-16, true},
                                  {Option::Call, 100.00, 105.00, 0.01, 0.10, 0.5, 0.20, 105.0000, 1e-16, true},
                                  {Option::Put, 100.00, 95.00, 0.01, 0.10, 0.5, 0.20, 95.0000, 1e-16, true}};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(100.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.04));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.01));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.25));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new AssetOrNothingPayoff(value.type, value.strike));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> amExercise(new AmericanExercise(today, exDate));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
        ext::shared_ptr<PricingEngine> engine(new AnalyticDigitalAmericanEngine(stochProcess));

        VanillaOption opt(payoff, amExercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, amExercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtExpiryOrNothingAmericanValues)
{

    BOOST_TEST_MESSAGE("Testing American cash-(at-expiry)-or-nothing "
                       "digital option...");

    DigitalOptionData values[] = {
        //        type, strike,   spot,    q,    r,   t,  vol,   value, tol
        // "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 95, case 5,6,9,10
        {Option::Put, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 9.3604, 1e-4, true},
        {Option::Call, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 11.2223, 1e-4, true},
        {Option::Put, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 4.9081, 1e-4, false},
        {Option::Call, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 3.0461, 1e-4, false},
        // in the money options (guaranteed discounted payoff)
        {Option::Call, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 15.0000 * std::exp(-0.05), 1e-12, true},
        {Option::Put, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 15.0000 * std::exp(-0.05), 1e-12, true},
        // out of bonds case
        {Option::Call, 2.37, 2.33, 0.07, 0.43, 0.19, 0.005, 0.0000, 1e-4, false},
    };

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(100.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.04));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.01));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.25));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(value.type, value.strike, 15.0));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> amExercise(new AmericanExercise(today, exDate, true));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
        ext::shared_ptr<PricingEngine> engine;
        if (value.knockin)
            engine = ext::make_shared<AnalyticDigitalAmericanEngine>(stochProcess);
        else
            engine = ext::make_shared<AnalyticDigitalAmericanKOEngine>(stochProcess);

        VanillaOption opt(payoff, amExercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, amExercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAssetAtExpiryOrNothingAmericanValues)
{

    BOOST_TEST_MESSAGE("Testing American asset-(at-expiry)-or-nothing "
                       "digital option...");

    DigitalOptionData values[] = {
        //        type, strike,   spot,    q,    r,   t,  vol,   value, tol
        // "Option pricing formulas", E.G. Haug, McGraw-Hill 1998 - pag 95, case 7,8,11,12
        {Option::Put, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 64.8426, 1e-04, true},
        {Option::Call, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 77.7017, 1e-04, true},
        {Option::Put, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 40.1574, 1e-04, false},
        {Option::Call, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 17.2983, 1e-04, false},
        // data from Haug VBA code results
        {Option::Put, 100.00, 105.00, 0.01, 0.10, 0.5, 0.20, 65.5291, 1e-04, true},
        {Option::Call, 100.00, 95.00, 0.01, 0.10, 0.5, 0.20, 76.5951, 1e-04, true},
        // in the money options (guaranteed discounted payoff = forward * riskFreeDiscount
        //                                                    = spot * dividendDiscount)
        {Option::Call, 100.00, 105.00, 0.00, 0.10, 0.5, 0.20, 105.0000, 1e-12, true},
        {Option::Put, 100.00, 95.00, 0.00, 0.10, 0.5, 0.20, 95.0000, 1e-12, true},
        {Option::Call, 100.00, 105.00, 0.01, 0.10, 0.5, 0.20, 105.0000 * std::exp(-0.005), 1e-12, true},
        {Option::Put, 100.00, 95.00, 0.01, 0.10, 0.5, 0.20, 95.0000 * std::exp(-0.005), 1e-12, true}};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(100.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.04));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.01));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.25));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new AssetOrNothingPayoff(value.type, value.strike));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> amExercise(new AmericanExercise(today, exDate, true));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
        ext::shared_ptr<PricingEngine> engine;
        if (value.knockin)
            engine = ext::make_shared<AnalyticDigitalAmericanEngine>(stochProcess);
        else
            engine = ext::make_shared<AnalyticDigitalAmericanKOEngine>(stochProcess);

        VanillaOption opt(payoff, amExercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, amExercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtHitOrNothingAmericanGreeks)
{

    BOOST_TEST_MESSAGE("Testing American cash-(at-hit)-or-nothing "
                       "digital option greeks...");

    std::map<std::string, Real> calculated, expected, tolerance;
    tolerance["delta"] = 5.0e-5;
    tolerance["gamma"] = 5.0e-5;
    // tolerance["theta"]  = 5.0e-5;
    tolerance["rho"] = 5.0e-5;
    // tolerance["divRho"] = 5.0e-5;
    // tolerance["vega"]   = 5.0e-5;

    Option::Type types[] = {Option::Call, Option::Put};
    Real strikes[] = {50.0, 99.5, 100.5, 150.0};
    Real cashPayoff = 100.0;
    Real underlyings[] = {100};
    Rate qRates[] = {0.04, 0.05, 0.06};
    Rate rRates[] = {0.01, 0.05, 0.15};
    Volatility vols[] = {0.11, 0.5, 1.2};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();
    Settings::instance().evaluationDate() = today;

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> qTS(flatRate(qRate, dc));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> rTS(flatRate(rRate, dc));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    Handle<BlackVolTermStructure> volTS(flatVol(vol, dc));

    // there is no cycling on different residual times
    Date exDate = today + 360;
    ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));
    ext::shared_ptr<Exercise> amExercise(new AmericanExercise(today, exDate, false));
    ext::shared_ptr<Exercise> exercises[] = {exercise, amExercise};

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
        new BlackScholesMertonProcess(Handle<Quote>(spot), qTS, rTS, volTS));

    ext::shared_ptr<PricingEngine> euroEngine(new AnalyticEuropeanEngine(stochProcess));

    ext::shared_ptr<PricingEngine> amEngine(new AnalyticDigitalAmericanEngine(stochProcess));

    ext::shared_ptr<PricingEngine> engines[] = {euroEngine, amEngine};

    bool knockin = true;
    for (Size j = 0; j < std::size(engines); j++)
    {
        for (auto& type : types)
        {
            for (Real strike : strikes)
            {
                ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(type, strike, cashPayoff));

                VanillaOption opt(payoff, exercises[j]);
                opt.setPricingEngine(engines[j]);

                for (Real u : underlyings)
                {
                    for (Real q : qRates)
                    {
                        for (Real r : rRates)
                        {
                            for (Real v : vols)
                            {
                                // test data
                                spot->setValue(u);
                                qRate->setValue(q);
                                rRate->setValue(r);
                                vol->setValue(v);

                                // theta, dividend rho and vega are not available for
                                // digital option with american exercise. Greeks of
                                // digital options with european payoff are tested
                                // in the europeanoption.cpp test
                                Real value = opt.NPV();
                                calculated["delta"] = opt.delta();
                                calculated["gamma"] = opt.gamma();
                                // calculated["theta"]  = opt.theta();
                                calculated["rho"] = opt.rho();
                                // calculated["divRho"] = opt.dividendRho();
                                // calculated["vega"]   = opt.vega();

                                if (value > 1.0e-6)
                                {
                                    // perturb spot and get delta and gamma
                                    Real du = u * 1.0e-4;
                                    spot->setValue(u + du);
                                    Real value_p = opt.NPV(), delta_p = opt.delta();
                                    spot->setValue(u - du);
                                    Real value_m = opt.NPV(), delta_m = opt.delta();
                                    spot->setValue(u);
                                    expected["delta"] = (value_p - value_m) / (2 * du);
                                    expected["gamma"] = (delta_p - delta_m) / (2 * du);

                                    // perturb rates and get rho and dividend rho
                                    Spread dr = r * 1.0e-4;
                                    rRate->setValue(r + dr);
                                    value_p = opt.NPV();
                                    rRate->setValue(r - dr);
                                    value_m = opt.NPV();
                                    rRate->setValue(r);
                                    expected["rho"] = (value_p - value_m) / (2 * dr);

                                    /*
                                    Spread dq = q*1.0e-4;
                                    qRate->setValue(q+dq);
                                    value_p = opt.NPV();
                                    qRate->setValue(q-dq);
                                    value_m = opt.NPV();
                                    qRate->setValue(q);
                                    expected["divRho"] = (value_p - value_m)/(2*dq);

                                    // perturb volatility and get vega
                                    Volatility dv = v*1.0e-4;
                                    vol->setValue(v+dv);
                                    value_p = opt.NPV();
                                    vol->setValue(v-dv);
                                    value_m = opt.NPV();
                                    vol->setValue(v);
                                    expected["vega"] = (value_p - value_m)/(2*dv);

                                    // perturb date and get theta
                                    Time dT = dc.yearFraction(today-1, today+1);
                                    Settings::instance().setEvaluationDate(today-1);
                                    value_m = opt.NPV();
                                    Settings::instance().setEvaluationDate(today+1);
                                    value_p = opt.NPV();
                                    Settings::instance().setEvaluationDate(today);
                                    expected["theta"] = (value_p - value_m)/dT;
                                    */

                                    // check
                                    std::map<std::string, Real>::iterator it;
                                    for (it = calculated.begin(); it != calculated.end(); ++it)
                                    {
                                        std::string greek = it->first;
                                        Real expct = expected[greek], calcl = calculated[greek], tol = tolerance[greek];
                                        Real error = relativeError(expct, calcl, value);
                                        if (error > tol)
                                        {
                                            REPORT_FAILURE(greek, payoff, exercise, u, q, r, today, v, expct, calcl,
                                                           error, tol, knockin);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testMCCashAtHit)
{

    BOOST_TEST_MESSAGE("Testing Monte Carlo cash-(at-hit)-or-nothing "
                       "American engine...");

    DigitalOptionData values[] = {//        type, strike,   spot,    q,    r,   t,  vol,   value, tol
                                  {Option::Put, 100.00, 105.00, 0.20, 0.10, 0.5, 0.20, 12.2715, 1e-2, true},
                                  {Option::Call, 100.00, 95.00, 0.20, 0.10, 0.5, 0.20, 8.9109, 1e-2, true}};

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    Size timeStepsPerYear = 90;
    Size maxSamples = 1000000;
    BigNatural seed = 1;

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(value.type, value.strike, 15.0));
        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> amExercise(new AmericanExercise(today, exDate));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));

        Size requiredSamples = Size(std::pow(2.0, 14) - 1);
        ext::shared_ptr<PricingEngine> mcldEngine = MakeMCDigitalEngine<LowDiscrepancy>(stochProcess)
                                                        .withStepsPerYear(timeStepsPerYear)
                                                        .withBrownianBridge()
                                                        .withSamples(requiredSamples)
                                                        .withMaxSamples(maxSamples)
                                                        .withSeed(seed);

        VanillaOption opt(payoff, amExercise);
        opt.setPricingEngine(mcldEngine);

        Real calculated = opt.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, amExercise, value.s, value.q, value.r, today, value.v, value.result,
                           calculated, error, value.tol, value.knockin);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtHitOrNothingAmericanLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing American cash-(at-hit)-or-nothing digital option at low volatility...");

    // At low volatility a power of H/S can overflow while the probability it multiplies
    // underflows, and their product is finite.  The expected values are the closed form in
    // 200-digit arithmetic.
    const DigitalOptionData values[] = {
        {Option::Call, 105.0, 100.0, 0.03, 0.08, 1.0, 0.01, 5.4765152014571274, 1e-10, true},
        {Option::Call, 105.0, 100.0, 0.03, 0.08, 1.0, 0.003, 6.1874883269953051, 1e-10, true},
        {Option::Call, 103.0454533953517, 100.0, 0.0, 0.03, 1.0, 0.001, 4.9186702646932065, 1e-10, true},
        {Option::Put, 99.0, 100.0, 0.03, -0.02, 1.0, 0.0001, 10.040282256787277, 1e-10, true},
        // The forward on the barrier: here the power is still finite, at about exp(700), and N(y) is already 0.
        {Option::Call, 105.12710963760242, 100.0, 0.0, 0.05, 1.0, 0.00267, 4.8624601470182828, 1e-10, true},
        {Option::Put, 95.1229424500714, 100.0, 0.0, -0.05, 1.0, 0.00267, 5.3626596277116115, 1e-10, true},
        // Lower still, where m + lambda or m - lambda would cancel.
        {Option::Call, 105.12710963760242, 100.0, 0.0, 0.05, 1.0, 1e-5, 4.7565455823743784, 1e-9, true},
        {Option::Put, 95.1229424500714, 100.0, 0.0, -0.05, 1.0, 1e-6, 5.2563953247838838, 1e-8, true},
        {Option::Call, 103.0, 100.0, 0.0, 0.05, 1.0, 1e-6, 9.7087378640776699, 1e-10, true},
        {Option::Put, 97.0, 100.0, 0.08, 0.03, 1.0, 1e-5, 9.8189046024500073, 1e-10, true},
    };

    const DayCounter dc = Actual360();
    const Date today(15, October, 2026);
    Settings::instance().evaluationDate() = today;

    const auto spot = ext::make_shared<SimpleQuote>(0.0);
    const auto qRate = ext::make_shared<SimpleQuote>(0.0);
    const auto rRate = ext::make_shared<SimpleQuote>(0.0);
    const auto vol = ext::make_shared<SimpleQuote>(0.0);
    const auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(spot), Handle<YieldTermStructure>(flatRate(today, qRate, dc)),
        Handle<YieldTermStructure>(flatRate(today, rRate, dc)), Handle<BlackVolTermStructure>(flatVol(today, vol, dc)));
    const auto engine = ext::make_shared<AnalyticDigitalAmericanEngine>(process);

    for (const auto& value : values)
    {
        BOOST_TEST_CONTEXT(value.type << " at " << value.strike << ", r " << value.r << ", q " << value.q
                                      << ", volatility " << value.v)
        {
            spot->setValue(value.s);
            qRate->setValue(value.q);
            rRate->setValue(value.r);
            vol->setValue(value.v);

            VanillaOption option(ext::make_shared<CashOrNothingPayoff>(value.type, value.strike, 10.0),
                                 ext::make_shared<AmericanExercise>(today, today + timeToDays(value.t)));
            option.setPricingEngine(engine);
            BOOST_CHECK_SMALL(option.NPV() - value.result, value.tol);

            // The bumps stay small against a standard deviation.
            const Real ds = std::min(1.0e-4, 1.0e-2 * value.s * value.v);
            const Rate dr = std::min(1.0e-7, 1.0e-2 * value.v);
            spot->setValue(value.s + ds);
            Real up = option.NPV();
            spot->setValue(value.s - ds);
            Real down = option.NPV();
            spot->setValue(value.s);
            const Real expectedDelta = (up - down) / (2.0 * ds);
            rRate->setValue(value.r + dr);
            up = option.NPV();
            rRate->setValue(value.r - dr);
            down = option.NPV();
            rRate->setValue(value.r);
            const Real expectedRho = (up - down) / (2.0 * dr);

            BOOST_CHECK_SMALL((option.delta() - expectedDelta) / std::max(1.0, std::fabs(expectedDelta)), 1.0e-4);
            BOOST_CHECK_SMALL((option.rho() - expectedRho) / std::max(1.0, std::fabs(expectedRho)), 1.0e-4);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtHitOrNothingAmericanZeroVolatility)
{
    BOOST_TEST_MESSAGE("Testing American cash-(at-hit)-or-nothing digital option at zero volatility...");

    const DayCounter dc = Actual365Fixed();
    const Date today(15, October, 2026);
    Settings::instance().evaluationDate() = today;
    const ext::shared_ptr<Exercise> exercise = ext::make_shared<AmericanExercise>(today, today + 365);

    // The underlying follows its forward, and the payoff comes when the forward reaches the
    // strike, if it does before maturity.  An asset option pays the strike then.
    const struct
    {
        Option::Type type;
        Real strike;
        Rate r, q;
        Real cashValue, assetValue;
    } cases[] = {
        {Option::Call, 103.0, 0.05, 0.0, 10.0 / 1.03, 100.0},
        {Option::Call, 110.0, 0.05, 0.0, 0.0, 0.0},
        {Option::Put, 96.0, -0.05, 0.0, 10.0 / 0.96, 100.0},
        {Option::Put, 95.0, -0.05, 0.0, 0.0, 0.0},
        {Option::Call, 101.0, 0.03, 0.01, 10.0 * std::exp(-0.03 * std::log(1.01) / 0.02),
         101.0 * std::exp(-0.03 * std::log(1.01) / 0.02)},
        {Option::Call, 101.0, 0.03, 0.03, 0.0, 0.0},
        {Option::Call, 101.0, 0.01, 0.03, 0.0, 0.0},
        {Option::Call, 95.0, 0.05, 0.0, 10.0, 100.0},
        {Option::Put, 105.0, 0.05, 0.0, 10.0, 100.0},
        {Option::Call, 100.0, 0.05, 0.0, 10.0, 100.0},
    };

    // Below the threshold where the formula stops dividing by the variance, and above it.
    for (Volatility vol : {0.0, 1.0e-9, 1.0e-6})
    {
        for (const auto& c : cases)
        {
            BOOST_TEST_CONTEXT(c.type << " at " << c.strike << ", r " << c.r << ", q " << c.q << ", volatility " << vol)
            {
                const auto process =
                    ext::make_shared<BlackScholesMertonProcess>(Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)),
                                                                Handle<YieldTermStructure>(flatRate(today, c.q, dc)),
                                                                Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
                                                                Handle<BlackVolTermStructure>(flatVol(today, vol, dc)));
                const auto engine = ext::make_shared<AnalyticDigitalAmericanEngine>(process);

                VanillaOption cash(ext::make_shared<CashOrNothingPayoff>(c.type, c.strike, 10.0), exercise);
                cash.setPricingEngine(engine);
                BOOST_CHECK_SMALL(cash.NPV() - c.cashValue, 1.0e-8);

                VanillaOption asset(ext::make_shared<AssetOrNothingPayoff>(c.type, c.strike), exercise);
                asset.setPricingEngine(engine);
                BOOST_CHECK_SMALL(asset.NPV() - c.assetValue, 1.0e-8);

                if (vol == 0.0)
                    BOOST_CHECK_EXCEPTION(cash.delta(), Error, ExpectedErrorMessage("delta not provided"));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtHitOrNothingAmericanNegativeRates)
{
    BOOST_TEST_MESSAGE("Testing American cash-(at-hit)-or-nothing digital option with negative rates...");

    // With mu = (r - q) / vol^2 - 1/2, a negative rate can make mu^2 + 2 r / vol^2 negative.  The
    // closed form would then take N at complex arguments, and the engine integrates the
    // first-passage density instead.  The expected values are that integral in 50-digit
    // arithmetic, with the Greeks differentiated under it.  At the strike the payoff is due now,
    // and the expected Greeks are the closed form's there, at either sign.  The last two rows put
    // mu^2 + 2 r / vol^2 at -1e-6 and 1e-6, either side of the switch.
    const struct
    {
        Option::Type type;
        Real strike;
        Rate r, q;
        Volatility vol;
        Time t;
        Real value, delta, gamma, rho;
    } cases[] = {
        {Option::Call, 110.0, -0.03, -0.03, 0.2, 1.0, 6.0888174705786616, 0.36868328467983525, 0.0073820534493988355,
         13.564291632072978},
        {Option::Call, 110.0, -0.01, -0.01, 0.2, 1.0, 6.0512526067329478, 0.36954412230552514, 0.0078285539880438774,
         13.484251345244579},
        {Option::Call, 110.0, -0.03, -0.08, 0.2, 1.0, 6.8459352891004656, 0.33006223118685099, 8.279142856857146e-6,
         12.602391508135078},
        {Option::Call, 110.0, -0.03, -0.03, 0.1, 1.0, 3.2949012374576558, 0.50344536032244349, 0.045404881692003936,
         30.561551835878825},
        {Option::Put, 90.0, -0.03, -0.03, 0.2, 1.0, 6.359958956891341, -0.33337822340373149, 0.0089315634914966164,
         -17.819098918427761},
        {Option::Put, 90.0, -0.02, -0.01, 0.15, 5.0, 8.413964846973719, -0.16588546923881427, -0.00050818885758786037,
         -39.915407565552525},
        {Option::Call, 150.0, -0.01, -0.01, 0.05, 10.0, 0.091316967371345233, 0.017045076739810933,
         0.0026750491112480538, 14.438203296145558},
        {Option::Call, 100.01, -0.03, -0.03, 0.2, 1.0, 9.9956110944925355, 0.43889801397024786, -0.0014891166491499625,
         0.022977769652113631},
        {Option::Put, 99.99, -0.03, -0.03, 0.2, 1.0, 9.9966103166261839, -0.3389758020946481, -0.0014892644872627408,
         -0.027003186084196699},
        {Option::Call, 100.0, -0.03, -0.03, 0.2, 1.0, 10.0, 0.05, -0.0015, 0.0},
        {Option::Call, 110.0, -0.03, -0.03, 0.4898969687636779, 1.0, 8.0638536489070991, 0.19269642054361134,
         0.00040353429527062846, 2.6288315116348002},
        {Option::Call, 110.0, -0.03, -0.03, 0.4898989283554721, 1.0, 8.0638591828325502, 0.19269588297479171,
         0.00040352845454633638, 2.6288098304759226},
    };

    const DayCounter dc = Actual360();
    const Date today(15, October, 2026);
    Settings::instance().evaluationDate() = today;

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.type << " at " << c.strike << ", r " << c.r << ", q " << c.q << ", volatility " << c.vol
                                  << ", " << c.t << " years")
        {
            const auto process =
                ext::make_shared<BlackScholesMertonProcess>(Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)),
                                                            Handle<YieldTermStructure>(flatRate(today, c.q, dc)),
                                                            Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
                                                            Handle<BlackVolTermStructure>(flatVol(today, c.vol, dc)));
            VanillaOption option(ext::make_shared<CashOrNothingPayoff>(c.type, c.strike, 10.0),
                                 ext::make_shared<AmericanExercise>(today, today + timeToDays(c.t)));
            option.setPricingEngine(ext::make_shared<AnalyticDigitalAmericanEngine>(process));

            // The error is relative, but absolute below 1.  In the last row the closed form's rho
            // loses digits to cancellation as lambda goes to 0, and is good to about 3e-13.
            const auto error = [](Real calculated, Real expected)
            { return (calculated - expected) / std::max(std::fabs(expected), Real(1.0)); };
            BOOST_CHECK_SMALL(error(option.NPV(), c.value), 1.0e-12);
            BOOST_CHECK_SMALL(error(option.delta(), c.delta), 1.0e-12);
            BOOST_CHECK_SMALL(error(option.gamma(), c.gamma), 1.0e-12);
            BOOST_CHECK_SMALL(error(option.rho(), c.rho), 1.0e-11);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtExpiryOrNothingAmericanLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing American cash-(at-expiry)-or-nothing digital option at low volatility...");

    // The expected values are the closed form in 200-digit arithmetic.
    const DigitalOptionData values[] = {
        {Option::Call, 105.0, 100.0, 0.03, 0.08, 1.0, 0.001, 8.2035184454191516, 1e-10, true},
        {Option::Put, 95.0, 100.0, 0.03, -0.02, 1.0, 0.001, 1.0176365525852889, 1e-10, true},
        {Option::Call, 115.0, 100.0, 0.0, 0.03, 5.0, 0.003, 8.0820190229249983, 1e-10, true},
        {Option::Put, 85.0, 100.0, 0.03, 0.0, 5.0, 0.003, 0.3275072301893173, 1e-10, true},
        // The forward on the barrier: here the power is still finite, at about exp(700), and N(y) is already 0.
        {Option::Call, 105.12710963760242, 100.0, 0.0, 0.05, 1.0, 0.00267, 4.8523350913440141, 1e-10, true},
        {Option::Put, 95.1229424500714, 100.0, 0.0, -0.05, 1.0, 0.00267, 5.3738495447867109, 1e-10, true},
        {Option::Call, 105.12710963760242, 100.0, 0.0, 0.05, 1.0, 1e-5, 4.7565076338111791, 1e-9, true},
        {Option::Put, 95.1229424500714, 100.0, 0.0, -0.05, 1.0, 1e-6, 5.2563995187487688, 1e-8, true},
    };

    const DayCounter dc = Actual360();
    const Date today(15, October, 2026);
    Settings::instance().evaluationDate() = today;

    for (const auto& value : values)
    {
        BOOST_TEST_CONTEXT(value.type << " at " << value.strike << ", r " << value.r << ", q " << value.q
                                      << ", volatility " << value.v)
        {
            const auto process =
                ext::make_shared<BlackScholesMertonProcess>(Handle<Quote>(ext::make_shared<SimpleQuote>(value.s)),
                                                            Handle<YieldTermStructure>(flatRate(today, value.q, dc)),
                                                            Handle<YieldTermStructure>(flatRate(today, value.r, dc)),
                                                            Handle<BlackVolTermStructure>(flatVol(today, value.v, dc)));
            const auto payoff = ext::make_shared<CashOrNothingPayoff>(value.type, value.strike, 10.0);
            const Date exDate = today + timeToDays(value.t);
            const ext::shared_ptr<Exercise> exercise = ext::make_shared<AmericanExercise>(today, exDate, true);

            VanillaOption knockIn(payoff, exercise);
            knockIn.setPricingEngine(ext::make_shared<AnalyticDigitalAmericanEngine>(process));
            BOOST_CHECK_SMALL(knockIn.NPV() - value.result, value.tol);

            // A knock-in and a knock-out add up to the discounted cash.
            VanillaOption knockOut(payoff, exercise);
            knockOut.setPricingEngine(ext::make_shared<AnalyticDigitalAmericanKOEngine>(process));
            const Real cash = 10.0 * process->riskFreeRate()->discount(exDate);
            BOOST_CHECK_SMALL(knockIn.NPV() + knockOut.NPV() - cash, value.tol);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCashAtExpiryOrNothingAmericanZeroVolatility)
{
    BOOST_TEST_MESSAGE("Testing American cash-(at-expiry)-or-nothing digital option at zero volatility...");

    const DayCounter dc = Actual365Fixed();
    const Date today(15, October, 2026);
    Settings::instance().evaluationDate() = today;
    const ext::shared_ptr<Exercise> exercise = ext::make_shared<AmericanExercise>(today, today + 365, true);

    // The underlying follows its forward, which is 105.13 at a rate of 5% and 95.12 at -5%.
    // A knock-in pays if the forward reaches the strike, and a knock-out pays if it doesn't.
    // A cash option that pays is worth 10 discounted, and an asset option the spot.
    const struct
    {
        Option::Type type;
        Real strike;
        Rate r;
        bool reached;
    } cases[] = {
        {Option::Call, 103.0, 0.05, true}, {Option::Call, 110.0, 0.05, false}, {Option::Put, 95.0, 0.05, false},
        {Option::Put, 96.0, -0.05, true},  {Option::Put, 94.0, -0.05, false},  {Option::Call, 103.0, -0.05, false},
        {Option::Call, 95.0, 0.05, true},  {Option::Put, 105.0, 0.05, true},   {Option::Call, 100.0, 0.05, true},
        {Option::Put, 100.0, 0.05, true},
    };

    // Below the threshold where the formula stops dividing by the variance, and above it.
    for (Volatility vol : {0.0, 1.0e-9, 1.0e-6})
    {
        for (const auto& c : cases)
        {
            BOOST_TEST_CONTEXT(c.type << " at " << c.strike << ", r " << c.r << ", volatility " << vol)
            {
                const auto process =
                    ext::make_shared<BlackScholesMertonProcess>(Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)),
                                                                Handle<YieldTermStructure>(flatRate(today, 0.0, dc)),
                                                                Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
                                                                Handle<BlackVolTermStructure>(flatVol(today, vol, dc)));
                const auto knockInEngine = ext::make_shared<AnalyticDigitalAmericanEngine>(process);
                const auto knockOutEngine = ext::make_shared<AnalyticDigitalAmericanKOEngine>(process);
                const Real cashValue = 10.0 * std::exp(-c.r);

                VanillaOption cash(ext::make_shared<CashOrNothingPayoff>(c.type, c.strike, 10.0), exercise);
                cash.setPricingEngine(knockInEngine);
                BOOST_CHECK_SMALL(cash.NPV() - (c.reached ? cashValue : 0.0), 1.0e-10);
                cash.setPricingEngine(knockOutEngine);
                BOOST_CHECK_SMALL(cash.NPV() - (c.reached ? 0.0 : cashValue), 1.0e-10);

                VanillaOption asset(ext::make_shared<AssetOrNothingPayoff>(c.type, c.strike), exercise);
                asset.setPricingEngine(knockInEngine);
                BOOST_CHECK_SMALL(asset.NPV() - (c.reached ? 100.0 : 0.0), 1.0e-10);
                asset.setPricingEngine(knockOutEngine);
                BOOST_CHECK_SMALL(asset.NPV() - (c.reached ? 0.0 : 100.0), 1.0e-10);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
