/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
Copyright (C) 2015 Thema Consulting SA

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
#include <ql/experimental/barrieroption/binomialdoublebarrierengine.hpp>
#include <ql/instruments/doublebarrieroption.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.hpp>
#include <ql/pricingengines/barrier/fdhestondoublebarrierengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/shared_ptr.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DoubleBinaryOptionTests)

#undef REPORT_FAILURE
#define REPORT_FAILURE(greekName, payoff, exercise, barrierType, barrier_lo, barrier_hi, s, q, r, today, v, expected, \
                       calculated, error, tolerance)                                                                  \
    BOOST_ERROR(payoff->optionType() << " option with " << barrierType << " barrier type:\n"                          \
                                     << "    barrier_lo:       " << barrier_lo << "\n"                                \
                                     << "    barrier_hi:       " << barrier_hi << "\n"                                \
                                     << payoffTypeToString(payoff) << " payoff:\n"                                    \
                                     << "    spot value:       " << s << "\n"                                         \
                                     << "    strike:           " << payoff->strike() << "\n"                          \
                                     << "    dividend yield:   " << io::rate(q) << "\n"                               \
                                     << "    risk-free rate:   " << io::rate(r) << "\n"                               \
                                     << "    reference date:   " << today << "\n"                                     \
                                     << "    maturity:         " << exercise->lastDate() << "\n"                      \
                                     << "    volatility:       " << io::volatility(v) << "\n\n"                       \
                                     << "    expected   " << greekName << ": " << expected << "\n"                    \
                                     << "    calculated " << greekName << ": " << calculated << "\n"                  \
                                     << "    error:            " << error << "\n"                                     \
                                     << "    tolerance:        " << tolerance << "\n");

struct DoubleBinaryOptionData
{
    DoubleBarrier::Type barrierType;
    Real barrier_lo;
    Real barrier_hi;
    Real cash;    // cash payoff for cash-or-nothing
    Real s;       // spot
    Rate q;       // dividend
    Rate r;       // risk-free rate
    Time t;       // time to maturity
    Volatility v; // volatility
    Real result;  // expected result
    Real tol;     // tolerance
};


BOOST_AUTO_TEST_CASE(testHaugValues)
{

    BOOST_TEST_MESSAGE("Testing cash-or-nothing double barrier options against Haug's values...");

    DoubleBinaryOptionData values[] = {
        /* The data below are from
          "Option pricing formulas 2nd Ed.", E.G. Haug, McGraw-Hill 2007 pag. 181
          Note: book uses cost of carry b, instead of dividend rate q
        */
        //    barrierType,          bar_lo, bar_hi,  cash,   spot,    q,    r,    t,  vol,   value, tol
        {DoubleBarrier::KnockOut, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 9.8716, 1e-4},
        {DoubleBarrier::KnockOut, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 8.9307, 1e-4},
        {DoubleBarrier::KnockOut, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 6.3272, 1e-4},
        {DoubleBarrier::KnockOut, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 1.9094, 1e-4},

        {DoubleBarrier::KnockOut, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 9.7961, 1e-4},
        {DoubleBarrier::KnockOut, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 7.2300, 1e-4},
        {DoubleBarrier::KnockOut, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 3.7100, 1e-4},
        {DoubleBarrier::KnockOut, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 0.4271, 1e-4},

        {DoubleBarrier::KnockOut, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 8.9054, 1e-4},
        {DoubleBarrier::KnockOut, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 3.6752, 1e-4},
        {DoubleBarrier::KnockOut, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 0.7960, 1e-4},
        {DoubleBarrier::KnockOut, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 0.0059, 1e-4},

        {DoubleBarrier::KnockOut, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 3.6323, 1e-4},
        {DoubleBarrier::KnockOut, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 0.0911, 1e-4},
        {DoubleBarrier::KnockOut, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 0.0002, 1e-4},
        {DoubleBarrier::KnockOut, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 0.0000, 1e-4},

        {DoubleBarrier::KIKO, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.0000, 1e-4},
        {DoubleBarrier::KIKO, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 0.2402, 1e-4},
        {DoubleBarrier::KIKO, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 1.4076, 1e-4},
        {DoubleBarrier::KIKO, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 3.8160, 1e-4},

        {DoubleBarrier::KIKO, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.0075, 1e-4},
        {DoubleBarrier::KIKO, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 0.9910, 1e-4},
        {DoubleBarrier::KIKO, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 2.8098, 1e-4},
        {DoubleBarrier::KIKO, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 4.6612, 1e-4},

        {DoubleBarrier::KIKO, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.2656, 1e-4},
        {DoubleBarrier::KIKO, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 2.7954, 1e-4},
        {DoubleBarrier::KIKO, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 4.4024, 1e-4},
        {DoubleBarrier::KIKO, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 4.9266, 1e-4},

        {DoubleBarrier::KIKO, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 2.6285, 1e-4},
        {DoubleBarrier::KIKO, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 4.7523, 1e-4},
        {DoubleBarrier::KIKO, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 4.9096, 1e-4},
        {DoubleBarrier::KIKO, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 4.9675, 1e-4},

        // following values calculated with haug's VBA code
        {DoubleBarrier::KnockIn, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.0042, 1e-4},
        {DoubleBarrier::KnockIn, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 0.9450, 1e-4},
        {DoubleBarrier::KnockIn, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 3.5486, 1e-4},
        {DoubleBarrier::KnockIn, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 7.9663, 1e-4},

        {DoubleBarrier::KnockIn, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.0797, 1e-4},
        {DoubleBarrier::KnockIn, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 2.6458, 1e-4},
        {DoubleBarrier::KnockIn, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 6.1658, 1e-4},
        {DoubleBarrier::KnockIn, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 9.4486, 1e-4},

        {DoubleBarrier::KnockIn, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.9704, 1e-4},
        {DoubleBarrier::KnockIn, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 6.2006, 1e-4},
        {DoubleBarrier::KnockIn, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 9.0798, 1e-4},
        {DoubleBarrier::KnockIn, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 9.8699, 1e-4},

        {DoubleBarrier::KnockIn, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 6.2434, 1e-4},
        {DoubleBarrier::KnockIn, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 9.7847, 1e-4},
        {DoubleBarrier::KnockIn, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 9.8756, 1e-4},
        {DoubleBarrier::KnockIn, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 9.8758, 1e-4},

        {DoubleBarrier::KOKI, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.0041, 1e-4},
        {DoubleBarrier::KOKI, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 0.7080, 1e-4},
        {DoubleBarrier::KOKI, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 2.1581, 1e-4},
        {DoubleBarrier::KOKI, 80.00, 120.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 4.2061, 1e-4},

        {DoubleBarrier::KOKI, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.0723, 1e-4},
        {DoubleBarrier::KOKI, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 1.6663, 1e-4},
        {DoubleBarrier::KOKI, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 3.3930, 1e-4},
        {DoubleBarrier::KOKI, 85.00, 115.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 4.8679, 1e-4},

        {DoubleBarrier::KOKI, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 0.7080, 1e-4},
        {DoubleBarrier::KOKI, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 3.4424, 1e-4},
        {DoubleBarrier::KOKI, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 4.7496, 1e-4},
        {DoubleBarrier::KOKI, 90.00, 110.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 5.0475, 1e-4},

        {DoubleBarrier::KOKI, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.10, 3.6524, 1e-4},
        {DoubleBarrier::KOKI, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.20, 5.1256, 1e-4},
        {DoubleBarrier::KOKI, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.30, 5.0763, 1e-4},
        {DoubleBarrier::KOKI, 95.00, 105.00, 10.00, 100.00, 0.02, 0.05, 0.25, 0.50, 5.0275, 1e-4},

        // degenerate cases
        {DoubleBarrier::KnockOut, 95.00, 105.00, 10.00, 80.00, 0.02, 0.05, 0.25, 0.10, 0.0000, 1e-4},
        {DoubleBarrier::KnockOut, 95.00, 105.00, 10.00, 110.00, 0.02, 0.05, 0.25, 0.10, 0.0000, 1e-4},
        {DoubleBarrier::KnockIn, 95.00, 105.00, 10.00, 80.00, 0.02, 0.05, 0.25, 0.10, 9.8758, 1e-4},
        {DoubleBarrier::KnockIn, 95.00, 105.00, 10.00, 110.00, 0.02, 0.05, 0.25, 0.10, 9.8758, 1e-4},
        {DoubleBarrier::KIKO, 95.00, 105.00, 10.00, 80.00, 0.02, 0.05, 0.25, 0.10, 10.0000, 1e-4},
        {DoubleBarrier::KIKO, 95.00, 105.00, 10.00, 110.00, 0.02, 0.05, 0.25, 0.10, 0.0000, 1e-4},
        {DoubleBarrier::KOKI, 95.00, 105.00, 10.00, 80.00, 0.02, 0.05, 0.25, 0.10, 0.0000, 1e-4},
        {DoubleBarrier::KOKI, 95.00, 105.00, 10.00, 110.00, 0.02, 0.05, 0.25, 0.10, 10.0000, 1e-4},
    };

    DayCounter dc = Actual360();
    Date today = Date::todaysDate();

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(100.0));
    ext::shared_ptr<SimpleQuote> qRate(new SimpleQuote(0.04));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(today, qRate, dc);
    ext::shared_ptr<SimpleQuote> rRate(new SimpleQuote(0.01));
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(today, rRate, dc);
    ext::shared_ptr<SimpleQuote> vol(new SimpleQuote(0.25));
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(today, vol, dc);

    for (auto& value : values)
    {

        ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(Option::Call, 0, value.cash));

        Date exDate = today + timeToDays(value.t);
        ext::shared_ptr<Exercise> exercise;
        if (value.barrierType == DoubleBarrier::KIKO || value.barrierType == DoubleBarrier::KOKI)
            exercise = ext::make_shared<AmericanExercise>(today, exDate);
        else
            exercise = ext::make_shared<EuropeanExercise>(exDate);

        spot->setValue(value.s);
        qRate->setValue(value.q);
        rRate->setValue(value.r);
        vol->setValue(value.v);

        ext::shared_ptr<BlackScholesMertonProcess> stochProcess(
            new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                          Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));

        // checking with analytic engine
        ext::shared_ptr<PricingEngine> engine(new AnalyticDoubleBarrierBinaryEngine(stochProcess));
        DoubleBarrierOption opt(value.barrierType, value.barrier_lo, value.barrier_hi, 0, payoff, exercise);
        opt.setPricingEngine(engine);

        Real calculated = opt.NPV();
        Real expected = value.result;
        Real error = std::fabs(calculated - expected);
        if (error > value.tol)
        {
            REPORT_FAILURE("value", payoff, exercise, value.barrierType, value.barrier_lo, value.barrier_hi, value.s,
                           value.q, value.r, today, value.v, value.result, calculated, error, value.tol);
        }

        Size steps = 500;
        // checking with binomial engine
        engine = ext::shared_ptr<PricingEngine>(
            new BinomialDoubleBarrierEngine<CoxRossRubinstein, DiscretizedDoubleBarrierOption>(stochProcess, steps));
        opt.setPricingEngine(engine);
        calculated = opt.NPV();
        expected = value.result;
        error = std::fabs(calculated - expected);
        double tol = 0.22;
        if (error > tol)
        {
            REPORT_FAILURE("Binomial value", payoff, exercise, value.barrierType, value.barrier_lo, value.barrier_hi,
                           value.s, value.q, value.r, today, value.v, value.result, calculated, error, tol);
        }
    }
}

BOOST_AUTO_TEST_CASE(testPdeDoubleBarrierWithAnalytical)
{
    BOOST_TEST_MESSAGE("Testing cash-or-nothing double barrier options "
                       "against PDE Heston version...");

    const DayCounter dc = Actual360();
    const Date todaysDate(30, Jan, 2023);
    const Date maturityDate = todaysDate + Period(1, Years);
    Settings::instance().evaluationDate() = todaysDate;

    const Handle<Quote> spot(ext::make_shared<SimpleQuote>(100));
    const Rate r = 0.075;
    const Rate q = 0.03;
    const Volatility vol = 0.4;

    const Real kappa = 1.0;
    const Real theta = vol * vol;
    const Real rho = 0.0;
    const Real sigma = 1e-4;
    const Real v0 = theta;

    const Handle<YieldTermStructure> rTS(flatRate(r, dc));
    const Handle<YieldTermStructure> qTS(flatRate(q, dc));

    const ext::shared_ptr<HestonModel> hestonModel =
        ext::make_shared<HestonModel>(ext::make_shared<HestonProcess>(rTS, qTS, spot, v0, kappa, theta, sigma, rho));

    const ext::shared_ptr<BlackScholesMertonProcess> bsProcess = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(spot), Handle<YieldTermStructure>(qTS), Handle<YieldTermStructure>(rTS),
        Handle<BlackVolTermStructure>(flatVol(todaysDate, vol, dc)));

    const ext::shared_ptr<PricingEngine> analyticEngine =
        ext::make_shared<AnalyticDoubleBarrierBinaryEngine>(bsProcess);

    const ext::shared_ptr<PricingEngine> fdEngine =
        ext::make_shared<FdHestonDoubleBarrierEngine>(hestonModel, 201, 101, 3, 0, FdmSchemeDesc::Hundsdorfer());

    const ext::shared_ptr<Exercise> europeanExercise(ext::make_shared<EuropeanExercise>(maturityDate));

    const Real tol = 5e-3;
    for (Size i = 5; i < 18; i += 2)
    {
        const Real dist = 10.0 + 5.0 * i;

        const Real barrier_lo = std::max(spot->value() - dist, 1e-2);
        const Real barrier_hi = spot->value() + dist;
        DoubleBarrierOption doubleBarrier(DoubleBarrier::KnockOut, barrier_lo, barrier_hi, 0.0,
                                          ext::make_shared<CashOrNothingPayoff>(Option::Call, 0.0, 1.0),
                                          europeanExercise);

        doubleBarrier.setPricingEngine(analyticEngine);
        const Real bsNPV = doubleBarrier.NPV();

        doubleBarrier.setPricingEngine(fdEngine);
        const Real slvNPV = doubleBarrier.NPV();

        const Real diff = slvNPV - bsNPV;
        if (std::fabs(diff) > tol)
        {
            BOOST_ERROR("Failed to reproduce price difference for a Double-No-Touch "
                        << "option between Black-Scholes and "
                        << "Heston Stochastic Local Volatility model"
                        << "\n Barrier Low        : " << barrier_lo << "\n Barrier High       : " << barrier_hi
                        << "\n Black-Scholes Price: " << bsNPV << "\n Heston SLV Price   : " << slvNPV
                        << "\n diff               : " << diff << "\n tolerance          : " << tol);
        }
    }
}

// Options paying a cash of 10, against references to 20 digits: sums over images of the no-touch probability and of
// the exit time's density, the latter with each image's value up to expiry in closed form, at complex arguments where
// a negative rate makes its exponent imaginary; Hui's series, with the KIKO's part that doesn't depend on expiry
// summed in closed form; and, for KIKO and KOKI options, quadrature of the discounted density of the exit time.  The
// tolerance is relative to the cash for knock-outs and knock-ins, whose error is absolute, and to the value for KIKO
// and KOKI options.
struct DoubleBinaryReferenceData
{
    DoubleBarrier::Type barrierType;
    Real s, barrier_lo, barrier_hi;
    Rate r, q;
    Volatility v;
    Integer days;
    Real value, tolerance;
};

void checkDoubleBinaryReferences(const std::vector<DoubleBinaryReferenceData>& cases)
{
    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();
    const Real cash = 10.0;

    for (const auto& c : cases)
    {
        const bool atHit = c.barrierType == DoubleBarrier::KIKO || c.barrierType == DoubleBarrier::KOKI;
        ext::shared_ptr<Exercise> exercise;
        if (atHit)
            exercise = ext::make_shared<AmericanExercise>(today, today + c.days);
        else
            exercise = ext::make_shared<EuropeanExercise>(today + c.days);
        const auto process = ext::make_shared<BlackScholesMertonProcess>(
            Handle<Quote>(ext::make_shared<SimpleQuote>(c.s)), Handle<YieldTermStructure>(flatRate(today, c.q, dc)),
            Handle<YieldTermStructure>(flatRate(today, c.r, dc)),
            Handle<BlackVolTermStructure>(flatVol(today, c.v, dc)));
        DoubleBarrierOption option(c.barrierType, c.barrier_lo, c.barrier_hi, 0.0,
                                   ext::make_shared<CashOrNothingPayoff>(Option::Call, 0.0, cash), exercise);
        option.setPricingEngine(ext::make_shared<AnalyticDoubleBarrierBinaryEngine>(process));

        BOOST_TEST_CONTEXT(c.barrierType << " between " << c.barrier_lo << " and " << c.barrier_hi << ", spot " << c.s
                                         << ", r " << c.r << ", q " << c.q << ", volatility " << c.v << ", " << c.days
                                         << " days")
        {
            BOOST_CHECK_SMALL(option.NPV() - c.value, c.tolerance * (atHit ? std::fabs(c.value) : cash));
        }
    }
}

BOOST_AUTO_TEST_CASE(testLowVolatility)
{
    BOOST_TEST_MESSAGE("Testing cash-or-nothing double barrier options at low volatility...");

    // Hui's series threw below about 1% volatility for double knock-outs and knock-ins, and below about 2% for KIKO
    // and KOKI options, and at 2% the knock-out was off by 4.4e-8 (#136).  Far in the tail the value is sensitive to
    // its inputs: the zero rate a flat curve gives back is off by about 1e-15 of itself, which moves the KOKI at 0.5%
    // volatility by about 4e-13 of its value.
    const std::vector<DoubleBinaryReferenceData> cases = {
        //    barrierType,          spot, lo,  hi,    r,     q,  vol, days,                   value, tolerance
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 0.0, 365, 9.5122942450071401, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 1e-9, 365, 9.5122942450071401, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 1e-5, 365, 9.5122942450071401, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 0.001, 365, 9.5122942450071401, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 0.01, 365, 9.5122942450071401, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 0.02, 365, 9.5122942447482304, 1e-14},
        {DoubleBarrier::KnockIn, 100.0, 80.0, 120.0, 0.05, 0.0, 0.0, 365, 0.0, 1e-14},
        {DoubleBarrier::KnockIn, 100.0, 80.0, 120.0, 0.05, 0.0, 0.01, 365, 0.0, 1e-14},
        {DoubleBarrier::KnockIn, 100.0, 80.0, 120.0, 0.05, 0.0, 0.02, 365, 2.5890968930245181e-10, 1e-14},
        // the forward leaves the corridor
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.3, 0.01, 365, 0.031770632074513120, 1e-14},
        {DoubleBarrier::KnockIn, 100.0, 80.0, 120.0, 0.05, 0.3, 0.01, 365, 9.4805236129326270, 1e-14},
        // the forward stays inside
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.0, 365, 0.0, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.001, 365, 0.0, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.01, 365, 4.0211538240753469e-163, 3e-12},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.02, 365, 2.5598106963112008e-41, 3e-12},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.05, 365, 6.4392207767861097e-7, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.0, 365, 0.0, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.005, 365, 1.7455677222221416e-153, 3e-12},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.01, 365, 4.0043076617499302e-39, 3e-12},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.02, 365, 2.5922457350895970e-10, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.05, 365, 0.058464987286955278, 1e-13},
        // the forward reaches the knock-in barrier before expiry
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.3, 0.0, 365, 9.5635249979003699, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.3, 1e-7, 365, 9.5635249979003801, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.3, 1e-5, 365, 9.5635249980028037, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.3, 1e-4, 365, 9.5635250081437564, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.3, 0.001, 365, 9.5635260222362435, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, -0.3, 0.0, 365, 9.7429033291279402, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, -0.3, 1e-7, 365, 9.7429033291279371, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, -0.3, 1e-5, 365, 9.7429033290968672, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, -0.3, 1e-4, 365, 9.7429033260206376, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, -0.3, 0.001, 365, 9.7429030183973711, 1e-13}};

    checkDoubleBinaryReferences(cases);
}

BOOST_AUTO_TEST_CASE(testHitOptionsAgainstReferences)
{
    BOOST_TEST_MESSAGE("Testing KIKO and KOKI cash-or-nothing double barrier options against reference values...");

    // Hui's KIKO and KOKI series converge only like 1 / i^3, and the engine stopped at 1,000 terms: at 5% volatility
    // the KOKI was off by 2.4e-7, and with negative rates the values were off by 1e-11 to 3e-10 (#187).  In the rows
    // with r = q < 0, and in the KIKO and KOKI at 50% volatility, rho^2 = mu^2 + 2 r vol^2 is negative, so the time
    // to the hit has an imaginary transform.  The former sum images, the latter Hui's series, whose closed form is
    // then a ratio of sines.
    const std::vector<DoubleBinaryReferenceData> cases = {
        //    barrierType,      spot, lo,   hi,     r,      q,   vol, days,                  value, tolerance
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.1, 365, 0.083580724255796744, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.2, 365, 2.1359358579319222, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.5, 365, 4.7534077139350090, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.1, 365, 1.3915142538024981, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.2, 365, 4.0084439625835162, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.5, 365, 5.1590334697004016, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, -0.005, -0.005, 0.05, 365, 9.0804037067037583e-5, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, -0.005, -0.005, 0.1, 365, 0.28763843767613530, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, -0.005, -0.005, 0.1, 365, 0.62490670083292046, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 90.0, 110.0, -0.01, -0.01, 0.03, 1825, 1.2669954821627723, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 90.0, 110.0, -0.01, -0.01, 0.03, 1825, 1.5291330811159960, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, -0.01, -0.135, 0.5, 365, 4.5009755155416425, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, -0.01, -0.135, 0.5, 365, 5.5083161257577621, 1e-13},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, -0.005, -0.005, 0.1, 365, 9.1363811172903737, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 90.0, 110.0, -0.01, -0.01, 0.03, 1825, 7.6680908342903726, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, -0.01, -0.135, 0.5, 365, 0.0069948564530145912, 1e-14}};

    checkDoubleBinaryReferences(cases);
}

BOOST_AUTO_TEST_CASE(testAcrossSeriesSwitches)
{
    BOOST_TEST_MESSAGE("Testing cash-or-nothing double barrier options on either side of the engine's switches...");

    // Images give the no-touch probability up to vol sqrt(T) = ln(U / L) / 2, here at a volatility of 0.20273, and the
    // value paid at a hit up to vol sqrt(T) = ln(U / L), at 0.40547; Hui's series give them above.
    const std::vector<DoubleBinaryReferenceData> cases = {
        //    barrierType,          spot, lo,  hi,    r,    q,    vol, days,                value, tolerance
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 0.2027, 365, 3.4063642950964111, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.0, 0.2028, 365, 3.4023563500806989, 1e-14},
        {DoubleBarrier::KnockIn, 100.0, 80.0, 120.0, 0.05, 0.0, 0.2027, 365, 6.1059299499107290, 1e-14},
        {DoubleBarrier::KnockIn, 100.0, 80.0, 120.0, 0.05, 0.0, 0.2028, 365, 6.1099378949264412, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.3, 0.2027, 365, 1.9196026167920524, 1e-14},
        {DoubleBarrier::KnockOut, 100.0, 80.0, 120.0, 0.05, 0.3, 0.2028, 365, 1.9179395211125049, 1e-14},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.405, 365, 4.5861909058194026, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.0, 0.406, 365, 4.5890472593615990, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.405, 365, 5.2046004307494607, 1e-13},
        {DoubleBarrier::KOKI, 100.0, 80.0, 120.0, 0.05, 0.0, 0.406, 365, 5.2044132334877143, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.3, 0.405, 365, 6.3473407461132950, 1e-13},
        {DoubleBarrier::KIKO, 100.0, 80.0, 120.0, 0.05, 0.3, 0.406, 365, 6.3421364296929152, 1e-13}};

    checkDoubleBinaryReferences(cases);
}

BOOST_AUTO_TEST_CASE(testKnockedInDoubleKnockIn)
{
    BOOST_TEST_MESSAGE("Testing knocked-in double knock-in cash-or-nothing options...");

    // A double knock-in pays at expiry, but with the spot at or past a barrier the engine valued it at the cash itself,
    // so its value jumped by cash (1 - discount) at each barrier, here by 0.124 (#189).
    const Date today(7, November, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual360();
    const Real cash = 10.0;
    const Rate r = 0.05;
    const Date expiry = today + 90;

    const auto spot = ext::make_shared<SimpleQuote>(100.0);
    const auto rate = ext::make_shared<SimpleQuote>(r);
    const auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(spot), Handle<YieldTermStructure>(flatRate(today, 0.02, dc)),
        Handle<YieldTermStructure>(flatRate(today, rate, dc)), Handle<BlackVolTermStructure>(flatVol(today, 0.10, dc)));
    DoubleBarrierOption option(DoubleBarrier::KnockIn, 95.0, 105.0, 0.0,
                               ext::make_shared<CashOrNothingPayoff>(Option::Call, 0.0, cash),
                               ext::make_shared<EuropeanExercise>(expiry));
    option.setPricingEngine(ext::make_shared<AnalyticDoubleBarrierBinaryEngine>(process));

    const Real discounted = cash * std::exp(-r * 90.0 / 360.0);
    const Real h = 1e-5;
    for (Real s : {80.0, 95.0, 105.0, 110.0})
    {
        BOOST_TEST_CONTEXT("spot " << s)
        {
            spot->setValue(s);
            rate->setValue(r);
            BOOST_CHECK_SMALL(option.NPV() - discounted, 1e-14);
            const Real rho = option.rho();
            rate->setValue(r + h);
            const Real up = option.NPV();
            rate->setValue(r - h);
            const Real down = option.NPV();
            BOOST_CHECK_SMALL(rho - (up - down) / (2.0 * h), 1e-8);
        }
    }

    // Just inside a barrier the knock-in is all but certain, so its value meets the knocked-in one.
    rate->setValue(r);
    for (Real s : {95.0 * (1.0 + 1e-9), 105.0 * (1.0 - 1e-9)})
    {
        BOOST_TEST_CONTEXT("spot " << s)
        {
            spot->setValue(s);
            BOOST_CHECK_SMALL(option.NPV() - discounted, 1e-6);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
