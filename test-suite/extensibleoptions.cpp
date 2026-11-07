/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2011 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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
#include <ql/instruments/holderextensibleoption.hpp>
#include <ql/instruments/writerextensibleoption.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/exotic/analyticholderextensibleoptionengine.hpp>
#include <ql/pricingengines/exotic/analyticwriterextensibleoptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ExtensibleOptionsTests)

BOOST_AUTO_TEST_CASE(testAnalyticHolderExtensibleOptionEngine)
{
    BOOST_TEST_MESSAGE("Testing analytic engine for holder-extensible option...");

    Option::Type type = Option::Call;
    Real strike1 = 100.0;
    Real strike2 = 105.0;
    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();
    Date exDate1 = today + 180;
    Date exDate2 = today + 270;
    Real premium = 1.0;

    ext::shared_ptr<SimpleQuote> spot = ext::make_shared<SimpleQuote>(100.0);
    ext::shared_ptr<SimpleQuote> qRate = ext::make_shared<SimpleQuote>(0.0);
    ext::shared_ptr<SimpleQuote> rRate = ext::make_shared<SimpleQuote>(0.08);
    ext::shared_ptr<SimpleQuote> vol = ext::make_shared<SimpleQuote>(0.25);

    ext::shared_ptr<StrikedTypePayoff> payoff = ext::make_shared<PlainVanillaPayoff>(type, strike1);
    ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(exDate1);

    HolderExtensibleOption option(type, premium, exDate2, strike2, payoff, exercise);

    Handle<Quote> underlying(spot);
    Handle<YieldTermStructure> dividendTS(flatRate(today, qRate, dc));
    Handle<YieldTermStructure> riskFreeTS(flatRate(today, rRate, dc));
    Handle<BlackVolTermStructure> blackVolTS(flatVol(today, vol, dc));

    const ext::shared_ptr<BlackScholesMertonProcess> process =
        ext::make_shared<BlackScholesMertonProcess>(underlying, dividendTS, riskFreeTS, blackVolTS);

    option.setPricingEngine(ext::make_shared<AnalyticHolderExtensibleOptionEngine>(process));

    Real calculated = option.NPV();
    Real expected = 9.4233;
    Real error = std::fabs(calculated - expected);
    Real tolerance = 1e-4;
    if (error > tolerance)
        BOOST_ERROR("Failed to reproduce holder-extensible option value" << "\n    expected:   " << expected
                                                                         << "\n    calculated: " << calculated
                                                                         << "\n    error:      " << error);
}

BOOST_AUTO_TEST_CASE(testAnalyticWriterExtensibleOptionEngine)
{
    BOOST_TEST_MESSAGE("Testing analytic engine for writer-extensible option...");

    // What we need for the option (tests):
    Option::Type type = Option::Call;
    Real strike1 = 90.0;
    Real strike2 = 82.0;
    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();
    Date exDate1 = today + 180;
    Date exDate2 = today + 270;

    ext::shared_ptr<SimpleQuote> spot = ext::make_shared<SimpleQuote>(80.0);
    ext::shared_ptr<SimpleQuote> qRate = ext::make_shared<SimpleQuote>(0.0);
    ext::shared_ptr<YieldTermStructure> dividendTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate = ext::make_shared<SimpleQuote>(0.10);
    ext::shared_ptr<YieldTermStructure> riskFreeTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol = ext::make_shared<SimpleQuote>(0.30);
    ext::shared_ptr<BlackVolTermStructure> blackVolTS = flatVol(today, vol, dc);

    // B&S process (needed for the engine):
    const ext::shared_ptr<GeneralizedBlackScholesProcess> process = ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(spot), Handle<YieldTermStructure>(dividendTS), Handle<YieldTermStructure>(riskFreeTS),
        Handle<BlackVolTermStructure>(blackVolTS));

    // The engine:
    ext::shared_ptr<PricingEngine> engine = ext::make_shared<AnalyticWriterExtensibleOptionEngine>(process);

    // Create the arguments:
    ext::shared_ptr<PlainVanillaPayoff> payoff1 = ext::make_shared<PlainVanillaPayoff>(type, strike1);
    ext::shared_ptr<Exercise> exercise1 = ext::make_shared<EuropeanExercise>(exDate1);
    ext::shared_ptr<PlainVanillaPayoff> payoff2 = ext::make_shared<PlainVanillaPayoff>(type, strike2);
    ext::shared_ptr<Exercise> exercise2 = ext::make_shared<EuropeanExercise>(exDate2);

    // Create the option by calling the constructor:
    WriterExtensibleOption option(payoff1, exercise1, payoff2, exercise2);

    // Set the engine of our option:
    option.setPricingEngine(engine);

    // Compare the calculated NPV value to the theoretical value:
    Real calculated = option.NPV();
    Real expected = 6.8238;
    Real error = std::fabs(calculated - expected);
    Real tolerance = 1e-4;
    if (error > tolerance)
        BOOST_ERROR("Failed to reproduce writer-extensible option value" << "\n    expected:   " << expected
                                                                         << "\n    calculated: " << calculated
                                                                         << "\n    error:      " << error);
}

namespace
{
    // Days on Actual360, so that a table can state its exact times.
    Real holderExtensibleValue(Option::Type type,
                               Real spot,
                               Real strike1,
                               Real strike2,
                               Real premium,
                               Rate r,
                               Rate q,
                               Volatility v,
                               Integer firstDays,
                               Integer secondDays)
    {
        DayCounter dc = Actual360();
        Date today = Settings::instance().evaluationDate();
        auto process = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(spot)), Handle<YieldTermStructure>(flatRate(today, q, dc)),
            Handle<YieldTermStructure>(flatRate(today, r, dc)), Handle<BlackVolTermStructure>(flatVol(today, v, dc)));
        HolderExtensibleOption option(type, premium, today + secondDays, strike2,
                                      ext::make_shared<PlainVanillaPayoff>(type, strike1),
                                      ext::make_shared<EuropeanExercise>(today + firstDays));
        option.setPricingEngine(ext::make_shared<AnalyticHolderExtensibleOptionEngine>(process));
        return option.NPV();
    }

    // Curves given by their values at the two expiries, in days on Actual360.
    ext::shared_ptr<GeneralizedBlackScholesProcess> extensibleProcess(
        Real spot, const Rate r[2], const Rate q[2], const Volatility v[2], Integer firstDays, Integer secondDays)
    {
        DayCounter dc = Actual360();
        Date today = Settings::instance().evaluationDate();
        std::vector<Date> dates = {today, today + firstDays, today + secondDays};
        std::vector<Date> volDates = {today + firstDays, today + secondDays};
        return ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(spot)),
            Handle<YieldTermStructure>(ext::make_shared<ZeroCurve>(dates, std::vector<Rate>{q[0], q[0], q[1]}, dc)),
            Handle<YieldTermStructure>(ext::make_shared<ZeroCurve>(dates, std::vector<Rate>{r[0], r[0], r[1]}, dc)),
            Handle<BlackVolTermStructure>(
                ext::make_shared<BlackVarianceCurve>(today, volDates, std::vector<Volatility>{v[0], v[1]}, dc)));
    }

    Real writerExtensibleValue(Option::Type type,
                               Real spot,
                               Real strike1,
                               Real strike2,
                               Rate r,
                               Rate q,
                               Volatility v,
                               Integer firstDays,
                               Integer secondDays)
    {
        DayCounter dc = Actual360();
        Date today = Settings::instance().evaluationDate();
        auto process = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(spot)), Handle<YieldTermStructure>(flatRate(today, q, dc)),
            Handle<YieldTermStructure>(flatRate(today, r, dc)), Handle<BlackVolTermStructure>(flatVol(today, v, dc)));
        WriterExtensibleOption option(ext::make_shared<PlainVanillaPayoff>(type, strike1),
                                      ext::make_shared<EuropeanExercise>(today + firstDays),
                                      ext::make_shared<PlainVanillaPayoff>(type, strike2),
                                      ext::make_shared<EuropeanExercise>(today + secondDays));
        option.setPricingEngine(ext::make_shared<AnalyticWriterExtensibleOptionEngine>(process));
        return option.NPV();
    }
}

BOOST_AUTO_TEST_CASE(testHolderExtensibleOptionAgainstItsDefinition)
{
    BOOST_TEST_MESSAGE("Testing holder-extensible options against their definition...");

    Settings::instance().evaluationDate() = Date(20, October, 2026);

    // At the first expiry, the best of exercising, lapsing, and paying the premium for
    // an option on the second strike and expiry, integrated over the spot to 25 digits.
    struct Case
    {
        Option::Type type;
        Real spot, strike1, strike2, premium;
        Rate r, q;
        Volatility v;
        Integer firstDays, secondDays;
        Real expected;
    };
    Case cases[] = {
        // The second strike on either side of the first
        {Option::Put, 100.0, 100.0, 95.0, 1.0, 0.08, 0.0, 0.25, 180, 270, 5.2009554331728382},
        {Option::Put, 100.0, 100.0, 105.0, 1.0, 0.08, 0.0, 0.25, 180, 270, 7.2042184187006566},
        {Option::Put, 80.0, 100.0, 95.0, 1.0, 0.08, 0.0, 0.25, 180, 270, 17.320976762270057},
        {Option::Call, 100.0, 100.0, 120.0, 1.0, 0.08, 0.0, 0.25, 180, 270, 9.0411753343981096},
        {Option::Call, 130.0, 100.0, 105.0, 1.0, 0.08, 0.0, 0.25, 180, 270, 34.370676070189252},
        {Option::Call, 100.0, 100.0, 100.0, 1.0, 0.08, 0.03, 0.25, 180, 270, 9.3728040610972895},
        {Option::Call, 120.0, 100.0, 100.0, 1.0, 0.08, 0.03, 0.25, 180, 270, 24.081677496910647},
        // call, extending then exercising
        {Option::Call, 120.0, 100.0, 300.0, 0.5, 0.08, 0.0, 1.0, 360, 3600, 105.06559015687641},
        // call, extending with no end
        {Option::Call, 120.0, 100.0, 70.0, 0.5, 0.08, 0.0, 0.5, 360, 3600, 97.013523325319559},
        // call, never extending
        {Option::Call, 120.0, 70.0, 100.0, 20.0, 0.08, 0.0, 1.0, 180, 270, 59.862122368636668},
        // put, exercising then extending
        {Option::Put, 120.0, 100.0, 105.0, 0.5, 0.01, 0.1, 1.0, 360, 3600, 87.293054116508280},
        // put, extending from a spot of 0
        {Option::Put, 120.0, 100.0, 300.0, 0.5, 0.01, 0.1, 1.0, 360, 3600, 259.80028146766244},
        // put, never extending
        {Option::Put, 80.0, 100.0, 95.0, 5.0, 0.08, 0.0, 0.25, 360, 3600, 16.084013220403779},
        // negative dividend yields: call, extending with no end
        {Option::Call, 120.0, 100.0, 70.0, 0.5, 0.08, -0.1, 0.25, 360, 3600, 294.31231818529110},
        // call, extending, exercising, extending again
        {Option::Call, 120.0, 100.0, 300.0, 0.5, 0.03, -0.02, 1.0, 90, 1080, 58.203800714776474},
        // call, exercising, then extending
        {Option::Call, 200.0, 70.0, 100.0, 20.0, -0.01, -0.02, 0.25, 360, 3600, 133.78295701660791},
        // put, extending from a spot of 0
        {Option::Put, 120.0, 100.0, 300.0, 0.5, -0.01, -0.02, 1.0, 360, 3600, 306.50709619278509},
        // put, extending, exercising, extending again
        {Option::Put, 120.0, 100.0, 95.0, 0.5, -0.01, -0.02, 0.25, 360, 3600, 20.549279126852085},
        // put, extending, then exercising
        {Option::Put, 30.0, 100.0, 300.0, 20.0, 0.08, -0.1, 0.25, 360, 3600, 59.168547986668496},
    };

    for (const Case& c : cases)
    {
        BOOST_TEST_CONTEXT(c.type << ", spot " << c.spot << ", strikes " << c.strike1 << " and " << c.strike2
                                  << ", premium " << c.premium << ", r " << c.r << ", q " << c.q << ", vol " << c.v
                                  << ", days " << c.firstDays << " and " << c.secondDays)
        {
            BOOST_CHECK_SMALL(holderExtensibleValue(c.type, c.spot, c.strike1, c.strike2, c.premium, c.r, c.q, c.v,
                                                    c.firstDays, c.secondDays) -
                                  c.expected,
                              1e-11 * std::max(1.0, c.expected));
        }
    }
}

BOOST_AUTO_TEST_CASE(testHolderExtensibleOptionScalesWithItsTerms)
{
    BOOST_TEST_MESSAGE("Testing holder-extensible options with their spot, strikes and premium scaled...");

    Settings::instance().evaluationDate() = Date(20, October, 2026);

    Rate r = 0.08, q = 0.03;

    for (Option::Type type : {Option::Call, Option::Put})
    {
        for (Volatility v : {0.05, 0.4})
        {
            Real unscaled = holderExtensibleValue(type, 100.0, 100.0, 105.0, 1.0, r, q, v, 180, 270);
            for (Real scale : {1e-6, 1e-3, 1e3, 1e6, 1e9})
            {
                BOOST_TEST_CONTEXT(type << ", vol " << v << ", scale " << scale)
                {
                    Real scaled = holderExtensibleValue(type, 100.0 * scale, 100.0 * scale, 105.0 * scale, scale, r, q,
                                                        v, 180, 270);
                    BOOST_CHECK_SMALL(scaled / scale - unscaled, 1e-10);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testHolderExtensibleOptionOnCurvesThatAreNotFlat)
{
    BOOST_TEST_MESSAGE("Testing holder-extensible options on curves that aren't flat...");

    Date today(20, October, 2026);
    Settings::instance().evaluationDate() = today;

    // The definition, integrated over the spot to 25 digits.  Each term
    // structure is given by its values at the first and second expiries.
    struct Case
    {
        Option::Type type;
        Real spot, strike1, strike2, premium;
        Rate r[2], q[2];
        Volatility v[2];
        Integer firstDays, secondDays;
        Real expected;
    };
    Case cases[] = {
        {Option::Call, 100.0, 100.0, 105.0, 1.0, {0.01, 0.06}, {0.02, 0.02}, {0.3, 0.3}, 180, 270, 8.9930763812729706},
        {Option::Put, 100.0, 100.0, 105.0, 1.0, {0.01, 0.06}, {0.02, 0.02}, {0.3, 0.3}, 180, 270, 10.343417781979556},
        {Option::Call, 120.0, 100.0, 95.0, 5.0, {0.05, 0.05}, {0.0, 0.05}, {0.3, 0.3}, 30, 365, 22.753902568378055},
        {Option::Put, 80.0, 100.0, 120.0, 1.0, {0.05, 0.05}, {0.0, 0.05}, {0.3, 0.3}, 30, 365, 38.187668821924450},
        {Option::Call, 100.0, 100.0, 120.0, 1.0, {0.05, 0.05}, {0.02, 0.02}, {0.2, 0.4}, 180, 270, 8.0957299615108391},
        {Option::Put, 100.0, 100.0, 95.0, 5.0, {0.05, 0.05}, {0.02, 0.02}, {0.2, 0.4}, 30, 365, 6.5322316934655399},
        {Option::Call, 120.0, 100.0, 105.0, 1.0, {0.01, 0.06}, {0.0, 0.05}, {0.2, 0.4}, 180, 270, 23.728242738951259},
        {Option::Put, 80.0, 100.0, 105.0, 1.0, {0.01, 0.06}, {0.0, 0.05}, {0.2, 0.4}, 30, 365, 27.122385360127065},
        {Option::Call, 120.0, 100.0, 120.0, 1.0, {0.03, 0.03}, {0.01, -0.03}, {0.3, 0.3}, 30, 365, 20.687629883408000},
        {Option::Put, 100.0, 100.0, 95.0, 1.0, {0.03, 0.03}, {0.01, -0.03}, {0.3, 0.3}, 30, 365, 6.3349285552223684},
        // Forward yields and rates that are negative, though the zero rates give no sign of it.
        {Option::Call, 120.0, 100.0, 100.0, 1.0, {0.05, 0.05}, {0.05, 0.02}, {0.3, 0.3}, 180, 270, 24.333919562754299},
        {Option::Call, 120.0, 100.0, 100.0, 1.0, {0.06, 0.01}, {0.05, 0.02}, {0.3, 0.3}, 180, 270, 22.992898116359363},
        {Option::Call, 120.0, 100.0, 105.0, 0.5, {0.05, 0.05}, {0.01, -0.03}, {0.3, 0.3}, 180, 270, 24.982004838194571},
        {Option::Put, 100.0, 100.0, 100.0, 1.0, {0.06, 0.01}, {0.01, -0.03}, {0.5, 0.5}, 180, 270, 14.937813153583226},
        // Extending pays only for spots between 99.994 and 100.001 at the first expiry.
        {Option::Call, 100.0, 100.0, 120.0, 1.0, {0.06, 0.01}, {0.01, -0.03}, {0.3, 0.3}, 90, 180, 6.5666474247746335},
    };

    for (const Case& c : cases)
    {
        HolderExtensibleOption option(c.type, c.premium, today + c.secondDays, c.strike2,
                                      ext::make_shared<PlainVanillaPayoff>(c.type, c.strike1),
                                      ext::make_shared<EuropeanExercise>(today + c.firstDays));
        option.setPricingEngine(ext::make_shared<AnalyticHolderExtensibleOptionEngine>(
            extensibleProcess(c.spot, c.r, c.q, c.v, c.firstDays, c.secondDays)));
        BOOST_TEST_CONTEXT(c.type << ", spot " << c.spot << ", strikes " << c.strike1 << " and " << c.strike2 << ", r "
                                  << c.r[0] << " and " << c.r[1] << ", q " << c.q[0] << " and " << c.q[1] << ", vol "
                                  << c.v[0] << " and " << c.v[1] << ", days " << c.firstDays << " and " << c.secondDays)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.expected, 1e-11);
        }
    }
}

BOOST_AUTO_TEST_CASE(testWriterExtensibleOptionOnCurvesThatAreNotFlat)
{
    BOOST_TEST_MESSAGE("Testing writer-extensible options on curves that aren't flat...");

    Date today(20, October, 2026);
    Settings::instance().evaluationDate() = today;

    // In the money at the first expiry, exercised, and otherwise extended, integrated over
    // the spot to 25 digits.  Each term structure is given as in the previous test.
    struct Case
    {
        Option::Type type;
        Real spot, strike1, strike2;
        Rate r[2], q[2];
        Volatility v[2];
        Integer firstDays, secondDays;
        Real expected;
    };
    Case cases[] = {
        {Option::Call, 100.0, 100.0, 105.0, {0.01, 0.06}, {0.02, 0.02}, {0.3, 0.3}, 180, 270, 8.9567178039757611},
        {Option::Put, 100.0, 100.0, 105.0, {0.01, 0.06}, {0.02, 0.02}, {0.3, 0.3}, 180, 270, 9.7055918683868521},
        {Option::Call, 120.0, 100.0, 95.0, {0.05, 0.05}, {0.0, 0.05}, {0.3, 0.3}, 30, 365, 20.665110840043563},
        {Option::Put, 80.0, 100.0, 120.0, {0.05, 0.05}, {0.0, 0.05}, {0.3, 0.3}, 30, 365, 19.711376788747862},
        {Option::Call, 100.0, 100.0, 120.0, {0.05, 0.05}, {0.02, 0.02}, {0.2, 0.4}, 180, 270, 7.9922218203224798},
        {Option::Put, 100.0, 100.0, 95.0, {0.05, 0.05}, {0.02, 0.02}, {0.2, 0.4}, 30, 365, 7.2074260770124171},
        {Option::Call, 120.0, 100.0, 105.0, {0.01, 0.06}, {0.0, 0.05}, {0.2, 0.4}, 180, 270, 21.955024568832709},
        {Option::Put, 80.0, 100.0, 105.0, {0.01, 0.06}, {0.0, 0.05}, {0.2, 0.4}, 30, 365, 19.917647504509510},
    };

    for (const Case& c : cases)
    {
        WriterExtensibleOption option(ext::make_shared<PlainVanillaPayoff>(c.type, c.strike1),
                                      ext::make_shared<EuropeanExercise>(today + c.firstDays),
                                      ext::make_shared<PlainVanillaPayoff>(c.type, c.strike2),
                                      ext::make_shared<EuropeanExercise>(today + c.secondDays));
        option.setPricingEngine(ext::make_shared<AnalyticWriterExtensibleOptionEngine>(
            extensibleProcess(c.spot, c.r, c.q, c.v, c.firstDays, c.secondDays)));
        BOOST_TEST_CONTEXT(c.type << ", spot " << c.spot << ", strikes " << c.strike1 << " and " << c.strike2 << ", r "
                                  << c.r[0] << " and " << c.r[1] << ", q " << c.q[0] << " and " << c.q[1] << ", vol "
                                  << c.v[0] << " and " << c.v[1] << ", days " << c.firstDays << " and " << c.secondDays)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.expected, 1e-11);
        }
    }
}

BOOST_AUTO_TEST_CASE(testHolderExtensibleOptionAtLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing holder-extensible options at low and zero volatility...");

    Settings::instance().evaluationDate() = Date(20, October, 2026);

    Rate r = 0.08, q = 0.02;
    Real premium = 1.0;
    Time t1 = 180.0 / 360.0, t2 = 270.0 / 360.0;

    struct Case
    {
        Option::Type type;
        Real spot, strike1, strike2;
    };
    // Extending, exercising and lapsing being the best in turn.
    Case cases[] = {{Option::Call, 100.0, 100.0, 80.0}, {Option::Call, 130.0, 100.0, 115.0},
                    {Option::Call, 90.0, 100.0, 120.0}, {Option::Put, 100.0, 110.0, 130.0},
                    {Option::Put, 90.0, 110.0, 100.0},  {Option::Put, 130.0, 100.0, 95.0}};

    for (Volatility v : {0.0, 1e-158, 1e-16, 1e-14, 1e-12, 1e-10, 1e-8, 1e-6, 1e-4})
    {
        for (const Case& c : cases)
        {
            BOOST_TEST_CONTEXT(c.type << ", spot " << c.spot << ", strikes " << c.strike1 << " and " << c.strike2
                                      << ", vol " << v)
            {
                // With no volatility to speak of, the spot at the first expiry is its forward.
                Real eta = Real(c.type);
                Real forward = c.spot * std::exp((r - q) * t1);
                Real exercised = eta * (forward - c.strike1);
                Real extended =
                    eta * (forward * std::exp(-q * (t2 - t1)) - c.strike2 * std::exp(-r * (t2 - t1))) - premium;
                Real expected = std::exp(-r * t1) * std::max({exercised, extended, 0.0});
                BOOST_CHECK_SMALL(
                    holderExtensibleValue(c.type, c.spot, c.strike1, c.strike2, premium, r, q, v, 180, 270) - expected,
                    1e-12);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testWriterExtensibleOptionAtLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing writer-extensible options at low and zero volatility...");

    Settings::instance().evaluationDate() = Date(20, October, 2026);

    Real spot = 100.0;
    Rate r = 0.08, q = 0.02;
    Time t1 = 180.0 / 360.0, t2 = 270.0 / 360.0;

    struct Case
    {
        Option::Type type;
        Real strike1, strike2;
    };
    // Exercised, and extended into something and into nothing.
    Case cases[] = {{Option::Call, 100.0, 110.0}, {Option::Call, 110.0, 100.0}, {Option::Call, 110.0, 115.0},
                    {Option::Put, 110.0, 100.0},  {Option::Put, 100.0, 110.0},  {Option::Put, 100.0, 95.0}};

    for (Volatility v : {0.0, 1e-158, 1e-16, 1e-14, 1e-12, 1e-10, 1e-8, 1e-6, 1e-4})
    {
        for (const Case& c : cases)
        {
            BOOST_TEST_CONTEXT(c.type << ", strikes " << c.strike1 << " and " << c.strike2 << ", vol " << v)
            {
                Real eta = Real(c.type);
                Real exercised = std::exp(-r * t1) * eta * (spot * std::exp((r - q) * t1) - c.strike1);
                Real extended = std::exp(-r * t2) * eta * (spot * std::exp((r - q) * t2) - c.strike2);
                Real expected = exercised > 0.0 ? exercised : std::max(extended, 0.0);
                BOOST_CHECK_SMALL(
                    writerExtensibleValue(c.type, spot, c.strike1, c.strike2, r, q, v, 180, 270) - expected, 1e-12);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testExtensibleOptionsWithNoVarianceBeforeTheFirstExpiry)
{
    BOOST_TEST_MESSAGE("Testing extensible options with no variance before the first expiry...");

    Date today(20, October, 2026);
    Settings::instance().evaluationDate() = today;

    Real strike1 = 100.0, premium = 1.0;
    Rate rates[] = {0.08, 0.08}, yields[] = {0.02, 0.02};
    Volatility vols[] = {0.0, 0.3};
    Integer firstDays = 180, secondDays = 270;
    Time t1 = firstDays / 360.0, t2 = secondDays / 360.0;
    Real r = rates[0], q = yields[0];

    for (Option::Type type : {Option::Call, Option::Put})
    {
        // Lapsed, extended and exercised, as far as the holder's concerned, on either side of the first strike.
        for (Real spot : {60.0, 97.0, 140.0})
        {
            for (Real strike2 : {95.0, 105.0})
            {
                BOOST_TEST_CONTEXT(type << ", spot " << spot << ", second strike " << strike2)
                {
                    // The spot at the first expiry is its forward.
                    Real exercised = std::exp(-r * t1) * Real(type) * (spot * std::exp((r - q) * t1) - strike1);
                    Real extended = blackFormula(type, strike2, spot * std::exp((r - q) * t2), vols[1] * std::sqrt(t2),
                                                 std::exp(-r * t2));

                    auto process = extensibleProcess(spot, rates, yields, vols, firstDays, secondDays);

                    HolderExtensibleOption holder(type, premium, today + secondDays, strike2,
                                                  ext::make_shared<PlainVanillaPayoff>(type, strike1),
                                                  ext::make_shared<EuropeanExercise>(today + firstDays));
                    holder.setPricingEngine(ext::make_shared<AnalyticHolderExtensibleOptionEngine>(process));
                    BOOST_CHECK_SMALL(holder.NPV() - std::max({exercised, extended - std::exp(-r * t1) * premium, 0.0}),
                                      1e-12);

                    WriterExtensibleOption writer(ext::make_shared<PlainVanillaPayoff>(type, strike1),
                                                  ext::make_shared<EuropeanExercise>(today + firstDays),
                                                  ext::make_shared<PlainVanillaPayoff>(type, strike2),
                                                  ext::make_shared<EuropeanExercise>(today + secondDays));
                    writer.setPricingEngine(ext::make_shared<AnalyticWriterExtensibleOptionEngine>(process));
                    BOOST_CHECK_SMALL(writer.NPV() - (exercised > 0.0 ? exercised : extended), 1e-12);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
