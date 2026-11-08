/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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
#include <ql/instruments/twoassetcorrelationoption.hpp>
#include <ql/pricingengines/exotic/analytictwoassetcorrelationengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(TwoAssetCorrelationOptionTests)

BOOST_AUTO_TEST_CASE(testAnalyticEngine)
{
    BOOST_TEST_MESSAGE("Testing analytic engine for two-asset correlation option...");

    Date today = Settings::instance().evaluationDate();
    DayCounter dc = Actual360();

    Option::Type type = Option::Call;
    Real strike1 = 50.0;
    Real strike2 = 70.0;
    Date exDate = today + 180;

    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(exDate);

    TwoAssetCorrelationOption option(type, strike1, strike2, exercise);

    Handle<Quote> underlying1(ext::make_shared<SimpleQuote>(52.0));
    Handle<Quote> underlying2(ext::make_shared<SimpleQuote>(65.0));
    Handle<YieldTermStructure> dividendTS1(flatRate(today, 0.0, dc));
    Handle<YieldTermStructure> dividendTS2(flatRate(today, 0.0, dc));
    Handle<YieldTermStructure> riskFreeTS(flatRate(today, 0.1, dc));
    Handle<BlackVolTermStructure> blackVolTS1(flatVol(today, 0.2, dc));
    Handle<BlackVolTermStructure> blackVolTS2(flatVol(today, 0.3, dc));
    Handle<Quote> correlation(ext::make_shared<SimpleQuote>(0.75));

    ext::shared_ptr<BlackScholesMertonProcess> process1 =
        ext::make_shared<BlackScholesMertonProcess>(underlying1, dividendTS1, riskFreeTS, blackVolTS1);

    ext::shared_ptr<BlackScholesMertonProcess> process2 =
        ext::make_shared<BlackScholesMertonProcess>(underlying2, dividendTS2, riskFreeTS, blackVolTS2);

    option.setPricingEngine(ext::make_shared<AnalyticTwoAssetCorrelationEngine>(process1, process2, correlation));

    Real calculated = option.NPV();
    Real expected = 4.7073;
    Real error = std::fabs(calculated - expected);
    Real tolerance = 1e-4;
    if (error > tolerance)
        BOOST_ERROR("Failed to reproduce holder-extensible option value" << "\n    expected:   " << expected
                                                                         << "\n    calculated: " << calculated
                                                                         << "\n    error:      " << error);
}

BOOST_AUTO_TEST_CASE(testAgainstDefinition)
{
    BOOST_TEST_MESSAGE("Testing two-asset correlation options against their definition...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // Values from the definition, integrated in mpmath at 40 digits over the first asset's normal: where it ends past
    // its strike, the second asset's conditional Black-Scholes value. With Drezner's six-decimal bivariate normal, the
    // engine missed them by 2.4e-7 to 1.3e-5.
    const struct
    {
        Option::Type type;
        Real s1, s2, strike1, strike2;
        Rate r, q1, q2;
        Volatility vol1, vol2;
        Real rho;
        Integer days;
        Real value;
    } cases[] = {{Option::Call, 100, 100, 95, 105, 0.05, 0.01, 0.02, 0.2, 0.3, 0.5, 365, 9.569572037467},
                 {Option::Put, 100, 100, 105, 95, 0.05, 0.01, 0.02, 0.2, 0.3, -0.5, 365, 2.328588798723},
                 {Option::Call, 52, 65, 50, 70, 0.10, 0.0, 0.0, 0.2, 0.3, 0.75, 182, 4.696339492015},
                 {Option::Put, 100, 110, 90, 120, 0.01, 0.05, 0.04, 0.35, 0.15, 0.9, 730, 17.00428278003}};

    for (const auto& c : cases)
    {
        const Handle<YieldTermStructure> rTS(flatRate(today, c.r, dc));
        const auto process1 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(c.s1)), Handle<YieldTermStructure>(flatRate(today, c.q1, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol1, dc)));
        const auto process2 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(c.s2)), Handle<YieldTermStructure>(flatRate(today, c.q2, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol2, dc)));
        TwoAssetCorrelationOption option(c.type, c.strike1, c.strike2,
                                         ext::make_shared<EuropeanExercise>(today + c.days));
        option.setPricingEngine(ext::make_shared<AnalyticTwoAssetCorrelationEngine>(
            process1, process2, Handle<Quote>(ext::make_shared<SimpleQuote>(c.rho))));

        BOOST_TEST_CONTEXT(c.type << " with spots " << c.s1 << " and " << c.s2 << ", strikes " << c.strike1 << " and "
                                  << c.strike2 << ", correlation " << c.rho)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, 1.0e-10);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
