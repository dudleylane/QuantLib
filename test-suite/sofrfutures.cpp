/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2018 Roy Zywina
 Copyright (C) 2018 StatPro Italia srl

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
#include <ql/indexes/ibor/sofr.hpp>
#include <ql/instruments/overnightindexfuture.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/overnightindexfutureratehelper.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <algorithm>
#include <iomanip>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(SofrFuturesTests)

struct SofrQuotes
{
    Frequency freq;
    Month month;
    Year year;
    Real price;
};


BOOST_AUTO_TEST_CASE(testBootstrap)
{
    BOOST_TEST_MESSAGE("Testing bootstrap over SOFR futures...");

    Date today = Date(26, October, 2018);
    Settings::instance().evaluationDate() = today;

    const SofrQuotes sofrQuotes[] = {
        {Monthly, Oct, 2018, 97.8175},  {Monthly, Nov, 2018, 97.770},   {Monthly, Dec, 2018, 97.685},
        {Monthly, Jan, 2019, 97.595},   {Monthly, Feb, 2019, 97.590},   {Monthly, Mar, 2019, 97.525},
        {Quarterly, Mar, 2019, 97.440}, {Quarterly, Jun, 2019, 97.295}, {Quarterly, Sep, 2019, 97.220},
        {Quarterly, Dec, 2019, 97.170}, {Quarterly, Mar, 2020, 97.160}, {Quarterly, Jun, 2020, 97.165},
        {Quarterly, Sep, 2020, 97.175},
    };

    ext::shared_ptr<OvernightIndex> index = ext::make_shared<Sofr>();
    index->addFixing(Date(1, October, 2018), 0.0222);
    index->addFixing(Date(2, October, 2018), 0.022);
    index->addFixing(Date(3, October, 2018), 0.022);
    index->addFixing(Date(4, October, 2018), 0.0218);
    index->addFixing(Date(5, October, 2018), 0.0216);
    index->addFixing(Date(9, October, 2018), 0.0215);
    index->addFixing(Date(10, October, 2018), 0.0215);
    index->addFixing(Date(11, October, 2018), 0.0217);
    index->addFixing(Date(12, October, 2018), 0.0218);
    index->addFixing(Date(15, October, 2018), 0.0221);
    index->addFixing(Date(16, October, 2018), 0.0218);
    index->addFixing(Date(17, October, 2018), 0.0218);
    index->addFixing(Date(18, October, 2018), 0.0219);
    index->addFixing(Date(19, October, 2018), 0.0219);
    index->addFixing(Date(22, October, 2018), 0.0218);
    index->addFixing(Date(23, October, 2018), 0.0217);
    index->addFixing(Date(24, October, 2018), 0.0218);
    index->addFixing(Date(25, October, 2018), 0.0219);

    std::vector<ext::shared_ptr<RateHelper>> helpers;
    for (const auto& sofrQuote : sofrQuotes)
    {
        helpers.push_back(
            ext::make_shared<SofrFutureRateHelper>(sofrQuote.price, sofrQuote.month, sofrQuote.year, sofrQuote.freq));
    }

    ext::shared_ptr<PiecewiseYieldCurve<Discount, Linear>> curve =
        ext::make_shared<PiecewiseYieldCurve<Discount, Linear>>(today, helpers, Actual365Fixed());

    // test curve with one of the futures
    ext::shared_ptr<OvernightIndex> sofr = ext::make_shared<Sofr>(Handle<YieldTermStructure>(curve));
    auto convQuote = ext::make_shared<SimpleQuote>();
    OvernightIndexFuture sf(sofr, Date(20, March, 2019), Date(19, June, 2019), Handle<Quote>(convQuote));

    Real tolerance = 1.0e-9;
    for (auto convAdj : {0.0, 0.1})
    {
        convQuote->setValue(convAdj);
        Real expected_price = 100.0 * (1 - (0.0256 + convAdj));
        Real error = std::fabs(sf.NPV() - expected_price);
        if (error > tolerance)
        {
            BOOST_ERROR("sample futures:\n"
                        << std::setprecision(8) << "\n estimated price: " << sf.NPV() << "\n expected price:  "
                        << expected_price << "\n error:           " << error << "\n tolerance:       " << tolerance);
        }
    }
}


BOOST_AUTO_TEST_CASE(testBootstrapWithJuneteenth)
{
    BOOST_TEST_MESSAGE("Testing bootstrap over SOFR futures when third Wednesday falls on Juneteenth...");

    Date today = Date(27, June, 2024);
    Settings::instance().evaluationDate() = today;

    const SofrQuotes sofrQuotes[] = {
        {Quarterly, Jun, 2024, 97.220}, {Quarterly, Sep, 2024, 97.170}, {Quarterly, Dec, 2024, 97.160},
        {Quarterly, Mar, 2025, 97.165}, {Quarterly, Jun, 2025, 97.175},
    };

    ext::shared_ptr<OvernightIndex> index = ext::make_shared<Sofr>();
    index->addFixing(Date(18, June, 2024), 0.02);
    index->addFixing(Date(20, June, 2024), 0.02);
    index->addFixing(Date(21, June, 2024), 0.02);
    index->addFixing(Date(24, June, 2024), 0.02);
    index->addFixing(Date(25, June, 2024), 0.02);
    index->addFixing(Date(26, June, 2024), 0.02);
    index->addFixing(Date(27, June, 2024), 0.02);

    std::vector<ext::shared_ptr<RateHelper>> helpers;
    for (const auto& sofrQuote : sofrQuotes)
    {
        helpers.push_back(
            ext::make_shared<SofrFutureRateHelper>(sofrQuote.price, sofrQuote.month, sofrQuote.year, sofrQuote.freq));
    }

    ext::shared_ptr<PiecewiseYieldCurve<Discount, Linear>> curve =
        ext::make_shared<PiecewiseYieldCurve<Discount, Linear>>(today, helpers, Actual365Fixed());

    ext::shared_ptr<OvernightIndex> sofr = ext::make_shared<Sofr>(Handle<YieldTermStructure>(curve));
    OvernightIndexFuture sf(sofr, Date(19, June, 2024), Date(18, September, 2024));

    Real expected_price = 97.220;
    Real tolerance = 1.0e-9;

    Real error = std::fabs(sf.NPV() - expected_price);
    if (error > tolerance)
    {
        BOOST_ERROR("sample futures:\n"
                    << std::setprecision(8) << "\n estimated price: " << sf.NPV() << "\n expected price:  "
                    << expected_price << "\n error:           " << error << "\n tolerance:       " << tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testPillarDates)
{
    BOOST_TEST_MESSAGE("Testing pillar date support in SOFR futures helpers...");

    Date today(15, March, 2024);
    Settings::instance().evaluationDate() = today;

    Handle<Quote> price(ext::make_shared<SimpleQuote>(99.0));
    auto index = ext::make_shared<Sofr>();

    Date valueDate(20, March, 2024);
    Date maturityDate(20, June, 2024);

    // Default pillar (LastRelevantDate)
    OvernightIndexFutureRateHelper h1(price, valueDate, maturityDate, index);
    BOOST_CHECK_EQUAL(h1.pillarDate(), maturityDate);

    // maturity pillar
    OvernightIndexFutureRateHelper h2(price, valueDate, maturityDate, index, {}, RateAveraging::Compound,
                                      Pillar::MaturityDate);
    BOOST_CHECK_EQUAL(h2.pillarDate(), maturityDate);

    // Custom pillar
    Date custom(20, April, 2024);
    OvernightIndexFutureRateHelper h3(price, valueDate, maturityDate, index, {}, RateAveraging::Compound,
                                      Pillar::CustomDate, custom);
    BOOST_CHECK_EQUAL(h3.pillarDate(), custom);

    // Invalid custom pillar (after maturity)
    Date badCustom(20, July, 2024);
    BOOST_CHECK_EXCEPTION(OvernightIndexFutureRateHelper(price, valueDate, maturityDate, index, {},
                                                         RateAveraging::Compound, Pillar::CustomDate, badCustom),
                          Error, ExpectedErrorMessage("after end of reference period"));

    // SOFR helper custom pillar
    Date sofrCustom(15, July, 2024);
    SofrFutureRateHelper sh(price, June, 2024, Quarterly, {}, Pillar::CustomDate, sofrCustom);
    BOOST_CHECK_EQUAL(sh.pillarDate(), sofrCustom);
}

BOOST_AUTO_TEST_CASE(testTodaysFixingWithHolidayMaturity)
{
    BOOST_TEST_MESSAGE("Testing today's fixing in a future maturing on a holiday...");

    // Juneteenth is the day after today, so the next business day is past the
    // maturity, and today's fixing has to stop accruing at the maturity.
    const Date today(18, June, 2024), valueDate(17, June, 2024), maturity(19, June, 2024);
    Settings::instance().evaluationDate() = today;

    Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(today, 0.0, Actual360()));
    auto sofr = ext::make_shared<Sofr>(curve);
    const Rate previous = 0.03, todays = 0.09;
    sofr->addFixing(valueDate, previous);
    sofr->addFixing(today, todays);

    OvernightIndexFuture future(sofr, valueDate, maturity);

    const DayCounter dayCounter = sofr->dayCounter();
    const Real growth = (1.0 + previous * dayCounter.yearFraction(valueDate, today)) *
                        (1.0 + todays * dayCounter.yearFraction(today, maturity));
    const Rate rate = (growth - 1.0) / dayCounter.yearFraction(valueDate, maturity);

    BOOST_CHECK_SMALL(future.NPV() - 100.0 * (1.0 - rate), 1.0e-10);
}

BOOST_AUTO_TEST_CASE(testAveragedRateStartingToday)
{
    BOOST_TEST_MESSAGE("Testing an averaging future whose first fixing is today's...");

    // A curve at zero forecasts every fixing at zero, so the price tells whether
    // today's fixing was read.  It is the future's first, so the future reads
    // the index's history although no earlier fixing is due (#89).
    const Date today(3, June, 2024), tomorrow(4, June, 2024), maturity(5, June, 2024);
    Settings::instance().evaluationDate() = today;

    Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(today, 0.0, Actual360()));
    auto sofr = ext::make_shared<Sofr>(curve);

    const OvernightIndexFuture notFixed(sofr, today, maturity, Handle<Quote>(), RateAveraging::Simple);
    BOOST_CHECK_SMALL(notFixed.NPV() - 100.0, 1.0e-10);

    const Rate todays = 0.09;
    sofr->addFixing(today, todays);

    const OvernightIndexFuture fixed(sofr, today, maturity, Handle<Quote>(), RateAveraging::Simple);
    const DayCounter dayCounter = sofr->dayCounter();
    const Rate rate = todays * dayCounter.yearFraction(today, tomorrow) / dayCounter.yearFraction(today, maturity);
    BOOST_CHECK_SMALL(fixed.NPV() - 100.0 * (1.0 - rate), 1.0e-10);
}

BOOST_AUTO_TEST_CASE(testCompoundedRateWithNextBusinessDayAfterMaturity)
{
    BOOST_TEST_MESSAGE("Testing a compounded future priced where the next business day is after its maturity...");

    // Every fixing is stored, so the rate is the fixings compounded up to the maturity; the curve's forward differs
    // from them, so a ratio of its discount factors other than 1 would show (#435).
    struct Row
    {
        Date valueDate, maturity, today;
        bool includeReferenceDateEvents;
    };
    const Row rows[] = {
        // SOFR's quarterly contract maturing on Juneteenth 2024, priced on its maturity
        {Date(20, March, 2024), Date(19, June, 2024), Date(19, June, 2024), true},
        // a future maturing on a Sunday, priced on the Saturday before
        {Date(3, August, 2026), Date(16, August, 2026), Date(15, August, 2026), false},
    };
    for (const Row& row : rows)
    {
        Settings::instance().evaluationDate() = row.today;
        Settings::instance().includeReferenceDateEvents() = row.includeReferenceDateEvents;

        Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(row.today, 0.05, Actual360()));
        auto sofr = ext::make_shared<Sofr>(curve);
        const Calendar calendar = sofr->fixingCalendar();
        const DayCounter dayCounter = sofr->dayCounter();
        const Rate fixing = 0.02;
        Real growth = 1.0;
        for (Date d1 = row.valueDate; d1 < row.maturity;)
        {
            const Date d2 = calendar.advance(d1, 1, Days);
            sofr->addFixing(d1, fixing);
            growth *= 1.0 + fixing * dayCounter.yearFraction(d1, std::min(d2, row.maturity));
            d1 = d2;
        }
        const Rate rate = (growth - 1.0) / dayCounter.yearFraction(row.valueDate, row.maturity);

        const OvernightIndexFuture future(sofr, row.valueDate, row.maturity);
        BOOST_CHECK_SMALL(future.NPV() - 100.0 * (1.0 - rate), 1.0e-10);
    }
}

BOOST_AUTO_TEST_CASE(testTodaysFixingWhereEnforced)
{
    BOOST_TEST_MESSAGE("Testing a future's fixing for today where today's fixings are enforced...");

    // A missing fixing for today is forecast unless today's fixings are enforced, as InterestRateIndex::fixing has
    // it; priced on a Saturday, the future forecasts Monday's fixing, which isn't due yet, either way (#427).
    const Date valueDate(2, March, 2026), maturity(1, April, 2026);
    const Rate fixing = 0.02;
    struct Row
    {
        Date today;
        bool enforced, todaysFixingStored;
        Date firstForecast; // the future throws where there's none
    };
    const Row rows[] = {
        {Date(10, March, 2026), false, false, Date(10, March, 2026)},
        {Date(10, March, 2026), true, false, Date()},
        {Date(10, March, 2026), true, true, Date(11, March, 2026)},
        {Date(7, March, 2026), true, false, Date(9, March, 2026)},
    };
    for (const Row& row : rows)
    {
        Settings::instance().evaluationDate() = row.today;
        Settings::instance().enforcesTodaysHistoricFixings() = row.enforced;
        IndexManager::instance().clearHistories();

        Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(row.today, 0.05, Actual360()));
        auto sofr = ext::make_shared<Sofr>(curve);
        const Calendar calendar = sofr->fixingCalendar();
        const DayCounter dayCounter = sofr->dayCounter();
        for (Date d = valueDate; d < row.today || (row.todaysFixingStored && d == row.today);
             d = calendar.advance(d, 1, Days))
            sofr->addFixing(d, fixing);

        const OvernightIndexFuture averaging(sofr, valueDate, maturity, Handle<Quote>(), RateAveraging::Simple);
        const OvernightIndexFuture compounding(sofr, valueDate, maturity);
        if (row.firstForecast == Date())
        {
            const std::string missing = "Missing SOFRON Actual/360 fixing for March 10th, 2026";
            BOOST_CHECK_EXCEPTION(averaging.NPV(), Error, ExpectedErrorMessage(missing));
            BOOST_CHECK_EXCEPTION(compounding.NPV(), Error, ExpectedErrorMessage(missing));
            continue;
        }

        // the stored fixings up to the first forecast, and the curve's forecasts from it
        Real sum = 0.0, growth = 1.0;
        for (Date d1 = valueDate; d1 < maturity;)
        {
            const Date d2 = calendar.advance(d1, 1, Days);
            const Time tau = dayCounter.yearFraction(d1, std::min(d2, maturity));
            if (d1 < row.firstForecast)
            {
                sum += fixing * tau;
                growth *= 1.0 + fixing * tau;
            }
            else
            {
                sum += curve->forwardRate(d1, d2, dayCounter, Simple).rate() * tau;
            }
            d1 = d2;
        }
        const Time span = dayCounter.yearFraction(valueDate, maturity);
        const Rate averaged = sum / span;
        const Rate compounded = (growth * curve->discount(row.firstForecast) / curve->discount(maturity) - 1.0) / span;
        BOOST_CHECK_SMALL(averaging.NPV() - 100.0 * (1.0 - averaged), 1.0e-10);
        BOOST_CHECK_SMALL(compounding.NPV() - 100.0 * (1.0 - compounded), 1.0e-10);
    }
}

BOOST_AUTO_TEST_CASE(testCompoundedRateOnItsValueDate)
{
    BOOST_TEST_MESSAGE("Testing a compounded future priced on its value date...");

    // On its value date the future's first fixing is due, and is taken where it's stored, as the averaged rate takes
    // it (#427); on the Saturday before, the fixing isn't due, and the future is priced off the curve alone.
    const Date valueDate(9, March, 2026), maturity(16, March, 2026);
    const Rate fixing = 0.02;
    for (const Date& today : {Date(9, March, 2026), Date(7, March, 2026)})
    {
        Settings::instance().evaluationDate() = today;
        IndexManager::instance().clearHistories();

        Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(today, 0.05, Actual360()));
        auto sofr = ext::make_shared<Sofr>(curve);
        sofr->addFixing(valueDate, fixing);
        const DayCounter dayCounter = sofr->dayCounter();
        const Date next = sofr->fixingCalendar().advance(valueDate, 1, Days);
        const Real growth = today == valueDate ? (1.0 + fixing * dayCounter.yearFraction(valueDate, next)) *
                                                     curve->discount(next) / curve->discount(maturity) :
                                                 curve->discount(valueDate) / curve->discount(maturity);
        const Rate rate = (growth - 1.0) / dayCounter.yearFraction(valueDate, maturity);

        const OvernightIndexFuture future(sofr, valueDate, maturity);
        BOOST_CHECK_SMALL(future.NPV() - 100.0 * (1.0 - rate), 1.0e-10);
    }
}

BOOST_AUTO_TEST_CASE(testRatesOnADayThatIsNotABusinessDay)
{
    BOOST_TEST_MESSAGE("Testing a future priced on a day that isn't a business day...");

    // The next business day's fixing isn't due until that day, so both rates forecast it from the curve, whether it's
    // stored or not (#436).
    struct Row
    {
        Date valueDate, maturity, today, next;
    };
    const Row rows[] = {
        // a Saturday
        {Date(2, March, 2026), Date(1, April, 2026), Date(7, March, 2026), Date(9, March, 2026)},
        // Juneteenth, a Friday
        {Date(1, June, 2026), Date(1, July, 2026), Date(19, June, 2026), Date(22, June, 2026)},
    };
    const Rate fixing = 0.02, nextFixing = 0.03;
    for (const Row& row : rows)
    {
        for (bool nextStored : {false, true})
        {
            Settings::instance().evaluationDate() = row.today;
            IndexManager::instance().clearHistories();

            Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(row.today, 0.05, Actual360()));
            auto sofr = ext::make_shared<Sofr>(curve);
            const Calendar calendar = sofr->fixingCalendar();
            const DayCounter dayCounter = sofr->dayCounter();
            for (Date d = row.valueDate; d < row.today; d = calendar.advance(d, 1, Days))
                sofr->addFixing(d, fixing);
            if (nextStored)
                sofr->addFixing(row.next, nextFixing);

            // the stored fixings up to today, and the curve's forecasts from the next business day
            Real sum = 0.0, growth = 1.0;
            for (Date d1 = row.valueDate; d1 < row.maturity;)
            {
                const Date d2 = calendar.advance(d1, 1, Days);
                const Time tau = dayCounter.yearFraction(d1, std::min(d2, row.maturity));
                if (d1 < row.next)
                {
                    sum += fixing * tau;
                    growth *= 1.0 + fixing * tau;
                }
                else
                {
                    sum += curve->forwardRate(d1, d2, dayCounter, Simple).rate() * tau;
                }
                d1 = d2;
            }
            const Time span = dayCounter.yearFraction(row.valueDate, row.maturity);
            const Rate averaged = sum / span;
            const Rate compounded = (growth * curve->discount(row.next) / curve->discount(row.maturity) - 1.0) / span;

            const OvernightIndexFuture averaging(sofr, row.valueDate, row.maturity, Handle<Quote>(),
                                                 RateAveraging::Simple);
            const OvernightIndexFuture compounding(sofr, row.valueDate, row.maturity);
            BOOST_CHECK_SMALL(averaging.NPV() - 100.0 * (1.0 - averaged), 1.0e-10);
            BOOST_CHECK_SMALL(compounding.NPV() - 100.0 * (1.0 - compounded), 1.0e-10);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCompoundedRateFromHolidayValueDate)
{
    BOOST_TEST_MESSAGE("Testing a compounded future whose value date is a holiday...");

    // SOFR's quarterly contract from Juneteenth 2030 takes Tuesday's fixing for its first day; that fixing is due on
    // Tuesday, and from then on the compounded rate reads it, as the averaged rate does (#427).
    const Date fixingDate(18, June, 2030), valueDate(19, June, 2030), next(20, June, 2030),
        maturity(18, September, 2030);
    const Rate fixing = 0.04;
    for (const Date& today : {fixingDate, valueDate})
    {
        Settings::instance().evaluationDate() = today;
        Settings::instance().enforcesTodaysHistoricFixings() = false;
        IndexManager::instance().clearHistories();

        Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(today, 0.045, Actual360()));
        auto sofr = ext::make_shared<Sofr>(curve);
        const DayCounter dayCounter = sofr->dayCounter();
        const Time span = dayCounter.yearFraction(valueDate, maturity);

        // stored, it applies from the value date to the next business day, and the curve from there
        sofr->addFixing(fixingDate, fixing);
        const Rate rate = ((1.0 + fixing * dayCounter.yearFraction(valueDate, next)) * curve->discount(next) /
                               curve->discount(maturity) -
                           1.0) /
                          span;
        BOOST_CHECK_SMALL(OvernightIndexFuture(sofr, valueDate, maturity).NPV() - 100.0 * (1.0 - rate), 1.0e-10);

        IndexManager::instance().clearHistories();
        if (today == fixingDate)
        {
            // missing on the day it's due, it's forecast, unless today's fixings are enforced
            const Rate forecast = (curve->discount(valueDate) / curve->discount(maturity) - 1.0) / span;
            BOOST_CHECK_SMALL(OvernightIndexFuture(sofr, valueDate, maturity).NPV() - 100.0 * (1.0 - forecast),
                              1.0e-10);
            Settings::instance().enforcesTodaysHistoricFixings() = true;
            const std::string missing = "Missing SOFRON Actual/360 fixing for June 18th, 2030";
            BOOST_CHECK_EXCEPTION(OvernightIndexFuture(sofr, valueDate, maturity).NPV(), Error,
                                  ExpectedErrorMessage(missing));
            BOOST_CHECK_EXCEPTION(
                OvernightIndexFuture(sofr, valueDate, maturity, Handle<Quote>(), RateAveraging::Simple).NPV(), Error,
                ExpectedErrorMessage(missing));
        }
        else
        {
            // missing after the day it was due, it's refused
            BOOST_CHECK_EXCEPTION(OvernightIndexFuture(sofr, valueDate, maturity).NPV(), Error,
                                  ExpectedErrorMessage("missing rate on June 18th, 2030 for index SOFRON Actual/360"));
        }
    }
}

BOOST_AUTO_TEST_CASE(testCompoundedRateWithoutForwardingCurve)
{
    BOOST_TEST_MESSAGE("Testing a compounded future whose fixings are all stored, without a forwarding curve...");

    // Every fixing up to the maturity is stored, so there's nothing to forecast (#427).
    const Date today(6, March, 2026), valueDate(2, March, 2026), maturity(9, March, 2026);
    Settings::instance().evaluationDate() = today;

    auto sofr = ext::make_shared<Sofr>();
    const Calendar calendar = sofr->fixingCalendar();
    const DayCounter dayCounter = sofr->dayCounter();
    const Rate fixing = 0.02;
    Real growth = 1.0;
    for (Date d1 = valueDate; d1 < maturity;)
    {
        const Date d2 = calendar.advance(d1, 1, Days);
        sofr->addFixing(d1, fixing);
        growth *= 1.0 + fixing * dayCounter.yearFraction(d1, d2);
        d1 = d2;
    }
    const Rate rate = (growth - 1.0) / dayCounter.yearFraction(valueDate, maturity);

    const OvernightIndexFuture averaging(sofr, valueDate, maturity, Handle<Quote>(), RateAveraging::Simple);
    const OvernightIndexFuture compounding(sofr, valueDate, maturity);
    BOOST_CHECK_SMALL(averaging.NPV() - 100.0 * (1.0 - fixing), 1.0e-10);
    BOOST_CHECK_SMALL(compounding.NPV() - 100.0 * (1.0 - rate), 1.0e-10);
}

BOOST_AUTO_TEST_CASE(testForecastWithoutUsableForwardingCurve)
{
    BOOST_TEST_MESSAGE("Testing a future that has to forecast without a curve that can...");

    // Both rates say which index can't be forecast from which day, and why (#427).
    const Date today(10, March, 2026), valueDate(9, March, 2026), maturity(9, April, 2026);
    Settings::instance().evaluationDate() = today;

    struct Row
    {
        bool withCurve, todaysFixingStored;
        std::string error;
    };
    const Row rows[] = {
        {false, false, "can't forecast SOFRON Actual/360 from March 10th, 2026: it has no forwarding curve"},
        {true, false,
         "can't forecast SOFRON Actual/360 from March 10th, 2026: its forwarding curve starts on March 12th, 2026"},
        {true, true,
         "can't forecast SOFRON Actual/360 from March 11th, 2026: its forwarding curve starts on March 12th, 2026"},
    };
    for (const Row& row : rows)
    {
        IndexManager::instance().clearHistories();
        Handle<YieldTermStructure> curve;
        if (row.withCurve)
            curve = Handle<YieldTermStructure>(ext::make_shared<FlatForward>(Date(12, March, 2026), 0.05, Actual360()));
        auto sofr = ext::make_shared<Sofr>(curve);
        sofr->addFixing(valueDate, 0.02);
        if (row.todaysFixingStored)
            sofr->addFixing(today, 0.02);

        BOOST_CHECK_EXCEPTION(OvernightIndexFuture(sofr, valueDate, maturity).NPV(), Error,
                              ExpectedErrorMessage(row.error));
        BOOST_CHECK_EXCEPTION(
            OvernightIndexFuture(sofr, valueDate, maturity, Handle<Quote>(), RateAveraging::Simple).NPV(), Error,
            ExpectedErrorMessage(row.error));
    }
}

BOOST_AUTO_TEST_CASE(testHelperNotNotifiedByItsOwnBootstrap)
{
    BOOST_TEST_MESSAGE("Testing that a future's rate helper isn't notified when it bootstraps a curve...");

    const Date today(26, October, 2018);
    Settings::instance().evaluationDate() = today;

    auto price = ext::make_shared<SimpleQuote>(97.52);
    auto helper = ext::make_shared<OvernightIndexFutureRateHelper>(Handle<Quote>(price), Date(20, March, 2019),
                                                                   Date(19, June, 2019), ext::make_shared<Sofr>());
    auto curve = ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear>>(
        today, std::vector<ext::shared_ptr<RateHelper>>{helper}, Actual360());

    Flag flag;
    flag.registerWith(helper);

    curve->nodes();
    BOOST_CHECK(!flag.isUp());

    // It still hears about what it depends on, and the curve still reprices it.
    price->setValue(97.60);
    BOOST_CHECK(flag.isUp());
    curve->nodes();
    BOOST_CHECK_SMALL(helper->quoteError(), 1.0e-10);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
