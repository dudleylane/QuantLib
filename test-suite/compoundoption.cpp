/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2009 Dimitri Reiswich

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
#include <ql/instruments/compoundoption.hpp>
#include <ql/instruments/europeanoption.hpp>
#include <ql/pricingengines/exotic/analyticcompoundoptionengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CompoundOptionTests)

#undef REPORT_FAILURE
#define REPORT_FAILURE(greekName, payoffM, payoffD, exerciseM, exerciseD, s, q, r, today, v, expected, calculated, \
                       error, tolerance)                                                                           \
    BOOST_FAIL("\nmother option type:   "                                                                          \
               << payoffM->optionType() << "\ndaughter option type: " << payoffD->optionType()                     \
               << "\nspot value:           " << s << "\nstrike mother:        " << payoffM->strike()               \
               << "\nstrike daughter:      " << payoffD->strike() << "\ndividend yield:       " << io::rate(q)     \
               << "\nrisk-free rate:       " << io::rate(r) << "\nreference date:       " << today                 \
               << "\nmaturity mother:      " << exerciseM->lastDate() << "\nmaturity daughter:    "                \
               << exerciseD->lastDate() << "\nvolatility:           " << io::volatility(v) << "\n  expected "      \
               << greekName << ": " << expected << "\ncalculated " << greekName << ": " << calculated              \
               << "\nerror:                " << error << "\ntolerance:            " << tolerance);

struct CompoundOptionData
{
    Option::Type typeMother;
    Option::Type typeDaughter;
    Real strikeMother;
    Real strikeDaughter;
    Real s;         // spot
    Rate q;         // dividend
    Rate r;         // risk-free rate
    Time tMother;   // time to maturity
    Time tDaughter; // time to maturity
    Volatility v;   // volatility
    Real npv;       // expected result
    Real tol;       // tolerance
    Real delta;
    Real gamma;
    Real vega;
    Real theta;
};


BOOST_AUTO_TEST_CASE(testPutCallParity)
{

    BOOST_TEST_MESSAGE("Testing compound-option put-call parity...");

    // Test Put Call Parity for compound options.
    // Formula taken from: "Foreign Exchange Risk", Wystup, Risk 2002
    // Page 81, Equation 9.5


    CompoundOptionData values[] = {
        // type Mother, typeDaughter, strike Mother, strike Daughter,  spot,    q,    r,    t Mother, t Daughter,  vol
        {Option::Put, Option::Call, 50.0, 520.0, 500.0, 0.03, 0.08, 0.25, 0.5, 0.35},
        {Option::Call, Option::Call, 50.0, 520.0, 500.0, 0.03, 0.08, 0.25, 0.5, 0.35},
        {Option::Call, Option::Put, 50.0, 520.0, 500.0, 0.03, 0.08, 0.25, 0.5, 0.35},
        {Option::Call, Option::Call, 0.05, 1.14, 1.20, 0.0, 0.01, 0.5, 2.0, 0.11},
        {Option::Call, Option::Put, 0.05, 1.14, 1.20, 0.0, 0.01, 0.5, 2.0, 0.11},
        {Option::Call, Option::Call, 10.0, 122.0, 120.0, 0.06, 0.02, 0.1, 0.7, 0.22},
        {Option::Call, Option::Put, 10.0, 122.0, 120.0, 0.06, 0.02, 0.1, 0.7, 0.22},
        {Option::Call, Option::Call, 0.4, 8.2, 8.0, 0.05, 0.00, 2.0, 3.0, 0.08},
        {Option::Call, Option::Put, 0.4, 8.2, 8.0, 0.05, 0.00, 2.0, 3.0, 0.08},
        {Option::Call, Option::Call, 0.02, 1.6, 1.6, 0.013, 0.022, 0.45, 0.5, 0.17},
        {Option::Call, Option::Put, 0.02, 1.6, 1.6, 0.013, 0.022, 0.45, 0.5, 0.17},
    };

    Calendar calendar = TARGET();

    DayCounter dc = Actual360();
    Date todaysDate = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));

    ext::shared_ptr<YieldTermStructure> rTS(new FlatForward(0, NullCalendar(), Handle<Quote>(rRate), dc));

    ext::shared_ptr<YieldTermStructure> qTS(new FlatForward(0, NullCalendar(), Handle<Quote>(qRate), dc));

    ext::shared_ptr<BlackVolTermStructure> volTS(
        new BlackConstantVol(todaysDate, NullCalendar(), Handle<Quote>(vol), dc));

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoffMotherCall(new PlainVanillaPayoff(Option::Call, value.strikeMother));

        ext::shared_ptr<StrikedTypePayoff> payoffMotherPut(new PlainVanillaPayoff(Option::Put, value.strikeMother));

        ext::shared_ptr<StrikedTypePayoff> payoffDaughter(
            new PlainVanillaPayoff(value.typeDaughter, value.strikeDaughter));

        Date matDateMom = todaysDate + timeToDays(value.tMother);
        Date matDateDaughter = todaysDate + timeToDays(value.tDaughter);

        ext::shared_ptr<Exercise> exerciseCompound(new EuropeanExercise(matDateMom));
        ext::shared_ptr<Exercise> exerciseDaughter(new EuropeanExercise(matDateDaughter));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        CompoundOption compoundOptionCall(payoffMotherCall, exerciseCompound, payoffDaughter, exerciseDaughter);

        CompoundOption compoundOptionPut(payoffMotherPut, exerciseCompound, payoffDaughter, exerciseDaughter);

        VanillaOption vanillaOption(EuropeanOption(payoffDaughter, exerciseDaughter));

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));


        ext::shared_ptr<PricingEngine> engineCompound(new AnalyticCompoundOptionEngine(stochProcess));

        ext::shared_ptr<PricingEngine> engineEuropean(new AnalyticEuropeanEngine(stochProcess));

        compoundOptionCall.setPricingEngine(engineCompound);
        compoundOptionPut.setPricingEngine(engineCompound);
        vanillaOption.setPricingEngine(engineEuropean);

        Real discFact = rTS->discount(matDateMom);
        Real discStrike = value.strikeMother * discFact;

        Real calculated = compoundOptionCall.NPV() + discStrike - compoundOptionPut.NPV() - vanillaOption.NPV();

        Real expected = 0.0;
        Real error = std::abs(calculated - expected);
        Real tolerance = 1.0e-8;

        if (error > tolerance)
        {
            REPORT_FAILURE("put call parity", payoffMotherCall, payoffDaughter, exerciseCompound, exerciseDaughter,
                           value.s, value.q, value.r, todaysDate, value.v, value.delta, calculated, error, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testValues)
{

    BOOST_TEST_MESSAGE("Testing compound-option values and greeks...");

    CompoundOptionData values[] = {
        // type Mother, typeDaughter, strike Mother, strike Daughter,  spot,    q,    r,    t Mother, t Daughter,  vol,
        // value,    tol, delta, gamma, vega, theta
        // Tolerance is taken to be pretty high with 1.0e-3, since the price/theta is very sensitive with respect to
        // the implementation of the bivariate normal - which differs in the various implementations.
        // Option Value Taken from Haug 2007, Greeks from www.sitmo.com
        {Option::Put, Option::Call, 50.0, 520.0, 500.0, 0.03, 0.08, 0.25, 0.5, 0.35, 21.1965, 1.0e-3, -0.1966, 0.0007,
         -32.1241, -3.3837},
        //*********************************************************
        // Option Values and Greeks taken from www.sitmo.com
        {Option::Call, Option::Call, 50.0, 520.0, 500.0, 0.03, 0.08, 0.25, 0.5, 0.35, 17.5945, 1.0e-3, 0.3219, 0.0038,
         106.5185, -65.1614},
        {Option::Call, Option::Put, 50.0, 520.0, 500.0, 0.03, 0.08, 0.25, 0.5, 0.35, 18.7128, 1.0e-3, -0.2906, 0.0036,
         103.3856, -46.6982},
        {Option::Put, Option::Put, 50.0, 520.0, 500.0, 0.03, 0.08, 0.25, 0.5, 0.35, 15.2601, 1.0e-3, 0.1760, 0.0005,
         -35.2570, -10.1126},
        // type Mother, typeDaughter, strike Mother, strike Daughter,  spot,    q,    r,    t Mother, t Daughter,  vol,
        // value,    tol, delta, gamma, vega, theta
        {Option::Call, Option::Call, 0.05, 1.14, 1.20, 0.0, 0.01, 0.5, 2.0, 0.11, 0.0729, 1.0e-3, 0.6614, 2.5762,
         0.5812, -0.0297},
        {Option::Call, Option::Put, 0.05, 1.14, 1.20, 0.0, 0.01, 0.5, 2.0, 0.11, 0.0074, 1.0e-3, -0.1334, 1.9681,
         0.2933, -0.0155},
        {Option::Put, Option::Call, 0.05, 1.14, 1.20, 0.0, 0.01, 0.5, 2.0, 0.11, 0.0021, 1.0e-3, -0.0426, 0.7252,
         -0.0052, -0.0058},
        {Option::Put, Option::Put, 0.05, 1.14, 1.20, 0.0, 0.01, 0.5, 2.0, 0.11, 0.0192, 1.0e-3, 0.1626, 0.1171, -0.2931,
         -0.0028},
        // type Mother, typeDaughter, strike Mother, strike Daughter,  spot,    q,    r,    t Mother, t Daughter,  vol,
        // value,    tol, delta, gamma, vega, theta
        {Option::Call, Option::Call, 10.0, 122.0, 120.0, 0.06, 0.02, 0.1, 0.7, 0.22, 0.4419, 1.0e-3, 0.1049, 0.0195,
         11.3368, -6.2871},
        {Option::Call, Option::Put, 10.0, 122.0, 120.0, 0.06, 0.02, 0.1, 0.7, 0.22, 2.6112, 1.0e-3, -0.3618, 0.0337,
         28.4843, -13.4124},
        {Option::Put, Option::Call, 10.0, 122.0, 120.0, 0.06, 0.02, 0.1, 0.7, 0.22, 4.1616, 1.0e-3, -0.3174, 0.0024,
         -26.6403, -2.2720},
        {Option::Put, Option::Put, 10.0, 122.0, 120.0, 0.06, 0.02, 0.1, 0.7, 0.22, 1.0914, 1.0e-3, 0.1748, 0.0165,
         -9.4928, -4.8995},
        //*********************************************************
        //*********************************************************
        // Option Values and Greeks taken from mathfinance VBA implementation
        // type Mother, typeDaughter, strike Mother, strike Daughter,  spot,    q,    r,    t Mother, t Daughter,  vol,
        // value,    tol, delta, gamma, vega, theta
        {Option::Call, Option::Call, 0.4, 8.2, 8.0, 0.05, 0.00, 2.0, 3.0, 0.08, 0.0099, 1.0e-3, 0.0285, 0.0688, 0.7764,
         -0.0027},
        {Option::Call, Option::Put, 0.4, 8.2, 8.0, 0.05, 0.00, 2.0, 3.0, 0.08, 0.9826, 1.0e-3, -0.7224, 0.2158, 2.7279,
         -0.3332},
        {Option::Put, Option::Call, 0.4, 8.2, 8.0, 0.05, 0.00, 2.0, 3.0, 0.08, 0.3585, 1.0e-3, -0.0720, -0.0835,
         -1.5633, -0.0117},
        {Option::Put, Option::Put, 0.4, 8.2, 8.0, 0.05, 0.00, 2.0, 3.0, 0.08, 0.0168, 1.0e-3, 0.0378, 0.0635, 0.3882,
         0.0021},
        // type Mother, typeDaughter, strike Mother, strike Daughter,  spot,    q,    r,    t Mother, t Daughter,  vol,
        // value,    tol, delta, gamma, vega, theta
        {Option::Call, Option::Call, 0.02, 1.6, 1.6, 0.013, 0.022, 0.45, 0.5, 0.17, 0.0680, 1.0e-3, 0.4937, 2.1271,
         0.4418, -0.0843},
        {Option::Call, Option::Put, 0.02, 1.6, 1.6, 0.013, 0.022, 0.45, 0.5, 0.17, 0.0605, 1.0e-3, -0.4169, 2.0836,
         0.4330, -0.0697},
        {Option::Put, Option::Call, 0.02, 1.6, 1.6, 0.013, 0.022, 0.45, 0.5, 0.17, 0.0081, 1.0e-3, -0.0417, 0.0761,
         -0.0045, -0.0020},
        {Option::Put, Option::Put, 0.02, 1.6, 1.6, 0.013, 0.022, 0.45, 0.5, 0.17, 0.0078, 1.0e-3, 0.0413, 0.0326,
         -0.0133, -0.0016}};

    Calendar calendar = TARGET();

    DayCounter dc = Actual360();
    Date todaysDate = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));

    ext::shared_ptr<YieldTermStructure> rTS(new FlatForward(0, NullCalendar(), Handle<Quote>(rRate), dc));

    ext::shared_ptr<YieldTermStructure> qTS(new FlatForward(0, NullCalendar(), Handle<Quote>(qRate), dc));

    ext::shared_ptr<BlackVolTermStructure> volTS(
        new BlackConstantVol(todaysDate, NullCalendar(), Handle<Quote>(vol), dc));

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoffMother(new PlainVanillaPayoff(value.typeMother, value.strikeMother));

        ext::shared_ptr<StrikedTypePayoff> payoffDaughter(
            new PlainVanillaPayoff(value.typeDaughter, value.strikeDaughter));

        Date matDateMom = todaysDate + timeToDays(value.tMother);
        Date matDateDaughter = todaysDate + timeToDays(value.tDaughter);

        ext::shared_ptr<Exercise> exerciseMother(new EuropeanExercise(matDateMom));
        ext::shared_ptr<Exercise> exerciseDaughter(new EuropeanExercise(matDateDaughter));

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        CompoundOption compoundOption(payoffMother, exerciseMother, payoffDaughter, exerciseDaughter);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));

        ext::shared_ptr<PricingEngine> engineCompound(new AnalyticCompoundOptionEngine(stochProcess));

        compoundOption.setPricingEngine(engineCompound);

        Real calculated = compoundOption.NPV();
        Real error = std::fabs(calculated - value.npv); //-values[i].npv
        Real tolerance = value.tol;

        if (error > tolerance)
        {
            REPORT_FAILURE("value", payoffMother, payoffDaughter, exerciseMother, exerciseDaughter, value.s, value.q,
                           value.r, todaysDate, value.v, value.npv, calculated, error, tolerance);
        }

        calculated = compoundOption.delta();
        error = std::fabs(calculated - value.delta);
        tolerance = value.tol;

        if (error > tolerance)
        {
            REPORT_FAILURE("delta", payoffMother, payoffDaughter, exerciseMother, exerciseDaughter, value.s, value.q,
                           value.r, todaysDate, value.v, value.delta, calculated, error, tolerance);
        }

        calculated = compoundOption.gamma();
        error = std::fabs(calculated - value.gamma);
        tolerance = value.tol;

        if (error > tolerance)
        {
            REPORT_FAILURE("gamma", payoffMother, payoffDaughter, exerciseMother, exerciseDaughter, value.s, value.q,
                           value.r, todaysDate, value.v, value.gamma, calculated, error, tolerance);
        }

        calculated = compoundOption.vega();
        error = std::fabs(calculated - value.vega);
        tolerance = value.tol;

        if (error > tolerance)
        {
            REPORT_FAILURE("vega", payoffMother, payoffDaughter, exerciseMother, exerciseDaughter, value.s, value.q,
                           value.r, todaysDate, value.v, value.vega, calculated, error, tolerance);
        }

        calculated = compoundOption.theta();
        error = std::fabs(calculated - value.theta);
        tolerance = value.tol;

        if (error > tolerance)
        {
            REPORT_FAILURE("theta", payoffMother, payoffDaughter, exerciseMother, exerciseDaughter, value.s, value.q,
                           value.r, todaysDate, value.v, value.theta, calculated, error, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAgainstDefinition)
{
    BOOST_TEST_MESSAGE("Testing compound options against their definition...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();

    // Values from the definition, integrated in mpmath at 40 digits over the spot at the mother's expiry: the mother's
    // payoff on the daughter's Black-Scholes value there. With Drezner's six-decimal bivariate normal, the engine
    // missed them by 7e-7 to 8.2e-6.
    const struct
    {
        Option::Type typeMother, typeDaughter;
        Real strikeMother, strikeDaughter;
        Rate r, q;
        Volatility vol;
        Integer daysMother, daysDaughter;
        Real value;
    } cases[] = {{Option::Call, Option::Call, 5, 100, 0.05, 0.01, 0.2, 182, 365, 6.011025521920},
                 {Option::Call, Option::Put, 5, 100, 0.05, 0.01, 0.2, 182, 365, 2.693314135442},
                 {Option::Put, Option::Call, 2, 95, 0.05, 0.01, 0.3, 91, 365, 0.009672355748556},
                 {Option::Put, Option::Put, 2, 105, 0.01, 0.05, 0.25, 182, 730, 0.0007917069755117}};

    for (const auto& c : cases)
    {
        const auto process = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, c.q, dc)),
            Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
            Handle<BlackVolTermStructure>(flatVol(today, c.vol, dc)));
        CompoundOption option(ext::make_shared<PlainVanillaPayoff>(c.typeMother, c.strikeMother),
                              ext::make_shared<EuropeanExercise>(today + c.daysMother),
                              ext::make_shared<PlainVanillaPayoff>(c.typeDaughter, c.strikeDaughter),
                              ext::make_shared<EuropeanExercise>(today + c.daysDaughter));
        option.setPricingEngine(ext::make_shared<AnalyticCompoundOptionEngine>(process));

        BOOST_TEST_CONTEXT(c.typeMother << " on a " << c.typeDaughter << ", strikes " << c.strikeMother << " and "
                                        << c.strikeDaughter)
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, 1.0e-10);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
