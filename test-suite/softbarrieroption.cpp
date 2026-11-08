/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 William Day

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
#include <ql/instruments/barrieroption.hpp>
#include <ql/instruments/softbarrieroption.hpp>
#include <ql/pricingengines/barrier/analyticbarrierengine.hpp>
#include <ql/pricingengines/barrier/analyticsoftbarrierengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/types.hpp>
#include <iostream>


using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)
BOOST_AUTO_TEST_SUITE(SoftBarrierOptionTests)


#undef REPORT_FAILURE
#define REPORT_FAILURE(name, barrierType, upperBarrier, lowerBarrier, payoff, exercise, spot, q, r, today, vol, \
                       expected, calculated, error, tolerance)                                                  \
    BOOST_ERROR("\n"                                                                                            \
                << barrierType << " " << exerciseTypeToString(exercise) << " " << payoff->optionType()          \
                << " option with " << payoffTypeToString(payoff) << " payoff:\n"                                \
                << "    underlying value: " << spot << "\n"                                                     \
                << "    strike:           " << payoff->strike() << "\n"                                         \
                << "    upper barrier:    " << upperBarrier << "\n"                                             \
                << "    lower barrier:    " << lowerBarrier << "\n"                                             \
                << "    dividend yield:   " << q << "\n"                                                        \
                << "    risk-free rate:   " << r << "\n"                                                        \
                << "    reference date:   " << today << "\n"                                                    \
                << "    maturity:         " << exercise->lastDate() << "\n"                                     \
                << "    volatility:       " << vol << "\n\n"                                                    \
                << "    expected   " << name << ": " << expected << "\n"                                        \
                << "    calculated " << name << ": " << calculated << "\n"                                      \
                << "    error:            " << error << "\n"                                                    \
                << "    tolerance:        " << tolerance)


struct SoftBarrierOptionData
{
    Barrier::Type barrierType; // e.g DownOut/UpIn/etc.
    Option::Type type;         // Call/Put
    Real s;                    // Spot price
    Real strike;               // X
    Real U;                    // U
    Real L;                    // L
    Rate q;                    // Dividend Yield
    Rate r;                    // Risk free rate
    Time t;                    // Time to maturity
    Volatility v;              // Volatility
    Real result;               // Expected NPV
    Real tol;                  // Tolerance
};


BOOST_AUTO_TEST_CASE(testSoftBarrierHaug)
{

    BOOST_TEST_MESSAGE("Testing soft barrier option pricing against textbook values...");

    SoftBarrierOptionData values[] = {
        /* The data below is from "The complete guide to option pricing formulas 2nd Ed",E.G. Haug p.166
          Note: In the book, b represents the cost of carry (r-q)
        */

        // barrierType,       optionType,   S,   X,  U,  L,    q,   r,   T,  v, result, tol
        {Barrier::DownOut, Option::Call, 100, 100, 95, 95, 0.05, 0.1, 0.5, 0.1, 3.8075, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 90, 0.05, 0.1, 0.5, 0.1, 4.0175, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 85, 0.05, 0.1, 0.5, 0.1, 4.0529, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 80, 0.05, 0.1, 0.5, 0.1, 4.0648, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 75, 0.05, 0.1, 0.5, 0.1, 4.0708, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 70, 0.05, 0.1, 0.5, 0.1, 4.0744, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 65, 0.05, 0.1, 0.5, 0.1, 4.0768, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 60, 0.05, 0.1, 0.5, 0.1, 4.0785, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 55, 0.05, 0.1, 0.5, 0.1, 4.0798, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 50, 0.05, 0.1, 0.5, 0.1, 4.0808, 1e-4},

        {Barrier::DownOut, Option::Call, 100, 100, 95, 95, 0.05, 0.1, 0.5, 0.2, 4.5263, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 90, 0.05, 0.1, 0.5, 0.2, 5.5615, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 85, 0.05, 0.1, 0.5, 0.2, 6.0394, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 80, 0.05, 0.1, 0.5, 0.2, 6.2594, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 75, 0.05, 0.1, 0.5, 0.2, 6.3740, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 70, 0.05, 0.1, 0.5, 0.2, 6.4429, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 65, 0.05, 0.1, 0.5, 0.2, 6.4889, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 60, 0.05, 0.1, 0.5, 0.2, 6.5217, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 55, 0.05, 0.1, 0.5, 0.2, 6.5463, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 50, 0.05, 0.1, 0.5, 0.2, 6.5654, 1e-4},

        {Barrier::DownOut, Option::Call, 100, 100, 95, 95, 0.05, 0.1, 0.5, 0.3, 4.7297, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 90, 0.05, 0.1, 0.5, 0.3, 6.2595, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 85, 0.05, 0.1, 0.5, 0.3, 7.2496, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 80, 0.05, 0.1, 0.5, 0.3, 7.8567, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 75, 0.05, 0.1, 0.5, 0.3, 8.2253, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 70, 0.05, 0.1, 0.5, 0.3, 8.4578, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 65, 0.05, 0.1, 0.5, 0.3, 8.6142, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 60, 0.05, 0.1, 0.5, 0.3, 8.7260, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 55, 0.05, 0.1, 0.5, 0.3, 8.8099, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 50, 0.05, 0.1, 0.5, 0.3, 8.8751, 1e-4},

        {Barrier::DownOut, Option::Call, 100, 100, 95, 95, 0.05, 0.1, 1.0, 0.1, 5.4187, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 90, 0.05, 0.1, 1.0, 0.1, 6.0758, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 85, 0.05, 0.1, 1.0, 0.1, 6.2641, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 80, 0.05, 0.1, 1.0, 0.1, 6.3336, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 75, 0.05, 0.1, 1.0, 0.1, 6.3685, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 70, 0.05, 0.1, 1.0, 0.1, 6.3894, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 65, 0.05, 0.1, 1.0, 0.1, 6.4034, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 60, 0.05, 0.1, 1.0, 0.1, 6.4133, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 55, 0.05, 0.1, 1.0, 0.1, 6.4208, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 50, 0.05, 0.1, 1.0, 0.1, 6.4266, 1e-4},

        {Barrier::DownOut, Option::Call, 100, 100, 95, 95, 0.05, 0.1, 1.0, 0.2, 5.3614, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 90, 0.05, 0.1, 1.0, 0.2, 6.9776, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 85, 0.05, 0.1, 1.0, 0.2, 7.9662, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 80, 0.05, 0.1, 1.0, 0.2, 8.5432, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 75, 0.05, 0.1, 1.0, 0.2, 8.8822, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 70, 0.05, 0.1, 1.0, 0.2, 9.0931, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 65, 0.05, 0.1, 1.0, 0.2, 9.2343, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 60, 0.05, 0.1, 1.0, 0.2, 9.3353, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 55, 0.05, 0.1, 1.0, 0.2, 9.4110, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 50, 0.05, 0.1, 1.0, 0.2, 9.4698, 1e-4},

        {Barrier::DownOut, Option::Call, 100, 100, 95, 95, 0.05, 0.1, 1.0, 0.3, 5.2300, 1e-4},
        // The book has 7.2046 here.  The average of the single-barrier options, which is what a soft barrier is,
        // gives 7.204364.
        {Barrier::DownOut, Option::Call, 100, 100, 95, 90, 0.05, 0.1, 1.0, 0.3, 7.2044, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 85, 0.05, 0.1, 1.0, 0.3, 8.7092, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 80, 0.05, 0.1, 1.0, 0.3, 9.8118, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 75, 0.05, 0.1, 1.0, 0.3, 10.5964, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 70, 0.05, 0.1, 1.0, 0.3, 11.1476, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 65, 0.05, 0.1, 1.0, 0.3, 11.5384, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 60, 0.05, 0.1, 1.0, 0.3, 11.8228, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 55, 0.05, 0.1, 1.0, 0.3, 12.0369, 1e-4},
        {Barrier::DownOut, Option::Call, 100, 100, 95, 50, 0.05, 0.1, 1.0, 0.3, 12.2036, 1e-4}};


    DayCounter dc = Actual360(); //
    Date today = {8, August, 2025};
    Settings::instance().evaluationDate() = today;


    auto spot = ext::make_shared<SimpleQuote>(0.0);
    auto qRate = ext::make_shared<SimpleQuote>(0.0);
    auto qTS = flatRate(today, qRate, dc);
    auto rRate = ext::make_shared<SimpleQuote>(0.0);
    auto rTS = flatRate(today, rRate, dc);
    auto vol = ext::make_shared<SimpleQuote>(0.0);
    auto volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {
        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        Date exDate = today + timeToDays(value.t);
        auto exercise = ext::make_shared<EuropeanExercise>(exDate);
        auto payoff = ext::make_shared<PlainVanillaPayoff>(value.type, value.strike);

        auto process = ext::make_shared<GeneralizedBlackScholesProcess>(
            Handle<Quote>(spot), Handle<YieldTermStructure>(qTS), Handle<YieldTermStructure>(rTS),
            Handle<BlackVolTermStructure>(volTS));

        SoftBarrierOption option(value.barrierType, value.L, value.U, payoff, exercise);
        option.setPricingEngine(ext::make_shared<AnalyticSoftBarrierEngine>(process));

        // Value check
        Real calculated = option.NPV();
        Real error = std::fabs(calculated - value.result);
        if (error > value.tol)
        {
            REPORT_FAILURE("Soft barrier option value", value.barrierType, value.U, value.L, payoff, exercise, value.s,
                           value.q, value.r, value.t, value.v, value.result, calculated, error, value.tol);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSoftBarrierDirection)
{
    BOOST_TEST_MESSAGE("Testing soft barrier options against averaged single-barrier options...");

    const DayCounter dc = Actual360();
    const Date today(8, August, 2025);
    Settings::instance().evaluationDate() = today;

    const auto process = ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, 0.04, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.08, dc)), Handle<BlackVolTermStructure>(flatVol(today, 0.25, dc)));

    const ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(today + 360);

    // A soft barrier option is the average of the single-barrier options with
    // barriers in [L, U].  This takes that average by Simpson's rule.
    auto averaged = [&](Barrier::Type barrierType, const ext::shared_ptr<StrikedTypePayoff>& payoff, Real L, Real U)
    {
        const Size n = 128;
        const Real h = (U - L) / n;
        Real sum = 0.0;
        for (Size i = 0; i <= n; ++i)
        {
            BarrierOption option(barrierType, L + i * h, 0.0, payoff, exercise);
            option.setPricingEngine(ext::make_shared<AnalyticBarrierEngine>(process));
            const Real weight = (i == 0 || i == n) ? 1.0 : (i % 2 == 1 ? 4.0 : 2.0);
            sum += weight * option.NPV();
        }
        return sum * h / 3.0 / (U - L);
    };

    const struct
    {
        Barrier::Type barrierType;
        Real L, U;
    } cases[] = {{Barrier::DownIn, 80.0, 90.0},
                 {Barrier::DownOut, 80.0, 90.0},
                 {Barrier::UpIn, 110.0, 120.0},
                 {Barrier::UpOut, 110.0, 120.0}};

    for (const auto& c : cases)
    {
        const bool isDown = c.barrierType == Barrier::DownIn || c.barrierType == Barrier::DownOut;
        for (auto type : {Option::Call, Option::Put})
        {
            BOOST_TEST_CONTEXT(c.barrierType << " " << type)
            {
                const ext::shared_ptr<StrikedTypePayoff> payoff = ext::make_shared<PlainVanillaPayoff>(type, 100.0);
                SoftBarrierOption option(c.barrierType, c.L, c.U, payoff, exercise);
                option.setPricingEngine(ext::make_shared<AnalyticSoftBarrierEngine>(process));

                if ((type == Option::Call) == isDown)
                {
                    BOOST_CHECK_SMALL(option.NPV() - averaged(c.barrierType, payoff, c.L, c.U), 1.0e-6);
                }
                else
                {
                    BOOST_CHECK_EXCEPTION(option.NPV(), Error,
                                          ExpectedErrorMessage("soft-barrier formula not available"));
                }
            }
        }
    }
}

namespace
{
    // A down barrier for a call and an up barrier for a put, one year out on Act/365 curves.
    struct SoftBarrierCase
    {
        Option::Type type;
        Real s, strike, L, U;
        Rate r, q;
        Volatility v;
    };

    Real softBarrierValue(const SoftBarrierCase& c, bool knockIn)
    {
        const DayCounter dc = Actual365Fixed();
        const Date today = Settings::instance().evaluationDate();
        const auto process = ext::make_shared<GeneralizedBlackScholesProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(c.s)), Handle<YieldTermStructure>(flatRate(today, c.q, dc)),
            Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
            Handle<BlackVolTermStructure>(flatVol(today, c.v, dc)));
        const bool isCall = c.type == Option::Call;
        const Barrier::Type barrierType =
            knockIn ? (isCall ? Barrier::DownIn : Barrier::UpIn) : (isCall ? Barrier::DownOut : Barrier::UpOut);
        SoftBarrierOption option(barrierType, c.L, c.U, ext::make_shared<PlainVanillaPayoff>(c.type, c.strike),
                                 ext::make_shared<EuropeanExercise>(today + 365));
        option.setPricingEngine(ext::make_shared<AnalyticSoftBarrierEngine>(process));
        return option.NPV();
    }
}

BOOST_AUTO_TEST_CASE(testSoftBarrierAgainstDefinition)
{
    BOOST_TEST_MESSAGE("Testing soft barrier options with the spot or the strike inside the barrier range...");

    Settings::instance().evaluationDate() = Date(7, November, 2026);

    // Knock-in values from the definition, the average over [L, U] of single-barrier knock-ins, integrated at
    // 80 digits.  Past the spot the average takes the vanilla, short of the strike Reiner and Rubinstein's C,
    // and between the two A - B + D.
    const struct
    {
        SoftBarrierCase c;
        Real knockIn;
    } cases[] = {// the whole range short of the spot and the strike, where Haug's formula holds
                 {{Option::Call, 100, 100, 80, 90, 0.05, 0.0, 0.2}, 0.6463869226285},
                 {{Option::Put, 100, 100, 105, 110, 0.05, 0.0, 0.2}, 2.081218270259},
                 // the strike inside the range, or past it
                 {{Option::Call, 100, 85, 80, 90, 0.05, 0.0, 0.2}, 2.455280167321},
                 {{Option::Call, 100, 75, 80, 90, 0.05, 0.0, 0.2}, 5.003753910257},
                 {{Option::Put, 100, 107, 105, 110, 0.05, 0.0, 0.2}, 3.889980769772},
                 {{Option::Put, 100, 115, 105, 110, 0.05, 0.0, 0.2}, 6.933416479895},
                 // the spot inside the range
                 {{Option::Call, 85, 100, 80, 90, 0.05, 0.0, 0.2}, 2.549204233456},
                 {{Option::Call, 88, 80, 80, 90, 0.05, 0.0, 0.2}, 9.431250527125},
                 {{Option::Put, 107, 100, 105, 110, 0.05, 0.0, 0.2}, 3.096612712331},
                 // b = 0 and b = -vol^2, where Haug's formula divides by zero
                 {{Option::Call, 100, 100, 80, 90, 0.03, 0.03, 0.2}, 0.5380047245289},
                 {{Option::Put, 100, 100, 105, 115, 0.03, 0.03, 0.2}, 1.942648643101},
                 {{Option::Call, 100, 100, 80, 90, 0.01, 0.05, 0.2}, 0.4507517399407},
                 {{Option::Put, 100, 100, 105, 110, 0.01, 0.05, 0.2}, 3.272342469076},
                 // a strike short of the range, where the knock-in needs A - B + D at every volatility
                 {{Option::Call, 100, 70, 80, 90, 0.01, 0.15, 0.2}, 7.085646728501},
                 {{Option::Call, 100, 70, 80, 90, 0.01, 0.15, 0.01}, 5.115348376711},
                 {{Option::Put, 100, 130, 110, 120, 0.08, -0.06, 0.01}, 6.883478283537}};

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.c.type << " with spot " << c.c.s << ", strike " << c.c.strike << ", barriers " << c.c.L
                                    << "-" << c.c.U << ", r " << c.c.r << ", q " << c.c.q << ", vol " << c.c.v)
        {
            BOOST_CHECK_SMALL(softBarrierValue(c.c, true) - c.knockIn, 1.0e-9);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSoftBarrierLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing soft barrier options at low and zero volatility...");

    Settings::instance().evaluationDate() = Date(7, November, 2026);

    // From the definition, as above; at zero volatility, the vanilla times the part of [L, U] that the forward
    // crosses.
    const struct
    {
        SoftBarrierCase c;
        bool knockIn;
        Real value;
    } cases[] = {{{Option::Call, 100, 100, 80, 90, 0.05, 0.0, 0.0}, false, 4.877057549929},
                 {{Option::Call, 100, 100, 80, 90, 0.05, 0.0, 0.005}, false, 4.877057549929},
                 {{Option::Call, 100, 100, 80, 90, 0.05, 0.0, 0.01}, false, 4.87705760207},
                 {{Option::Call, 100, 100, 80, 90, 0.05, 0.0, 0.02}, false, 4.880966697013},
                 {{Option::Call, 100, 70, 80, 90, 0.01, 0.15, 0.0}, true, 5.137799439551},
                 {{Option::Call, 100, 70, 80, 90, 0.01, 0.15, 2.0e-8}, true, 5.137799439551},
                 {{Option::Call, 100, 70, 80, 90, 0.01, 0.15, 0.001}, true, 5.137571797314},
                 {{Option::Put, 100, 130, 110, 120, 0.08, -0.06, 0.0}, true, 6.948578215804},
                 {{Option::Put, 100, 130, 110, 120, 0.08, -0.06, 0.001}, true, 6.947924641918}};

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.c.type << (c.knockIn ? " knock-in" : " knock-out") << " with strike " << c.c.strike
                                    << ", barriers " << c.c.L << "-" << c.c.U << ", vol " << c.c.v)
        {
            BOOST_CHECK_SMALL(softBarrierValue(c.c, c.knockIn) - c.value, 1.0e-9);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSoftBarrierNarrowRange)
{
    BOOST_TEST_MESSAGE("Testing soft barrier options with narrow barrier ranges...");

    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();
    const auto process = ext::make_shared<GeneralizedBlackScholesProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, 0.02, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.05, dc)), Handle<BlackVolTermStructure>(flatVol(today, 0.2, dc)));
    const ext::shared_ptr<Exercise> exercise = ext::make_shared<EuropeanExercise>(today + 365);

    // Knock-in values from the definition as U - L shrinks towards the single barrier at L.
    const struct
    {
        Option::Type type;
        Real L, width, knockIn;
    } cases[] = {{Option::Call, 90.0, 1.0e-3, 1.64023253511787},    {Option::Call, 90.0, 1.0001e-4, 1.64006963897484},
                 {Option::Call, 90.0, 0.9999e-4, 1.64006963535507}, {Option::Call, 90.0, 1.0e-6, 1.64005171940409},
                 {Option::Call, 90.0, 1.0e-9, 1.64005153859862},    {Option::Call, 90.0, 0.0, 1.64005153841763},
                 {Option::Put, 110.0, 1.0e-3, 1.51440537756762},    {Option::Put, 110.0, 1.0001e-4, 1.51451877411819},
                 {Option::Put, 110.0, 0.9999e-4, 1.51451877663825}, {Option::Put, 110.0, 1.0e-6, 1.51453124971494},
                 {Option::Put, 110.0, 1.0e-9, 1.51453137559292},    {Option::Put, 110.0, 0.0, 1.51453137571893}};

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.type << " with barriers " << c.L << "-" << c.L + c.width)
        {
            const auto payoff = ext::make_shared<PlainVanillaPayoff>(c.type, 100.0);
            SoftBarrierOption option(c.type == Option::Call ? Barrier::DownIn : Barrier::UpIn, c.L, c.L + c.width,
                                     payoff, exercise);
            option.setPricingEngine(ext::make_shared<AnalyticSoftBarrierEngine>(process));
            BOOST_CHECK_SMALL(option.NPV() - c.knockIn, 1.0e-11);
        }
    }

    // With U = L the option is a single-barrier option, also in the directions the soft formula doesn't cover.
    for (auto barrierType : {Barrier::DownIn, Barrier::DownOut, Barrier::UpIn, Barrier::UpOut})
    {
        for (auto type : {Option::Call, Option::Put})
        {
            BOOST_TEST_CONTEXT(barrierType << " " << type)
            {
                const Real barrier = barrierType == Barrier::DownIn || barrierType == Barrier::DownOut ? 90.0 : 110.0;
                const auto payoff = ext::make_shared<PlainVanillaPayoff>(type, 100.0);
                SoftBarrierOption soft(barrierType, barrier, barrier, payoff, exercise);
                soft.setPricingEngine(ext::make_shared<AnalyticSoftBarrierEngine>(process));
                BarrierOption single(barrierType, barrier, 0.0, payoff, exercise);
                single.setPricingEngine(ext::make_shared<AnalyticBarrierEngine>(process));
                BOOST_CHECK_SMALL(soft.NPV() - single.NPV(), 1.0e-12);
            }
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
