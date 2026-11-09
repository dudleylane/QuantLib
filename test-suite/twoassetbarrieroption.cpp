/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 StatPro Italia srl

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
#include <ql/instruments/twoassetbarrieroption.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/pricingengines/barrier/analytictwoassetbarrierengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(TwoAssetBarrierOptionTests)

struct OptionData
{
    Barrier::Type barrierType;
    Option::Type type;
    Real barrier;
    Real strike;
    Real s1;       // spot
    Rate q1;       // dividend
    Volatility v1; // volatility
    Real s2;
    Rate q2;
    Volatility v2;
    Real correlation;
    Rate r;      // risk-free rate
    Real result; // result
};


BOOST_AUTO_TEST_CASE(testHaugValues)
{

    BOOST_TEST_MESSAGE("Testing two-asset barrier options against Haug's values...");

    OptionData values[] = {
        /* The data below are from
          "Option pricing formulas", E.G. Haug, McGraw-Hill 1998
        */
        {Barrier::DownOut, Option::Call, 95, 90, 100.0, 0.0, 0.2, 100.0, 0.0, 0.2, 0.5, 0.08, 6.6592},
        {Barrier::UpOut, Option::Call, 105, 90, 100.0, 0.0, 0.2, 100.0, 0.0, 0.2, -0.5, 0.08, 4.6670},
        {Barrier::DownOut, Option::Put, 95, 90, 100.0, 0.0, 0.2, 100.0, 0.0, 0.2, -0.5, 0.08, 0.6184},
        {Barrier::UpOut, Option::Put, 105, 100, 100.0, 0.0, 0.2, 100.0, 0.0, 0.2, 0.0, 0.08, 0.8246}};

    DayCounter dc = Actual360();
    Calendar calendar = TARGET();
    Date today = Date::todaysDate();
    Date maturity = today + 180;
    ext::shared_ptr<Exercise> exercise(new EuropeanExercise(maturity));

    ext::shared_ptr<SimpleQuote> r(new SimpleQuote);
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, r, dc);

    ext::shared_ptr<SimpleQuote> s1(new SimpleQuote);
    ext::shared_ptr<SimpleQuote> q1(new SimpleQuote);
    ext::shared_ptr<YieldTermStructure> qTS1 = flatRate(today, q1, dc);
    ext::shared_ptr<SimpleQuote> vol1(new SimpleQuote);
    ext::shared_ptr<BlackVolTermStructure> volTS1 = flatVol(today, vol1, dc);

    ext::shared_ptr<BlackScholesMertonProcess> process1(
        new BlackScholesMertonProcess(Handle<Quote>(s1), Handle<YieldTermStructure>(qTS1),
                                      Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS1)));

    ext::shared_ptr<SimpleQuote> s2(new SimpleQuote);
    ext::shared_ptr<SimpleQuote> q2(new SimpleQuote);
    ext::shared_ptr<YieldTermStructure> qTS2 = flatRate(today, q2, dc);
    ext::shared_ptr<SimpleQuote> vol2(new SimpleQuote);
    ext::shared_ptr<BlackVolTermStructure> volTS2 = flatVol(today, vol2, dc);

    ext::shared_ptr<BlackScholesMertonProcess> process2(
        new BlackScholesMertonProcess(Handle<Quote>(s2), Handle<YieldTermStructure>(qTS2),
                                      Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS2)));

    ext::shared_ptr<SimpleQuote> rho(new SimpleQuote);

    ext::shared_ptr<PricingEngine> engine(new AnalyticTwoAssetBarrierEngine(process1, process2, Handle<Quote>(rho)));

    for (auto& value : values)
    {

        s1->setValue(value.s1);
        q1->setValue(value.q1);
        vol1->setValue(value.v1);

        s2->setValue(value.s2);
        q2->setValue(value.q2);
        vol2->setValue(value.v2);

        rho->setValue(value.correlation);

        r->setValue(value.r);

        ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(value.type, value.strike));

        TwoAssetBarrierOption barrierOption(value.barrierType, value.barrier, payoff, exercise);
        barrierOption.setPricingEngine(engine);

        Real calculated = barrierOption.NPV();
        Real expected = value.result;
        Real error = std::fabs(calculated - expected);
        Real tolerance = 4.0e-3;
        if (error > tolerance)
        {
            BOOST_ERROR("failed to reproduce expected price"
                        << "\n    expected:   " << expected << "\n    calculated: " << calculated
                        << "\n    tolerance:  " << tolerance << "\n    error:      " << error);
        }
    }
}

BOOST_AUTO_TEST_CASE(testInOutParityWithDividends)
{
    BOOST_TEST_MESSAGE("Testing two-asset barrier in-out parity with dividends...");

    const DayCounter dc = Actual360();
    const Date today(15, January, 2026);
    Settings::instance().evaluationDate() = today;
    const ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(today + 365);

    const auto q1 = ext::make_shared<SimpleQuote>();
    const auto rho = ext::make_shared<SimpleQuote>();
    const Handle<YieldTermStructure> rTS(flatRate(today, 0.05, dc));

    const auto process1 = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, q1, dc)), rTS,
        Handle<BlackVolTermStructure>(flatVol(today, 0.2, dc)));
    const auto process2 = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, 0.02, dc)), rTS,
        Handle<BlackVolTermStructure>(flatVol(today, 0.3, dc)));

    const auto engine = ext::make_shared<AnalyticTwoAssetBarrierEngine>(process1, process2, Handle<Quote>(rho));
    const auto europeanEngine = ext::make_shared<AnalyticEuropeanEngine>(process1);

    const struct
    {
        Barrier::Type in, out;
        Real barrier;
    } barriers[] = {{Barrier::DownIn, Barrier::DownOut, 90.0}, {Barrier::UpIn, Barrier::UpOut, 110.0}};

    for (const auto& b : barriers)
    {
        for (auto type : {Option::Call, Option::Put})
        {
            for (Real strike : {90.0, 110.0})
            {
                for (Rate q : {0.0, 0.04})
                {
                    for (Real correlation : {-0.5, 0.5})
                    {
                        q1->setValue(q);
                        rho->setValue(correlation);
                        const auto payoff = ext::make_shared<PlainVanillaPayoff>(type, strike);

                        TwoAssetBarrierOption knockIn(b.in, b.barrier, payoff, exercise);
                        knockIn.setPricingEngine(engine);
                        TwoAssetBarrierOption knockOut(b.out, b.barrier, payoff, exercise);
                        knockOut.setPricingEngine(engine);
                        VanillaOption vanilla(payoff, exercise);
                        vanilla.setPricingEngine(europeanEngine);

                        BOOST_TEST_CONTEXT(b.in << " at " << b.barrier << ", " << type << " at " << strike
                                                << ", dividend yield " << q << ", correlation " << correlation)
                        {
                            BOOST_CHECK_SMALL(knockIn.NPV() + knockOut.NPV() - vanilla.NPV(), 1.0e-10);

                            // With a barrier out of reach, the knock-out is the vanilla option on its own.
                            TwoAssetBarrierOption farOut(b.out, b.in == Barrier::DownIn ? 1.0 : 1.0e4, payoff,
                                                         exercise);
                            farOut.setPricingEngine(engine);
                            BOOST_CHECK_SMALL(farOut.NPV() - vanilla.NPV(), 1.0e-8);
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testAgainstDefinition)
{
    BOOST_TEST_MESSAGE("Testing two-asset barrier options against their definition...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // Values from the definition, integrated in mpmath at 40 digits over the second asset's value at expiry: the
    // probability that its Brownian bridge misses the barrier, times the first asset's conditional Black-Scholes
    // value. With Drezner's six-decimal bivariate normal, the engine missed them by 1.5e-6 to 3.3e-5.
    const struct
    {
        Option::Type type;
        Barrier::Type barrierType;
        Real strike, barrier;
        Rate r, q1, q2;
        Volatility vol1, vol2;
        Real rho;
        Integer days;
        Real value;
    } cases[] = {{Option::Call, Barrier::DownOut, 95, 80, 0.05, 0.01, 0.02, 0.2, 0.3, 0.5, 365, 9.003144146795},
                 {Option::Call, Barrier::UpOut, 95, 120, 0.05, 0.01, 0.02, 0.2, 0.3, -0.5, 365, 8.288752087467},
                 {Option::Put, Barrier::DownOut, 105, 80, 0.05, 0.01, 0.02, 0.2, 0.3, 0.5, 365, 2.732187961970},
                 {Option::Put, Barrier::UpIn, 105, 120, 0.05, 0.01, 0.02, 0.25, 0.2, 0.7, 183, 0.3773214516479},
                 {Option::Call, Barrier::DownIn, 95, 90, 0.03, 0.0, 0.03, 0.3, 0.15, -0.8, 730, 20.32738570722}};

    for (const auto& c : cases)
    {
        const Handle<YieldTermStructure> rTS(flatRate(today, c.r, dc));
        const auto process1 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q1, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol1, dc)));
        const auto process2 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q2, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol2, dc)));
        TwoAssetBarrierOption option(c.barrierType, c.barrier, ext::make_shared<PlainVanillaPayoff>(c.type, c.strike),
                                     ext::make_shared<EuropeanExercise>(today + c.days));
        option.setPricingEngine(ext::make_shared<AnalyticTwoAssetBarrierEngine>(
            process1, process2, Handle<Quote>(ext::make_shared<SimpleQuote>(c.rho))));

        BOOST_TEST_CONTEXT(c.type << " " << c.barrierType << " at " << c.barrier << ", strike " << c.strike
                                  << ", correlation " << c.rho)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, 1.0e-10);
        }
    }
}

BOOST_AUTO_TEST_CASE(testZeroVariance)
{
    BOOST_TEST_MESSAGE("Testing two-asset barrier options with no variance in the barrier asset...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    // so that an option expiring today is priced
    Settings::instance().includeReferenceDateEvents() = true;
    const DayCounter dc = Actual365Fixed();

    // The second asset has no volatility, or the option expires today. Either way it follows its forward,
    // 100 exp((r - q2) t), and the first asset's payoff is worth its Black-Scholes value.
    const struct
    {
        Option::Type type;
        Barrier::Type barrierType;
        Real strike, barrier;
        Rate r, q1, q2;
        Volatility vol1, vol2;
        Real rho;
        Integer days;
        Real value;
    } cases[] = {
        {Option::Call, Barrier::DownOut, 95, 95, 0.05, 0.01, 0.01, 0.2, 0.0, 0.5, 365, 12.630520058070893}, // not hit
        {Option::Call, Barrier::DownOut, 95, 99, 0.01, 0.0, 0.05, 0.2, 0.0, -0.5, 365, 0.0},                // hit
        {Option::Call, Barrier::DownIn, 95, 99, 0.01, 0.0, 0.05, 0.2, 0.0, -0.5, 365, 11.060209946964035},  // hit
        {Option::Put, Barrier::UpOut, 105, 103, 0.05, 0.01, 0.01, 0.2, 0.0, 0.9, 365, 0.0},                 // hit
        {Option::Put, Barrier::UpOut, 105, 105, 0.05, 0.01, 0.01, 0.2, 0.0, 0.9, 365, 8.365799352666059},   // not hit
        {Option::Put, Barrier::UpIn, 105, 105, 0.05, 0.01, 0.01, 0.2, 0.0, 0.9, 365, 0.0},                  // not hit
        {Option::Call, Barrier::DownOut, 95, 100, 0.05, 0.01, 0.01, 0.2, 0.0, 0.5, 365, 0.0},             // spot on it
        {Option::Call, Barrier::UpOut, 95, 105, 0.05, 0.01, 0.01, 0.0, 0.0, 0.5, 365, 8.638188047348976}, // not hit
        {Option::Call, Barrier::DownOut, 95, 80, 0.05, 0.01, 0.02, 0.2, 0.3, 0.5, 0, 5.0},                // not hit
        {Option::Call, Barrier::DownIn, 95, 80, 0.05, 0.01, 0.02, 0.2, 0.3, 0.5, 0, 0.0},                 // not hit
        {Option::Put, Barrier::UpIn, 105, 100, 0.05, 0.01, 0.02, 0.2, 0.3, 0.5, 0, 5.0},                  // spot on it
        {Option::Put, Barrier::UpOut, 105, 100, 0.05, 0.01, 0.02, 0.2, 0.3, 0.5, 0, 0.0}};                // spot on it

    for (const auto& c : cases)
    {
        const Handle<YieldTermStructure> rTS(flatRate(today, c.r, dc));
        const auto process1 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q1, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol1, dc)));
        const auto process2 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q2, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol2, dc)));
        TwoAssetBarrierOption option(c.barrierType, c.barrier, ext::make_shared<PlainVanillaPayoff>(c.type, c.strike),
                                     ext::make_shared<EuropeanExercise>(today + c.days));
        option.setPricingEngine(ext::make_shared<AnalyticTwoAssetBarrierEngine>(
            process1, process2, Handle<Quote>(ext::make_shared<SimpleQuote>(c.rho))));

        BOOST_TEST_CONTEXT(c.type << " " << c.barrierType << " at " << c.barrier << ", strike " << c.strike
                                  << ", volatilities " << c.vol1 << " and " << c.vol2 << ", " << c.days << " days")
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, 1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing two-asset barrier options at low volatility...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // Values from the definition, as in testAgainstDefinition. The image terms multiply exponentials of terms that grow
    // as 1 / sigma2^2 by bivariate normals, and all these were NaN, where the exponential overflowed and the bivariate
    // normal underflowed.
    const struct
    {
        Option::Type type;
        Barrier::Type barrierType;
        Real strike, barrier;
        Rate r, q1, q2;
        Volatility vol1, vol2;
        Real rho;
        Integer days;
        Real value;
    } cases[] = {
        // the second asset's forward stays below the barrier, so these two are worth the vanilla; the first is #136's
        {Option::Call, Barrier::UpOut, 100, 120, 0.05, 0.0, 0.0, 0.005, 0.005, 0.5, 365, 4.8770575499285994},
        {Option::Call, Barrier::UpOut, 95, 120, 0.05, 0.01, 0.02, 1e-5, 1e-5, 0.5, 365, 8.6381880473489747},
        {Option::Call, Barrier::UpOut, 105, 120, 0.05, 0.01, 0.02, 0.003, 0.003, 0.5, 365, 0.00014564398588913934},
        {Option::Call, Barrier::DownOut, 95, 80, 0.01, 0.05, 0.04, 0.003, 0.003, 0.5, 365, 1.0682138598098806},
        {Option::Put, Barrier::DownOut, 95, 80, 0.01, 0.05, 0.04, 0.003, 0.003, 0.5, 365, 5.6159094450606410e-6},
        {Option::Put, Barrier::UpOut, 105, 120, 0.05, 0.01, 0.02, 0.003, 0.003, -1.0, 365, 0.87425184164405448}};

    for (const auto& c : cases)
    {
        const Handle<YieldTermStructure> rTS(flatRate(today, c.r, dc));
        const auto process1 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q1, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol1, dc)));
        const auto process2 = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q2, dc)),
            rTS, Handle<BlackVolTermStructure>(flatVol(today, c.vol2, dc)));
        TwoAssetBarrierOption option(c.barrierType, c.barrier, ext::make_shared<PlainVanillaPayoff>(c.type, c.strike),
                                     ext::make_shared<EuropeanExercise>(today + c.days));
        option.setPricingEngine(ext::make_shared<AnalyticTwoAssetBarrierEngine>(
            process1, process2, Handle<Quote>(ext::make_shared<SimpleQuote>(c.rho))));

        BOOST_TEST_CONTEXT(c.type << " " << c.barrierType << " at " << c.barrier << ", strike " << c.strike
                                  << ", volatilities " << c.vol1 << " and " << c.vol2 << ", correlation " << c.rho)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, 1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
