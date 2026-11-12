/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004, 2007 StatPro Italia srl

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
#include <ql/experimental/forward/analytichestonforwardeuropeanengine.hpp>
#include <ql/instruments/forwardvanillaoption.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/pricingengines/forward/forwardengine.hpp>
#include <ql/pricingengines/forward/forwardperformanceengine.hpp>
#include <ql/pricingengines/forward/mcforwardeuropeanbsengine.hpp>
#include <ql/pricingengines/forward/mcforwardeuropeanhestonengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <ql/pricingengines/vanilla/binomialengine.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <map>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ForwardOptionTests)

#undef REPORT_FAILURE
#define REPORT_FAILURE(greekName, payoff, exercise, s, q, r, today, v, moneyness, reset, expected, calculated, error, \
                       tolerance)                                                                                     \
    BOOST_ERROR("Forward " << exerciseTypeToString(exercise) << " " << payoff->optionType() << " option with "        \
                           << payoffTypeToString(payoff) << " payoff:\n"                                              \
                           << "    spot value:        " << s << "\n"                                                  \
                           << "    strike:            " << payoff->strike() << "\n"                                   \
                           << "    moneyness:         " << moneyness << "\n"                                          \
                           << "    dividend yield:    " << io::rate(q) << "\n"                                        \
                           << "    risk-free rate:    " << io::rate(r) << "\n"                                        \
                           << "    reference date:    " << today << "\n"                                              \
                           << "    reset date:        " << reset << "\n"                                              \
                           << "    maturity:          " << exercise->lastDate() << "\n"                               \
                           << "    volatility:        " << io::volatility(v) << "\n\n"                                \
                           << "    expected   " << greekName << ": " << expected << "\n"                              \
                           << "    calculated " << greekName << ": " << calculated << "\n"                            \
                           << "    error:            " << error << "\n"                                               \
                           << "    tolerance:        " << tolerance);

struct ForwardOptionData
{
    Option::Type type;
    Real moneyness;
    Real s;       // spot
    Rate q;       // dividend
    Rate r;       // risk-free rate
    Time start;   // time to reset
    Time t;       // time to maturity
    Volatility v; // volatility
    Real result;  // expected result
    Real tol;     // tolerance
};

template <template <class> class Engine>
void testForwardGreeks()
{

    std::map<std::string, Real> calculated, expected, tolerance;
    tolerance["delta"] = 1.0e-5;
    tolerance["gamma"] = 1.0e-5;
    tolerance["theta"] = 1.0e-5;
    tolerance["rho"] = 1.0e-5;
    tolerance["divRho"] = 1.0e-5;
    tolerance["vega"] = 1.0e-5;

    Option::Type types[] = {Option::Call, Option::Put};
    Real moneyness[] = {0.9, 1.0, 1.1};
    Real underlyings[] = {100.0};
    Rate qRates[] = {0.04, 0.05, 0.06};
    Rate rRates[] = {0.01, 0.05, 0.15};
    Integer lengths[] = {1, 2};
    Integer startMonths[] = {6, 9};
    Volatility vols[] = {0.11, 0.50, 1.20};

    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> qTS(flatRate(qRate, dc));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> rTS(flatRate(rRate, dc));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    Handle<BlackVolTermStructure> volTS(flatVol(vol, dc));

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
        new BlackScholesMertonProcess(Handle<Quote>(spot), qTS, rTS, volTS));

    ext::shared_ptr<PricingEngine> engine(new Engine<AnalyticEuropeanEngine>(stochProcess));

    for (auto& type : types)
    {
        for (Real& moneynes : moneyness)
        {
            for (int length : lengths)
            {
                for (int startMonth : startMonths)
                {

                    Date exDate = today + length * Years;
                    ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));

                    Date reset = today + startMonth * Months;

                    ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type, 0.0));

                    ForwardVanillaOption option(moneynes, reset, payoff, exercise);
                    option.setPricingEngine(engine);

                    for (Real u : underlyings)
                    {
                        for (Real m : qRates)
                        {
                            for (Real n : rRates)
                            {
                                for (Real v : vols)
                                {

                                    Rate q = m, r = n;
                                    spot->setValue(u);
                                    qRate->setValue(q);
                                    rRate->setValue(r);
                                    vol->setValue(v);

                                    Real value = option.NPV();
                                    calculated["delta"] = option.delta();
                                    calculated["gamma"] = option.gamma();
                                    calculated["theta"] = option.theta();
                                    calculated["rho"] = option.rho();
                                    calculated["divRho"] = option.dividendRho();
                                    calculated["vega"] = option.vega();

                                    if (value > spot->value() * 1.0e-5)
                                    {
                                        // perturb spot and get delta and gamma
                                        Real du = u * 1.0e-4;
                                        spot->setValue(u + du);
                                        Real value_p = option.NPV(), delta_p = option.delta();
                                        spot->setValue(u - du);
                                        Real value_m = option.NPV(), delta_m = option.delta();
                                        spot->setValue(u);
                                        expected["delta"] = (value_p - value_m) / (2 * du);
                                        expected["gamma"] = (delta_p - delta_m) / (2 * du);

                                        // perturb rates and get rho and dividend rho
                                        Spread dr = r * 1.0e-4;
                                        rRate->setValue(r + dr);
                                        value_p = option.NPV();
                                        rRate->setValue(r - dr);
                                        value_m = option.NPV();
                                        rRate->setValue(r);
                                        expected["rho"] = (value_p - value_m) / (2 * dr);

                                        Spread dq = q * 1.0e-4;
                                        qRate->setValue(q + dq);
                                        value_p = option.NPV();
                                        qRate->setValue(q - dq);
                                        value_m = option.NPV();
                                        qRate->setValue(q);
                                        expected["divRho"] = (value_p - value_m) / (2 * dq);

                                        // perturb volatility and get vega
                                        Volatility dv = v * 1.0e-4;
                                        vol->setValue(v + dv);
                                        value_p = option.NPV();
                                        vol->setValue(v - dv);
                                        value_m = option.NPV();
                                        vol->setValue(v);
                                        expected["vega"] = (value_p - value_m) / (2 * dv);

                                        // perturb date and get theta
                                        Time dT = dc.yearFraction(today - 1, today + 1);
                                        Settings::instance().evaluationDate() = today - 1;
                                        value_m = option.NPV();
                                        Settings::instance().evaluationDate() = today + 1;
                                        value_p = option.NPV();
                                        Settings::instance().evaluationDate() = today;
                                        expected["theta"] = (value_p - value_m) / dT;

                                        // compare
                                        std::map<std::string, Real>::iterator it;
                                        for (it = calculated.begin(); it != calculated.end(); ++it)
                                        {
                                            std::string greek = it->first;
                                            Real expct = expected[greek], calcl = calculated[greek],
                                                 tol = tolerance[greek];
                                            Real error = relativeError(expct, calcl, u);
                                            if (error > tol)
                                            {
                                                REPORT_FAILURE(greek, payoff, exercise, u, q, r, today, v, moneynes,
                                                               reset, expct, calcl, error, tol);
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
}


BOOST_AUTO_TEST_CASE(testValues)
{

    BOOST_TEST_MESSAGE("Testing forward option values...");

    /* The data below are from
       "Option pricing formulas", E.G. Haug, McGraw-Hill 1998
    */
    ForwardOptionData values[] = {//  type, moneyness, spot,  div, rate,start,   t,  vol, result, tol
                                  // "Option pricing formulas", pag. 37
                                  {Option::Call, 1.1, 60.0, 0.04, 0.08, 0.25, 1.0, 0.30, 4.4064, 1.0e-4},
                                  // "Option pricing formulas", VBA code
                                  {Option::Put, 1.1, 60.0, 0.04, 0.08, 0.25, 1.0, 0.30, 8.2971, 1.0e-4}};

    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> qTS(flatRate(today, qRate, dc));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> rTS(flatRate(today, rRate, dc));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    Handle<BlackVolTermStructure> volTS(flatVol(today, vol, dc));

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
        new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                      Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));

    ext::shared_ptr<PricingEngine> engine(new ForwardVanillaEngine<AnalyticEuropeanEngine>(stochProcess));

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(value.type, 0.0));
        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));
        Date reset = today + timeToDays(value.start);

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ForwardVanillaOption option(value.moneyness, reset, payoff, exercise);
        option.setPricingEngine(engine);

        Real calculated = option.NPV();
        Real error = std::fabs(calculated - value.result);
        Real tolerance = 1e-4;
        if (error > tolerance)
        {
            REPORT_FAILURE("value", payoff, exercise, value.s, value.q, value.r, today, value.v, value.moneyness, reset,
                           value.result, calculated, error, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testPerformanceValues)
{

    BOOST_TEST_MESSAGE("Testing forward performance option values...");

    /* The data below are the performance equivalent of the
       forward options tested above and taken from
       "Option pricing formulas", E.G. Haug, McGraw-Hill 1998
    */
    ForwardOptionData values[] = {
        //  type, moneyness, spot,  div, rate,start, maturity,  vol,                       result, tol
        {Option::Call, 1.1, 60.0, 0.04, 0.08, 0.25, 1.0, 0.30, 4.4064 / 60 * std::exp(-0.04 * 0.25), 1.0e-4},
        {Option::Put, 1.1, 60.0, 0.04, 0.08, 0.25, 1.0, 0.30, 8.2971 / 60 * std::exp(-0.04 * 0.25), 1.0e-4}};

    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(0.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> qTS(flatRate(today, qRate, dc));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.0));
    Handle<YieldTermStructure> rTS(flatRate(today, rRate, dc));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.0));
    Handle<BlackVolTermStructure> volTS(flatVol(today, vol, dc));

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
        new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                      Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));

    ext::shared_ptr<PricingEngine> engine(new ForwardPerformanceVanillaEngine<AnalyticEuropeanEngine>(stochProcess));

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(value.type, 0.0));
        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));
        Date reset = today + timeToDays(value.start);

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ForwardVanillaOption option(value.moneyness, reset, payoff, exercise);
        option.setPricingEngine(engine);

        Real calculated = option.NPV();
        Real error = std::fabs(calculated - value.result);
        Real tolerance = 1e-4;
        if (error > tolerance)
        {
            REPORT_FAILURE("value", payoff, exercise, value.s, value.q, value.r, today, value.v, value.moneyness, reset,
                           value.result, calculated, error, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testGreeks)
{

    BOOST_TEST_MESSAGE("Testing forward option greeks...");

    testForwardGreeks<ForwardVanillaEngine>();
}

BOOST_AUTO_TEST_CASE(testPerformanceGreeks)
{

    BOOST_TEST_MESSAGE("Testing forward performance option greeks...");

    testForwardGreeks<ForwardPerformanceVanillaEngine>();
}


class TestBinomialEngine : public BinomialVanillaEngine<CoxRossRubinstein>
{
  private:
  public:
    explicit TestBinomialEngine(const ext::shared_ptr<GeneralizedBlackScholesProcess>& process)
    : BinomialVanillaEngine<CoxRossRubinstein>(process, 300) // fixed steps
    {
    }
};

BOOST_AUTO_TEST_CASE(testGreeksInitialization)
{
    BOOST_TEST_MESSAGE("Testing forward option greeks initialization...");

    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(100.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.04));
    Handle<YieldTermStructure> qTS(flatRate(qRate, dc));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.01));
    Handle<YieldTermStructure> rTS(flatRate(rRate, dc));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.11));
    Handle<BlackVolTermStructure> volTS(flatVol(vol, dc));

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
        new BlackScholesMertonProcess(Handle<Quote>(spot), qTS, rTS, volTS));

    ext::shared_ptr<PricingEngine> engine(new ForwardVanillaEngine<TestBinomialEngine>(stochProcess));
    Date exDate = today + 1 * Years;
    ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));
    Date reset = today + 6 * Months;
    ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(Option::Call, 0.0));

    ForwardVanillaOption option(0.9, reset, payoff, exercise);
    option.setPricingEngine(engine);

    ext::shared_ptr<PricingEngine> ctrlengine(new TestBinomialEngine(stochProcess));
    VanillaOption ctrloption(payoff, exercise);
    ctrloption.setPricingEngine(ctrlengine);

    Real delta = 0;
    try
    {
        delta = ctrloption.delta();
    }
    catch (const QuantLib::Error&)
    {
        // if normal option can't calculate delta,
        // nor should forward
        try
        {
            delta = option.delta();
        }
        catch (const QuantLib::Error&)
        {
            delta = Null<Real>();
        }
        QL_REQUIRE(delta == Null<Real>(), "Forward delta invalid");
    }

    Real rho = 0;
    try
    {
        rho = ctrloption.rho();
    }
    catch (const QuantLib::Error&)
    {
        // if normal option can't calculate rho,
        // nor should forward
        try
        {
            rho = option.rho();
        }
        catch (const QuantLib::Error&)
        {
            rho = Null<Real>();
        }
        QL_REQUIRE(rho == Null<Real>(), "Forward rho invalid");
    }

    Real divRho = 0;
    try
    {
        divRho = ctrloption.dividendRho();
    }
    catch (const QuantLib::Error&)
    {
        // if normal option can't calculate divRho,
        // nor should forward
        try
        {
            divRho = option.dividendRho();
        }
        catch (const QuantLib::Error&)
        {
            divRho = Null<Real>();
        }
        QL_REQUIRE(divRho == Null<Real>(), "Forward dividendRho invalid");
    }

    Real vega = 0;
    try
    {
        vega = ctrloption.vega();
    }
    catch (const QuantLib::Error&)
    {
        // if normal option can't calculate vega,
        // nor should forward
        try
        {
            vega = option.vega();
        }
        catch (const QuantLib::Error&)
        {
            vega = Null<Real>();
        }
        QL_REQUIRE(vega == Null<Real>(), "Forward vega invalid");
    }
}

BOOST_AUTO_TEST_CASE(testMCPrices)
{
    BOOST_TEST_MESSAGE("Testing forward option MC prices...");

    Real tol[] = {0.002, 0.001, 0.0006, 5e-4, 5e-4};

    Size timeSteps = 100;
    Size numberOfSamples = 5000;
    Size mcSeed = 42;

    Real q = 0.04;
    Real r = 0.01;
    Real sigma = 0.11;
    Real s = 100;

    DayCounter dc = Actual360();
    Date today = Settings::instance().evaluationDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(s));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(q));
    Handle<YieldTermStructure> qTS(flatRate(qRate, dc));
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(r));
    Handle<YieldTermStructure> rTS(flatRate(rRate, dc));
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(sigma));
    Handle<BlackVolTermStructure> volTS(flatVol(vol, dc));

    ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
        new BlackScholesMertonProcess(Handle<Quote>(spot), qTS, rTS, volTS));

    ext::shared_ptr<PricingEngine> analyticEngine(new ForwardVanillaEngine<AnalyticEuropeanEngine>(stochProcess));

    ext::shared_ptr<PricingEngine> mcEngine = MakeMCForwardEuropeanBSEngine<PseudoRandom>(stochProcess)
                                                  .withSteps(timeSteps)
                                                  .withSamples(numberOfSamples)
                                                  .withSeed(mcSeed);

    Date exDate = today + 1 * Years;
    ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));
    Date reset = today + 6 * Months;
    ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(Option::Call, 0.0));

    Real moneyness[] = {0.8, 0.9, 1.0, 1.1, 1.2};

    for (Size moneyness_index = 0; moneyness_index < std::size(moneyness); ++moneyness_index)
    {

        ForwardVanillaOption option(moneyness[moneyness_index], reset, payoff, exercise);

        option.setPricingEngine(analyticEngine);
        Real analyticPrice = option.NPV();

        option.setPricingEngine(mcEngine);
        Real mcPrice = option.NPV();

        Real error = relativeError(analyticPrice, mcPrice, s);
        if (error > tol[moneyness_index])
        {
            REPORT_FAILURE("testMCPrices", payoff, exercise, s, q, r, today, sigma, moneyness[moneyness_index], reset,
                           analyticPrice, mcPrice, error, tol[moneyness_index]);
        }
    }
}

BOOST_AUTO_TEST_CASE(testHestonMCPrices)
{
    BOOST_TEST_MESSAGE("Testing forward option Heston MC prices...");

    Option::Type optionTypes[] = {Option::Call, Option::Put};
    Real mcForwardStartTolerance[][6] = {{7e-4,    // Call, moneyness=0.8
                                          8e-4,    // Call, moneyness=0.9
                                          6e-4,    // Call, moneyness=1.0
                                          5e-4,    // Call, moneyness=1.1
                                          5e-4},   // Call, moneyness=1.2
                                         {6e-4,    // Put, moneyness=0.8
                                          5e-4,    // Put, moneyness=0.9
                                          6e-4,    // Put, moneyness=1.0
                                          0.001,   // Put, moneyness=1.1
                                          0.001}}; // Put, moneyness=1.2

    Real tol[][6] = {{9e-4,    // Call, moneyness=0.8
                      9e-4,    // Call, moneyness=0.9
                      6e-4,    // Call, moneyness=1.0
                      5e-4,    // Call, moneyness=1.1
                      5e-4},   // Call, moneyness=1.2
                     {6e-4,    // Put, moneyness=0.8
                      5e-4,    // Put, moneyness=0.9
                      8e-4,    // Put, moneyness=1.0
                      0.002,   // Put, moneyness=1.1
                      0.002}}; // Put, moneyness=1.2

    for (Size type_index = 0; type_index < std::size(optionTypes); ++type_index)
    {

        Real analyticTolerance = 5e-4;

        Size timeSteps = 50;
        Size numberOfSamples = 4095;
        Size mcSeed = 42;

        Real q = 0.04;
        Real r = 0.01;
        Real sigma_bs = 0.245;
        Real s = 100;

        // Test 1: Set up an equivalent flat Heston and compare to analytical BS pricing
        Real v0 = sigma_bs * sigma_bs;
        Real kappa = 1e-8;
        Real theta = sigma_bs * sigma_bs;
        Real sigma = 1e-8;
        Real rho = -0.93;

        DayCounter dc = Actual360();
        Date today = Settings::instance().evaluationDate();

        Date exDate = today + 1 * Years;
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));
        Date reset = today + 6 * Months;
        ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(optionTypes[type_index], 0.0));

        ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(s));
        ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(q));
        Handle<YieldTermStructure> qTS(flatRate(qRate, dc));
        ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(r));
        Handle<YieldTermStructure> rTS(flatRate(rRate, dc));
        ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(sigma_bs));
        Handle<BlackVolTermStructure> volTS(flatVol(vol, dc));

        ext::shared_ptr<BlackScholesMertonProcess> bsProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), qTS, rTS, volTS));

        ext::shared_ptr<PricingEngine> analyticEngine(new ForwardVanillaEngine<AnalyticEuropeanEngine>(bsProcess));

        ext::shared_ptr<HestonProcess> hestonProcess(
            new HestonProcess(rTS, qTS, Handle<Quote>(spot), v0, kappa, theta, sigma, rho));

        ext::shared_ptr<PricingEngine> mcEngine = MakeMCForwardEuropeanHestonEngine<LowDiscrepancy>(hestonProcess)
                                                      .withSteps(timeSteps)
                                                      .withSamples(numberOfSamples)
                                                      .withSeed(mcSeed);

        Real moneyness[] = {0.8, 0.9, 1.0, 1.1, 1.2};

        for (Size moneyness_index = 0; moneyness_index < std::size(moneyness); ++moneyness_index)
        {

            ForwardVanillaOption option(moneyness[moneyness_index], reset, payoff, exercise);

            option.setPricingEngine(analyticEngine);
            Real analyticPrice = option.NPV();

            option.setPricingEngine(mcEngine);
            Real mcPrice = option.NPV();

            Real mcError = relativeError(analyticPrice, mcPrice, s);

            if (mcError > mcForwardStartTolerance[type_index][moneyness_index])
            {
                REPORT_FAILURE("testHestonMCForwardStartPrices", payoff, exercise, s, q, r, today, sigma_bs,
                               moneyness[moneyness_index], reset, analyticPrice, mcPrice, mcError,
                               mcForwardStartTolerance[type_index][moneyness_index]);
            }
        }

        // Test 2: Using an arbitrary Heston model, check that prices match semi-analytical
        // Heston prices when reset date is t=0
        v0 = sigma_bs * sigma_bs;
        kappa = 1.0;
        theta = 0.08;
        sigma = 0.39;
        rho = -0.93;

        reset = today;

        ext::shared_ptr<HestonProcess> hestonProcessSmile(
            new HestonProcess(rTS, qTS, Handle<Quote>(spot), v0, kappa, theta, sigma, rho));

        ext::shared_ptr<HestonModel> hestonModel(ext::make_shared<HestonModel>(hestonProcessSmile));

        ext::shared_ptr<PricingEngine> analyticHestonEngine(ext::make_shared<AnalyticHestonEngine>(hestonModel, 96));

        ext::shared_ptr<PricingEngine> mcEngineSmile =
            MakeMCForwardEuropeanHestonEngine<LowDiscrepancy>(hestonProcessSmile)
                .withSteps(timeSteps)
                .withSamples(numberOfSamples)
                .withSeed(mcSeed);

        ext::shared_ptr<AnalyticHestonForwardEuropeanEngine> analyticForwardHestonEngine(
            new AnalyticHestonForwardEuropeanEngine(hestonProcessSmile));

        for (Size moneyness_index = 0; moneyness_index < std::size(moneyness); ++moneyness_index)
        {

            Real strike = s * moneyness[moneyness_index];
            ext::shared_ptr<StrikedTypePayoff> vanillaPayoff(new PlainVanillaPayoff(optionTypes[type_index], strike));

            VanillaOption vanillaOption(vanillaPayoff, exercise);
            ForwardVanillaOption forwardOption(moneyness[moneyness_index], reset, payoff, exercise);

            vanillaOption.setPricingEngine(analyticHestonEngine);
            Real analyticPrice = vanillaOption.NPV();

            forwardOption.setPricingEngine(mcEngineSmile);
            Real mcPrice = forwardOption.NPV();

            Real mcError = relativeError(analyticPrice, mcPrice, s);
            auto tolerance = tol[type_index][moneyness_index];

            if (mcError > tolerance)
            {
                REPORT_FAILURE("testHestonMCPrices", vanillaPayoff, exercise, s, q, r, today, sigma_bs,
                               moneyness[moneyness_index], reset, analyticPrice, mcPrice, mcError, tolerance);
            }

            // T=0, testing the Analytic Pricer's T=0 analytical solution
            forwardOption.setPricingEngine(analyticForwardHestonEngine);
            Real hestonAnalyticPrice = forwardOption.NPV();

            Real analyticError = relativeError(analyticPrice, hestonAnalyticPrice, s);
            if (analyticError > analyticTolerance)
            {
                REPORT_FAILURE("testHestonAnalyticForwardStartPrices", vanillaPayoff, exercise, s, q, r, today,
                               sigma_bs, moneyness[moneyness_index], reset, analyticPrice, hestonAnalyticPrice,
                               analyticError, analyticTolerance);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticalVsMCPrices, *precondition(if_speed(Fast)))
{
    BOOST_TEST_MESSAGE("Testing Heston analytic vs MC prices...");

    Option::Type optionTypes[] = {Option::Call, Option::Put};
    Real tol[][6] = {{0.002,   // Call, moneyness=0.8, CV:false
                      0.002,   // Call, moneyness=0.8, CV:true
                      0.001,   // Call, moneyness=1.0, CV:false
                      0.001,   // Call, moneyness=1.0, CV:true
                      0.001,   // Call, moneyness=1.2, CV:false
                      0.001},  // Call, moneyness=1.2, CV:true
                     {0.001,   // Put, moneyness=0.8, CV:false
                      0.001,   // Put, moneyness=0.8, CV:true
                      0.003,   // Put, moneyness=1.0, CV:false
                      0.003,   // Put, moneyness=1.0, CV:true
                      0.003,   // Put, moneyness=1.2, CV:false
                      0.003}}; // Put, moneyness=1.2, CV:true

    for (Size option_type_index = 0; option_type_index < std::size(optionTypes); ++option_type_index)
    {

        Size timeSteps = 50;
        Size numberOfSamples = 5000;
        Size mcSeed = 42;

        Real q = 0.03;
        Real r = 0.005;
        Real s = 100;

        Real vol = 0.3;
        Real v0 = vol * vol;
        Real kappa = 11.35;
        Real theta = 0.022;
        Real sigma = 0.618;
        Real rho = -0.5;

        DayCounter dc = Actual360();
        Date today = Settings::instance().evaluationDate();

        Date exDate = today + 1 * Years;
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(exDate));
        Date reset = today + 6 * Months;
        ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(optionTypes[option_type_index], 0.0));

        ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(s));
        ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(q));
        Handle<YieldTermStructure> qTS(flatRate(qRate, dc));
        ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(r));
        Handle<YieldTermStructure> rTS(flatRate(rRate, dc));

        ext::shared_ptr<HestonProcess> hestonProcess(
            new HestonProcess(rTS, qTS, Handle<Quote>(spot), v0, kappa, theta, sigma, rho));

        ext::shared_ptr<PricingEngine> mcEngine = MakeMCForwardEuropeanHestonEngine<PseudoRandom>(hestonProcess)
                                                      .withSteps(timeSteps)
                                                      .withSamples(numberOfSamples)
                                                      .withSeed(mcSeed);

        ext::shared_ptr<PricingEngine> mcEngineCv = MakeMCForwardEuropeanHestonEngine<PseudoRandom>(hestonProcess)
                                                        .withSteps(timeSteps)
                                                        .withSamples(numberOfSamples)
                                                        .withSeed(mcSeed)
                                                        .withControlVariate(true);

        ext::shared_ptr<AnalyticHestonForwardEuropeanEngine> analyticEngine(
            new AnalyticHestonForwardEuropeanEngine(hestonProcess));

        Real moneyness[] = {0.8, 1.0, 1.2};

        for (Size moneyness_index = 0; moneyness_index < std::size(moneyness); ++moneyness_index)
        {

            auto m = moneyness[moneyness_index];
            ForwardVanillaOption option(m, reset, payoff, exercise);

            option.setPricingEngine(analyticEngine);
            Real analyticPrice = option.NPV();

            option.setPricingEngine(mcEngine);
            Real mcPrice = option.NPV();
            Real error = relativeError(analyticPrice, mcPrice, s);

            auto tolerance = tol[option_type_index][2 * moneyness_index];
            if (error > tolerance)
            {
                REPORT_FAILURE("testHestonMCVsAnalyticPrices", payoff, exercise, s, q, r, today, vol, m, reset,
                               analyticPrice, mcPrice, error, tolerance);
            }

            option.setPricingEngine(mcEngineCv);
            Real mcPriceCv = option.NPV();

            Real errorCv = relativeError(analyticPrice, mcPriceCv, s);
            tolerance = tol[option_type_index][2 * moneyness_index + 1];
            if (errorCv > tolerance)
            {
                REPORT_FAILURE("testHestonMCControlVariateVsAnalyticPrices", payoff, exercise, s, q, r, today, vol, m,
                               reset, analyticPrice, mcPriceCv, errorCv, tolerance);
            }
        }
    }
}

// A forward-start option priced by AnalyticHestonForwardEuropeanEngine on the market of the tests below: the spot at
// 100, r 0.5 % and q 3 %, on curves with the given day counter.
Real forwardHestonNpv(const Date& today,
                      const DayCounter& dc,
                      Real v0,
                      Real kappa,
                      Real theta,
                      Real sigma,
                      Real rho,
                      const Date& reset,
                      const Date& expiry,
                      Real moneyness,
                      Option::Type type)
{
    const Handle<Quote> spot(ext::make_shared<SimpleQuote>(100.0));
    const Handle<YieldTermStructure> rTS(flatRate(today, 0.005, dc));
    const Handle<YieldTermStructure> qTS(flatRate(today, 0.03, dc));
    const auto process = ext::make_shared<HestonProcess>(rTS, qTS, spot, v0, kappa, theta, sigma, rho);
    ForwardVanillaOption option(moneyness, reset, ext::make_shared<PlainVanillaPayoff>(type, 0.0),
                                ext::make_shared<EuropeanExercise>(expiry));
    option.setPricingEngine(ext::make_shared<AnalyticHestonForwardEuropeanEngine>(process));
    return option.NPV();
}

// A row of the tests below, with the reset and the expiry in days from 15 May 2026, under Actual/365 (Fixed).  The
// expected values are
//     S0 e^{-qt} \int p(v) h(v) dv,
// with p the density of the variance at the reset under the share measure, from Boost's noncentral chi-squared
// distribution, and h the Heston price at the reset with that variance, from AnalyticHestonEngine (adaptive
// Gauss-Lobatto at 1e-12), integrated by tanh-sinh, then by Gauss-Kronrod in the tail.  Where rho is near -1 or 1,
// or R small, that h loses accuracy or fails, and those rows say what they take instead, two ways that agree to
// 1e-14.  The rows are checked to 1e-9 relative: the engine agrees with every one of them to 1e-12, and the old
// engine, where it priced at all, was 8e-8 or more off.
struct ForwardHestonCase
{
    Real v0, kappa, theta, sigma, rho;
    Integer reset, expiry;
    Real moneyness;
    Option::Type type;
    Real expected;
};

void checkForwardHestonCase(const ForwardHestonCase& c)
{
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    BOOST_TEST_CONTEXT("v0 " << c.v0 << ", kappa " << c.kappa << ", theta " << c.theta << ", sigma " << c.sigma
                             << ", rho " << c.rho << ", reset in " << c.reset << " days, expiry in " << c.expiry
                             << " days, " << c.type << " at " << c.moneyness)
    {
        const Real npv = forwardHestonNpv(today, Actual365Fixed(), c.v0, c.kappa, c.theta, c.sigma, c.rho,
                                          today + c.reset, today + c.expiry, c.moneyness, c.type);
        BOOST_CHECK_CLOSE_FRACTION(npv, c.expected, 1e-9);
    }
}

// A day counter that puts today + 1 at t1 years from today and today + 2 at t2, and each later day 1/365 of a year
// further, so that an option resetting at today + 1 and expiring at today + 2 can have any reset and tenor.
class ForwardHestonDayCounter : public DayCounter
{
    class Impl final : public DayCounter::Impl
    {
      public:
        Impl(const Date& today, Time t1, Time t2) : today_(today), t1_(t1), t2_(t2) {}
        std::string name() const override { return "forward Heston test"; }
        Time yearFraction(const Date& d1, const Date& d2, const Date&, const Date&) const override
        {
            return time(d2) - time(d1);
        }

      private:
        Time time(const Date& d) const
        {
            const auto days = d - today_;
            QL_REQUIRE(days >= 0, d << " is before " << today_);
            if (days == 0)
                return 0.0;
            if (days == 1)
                return t1_;
            return t2_ + (days - 2) / 365.0;
        }
        Date today_;
        Time t1_, t2_;
    };

  public:
    ForwardHestonDayCounter(const Date& today, Time t1, Time t2) : DayCounter(ext::make_shared<Impl>(today, t1, t2)) {}
};

BOOST_AUTO_TEST_CASE(testHestonAnalyticShortResets)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine at resets within days...");

    // Forward-start options with a year to expiry.  The engine priced all but the last row at NaN, and its outer
    // integral, on a range of at least [0, 2], put the density between its nodes at the shortest resets: with the
    // density finite, a day's reset priced up to 64 % high (#202).  At the last row the variance is above 2, and the
    // old range left most of the density out: 77.5 % low.  Its nodes then moved by an extra 1e-8, which left it 9e-7
    // to 3e-6 below these values, and 6.5e-5 at the last row (#223).
    const ForwardHestonCase cases[] = {
        {0.04, 2.0, 0.04, 0.2, -0.7, 1, 365, 1.0, Option::Call, 6.37672584286359},
        {0.04, 2.0, 0.04, 0.2, -0.7, 3, 365, 1.0, Option::Call, 6.36015556997376},
        {0.04, 2.0, 0.04, 0.2, -0.7, 3, 365, 1.0, Option::Put, 8.79640677290727},
        {0.04, 2.0, 0.04, 0.2, -0.7, 3, 365, 0.8, Option::Call, 19.2453373103248},
        {0.04, 2.0, 0.04, 0.2, -0.7, 3, 365, 1.2, Option::Call, 1.07750216474642},
        {0.04, 2.0, 0.04, 0.11, -0.7, 1, 365, 1.0, Option::Call, 6.52586851216565},
        {0.04, 2.0, 0.04, 0.11, -0.7, 14, 365, 1.0, Option::Call, 6.42099974973167},
        {0.04, 2.0, 0.04, 0.11, -0.7, 14, 365, 1.0, Option::Put, 8.78232788954574},
        {0.04, 2.0, 0.04, 0.11, -0.7, 30, 365, 1.0, Option::Call, 6.28913326811781},
        {0.09, 11.35, 0.022, 0.618, -0.5, 1, 365, 1.0, Option::Call, 5.17099230135497},
        {2.25, 2.0, 2.25, 1.0, -0.5, 7, 365, 1.0, Option::Call, 50.1241177524065},
    };
    for (const auto& c : cases)
        checkForwardHestonCase(c);
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticShortTenors)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine at tenors down to minutes...");

    // A day or less from the reset to the expiry (#225).  The engine took P1 and P2 by Gil-Pelaez on the real line,
    // with 128 Gauss-Legendre nodes on [0, 100]: at a day's tenor the characteristic function hasn't decayed by
    // then, and away from the money it turns too often for a fixed rule.  At rho = -0.999 the integration line isn't
    // tilted above the forward, and the range takes up to six panels.  The engine was 9e-7 low at 182 days, and
    // priced the one-day call at -0.0123.
    const ForwardHestonCase cases[] = {
        {0.04, 2.0, 0.04, 0.2, -0.7, 364, 365, 1.0, Option::Call, 0.378225329676645},
        {0.04, 2.0, 0.04, 0.2, -0.999, 182, 365, 1.2, Option::Call, 0.182783582882488},
        {0.04, 2.0, 0.04, 0.2, -0.999, 364, 365, 1.01, Option::Call, 0.0792656949067135},
    };
    for (const auto& c : cases)
        checkForwardHestonCase(c);

    // 1e-5 years, about five minutes, from a reset half a year away.  Above the forward the line isn't tilted at
    // the larger variances, and its range takes up to five panels of at most 64 turns of e^{-iuk}.  The call out of
    // the money is worth nothing, and the engine prices it within 1e-14 of that, where it priced it at -0.268.  In
    // the money it's worth a forward contract, S0 e^{-qt} (e^{-q tau} - m e^{-r tau}).
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    const Time t = 0.5, tau = 1e-5;
    const ForwardHestonDayCounter dc(today, t, t + tau);
    BOOST_CHECK_SMALL(forwardHestonNpv(today, dc, 0.04, 2.0, 0.04, 0.2, -0.7, today + 1, today + 2, 1.2, Option::Call),
                      1e-10);
    BOOST_CHECK_CLOSE_FRACTION(
        forwardHestonNpv(today, dc, 0.04, 2.0, 0.04, 0.2, -0.7, today + 1, today + 2, 0.8, Option::Call),
        100.0 * std::exp(-0.03 * t) * (std::exp(-0.03 * tau) - 0.8 * std::exp(-0.005 * tau)), 1e-9);
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticCorrelationNearOne)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine at rho near and at -1 and 1...");

    // Near rho = -1 or 1 the inner integrand decays slowly along the untilted line, as e^{-c sqrt(1 - rho^2) u} with
    // c = (v + kappa theta tau) / sigma, and turns many times on the way out.  The engine's fixed rule on [0, 100]
    // was 8e-8 to 1.6e-6 relative off on the first four rows (#225).  At rho = -1 and 1 these rows lie on the side of
    // the forward where the line tilts.  The references take h by Gil-Pelaez, with fixed rules out to twice where the
    // integrand has decayed to 1e-22, on the untilted line or on lines tilted by pi / 8 and pi / 16, and by exp-sinh
    // on AngledContour's line, except at rho = 0.99999, where its characteristic function overflows; on each row
    // they agree to 1e-14.
    // The last two rows are at the forward, where log(K / F) is a rounding error, 2.2e-16 and 4.4e-16 here: with
    // rho > 0 the line then tilts, and the term e^{-izk} decays along it at a rate of 6e-17 and 1.2e-16.  The engine
    // was 1.2e-7 low on the first and 28.6 % low on the second, a day before expiry.
    const ForwardHestonCase cases[] = {
        {0.04, 2.0, 0.04, 0.2, -0.99999, 182, 365, 1.0, Option::Call, 4.5868361965187},
        {0.04, 2.0, 0.04, 0.2, 0.99999, 182, 365, 0.8, Option::Call, 18.5813694973778},
        {0.04, 2.0, 0.04, 0.2, -1.0, 182, 365, 0.8, Option::Call, 19.0546526642614},
        {0.04, 2.0, 0.04, 0.2, 1.0, 182, 365, 1.2, Option::Call, 1.06465182385709},
        {0.04, 2.0, 0.04, 0.2, 0.5, 182, 365, 0.9875439799154531, Option::Call, 5.44206731466711},
        {0.04, 2.0, 0.04, 0.2, 0.5, 364, 365, 0.9999315091949174, Option::Call, 0.401771736495361},
    };
    for (const auto& c : cases)
        checkForwardHestonCase(c);

    // On the other side of the forward, at rho = -1 and 1, the line isn't tilted and the integrand doesn't decay
    // along it, and the engine throws (#238), where it priced with its fixed rule.  At rho = -0.99999999 the
    // integrand turns 40,000 times before it decays, which would take more panels than the engine takes.
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    const auto npv = [&](Real rho, Real moneyness)
    {
        return forwardHestonNpv(today, Actual365Fixed(), 0.04, 2.0, 0.04, 0.2, rho, today + 182, today + 365, moneyness,
                                Option::Call);
    };
    BOOST_CHECK_THROW(npv(-1.0, 1.2), Error);
    BOOST_CHECK_THROW(npv(1.0, 0.8), Error);
    BOOST_CHECK_THROW(npv(-0.99999999, 1.0), Error);
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticFellerConditionViolated)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine where the Feller condition fails...");

    // Where R = 4 kappa theta / sigma^2 is below 2, the density of the variance at the reset goes to infinity at
    // zero, as v^(R/2 - 1), and just above 2 its derivative does (#224).  The engine integrated it with 128
    // Gauss-Legendre nodes, which can't follow either: with the inner integral right (#225), these rows were 2.5e-6
    // to 14.8 % off.  R is 0.96, 0.156, 1.14, 1.78, 2.6, 0.64 and, at the forward, 0.016 below; that row's reference
    // takes h by exp-sinh and by Gil-Pelaez on the untilted line, as Gauss-Lobatto is noisy below v = 1e-13.
    const ForwardHestonCase cases[] = {
        {0.04, 1.5, 0.04, 0.5, -0.7, 182, 365, 1.0, Option::Call, 3.7396721130103},
        {0.1, 0.5, 0.05, 0.8, -0.9, 91, 365, 1.0, Option::Call, 4.75612539170697},
        {0.1, 0.5, 0.05, 0.8, -0.9, 182, 365, 1.0, Option::Call, 3.31478037300948},
        {0.1, 0.5, 0.05, 0.8, -0.9, 182, 365, 1.0, Option::Put, 4.53881588967316},
        {0.36, 4.0, 0.16, 1.5, -0.8, 182, 365, 1.0, Option::Call, 8.42434642589705},
        {0.04, 1.0, 0.04, 0.3, -0.7, 182, 365, 1.0, Option::Call, 4.16719596717314},
        {0.09, 11.35, 0.022, 0.618, -0.5, 182, 365, 1.0, Option::Call, 3.31310543670044},
        {0.09, 11.35, 0.022, 0.618, -0.5, 182, 365, 1.0, Option::Put, 4.53714095336412},
        {0.09, 11.35, 0.022, 0.618, -0.5, 364, 365, 1.0, Option::Call, 0.267165861660158},
        {0.09, 11.35, 0.022, 0.618, -0.5, 364, 365, 1.0, Option::Put, 0.273812976514576},
        {0.04, 1.0, 0.04, 0.5, 0.5, 364, 365, 1.0, Option::Call, 0.322306139942502},
        {0.04, 0.1, 0.04, 1.0, -0.7, 182, 365, 0.9875439799154531, Option::Call, 1.49325621847104},
    };
    for (const auto& c : cases)
        checkForwardHestonCase(c);

    // At R = 1.6e-4, over a tenor of 1e-5 years, five minutes: most of the density's mass lies within 1e-16 of zero,
    // where the inner integrand's decay rate, (v + kappa theta tau) / sigma, is 4e-10, and the call at the forward
    // takes its value from kappa theta tau.  The engine priced it 87 % low.  Its reference takes h by exp-sinh and by
    // Gil-Pelaez on the untilted line, which agree to 2e-15.  The 128 nodes of the outer rule leave 4.5e-9 of it
    // (6e-12 with 512), hence the tolerance.
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    const Time t = 0.5, tau = 1e-5;
    BOOST_CHECK_CLOSE_FRACTION(forwardHestonNpv(today, ForwardHestonDayCounter(today, t, t + tau), 0.04, 0.001, 0.04,
                                                1.0, -0.999, today + 1, today + 2, std::exp(-0.025 * tau),
                                                Option::Call),
                               0.0058514120171337, 1e-7);
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticZeroKappaHat)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine at kappa = rho sigma...");

    // kappaHat = kappa - rho sigma, the variance's rate of mean reversion under the share measure, is zero here: 0.1
    // - 0.5 * 0.2 is exactly 0 in floating point.  The engine divided by it and threw (#223).  The density's scale
    // and moments go as (1 - e^{-kappaHat t}) / kappaHat, which is t at zero, and the price is continuous there.
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    const auto npv = [&](Real kappa)
    {
        return forwardHestonNpv(today, Actual365Fixed(), 0.04, kappa, 0.04, 0.2, 0.5, today + 91, today + 365, 1.0,
                                Option::Call);
    };
    const Real atZero = npv(0.1);
    BOOST_CHECK_CLOSE_FRACTION(atZero, 5.71318979573601, 1e-9);
    BOOST_CHECK_CLOSE_FRACTION(npv(0.1 + 1e-9), atZero, 1e-8);
    BOOST_CHECK_CLOSE_FRACTION(npv(0.1 - 1e-9), atZero, 1e-8);
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticVarianceDensity)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine's density of the variance at the reset...");

    // The density of the variance at the reset is noncentral chi-squared, which the engine took from its Bessel
    // function, I_nu with nu = R/2 - 1.  At v0 = 0 its noncentrality is zero and that form is 0/0: the engine priced
    // NaN (#223).  At long resets the noncentrality is small, I_nu underflows where nu is moderately large, and the
    // engine priced the call five years away, at R 138.9, at 0, and the put at 2.11 (#237).
    const ForwardHestonCase cases[] = {
        {0.0, 2.0, 0.04, 0.2, -0.7, 14, 365, 1.0, Option::Call, 4.53631526640341},
        {0.0, 2.0, 0.04, 0.2, -0.7, 91, 365, 1.0, Option::Call, 4.49572993793561},
        {0.0, 2.0, 0.04, 0.2, -0.7, 182, 365, 1.0, Option::Call, 4.02801361659341},
        {0.1, 5.0, 0.1, 0.12, -0.7, 1825, 2190, 1.0, Option::Call, 9.52429297081378},
        {0.1, 5.0, 0.1, 0.12, -0.7, 1825, 2190, 1.0, Option::Put, 11.6387895780479},
        {0.1, 5.0, 0.1, 0.12, -0.7, 1825, 2190, 0.8, Option::Call, 18.9098184144192},
        {0.25, 10.0, 0.25, 0.11, -0.7, 170, 535, 1.0, Option::Call, 17.8958818909176},
        {0.25, 10.0, 0.25, 0.11, -0.7, 260, 625, 1.0, Option::Call, 17.7639303437161},
    };
    for (const auto& c : cases)
        checkForwardHestonCase(c);

    // At R 826, between the last two rows, I_nu also underflows over the density's bulk, for resets from 184 to 227
    // days, where nu is 412 and the noncentrality a few units.  The engine priced up to 100 % low there (#237).  It
    // now throws: at 200 days the density is 0 at the variance's mean, and at 220 it doesn't integrate to 1.
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    for (Integer reset : {200, 220})
    {
        BOOST_TEST_CONTEXT("reset in " << reset << " days")
        {
            BOOST_CHECK_THROW(forwardHestonNpv(today, Actual365Fixed(), 0.25, 10.0, 0.25, 0.11, -0.7, today + reset,
                                               today + reset + 365, 1.0, Option::Call),
                              Error);
        }
    }
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticSmallSigma)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine at a small vol of vol...");

    // The engine refused sigma at or below 0.1, which nothing had measured (#223).  It prices these down to sigma
    // 0.01, where R = 4 kappa theta / sigma^2 is 3200.
    const ForwardHestonCase cases[] = {
        {0.04, 2.0, 0.04, 0.1, -0.7, 182, 365, 1.0, Option::Call, 4.81545420549318},
        {0.04, 2.0, 0.04, 0.05, -0.7, 30, 365, 1.0, Option::Call, 6.36879857771811},
        {0.04, 2.0, 0.04, 0.05, -0.7, 182, 365, 1.0, Option::Call, 4.87591853658243},
        {0.04, 2.0, 0.04, 0.02, -0.7, 182, 365, 1.0, Option::Call, 4.90538183376543},
        {0.04, 2.0, 0.04, 0.01, -0.7, 30, 365, 1.0, Option::Call, 6.410802504424},
        {0.04, 2.0, 0.04, 0.01, -0.7, 182, 365, 1.0, Option::Call, 4.91405131898079},
    };
    for (const auto& c : cases)
        checkForwardHestonCase(c);

    // At longer resets, as R grows, the density's Bessel function underflows (#237), and the engine throws: at
    // sigma 0.01 a year away, where the density is 0 at the variance's mean, and at sigma 0.02 three years away,
    // where it doesn't integrate to 1.
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    BOOST_CHECK_THROW(forwardHestonNpv(today, Actual365Fixed(), 0.04, 2.0, 0.04, 0.01, -0.7, today + 365, today + 730,
                                       1.0, Option::Call),
                      Error);
    BOOST_CHECK_THROW(forwardHestonNpv(today, Actual365Fixed(), 0.04, 2.0, 0.04, 0.02, -0.7, today + 1095, today + 1460,
                                       1.0, Option::Call),
                      Error);
}

BOOST_AUTO_TEST_CASE(testHestonAnalyticResetsNearToday)
{
    BOOST_TEST_MESSAGE("Testing the analytic Heston forward-start engine at resets within hours...");

    // The engine priced resets within 1e-3 years, about nine hours, as vanilla options over the tenor from v0, which
    // leaves out how the variance moves until the reset (#223): there the price moved with the reset time at -2.61
    // a year, where it moves at -3.02.  That fallback now covers 1e-11 years.  Up to 2e-3 years the price is smooth
    // in the reset time, and departs from the line through its values at 0 and 2e-3 by about 0.066 t (2e-3 - t),
    // relative, at most 6.6e-8, where the old fallback departed from it by 6.4e-6 at 1e-4 years.  A year from today
    // to the expiry.
    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    const auto npv = [&](Real v0, Time t)
    {
        return forwardHestonNpv(today, ForwardHestonDayCounter(today, t, 1.0), v0, 2.0, 0.04, 0.2, -0.7, today + 1,
                                today + 2, 1.0, Option::Call);
    };
    const Real atToday = npv(0.04, 0.0), at2e3 = npv(0.04, 2e-3);
    for (Time t : {1e-8, 1e-4, 5e-4})
    {
        BOOST_TEST_CONTEXT("reset in " << t << " years")
        {
            BOOST_CHECK_CLOSE_FRACTION(npv(0.04, t), atToday + (at2e3 - atToday) * t / 2e-3, 1e-6);
        }
    }

    // At v0 = 0 the vanilla option can't be priced from v0, and the old fallback threw; now only resets within 1e-11
    // years do.  From 1e-8 years on the price is smooth in the reset time again, and departs from the line through
    // its values at 1e-8 and 2e-3 years by 2.2e-7, relative, at 1e-4 years.
    BOOST_CHECK_THROW(npv(0.0, 0.0), Error);
    const Real at1e8 = npv(0.0, 1e-8), at2e3Zero = npv(0.0, 2e-3);
    BOOST_CHECK_CLOSE_FRACTION(npv(0.0, 1e-4), at1e8 + (at2e3Zero - at1e8) * (1e-4 - 1e-8) / (2e-3 - 1e-8), 1e-6);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
