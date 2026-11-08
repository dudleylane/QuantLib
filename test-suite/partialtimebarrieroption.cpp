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
#include <ql/instruments/partialtimebarrieroption.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/barrier/analyticpartialtimebarrieroptionengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PartialTimeBarrierOptionTests)

#undef REPORT_FAILURE
#define REPORT_FAILURE(greekName, barrierType, barrier, rebate, payoff, exercise, s, q, r, today, v, expected, \
                       calculated, error, tolerance)                                                           \
    BOOST_ERROR("\n"                                                                                           \
                << barrierTypeToString(barrierType) << " " << exerciseTypeToString(exercise) << " "            \
                << payoff->optionType() << " option with " << payoffTypeToString(payoff) << " payoff:\n"       \
                << "    underlying value: " << s << "\n"                                                       \
                << "    strike:           " << payoff->strike() << "\n"                                        \
                << "    barrier:          " << barrier << "\n"                                                 \
                << "    rebate:           " << rebate << "\n"                                                  \
                << "    dividend yield:   " << io::rate(q) << "\n"                                             \
                << "    risk-free rate:   " << io::rate(r) << "\n"                                             \
                << "    reference date:   " << today << "\n"                                                   \
                << "    maturity:         " << exercise->lastDate() << "\n"                                    \
                << "    volatility:       " << io::volatility(v) << "\n\n"                                     \
                << "    expected   " << greekName << ": " << expected << "\n"                                  \
                << "    calculated " << greekName << ": " << calculated << "\n"                                \
                << "    error:            " << error << "\n"                                                   \
                << "    tolerance:        " << tolerance);

struct TestCase
{
    Real underlying;
    Real strike;
    Integer days;
    Real result;
};


BOOST_AUTO_TEST_CASE(testAnalyticEngine)
{
    BOOST_TEST_MESSAGE("Testing analytic engine for partial-time barrier option...");

    Date today = Settings::instance().evaluationDate();

    Option::Type type = Option::Call;
    DayCounter dc = Actual360();
    Date maturity = today + 360;
    auto exercise = ext::make_shared<EuropeanExercise>(maturity);
    Real barrier = 100.0;
    Real rebate = 0.0;

    auto spot = ext::make_shared<SimpleQuote>();
    auto qRate = ext::make_shared<SimpleQuote>(0.0);
    auto rRate = ext::make_shared<SimpleQuote>(0.1);
    auto vol = ext::make_shared<SimpleQuote>(0.25);

    Handle<Quote> underlying(spot);
    Handle<YieldTermStructure> dividendTS(flatRate(today, qRate, dc));
    Handle<YieldTermStructure> riskFreeTS(flatRate(today, rRate, dc));
    Handle<BlackVolTermStructure> blackVolTS(flatVol(today, vol, dc));

    const auto process = ext::make_shared<BlackScholesMertonProcess>(underlying, dividendTS, riskFreeTS, blackVolTS);
    auto engine = ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process);

    TestCase cases[] = {{95.0, 90.0, 1, 0.0393},     {95.0, 110.0, 1, 0.0000},
                        {105.0, 90.0, 1, 9.8751},    {105.0, 110.0, 1, 6.2303},

                        {95.0, 90.0, 90, 6.2747},    {95.0, 110.0, 90, 3.7352},
                        {105.0, 90.0, 90, 15.6324},  {105.0, 110.0, 90, 9.6812},

                        {95.0, 90.0, 180, 10.3345},  {95.0, 110.0, 180, 5.8712},
                        {105.0, 90.0, 180, 19.2896}, {105.0, 110.0, 180, 11.6055},

                        {95.0, 90.0, 270, 13.4342},  {95.0, 110.0, 270, 7.1270},
                        {105.0, 90.0, 270, 22.0753}, {105.0, 110.0, 270, 12.7342},

                        {95.0, 90.0, 359, 16.8576},  {95.0, 110.0, 359, 7.5763},
                        {105.0, 90.0, 359, 25.1488}, {105.0, 110.0, 359, 13.1376}};

    for (auto& i : cases)
    {
        Date coverEventDate = today + i.days;
        auto payoff = ext::make_shared<PlainVanillaPayoff>(type, i.strike);
        PartialTimeBarrierOption option(Barrier::DownOut, PartialBarrier::EndB1, barrier, rebate, coverEventDate,
                                        payoff, exercise);
        option.setPricingEngine(engine);

        spot->setValue(i.underlying);
        Real calculated = option.NPV();
        Real expected = i.result;
        Real error = std::fabs(calculated - expected);
        Real tolerance = 1e-4;
        if (error > tolerance)
            REPORT_FAILURE("value", Barrier::DownOut, barrier, rebate, payoff, exercise, i.underlying, 0.0, 0.1, today,
                           0.25, expected, calculated, error, tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testAnalyticEnginePutOption)
{
    BOOST_TEST_MESSAGE("Testing analytic engine for partial-time put barrier option...");

    Date today = Settings::instance().evaluationDate();

    Option::Type type = Option::Put;
    DayCounter dc = Actual360();
    Date maturity = today + 360;
    auto exercise = ext::make_shared<EuropeanExercise>(maturity);
    Real barrier = 100.0;
    Real rebate = 0.0;

    auto spot = ext::make_shared<SimpleQuote>();
    auto qRate = ext::make_shared<SimpleQuote>(0.0);
    auto rRate = ext::make_shared<SimpleQuote>(0.1);
    auto vol = ext::make_shared<SimpleQuote>(0.25);

    Handle<Quote> underlying(spot);
    Handle<YieldTermStructure> dividendTS(flatRate(today, qRate, dc));
    Handle<YieldTermStructure> riskFreeTS(flatRate(today, rRate, dc));
    Handle<BlackVolTermStructure> blackVolTS(flatVol(today, vol, dc));

    const auto process = ext::make_shared<BlackScholesMertonProcess>(underlying, dividendTS, riskFreeTS, blackVolTS);
    auto engine = ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process);

    TestCase cases[] = {
        {95.0, 90.0, 1, 1.5551},   {95.0, 95.0, 1, 2.0589},   {90.0, 95.0, 1, 4.4512},   {99.0, 90.0, 1, 0.3404},

        {95.0, 90.0, 90, 2.4181},  {95.0, 95.0, 90, 3.2257},  {90.0, 95.0, 90, 5.0624},  {99.0, 90.0, 90, 1.5992},

        {95.0, 90.0, 180, 3.0021}, {95.0, 95.0, 180, 4.0617}, {90.0, 95.0, 180, 5.7960}, {99.0, 90.0, 180, 2.1903},

        {95.0, 90.0, 270, 3.4194}, {95.0, 95.0, 270, 4.7362}, {90.0, 95.0, 270, 6.4370}, {99.0, 90.0, 270, 2.6025},

        {95.0, 90.0, 359, 3.5965}, {95.0, 95.0, 359, 5.1865}, {90.0, 95.0, 359, 6.8782}, {99.0, 90.0, 359, 2.7759}};

    for (auto& i : cases)
    {
        Date coverEventDate = today + i.days;
        auto payoff = ext::make_shared<PlainVanillaPayoff>(type, i.strike);
        PartialTimeBarrierOption option(Barrier::UpOut, PartialBarrier::EndB1, barrier, rebate, coverEventDate, payoff,
                                        exercise);
        option.setPricingEngine(engine);

        spot->setValue(i.underlying);
        Real calculated = option.NPV();
        Real expected = i.result;
        Real error = std::fabs(calculated - expected);
        Real tolerance = 1e-4;
        if (error > tolerance)
            REPORT_FAILURE("value", Barrier::UpOut, barrier, rebate, payoff, exercise, i.underlying, 0.0, 0.1, today,
                           0.25, expected, calculated, error, tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testPutCallSymmetry)
{
    BOOST_TEST_MESSAGE("Testing put-call symmetry for the partial-time barrier option...");

    Date today = Settings::instance().evaluationDate();

    struct PutCallSymmetryTestCase
    {
        Real callStrike;
        Real callBarrier;
        Barrier::Type callType;
        Real putStrike;
        Real putBarrier;
        Integer days;
        Barrier::Type putType;
    };

    PutCallSymmetryTestCase cases[] = {
        {105.2631, 95.2380, Barrier::DownOut, 95.0, 105.0, 1, Barrier::UpOut},
        {105.2631, 95.2380, Barrier::DownOut, 95.0, 105.0, 90, Barrier::UpOut},
        {105.2631, 95.2380, Barrier::DownOut, 95.0, 105.0, 180, Barrier::UpOut},
        {105.2631, 95.2380, Barrier::DownOut, 95.0, 105.0, 270, Barrier::UpOut},
        {105.2631, 95.2380, Barrier::DownOut, 95.0, 105.0, 359, Barrier::UpOut},

        {110.0, 120.0, Barrier::UpOut, 90.9090, 83.3333, 1, Barrier::DownOut},
        {110.0, 120.0, Barrier::UpOut, 90.9090, 83.3333, 90, Barrier::DownOut},
        {110.0, 120.0, Barrier::UpOut, 90.9090, 83.3333, 180, Barrier::DownOut},
        {110.0, 120.0, Barrier::UpOut, 90.9090, 83.3333, 270, Barrier::DownOut},
        {110.0, 120.0, Barrier::UpOut, 90.9090, 83.3333, 359, Barrier::DownOut},
    };

    DayCounter dc = Actual360();
    Date maturity = today + 360;
    auto exercise = ext::make_shared<EuropeanExercise>(maturity);
    Real r = 0.01;
    Real rebate = 0.0;
    Real spotPrice = 100.0;

    auto spot = ext::make_shared<SimpleQuote>();
    auto qRate = ext::make_shared<SimpleQuote>(0.0);
    auto rRate = ext::make_shared<SimpleQuote>(r);
    auto vol = ext::make_shared<SimpleQuote>(0.25);

    Handle<Quote> underlying(spot);
    Handle<YieldTermStructure> dividendTSCall(flatRate(today, qRate, dc));
    Handle<YieldTermStructure> riskFreeTSCall(flatRate(today, rRate, dc));
    Handle<YieldTermStructure> dividendTSPut(flatRate(today, rRate, dc));
    Handle<YieldTermStructure> riskFreeTSPut(flatRate(today, qRate, dc));
    Handle<BlackVolTermStructure> blackVolTS(flatVol(today, vol, dc));

    const auto callProcess =
        ext::make_shared<BlackScholesMertonProcess>(underlying, dividendTSCall, riskFreeTSCall, blackVolTS);
    const auto putProcess =
        ext::make_shared<BlackScholesMertonProcess>(underlying, dividendTSPut, riskFreeTSPut, blackVolTS);
    auto callEngine = ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(callProcess);
    auto putEngine = ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(putProcess);

    for (auto& i : cases)
    {
        Date coverEventDate = today + i.days;
        auto putPayoff = ext::make_shared<PlainVanillaPayoff>(Option::Put, i.putStrike);
        auto callPayoff = ext::make_shared<PlainVanillaPayoff>(Option::Call, i.callStrike);
        PartialTimeBarrierOption putOption(i.putType, PartialBarrier::EndB1, i.putBarrier, rebate, coverEventDate,
                                           putPayoff, exercise);
        putOption.setPricingEngine(putEngine);
        PartialTimeBarrierOption callOption(i.callType, PartialBarrier::EndB1, i.callBarrier, rebate, coverEventDate,
                                            callPayoff, exercise);
        callOption.setPricingEngine(callEngine);

        spot->setValue(spotPrice);
        Real putValue = putOption.NPV();
        Real callValue = callOption.NPV();
        Real callAmount = (i.putStrike / spotPrice);
        Real error = std::fabs(putValue - callAmount * callValue);
        Real tolerance = 1e-4;
        if (error > tolerance)
            BOOST_ERROR("Failed to reproduce the put-call symmetry for the partial-time barrier options "
                        << "\n    error:      " << error);
    }
}

BOOST_AUTO_TEST_CASE(testInOutParity)
{
    BOOST_TEST_MESSAGE("Testing in-out parity for partial-time barrier options...");

    const Date today(1, January, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();
    const ext::shared_ptr<EuropeanExercise> exercise = ext::make_shared<EuropeanExercise>(today + 182);
    const Date coverEventDate = today + 91;
    const auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, 0.04, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.08, dc)), Handle<BlackVolTermStructure>(flatVol(today, 0.25, dc)));
    const auto barrierEngine = ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process);
    const auto vanillaEngine = ext::make_shared<AnalyticEuropeanEngine>(process);

    const struct
    {
        Barrier::Type knockIn, knockOut;
        Real barrier;
    } cases[] = {{Barrier::DownIn, Barrier::DownOut, 85.0}, {Barrier::UpIn, Barrier::UpOut, 115.0}};

    for (auto type : {Option::Put, Option::Call})
    {
        for (Real strike : {80.0, 100.0, 130.0})
        {
            const auto payoff = ext::make_shared<PlainVanillaPayoff>(type, strike);
            VanillaOption vanilla(payoff, exercise);
            vanilla.setPricingEngine(vanillaEngine);

            for (const auto& c : cases)
            {
                PartialTimeBarrierOption knockIn(c.knockIn, PartialBarrier::Start, c.barrier, 0.0, coverEventDate,
                                                 payoff, exercise);
                PartialTimeBarrierOption knockOut(c.knockOut, PartialBarrier::Start, c.barrier, 0.0, coverEventDate,
                                                  payoff, exercise);
                knockIn.setPricingEngine(barrierEngine);
                knockOut.setPricingEngine(barrierEngine);

                BOOST_TEST_CONTEXT(c.knockIn << " at " << c.barrier << ", " << type << " at " << strike)
                {
                    BOOST_CHECK_SMALL(knockIn.NPV() + knockOut.NPV() - vanilla.NPV(), 1.0e-10);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testAnalyticEngineNearZeroD2)
{
    BOOST_TEST_MESSAGE("Testing that partial-time barrier prices stay finite with d2 near zero...");

    const Date today(1, January, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();
    const Integer pillars[] = {91, 182, 365, 730, 1095};
    const Rate rates[] = {0.01, 0.03, 0.05, 0.02, 0.04};
    const Volatility instantVols[] = {0.30, 0.22, 0.20, 0.25, 0.20};
    std::vector<Date> dates = {today}, volDates;
    std::vector<Rate> forwards = {rates[0]};
    std::vector<Rate> yields = {rates[0] - 0.5 * instantVols[0] * instantVols[0]};
    std::vector<Volatility> blackVols;
    Real variance = 0.0;
    Time previous = 0.0;
    for (Size i = 0; i < std::size(pillars); ++i)
    {
        const Date date = today + pillars[i];
        const Time t = dc.yearFraction(today, date);
        variance += instantVols[i] * instantVols[i] * (t - previous);
        previous = t;
        dates.push_back(date);
        volDates.push_back(date);
        forwards.push_back(rates[i]);
        // The carry is half the variance rate, so that d2 cancels to near zero.
        yields.push_back(rates[i] - 0.5 * instantVols[i] * instantVols[i]);
        blackVols.push_back(std::sqrt(variance / t));
    }
    auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)),
        Handle<YieldTermStructure>(ext::make_shared<ForwardCurve>(dates, yields, dc)),
        Handle<YieldTermStructure>(ext::make_shared<ForwardCurve>(dates, forwards, dc)),
        Handle<BlackVolTermStructure>(ext::make_shared<BlackVarianceCurve>(today, volDates, blackVols, dc, false)));
    auto engine = ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process);
    auto payoff = ext::make_shared<PlainVanillaPayoff>(Option::Call, 100.0);
    auto exercise = ext::make_shared<EuropeanExercise>(today + 548);

    for (Barrier::Type type : {Barrier::DownOut, Barrier::DownIn})
    {
        for (PartialBarrier::Range range : {PartialBarrier::Start, PartialBarrier::EndB1})
        {
            if (type == Barrier::DownIn && range == PartialBarrier::EndB1)
                continue;
            PartialTimeBarrierOption option(type, range, 85.0, 0.0, today + 274, payoff, exercise);
            option.setPricingEngine(engine);
            BOOST_TEST_CONTEXT("type " << type << ", range " << Integer(range))
            {
                BOOST_CHECK(std::isfinite(option.NPV()));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testAnalyticEngineAgainstDefinition)
{
    BOOST_TEST_MESSAGE("Testing partial-time barrier options against their definition...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // Values from the definition, integrated in mpmath at 40 digits by conditioning on the spot at the cover date: the
    // no-hit density on [0, t1] times the vanilla from there for Start, and the density of the spot times a
    // single-barrier knock-out on [t1, T] for EndB1 and EndB2. With Drezner's six-decimal bivariate normal, the
    // engine missed them by 1e-5 to 1.2e-4.
    const struct
    {
        Option::Type type;
        Barrier::Type barrierType;
        PartialBarrier::Range range;
        Real strike, barrier;
        Rate r, q;
        Volatility vol;
        Integer coverDays, days;
        Real value;
    } cases[] = {
        {Option::Call, Barrier::UpIn, PartialBarrier::Start, 110, 110, 0.03, 0.03, 0.10, 46, 183, 0.01885093984689},
        {Option::Call, Barrier::UpOut, PartialBarrier::Start, 110, 120, 0.05, 0.01, 0.25, 182, 365, 2.219124627834},
        {Option::Put, Barrier::DownOut, PartialBarrier::Start, 110, 95, 0.02, 0.04, 0.10, 91, 365, 6.703830930539},
        {Option::Put, Barrier::DownIn, PartialBarrier::Start, 90, 90, 0.02, 0.04, 0.10, 91, 365, 0.1758179450867},
        {Option::Call, Barrier::UpOut, PartialBarrier::EndB1, 90, 120, 0.05, 0.01, 0.25, 182, 365, 6.274896049660},
        {Option::Put, Barrier::UpOut, PartialBarrier::EndB1, 110, 105, 0.05, 0.01, 0.25, 182, 365, 9.328861863291},
        {Option::Call, Barrier::DownOut, PartialBarrier::EndB2, 90, 95, 0.05, 0.01, 0.25, 182, 365, 13.11315486649},
        {Option::Put, Barrier::UpOut, PartialBarrier::EndB2, 110, 105, 0.05, 0.01, 0.25, 182, 365, 9.322155342781}};

    for (const auto& c : cases)
    {
        const auto process = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q, dc)),
            Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
            Handle<BlackVolTermStructure>(flatVol(today, c.vol, dc)));
        PartialTimeBarrierOption option(c.barrierType, c.range, c.barrier, 0.0, today + c.coverDays,
                                        ext::make_shared<PlainVanillaPayoff>(c.type, c.strike),
                                        ext::make_shared<EuropeanExercise>(today + c.days));
        option.setPricingEngine(ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process));

        BOOST_TEST_CONTEXT(c.type << " " << c.barrierType << " at " << c.barrier << ", range " << Integer(c.range)
                                  << ", strike " << c.strike << ", r " << c.r << ", q " << c.q << ", vol " << c.vol)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, 1.0e-10);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAnalyticEngineAtZeroVolatility)
{
    BOOST_TEST_MESSAGE("Testing partial-time barrier options at zero volatility...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // The underlying follows its forward, S exp((r - q) t): from a spot of 100, 102.01 at the cover date and 104.08 at
    // expiry where r is 5% and q 1%, and 98.03 and 96.08 the other way round. A knock-out that isn't hit, or a knock-in
    // that is, pays the discounted payoff at the forward.
    const struct
    {
        Option::Type type;
        Barrier::Type barrierType;
        PartialBarrier::Range range;
        Real spot, strike, barrier;
        Rate r, q;
        Real value;
    } cases[] = {
        {Option::Call, Barrier::UpOut, PartialBarrier::Start, 100, 95, 103, 0.05, 0.01, 8.638188047348976},   // not hit
        {Option::Call, Barrier::UpOut, PartialBarrier::Start, 100, 95, 101, 0.05, 0.01, 0.0},                 // hit
        {Option::Call, Barrier::UpIn, PartialBarrier::Start, 100, 95, 101, 0.05, 0.01, 8.638188047348976},    // hit
        {Option::Call, Barrier::UpIn, PartialBarrier::Start, 100, 95, 103, 0.05, 0.01, 0.0},                  // not hit
        {Option::Put, Barrier::DownOut, PartialBarrier::Start, 100, 105, 99, 0.05, 0.01, 0.8741061976581647}, // not hit
        {Option::Put, Barrier::DownOut, PartialBarrier::Start, 100, 105, 99, 0.01, 0.05, 0.0},                // hit
        {Option::Put, Barrier::DownIn, PartialBarrier::Start, 100, 105, 99, 0.01, 0.05, 8.83229009359125},    // hit
        {Option::Call, Barrier::DownOut, PartialBarrier::EndB1, 100, 95, 105, 0.05, 0.01, 8.638188047348976}, // below
        {Option::Call, Barrier::UpOut, PartialBarrier::EndB1, 100, 95, 103, 0.05, 0.01, 0.0},                 // crosses
        {Option::Put, Barrier::DownOut, PartialBarrier::EndB1, 100, 105, 97, 0.01, 0.05, 0.0},                // crosses
        {Option::Call, Barrier::DownOut, PartialBarrier::EndB2, 100, 95, 105, 0.05, 0.01, 0.0},               // below
        {Option::Call, Barrier::DownOut, PartialBarrier::EndB2, 100, 95, 101, 0.05, 0.01, 8.638188047348976}, // not hit
        {Option::Put, Barrier::UpOut, PartialBarrier::EndB2, 100, 110, 105, 0.05, 0.01, 5.630253320161735},   // not hit
        {Option::Put, Barrier::UpOut, PartialBarrier::EndB2, 100, 110, 103, 0.05, 0.01, 0.0},                 // hit
        // A spot on the barrier hits it. A put's barrier is reflected in the spot, and must stay on it there, though
        // 101.3 * 101.3 / 101.3 and 7.7 * 7.7 / 7.7 aren't 101.3 and 7.7 in floating point.
        {Option::Call, Barrier::DownOut, PartialBarrier::Start, 100, 95, 100, 0.05, 0.01, 0.0},
        {Option::Put, Barrier::UpOut, PartialBarrier::Start, 101.3, 105, 101.3, 0.01, 0.05, 0.0},
        {Option::Put, Barrier::UpIn, PartialBarrier::Start, 101.3, 105, 101.3, 0.01, 0.05, 7.5956918417403285},
        {Option::Put, Barrier::DownOut, PartialBarrier::Start, 7.7, 9, 7.7, 0.05, 0.01, 0.0},
        {Option::Put, Barrier::DownIn, PartialBarrier::Start, 7.7, 9, 7.7, 0.05, 0.01, 0.9376811006378315}};

    for (const auto& c : cases)
    {
        const auto process = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(c.spot)), Handle<YieldTermStructure>(flatRate(today, c.q, dc)),
            Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
            Handle<BlackVolTermStructure>(flatVol(today, 0.0, dc)));
        PartialTimeBarrierOption option(c.barrierType, c.range, c.barrier, 0.0, today + 182,
                                        ext::make_shared<PlainVanillaPayoff>(c.type, c.strike),
                                        ext::make_shared<EuropeanExercise>(today + 365));
        option.setPricingEngine(ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process));

        BOOST_TEST_CONTEXT(c.type << " " << c.barrierType << " at " << c.barrier << ", range " << Integer(c.range)
                                  << ", spot " << c.spot << ", strike " << c.strike << ", r " << c.r << ", q " << c.q)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, 1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testPutWithStrikeOnBarrier)
{
    BOOST_TEST_MESSAGE("Testing partial-time puts with the strike on the barrier...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // A put is priced as a call, with its strike and barrier reflected in the spot. EndB2 implements only call strikes
    // below the barrier, so a put with its strike on the barrier is a case it doesn't implement, whatever the spot.
    // Reflected by different expressions, a strike and barrier of 60 at a spot of 50 came apart, and the puts priced.
    const auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(50.0)), Handle<YieldTermStructure>(flatRate(today, 0.01, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.05, dc)), Handle<BlackVolTermStructure>(flatVol(today, 0.20, dc)));
    for (Barrier::Type barrierType : {Barrier::DownOut, Barrier::UpOut})
    {
        PartialTimeBarrierOption option(barrierType, PartialBarrier::EndB2, 60.0, 0.0, today + 182,
                                        ext::make_shared<PlainVanillaPayoff>(Option::Put, 60.0),
                                        ext::make_shared<EuropeanExercise>(today + 365));
        option.setPricingEngine(ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process));
        BOOST_TEST_CONTEXT(barrierType)
        {
            BOOST_CHECK_EXCEPTION(option.NPV(), Error, ExpectedErrorMessage("not implemented for OutEnd B2 type"));
        }
    }
}

BOOST_AUTO_TEST_CASE(testSpotPastBarrier)
{
    BOOST_TEST_MESSAGE("Testing partial-time barrier options with the spot past the barrier...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    const auto vol = ext::make_shared<SimpleQuote>(0.20);
    const auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, 0.01, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.05, dc)), Handle<BlackVolTermStructure>(flatVol(today, vol, dc)));
    const auto engine = ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process);
    auto price =
        [&](Option::Type type, Barrier::Type barrierType, PartialBarrier::Range range, Real strike, Real barrier)
    {
        PartialTimeBarrierOption option(barrierType, range, barrier, 0.0, today + 182,
                                        ext::make_shared<PlainVanillaPayoff>(type, strike),
                                        ext::make_shared<EuropeanExercise>(today + 365));
        option.setPricingEngine(engine);
        return option.NPV();
    };

    // A Start window watches the spot from today, so with the spot past the barrier a knock-out is already dead and a
    // knock-in already in. The engine refuses it at any volatility, as AnalyticBarrierEngine does; at 20% the formulas
    // gave -9.999 for the first case and 22.63 for the second. A spot on the barrier hits it, so a knock-out there is
    // worth 0.
    const struct
    {
        Option::Type type;
        Barrier::Type barrierType;
        Real strike, barrier;
    } startCases[] = {{Option::Call, Barrier::DownOut, 95, 105}, {Option::Call, Barrier::DownIn, 95, 105},
                      {Option::Call, Barrier::UpOut, 95, 95},    {Option::Call, Barrier::UpIn, 95, 95},
                      {Option::Put, Barrier::DownOut, 105, 105}, {Option::Put, Barrier::UpOut, 105, 95}};
    for (Volatility v : {0.20, 0.0})
    {
        vol->setValue(v);
        for (const auto& c : startCases)
        {
            BOOST_TEST_CONTEXT(c.type << " " << c.barrierType << " at " << c.barrier << ", strike " << c.strike
                                      << ", volatility " << v)
            {
                BOOST_CHECK_EXCEPTION(price(c.type, c.barrierType, PartialBarrier::Start, c.strike, c.barrier), Error,
                                      ExpectedErrorMessage("barrier touched"));
                if (c.barrierType == Barrier::DownOut || c.barrierType == Barrier::UpOut)
                    BOOST_CHECK_SMALL(price(c.type, c.barrierType, PartialBarrier::Start, c.strike, 100.0), 1.0e-12);
            }
        }
    }

    // The End windows start watching at the cover date, so today's spot can be on either side of the barrier. Values
    // from the definition, as in testAnalyticEngineAgainstDefinition. The two EndB2 options have today's spot where,
    // from the cover date, it would knock them out.
    vol->setValue(0.20);
    const struct
    {
        Option::Type type;
        Barrier::Type barrierType;
        PartialBarrier::Range range;
        Real strike, barrier, value;
    } endCases[] = {{Option::Call, Barrier::DownOut, PartialBarrier::EndB1, 95, 105, 6.715029217737929},
                    {Option::Call, Barrier::UpOut, PartialBarrier::EndB1, 95, 95, 10.32745322844149},
                    {Option::Put, Barrier::DownOut, PartialBarrier::EndB1, 105, 105, 6.396213073906356},
                    {Option::Put, Barrier::UpOut, PartialBarrier::EndB1, 105, 95, 3.780991933731649},
                    {Option::Call, Barrier::DownOut, PartialBarrier::EndB2, 95, 105, 6.563376024817099},
                    {Option::Put, Barrier::UpOut, PartialBarrier::EndB2, 105, 95, 3.620020939775625}};
    for (const auto& c : endCases)
    {
        BOOST_TEST_CONTEXT(c.type << " " << c.barrierType << " at " << c.barrier << ", range " << Integer(c.range)
                                  << ", strike " << c.strike)
        {
            BOOST_CHECK_SMALL(price(c.type, c.barrierType, c.range, c.strike, c.barrier) - c.value, 1.0e-10);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCoverDateInThePast)
{
    BOOST_TEST_MESSAGE("Testing that partial-time barrier options refuse a cover date in the past...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // Part of the window would be in the past, where the engine can't know the path. At 20% volatility the engine threw
    // on the volatility's negative time; at zero volatility it priced the options from the forward, read backwards.
    const auto vol = ext::make_shared<SimpleQuote>(0.20);
    const auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, 0.01, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.05, dc)), Handle<BlackVolTermStructure>(flatVol(today, vol, dc)));
    for (Volatility v : {0.20, 0.0})
    {
        vol->setValue(v);
        for (PartialBarrier::Range range : {PartialBarrier::Start, PartialBarrier::EndB1, PartialBarrier::EndB2})
        {
            PartialTimeBarrierOption option(Barrier::DownOut, range, 95.0, 0.0, today - 30,
                                            ext::make_shared<PlainVanillaPayoff>(Option::Call, 90.0),
                                            ext::make_shared<EuropeanExercise>(today + 365));
            option.setPricingEngine(ext::make_shared<AnalyticPartialTimeBarrierOptionEngine>(process));
            BOOST_TEST_CONTEXT("range " << Integer(range) << ", volatility " << v)
            {
                BOOST_CHECK_EXCEPTION(option.NPV(), Error, ExpectedErrorMessage("cover event date has passed"));
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
