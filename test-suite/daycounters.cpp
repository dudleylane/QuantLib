/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 RiskMap srl
 Copyright (C) 2006 Piter Dias
 Copyright (C) 2012 Simon Shakeshaft
 Copyright (c) 2015 Klaus Spanderen

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
#include <ql/math/comparison.hpp>
#include <ql/time/calendars/bespokecalendar.hpp>
#include <ql/time/calendars/brazil.hpp>
#include <ql/time/calendars/canada.hpp>
#include <ql/time/calendars/china.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual364.hpp>
#include <ql/time/daycounters/actual36525.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actual366.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/business252.hpp>
#include <ql/time/daycounters/one.hpp>
#include <ql/time/daycounters/simpledaycounter.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/daycounters/thirty365.hpp>
#include <ql/time/daycounters/yearfractiontodate.hpp>
#include <ql/time/schedule.hpp>
#include <cmath>
#include <iomanip>
#include <thread>
#include <utility>
#include <vector>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DayCounterTests)

struct SingleCase
{
    SingleCase(ActualActual::Convention convention,
               const Date& start,
               const Date& end,
               const Date& refStart,
               const Date& refEnd,
               Time result)
    : convention(convention), start(start), end(end), refStart(refStart), refEnd(refEnd), result(result)
    {
    }
    SingleCase(ActualActual::Convention convention, const Date& start, const Date& end, Time result)
    : convention(convention), start(start), end(end), result(result)
    {
    }
    ActualActual::Convention convention;
    Date start;
    Date end;
    Date refStart;
    Date refEnd;
    Time result;
};

struct Thirty360Case
{
    Date start;
    Date end;
    Date::serial_type expected;
};

Time ISMAYearFractionWithReferenceDates(const DayCounter& dayCounter, Date start, Date end, Date refStart, Date refEnd)
{
    Real referenceDayCount = Real(dayCounter.dayCount(refStart, refEnd));
    // guess how many coupon periods per year:
    auto couponsPerYear = (Integer)std::lround(365.0 / referenceDayCount);
    // the above is good enough for annual or semi annual payments.
    return Real(dayCounter.dayCount(start, end)) / (referenceDayCount * couponsPerYear);
}

Time actualActualDaycountComputation(const Schedule& schedule, Date start, Date end)
{

    DayCounter daycounter = ActualActual(ActualActual::ISMA, schedule);
    Time yearFraction = 0.0;

    for (Size i = 1; i < schedule.size() - 1; i++)
    {
        Date referenceStart = schedule.date(i);
        Date referenceEnd = schedule.date(i + 1);
        if (start < referenceEnd && end > referenceStart)
        {
            yearFraction += ISMAYearFractionWithReferenceDates(
                daycounter, (start > referenceStart) ? start : referenceStart,
                (end < referenceEnd) ? end : referenceEnd, referenceStart, referenceEnd);
        };
    }
    return yearFraction;
}


BOOST_AUTO_TEST_CASE(testActualActual)
{

    BOOST_TEST_MESSAGE("Testing actual/actual day counters...");

    SingleCase testCases[] = {
        // first example
        SingleCase(ActualActual::ISDA, Date(1, November, 2003), Date(1, May, 2004), 0.497724380567),
        SingleCase(ActualActual::ISMA, Date(1, November, 2003), Date(1, May, 2004), Date(1, November, 2003),
                   Date(1, May, 2004), 0.500000000000),
        SingleCase(ActualActual::AFB, Date(1, November, 2003), Date(1, May, 2004), 0.497267759563),
        // short first calculation period (first period)
        SingleCase(ActualActual::ISDA, Date(1, February, 1999), Date(1, July, 1999), 0.410958904110),
        SingleCase(ActualActual::ISMA, Date(1, February, 1999), Date(1, July, 1999), Date(1, July, 1998),
                   Date(1, July, 1999), 0.410958904110),
        SingleCase(ActualActual::AFB, Date(1, February, 1999), Date(1, July, 1999), 0.410958904110),
        // short first calculation period (second period)
        SingleCase(ActualActual::ISDA, Date(1, July, 1999), Date(1, July, 2000), 1.001377348600),
        SingleCase(ActualActual::ISMA, Date(1, July, 1999), Date(1, July, 2000), Date(1, July, 1999),
                   Date(1, July, 2000), 1.000000000000),
        SingleCase(ActualActual::AFB, Date(1, July, 1999), Date(1, July, 2000), 1.000000000000),
        // long first calculation period (first period)
        SingleCase(ActualActual::ISDA, Date(15, August, 2002), Date(15, July, 2003), 0.915068493151),
        SingleCase(ActualActual::ISMA, Date(15, August, 2002), Date(15, July, 2003), Date(15, January, 2003),
                   Date(15, July, 2003), 0.915760869565),
        SingleCase(ActualActual::AFB, Date(15, August, 2002), Date(15, July, 2003), 0.915068493151),
        // long first calculation period (second period)
        /* Warning: the ISDA case is in disagreement with mktc1198.pdf */
        SingleCase(ActualActual::ISDA, Date(15, July, 2003), Date(15, January, 2004), 0.504004790778),
        SingleCase(ActualActual::ISMA, Date(15, July, 2003), Date(15, January, 2004), Date(15, July, 2003),
                   Date(15, January, 2004), 0.500000000000),
        SingleCase(ActualActual::AFB, Date(15, July, 2003), Date(15, January, 2004), 0.504109589041),
        // short final calculation period (penultimate period)
        SingleCase(ActualActual::ISDA, Date(30, July, 1999), Date(30, January, 2000), 0.503892506924),
        SingleCase(ActualActual::ISMA, Date(30, July, 1999), Date(30, January, 2000), Date(30, July, 1999),
                   Date(30, January, 2000), 0.500000000000),
        SingleCase(ActualActual::AFB, Date(30, July, 1999), Date(30, January, 2000), 0.504109589041),
        // short final calculation period (final period)
        SingleCase(ActualActual::ISDA, Date(30, January, 2000), Date(30, June, 2000), 0.415300546448),
        SingleCase(ActualActual::ISMA, Date(30, January, 2000), Date(30, June, 2000), Date(30, January, 2000),
                   Date(30, July, 2000), 0.417582417582),
        SingleCase(ActualActual::AFB, Date(30, January, 2000), Date(30, June, 2000), 0.41530054644)};

    Size n = sizeof(testCases) / sizeof(SingleCase);
    for (Size i = 0; i < n; i++)
    {
        ActualActual dayCounter(testCases[i].convention);
        Date d1 = testCases[i].start, d2 = testCases[i].end, rd1 = testCases[i].refStart, rd2 = testCases[i].refEnd;
        Time calculated = dayCounter.yearFraction(d1, d2, rd1, rd2);

        if (std::fabs(calculated - testCases[i].result) > 1.0e-10)
        {
            std::ostringstream period, refPeriod;
            period << "period: " << d1 << " to " << d2;
            if (testCases[i].convention == ActualActual::ISMA)
                refPeriod << "referencePeriod: " << rd1 << " to " << rd2;
            BOOST_ERROR(dayCounter.name() << ":\n"
                                          << period.str() << "\n"
                                          << refPeriod.str() << "\n"
                                          << std::setprecision(10) << "    calculated: " << calculated << "\n"
                                          << "    expected:   " << testCases[i].result);
        }
    }
}

BOOST_AUTO_TEST_CASE(testActualActualIsma)
{
    BOOST_TEST_MESSAGE("Testing actual/actual (ISMA) with odd last period...");

    bool isEndOfMonth(false);
    Frequency frequency(Semiannual);
    Date interestAccrualDate(30, Jan, 1999);
    Date maturityDate(30, Jun, 2000);
    Date firstCouponDate(30, Jul, 1999);
    Date penultimateCouponDate(30, Jan, 2000);
    Date d1(30, Jan, 2000);
    Date d2(30, Jun, 2000);

    double expected(152. / (182. * 2));

    Schedule schedule = MakeSchedule()
                            .from(interestAccrualDate)
                            .to(maturityDate)
                            .withFrequency(frequency)
                            .withFirstDate(firstCouponDate)
                            .withNextToLastDate(penultimateCouponDate)
                            .endOfMonth(isEndOfMonth);

    DayCounter dayCounter = ActualActual(ActualActual::ISMA, schedule);

    Real calculated(dayCounter.yearFraction(d1, d2));

    if (std::fabs(calculated - expected) > 1.0e-10)
    {
        std::ostringstream period;
        period << "period:                " << d1 << " to " << d2 << "\n"
               << "firstCouponDate:       " << firstCouponDate << "\n"
               << "penultimateCouponDate: " << penultimateCouponDate << "\n";
        BOOST_ERROR(dayCounter.name() << ":\n"
                                      << period.str() << std::setprecision(10) << "    calculated: " << calculated
                                      << "\n"
                                      << "    expected:   " << expected);
    }

    //////////////////////////////////

    isEndOfMonth = true;
    frequency = Quarterly;
    interestAccrualDate = Date(31, May, 1999);
    maturityDate = Date(30, Apr, 2000);
    firstCouponDate = Date(31, Aug, 1999);
    penultimateCouponDate = Date(30, Nov, 1999);
    d1 = Date(30, Nov, 1999);
    d2 = Date(30, Apr, 2000);

    expected = 91.0 / (91.0 * 4) + 61.0 / (92.0 * 4);

    schedule = MakeSchedule()
                   .from(interestAccrualDate)
                   .to(maturityDate)
                   .withFrequency(frequency)
                   .withFirstDate(firstCouponDate)
                   .withNextToLastDate(penultimateCouponDate)
                   .endOfMonth(isEndOfMonth);

    dayCounter = ActualActual(ActualActual::ISMA, schedule);

    calculated = dayCounter.yearFraction(d1, d2);

    if (std::fabs(calculated - expected) > 1.0e-10)
    {
        std::ostringstream period;
        period << "period:                " << d1 << " to " << d2 << "\n"
               << "firstCouponDate:       " << firstCouponDate << "\n"
               << "penultimateCouponDate: " << penultimateCouponDate << "\n";
        BOOST_ERROR(dayCounter.name() << ":\n"
                                      << period.str() << std::setprecision(10) << "    calculated: " << calculated
                                      << "\n"
                                      << "    expected:   " << expected);
    }


    //////////////////////////////////

    isEndOfMonth = false;
    frequency = Quarterly;
    interestAccrualDate = Date(31, May, 1999);
    maturityDate = Date(30, Apr, 2000);
    firstCouponDate = Date(31, Aug, 1999);
    penultimateCouponDate = Date(30, Nov, 1999);
    d1 = Date(30, Nov, 1999);
    d2 = Date(30, Apr, 2000);

    expected = 91.0 / (91.0 * 4) + 61.0 / (90.0 * 4);

    schedule = MakeSchedule()
                   .from(interestAccrualDate)
                   .to(maturityDate)
                   .withFrequency(frequency)
                   .withFirstDate(firstCouponDate)
                   .withNextToLastDate(penultimateCouponDate)
                   .endOfMonth(isEndOfMonth);

    dayCounter = ActualActual(ActualActual::ISMA, schedule);

    calculated = dayCounter.yearFraction(d1, d2);

    if (std::fabs(calculated - expected) > 1.0e-10)
    {
        std::ostringstream period;
        period << "period:                " << d1 << " to " << d2 << "\n"
               << "firstCouponDate:       " << firstCouponDate << "\n"
               << "penultimateCouponDate: " << penultimateCouponDate << "\n";
        BOOST_ERROR(dayCounter.name() << ":\n"
                                      << period.str() << std::setprecision(10) << "    calculated: " << calculated
                                      << "\n"
                                      << "    expected:   " << expected);
    }
}

BOOST_AUTO_TEST_CASE(testActualActualWithSemiannualSchedule)
{

    BOOST_TEST_MESSAGE("Testing actual/actual with schedule "
                       "for undefined semiannual reference periods...");

    Calendar calendar = UnitedStates(UnitedStates::GovernmentBond);
    Date fromDate = Date(10, January, 2017);
    Date firstCoupon = Date(31, August, 2017);
    Date quasiCoupon = Date(28, February, 2017);
    Date quasiCoupon2 = Date(31, August, 2016);

    Schedule schedule = MakeSchedule()
                            .from(fromDate)
                            .withFirstDate(firstCoupon)
                            .to(Date(28, February, 2026))
                            .withFrequency(Semiannual)
                            .withCalendar(calendar)
                            .withConvention(Unadjusted)
                            .backwards()
                            .endOfMonth(true);

    Date testDate = schedule.date(1);
    DayCounter dayCounter = ActualActual(ActualActual::ISMA, schedule);
    DayCounter dayCounterNoSchedule = ActualActual(ActualActual::ISMA);

    Date referencePeriodStart = schedule.date(1);
    Date referencePeriodEnd = schedule.date(2);

    // Test
    QL_ASSERT(dayCounter.yearFraction(referencePeriodStart, referencePeriodStart) == 0.0, "This should be zero.");
    QL_ASSERT(dayCounterNoSchedule.yearFraction(referencePeriodStart, referencePeriodStart) == 0.0,
              "This should be zero");
    QL_ASSERT(dayCounterNoSchedule.yearFraction(referencePeriodStart, referencePeriodStart, referencePeriodStart,
                                                referencePeriodStart) == 0.0,
              "This should be zero");
    QL_ASSERT(dayCounter.yearFraction(referencePeriodStart, referencePeriodEnd) == 0.5,
              "This should be exact using schedule; " << referencePeriodStart << " to " << referencePeriodEnd
                                                      << "Should be 0.5");
    QL_ASSERT(dayCounterNoSchedule.yearFraction(referencePeriodStart, referencePeriodEnd, referencePeriodStart,
                                                referencePeriodEnd) == 0.5,
              "This should be exact for explicit reference "
              "periods with no schedule");


    while (testDate < referencePeriodEnd)
    {
        Time difference =
            dayCounter.yearFraction(testDate, referencePeriodEnd, referencePeriodStart, referencePeriodEnd) -
            dayCounter.yearFraction(testDate, referencePeriodEnd);
        if (std::fabs(difference) > 1.0e-10)
        {
            BOOST_ERROR("Failed to correctly use the schedule "
                        "to find the reference period for Act/Act");
        };
        testDate = calendar.advance(testDate, 1, Days);
    }

    // Test long first coupon
    Real calculatedYearFraction = dayCounter.yearFraction(fromDate, firstCoupon);
    Real expectedYearFraction =
        0.5 + ((Real)dayCounter.dayCount(fromDate, quasiCoupon)) / (2 * dayCounter.dayCount(quasiCoupon2, quasiCoupon));

    QL_ASSERT(std::fabs(calculatedYearFraction - expectedYearFraction) < 1.0e-10,
              "Failed to compute the expected year fraction "
              "\n expected:   "
                  << expectedYearFraction << "\n calculated: " << calculatedYearFraction);

    // test multiple periods

    schedule = MakeSchedule()
                   .from(Date(10, January, 2017))
                   .withFirstDate(Date(31, August, 2017))
                   .to(Date(28, February, 2026))
                   .withFrequency(Semiannual)
                   .withCalendar(calendar)
                   .withConvention(Unadjusted)
                   .backwards()
                   .endOfMonth(false);

    Date periodStartDate = schedule.date(1);
    Date periodEndDate = schedule.date(2);

    dayCounter = ActualActual(ActualActual::ISMA, schedule);

    while (periodEndDate < schedule.date(schedule.size() - 2))
    {
        Time expected = actualActualDaycountComputation(schedule, periodStartDate, periodEndDate);
        Time calculated = dayCounter.yearFraction(periodStartDate, periodEndDate);

        if (std::fabs(expected - calculated) > 1e-8)
        {
            BOOST_ERROR("Failed to compute the correct year fraction "
                        "given a schedule: "
                        << periodStartDate << " to " << periodEndDate << "\n expected: " << expected
                        << " calculated: " << calculated);
        }
        periodEndDate = calendar.advance(periodEndDate, 1, Days);
    }
}

BOOST_AUTO_TEST_CASE(testActualActualWithAnnualSchedule)
{

    BOOST_TEST_MESSAGE("Testing actual/actual with schedule "
                       "for undefined annual reference periods...");

    // Now do an annual schedule
    Calendar calendar = UnitedStates(UnitedStates::GovernmentBond);
    Schedule schedule = MakeSchedule()
                            .from(Date(10, January, 2017))
                            .withFirstDate(Date(31, August, 2017))
                            .to(Date(28, February, 2026))
                            .withFrequency(Annual)
                            .withCalendar(calendar)
                            .withConvention(Unadjusted)
                            .backwards()
                            .endOfMonth(false);

    Date referencePeriodStart = schedule.date(1);
    Date referencePeriodEnd = schedule.date(2);

    Date testDate = schedule.date(1);
    DayCounter dayCounter = ActualActual(ActualActual::ISMA, schedule);

    while (testDate < referencePeriodEnd)
    {
        Time difference = ISMAYearFractionWithReferenceDates(dayCounter, testDate, referencePeriodEnd,
                                                             referencePeriodStart, referencePeriodEnd) -
                          dayCounter.yearFraction(testDate, referencePeriodEnd);
        if (std::fabs(difference) > 1.0e-10)
        {
            BOOST_ERROR("Failed to correctly use the schedule "
                        "to find the reference period for Act/Act:\n"
                        << testDate << " to " << referencePeriodEnd << "\n Ref: " << referencePeriodStart << " to "
                        << referencePeriodEnd);
        }

        testDate = calendar.advance(testDate, 1, Days);
    }
}

BOOST_AUTO_TEST_CASE(testActualActualWithSchedule)
{

    BOOST_TEST_MESSAGE("Testing actual/actual day counter with schedule...");

    // long first coupon
    Date issueDateExpected = Date(17, January, 2017);
    Date firstCouponDateExpected = Date(31, August, 2017);

    Schedule schedule = MakeSchedule()
                            .from(issueDateExpected)
                            .withFirstDate(firstCouponDateExpected)
                            .to(Date(28, February, 2026))
                            .withFrequency(Semiannual)
                            .withCalendar(Canada())
                            .withConvention(Unadjusted)
                            .backwards()
                            .endOfMonth();

    Date issueDate = schedule.date(0);
    QL_REQUIRE(issueDate == issueDateExpected,
               "This is not the expected issue date " << issueDate << " expected " << issueDateExpected);
    Date firstCouponDate = schedule.date(1);
    QL_REQUIRE(firstCouponDate == firstCouponDateExpected, "This is not the expected first coupon date "
                                                               << firstCouponDate
                                                               << " expected: " << firstCouponDateExpected);

    // Make thw quasi coupon dates:
    Date quasiCouponDate2 = schedule.calendar().advance(firstCouponDate, -schedule.tenor(),
                                                        schedule.businessDayConvention(), schedule.endOfMonth());
    Date quasiCouponDate1 = schedule.calendar().advance(quasiCouponDate2, -schedule.tenor(),
                                                        schedule.businessDayConvention(), schedule.endOfMonth());

    Date quasiCouponDate1Expected = Date(31, August, 2016);
    Date quasiCouponDate2Expected = Date(28, February, 2017);

    QL_REQUIRE(quasiCouponDate2 == quasiCouponDate2Expected,
               "Expected " << quasiCouponDate2Expected << " as the later quasi coupon date but received "
                           << quasiCouponDate2);
    QL_REQUIRE(quasiCouponDate1 == quasiCouponDate1Expected,
               "Expected " << quasiCouponDate1Expected << " as the earlier quasi coupon date but received "
                           << quasiCouponDate1);

    DayCounter dayCounter = ActualActual(ActualActual::ISMA, schedule);

    // full coupon
    Time t_with_reference = dayCounter.yearFraction(issueDate, firstCouponDate, quasiCouponDate2, firstCouponDate);
    Time t_no_reference = dayCounter.yearFraction(issueDate, firstCouponDate);
    Time t_total = ISMAYearFractionWithReferenceDates(dayCounter, issueDate, quasiCouponDate2, quasiCouponDate1,
                                                      quasiCouponDate2) +
                   0.5;
    Time expected = 0.6160220994;


    if (std::fabs(t_total - expected) > 1.0e-10)
    {
        BOOST_ERROR("Failed to reproduce expected time:\n"
                    << std::setprecision(10) << "    calculated: " << t_total << "\n"
                    << "    expected:   " << expected);
    }
    if (std::fabs(t_with_reference - expected) > 1.0e-10)
    {
        BOOST_ERROR("Failed to reproduce expected time:\n"
                    << std::setprecision(10) << "    calculated: " << t_with_reference << "\n"
                    << "    expected:   " << expected);
    }
    if (std::fabs(t_no_reference - t_with_reference) > 1.0e-10)
    {
        BOOST_ERROR("Should produce the same time "
                    "whether or not references are present");
    }

    // settlement date in the first quasi-period
    Date settlementDate = Date(29, January, 2017);

    t_with_reference =
        ISMAYearFractionWithReferenceDates(dayCounter, issueDate, settlementDate, quasiCouponDate1, quasiCouponDate2);
    t_no_reference = dayCounter.yearFraction(issueDate, settlementDate);
    Time t_expected_first_qp = 0.03314917127071823; // 12.0/362
    if (std::fabs(t_with_reference - t_expected_first_qp) > 1.0e-10)
    {
        BOOST_ERROR("Failed to reproduce expected time:\n"
                    << std::setprecision(10) << "    calculated: " << t_no_reference << "\n"
                    << "    expected:   " << t_expected_first_qp);
    }
    if (std::fabs(t_no_reference - t_with_reference) > 1.0e-10)
    {
        BOOST_ERROR("Should produce the same time "
                    "whether or not references are present");
    }
    Time t2 = dayCounter.yearFraction(settlementDate, firstCouponDate);
    if (std::fabs(t_expected_first_qp + t2 - expected) > 1.0e-10)
    {
        BOOST_ERROR("Sum of quasiperiod2 split is not consistent");
    }

    // settlement date in the second quasi-period
    settlementDate = Date(29, July, 2017);

    /*T = dayCounter.yearFraction(issueDate,
                                settlementDate,
                                quasiCouponDate2,
                                firstCouponDate);
    t1 = dayCounter.yearFraction(issueDate,
                                 quasiCouponDate2,
                                 quasiCouponDate1,
                                 quasiCouponDate2);
    Time t2 = dayCounter.yearFraction(quasiCouponDate2,
                                      settlementDate,
                                      quasiCouponDate2,
                                      firstCouponDate);*/
    t_no_reference = dayCounter.yearFraction(issueDate, settlementDate);
    t_with_reference = ISMAYearFractionWithReferenceDates(dayCounter, issueDate, quasiCouponDate2, quasiCouponDate1,
                                                          quasiCouponDate2) +
                       ISMAYearFractionWithReferenceDates(dayCounter, quasiCouponDate2, settlementDate,
                                                          quasiCouponDate2, firstCouponDate);
    if (std::fabs(t_no_reference - t_with_reference) > 1.0e-10)
    {
        BOOST_ERROR("These two cases should be identical");
    };
    t2 = dayCounter.yearFraction(settlementDate, firstCouponDate);


    if (std::fabs(t_total - (t_no_reference + t2)) > 1.0e-10)
    {
        BOOST_ERROR("Failed to reproduce expected time:\n"
                    << std::setprecision(10) << "    calculated: " << t_total << "\n"
                    << "    expected:   " << t_no_reference + t2);
    }
}

BOOST_AUTO_TEST_CASE(testActualActualWithLongFirstAndIrregularLastCoupon)
{
    BOOST_TEST_MESSAGE("Testing actual/actual (ISMA) with a long first and an irregular last coupon...");

    // A semiannual schedule whose first coupon runs for seven months, ending in
    // a short or a long last coupon.  Each period's fraction depends only on its
    // own dates and those of its notional periods.
    const Date issue(15, January, 2020), firstCoupon(15, August, 2020), nextToLast(15, February, 2023);
    const Real longFirst = 1.0 / 2.0 + 31.0 / (2 * 184);
    const std::vector<std::pair<Date, Real>> lastCoupons = {
        // 89 days of the notional period up to 15 August 2023, which has 181
        {Date(15, May, 2023), 89.0 / (2 * 181)},
        // that whole period, and 61 days of the next, which has 184
        {Date(15, October, 2023), 1.0 / 2.0 + 61.0 / (2 * 184)}};

    for (const auto& [maturity, lastFraction] : lastCoupons)
    {
        const Schedule schedule(issue, maturity, 6 * Months, NullCalendar(), Unadjusted, Unadjusted,
                                DateGeneration::Forward, false, firstCoupon, nextToLast);
        const DayCounter dayCounter = ActualActual(ActualActual::ISMA, schedule);

        for (Size i = 0; i + 1 < schedule.size(); ++i)
        {
            const Real expected = i == 0 ? longFirst : i + 2 == schedule.size() ? lastFraction : 0.5;
            const Real calculated = dayCounter.yearFraction(schedule[i], schedule[i + 1]);
            if (std::fabs(calculated - expected) > 1.0e-12)
                BOOST_ERROR("wrong year fraction from " << schedule[i] << " to " << schedule[i + 1]
                                                        << " on a schedule ending on " << maturity
                                                        << std::setprecision(12) << "\n    calculated: " << calculated
                                                        << "\n    expected:   " << expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSimple)
{

    BOOST_TEST_MESSAGE("Testing simple day counter...");

    Period p[] = {Period(3, Months), Period(6, Months), Period(1, Years)};
    Time expected[] = {0.25, 0.5, 1.0};
    Size n = sizeof(p) / sizeof(Period);

    // 4 years should be enough
    Date first(1, January, 2002), last(31, December, 2005);
    DayCounter dayCounter = SimpleDayCounter();

    for (Date start = first; start <= last; start++)
    {
        for (Size i = 0; i < n; i++)
        {
            Date end = start + p[i];
            Time calculated = dayCounter.yearFraction(start, end);
            if (std::fabs(calculated - expected[i]) > 1.0e-12)
            {
                BOOST_ERROR("from " << start << " to " << end << ":\n"
                                    << std::setprecision(12) << "    calculated: " << calculated << "\n"
                                    << "    expected:   " << expected[i]);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testOne)
{

    BOOST_TEST_MESSAGE("Testing 1/1 day counter...");

    Period p[] = {Period(3, Months), Period(6, Months), Period(1, Years)};
    Time expected[] = {1.0, 1.0, 1.0};
    Size n = sizeof(p) / sizeof(Period);

    // 1 years should be enough
    Date first(1, January, 2004), last(31, December, 2004);
    DayCounter dayCounter = OneDayCounter();

    for (Date start = first; start <= last; start++)
    {
        for (Size i = 0; i < n; i++)
        {
            Date end = start + p[i];
            Time calculated = dayCounter.yearFraction(start, end);
            if (std::fabs(calculated - expected[i]) > 1.0e-12)
            {
                BOOST_ERROR("from " << start << " to " << end << ":\n"
                                    << std::setprecision(12) << "    calculated: " << calculated << "\n"
                                    << "    expected:   " << expected[i]);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testBusiness252)
{

    BOOST_TEST_MESSAGE("Testing business/252 day counter...");

    std::vector<Date> testDates = {Date(1, February, 2002),  Date(4, February, 2002),   Date(16, May, 2003),
                                   Date(17, December, 2003), Date(17, December, 2004),  Date(19, December, 2005),
                                   Date(2, January, 2006),   Date(13, March, 2006),     Date(15, May, 2006),
                                   Date(17, March, 2006),    Date(15, May, 2006),       Date(26, July, 2006),
                                   Date(28, June, 2007),     Date(16, September, 2009), Date(26, July, 2016)};

    Time expected[] = {0.0039682539683, 1.2738095238095, 0.6031746031746, 0.9960317460317,  1.0000000000000,
                       0.0396825396825, 0.1904761904762, 0.1666666666667, -0.1507936507937, 0.1507936507937,
                       0.2023809523810, 0.912698412698,  2.214285714286,  6.84126984127};

    DayCounter dayCounter1 = Business252(Brazil());

    Time calculated;

    for (Size i = 1; i < testDates.size(); i++)
    {
        calculated = dayCounter1.yearFraction(testDates[i - 1], testDates[i]);
        if (std::fabs(calculated - expected[i - 1]) > 1.0e-12)
        {
            BOOST_ERROR("from " << testDates[i - 1] << " to " << testDates[i] << ":\n"
                                << std::setprecision(14) << "    calculated: " << calculated << "\n"
                                << "    expected:   " << expected[i - 1]);
        }
    }

    DayCounter dayCounter2 = Business252();

    for (Size i = 1; i < testDates.size(); i++)
    {
        calculated = dayCounter2.yearFraction(testDates[i - 1], testDates[i]);
        if (std::fabs(calculated - expected[i - 1]) > 1.0e-12)
        {
            BOOST_ERROR("from " << testDates[i - 1] << " to " << testDates[i] << ":\n"
                                << std::setprecision(14) << "    calculated: " << calculated << "\n"
                                << "    expected:   " << expected[i - 1]);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBusiness252FromSeveralThreads)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter from several threads...");

    // Spans across months and across years, in years no other case reaches, so
    // that the threads are the ones filling both caches.  Each thread has its
    // own day counter, as threads pricing their own object graphs would.  Under
    // the tsan cell this catches caches the threads share (#87).
    std::vector<std::pair<Date, Date>> spans;
    for (Year y = 2100; y < 2110; ++y)
        for (Integer m = 1; m <= 12; ++m)
        {
            const Date start(15, Month(m), y);
            spans.emplace_back(start, start + 5 * Months);
            spans.emplace_back(start, start + 3 * Years);
        }

    constexpr Size nThreads = 8;
    std::vector<std::vector<Date::serial_type>> seen(nThreads);
    std::vector<std::thread> threads;
    for (Size i = 0; i < nThreads; ++i)
        threads.emplace_back(
            [&spans, &out = seen[i]]
            {
                const DayCounter dayCounter = Business252(Brazil());
                out.reserve(spans.size());
                for (const auto& [d1, d2] : spans)
                    out.push_back(dayCounter.dayCount(d1, d2));
            });
    for (auto& t : threads)
        t.join();

    // The calendar walks each span day by day, using neither cache.
    const Calendar calendar = Brazil();
    std::vector<Date::serial_type> expected;
    expected.reserve(spans.size());
    for (const auto& [d1, d2] : spans)
        expected.push_back(calendar.businessDaysBetween(d1, d2));
    for (Size i = 0; i < nThreads; ++i)
        BOOST_CHECK_MESSAGE(seen[i] == expected, "thread " << i << " got day counts that differ from the calendar's");
}

// Spans across months and across years, so that Business252 counts them
// from its figures for whole months and whole years, against the calendar's
// own walk, which uses neither.
void checkBusiness252AgainstCalendar(const Calendar& calendar, const std::string& when)
{
    const DayCounter dayCounter = Business252(calendar);
    const std::vector<std::pair<Date, Date>> spans = {{Date(15, January, 2030), Date(15, June, 2030)},
                                                      {Date(15, January, 2030), Date(15, June, 2032)},
                                                      {Date(3, November, 2029), Date(20, February, 2031)}};
    for (const auto& [d1, d2] : spans)
    {
        const Date::serial_type counted = dayCounter.dayCount(d1, d2);
        const Date::serial_type walked = calendar.businessDaysBetween(d1, d2);
        BOOST_CHECK_MESSAGE(counted == walked, calendar.name() << ", " << when << ", from " << d1 << " to " << d2
                                                               << ": Business252 counts " << counted
                                                               << " business days, the calendar " << walked);
    }
}

BOOST_AUTO_TEST_CASE(testBusiness252AfterHolidayChanges)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter after a calendar's holidays change...");

    // Each change comes after the figures for the months and years it
    // touches have been filled (#122).  15 March 2030 and 14 March 2031 are
    // Fridays, 16 March 2030 a Saturday; 2031 is a whole year inside the
    // second span.
    BespokeCalendar calendar("Business252 holiday changes");
    calendar.addWeekend(Saturday);
    calendar.addWeekend(Sunday);
    checkBusiness252AgainstCalendar(calendar, "before any change");
    calendar.addHoliday(Date(15, March, 2030));
    checkBusiness252AgainstCalendar(calendar, "after adding a holiday");
    calendar.addHoliday(Date(14, March, 2031));
    checkBusiness252AgainstCalendar(calendar, "after adding a holiday in a whole year");
    calendar.removeHoliday(Date(15, March, 2030));
    checkBusiness252AgainstCalendar(calendar, "after removing an added holiday");
    calendar.removeHoliday(Date(16, March, 2030));
    checkBusiness252AgainstCalendar(calendar, "after removing a weekend day");
    calendar.resetAddedAndRemovedHolidays();
    checkBusiness252AgainstCalendar(calendar, "after resetting its holidays");
    calendar.addWeekend(Friday);
    checkBusiness252AgainstCalendar(calendar, "after adding a weekend day");
}

BOOST_AUTO_TEST_CASE(testBusiness252WithCalendarsOfTheSameName)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter with two calendars of the same name...");

    // Both are set up before either is used, so nothing changes once the
    // figures are filled, and only the calendars' identities tell their
    // figures apart (#122, which measured 113 business days against 130
    // from 15 January to 15 June 2030 on the second).
    BespokeCalendar fiveDays("Business252 same name"), sixDays("Business252 same name");
    fiveDays.addWeekend(Saturday);
    fiveDays.addWeekend(Sunday);
    sixDays.addWeekend(Sunday);
    checkBusiness252AgainstCalendar(fiveDays, "five-day week");
    checkBusiness252AgainstCalendar(sixDays, "six-day week");
    checkBusiness252AgainstCalendar(fiveDays, "five-day week, again");
}

BOOST_AUTO_TEST_CASE(testBusiness252AfterJointCalendarComponentsChange)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter after a joint calendar's components change...");

    // The joint calendar's own holidays don't change; its components' do,
    // and each change is a holiday of the joint calendar.
    BespokeCalendar fiveDays("Business252 component, five days"), sixDays("Business252 component, six days");
    fiveDays.addWeekend(Saturday);
    fiveDays.addWeekend(Sunday);
    sixDays.addWeekend(Sunday);
    const JointCalendar joint(fiveDays, sixDays);
    checkBusiness252AgainstCalendar(joint, "before any change");
    sixDays.addHoliday(Date(15, March, 2030));
    checkBusiness252AgainstCalendar(joint, "after a component's holiday");
    fiveDays.addHoliday(Date(14, March, 2031));
    checkBusiness252AgainstCalendar(joint, "after a component's holiday in a whole year");
    sixDays.addWeekend(Monday);
    checkBusiness252AgainstCalendar(joint, "after a component's weekend day");
}

BOOST_AUTO_TEST_CASE(testBusiness252WithShortLivedCalendars)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter with short-lived calendars...");

    // Each pass builds a joint calendar, and day counters on it, and drops
    // them.  The joint calendars share a name, their business days alternate,
    // and building them changes no calendar's holidays.  So their figures
    // are told apart only by the calendars' identities, and a calendar
    // dropped while its figures are kept mustn't lend its address to the
    // next (#122).  The passes outnumber the calendars kept per thread.
    BespokeCalendar fiveDays("Business252 short-lived"), sixDays("Business252 short-lived"),
        noWeekend("Business252 short-lived, no weekend");
    fiveDays.addWeekend(Saturday);
    fiveDays.addWeekend(Sunday);
    sixDays.addWeekend(Sunday);
    for (Size i = 0; i < 20; ++i)
    {
        const JointCalendar joint(i % 2 == 0 ? fiveDays : sixDays, noWeekend);
        checkBusiness252AgainstCalendar(joint, "pass " + std::to_string(i));
    }
}

// A five-day week whose implementation counts the dates it's asked about,
// and which can take Fridays off: a change its key doesn't show, so it
// records it through detail::recordCalendarChange.
class Business252CountingCalendar : public Calendar
{
    class Impl final : public Calendar::WesternImpl
    {
      public:
        mutable Size asked = 0;
        bool fridaysOff = false;
        std::string name() const override { return "Business252 counting"; }
        bool isBusinessDay(const Date& d) const override
        {
            ++asked;
            return !isWeekend(d.weekday()) && !(fridaysOff && d.weekday() == Friday);
        }
    };
    ext::shared_ptr<Impl> counting_;

  public:
    Business252CountingCalendar() : counting_(ext::make_shared<Impl>()) { impl_ = counting_; }
    Size asked() const { return counting_->asked; }
    void takeFridaysOff()
    {
        counting_->fridaysOff = true;
        QuantLib::detail::recordCalendarChange();
    }
};

BOOST_AUTO_TEST_CASE(testBusiness252WithCalendarsBuiltAlike)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter with joint calendars built alike...");

    // A joint calendar built for each count, as for each instrument, on the
    // same calendars: their business days are the same, so after the first
    // count they share its figures for whole months and years, and a count
    // asks only about the 17 days from 15 to 31 January 2030 and the 14 from
    // 1 to 14 June 2032 (#226).  Before, each filled its own figures, asking
    // about every day of the span.  The passes outnumber the calendars kept
    // per thread.
    const Business252CountingCalendar counting;
    const Date d1(15, January, 2030), d2(15, June, 2032);
    for (Size i = 0; i < 12; ++i)
    {
        const JointCalendar joint(counting, UnitedStates(UnitedStates::Settlement));
        const Size before = counting.asked();
        const Date::serial_type counted = Business252(joint).dayCount(d1, d2);
        const Size asked = counting.asked() - before;
        const Date::serial_type walked = joint.businessDaysBetween(d1, d2);
        BOOST_CHECK_MESSAGE(counted == walked, "pass " << i << ": Business252 counts " << counted
                                                       << " business days, the calendar " << walked);
        if (i > 0)
            BOOST_CHECK_MESSAGE(asked == 31, "pass " << i << ": Business252 asked about " << asked
                                                     << " days, where the shared figures leave 31");
    }
}

BOOST_AUTO_TEST_CASE(testBusiness252WithEqualCalendarsOneOfWhichChanges)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter with equal calendars, one of which changes...");

    // Calendars with the same business days share figures (#226) until one
    // of them changes; from then on each counts its own.  After each change,
    // the unchanged calendar fills figures first, which the changed one must
    // not take.  15 March 2030 and 14 March 2031 are in whole months of the
    // spans, and 2031 in a whole year.  detail::recordCalendarChange() drops
    // this thread's figures first, so that the first calendar of each pair
    // makes the entry the other shares, and not a calendar an earlier case
    // left with the same key.
    QuantLib::detail::recordCalendarChange();
    BespokeCalendar first("Business252 equal, first"), second("Business252 equal, second");
    for (BespokeCalendar* c : {&first, &second})
    {
        c->addWeekend(Saturday);
        c->addWeekend(Sunday);
    }
    checkBusiness252AgainstCalendar(first, "first bespoke, before any change");
    checkBusiness252AgainstCalendar(second, "second bespoke, before any change");
    second.addHoliday(Date(15, March, 2030));
    checkBusiness252AgainstCalendar(first, "first bespoke, after a holiday on the second");
    checkBusiness252AgainstCalendar(second, "second bespoke, after a holiday on it");
    second.removeHoliday(Date(15, March, 2030));
    first.addWeekend(Friday);
    checkBusiness252AgainstCalendar(second, "second bespoke, after a weekend day on the first");
    checkBusiness252AgainstCalendar(first, "first bespoke, after a weekend day on it");

    NullCalendar firstNull, secondNull;
    checkBusiness252AgainstCalendar(firstNull, "first null, before any change");
    checkBusiness252AgainstCalendar(secondNull, "second null, before any change");
    secondNull.addHoliday(Date(14, March, 2031));
    checkBusiness252AgainstCalendar(firstNull, "first null, after a holiday on the second");
    checkBusiness252AgainstCalendar(secondNull, "second null, after a holiday on it");
    firstNull.addHoliday(Date(15, March, 2030));
    checkBusiness252AgainstCalendar(secondNull, "second null, after a holiday on each, on other dates");
    checkBusiness252AgainstCalendar(firstNull, "first null, after a holiday on each, on other dates");

    const UnitedStates settlement(UnitedStates::Settlement);
    JointCalendar firstJoint(settlement, NullCalendar()), secondJoint(settlement, NullCalendar());
    checkBusiness252AgainstCalendar(firstJoint, "first joint, before any change");
    checkBusiness252AgainstCalendar(secondJoint, "second joint, before any change");
    secondJoint.addHoliday(Date(15, March, 2030));
    checkBusiness252AgainstCalendar(firstJoint, "first joint, after a holiday on the second");
    checkBusiness252AgainstCalendar(secondJoint, "second joint, after a holiday on it");
}

BOOST_AUTO_TEST_CASE(testBusiness252AfterChangesToOtherCalendars)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter after changes to other calendars...");

    // A change to a calendar's holidays or weekend leaves in use the
    // figures of the calendars it doesn't change (#226): building a bespoke
    // calendar, which sets its weekend, and adding a holiday to another
    // calendar leave a count asking only about the 31 days of the span's
    // part-months.  A change keys don't show, recorded through
    // detail::recordCalendarChange, drops the figures, and the next count
    // asks about all 882 days of the span again.
    Business252CountingCalendar counting;
    const DayCounter dayCounter = Business252(counting);
    const Date d1(15, January, 2030), d2(15, June, 2032);
    auto check = [&](const std::string& when, Size expected)
    {
        const Size before = counting.asked();
        const Date::serial_type counted = dayCounter.dayCount(d1, d2);
        const Size asked = counting.asked() - before;
        const Date::serial_type walked = counting.businessDaysBetween(d1, d2);
        BOOST_CHECK_MESSAGE(counted == walked,
                            when << ": Business252 counts " << counted << " business days, the calendar " << walked);
        BOOST_CHECK_MESSAGE(asked == expected, when << ": Business252 asked about " << asked << " days, where "
                                                    << expected << " were due");
    };
    check("first count", 882);
    check("second count", 31);
    BespokeCalendar other("Business252 other");
    other.addWeekend(Sunday);
    check("after building a bespoke calendar", 31);
    NullCalendar another;
    another.addHoliday(Date(14, March, 2031));
    check("after a holiday on another calendar", 31);
    counting.takeFridaysOff();
    check("after a change keys don't show", 882);
    check("a count after that", 31);
}

// A calendar on another, with Wednesdays off too.  Its business days depend
// on the other calendar, so its implementation appends that calendar's key,
// as Calendar::Impl::appendBusinessDaysKey asks.
class Business252WednesdaysOffCalendar : public Calendar
{
    class Impl final : public Calendar::Impl
    {
        Calendar underlying_;

      public:
        explicit Impl(Calendar underlying) : underlying_(std::move(underlying)) {}
        std::string name() const override { return underlying_.name() + " with Wednesdays off"; }
        bool isWeekend(Weekday w) const override { return underlying_.isWeekend(w); }
        bool isBusinessDay(const Date& d) const override
        {
            return d.weekday() != Wednesday && underlying_.isBusinessDay(d);
        }
        void appendBusinessDaysKey(std::vector<std::uintptr_t>& key) const override
        {
            static const char tag = 0;
            key.push_back(reinterpret_cast<std::uintptr_t>(&tag));
            QuantLib::detail::appendBusinessDaysKey(underlying_, key);
        }
    };

  public:
    explicit Business252WednesdaysOffCalendar(Calendar underlying)
    {
        impl_ = ext::make_shared<Impl>(std::move(underlying));
    }
};

BOOST_AUTO_TEST_CASE(testBusiness252WithCalendarsOnAnotherThatChanges)
{
    BOOST_TEST_MESSAGE("Testing business/252 day counter with calendars on another that changes...");

    // Two calendars built alike on a third, whose implementations append
    // its key (#226), share figures, and follow the third calendar's
    // changes.  14 March 2031 is a Friday in a whole year of a span.
    BespokeCalendar underlying("Business252 underlying");
    underlying.addWeekend(Saturday);
    underlying.addWeekend(Sunday);
    const Business252WednesdaysOffCalendar first(underlying), second(underlying);
    checkBusiness252AgainstCalendar(first, "first, before any change");
    checkBusiness252AgainstCalendar(second, "second, before any change");
    underlying.addHoliday(Date(14, March, 2031));
    checkBusiness252AgainstCalendar(first, "first, after a holiday on the calendar it's on");
    checkBusiness252AgainstCalendar(second, "second, after a holiday on the calendar it's on");
    underlying.addWeekend(Friday);
    checkBusiness252AgainstCalendar(second, "second, after a weekend day on the calendar it's on");
    checkBusiness252AgainstCalendar(first, "first, after a weekend day on the calendar it's on");
}

BOOST_AUTO_TEST_CASE(testThirty365)
{

    BOOST_TEST_MESSAGE("Testing 30/365 day counter...");

    struct
    {
        Date startDate;
        Date endDate;
        BigInteger expected;
    } testCases[] = {{Date(17, June, 2011), Date(30, December, 2012), 553},
                     // month end to month end
                     {Date(31, March, 2025), Date(30, April, 2025), 30},
                     // month end to 6 month ends later
                     {Date(30, September, 2024), Date(31, March, 2025), 180},
                     // no accrual beyond the 30th
                     {Date(30, March, 2025), Date(31, March, 2025), 0}};

    DayCounter dayCounter = Thirty365();
    for (const auto& testCase : testCases)
    {
        Date d1 = testCase.startDate;
        Date d2 = testCase.endDate;
        BigInteger expectedDays = testCase.expected;

        BigInteger days = dayCounter.dayCount(d1, d2);
        if (days != expectedDays)
        {
            BOOST_FAIL("from " << d1 << " to " << d2 << ":\n"
                               << "    calculated: " << days << "\n"
                               << "    expected:   " << expectedDays);
        }

        Time t = dayCounter.yearFraction(d1, d2);
        Time expectedTime = expectedDays / 365.0;
        if (std::fabs(t - expectedTime) > 1.0e-12)
        {
            BOOST_FAIL("from " << d1 << " to " << d2 << ":\n"
                               << std::setprecision(12) << "    calculated: " << t << "\n"
                               << "    expected:   " << expectedTime);
        }
    }
}

BOOST_AUTO_TEST_CASE(testThirty360_BondBasis)
{

    BOOST_TEST_MESSAGE("Testing 30/360 day counter (Bond Basis)...");

    // See https://www.isda.org/2008/12/22/30-360-day-count-conventions/

    DayCounter dayCounter = Thirty360(Thirty360::BondBasis);

    Thirty360Case data[] = {// Example 1: End dates do not involve the last day of February
                            {Date(20, August, 2006), Date(20, February, 2007), 180},
                            {Date(20, February, 2007), Date(20, August, 2007), 180},
                            {Date(20, August, 2007), Date(20, February, 2008), 180},
                            {Date(20, February, 2008), Date(20, August, 2008), 180},
                            {Date(20, August, 2008), Date(20, February, 2009), 180},
                            {Date(20, February, 2009), Date(20, August, 2009), 180},

                            // Example 2: End dates include some end-February dates
                            {Date(31, August, 2006), Date(28, February, 2007), 178},
                            {Date(28, February, 2007), Date(31, August, 2007), 183},
                            {Date(31, August, 2007), Date(29, February, 2008), 179},
                            {Date(29, February, 2008), Date(31, August, 2008), 182},
                            {Date(31, August, 2008), Date(28, February, 2009), 178},
                            {Date(28, February, 2009), Date(31, August, 2009), 183},

                            // Example 3: Miscellaneous calculations
                            {Date(31, January, 2006), Date(28, February, 2006), 28},
                            {Date(30, January, 2006), Date(28, February, 2006), 28},
                            {Date(28, February, 2006), Date(3, March, 2006), 5},
                            {Date(14, February, 2006), Date(28, February, 2006), 14},
                            {Date(30, September, 2006), Date(31, October, 2006), 30},
                            {Date(31, October, 2006), Date(28, November, 2006), 28},
                            {Date(31, August, 2007), Date(28, February, 2008), 178},
                            {Date(28, February, 2008), Date(28, August, 2008), 180},
                            {Date(28, February, 2008), Date(30, August, 2008), 182},
                            {Date(28, February, 2008), Date(31, August, 2008), 183},
                            {Date(26, February, 2007), Date(28, February, 2008), 362},
                            {Date(26, February, 2007), Date(29, February, 2008), 363},
                            {Date(29, February, 2008), Date(28, February, 2009), 359},
                            {Date(28, February, 2008), Date(30, March, 2008), 32},
                            {Date(28, February, 2008), Date(31, March, 2008), 33}};

    for (auto x : data)
    {
        Date::serial_type calculated = dayCounter.dayCount(x.start, x.end);
        if (calculated != x.expected)
        {
            BOOST_ERROR("from " << x.start << " to " << x.end << ":\n"
                                << "    calculated: " << calculated << "\n"
                                << "    expected:   " << x.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testThirty360_EurobondBasis)
{

    BOOST_TEST_MESSAGE("Testing 30/360 day counter (Eurobond Basis)...");

    // See https://www.isda.org/2008/12/22/30-360-day-count-conventions/

    DayCounter dayCounter = Thirty360(Thirty360::EurobondBasis);

    Thirty360Case data[] = {// Example 1: End dates do not involve the last day of February
                            {Date(20, August, 2006), Date(20, February, 2007), 180},
                            {Date(20, February, 2007), Date(20, August, 2007), 180},
                            {Date(20, August, 2007), Date(20, February, 2008), 180},
                            {Date(20, February, 2008), Date(20, August, 2008), 180},
                            {Date(20, August, 2008), Date(20, February, 2009), 180},
                            {Date(20, February, 2009), Date(20, August, 2009), 180},

                            // Example 2: End dates include some end-February dates
                            {Date(28, February, 2006), Date(31, August, 2006), 182},
                            {Date(31, August, 2006), Date(28, February, 2007), 178},
                            {Date(28, February, 2007), Date(31, August, 2007), 182},
                            {Date(31, August, 2007), Date(29, February, 2008), 179},
                            {Date(29, February, 2008), Date(31, August, 2008), 181},
                            {Date(31, August, 2008), Date(28, Feb, 2009), 178},
                            {Date(28, February, 2009), Date(31, August, 2009), 182},
                            {Date(31, August, 2009), Date(28, Feb, 2010), 178},
                            {Date(28, February, 2010), Date(31, August, 2010), 182},
                            {Date(31, August, 2010), Date(28, Feb, 2011), 178},
                            {Date(28, February, 2011), Date(31, August, 2011), 182},
                            {Date(31, August, 2011), Date(29, Feb, 2012), 179},

                            // Example 3: Miscellaneous calculations
                            {Date(31, January, 2006), Date(28, February, 2006), 28},
                            {Date(30, January, 2006), Date(28, February, 2006), 28},
                            {Date(28, February, 2006), Date(3, March, 2006), 5},
                            {Date(14, February, 2006), Date(28, February, 2006), 14},
                            {Date(30, September, 2006), Date(31, October, 2006), 30},
                            {Date(31, October, 2006), Date(28, November, 2006), 28},
                            {Date(31, August, 2007), Date(28, February, 2008), 178},
                            {Date(28, February, 2008), Date(28, August, 2008), 180},
                            {Date(28, February, 2008), Date(30, August, 2008), 182},
                            {Date(28, February, 2008), Date(31, August, 2008), 182},
                            {Date(26, February, 2007), Date(28, February, 2008), 362},
                            {Date(26, February, 2007), Date(29, February, 2008), 363},
                            {Date(29, February, 2008), Date(28, February, 2009), 359},
                            {Date(28, February, 2008), Date(30, March, 2008), 32},
                            {Date(28, February, 2008), Date(31, March, 2008), 32}};

    for (auto x : data)
    {
        Date::serial_type calculated = dayCounter.dayCount(x.start, x.end);
        if (calculated != x.expected)
        {
            BOOST_ERROR("from " << x.start << " to " << x.end << ":\n"
                                << "    calculated: " << calculated << "\n"
                                << "    expected:   " << x.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testThirty360_USA)
{

    BOOST_TEST_MESSAGE("Testing 30/360 day counter (USA)...");

    // See https://en.wikipedia.org/wiki/Day_count_convention#30/360_US

    DayCounter dayCounter = Thirty360(Thirty360::USA);

    Thirty360Case data[] = {// Example 1: End dates do not involve the last day of February
                            {Date(20, August, 2006), Date(20, February, 2007), 180},
                            {Date(20, February, 2007), Date(20, August, 2007), 180},
                            {Date(20, August, 2007), Date(20, February, 2008), 180},
                            {Date(20, February, 2008), Date(20, August, 2008), 180},
                            {Date(20, August, 2008), Date(20, February, 2009), 180},
                            {Date(20, February, 2009), Date(20, August, 2009), 180},

                            // Example 2: End dates include some end-February dates
                            {Date(31, August, 2006), Date(28, February, 2007), 178},
                            {Date(28, February, 2007), Date(31, August, 2007), 180},
                            {Date(31, August, 2007), Date(29, February, 2008), 179},
                            {Date(29, February, 2008), Date(31, August, 2008), 180},
                            {Date(31, August, 2008), Date(28, February, 2009), 178},
                            {Date(28, February, 2009), Date(31, August, 2009), 180},

                            // Example 3: Miscellaneous calculations
                            {Date(31, January, 2006), Date(28, February, 2006), 28},
                            {Date(30, January, 2006), Date(28, February, 2006), 28},
                            {Date(28, February, 2006), Date(3, March, 2006), 3},
                            {Date(14, February, 2006), Date(28, February, 2006), 14},
                            {Date(30, September, 2006), Date(31, October, 2006), 30},
                            {Date(31, October, 2006), Date(28, November, 2006), 28},
                            {Date(31, August, 2007), Date(28, February, 2008), 178},
                            {Date(28, February, 2008), Date(28, August, 2008), 180},
                            {Date(28, February, 2008), Date(30, August, 2008), 182},
                            {Date(28, February, 2008), Date(31, August, 2008), 183},
                            {Date(26, February, 2007), Date(28, February, 2008), 362},
                            {Date(26, February, 2007), Date(29, February, 2008), 363},
                            {Date(29, February, 2008), Date(28, February, 2009), 360},
                            {Date(28, February, 2008), Date(30, March, 2008), 32},
                            {Date(28, February, 2008), Date(31, March, 2008), 33}};

    for (auto x : data)
    {
        Date::serial_type calculated = dayCounter.dayCount(x.start, x.end);
        if (calculated != x.expected)
        {
            BOOST_ERROR("from " << x.start << " to " << x.end << ":\n"
                                << "    calculated: " << calculated << "\n"
                                << "    expected:   " << x.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testThirty360_ISDA)
{

    BOOST_TEST_MESSAGE("Testing 30/360 day counter (ISDA)...");

    // See https://www.isda.org/2008/12/22/30-360-day-count-conventions/

    Thirty360Case data1[] = {
        // Example 1: End dates do not involve the last day of February
        {Date(20, August, 2006), Date(20, February, 2007), 180},
        {Date(20, February, 2007), Date(20, August, 2007), 180},
        {Date(20, August, 2007), Date(20, February, 2008), 180},
        {Date(20, February, 2008), Date(20, August, 2008), 180},
        {Date(20, August, 2008), Date(20, February, 2009), 180},
        {Date(20, February, 2009), Date(20, August, 2009), 180},
    };

    Date terminationDate = Date(20, August, 2009);
    Thirty360 dayCounter(Thirty360::ISDA, terminationDate);

    for (auto x : data1)
    {
        Date::serial_type calculated = dayCounter.dayCount(x.start, x.end);
        if (calculated != x.expected)
        {
            BOOST_ERROR("from " << x.start << " to " << x.end << ":\n"
                                << "    calculated: " << calculated << "\n"
                                << "    expected:   " << x.expected);
        }
    }

    Thirty360Case data2[] = {
        // Example 2: End dates include some end-February dates
        {Date(28, February, 2006), Date(31, August, 2006), 180},
        {Date(31, August, 2006), Date(28, February, 2007), 180},
        {Date(28, February, 2007), Date(31, August, 2007), 180},
        {Date(31, August, 2007), Date(29, February, 2008), 180},
        {Date(29, February, 2008), Date(31, August, 2008), 180},
        {Date(31, August, 2008), Date(28, February, 2009), 180},
        {Date(28, February, 2009), Date(31, August, 2009), 180},
        {Date(31, August, 2009), Date(28, February, 2010), 180},
        {Date(28, February, 2010), Date(31, August, 2010), 180},
        {Date(31, August, 2010), Date(28, February, 2011), 180},
        {Date(28, February, 2011), Date(31, August, 2011), 180},
        {Date(31, August, 2011), Date(29, February, 2012), 179},
    };

    terminationDate = Date(29, February, 2012);
    dayCounter = Thirty360(Thirty360::ISDA, terminationDate);

    for (auto x : data2)
    {
        Date::serial_type calculated = dayCounter.dayCount(x.start, x.end);
        if (calculated != x.expected)
        {
            BOOST_ERROR("from " << x.start << " to " << x.end << ":\n"
                                << "    calculated: " << calculated << "\n"
                                << "    expected:   " << x.expected);
        }
    }

    Thirty360Case data3[] = {// Example 3: Miscellaneous calculations
                             {Date(31, January, 2006), Date(28, February, 2006), 30},
                             {Date(30, January, 2006), Date(28, February, 2006), 30},
                             {Date(28, February, 2006), Date(3, March, 2006), 3},
                             {Date(14, February, 2006), Date(28, February, 2006), 16},
                             {Date(30, September, 2006), Date(31, October, 2006), 30},
                             {Date(31, October, 2006), Date(28, November, 2006), 28},
                             {Date(31, August, 2007), Date(28, February, 2008), 178},
                             {Date(28, February, 2008), Date(28, August, 2008), 180},
                             {Date(28, February, 2008), Date(30, August, 2008), 182},
                             {Date(28, February, 2008), Date(31, August, 2008), 182},
                             {Date(28, February, 2007), Date(28, February, 2008), 358},
                             {Date(28, February, 2007), Date(29, February, 2008), 359},
                             {Date(29, February, 2008), Date(28, February, 2009), 360},
                             {Date(29, February, 2008), Date(30, March, 2008), 30},
                             {Date(29, February, 2008), Date(31, March, 2008), 30}};

    terminationDate = Date(29, February, 2008);
    dayCounter = Thirty360(Thirty360::ISDA, terminationDate);

    for (auto x : data3)
    {
        Date::serial_type calculated = dayCounter.dayCount(x.start, x.end);
        if (calculated != x.expected)
        {
            BOOST_ERROR("from " << x.start << " to " << x.end << ":\n"
                                << "    calculated: " << calculated << "\n"
                                << "    expected:   " << x.expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testActual365_Canadian)
{

    BOOST_TEST_MESSAGE("Testing that Actual/365 (Canadian) throws when needed...");

    Actual365Fixed dayCounter(Actual365Fixed::Canadian);

    // no reference period
    BOOST_CHECK_THROW(dayCounter.yearFraction(Date(10, September, 2018), Date(10, September, 2019)), Error);

    // reference period shorter than a month
    BOOST_CHECK_THROW(dayCounter.yearFraction(Date(10, September, 2018), Date(12, September, 2018),
                                              Date(10, September, 2018), Date(15, September, 2018)),
                      Error);

    // reference period longer than a year
    BOOST_CHECK_THROW(dayCounter.yearFraction(Date(8, January, 2025), Date(8, January, 2027), Date(8, January, 2025),
                                              Date(8, January, 2027)),
                      Error);
}

#ifdef QL_HIGH_RESOLUTION_DATE
BOOST_AUTO_TEST_CASE(testIntraday)
{

    BOOST_TEST_MESSAGE("Testing intraday behavior of day counter...");

    const Date d1(12, February, 2015);
    const Date d2(14, February, 2015, 12, 34, 17, 1, 230298);

    const Time tol = 100 * QL_EPSILON;

    const DayCounter dayCounters[] = {ActualActual(ActualActual::ISDA), Actual365Fixed(), Actual360()};

    for (DayCounter dc : dayCounters)
    {
        const Time expected =
            ((12 * 60 + 34) * 60 + 17 + 0.231298) * dc.yearFraction(d1, d1 + 1) / 86400 + dc.yearFraction(d1, d1 + 2);

        BOOST_CHECK_MESSAGE(std::fabs(dc.yearFraction(d1, d2) - expected) < tol,
                            "can not reproduce result for day counter " << dc.name());

        BOOST_CHECK_MESSAGE(std::fabs(dc.yearFraction(d2, d1) + expected) < tol,
                            "can not reproduce result for day counter " << dc.name());
    }
}
#endif

BOOST_AUTO_TEST_CASE(testActualActualOutOfScheduleRange)
{
    BOOST_TEST_MESSAGE("Testing usage of actual/actual out of schedule...");

    Date today = Date(10, November, 2020);
    Date temp = Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    Date effectiveDate = Date(21, May, 2019);
    Date terminationDate = Date(21, May, 2029);
    Period tenor = Period(1, Years);
    Calendar calendar = China(China::Market::IB);
    BusinessDayConvention convention = Unadjusted;
    BusinessDayConvention terminationDateConvention = convention;
    DateGeneration::Rule rule = DateGeneration::Backward;
    bool endOfMonth = false;

    Schedule schedule = Schedule(effectiveDate, terminationDate, tenor, calendar, convention, terminationDateConvention,
                                 rule, endOfMonth);
    DayCounter dayCounter = ActualActual(ActualActual::Convention::Bond, schedule);
    bool raised = false;

    try
    {
        dayCounter.yearFraction(today, today + Period(9, Years));
    }
    catch (const std::exception&)
    {
        raised = true;
    }

    if (!raised)
    {
        BOOST_FAIL("Exception expected but did not happen!");
    }

    Settings::instance().evaluationDate() = temp;
}

BOOST_AUTO_TEST_CASE(testAct366)
{

    BOOST_TEST_MESSAGE("Testing Act/366 day counter...");

    std::vector<Date> testDates = {Date(1, February, 2002),  Date(4, February, 2002),   Date(16, May, 2003),
                                   Date(17, December, 2003), Date(17, December, 2004),  Date(19, December, 2005),
                                   Date(2, January, 2006),   Date(13, March, 2006),     Date(15, May, 2006),
                                   Date(17, March, 2006),    Date(15, May, 2006),       Date(26, July, 2006),
                                   Date(28, June, 2007),     Date(16, September, 2009), Date(26, July, 2016)};

    Time expected[] = {0.00819672131147541, 1.27322404371585,  0.587431693989071, 1.0000000000000,   1.00273224043716,
                       0.0382513661202186,  0.191256830601093, 0.172131147540984, -0.16120218579235, 0.16120218579235,
                       0.19672131147541,    0.920765027322404, 2.21584699453552,  6.84426229508197};

    DayCounter dayCounter = Actual366();

    Time calculated;

    for (Size i = 1; i < testDates.size(); i++)
    {
        calculated = dayCounter.yearFraction(testDates[i - 1], testDates[i]);
        if (std::fabs(calculated - expected[i - 1]) > 1.0e-12)
        {
            BOOST_ERROR("from " << testDates[i - 1] << " to " << testDates[i] << ":\n"
                                << std::setprecision(14) << "    calculated: " << calculated << "\n"
                                << "    expected:   " << expected[i - 1]);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAct36525)
{

    BOOST_TEST_MESSAGE("Testing Act/365.25 day counter...");

    std::vector<Date> testDates = {Date(1, February, 2002),  Date(4, February, 2002),   Date(16, May, 2003),
                                   Date(17, December, 2003), Date(17, December, 2004),  Date(19, December, 2005),
                                   Date(2, January, 2006),   Date(13, March, 2006),     Date(15, May, 2006),
                                   Date(17, March, 2006),    Date(15, May, 2006),       Date(26, July, 2006),
                                   Date(28, June, 2007),     Date(16, September, 2009), Date(26, July, 2016)};

    Time expected[] = {0.0082135523613963, 1.27583846680356,  0.588637919233402, 1.00205338809035,   1.00479123887748,
                       0.0383299110198494, 0.191649555099247, 0.172484599589322, -0.161533196440794, 0.161533196440794,
                       0.197125256673511,  0.922655715263518, 2.22039698836413,  6.85831622176591};

    DayCounter dayCounter = Actual36525();

    Time calculated;

    for (Size i = 1; i < testDates.size(); i++)
    {
        calculated = dayCounter.yearFraction(testDates[i - 1], testDates[i]);
        if (std::fabs(calculated - expected[i - 1]) > 1.0e-12)
        {
            BOOST_ERROR("from " << testDates[i - 1] << " to " << testDates[i] << ":\n"
                                << std::setprecision(14) << "    calculated: " << calculated << "\n"
                                << "    expected:   " << expected[i - 1]);
        }
    }
}

BOOST_AUTO_TEST_CASE(testActualConsistency)
{
    BOOST_TEST_MESSAGE("Testing consistency between different actual day-counters...");

    const std::vector<Date> todayDates = {Date(12, January, 2022)
#ifdef QL_HIGH_RESOLUTION_DATE
                                              ,
                                          Date(7, February, 2022, 11, 43, 12, 293, 32)
#endif
    };

    const std::vector<Date> testDates = {Date(1, February, 2023),
                                         Date(4, February, 2023),
                                         Date(16, May, 2024),
                                         Date(17, December, 2024),
                                         Date(17, December, 2025),
                                         Date(19, December, 2026),
                                         Date(2, January, 2027),
                                         Date(13, March, 2028),
                                         Date(15, May, 2028),
                                         Date(26, July, 2036)
#ifdef QL_HIGH_RESOLUTION_DATE
                                             ,
                                         Date(23, August, 2025, 18, 1, 22, 927, 832),
                                         Date(23, August, 2032, 2, 23, 22, 0, 636)
#endif
    };

    const DayCounter actual365 = Actual365Fixed();
    const DayCounter actual366 = Actual366();
    const DayCounter actual364 = Actual364();
    const DayCounter actual36525 = Actual36525();
    const DayCounter actual360 = Actual360();
    const DayCounter actual360incl = Actual360(true);

    for (const auto& today : todayDates)
        for (const auto& d : testDates)
        {
            const Time t365 = actual365.yearFraction(today, d);
            const Time t366 = actual366.yearFraction(today, d);
            const Time t364 = actual364.yearFraction(today, d);
            const Time t360 = actual360.yearFraction(today, d);
            const Time t360incl = actual360incl.yearFraction(today, d);
            const Time t36525 = actual36525.yearFraction(today, d);

            QL_CHECK_SMALL(t365 * 365 / 366.0 - t366, 1e-14);
            QL_CHECK_SMALL(t365 * 365 / 364.0 - t364, 1e-14);
            QL_CHECK_SMALL(t365 * 365 / 360.0 - t360, 1e-14);
            QL_CHECK_SMALL(t365 * 365 / 364.0 - t364, 1e-14);
            QL_CHECK_SMALL(t365 * 365 / 365.25 - t36525, 1e-14);
            QL_CHECK_SMALL(t365 * 365 / 360.0 - (t360incl * 360 - 1) / 360, 1e-14);
        }
}

BOOST_AUTO_TEST_CASE(testYearFraction2DateBulk)
{
    BOOST_TEST_MESSAGE("Testing bulk dates for YearFractionToDate ...");

    const auto dayCounters = std::vector<DayCounter>{Actual365Fixed(),
                                                     Actual365Fixed(Actual365Fixed::NoLeap),
                                                     Actual360(),
                                                     Actual360(true),
                                                     Actual36525(),
                                                     Actual36525(true),
                                                     Actual364(),
                                                     Actual366(),
                                                     Actual366(true),
                                                     ActualActual(ActualActual::ISDA),
                                                     ActualActual(ActualActual::ISMA),
                                                     ActualActual(ActualActual::Bond),
                                                     ActualActual(ActualActual::Historical),
                                                     ActualActual(ActualActual::Actual365),
                                                     ActualActual(ActualActual::AFB),
                                                     ActualActual(ActualActual::Euro),
                                                     Business252(),
                                                     Thirty360(Thirty360::USA),
                                                     Thirty360(Thirty360::BondBasis),
                                                     Thirty360(Thirty360::European),
                                                     Thirty360(Thirty360::EurobondBasis),
                                                     Thirty360(Thirty360::Italian),
                                                     Thirty360(Thirty360::German),
                                                     Thirty360(Thirty360::ISMA),
                                                     Thirty360(Thirty360::ISDA),
                                                     Thirty360(Thirty360::NASD),
                                                     Thirty365(),
                                                     SimpleDayCounter()};

    for (const auto& dc : dayCounters)
        for (Integer i = -360; i < 730; ++i)
        {
            const Date today = Date(1, January, 2020) + Period(i, Days);
            const Date target = today + Period(i, Days);

            const Time t = dc.yearFraction(today, target);
            const Date time2Date = yearFractionToDate(dc, today, t);
            const Time tNew = dc.yearFraction(today, time2Date);

            if (!close_enough(t, tNew))
            {
                BOOST_FAIL("\ntoday      : " << today << "\ntarget     : " << target << "\ninverse    : " << time2Date
                                             << "\ntime diff  : " << t - tNew << "\nday counter: " << dc.name());
            }
        }
}

BOOST_AUTO_TEST_CASE(testYearFraction2DateRounding)
{
    BOOST_TEST_MESSAGE("Testing YearFractionToDate rounding to closer date...");

    const std::vector<DayCounter> dayCounters = {Thirty360(Thirty360::USA), Actual360()};
    const Date d1(1, February, 2023), d2(17, February, 2124);

    for (const DayCounter& dc : dayCounters)
    {
        Time t = dc.yearFraction(d1, d2);
        for (Time offset = 0; offset < 1 + 1e-10; offset += 0.05)
        {
            const Date inv = yearFractionToDate(dc, d1, t + offset / 360);
            if (offset < 0.4999)
                BOOST_CHECK_EQUAL(inv, d2);
            else
                BOOST_CHECK_EQUAL(inv, d2 + Period(1, Days));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
