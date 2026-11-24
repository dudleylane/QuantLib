/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 RiskMap srl

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
#include <ql/event.hpp>
#include <ql/experimental/credit/cdo.hpp>
#include <ql/experimental/credit/riskyassetswap.hpp>
#include <ql/instruments/bondforward.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/europeanoption.hpp>
#include <ql/instruments/stock.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/schedule.hpp>

using namespace QuantLib;
using namespace boost::unit_test;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, QuantLib::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(InstrumentTests)

BOOST_AUTO_TEST_CASE(testObservable)
{

    BOOST_TEST_MESSAGE("Testing observability of instruments...");

    ext::shared_ptr<SimpleQuote> me1(new SimpleQuote(0.0));
    RelinkableHandle<Quote> h(me1);
    ext::shared_ptr<Instrument> s(new Stock(h));

    Flag f;
    f.registerWith(s);

    s->NPV();
    me1->setValue(3.14);
    if (!f.isUp())
        BOOST_FAIL("Observer was not notified of instrument change");

    s->NPV();
    f.lower();
    ext::shared_ptr<SimpleQuote> me2(new SimpleQuote(0.0));
    h.linkTo(me2);
    if (!f.isUp())
        BOOST_FAIL("Observer was not notified of instrument change");

    f.lower();
    s->freeze();
    s->NPV();
    me2->setValue(2.71);
    if (f.isUp())
        BOOST_FAIL("Observer was notified of frozen instrument change");
    s->NPV();
    s->unfreeze();
    if (!f.isUp())
        BOOST_FAIL("Observer was not notified of instrument change");
}

BOOST_AUTO_TEST_CASE(testCompositeWhenShiftingDates)
{
    BOOST_TEST_MESSAGE("Testing reaction of composite instrument to date changes...");

    Date today = Date::todaysDate();
    DayCounter dc = Actual360();

    ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(Option::Call, 100.0));
    ext::shared_ptr<Exercise> exercise(new EuropeanExercise(today + 30));

    ext::shared_ptr<Instrument> option(new EuropeanOption(payoff, exercise));

    ext::shared_ptr<SimpleQuote> spot(new SimpleQuote(100.0));
    ext::shared_ptr<YieldTermStructure> qTS = flatRate(0.0, dc);
    ext::shared_ptr<YieldTermStructure> rTS = flatRate(0.01, dc);
    ext::shared_ptr<BlackVolTermStructure> volTS = flatVol(0.1, dc);

    ext::shared_ptr<BlackScholesMertonProcess> process(
        new BlackScholesMertonProcess(Handle<Quote>(spot), Handle<YieldTermStructure>(qTS),
                                      Handle<YieldTermStructure>(rTS), Handle<BlackVolTermStructure>(volTS)));
    ext::shared_ptr<PricingEngine> engine(new AnalyticEuropeanEngine(process));

    option->setPricingEngine(engine);

    CompositeInstrument composite;
    composite.add(option);

    Settings::instance().evaluationDate() = today + 45;

    if (!composite.isExpired())
        BOOST_FAIL("Composite didn't detect expiration");
    if (composite.NPV() != 0.0)
        BOOST_FAIL("Composite didn't return a null NPV");

    Settings::instance().evaluationDate() = today;

    if (composite.isExpired())
        BOOST_FAIL("Composite didn't detect aliveness");
    if (composite.NPV() == 0.0)
        BOOST_FAIL("Composite didn't recalculate");
}

BOOST_AUTO_TEST_CASE(testExpiryFollowsTheEventDateRule)
{
    BOOST_TEST_MESSAGE("Testing that expiry checks follow Event::hasOccurred's rule...");

    Date today(23, November, 2026);
    Settings::instance().evaluationDate() = today;

    // The rule, restated: an event on d has occurred by r, the reference date or else the evaluation date, if d < r;
    // also if d == r, unless events on the reference date are included (by the argument, or else by Settings).
    for (bool includedByDefault : {false, true})
    {
        Settings::instance().includeReferenceDateEvents() = includedByDefault;
        for (const Date& refDate : {Date(), today, today + 7})
        {
            const Date r = refDate == Date() ? today : refDate;
            for (const ext::optional<bool>& include :
                 {ext::optional<bool>(), ext::optional<bool>(true), ext::optional<bool>(false)})
            {
                const bool included = include ? *include : includedByDefault;
                for (const Date& d : {r - 1, r, r + 1})
                {
                    const bool expected = included ? d < r : d <= r;
                    BOOST_CHECK_EQUAL(detail::hasOccurred(d, refDate, include), expected);
                    BOOST_CHECK_EQUAL(detail::simple_event(d).hasOccurred(refDate, include), expected);
                }
            }
            for (const Date& d : {r - 1, r, r + 1})
            {
                const bool expected = includedByDefault ? d < r : d <= r;
                BOOST_CHECK_EQUAL(detail::hasOccurred(d, refDate), expected);
                if (refDate == Date())
                    BOOST_CHECK_EQUAL(detail::hasOccurred(d), expected);
            }
        }
    }

    // An option in the money expiring today is alive, and worth its exercise value, only while events on the
    // reference date are included; otherwise it has expired and is worth nothing.
    DayCounter dc = Actual360();
    auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)), Handle<YieldTermStructure>(flatRate(today, 0.0, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.0, dc)), Handle<BlackVolTermStructure>(flatVol(today, 0.2, dc)));
    for (bool included : {false, true})
    {
        Settings::instance().includeReferenceDateEvents() = included;
        EuropeanOption option(ext::make_shared<PlainVanillaPayoff>(Option::Call, 90.0),
                              ext::make_shared<EuropeanExercise>(today));
        option.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(process));
        BOOST_CHECK_EQUAL(option.isExpired(), !included);
        if (included)
            BOOST_CHECK_CLOSE(option.NPV(), 10.0, 1e-10);
        else
            BOOST_CHECK_EQUAL(option.NPV(), 0.0);
    }

    // RiskyAssetSwap and CDO ask the rule at their yield curve's reference date, and a forward at its settlement date.
    // Here each is two days after the evaluation date, a Monday: an instrument whose last date falls before it has
    // expired, and one whose last date is on it has too, unless events on the reference date are included.
    const Date refDate = today + 2;
    Handle<YieldTermStructure> curve(flatRate(refDate, 0.03, dc));
    Handle<DefaultProbabilityTermStructure> defaultCurve(ext::make_shared<FlatHazardRate>(refDate, 0.01, dc));
    for (bool included : {false, true})
    {
        Settings::instance().includeReferenceDateEvents() = included;
        for (const Date& last : {today, today + 1, refDate, refDate + 1})
        {
            const bool expected = included ? last < refDate : last <= refDate;
            Schedule schedule(last - 1 * Years, last, Period(Annual), NullCalendar(), Unadjusted, Unadjusted,
                              DateGeneration::Backward, false);
            RiskyAssetSwap swap(true, 100.0, schedule, schedule, dc, dc, 0.0, 0.4, curve, defaultCurve, 0.05);
            BOOST_CHECK_EQUAL(static_cast<const Instrument&>(swap).isExpired(), expected);
            CDO cdo(0.0, 0.1, {100.0}, {defaultCurve}, Handle<OneFactorCopula>(), true, schedule, 0.05, dc, 0.4, 0.0,
                    curve, 10);
            BOOST_CHECK_EQUAL(static_cast<const Instrument&>(cdo).isExpired(), expected);
            BondForward forward(today, last, Position::Long, 100.0, 2, dc, TARGET(), Following,
                                ext::make_shared<ZeroCouponBond>(0, TARGET(), 100.0, refDate + 365), curve, curve);
            BOOST_REQUIRE_EQUAL(forward.settlementDate(), refDate);
            BOOST_CHECK_EQUAL(static_cast<const Instrument&>(forward).isExpired(), expected);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
