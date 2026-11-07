/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 Shubham Gaur

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
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/exercise.hpp>
#include <ql/experimental/coupons/swapspreadindex.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/floatfloatswap.hpp>
#include <ql/instruments/floatfloatswaption.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dfloatfloatswaptionengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/rebatedexercise.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/schedule.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FloatFloatSwapTests)

struct CommonVars
{
    Date today, settlement;
    Real nominal;
    Calendar calendar;
    RelinkableHandle<YieldTermStructure> termStructure;
    ext::shared_ptr<IborIndex> index1, index2;
    Natural settlementDays;

    ext::shared_ptr<FloatFloatSwap>
    makeSwap(Swap::Type type, Spread spread1, Spread spread2, Integer lengthInYears = 10) const
    {

        Date maturity = calendar.advance(settlement, lengthInYears, Years, ModifiedFollowing);

        Schedule schedule1(settlement, maturity, index1->tenor(), calendar, ModifiedFollowing, ModifiedFollowing,
                           DateGeneration::Forward, false);

        Schedule schedule2(settlement, maturity, index2->tenor(), calendar, ModifiedFollowing, ModifiedFollowing,
                           DateGeneration::Forward, false);

        auto swap = ext::make_shared<FloatFloatSwap>(
            type, nominal, nominal, schedule1, index1, index1->dayCounter(), schedule2, index2, index2->dayCounter(),
            false, false, 1.0, spread1, Null<Real>(), Null<Real>(), 1.0, spread2, Null<Real>(), Null<Real>());

        auto engine = ext::make_shared<DiscountingSwapEngine>(termStructure);
        swap->setPricingEngine(engine);

        auto pricer = ext::make_shared<BlackIborCouponPricer>();
        setCouponPricer(swap->leg1(), pricer);
        setCouponPricer(swap->leg2(), pricer);

        return swap;
    }

    CommonVars()
    {
        settlementDays = 2;
        nominal = 100.0;
        calendar = TARGET();
        today = calendar.adjust(Settings::instance().evaluationDate());
        settlement = calendar.advance(today, settlementDays, Days);
        termStructure.linkTo(flatRate(settlement, 0.05, Actual365Fixed()));
        index1 = ext::make_shared<Euribor>(3 * Months, termStructure);
        index2 = ext::make_shared<Euribor>(6 * Months, termStructure);
    }
};

// Values below are pinned, so the date has to be set before CommonVars reads it.
struct PinnedDate
{
    PinnedDate() { Settings::instance().evaluationDate() = Date(10, June, 2025); }
};

struct Gaussian1dVars : PinnedDate, CommonVars
{
    ext::shared_ptr<Gsr> model;
    ext::shared_ptr<SwaptionVolatilityStructure> swaptionVolatility;
    ext::shared_ptr<SwapIndex> standardSwapBase;

    Gaussian1dVars()
    {
        model = ext::make_shared<Gsr>(termStructure, std::vector<Date>(), std::vector<Real>{0.01}, 0.03, 60.0);
        swaptionVolatility =
            ext::make_shared<ConstantSwaptionVolatility>(settlementDays, calendar, Following, 0.20, Actual365Fixed());
        standardSwapBase = ext::make_shared<EuriborSwapIsdaFixA>(5 * Years, termStructure);
    }

    Schedule schedule(const Period& tenor, Integer lengthInYears) const
    {
        return Schedule(settlement, calendar.advance(settlement, lengthInYears * Years), tenor, calendar,
                        ModifiedFollowing, ModifiedFollowing, DateGeneration::Forward, false);
    }

    // With a gearing of 0 the first leg pays its spread alone, so this is a plain swap.
    ext::shared_ptr<FloatFloatSwap> swapPayingAFixedRate(Swap::Type type,
                                                         Rate fixedRate,
                                                         const Schedule& fixedSchedule,
                                                         const Schedule& floatingSchedule) const
    {
        return ext::make_shared<FloatFloatSwap>(type, nominal, nominal, fixedSchedule, index2, Actual365Fixed(),
                                                floatingSchedule, index2, index2->dayCounter(), false, false, 0.0,
                                                fixedRate, Null<Real>(), Null<Real>(), 1.0, 0.0, Null<Real>(),
                                                Null<Real>());
    }
};


BOOST_AUTO_TEST_CASE(testFairSpread1)
{

    BOOST_TEST_MESSAGE("Testing float-float swap calculation of fair spread on leg 1...");

    CommonVars vars;

    Swap::Type types[] = {Swap::Payer, Swap::Receiver};
    Spread spread2Values[] = {-0.002, 0.0, 0.002, 0.005};

    for (auto type : types)
    {
        for (Real spread2 : spread2Values)
        {

            auto swap = vars.makeSwap(type, 0.0, spread2);
            Spread fair = swap->fairSpread1();

            auto swap2 = vars.makeSwap(type, fair, spread2);
            if (std::fabs(swap2->NPV()) > 1.0e-10)
            {
                BOOST_ERROR("recalculating with fair spread on leg 1:\n"
                            << "    type: " << ((type == Swap::Payer) ? "Payer" : "Receiver") << "\n"
                            << "    spread on leg 2: " << io::rate(spread2) << "\n"
                            << "    fair spread on leg 1: " << io::rate(fair) << "\n"
                            << "    swap NPV: " << swap2->NPV());
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(testFairSpread2)
{

    BOOST_TEST_MESSAGE("Testing float-float swap calculation of fair spread on leg 2...");

    CommonVars vars;

    Swap::Type types[] = {Swap::Payer, Swap::Receiver};
    Spread spread1Values[] = {-0.002, 0.0, 0.002, 0.005};

    for (auto type : types)
    {
        for (Real spread1 : spread1Values)
        {

            auto swap = vars.makeSwap(type, spread1, 0.0);
            Spread fair = swap->fairSpread2();

            auto swap2 = vars.makeSwap(type, spread1, fair);
            if (std::fabs(swap2->NPV()) > 1.0e-10)
            {
                BOOST_ERROR("recalculating with fair spread on leg 2:\n"
                            << "    type: " << ((type == Swap::Payer) ? "Payer" : "Receiver") << "\n"
                            << "    spread on leg 1: " << io::rate(spread1) << "\n"
                            << "    fair spread on leg 2: " << io::rate(fair) << "\n"
                            << "    swap NPV: " << swap2->NPV());
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(testPayerReceiverSymmetry)
{

    BOOST_TEST_MESSAGE("Testing float-float swap payer/receiver NPV symmetry...");

    CommonVars vars;

    Spread spread1 = 0.001;
    Spread spread2 = 0.003;

    auto payer = vars.makeSwap(Swap::Payer, spread1, spread2);
    auto receiver = vars.makeSwap(Swap::Receiver, spread1, spread2);

    Real tolerance = 1.0e-10;
    if (std::fabs(payer->NPV() + receiver->NPV()) > tolerance)
    {
        BOOST_ERROR("payer and receiver NPVs do not cancel:\n"
                    << "    payer NPV:    " << payer->NPV() << "\n"
                    << "    receiver NPV: " << receiver->NPV() << "\n"
                    << "    sum:          " << (payer->NPV() + receiver->NPV()));
    }
}


BOOST_AUTO_TEST_CASE(testFairSpreadPayerReceiverConsistency)
{

    BOOST_TEST_MESSAGE("Testing float-float swap fair spread consistency "
                       "between payer and receiver...");

    CommonVars vars;

    Spread spread2 = 0.002;

    auto payer = vars.makeSwap(Swap::Payer, 0.0, spread2);
    auto receiver = vars.makeSwap(Swap::Receiver, 0.0, spread2);

    Spread fairPayer = payer->fairSpread1();
    Spread fairReceiver = receiver->fairSpread1();

    Real tolerance = 1.0e-10;
    if (std::fabs(fairPayer - fairReceiver) > tolerance)
    {
        BOOST_ERROR("fair spread on leg 1 differs between payer and receiver:\n"
                    << "    payer fair spread 1:    " << io::rate(fairPayer) << "\n"
                    << "    receiver fair spread 1: " << io::rate(fairReceiver));
    }

    auto payer2 = vars.makeSwap(Swap::Payer, spread2, 0.0);
    auto receiver2 = vars.makeSwap(Swap::Receiver, spread2, 0.0);

    fairPayer = payer2->fairSpread2();
    fairReceiver = receiver2->fairSpread2();

    if (std::fabs(fairPayer - fairReceiver) > tolerance)
    {
        BOOST_ERROR("fair spread on leg 2 differs between payer and receiver:\n"
                    << "    payer fair spread 2:    " << io::rate(fairPayer) << "\n"
                    << "    receiver fair spread 2: " << io::rate(fairReceiver));
    }
}


BOOST_AUTO_TEST_CASE(testFairSpreadsWithZeroBps)
{
    BOOST_TEST_MESSAGE("Testing a float-float swap's fair spreads when its legs have no BPS...");

    CommonVars vars;
    vars.nominal = 0.0;
    auto swap = vars.makeSwap(Swap::Payer, 0.0, 0.0);

    BOOST_CHECK(!swap->isExpired());
    BOOST_CHECK_EQUAL(swap->legBPS(0), 0.0);
    BOOST_CHECK_EQUAL(swap->legBPS(1), 0.0);

    BOOST_CHECK_EXCEPTION(swap->fairSpread1(), Error, ExpectedErrorMessage("fair spread 1 not available"));
    BOOST_CHECK_EXCEPTION(swap->fairSpread2(), Error, ExpectedErrorMessage("fair spread 2 not available"));
}

BOOST_AUTO_TEST_CASE(testGaussian1dEngineAgainstPlainSwaptions)
{
    BOOST_TEST_MESSAGE("Testing the Gaussian 1d float-float swaption engine against plain swaptions...");

    const Gaussian1dVars vars;
    const Schedule fixedSchedule = vars.schedule(1 * Years, 10);
    const Schedule floatingSchedule = vars.schedule(6 * Months, 10);

    // The engine takes in what fixes on or after the exercise, so these exercise on fixing dates. Later than
    // that and before the coupon starts, it leaves out a coupon that the plain engine takes in.
    std::vector<Date> dates;
    for (Size i : {4, 8, 12})
        dates.push_back(vars.index2->fixingDate(floatingSchedule.date(i)));

    const auto plainEngine =
        ext::make_shared<Gaussian1dSwaptionEngine>(vars.model, 32, 7.0, true, false, vars.termStructure);
    const auto floatFloatEngine = ext::make_shared<Gaussian1dFloatFloatSwaptionEngine>(
        vars.model, 32, 7.0, true, false, Handle<Quote>(), vars.termStructure);

    for (Rate fixedRate : {0.03, 0.05, 0.07})
    {
        for (Swap::Type type : {Swap::Payer, Swap::Receiver})
        {
            for (bool bermudan : {false, true})
            {
                BOOST_TEST_CONTEXT("fixed rate " << fixedRate << ", " << type
                                                 << (bermudan ? ", Bermudan" : ", European"))
                {
                    ext::shared_ptr<Exercise> exercise;
                    if (bermudan)
                        exercise = ext::make_shared<BermudanExercise>(dates);
                    else
                        exercise = ext::make_shared<EuropeanExercise>(dates[1]);

                    Swaption plain(ext::make_shared<VanillaSwap>(type, vars.nominal, fixedSchedule, fixedRate,
                                                                 Actual365Fixed(), floatingSchedule, vars.index2, 0.0,
                                                                 vars.index2->dayCounter()),
                                   exercise);
                    plain.setPricingEngine(plainEngine);

                    FloatFloatSwaption floatFloat(
                        vars.swapPayingAFixedRate(type, fixedRate, fixedSchedule, floatingSchedule), exercise);
                    floatFloat.setPricingEngine(floatFloatEngine);

                    BOOST_CHECK_SMALL(floatFloat.NPV() - plain.NPV(), 5.0e-4);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testGaussian1dEngineOnEveryRollBackPath)
{
    BOOST_TEST_MESSAGE("Testing the Gaussian 1d float-float swaption engine on each path of its roll-back...");

    using Engine = Gaussian1dFloatFloatSwaptionEngine;

    const Gaussian1dVars vars;
    const Schedule schedule1 = vars.schedule(3 * Months, 5);
    const Schedule schedule2 = vars.schedule(6 * Months, 5);

    const auto swap = [&](Swap::Type type, const ext::shared_ptr<InterestRateIndex>& index1,
                          const ext::shared_ptr<InterestRateIndex>& index2, bool capped = false)
    {
        const Rate cap = capped ? 0.01 : Null<Real>();
        const Rate floor = capped ? 0.001 : Null<Real>();
        return ext::make_shared<FloatFloatSwap>(type, vars.nominal, vars.nominal, schedule1, index1,
                                                index1->dayCounter(), schedule2, index2, index2->dayCounter(), capped,
                                                capped, 1.0, 0.001, cap, floor, 1.0, -0.001, cap, floor);
    };
    const auto cms5 = ext::make_shared<EuriborSwapIsdaFixA>(5 * Years, vars.termStructure);
    const auto cms10 = ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, vars.termStructure);
    const auto spreadIndex = ext::make_shared<SwapSpreadIndex>("cms spread", cms10, cms5);
    const auto payer = swap(Swap::Payer, vars.index1, vars.index2);
    const auto receiver = swap(Swap::Receiver, vars.index1, vars.index2);

    const auto exercise = ext::make_shared<BermudanExercise>(
        std::vector<Date>{vars.calendar.advance(vars.today, 2 * Years), vars.calendar.advance(vars.today, 3 * Years)});
    const auto rebated = ext::make_shared<RebatedExercise>(*exercise, std::vector<Real>{100.0, 50.0}, 2, vars.calendar);
    const auto atSettlement = ext::make_shared<EuropeanExercise>(vars.settlement);

    struct PathCase
    {
        std::string name;
        ext::shared_ptr<FloatFloatSwap> swap;
        ext::shared_ptr<Exercise> exercise;
        Engine::Probabilities probabilities;
        bool extrapolate, flatExtrapolation, spreadOverTheCurve;
        Real value, underlying;
        std::vector<Real> exerciseProbabilities;
    };
    // What the engine gave before its splines moved out of the loop over the grid.
    const std::vector<PathCase> cases = {
        {"call", payer, exercise, Engine::None, true, false, false, 4.3650927110819524e-06, -2.0376482158632796, {}},
        {"flat call with a spread",
         payer,
         exercise,
         Engine::Naive,
         true,
         true,
         true,
         4.371077784855318e-06,
         -2.0368221818260386,
         {0.0, 0.0002122396580337433, 0.99380027773704815}},
        {"flat call",
         payer,
         exercise,
         Engine::Naive,
         true,
         true,
         false,
         4.3663108633836434e-06,
         -2.0376490476837383,
         {0.0, 0.00021351809390680342, 0.99978648190609176}},
        {"put", receiver, exercise, Engine::None, true, false, false, 1.317453245487997, 2.0376361102247182, {}},
        {"put with no extrapolation",
         receiver,
         exercise,
         Engine::Digital,
         false,
         false,
         false,
         1.3174511047449278,
         2.0376339248646063,
         {0.0010143157355989797, 0.99890422191252404, 1.0494638764143242e-05}},
        {"digital call",
         payer,
         exercise,
         Engine::Digital,
         true,
         false,
         false,
         4.3650927110819524e-06,
         -2.0376482158632796,
         {0.0, 3.3039680156532956e-05, 0.99995827294831785}},
        {"digital put",
         receiver,
         exercise,
         Engine::Digital,
         true,
         false,
         false,
         1.317453245487997,
         2.0376361102247182,
         {0.0010144267599719069, 0.99890608153039295, 1.0497930694464126e-05}},
        {"put with a spread",
         receiver,
         exercise,
         Engine::Naive,
         true,
         false,
         true,
         1.3093143779184901,
         2.0368092848787058,
         {0.0034842461481403592, 0.99047000457560697, 6.7264042999569127e-05}},
        {"rebated call with a spread",
         payer,
         rebated,
         Engine::Naive,
         true,
         false,
         true,
         89.596578117401251,
         -2.0368213570150591,
         {0.99601662047906336, 0.0, 0.0}},
        {"capped and floored, exchanging capital",
         swap(Swap::Payer, vars.index1, vars.index2, true),
         exercise,
         Engine::None,
         true,
         false,
         false,
         0.0,
         -0.27351502583060228,
         {}},
        {"cms",
         swap(Swap::Payer, cms10, vars.index2),
         exercise,
         Engine::None,
         false,
         false,
         false,
         0.0,
         -2.8052521273232531,
         {}},
        {"cms spread on leg 1",
         swap(Swap::Receiver, spreadIndex, cms5),
         exercise,
         Engine::None,
         true,
         false,
         false,
         0.00081515794727344279,
         -19.154177189928934,
         {}},
        {"cms spread on leg 2",
         swap(Swap::Payer, vars.index1, spreadIndex),
         exercise,
         Engine::None,
         true,
         false,
         false,
         0.00084508261142160208,
         -21.552036734515646,
         {}}};

    const Handle<Quote> spread(ext::make_shared<SimpleQuote>(0.002));
    const Real tolerance = 1.0e-10;
    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.name)
        {
            FloatFloatSwaption swaption(c.swap, c.exercise);
            swaption.setPricingEngine(ext::make_shared<Engine>(vars.model, 12, 5.0, c.extrapolate, c.flatExtrapolation,
                                                               c.spreadOverTheCurve ? spread : Handle<Quote>(),
                                                               vars.termStructure, false, c.probabilities));

            BOOST_CHECK_SMALL(swaption.NPV() - c.value, tolerance);
            BOOST_CHECK_SMALL(swaption.result<Real>("underlyingValue") - c.underlying, tolerance);
            if (c.probabilities != Engine::None)
            {
                const auto probabilities = swaption.result<std::vector<Real>>("probabilities");
                BOOST_REQUIRE_EQUAL(probabilities.size(), c.exerciseProbabilities.size());
                for (Size i = 0; i < probabilities.size(); ++i)
                    BOOST_CHECK_SMALL(probabilities[i] - c.exerciseProbabilities[i], tolerance);
            }
        }
    }

    FloatFloatSwaption cashSettled(payer, exercise, Settlement::Cash, Settlement::ParYieldCurve);
    cashSettled.setPricingEngine(ext::make_shared<Engine>(vars.model, 12, 5.0));
    BOOST_CHECK_EXCEPTION(cashSettled.NPV(), Error,
                          ExpectedErrorMessage("cash settled (ParYieldCurve) swaptions not priced"));

    FloatFloatSwaption intoTheSettlementDate(payer, atSettlement);
    intoTheSettlementDate.setPricingEngine(ext::make_shared<Engine>(vars.model, 12, 5.0));
    BOOST_CHECK_SMALL(intoTheSettlementDate.NPV(), tolerance);
}

BOOST_AUTO_TEST_CASE(testCalibrationBasketOfAFloatFloatSwaption)
{
    BOOST_TEST_MESSAGE("Testing the calibration basket of a float-float swaption...");

    const Gaussian1dVars vars;
    const Schedule fixedSchedule = vars.schedule(1 * Years, 10);
    const Schedule floatingSchedule = vars.schedule(6 * Months, 10);
    const auto exercise = ext::make_shared<BermudanExercise>(std::vector<Date>{
        vars.index2->fixingDate(floatingSchedule.date(4)), vars.index2->fixingDate(floatingSchedule.date(8))});

    // A swap that pays a fixed rate should come back as itself, within what the fit allows.
    for (Rate fixedRate : {0.03, 0.05, 0.07})
    {
        BOOST_TEST_CONTEXT("fixed rate " << fixedRate)
        {
            FloatFloatSwaption swaption(
                vars.swapPayingAFixedRate(Swap::Payer, fixedRate, fixedSchedule, floatingSchedule), exercise);
            swaption.setPricingEngine(ext::make_shared<Gaussian1dFloatFloatSwaptionEngine>(vars.model, 16, 5.0));

            BOOST_CHECK_EQUAL(
                swaption
                    .calibrationBasket(vars.standardSwapBase, vars.swaptionVolatility, BasketGeneratingEngine::Naive)
                    .size(),
                exercise->dates().size());

            const auto basket = swaption.calibrationBasket(vars.standardSwapBase, vars.swaptionVolatility,
                                                           BasketGeneratingEngine::MaturityStrikeByDeltaGamma);
            BOOST_REQUIRE_EQUAL(basket.size(), exercise->dates().size());
            for (Size i = 0; i < basket.size(); ++i)
            {
                const auto helper = ext::dynamic_pointer_cast<SwaptionHelper>(basket[i]);
                BOOST_REQUIRE(helper);
                BOOST_CHECK_EQUAL(helper->swaption()->exercise()->dates().front(), exercise->dates()[i]);
                const auto underlying = helper->underlying();
                BOOST_CHECK_SMALL(underlying->nominal() - vars.nominal, 0.2);
                BOOST_CHECK_SMALL(underlying->fixedRate() - fixedRate, 1.0e-4);
                BOOST_CHECK_SMALL(Real(underlying->fixedSchedule().endDate() - fixedSchedule.endDate()), 5.0);
            }
        }
    }

    // The fit lands in the same place from a poor start, so what it starts from has to be asked for.
    struct EngineShowingItsGuess : Gaussian1dFloatFloatSwaptionEngine
    {
        using Gaussian1dFloatFloatSwaptionEngine::Gaussian1dFloatFloatSwaptionEngine;
        using Gaussian1dFloatFloatSwaptionEngine::initialGuess;
    };

    const Schedule yearly = vars.schedule(1 * Years, 5);
    const auto amortizing = ext::make_shared<FloatFloatSwap>(
        Swap::Payer, std::vector<Real>{100.0, 80.0, 60.0, 40.0, 20.0}, std::vector<Real>(5, vars.nominal), yearly,
        vars.index2, vars.index2->dayCounter(), yearly, vars.index2, vars.index2->dayCounter());
    FloatFloatSwaption amortizingSwaption(amortizing, ext::make_shared<EuropeanExercise>(yearly.date(3)));
    const auto engine = ext::make_shared<EngineShowingItsGuess>(vars.model, 16, 5.0);
    amortizingSwaption.setupArguments(engine->getArguments());

    // two coupons are left of the five, of 40 and 20
    BOOST_CHECK_CLOSE(engine->initialGuess(yearly.date(3))[0], 30.0, 1.0e-12);
}

BOOST_AUTO_TEST_CASE(testCalibrationBasketWithNothingToStartFrom)
{
    BOOST_TEST_MESSAGE("Testing that a float-float calibration refuses what leaves it no first guess...");

    const Gaussian1dVars vars;
    const auto engine = ext::make_shared<Gaussian1dFloatFloatSwaptionEngine>(vars.model, 12, 5.0);

    const Schedule oneYear = vars.schedule(6 * Months, 1);
    const auto oneYearSwap =
        ext::make_shared<FloatFloatSwap>(Swap::Payer, vars.nominal, vars.nominal, oneYear, vars.index2,
                                         vars.index2->dayCounter(), oneYear, vars.index2, vars.index2->dayCounter());
    // Without the rebate there'd be nothing left to match, and the basket says so before it needs a guess.
    const Date afterTheLastReset = vars.calendar.advance(vars.settlement, 9 * Months);
    FloatFloatSwaption noResetLeft(
        oneYearSwap, ext::make_shared<RebatedExercise>(EuropeanExercise(afterTheLastReset), 100.0, 2, vars.calendar));
    noResetLeft.setPricingEngine(engine);
    BOOST_CHECK_EXCEPTION(noResetLeft.calibrationBasket(vars.standardSwapBase, vars.swaptionVolatility,
                                                        BasketGeneratingEngine::MaturityStrikeByDeltaGamma),
                          Error, ExpectedErrorMessage("no leg 1 reset dates remain"));

    // A second leg that runs on leaves something to match with no rebate.
    const Schedule fiveYears = vars.schedule(6 * Months, 5);
    const auto legsOfTheirOwnLengths =
        ext::make_shared<FloatFloatSwap>(Swap::Payer, vars.nominal, vars.nominal, oneYear, vars.index2,
                                         vars.index2->dayCounter(), fiveYears, vars.index2, vars.index2->dayCounter());
    FloatFloatSwaption firstLegOver(legsOfTheirOwnLengths, ext::make_shared<EuropeanExercise>(afterTheLastReset));
    firstLegOver.setPricingEngine(engine);
    BOOST_CHECK_EXCEPTION(firstLegOver.calibrationBasket(vars.standardSwapBase, vars.swaptionVolatility,
                                                         BasketGeneratingEngine::MaturityStrikeByDeltaGamma),
                          Error, ExpectedErrorMessage("no leg 1 reset dates remain"));

    const Schedule quarterly = vars.schedule(3 * Months, 5);
    std::vector<Real> nominals1(quarterly.size() - 1, 0.0);
    nominals1[nominals1.size() - 2] = vars.nominal;
    nominals1.back() = -vars.nominal;
    const std::vector<Real> nominals2(quarterly.size() - 1, vars.nominal);
    const auto cancellingSwap =
        ext::make_shared<FloatFloatSwap>(Swap::Payer, nominals1, nominals2, quarterly, vars.index1,
                                         vars.index1->dayCounter(), quarterly, vars.index1, vars.index1->dayCounter());
    FloatFloatSwaption nothingLeftOnTheLeg(
        cancellingSwap, ext::make_shared<EuropeanExercise>(vars.calendar.advance(vars.today, 2 * Years)));
    nothingLeftOnTheLeg.setPricingEngine(engine);
    BOOST_CHECK_EXCEPTION(nothingLeftOnTheLeg.calibrationBasket(vars.standardSwapBase, vars.swaptionVolatility,
                                                                BasketGeneratingEngine::MaturityStrikeByDeltaGamma),
                          Error, ExpectedErrorMessage("remaining leg 1 notionals sum to zero"));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
