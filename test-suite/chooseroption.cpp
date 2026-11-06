/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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
#include <ql/instruments/complexchooseroption.hpp>
#include <ql/instruments/simplechooseroption.hpp>
#include <ql/pricingengines/exotic/analyticcomplexchooserengine.hpp>
#include <ql/pricingengines/exotic/analyticsimplechooserengine.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <algorithm>
#include <cmath>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ChooserOptionTests)

#undef REPORT_FAILURE
#define REPORT_FAILURE(greekName, choosingDate, exercise, s, q, r, today, v, expected, calculated, tolerance)          \
    BOOST_ERROR(exerciseTypeToString(exercise) << " Chooser option with "                                              \
                                               << "    spot value: " << s << "\n"                                      \
                                               << "    dividend yield:   " << io::rate(q) << "\n"                      \
                                               << "    risk-free rate:   " << io::rate(r) << "\n"                      \
                                               << "    reference date:   " << today << "\n"                            \
                                               << "    maturity:         " << exercise->lastDate() << "\n"             \
                                               << "    volatility:       " << io::volatility(v) << "\n\n"              \
                                               << "    expected   " << greekName << ": " << expected << "\n"           \
                                               << "    calculated " << greekName << ": " << calculated << "\n"         \
                                               << "    error:            " << std::fabs(expected - calculated) << "\n" \
                                               << "    tolerance:        " << tolerance);

BOOST_AUTO_TEST_CASE(testAnalyticSimpleChooserEngine)
{

    BOOST_TEST_MESSAGE("Testing analytic simple chooser option...");

    /* The data below are from
       "Complete Guide to Option Pricing Formulas", Espen Gaarder Haug
       pages 39-40
    */
    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot = ext::make_shared<SimpleQuote>(50.0);
    ext::shared_ptr<SimpleQuote> qRate = ext::make_shared<SimpleQuote>(0.0);
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate = ext::make_shared<SimpleQuote>(0.08);
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol = ext::make_shared<SimpleQuote>(0.25);
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(spot), Handle<YieldTermStructure>(qTS), Handle<YieldTermStructure>(rTS),
        Handle<BlackVolTermStructure>(volTS));

    ext::shared_ptr<PricingEngine> engine = ext::make_shared<AnalyticSimpleChooserEngine>(stochProcess);

    Real strike = 50.0;

    Date exerciseDate = today + 180;
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(exerciseDate);

    Date choosingDate = today + 90;
    SimpleChooserOption option(choosingDate, strike, exercise);
    option.setPricingEngine(engine);

    Real calculated = option.NPV();
    Real expected = 6.1071;
    Real tolerance = 3e-5;
    if (std::fabs(calculated - expected) > tolerance)
    {
        REPORT_FAILURE("value", choosingDate, exercise, spot->value(), qRate->value(), rRate->value(), today,
                       vol->value(), expected, calculated, tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testAnalyticComplexChooserEngine)
{
    BOOST_TEST_MESSAGE("Testing analytic complex chooser option...");

    /* The example below is from
       "Complete Guide to Option Pricing Formulas", Espen Gaarder Haug
    */
    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot = ext::make_shared<SimpleQuote>(50.0);
    ext::shared_ptr<SimpleQuote> qRate = ext::make_shared<SimpleQuote>(0.05);
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate = ext::make_shared<SimpleQuote>(0.10);
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol = ext::make_shared<SimpleQuote>(0.35);
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(spot), Handle<YieldTermStructure>(qTS), Handle<YieldTermStructure>(rTS),
        Handle<BlackVolTermStructure>(volTS));

    ext::shared_ptr<PricingEngine> engine = ext::make_shared<AnalyticComplexChooserEngine>(stochProcess);

    Real callStrike = 55.0;
    Real putStrike = 48.0;

    Date choosingDate = today + 90;
    Date callExerciseDate = today + 180;
    Date putExerciseDate = today + 210;
    ext::shared_ptr<Exercise> callExercise = ext::make_shared<EuropeanExercise>(callExerciseDate);
    ext::shared_ptr<Exercise> putExercise = ext::make_shared<EuropeanExercise>(putExerciseDate);

    ComplexChooserOption option(choosingDate, callStrike, putStrike, callExercise, putExercise);
    option.setPricingEngine(engine);

    Real calculated = option.NPV();
    Real expected = 6.0508;
    Real error = std::fabs(calculated - expected);
    Real tolerance = 1e-4;
    if (error > tolerance)
    {
        BOOST_ERROR("Failed to reproduce complex chooser option value" << "\n    expected:   " << expected
                                                                       << "\n    calculated: " << calculated
                                                                       << "\n    error:      " << error);
    }
}

namespace
{
    // Days on Actual360, so that a table can state its exact times.
    Real complexChooserValue(Real spot,
                             Real callStrike,
                             Real putStrike,
                             Rate r,
                             Rate q,
                             Volatility v,
                             Integer choosingDays,
                             Integer callDays,
                             Integer putDays)
    {
        DayCounter dc = Actual360();
        Date today = Settings::instance().evaluationDate();
        auto process = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(spot)), Handle<YieldTermStructure>(flatRate(today, q, dc)),
            Handle<YieldTermStructure>(flatRate(today, r, dc)), Handle<BlackVolTermStructure>(flatVol(today, v, dc)));
        ComplexChooserOption option(today + choosingDays, callStrike, putStrike,
                                    ext::make_shared<EuropeanExercise>(today + callDays),
                                    ext::make_shared<EuropeanExercise>(today + putDays));
        option.setPricingEngine(ext::make_shared<AnalyticComplexChooserEngine>(process));
        return option.NPV();
    }
}

BOOST_AUTO_TEST_CASE(testComplexChooserWithEqualTermsMatchesSimpleChooser)
{
    BOOST_TEST_MESSAGE("Testing complex chooser with equal strikes and maturities against simple chooser...");

    DayCounter dc = Actual360();
    Date today(20, October, 2026);
    Settings::instance().evaluationDate() = today;

    Real strike = 100.0;

    for (Real s : {80.0, 100.0, 120.0})
    {
        for (Rate q : {0.0, 0.03})
        {
            for (Rate r : {0.01, 0.06})
            {
                for (Volatility v : {0.15, 0.40})
                {
                    for (Integer choosingDays : {30, 90})
                    {
                        for (Integer exerciseDays : {91, 180, 365})
                        {
                            auto process = ext::make_shared<BlackScholesMertonProcess>(
                                Handle<Quote>(ext::make_shared<SimpleQuote>(s)),
                                Handle<YieldTermStructure>(flatRate(today, q, dc)),
                                Handle<YieldTermStructure>(flatRate(today, r, dc)),
                                Handle<BlackVolTermStructure>(flatVol(today, v, dc)));
                            SimpleChooserOption simple(today + choosingDays, strike,
                                                       ext::make_shared<EuropeanExercise>(today + exerciseDays));
                            simple.setPricingEngine(ext::make_shared<AnalyticSimpleChooserEngine>(process));

                            BOOST_TEST_CONTEXT("spot " << s << ", q " << q << ", r " << r << ", vol " << v
                                                       << ", choosing in " << choosingDays << " days, exercise in "
                                                       << exerciseDays)
                            {
                                BOOST_CHECK_SMALL(complexChooserValue(s, strike, strike, r, q, v, choosingDays,
                                                                      exerciseDays, exerciseDays) -
                                                      simple.NPV(),
                                                  1e-11);
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testComplexChooserAgainstItsDefinition)
{
    BOOST_TEST_MESSAGE("Testing complex chooser against its definition...");

    Settings::instance().evaluationDate() = Date(20, October, 2026);

    // The better of a call and a put at the choosing date, integrated over the spot
    // to 30 digits.  The first row is the example in Haug's book.
    struct Case
    {
        Real spot, callStrike, putStrike;
        Rate r, q;
        Volatility v;
        Integer choosingDays, callDays, putDays;
        Real expected;
    };
    Case cases[] = {
        {50.0, 55.0, 48.0, 0.1, 0.05, 0.35, 90, 180, 210, 6.0507712394946891},
        {120.0, 110.0, 90.0, 0.06, 0.0, 0.15, 30, 365, 180, 17.949410518682577},
        {80.0, 110.0, 90.0, 0.06, 0.0, 0.15, 90, 91, 91, 8.8990079577215627},
        {120.0, 100.0, 100.0, 0.06, 0.0, 0.4, 30, 365, 180, 33.039525076229599},
        {100.0, 110.0, 90.0, 0.01, 0.03, 0.4, 90, 180, 210, 12.844563043249066},
        {120.0, 110.0, 90.0, 0.01, 0.03, 0.4, 90, 91, 91, 15.347496933784438},
        {100.0, 90.0, 110.0, 0.06, 0.03, 0.15, 30, 180, 210, 13.628201313429671},
        {80.0, 100.0, 100.0, 0.01, 0.0, 0.15, 30, 91, 91, 19.750882775192171},
        {100.0, 100.0, 100.0, 0.01, 0.0, 0.4, 90, 365, 180, 22.061449500458775},
        {100.0, 105.0, 95.0, 0.03, 0.03, 0.25, 1, 2, 720, 10.663623601268872},
    };

    for (const Case& c : cases)
    {
        BOOST_TEST_CONTEXT("spot " << c.spot << ", strikes " << c.callStrike << " and " << c.putStrike << ", r " << c.r
                                   << ", q " << c.q << ", vol " << c.v << ", days " << c.choosingDays << ", "
                                   << c.callDays << ", " << c.putDays)
        {
            BOOST_CHECK_SMALL(complexChooserValue(c.spot, c.callStrike, c.putStrike, c.r, c.q, c.v, c.choosingDays,
                                                  c.callDays, c.putDays) -
                                  c.expected,
                              1e-11);
        }
    }
}

BOOST_AUTO_TEST_CASE(testComplexChooserScalesWithItsTerms)
{
    BOOST_TEST_MESSAGE("Testing complex chooser with its spot and strikes scaled...");

    Settings::instance().evaluationDate() = Date(20, October, 2026);

    Rate r = 0.06, q = 0.03;
    Integer choosingDays = 90, callDays = 180, putDays = 210;

    for (Volatility v : {0.005, 0.35})
    {
        Real unscaled = complexChooserValue(100.0, 105.0, 95.0, r, q, v, choosingDays, callDays, putDays);
        for (Real scale : {1e-6, 1e-3, 1e3, 1e6, 1e9})
        {
            BOOST_TEST_CONTEXT("vol " << v << ", scale " << scale)
            {
                Real scaled = complexChooserValue(100.0 * scale, 105.0 * scale, 95.0 * scale, r, q, v, choosingDays,
                                                  callDays, putDays);
                BOOST_CHECK_SMALL(scaled / scale - unscaled, 1e-10);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testComplexChooserAtLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing complex chooser at low volatility...");

    Settings::instance().evaluationDate() = Date(20, October, 2026);

    Real spot = 100.0, callStrike = 100.0, putStrike = 90.0;
    Rate r = 0.06, q = 0.03;
    Time t = 90.0 / 360.0, tc = 180.0 / 360.0, tp = 210.0 / 360.0;

    // With no volatility left, the spot at the choosing date is its forward.
    Real forward = spot * std::exp((r - q) * t);
    Real call = forward * std::exp(-q * (tc - t)) - callStrike * std::exp(-r * (tc - t));
    Real put = putStrike * std::exp(-r * (tp - t)) - forward * std::exp(-q * (tp - t));
    Real limit = std::exp(-r * t) * std::max({call, put, 0.0});
    BOOST_REQUIRE(limit > 1.0);

    for (Volatility v : {1e-3, 1e-4, 1e-6, 1e-8, 1e-10, 1e-12, 1e-14})
    {
        BOOST_TEST_CONTEXT("vol " << v)
        {
            BOOST_CHECK_SMALL(complexChooserValue(spot, callStrike, putStrike, r, q, v, 90, 180, 210) - limit,
                              100.0 * v + 1e-12);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
