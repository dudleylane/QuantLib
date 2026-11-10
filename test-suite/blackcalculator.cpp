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
#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/comparison.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/pricingengines/blackscholescalculator.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <cmath>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackCalculatorTests)

struct BlackCalculatorTestData
{
    Option::Type type;
    Real strike;
    Real forward;
    Real stdDev;
    Real discount;
    Real tolerance;
    Real refValue;
};

BOOST_AUTO_TEST_CASE(testBlackCalculatorBasicValues)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator basic option values...");

    BlackCalculatorTestData values[] = {
        // type, strike, forward, stdDev, discount, tolerance
        {Option::Call, 100.0, 100.0, 0.20, 1.0, 1e-8, 7.9655674554058038}, // ATM Call
        {Option::Put, 100.0, 100.0, 0.20, 1.0, 1e-8, 7.9655674554058038},  // ATM Put
        {Option::Call, 90.0, 100.0, 0.20, 1.0, 1e-8, 13.589108116054803},  // ITM Call
        {Option::Put, 110.0, 100.0, 0.20, 1.0, 1e-8, 14.292010941409899},  // ITM Put
        {Option::Call, 110.0, 100.0, 0.20, 1.0, 1e-8, 4.2920109414098846}, // OTM Call
        {Option::Put, 90.0, 100.0, 0.20, 1.0, 1e-8, 3.5891081160548062},   // OTM Put
        {Option::Call, 100.0, 100.0, 0.0, 1.0, 1e-8, 0.0},                 // Zero vol Call
        {Option::Put, 100.0, 100.0, 0.0, 1.0, 1e-8, 0.0},                  // Zero vol Put
    };

    for (auto& data : values)
    {
        // Test constructor with Option::Type
        BlackCalculator calc1(data.type, data.strike, data.forward, data.stdDev, data.discount);
        Real value1 = calc1.value();

        // Test constructor with Payoff
        ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(data.type, data.strike));
        BlackCalculator calc2(payoff, data.forward, data.stdDev, data.discount);
        Real value2 = calc2.value();

        // Both constructors should give the same result
        Real error = std::fabs(value1 - value2);
        if (error > data.tolerance)
        {
            BOOST_ERROR("BlackCalculator constructor mismatch for "
                        << (data.type == Option::Call ? "Call" : "Put") << " strike=" << data.strike
                        << " forward=" << data.forward << " stdDev=" << data.stdDev << " discount=" << data.discount
                        << " value1=" << value1 << " value2=" << value2 << " error=" << error);
        }

        Real error2 = std::fabs(value1 - data.refValue);
        if (error2 > data.tolerance)
        {
            BOOST_ERROR("BlackCalculator constructor rf value error for "
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
                BOOST_ERROR("BlackCalculator zero volatility test failed for "
                            << (data.type == Option::Call ? "Call" : "Put") << " expected=" << intrinsic
                            << " calculated=" << value1);
            }
        }

        // Option value should be non-negative
        if (value1 < -data.tolerance)
        {
            BOOST_ERROR("BlackCalculator negative option value: " << value1);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBlackCalculatorGreeks)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator Greeks calculations...");

    Real forward = 100.0;
    Real strike = 105.0;
    Real stdDev = 0.20;
    Real discount = 0.95;
    Real spot = 98.0;
    Real maturity = 1.0;
    Real tolerance = 1e-6;
    Real refDelta = 0.42921547913932068;
    Real refDeltaFwd = 0.42063116955653351;
    Real refGamma = 0.019527733248736884;
    Real refGammaFwd = 0.018754435012086908;
    Real refTheta = -4.31290436588883;
    Real refVega = 37.508870024173795;
    Real refRho = 36.452803157675653;
    Real refElasticity = 7.4974624362037323;
    Real refElasticityFwd = 7.4974624362037199;
    Real refItmCashProb = 0.36544163566592136;
    Real refItmAssetProb = 0.44276965216477238;
    Real refDividendRho = -42.063116955653371;
    Real refStrikeSensitivity = -0.34716955388262527;
    Real refStrikeGamma = 0.017010825407788574;
    Real refVanna = 0.65822482825836837;
    Real refVolga = 9.2856964243177753;

    BlackCalculator calc(Option::Call, strike, forward, stdDev, discount);

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
    Real vanna = calc.vanna(spot, maturity);
    Real volga = calc.volga(maturity);

    if (std::fabs(deltaForward - refDeltaFwd) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call fwd delta error");
    }

    if (std::fabs(delta - refDelta) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call delta error");
    }

    if (std::fabs(gammaForward - refGammaFwd) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call fwd gamma error");
    }

    if (std::fabs(gamma - refGamma) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call gamma error");
    }

    if (std::fabs(theta - refTheta) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call theta error");
    }

    if (std::fabs(vega - refVega) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call vega error");
    }

    if (std::fabs(rho - refRho) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call rho error");
    }

    if (std::fabs(elasticityForward - refElasticityFwd) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call fwd elasticity error");
    }

    if (std::fabs(elasticity - refElasticity) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call elasticity error");
    }

    if (std::fabs(itmCashProb - refItmCashProb) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call itm cash probability error");
    }

    if (std::fabs(itmAssetProb - refItmAssetProb) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call itm asset probability error");
    }

    if (std::fabs(dividendRho - refDividendRho) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call dividend rho error");
    }

    if (std::fabs(strikeSensitivity - refStrikeSensitivity) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call strike sensitivity error");
    }

    if (std::fabs(strikeGamma - refStrikeGamma) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call strike gamma error");
    }

    if (std::fabs(vanna - refVanna) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call vanna error");
    }

    if (std::fabs(volga - refVolga) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call volga error");
    }
}

BOOST_AUTO_TEST_CASE(testBlackCalculatorPutCallParity)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator put-call parity...");

    Real forward = 100.0;
    Real strike = 105.0;
    Real stdDev = 0.25;
    Real discount = 0.95;
    Real tolerance = 1e-10;

    BlackCalculator callCalc(Option::Call, strike, forward, stdDev, discount);
    BlackCalculator putCalc(Option::Put, strike, forward, stdDev, discount);

    Real callValue = callCalc.value();
    Real putValue = putCalc.value();

    // Put-Call parity: C - P = discount * (F - K)
    Real paritylhs = callValue - putValue;
    Real parityrhs = discount * (forward - strike);
    Real parityError = std::fabs(paritylhs - parityrhs);

    if (parityError > tolerance)
    {
        BOOST_ERROR("BlackCalculator put-call parity violation: " << "C-P=" << paritylhs << " discount*(F-K)="
                                                                  << parityrhs << " error=" << parityError);
    }
}

BOOST_AUTO_TEST_CASE(testBlackCalculatorEdgeCases)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator edge cases...");

    Real tolerance = 1e-10;

    // Test zero volatility
    {
        BlackCalculator calc(Option::Call, 100.0, 100.0, 0.0, 1.0);
        Real value = calc.value();
        Real refValue = 0.0;
        if (std::fabs(value - refValue) > tolerance)
        {
            BOOST_ERROR("BlackCalculator failed for zero volatility: " << value);
        }
    }

    // Test very high volatility
    {
        BlackCalculator calc(Option::Call, 100.0, 100.0, 2.0, 1.0);
        Real value = calc.value();
        Real refValue = 68.268949213708595;
        if (std::fabs(value - refValue) > tolerance)
        {
            BOOST_ERROR("BlackCalculator failed for very high volatility: " << value);
        }
    }

    // Test deep ITM call
    {
        BlackCalculator calc(Option::Call, 50.0, 100.0, 0.20, 1.0);
        Real value = calc.value();
        Real intrinsicValue = 100.0 - 50.0; // Should be close to intrinsic
        if (value < intrinsicValue - tolerance)
        {
            BOOST_ERROR("BlackCalculator deep ITM call below intrinsic: " << value << " vs " << intrinsicValue);
        }
    }

    // Test deep OTM call
    {
        BlackCalculator calc(Option::Call, 150.0, 100.0, 0.20, 1.0);
        Real value = calc.value();
        if (value < 0 || value > 10.0)
        { // Should be small positive value
            BOOST_ERROR("BlackCalculator deep OTM call unreasonable: " << value);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBlackCalculatorNumericalDerivatives)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator numerical derivative consistency...");

    Real forward = 100.0;
    Real strike = 100.0;
    Real stdDev = 0.20;
    Real discount = 0.95;
    Real maturity = 1.0;
    Real bump = 1e-4;
    Real tolerance = 1e-3;

    BlackCalculator calc(Option::Call, strike, forward, stdDev, discount);

    // Test delta via finite differences
    BlackCalculator calcUp(Option::Call, strike, forward + bump, stdDev, discount);
    BlackCalculator calcDown(Option::Call, strike, forward - bump, stdDev, discount);

    Real analyticalDelta = calc.deltaForward();
    Real numericalDelta = (calcUp.value() - calcDown.value()) / (2.0 * bump);
    Real deltaError = std::fabs(analyticalDelta - numericalDelta);

    if (deltaError > tolerance)
    {
        BOOST_ERROR("BlackCalculator delta finite difference test failed: "
                    << "analytical=" << analyticalDelta << " numerical=" << numericalDelta << " error=" << deltaError);
    }

    // Test gamma via finite differences
    Real analyticalGamma = calc.gammaForward();
    Real numericalGamma = (calcUp.deltaForward() - calcDown.deltaForward()) / (2.0 * bump);
    Real gammaError = std::fabs(analyticalGamma - numericalGamma);

    if (gammaError > tolerance)
    {
        BOOST_ERROR("BlackCalculator gamma finite difference test failed: "
                    << "analytical=" << analyticalGamma << " numerical=" << numericalGamma << " error=" << gammaError);
    }

    Real vanna = calc.vanna(forward, maturity);
    Real volga = calc.volga(maturity);

    // Finite difference for vanna: dVega/dForward
    Real vegaFwdUp = calcUp.vega(maturity);
    Real vegaFwdDown = calcDown.vega(maturity);
    Real vannaFD = (vegaFwdUp - vegaFwdDown) / (2.0 * bump);

    if (std::fabs(vanna - vannaFD) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call vanna error: analytical=" << vanna << " finite-diff=" << vannaFD
                                                                    << " error=" << (vanna - vannaFD));
    }

    // Finite difference for volga: dVega/dVol
    BlackCalculator calcVolUp(Option::Call, strike, forward, stdDev + bump, discount);
    BlackCalculator calcVolDown(Option::Call, strike, forward, stdDev - bump, discount);
    Real vegaVolUp = calcVolUp.vega(maturity);
    Real vegaVolDown = calcVolDown.vega(maturity);
    Real volgaFD = (vegaVolUp - vegaVolDown) / (2.0 * bump);

    if (std::fabs(volga - volgaFD) > tolerance)
    {
        BOOST_ERROR("BlackCalculator call volga error: analytical=" << volga << " finite-diff=" << volgaFD
                                                                    << " error=" << (volga - volgaFD));
    }
}

BOOST_AUTO_TEST_CASE(testBlackCalculatorZeroVolatilityGreeks)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator Greeks with zero volatility...");

    Real tolerance = 1e-10;
    Real forward = 100.0;
    Real discount = 1.0;
    Real spot = 98.0;
    Real maturity = 1.0;
    Real stdDev = 0.0; // Zero volatility

    // Test different moneyness scenarios
    struct ZeroVolTestCase
    {
        Option::Type type;
        Real strike;
        std::string description;
        Real expectedDelta;
        Real expectedGamma;
        Real expectedVega;
        Real expectedTheta; // Approximate expected theta
    };

    ZeroVolTestCase testCases[] = {// ITM options should have delta = 1 for calls, -1 for puts (approximately)
                                   {Option::Call, 90.0, "ITM Call", 1.0, 0.0, 0.0, 0.0},
                                   {Option::Put, 110.0, "ITM Put", -1.0, 0.0, 0.0, 0.0},
                                   // ATM options have undefined behavior at zero vol, but should be finite
                                   {Option::Call, 100.0, "ATM Call", 0.5, 0.0, 0.0, 0.0},
                                   {Option::Put, 100.0, "ATM Put", -0.5, 0.0, 0.0, 0.0},
                                   // OTM options should have delta = 0
                                   {Option::Call, 110.0, "OTM Call", 0.0, 0.0, 0.0, 0.0},
                                   {Option::Put, 90.0, "OTM Put", 0.0, 0.0, 0.0, 0.0}};

    for (const auto& testCase : testCases)
    {
        BlackCalculator calc(testCase.type, testCase.strike, forward, stdDev, discount);

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
            BOOST_ERROR("BlackCalculator " << testCase.description
                                           << " produced non-finite Greeks with zero volatility");
        }

        // Gamma should be zero (no convexity with zero vol)
        if (std::fabs(gammaForward) > tolerance || std::fabs(gamma) > tolerance)
        {
            BOOST_ERROR("BlackCalculator " << testCase.description << " gamma should be zero with zero volatility: "
                                           << "gammaForward=" << gammaForward << " gamma=" << gamma);
        }

        // Vega should be zero (no vol sensitivity)
        if (std::fabs(vega) > tolerance)
        {
            BOOST_ERROR("BlackCalculator " << testCase.description
                                           << " vega should be zero with zero volatility: " << vega);
        }

        // For clearly ITM/OTM cases, check delta bounds
        if (testCase.strike < forward * 0.95)
        { // Clearly ITM call
            if (testCase.type == Option::Call && (deltaForward < 0.99 || deltaForward > 1.01))
            {
                BOOST_ERROR("BlackCalculator ITM call deltaForward should be ~1.0 with zero vol: " << deltaForward);
            }
        }
        if (testCase.strike > forward * 1.05)
        { // Clearly OTM call
            if (testCase.type == Option::Call && std::fabs(deltaForward) > tolerance)
            {
                BOOST_ERROR("BlackCalculator OTM call deltaForward should be ~0.0 with zero vol: " << deltaForward);
            }
        }

        // Strike sensitivities should be finite
        Real strikeSens = calc.strikeSensitivity();
        Real strikeGamma = calc.strikeGamma();

        if (!std::isfinite(strikeSens) || !std::isfinite(strikeGamma))
        {
            BOOST_ERROR("BlackCalculator " << testCase.description
                                           << " strike sensitivities should be finite with zero volatility");
        }

        Real vanna = calc.vanna(spot, maturity);
        Real volga = calc.volga(maturity);

        if (std::fabs(vanna) > tolerance)
        {
            BOOST_ERROR("BlackCalculator " << testCase.description
                                           << " vanna should be zero with zero volatility: " << vanna);
        }
        if (std::fabs(volga) > tolerance)
        {
            BOOST_ERROR("BlackCalculator " << testCase.description
                                           << " volga should be zero with zero volatility: " << volga);
        }
    }

    // Test specific edge case: very small but non-zero volatility to ensure it still works
    Real smallVol = 1e-12;
    BlackCalculator calcSmallVol(Option::Call, 100.0, forward, smallVol, discount);

    Real deltaSmallVol = calcSmallVol.deltaForward();
    Real gammaSmallVol = calcSmallVol.gammaForward();
    Real vegaSmallVol = calcSmallVol.vega(maturity);

    // These should be finite and reasonable
    if (!std::isfinite(deltaSmallVol) || !std::isfinite(gammaSmallVol) || !std::isfinite(vegaSmallVol))
    {
        BOOST_ERROR("BlackCalculator failed for very small volatility");
    }

    // Delta should be close to 0.5 for ATM
    if (std::fabs(deltaSmallVol - discount * 0.5) > 0.1)
    {
        BOOST_ERROR("BlackCalculator ATM delta with small vol unreasonable: " << deltaSmallVol);
    }
}

BOOST_AUTO_TEST_CASE(testBlackCalculatorZeroVolatilityLimit)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator at zero volatility against a tiny one...");

    const Real spot = 100.0, strike = 100.0, discount = 0.9, tiny = 1.0e-9;
    const Time maturity = 1.0;

    const auto check = [&](const ext::shared_ptr<StrikedTypePayoff>& payoff, Real forward, Real tolerance)
    {
        const BlackCalculator zero(payoff, forward, 0.0, discount);
        const BlackCalculator limit(payoff, forward, tiny, discount);
        BOOST_TEST_CONTEXT(payoff->name() << " " << payoff->optionType() << ", forward " << forward)
        {
            BOOST_CHECK_SMALL(zero.value() - limit.value(), tolerance);
            BOOST_CHECK_SMALL(zero.delta(spot) - limit.delta(spot), tolerance);
            BOOST_CHECK_SMALL(zero.deltaForward() - limit.deltaForward(), tolerance);
            BOOST_CHECK_SMALL(zero.strikeSensitivity() - limit.strikeSensitivity(), tolerance);
            BOOST_CHECK_SMALL(zero.rho(maturity) - limit.rho(maturity), tolerance);
            BOOST_CHECK_SMALL(zero.dividendRho(maturity) - limit.dividendRho(maturity), tolerance);
            BOOST_CHECK_SMALL(zero.theta(spot, maturity) - limit.theta(spot, maturity), tolerance);
        }
    };

    for (Option::Type type : {Option::Call, Option::Put})
    {
        const ext::shared_ptr<StrikedTypePayoff> payoffs[] = {ext::make_shared<PlainVanillaPayoff>(type, strike),
                                                              ext::make_shared<CashOrNothingPayoff>(type, strike, 10.0),
                                                              ext::make_shared<AssetOrNothingPayoff>(type, strike),
                                                              ext::make_shared<GapPayoff>(type, strike, 90.0)};
        for (const auto& payoff : payoffs)
        {
            check(payoff, 120.0, 1.0e-10);
            check(payoff, 80.0, 1.0e-10);
        }
        // At the money the value differs by about 0.4 tiny, and rho by rounding over tiny.
        check(payoffs[0], strike, 1.0e-5);
    }
}

BOOST_AUTO_TEST_CASE(testBlackCalculatorVannaAndVolgaOfEveryPayoff)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator vanna and volga of every payoff...");

    // #193: every payoff took the vanilla option's vanna and volga, and volga was the derivative of vega in the
    // standard deviation, off by sqrt(T). The references differentiate the value numerically at 50 digits (mpmath),
    // with the spot at 98, r 3 %, q 1 %, a volatility of 20 %, a strike of 105, a cash payoff of 10 and, for the gap
    // option, a second strike of 95.
    const Real spot = 98.0, r = 0.03, q = 0.01, vol = 0.20, strike = 105.0;

    const auto payoff = [&](int kind, Option::Type type) -> ext::shared_ptr<StrikedTypePayoff>
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

    // The calls' values. A put's are the same for a vanilla or gap option, whose call less put is a forward, and the
    // opposite for a digital one, whose call and put add up to a certain payment.
    struct Reference
    {
        int kind;
        Time maturity;
        Real vanna, volga;
    };
    const Reference references[] = {
        {0, 0.25, 1.1535272664931643054, 33.344444036573564081},
        {0, 1.0, 0.67413685999881116858, 9.5771300398156322677},
        {0, 2.0, 0.47656763695992381716, -2.5703642003370959040},
        {0, 10.0, 0.17169004440697774126, -27.847008010781431753},
        {1, 0.25, -0.94424049664847642690, -81.125966989411471049},
        {1, 1.0, -0.88404569593266916028, -44.018921526154388396},
        {1, 2.0, -0.66410109407338786254, -18.657998474950035381},
        {1, 10.0, -0.25053287408792578972, 32.622599880157448719},
        {2, 0.25, -8.7609979483158381771, -818.47820935224688193},
        {2, 1.0, -8.6083429472942150144, -452.62154598480544589},
        {2, 2.0, -6.4964938508106487395, -198.47934818731246741},
        {2, 10.0, -2.4589051335162430508, 314.69029073087177980},
        {3, 0.25, 0.20928676984468787846, -47.781522952837906968},
        {3, 1.0, -0.20990883593385799170, -34.441791486338756128},
        {3, 2.0, -0.18753345711346404538, -21.228362675287131285},
        {3, 10.0, -0.078842829680948048458, 4.7755918693760169663},
    };

    for (const auto& reference : references)
    {
        for (Option::Type type : {Option::Call, Option::Put})
        {
            const Real sign = type == Option::Put && (reference.kind == 1 || reference.kind == 2) ? -1.0 : 1.0;
            const Time t = reference.maturity;
            const BlackCalculator calc(payoff(reference.kind, type), spot * std::exp((r - q) * t), vol * std::sqrt(t),
                                       std::exp(-r * t));
            BOOST_TEST_CONTEXT(payoff(reference.kind, type)->description() << ", T = " << t)
            {
                BOOST_CHECK_SMALL(calc.vanna(spot, t) / (sign * reference.vanna) - 1.0, 1.0e-12);
                BOOST_CHECK_SMALL(calc.volga(t) / (sign * reference.volga) - 1.0, 1.0e-12);
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(testBlackCalculatorGreeksAtAZeroStrike)
{
    BOOST_TEST_MESSAGE("Testing BlackCalculator Greeks at a zero strike...");

    // #201: at a strike that close() takes for zero, initialize() takes d1 and d2 as infinite and the densities as
    // zero, and eight Greeks multiplied those zeros by something infinite. Such an option can't end out of the money.
    // A vanilla or asset-or-nothing call is worth S e^(-qT), a cash-or-nothing call its cash discounted, a gap call
    // the difference, and a put nothing; theta is minus their derivatives in the expiry.
    const Real spot = 100.0, r = 0.03, q = 0.01, vol = 0.20, cash = 10.0, secondStrike = 95.0;
    const Time maturity = 1.0;
    const Real forward = spot * std::exp((r - q) * maturity), discount = std::exp(-r * maturity);
    const Real asset = spot * std::exp(-q * maturity);

    // Two strikes inside close()'s band, and one outside it, whose densities underflow without its help.
    for (Real strike : {0.0, 1.0e-30, 1.0e-20})
    {
        for (Option::Type type : {Option::Call, Option::Put})
        {
            const Real call = type == Option::Call ? 1.0 : 0.0;
            struct Case
            {
                ext::shared_ptr<StrikedTypePayoff> payoff;
                Real value, theta, strikeSensitivity;
            };
            const Case cases[] = {
                {ext::make_shared<PlainVanillaPayoff>(type, strike), call * asset, call * q * asset, -call * discount},
                {ext::make_shared<CashOrNothingPayoff>(type, strike, cash), call * cash * discount,
                 call * r * cash * discount, 0.0},
                {ext::make_shared<AssetOrNothingPayoff>(type, strike), call * asset, call * q * asset, 0.0},
                {ext::make_shared<GapPayoff>(type, strike, secondStrike), call * (asset - secondStrike * discount),
                 call * (q * asset - r * secondStrike * discount), 0.0}};

            for (const auto& c : cases)
            {
                const BlackCalculator calc(c.payoff, forward, vol * std::sqrt(maturity), discount);
                BOOST_TEST_CONTEXT(c.payoff->description())
                {
                    BOOST_CHECK_SMALL(calc.value() - c.value, 1.0e-12);
                    BOOST_CHECK_SMALL(calc.theta(spot, maturity) - c.theta, 1.0e-12);
                    BOOST_CHECK_SMALL(calc.strikeSensitivity() - c.strikeSensitivity, 1.0e-12);
                    BOOST_CHECK_SMALL(calc.gamma(spot), 1.0e-12);
                    BOOST_CHECK_SMALL(calc.gammaForward(), 1.0e-12);
                    BOOST_CHECK_SMALL(calc.vega(maturity), 1.0e-12);
                    BOOST_CHECK_SMALL(calc.strikeGamma(), 1.0e-12);
                    BOOST_CHECK_SMALL(calc.vanna(spot, maturity), 1.0e-12);
                    BOOST_CHECK_SMALL(calc.volga(maturity), 1.0e-12);
                }
            }
        }
    }

    // BlackScholesCalculator takes them from BlackCalculator.
    const BlackScholesCalculator bs(Option::Call, 0.0, spot, std::exp(-q * maturity), vol * std::sqrt(maturity),
                                    discount);
    BOOST_CHECK_SMALL(bs.gamma(), 1.0e-12);
    BOOST_CHECK_SMALL(bs.theta(maturity) - q * asset, 1.0e-12);
    BOOST_CHECK_SMALL(bs.vega(maturity), 1.0e-12);

    // And so does the engine through which a call struck at zero is usually priced.
    const Date today(9, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dayCounter = Actual365Fixed();
    const auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(spot)), Handle<YieldTermStructure>(flatRate(today, q, dayCounter)),
        Handle<YieldTermStructure>(flatRate(today, r, dayCounter)),
        Handle<BlackVolTermStructure>(flatVol(today, vol, dayCounter)));
    VanillaOption option(ext::make_shared<PlainVanillaPayoff>(Option::Call, 0.0),
                         ext::make_shared<EuropeanExercise>(today + 365));
    option.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(process));
    BOOST_CHECK_SMALL(option.NPV() - asset, 1.0e-12);
    BOOST_CHECK_SMALL(option.gamma(), 1.0e-12);
    BOOST_CHECK_SMALL(option.vega(), 1.0e-12);
    BOOST_CHECK_SMALL(option.theta() - q * asset, 1.0e-12);
    BOOST_CHECK_SMALL(option.strikeSensitivity() + discount, 1.0e-12);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()