/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*

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
#include <ql/instruments/payoffs.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/bacheliercalculator.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <cmath>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BachelierCalculatorTests)

struct BachelierCalculatorTestData
{
    Option::Type type;
    Real strike;
    Real forward;
    Real stdDev; // Absolute volatility
    Real discount;
    Real tolerance;
    Real refValue;
};

BOOST_AUTO_TEST_CASE(testBachelierCalculatorBasicValues)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator basic option values...");

    BachelierCalculatorTestData values[] = {
        // type, strike, forward, stdDev (absolute), discount, tolerance
        {Option::Call, 100.0, 100.0, 20.0, 1.0, 1e-8, 7.9788456080286538}, // ATM Call
        {Option::Put, 100.0, 100.0, 20.0, 1.0, 1e-8, 7.9788456080286538},  // ATM Put
        {Option::Call, 90.0, 100.0, 20.0, 1.0, 1e-8, 13.955931148026121},  // ITM Call
        {Option::Put, 110.0, 100.0, 20.0, 1.0, 1e-8, 13.955931148026121},  // ITM Put
        {Option::Call, 110.0, 100.0, 20.0, 1.0, 1e-8, 3.9559311480261217}, // OTM Call
        {Option::Put, 90.0, 100.0, 20.0, 1.0, 1e-8, 3.9559311480261217},   // OTM Put
        {Option::Call, 100.0, 100.0, 0.0, 1.0, 1e-8, 0.0},                 // Zero vol Call
        {Option::Put, 100.0, 100.0, 0.0, 1.0, 1e-8, 0.0},                  // Zero vol Put
        {Option::Call, 0.0, 100.0, 20.0, 1.0, 1e-8, 100.00000106923312},   // Zero strike
    };

    for (auto& data : values)
    {
        // Test constructor with Option::Type
        BachelierCalculator calc1(data.type, data.strike, data.forward, data.stdDev, data.discount);
        Real value1 = calc1.value();

        // Test constructor with Payoff
        ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(data.type, data.strike));
        BachelierCalculator calc2(payoff, data.forward, data.stdDev, data.discount);
        Real value2 = calc2.value();

        // Both constructors should give the same result
        Real error = std::fabs(value1 - value2);
        if (error > data.tolerance)
        {
            BOOST_ERROR("BachelierCalculator constructor mismatch for "
                        << (data.type == Option::Call ? "Call" : "Put") << " strike=" << data.strike
                        << " forward=" << data.forward << " stdDev=" << data.stdDev << " discount=" << data.discount
                        << " value1=" << value1 << " value2=" << value2 << " error=" << error);
        }

        Real error2 = std::fabs(value1 - data.refValue);
        if (error2 > data.tolerance)
        {
            BOOST_ERROR("BachelierCalculator constructor rf value error for "
                        << (data.type == Option::Call ? "Call" : "Put") << " strike=" << data.strike
                        << " forward=" << data.forward << " stdDev=" << data.stdDev << " discount=" << data.discount
                        << " value1=" << value1 << " value2=" << value2 << " error=" << error);
        }

        // Basic sanity checks
        if (data.stdDev == 0.0)
        {
            // With zero volatility, option value should be intrinsic value
            Real intrinsic = data.discount * std::max(0.0, data.type == Option::Call ? data.forward - data.strike :
                                                                                       data.strike - data.forward);
            if (std::fabs(value1 - intrinsic) > data.tolerance)
            {
                BOOST_ERROR("BachelierCalculator zero volatility test failed for "
                            << (data.type == Option::Call ? "Call" : "Put") << " expected=" << intrinsic
                            << " calculated=" << value1);
            }
        }

        // Option value should be non-negative
        if (value1 < -data.tolerance)
        {
            BOOST_ERROR("BachelierCalculator negative option value: " << value1);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBachelierCalculatorGreeks)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator Greeks calculations...");

    Real forward = 100.0;
    Real strike = 105.0;
    Real stdDev = 20.0; // Absolute volatility
    Real discount = 0.95;
    Real spot = 98.0;
    Real maturity = 1.0;
    Real tolerance = 1e-6;
    Real refDelta = 0.38900917408288;
    Real refDeltaFwd = 0.38122899060122245;
    Real refGamma = 0.019124047842706517;
    Real refGammaFwd = 0.018366735548135338;
    // #139: theta takes the forward's gamma, not the spot's, and vanna the discount factor.
    Real refTheta = -4.1644691862904235;
    Real refVega = 0.36733471096270676;
    Real refRho = 32.682349793874224;
    Real refElasticity = 7.0071783554334042;
    Real refElasticityFwd = 7.0071783554334051;
    Real refItmCashProb = 0.4012936743170763;
    // #195: N(d) + s n(d) / F, the asset-or-nothing value over D F, where it was the cash probability.
    Real refItmAssetProb = 0.4786272976776461;
    Real refDividendRho = -38.122899060122243;
    Real refStrikeSensitivity = -0.38122899060122245;
    Real refStrikeGamma = 0.018366735548135338;
    Real refVanna = 0.0045916838870338345;
    Real refVolga = 0.0011479209717584586;

    BachelierCalculator calc(Option::Call, strike, forward, stdDev, discount);

    Real delta = calc.delta(spot);
    Real deltaForward = calc.deltaForward();
    Real gamma = calc.gamma(spot);
    Real gammaForward = calc.gammaForward();
    Real theta = calc.theta(spot, maturity);
    Real vega = calc.vega(maturity);
    Real rho = calc.rho(maturity);
    Real elasticity = calc.elasticity(spot);
    Real elasticityForward = calc.elasticityForward();
    Real itmCashProb = calc.itmCashProbability();
    Real itmAssetProb = calc.itmAssetProbability();
    Real dividendRho = calc.dividendRho(maturity);
    Real strikeSensitivity = calc.strikeSensitivity();
    Real strikeGamma = calc.strikeGamma();
    Real vanna = calc.vanna(maturity);
    Real volga = calc.volga(maturity);

    if (std::fabs(deltaForward - refDeltaFwd) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call fwd delta error");
    }

    if (std::fabs(delta - refDelta) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call delta error");
    }

    if (std::fabs(gammaForward - refGammaFwd) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call fwd gamma error");
    }

    if (std::fabs(gamma - refGamma) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call gamma error");
    }

    if (std::fabs(theta - refTheta) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call theta error");
    }

    if (std::fabs(vega - refVega) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call vega error");
    }

    if (std::fabs(rho - refRho) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call rho error");
    }

    if (std::fabs(elasticityForward - refElasticityFwd) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call fwd elasticity error");
    }

    if (std::fabs(elasticity - refElasticity) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call elasticity error");
    }

    if (std::fabs(itmCashProb - refItmCashProb) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call itm cash probability error");
    }

    if (std::fabs(itmAssetProb - refItmAssetProb) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call itm asset probability error");
    }

    if (std::fabs(dividendRho - refDividendRho) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call dividend rho error");
    }

    if (std::fabs(strikeSensitivity - refStrikeSensitivity) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call strike sensitivity error");
    }

    if (std::fabs(strikeGamma - refStrikeGamma) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call strike gamma error");
    }

    if (std::fabs(vanna - refVanna) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call vanna error");
    }

    if (std::fabs(volga - refVolga) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call volga error");
    }
}

BOOST_AUTO_TEST_CASE(testBachelierCalculatorPutCallParity)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator put-call parity...");

    Real forward = 100.0;
    Real strike = 105.0;
    Real stdDev = 25.0; // Absolute volatility
    Real discount = 0.95;
    Real tolerance = 1e-10;

    BachelierCalculator callCalc(Option::Call, strike, forward, stdDev, discount);
    BachelierCalculator putCalc(Option::Put, strike, forward, stdDev, discount);

    Real callValue = callCalc.value();
    Real putValue = putCalc.value();

    // Put-Call parity: C - P = discount * (F - K)
    Real paritylhs = callValue - putValue;
    Real parityrhs = discount * (forward - strike);
    Real parityError = std::fabs(paritylhs - parityrhs);

    if (parityError > tolerance)
    {
        BOOST_ERROR("BachelierCalculator put-call parity violation: " << "C-P=" << paritylhs << " discount*(F-K)="
                                                                      << parityrhs << " error=" << parityError);
    }
}

BOOST_AUTO_TEST_CASE(testBachelierCalculatorEdgeCases)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator edge cases...");

    Real tolerance = 1e-8;

    // Test zero volatility
    {
        BachelierCalculator calc(Option::Call, 100.0, 100.0, 0.0, 1.0);
        Real value = calc.value();
        Real refValue = 0.0;
        if (std::fabs(value - refValue) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator failed for zero volatility: " << value);
        }
    }

    // Test very high volatility
    {
        BachelierCalculator calc(Option::Call, 100.0, 100.0, 200.0, 1.0);
        Real value = calc.value();
        Real refValue = 79.788456080286537;
        if (std::fabs(value - refValue) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator failed for very high volatility: " << value);
        }
    }

    // Test negative strikes (valid in Bachelier model)
    {
        BachelierCalculator calc(Option::Call, -50.0, 100.0, 20.0, 1.0);
        Real value = calc.value();
        Real intrinsicValue = 100.0 - (-50.0); // Should be close to intrinsic
        if (value < intrinsicValue - 10.0)
        { // Allow some time value
            BOOST_ERROR("BachelierCalculator negative strike call unreasonable: " << value << " vs intrinsic "
                                                                                  << intrinsicValue);
        }
    }

    // Test negative forward (valid in Bachelier model)
    {
        BachelierCalculator calc(Option::Call, 50.0, -100.0, 20.0, 1.0);
        Real value = calc.value();
        Real intrinsicValue = -100.0 - 50.0; // Should be close to intrinsic
        if (value < intrinsicValue + 10.0)
        { // Allow some time value
            BOOST_ERROR("BachelierCalculator negative strike call unreasonable: " << value << " vs intrinsic "
                                                                                  << intrinsicValue);
        }
    }

    // Test deep ITM call
    {
        BachelierCalculator calc(Option::Call, 50.0, 100.0, 20.0, 1.0);
        Real value = calc.value();
        Real intrinsicValue = 100.0 - 50.0; // Should be close to intrinsic
        if (value < intrinsicValue - tolerance)
        {
            BOOST_ERROR("BachelierCalculator deep ITM call below intrinsic: " << value << " vs " << intrinsicValue);
        }
    }

    // Test deep OTM call
    {
        BachelierCalculator calc(Option::Call, 150.0, 100.0, 20.0, 1.0);
        Real value = calc.value();
        Real refValue = 0.040082743582562863;
        if (std::fabs(value - refValue) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator deep OTM call negative: " << value);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBachelierCalculatorNumericalDerivatives)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator numerical derivative consistency...");

    Real forward = 100.0;
    Real strike = 100.0;
    Real stdDev = 20.0; // Absolute volatility
    Real discount = 0.95;
    Real maturity = 1.0;
    Real bump = 1e-4;
    Real tolerance = 1e-3;

    BachelierCalculator calc(Option::Call, strike, forward, stdDev, discount);

    // Test delta via finite differences
    BachelierCalculator calcUp(Option::Call, strike, forward + bump, stdDev, discount);
    BachelierCalculator calcDown(Option::Call, strike, forward - bump, stdDev, discount);

    Real analyticalDelta = calc.deltaForward();
    Real numericalDelta = (calcUp.value() - calcDown.value()) / (2.0 * bump);
    Real deltaError = std::fabs(analyticalDelta - numericalDelta);

    if (deltaError > tolerance)
    {
        BOOST_ERROR("BachelierCalculator delta finite difference test failed: "
                    << "analytical=" << analyticalDelta << " numerical=" << numericalDelta << " error=" << deltaError);
    }

    // Test gamma via finite differences
    Real analyticalGamma = calc.gammaForward();
    Real numericalGamma = (calcUp.deltaForward() - calcDown.deltaForward()) / (2.0 * bump);
    Real gammaError = std::fabs(analyticalGamma - numericalGamma);

    if (gammaError > tolerance)
    {
        BOOST_ERROR("BachelierCalculator gamma finite difference test failed: "
                    << "analytical=" << analyticalGamma << " numerical=" << numericalGamma << " error=" << gammaError);
    }

    // Test vega via finite differences
    BachelierCalculator calcVolUp(Option::Call, strike, forward, stdDev + bump, discount);
    BachelierCalculator calcVolDown(Option::Call, strike, forward, stdDev - bump, discount);

    Real analyticalVega = calc.vega(maturity);
    Real numericalVega = (calcVolUp.value() - calcVolDown.value()) / (2.0 * bump);
    Real vegaError = std::fabs(analyticalVega - numericalVega * std::sqrt(maturity));

    if (vegaError > tolerance)
    {
        BOOST_ERROR("BachelierCalculator vega finite difference test failed: "
                    << "analytical=" << analyticalVega << " numerical=" << numericalVega * std::sqrt(maturity)
                    << " error=" << vegaError);
    }

    // --- Vanna and Volga tests ---
    // Analytical values
    Real vanna = calc.vanna(maturity);
    Real volga = calc.volga(maturity);

    // Finite difference for vanna: dVega/dForward
    BachelierCalculator calcFwdUp(Option::Call, strike, forward + bump, stdDev, discount);
    BachelierCalculator calcFwdDown(Option::Call, strike, forward - bump, stdDev, discount);
    Real vegaFwdUp = calcFwdUp.vega(maturity);
    Real vegaFwdDown = calcFwdDown.vega(maturity);
    Real vannaFD = (vegaFwdUp - vegaFwdDown) / (2.0 * bump);

    if (std::fabs(vanna - vannaFD) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call vanna error: analytical=" << vanna << " finite-diff=" << vannaFD
                                                                        << " error=" << (vanna - vannaFD));
    }

    // Finite difference for volga: dVega/dVol, as the standard deviation is vol * sqrt(T)
    Real vegaVolUp = calcVolUp.vega(maturity);
    Real vegaVolDown = calcVolDown.vega(maturity);
    Real volgaFD = (vegaVolUp - vegaVolDown) / (2.0 * bump) * std::sqrt(maturity);

    if (std::fabs(volga - volgaFD) > tolerance)
    {
        BOOST_ERROR("BachelierCalculator call volga error: analytical=" << volga << " finite-diff=" << volgaFD
                                                                        << " error=" << (volga - volgaFD));
    }
}

BOOST_AUTO_TEST_CASE(testBachelierCalculatorAgainstAnalyticalFormula)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator against analytical Bachelier formula...");

    Real forward = 100.0;
    Real strike = 95.0;
    Real stdDev = 15.0; // Absolute volatility
    Real discount = 0.98;
    Real tolerance = 1e-10;

    BachelierCalculator calc(Option::Call, strike, forward, stdDev, discount);
    Real calculatedValue = calc.value();

    // Analytical Bachelier formula: C = (F-K)*N(d) + ?*n(d)
    // where d = (F-K)/?
    Real d = (forward - strike) / stdDev;
    CumulativeNormalDistribution N;
    NormalDistribution n;

    Real analyticalValue = discount * ((forward - strike) * N(d) + stdDev * n(d));
    Real error = std::fabs(calculatedValue - analyticalValue);

    if (error > tolerance)
    {
        BOOST_ERROR("BachelierCalculator analytical formula test failed: "
                    << "calculated=" << calculatedValue << " analytical=" << analyticalValue << " error=" << error);
    }
}

BOOST_AUTO_TEST_CASE(testBachelierCalculatorZeroVolatilityGreeks)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator Greeks with zero volatility...");

    Real tolerance = 1e-10;
    Real forward = 100.0;
    Real discount = 1.0;
    Real spot = 98.0;
    Real maturity = 1.0;
    Real stdDev = 0.0; // Zero absolute volatility

    // Test different moneyness scenarios
    struct ZeroVolTestCase
    {
        Option::Type type;
        Real strike;
        std::string description;
        Real expectedDelta;
        Real expectedGamma;
        Real expectedVega;
        Real expectedTheta;
    };

    ZeroVolTestCase testCases[] = {// ITM options should have delta = 1 for calls, -1 for puts (approximately)
                                   {Option::Call, 90.0, "ITM Call", 1.0, 0.0, 0.0, 0.0},
                                   {Option::Put, 110.0, "ITM Put", -1.0, 0.0, 0.0, 0.0},
                                   // ATM options in Bachelier model
                                   {Option::Call, 100.0, "ATM Call", 0.5, 0.0, 0.0, 0.0},
                                   {Option::Put, 100.0, "ATM Put", -0.5, 0.0, 0.0, 0.0},
                                   // OTM options should have delta = 0
                                   {Option::Call, 90.0, "OTM Call", 0.0, 0.0, 0.0, 0.0},
                                   {Option::Put, 110.0, "OTM Put", 0.0, 0.0, 0.0, 0.0},
                                   // Test negative strikes (valid in Bachelier model)
                                   {Option::Call, -10.0, "Negative Strike Call", 1.0, 0.0, 0.0, 0.0},
                                   {Option::Put, 200.0, "High Strike Put", -1.0, 0.0, 0.0, 0.0}};

    for (const auto& testCase : testCases)
    {
        BachelierCalculator calc(testCase.type, testCase.strike, forward, stdDev, discount);

        Real deltaForward = calc.deltaForward();
        Real delta = calc.delta(spot);
        Real gammaForward = calc.gammaForward();
        Real gamma = calc.gamma(spot);
        Real vega = calc.vega(maturity);
        Real theta = calc.theta(spot, maturity);
        Real rho = calc.rho(maturity);
        Real dividendRho = calc.dividendRho(maturity);

        // All Greeks should be finite (not NaN or infinite)
        if (!std::isfinite(deltaForward) || !std::isfinite(delta) || !std::isfinite(gammaForward) ||
            !std::isfinite(gamma) || !std::isfinite(vega) || !std::isfinite(theta) || !std::isfinite(rho) ||
            !std::isfinite(dividendRho))
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description
                                               << " produced non-finite Greeks with zero volatility");
        }

        // Gamma should be zero (no convexity with zero vol)
        if (std::fabs(gammaForward) > tolerance || std::fabs(gamma) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description << " gamma should be zero with zero volatility: "
                                               << "gammaForward=" << gammaForward << " gamma=" << gamma);
        }

        // Vega should be zero (no vol sensitivity)
        if (std::fabs(vega) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description
                                               << " vega should be zero with zero volatility: " << vega);
        }

        // For clearly ITM/OTM cases, check delta bounds
        if (testCase.strike < forward - 5.0)
        { // Clearly ITM call
            if (testCase.type == Option::Call && (deltaForward < 0.99 || deltaForward > 1.01))
            {
                BOOST_ERROR("BachelierCalculator ITM call deltaForward should be ~1.0 with zero vol: " << deltaForward);
            }
        }
        if (testCase.strike > forward + 5.0)
        { // Clearly OTM call
            if (testCase.type == Option::Call && std::fabs(deltaForward) > tolerance)
            {
                BOOST_ERROR("BachelierCalculator OTM call deltaForward should be ~0.0 with zero vol: " << deltaForward);
            }
        }

        // Strike sensitivities should be finite
        Real strikeSens = calc.strikeSensitivity();
        Real strikeGamma = calc.strikeGamma();

        if (!std::isfinite(strikeSens) || !std::isfinite(strikeGamma))
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description
                                               << " strike sensitivities should be finite with zero volatility");
        }

        // Test ITM probabilities
        Real itmCashProb = calc.itmCashProbability();
        Real itmAssetProb = calc.itmAssetProbability();

        if (!std::isfinite(itmCashProb) || !std::isfinite(itmAssetProb))
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description
                                               << " ITM probabilities should be finite with zero volatility");
        }

        // In Bachelier model with zero vol, ITM probabilities should be 0 or 1
        Real expectedProb = (testCase.type == Option::Call) ?
                                (forward > testCase.strike ? 1.0 : (forward == testCase.strike ? 0.5 : 0.0)) :
                                (forward < testCase.strike ? 1.0 : (forward == testCase.strike ? 0.5 : 0.0));

        if (std::fabs(itmCashProb - expectedProb) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description
                                               << " ITM cash probability incorrect with zero vol: expected="
                                               << expectedProb << " actual=" << itmCashProb);
        }

        Real vanna = calc.vanna(maturity);
        Real volga = calc.volga(maturity);

        if (std::fabs(vanna) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description
                                               << " vanna should be zero with zero volatility: " << vanna);
        }
        if (std::fabs(volga) > tolerance)
        {
            BOOST_ERROR("BachelierCalculator " << testCase.description
                                               << " volga should be zero with zero volatility: " << volga);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBachelierVsBlackConvergence)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator convergence to BlackCalculator for small relative volatilities...");

    Real forward = 100.0;
    Real strike = 100.0;
    Real relativeVol = 0.01;                  // 1% relative volatility
    Real absoluteVol = relativeVol * forward; // Convert to absolute
    Real discount = 1.0;
    Real tolerance = 1e-2; // Looser tolerance for convergence test

    BachelierCalculator bachelierCalc(Option::Call, strike, forward, absoluteVol, discount);
    BlackCalculator blackCalc(Option::Call, strike, forward, relativeVol, discount);

    Real bachelierValue = bachelierCalc.value();
    Real blackValue = blackCalc.value();

    // For small relative volatilities, Bachelier should approximate Black-Scholes
    Real error = std::fabs(bachelierValue - blackValue);
    Real relativeError = error / blackValue;

    if (relativeError > tolerance)
    {
        BOOST_ERROR("BachelierCalculator vs BlackCalculator convergence test failed: "
                    << "bachelier=" << bachelierValue << " black=" << blackValue
                    << " relative error=" << relativeError);
    }
}

BOOST_AUTO_TEST_CASE(testBachelierCalculatorZeroVolatilityLimit)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator at zero volatility against a tiny one...");

    const Real spot = 100.0, strike = 100.0, discount = 0.9, tiny = 1.0e-9;
    const Time maturity = 1.0;

    for (Option::Type type : {Option::Call, Option::Put})
    {
        for (Real forward : {120.0, 100.0, 80.0})
        {
            const BachelierCalculator zero(type, strike, forward, 0.0, discount);
            const BachelierCalculator limit(type, strike, forward, tiny, discount);
            BOOST_TEST_CONTEXT(type << ", forward " << forward)
            {
                BOOST_CHECK_SMALL(zero.value() - limit.value(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.delta(spot) - limit.delta(spot), 1.0e-9);
                BOOST_CHECK_SMALL(zero.deltaForward() - limit.deltaForward(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.strikeSensitivity() - limit.strikeSensitivity(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.rho(maturity) - limit.rho(maturity), 1.0e-7);
                BOOST_CHECK_SMALL(zero.dividendRho(maturity) - limit.dividendRho(maturity), 1.0e-7);
                BOOST_CHECK_SMALL(zero.itmCashProbability() - limit.itmCashProbability(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.itmAssetProbability() - limit.itmAssetProbability(), 1.0e-9);
            }
        }
    }

    // An out-of-the-money put is worth nothing, and it isn't a call.
    const BachelierCalculator put(Option::Put, strike, 120.0, 0.0, discount);
    BOOST_CHECK_EQUAL(put.value(), 0.0);
    BOOST_CHECK_EQUAL(put.deltaForward(), 0.0);
    BOOST_CHECK_EQUAL(put.itmCashProbability(), 0.0);
}

BOOST_AUTO_TEST_CASE(testDigitalPayoffValues)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator values of digital and gap payoffs...");

    // #139: every payoff was valued as the vanilla option, 10.042454 in the first four rows.  The references
    // integrate the payoff against the normal density.
    struct Case
    {
        ext::shared_ptr<StrikedTypePayoff> payoff;
        Real forward, stdDev, discount, expected;
    };
    const Case cases[] = {
        {ext::make_shared<PlainVanillaPayoff>(Option::Call, 100.0), 110.0, 5.0, 1.0, 10.042453513084148},
        {ext::make_shared<CashOrNothingPayoff>(Option::Call, 100.0, 10.0), 110.0, 5.0, 1.0, 9.7724986805182079},
        {ext::make_shared<AssetOrNothingPayoff>(Option::Call, 100.0), 110.0, 5.0, 1.0, 107.76744031826623},
        {ext::make_shared<GapPayoff>(Option::Call, 100.0, 90.0), 110.0, 5.0, 1.0, 19.814952193602356},
        {ext::make_shared<CashOrNothingPayoff>(Option::Put, 100.0, 10.0), 110.0, 5.0, 0.9, 0.20475118753361286},
        {ext::make_shared<AssetOrNothingPayoff>(Option::Put, 100.0), 110.0, 5.0, 0.9, 2.0093037135603953},
        {ext::make_shared<GapPayoff>(Option::Put, 100.0, 105.0), 95.0, 5.0, 0.9, 8.6609709749530316},
        // Worth less than nothing: the asset-or-nothing put pays the forward where it is negative, and the gap call
        // pays it less 110 where it is between 100 and 110.
        {ext::make_shared<AssetOrNothingPayoff>(Option::Put, 0.0), 10.0, 20.0, 0.9, -3.5603380332235085},
        {ext::make_shared<GapPayoff>(Option::Call, 100.0, 110.0), 100.0, 5.0, 0.9, -2.7047597381935529}};

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.payoff->description() << ", forward " << c.forward)
        {
            const BachelierCalculator calculator(c.payoff, c.forward, c.stdDev, c.discount);
            // A put takes N(-d) as 1 - N(d): at d = 2, the last bit of N(2), which compilers can round apart,
            // moves it by 5e-15.
            BOOST_CHECK_CLOSE_FRACTION(calculator.value(), c.expected, 1.0e-13);
        }
    }

    // A call and a put on the same strike pay the cash, or the forward, whatever happens.
    for (Real forward : {-20.0, 95.0, 130.0})
    {
        for (Real stdDev : {1.0, 20.0})
        {
            BOOST_TEST_CONTEXT("forward " << forward << ", standard deviation " << stdDev)
            {
                const Real discount = 0.9, strike = 100.0, cash = 10.0;
                const auto value = [&](const ext::shared_ptr<StrikedTypePayoff>& payoff)
                { return BachelierCalculator(payoff, forward, stdDev, discount).value(); };
                BOOST_CHECK_CLOSE_FRACTION(value(ext::make_shared<CashOrNothingPayoff>(Option::Call, strike, cash)) +
                                               value(ext::make_shared<CashOrNothingPayoff>(Option::Put, strike, cash)),
                                           discount * cash, 1.0e-14);
                BOOST_CHECK_CLOSE_FRACTION(value(ext::make_shared<AssetOrNothingPayoff>(Option::Call, strike)) +
                                               value(ext::make_shared<AssetOrNothingPayoff>(Option::Put, strike)),
                                           discount * forward, 1.0e-14);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testDigitalPayoffGreeks)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator Greeks of every payoff against finite differences...");

    // #139: every payoff took the vanilla option's Greeks.  The spot and forward are apart, the discount factor
    // isn't one, and the expiry isn't a year, so that vanna's discounting, volga's sqrt(T) and theta's gamma show.
    const Real spot = 98.0, r = 0.03, q = 0.01, vol = 20.0, h = 1.0e-3, hRate = 1.0e-5, hTime = 1.0e-5;
    const Time maturity = 0.5;

    const auto payoff = [](int kind, Option::Type type, Real strike) -> ext::shared_ptr<StrikedTypePayoff>
    {
        switch (kind)
        {
            case 0:
                return ext::make_shared<PlainVanillaPayoff>(type, strike);
            case 1:
                return ext::make_shared<CashOrNothingPayoff>(type, strike, 10.0);
            case 2:
                return ext::make_shared<AssetOrNothingPayoff>(type, strike);
            default:
                return ext::make_shared<GapPayoff>(type, strike, 95.0);
        }
    };

    for (int kind = 0; kind < 4; ++kind)
    {
        for (Option::Type type : {Option::Call, Option::Put})
        {
            for (Real strike : {90.0, 105.0})
            {
                // A calculator from the market: the volatility is the forward's, normal.
                const auto at = [&](Real s, Real rate, Real dividend, Real v, Time t, Real k)
                {
                    return BachelierCalculator(payoff(kind, type, k), s * std::exp((rate - dividend) * t),
                                               v * std::sqrt(t), std::exp(-rate * t));
                };
                const BachelierCalculator c = at(spot, r, q, vol, maturity, strike);
                const BachelierCalculator spotUp = at(spot + h, r, q, vol, maturity, strike),
                                          spotDown = at(spot - h, r, q, vol, maturity, strike),
                                          volUp = at(spot, r, q, vol + h, maturity, strike),
                                          volDown = at(spot, r, q, vol - h, maturity, strike),
                                          strikeUp = at(spot, r, q, vol, maturity, strike + h),
                                          strikeDown = at(spot, r, q, vol, maturity, strike - h);
                // The forward moves at the same discount factor and standard deviation.
                const Real forward = spot * std::exp((r - q) * maturity), discount = std::exp(-r * maturity),
                           stdDev = vol * std::sqrt(maturity);
                const BachelierCalculator forwardUp(payoff(kind, type, strike), forward + h, stdDev, discount),
                    forwardDown(payoff(kind, type, strike), forward - h, stdDev, discount);

                const auto check = [](const std::string& greek, Real analytic, Real numeric)
                {
                    BOOST_TEST_CONTEXT(greek)
                    {
                        BOOST_CHECK_SMALL(analytic - numeric, 1.0e-6 * std::fabs(numeric) + 1.0e-9);
                    }
                };
                BOOST_TEST_CONTEXT(payoff(kind, type, strike)->description())
                {
                    check("deltaForward", c.deltaForward(), (forwardUp.value() - forwardDown.value()) / (2 * h));
                    check("gammaForward", c.gammaForward(),
                          (forwardUp.deltaForward() - forwardDown.deltaForward()) / (2 * h));
                    check("vanna", c.vanna(maturity),
                          (forwardUp.vega(maturity) - forwardDown.vega(maturity)) / (2 * h));
                    check("delta", c.delta(spot), (spotUp.value() - spotDown.value()) / (2 * h));
                    check("gamma", c.gamma(spot), (spotUp.delta(spot + h) - spotDown.delta(spot - h)) / (2 * h));
                    check("vega", c.vega(maturity), (volUp.value() - volDown.value()) / (2 * h));
                    check("volga", c.volga(maturity), (volUp.vega(maturity) - volDown.vega(maturity)) / (2 * h));
                    check("theta", c.theta(spot, maturity),
                          -(at(spot, r, q, vol, maturity + hTime, strike).value() -
                            at(spot, r, q, vol, maturity - hTime, strike).value()) /
                              (2 * hTime));
                    check("rho", c.rho(maturity),
                          (at(spot, r + hRate, q, vol, maturity, strike).value() -
                           at(spot, r - hRate, q, vol, maturity, strike).value()) /
                              (2 * hRate));
                    check("dividendRho", c.dividendRho(maturity),
                          (at(spot, r, q + hRate, vol, maturity, strike).value() -
                           at(spot, r, q - hRate, vol, maturity, strike).value()) /
                              (2 * hRate));
                    check("strikeSensitivity", c.strikeSensitivity(),
                          (strikeUp.value() - strikeDown.value()) / (2 * h));
                    check("strikeGamma", c.strikeGamma(),
                          (strikeUp.strikeSensitivity() - strikeDown.strikeSensitivity()) / (2 * h));
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testDigitalPayoffsAtZeroVolatility)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator digital and gap payoffs at zero volatility...");

    const Real spot = 100.0, strike = 100.0, discount = 0.9, tiny = 1.0e-9;
    const Time maturity = 1.0;

    // With the forward at the strike, the value takes N(0) = 1/2, and the Greeks leave out the payoff's jump
    // there, which would put an infinite term in the delta.  So the forward delta is the discount factor times the
    // payoff's slope, over two.
    struct Case
    {
        ext::shared_ptr<StrikedTypePayoff> payoff;
        Real valueAtStrike, deltaForwardAtStrike;
    };
    const Case cases[] = {{ext::make_shared<CashOrNothingPayoff>(Option::Call, strike, 10.0), 4.5, 0.0},
                          {ext::make_shared<CashOrNothingPayoff>(Option::Put, strike, 10.0), 4.5, 0.0},
                          {ext::make_shared<AssetOrNothingPayoff>(Option::Call, strike), 45.0, 0.45},
                          {ext::make_shared<AssetOrNothingPayoff>(Option::Put, strike), 45.0, 0.45},
                          {ext::make_shared<GapPayoff>(Option::Call, strike, 90.0), 4.5, 0.45},
                          {ext::make_shared<GapPayoff>(Option::Put, strike, 110.0), 4.5, -0.45}};

    for (const auto& c : cases)
    {
        // Away from the strike, as with a tiny volatility.
        for (Real forward : {120.0, 80.0})
        {
            const BachelierCalculator zero(c.payoff, forward, 0.0, discount), limit(c.payoff, forward, tiny, discount);
            BOOST_TEST_CONTEXT(c.payoff->description() << ", forward " << forward)
            {
                BOOST_CHECK_SMALL(zero.value() - limit.value(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.delta(spot) - limit.delta(spot), 1.0e-9);
                BOOST_CHECK_SMALL(zero.deltaForward() - limit.deltaForward(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.gammaForward() - limit.gammaForward(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.vega(maturity) - limit.vega(maturity), 1.0e-9);
                BOOST_CHECK_SMALL(zero.strikeSensitivity() - limit.strikeSensitivity(), 1.0e-9);
                BOOST_CHECK_SMALL(zero.rho(maturity) - limit.rho(maturity), 1.0e-7);
                BOOST_CHECK_SMALL(zero.dividendRho(maturity) - limit.dividendRho(maturity), 1.0e-7);
            }
        }

        const BachelierCalculator atStrike(c.payoff, strike, 0.0, discount);
        BOOST_TEST_CONTEXT(c.payoff->description() << ", forward at the strike")
        {
            BOOST_CHECK_CLOSE_FRACTION(atStrike.value(), c.valueAtStrike, 1.0e-14);
            BOOST_CHECK_SMALL(atStrike.deltaForward() - c.deltaForwardAtStrike, 1.0e-15);
            BOOST_CHECK_EQUAL(atStrike.gammaForward(), 0.0);
            BOOST_CHECK_EQUAL(atStrike.vega(maturity), 0.0);
            BOOST_CHECK_EQUAL(atStrike.strikeGamma(), 0.0);
        }
    }
}

BOOST_AUTO_TEST_CASE(testItmAssetProbability)
{
    BOOST_TEST_MESSAGE("Testing BachelierCalculator's asset probability...");

    // It's E[F_T 1{in the money}] / F, the asset-or-nothing value over D F, as BlackCalculator's is
    // for a call (BlackCalculator's gives a put the call's, #235); it was the cash probability
    // (#195).  The forward is normal, so it needn't lie in [0, 1].
    const Real discount = 0.95;
    for (Option::Type type : {Option::Call, Option::Put})
    {
        for (Real forward : {100.0, 1.0, -10.0, 0.5})
        {
            for (Real strike : {105.0, 95.0, 0.0, -20.0})
            {
                for (Real stdDev : {20.0, 1.0, 0.5, 0.0})
                {
                    BOOST_TEST_CONTEXT(type << ", forward " << forward << ", strike " << strike << ", stdDev "
                                            << stdDev)
                    {
                        const BachelierCalculator calc(type, strike, forward, stdDev, discount);
                        const BachelierCalculator assetOrNothing(ext::make_shared<AssetOrNothingPayoff>(type, strike),
                                                                 forward, stdDev, discount);
                        BOOST_CHECK_SMALL(calc.itmAssetProbability() * discount * forward - assetOrNothing.value(),
                                          1.0e-14 * discount * (std::fabs(forward) + stdDev));
                        // An asset-or-nothing call and put together pay F_T.
                        const Option::Type other = type == Option::Call ? Option::Put : Option::Call;
                        const BachelierCalculator otherCalc(other, strike, forward, stdDev, discount);
                        BOOST_CHECK_SMALL(calc.itmAssetProbability() + otherCalc.itmAssetProbability() - 1.0,
                                          1.0e-14 * (1.0 + stdDev / std::fabs(forward)));
                        // At zero volatility it's the cash probability.
                        if (stdDev == 0.0)
                            BOOST_CHECK_EQUAL(calc.itmAssetProbability(), calc.itmCashProbability());
                    }
                }
            }
        }
    }

    // #195's case: the spot at 98, r 3 %, q 1 %, a normal volatility of 20, a strike of 105 and
    // a year, against 60-digit references.
    const Real forward = 98.0 * std::exp(0.02);
    const BachelierCalculator call(Option::Call, 105.0, forward, 20.0, std::exp(-0.03));
    BOOST_CHECK_SMALL(call.itmCashProbability() - 0.40090186145520748, 1.0e-14);
    BOOST_CHECK_SMALL(call.itmAssetProbability() - 0.47823152816738486, 1.0e-14);

    // Not a probability: above 1 for a call and below 0 for a put, with F 1, K 0 and s 20.
    BOOST_CHECK_CLOSE_FRACTION(BachelierCalculator(Option::Call, 0.0, 1.0, 20.0).itmAssetProbability(),
                               8.48881708773365, 1.0e-14);
    BOOST_CHECK_CLOSE_FRACTION(BachelierCalculator(Option::Put, 0.0, 1.0, 20.0).itmAssetProbability(),
                               -7.48881708773365, 1.0e-14);

    // No asset measure at all at a forward of 0.
    BOOST_CHECK_THROW(BachelierCalculator(Option::Call, 0.0, 0.0, 20.0).itmAssetProbability(), Error);
}


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()