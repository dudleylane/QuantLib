/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2021 StatPro Italia srl

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
#include <ql/cashflows/blackovernightindexedcouponpricer.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/digitalcoupon.hpp>
#include <ql/cashflows/digitaliborcoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/overnightindexedcouponpricer.hpp>
#include <ql/currencies/america.hpp>
#include <ql/indexes/ibor/estr.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/ibor/sofr.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/settings.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <iomanip>
#include <map>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(OvernightIndexedCouponTests)

struct CommonVars
{
    Date today;
    Real notional = 10000.0;
    ext::shared_ptr<OvernightIndex> sofr;
    RelinkableHandle<YieldTermStructure> forecastCurve;

    ext::shared_ptr<OvernightIndexedCoupon> makeCoupon(Date startDate,
                                                       Date endDate,
                                                       Natural fixingDays = Null<Natural>(),
                                                       Natural lockoutDays = 0,
                                                       bool applyObservationShift = false,
                                                       bool telescopicValueDates = false,
                                                       RateAveraging::Type averaging = RateAveraging::Compound)
    {
        return ext::make_shared<OvernightIndexedCoupon>(endDate, notional, startDate, endDate, sofr, 1.0, 0.0, Date(),
                                                        Date(), DayCounter(), telescopicValueDates, averaging,
                                                        fixingDays, lockoutDays, applyObservationShift);
    }

    ext::shared_ptr<OvernightIndexedCoupon> makeSpreadedCoupon(Date startDate,
                                                               Date endDate,
                                                               Spread spread = 0.0001,
                                                               bool compoundSpreadDaily = true,
                                                               Natural fixingDays = Null<Natural>(),
                                                               Natural lockoutDays = 0,
                                                               bool applyObservationShift = false,
                                                               bool telescopicValueDates = false,
                                                               RateAveraging::Type averaging = RateAveraging::Compound)
    {
        return ext::make_shared<OvernightIndexedCoupon>(
            endDate, notional, startDate, endDate, sofr, 1.0, spread, Date(), Date(), DayCounter(),
            telescopicValueDates, averaging, fixingDays, lockoutDays, applyObservationShift, compoundSpreadDaily);
    }

    CommonVars(const Date& evaluationDate)
    {
        today = evaluationDate;

        Settings::instance().evaluationDate() = today;

        sofr = ext::make_shared<Sofr>(forecastCurve);

        std::vector<Date> pastDates = {
            Date(21, June, 2019),     Date(24, June, 2019),     Date(25, June, 2019),     Date(26, June, 2019),
            Date(27, June, 2019),     Date(28, June, 2019),     Date(1, July, 2019),      Date(2, July, 2019),
            Date(3, July, 2019),      Date(5, July, 2019),      Date(8, July, 2019),      Date(9, July, 2019),
            Date(10, July, 2019),     Date(11, July, 2019),     Date(12, July, 2019),     Date(15, July, 2019),
            Date(16, July, 2019),     Date(17, July, 2019),     Date(18, July, 2019),     Date(19, July, 2019),
            Date(22, July, 2019),     Date(23, July, 2019),     Date(24, July, 2019),     Date(25, July, 2019),
            Date(26, July, 2019),     Date(29, July, 2019),     Date(30, July, 2019),     Date(31, July, 2019),
            Date(1, August, 2019),    Date(2, August, 2019),    Date(5, August, 2019),

            Date(18, October, 2021),  Date(19, October, 2021),  Date(20, October, 2021),  Date(21, October, 2021),
            Date(22, October, 2021),  Date(25, October, 2021),  Date(26, October, 2021),  Date(27, October, 2021),
            Date(28, October, 2021),  Date(29, October, 2021),  Date(1, November, 2021),  Date(2, November, 2021),
            Date(3, November, 2021),  Date(4, November, 2021),  Date(5, November, 2021),  Date(8, November, 2021),
            Date(9, November, 2021),  Date(10, November, 2021), Date(12, November, 2021), Date(15, November, 2021),
            Date(16, November, 2021), Date(17, November, 2021), Date(18, November, 2021), Date(19, November, 2021),
            Date(22, November, 2021)};
        std::vector<Rate> pastRates = {
            0.0237, 0.0239, 0.0241, 0.0243, 0.0242, 0.025,  0.0242, 0.0251, 0.0256, 0.0259, 0.0248, 0.0245, 0.0246,
            0.0241, 0.0236, 0.0246, 0.0247, 0.0247, 0.0246, 0.0241, 0.024,  0.024,  0.0241, 0.0242, 0.0241, 0.024,
            0.0239, 0.0255, 0.0219, 0.0219, 0.0213,

            0.0008, 0.0009, 0.0008, 0.0010, 0.0012, 0.0011, 0.0013, 0.0012, 0.0012, 0.0008, 0.0009, 0.0010, 0.0011,
            0.0014, 0.0013, 0.0011, 0.0009, 0.0008, 0.0007, 0.0008, 0.0008, 0.0007, 0.0009, 0.0010, 0.0009};

        sofr->addFixings(pastDates.begin(), pastDates.end(), pastRates.begin());
    }

    CommonVars() : CommonVars(Date(23, November, 2021)) {}
};

struct BlackONPricerVars
{
    Date today;
    Real notional = 1000000.0;
    RelinkableHandle<YieldTermStructure> forecastCurve;
    RelinkableHandle<OptionletVolatilityStructure> vol;
    ext::shared_ptr<OvernightIndex> sofr;
    DayCounter dc;

    BlackONPricerVars(const Date& evalDate = Date(1, July, 2025))
    {
        today = evalDate;
        dc = Actual360();
        Settings::instance().evaluationDate() = today;
        auto optionletVol = makeQuoteHandle(0.1);

        // Flat forward curve
        forecastCurve.linkTo(flatRate(today, 0.04, dc));
        sofr = ext::make_shared<Sofr>(forecastCurve);

        // Flat volatility
        vol.linkTo(ext::make_shared<ConstantOptionletVolatility>(today, TARGET(), Following, optionletVol, dc));
    }

    ext::shared_ptr<OvernightIndexedCoupon>
    makeBaseCoupon(Date start, Date end, RateAveraging::Type avgMethod = RateAveraging::Compound)
    {
        auto onCoupon =
            ext::make_shared<OvernightIndexedCoupon>(end, notional, start, end, sofr, 1.0, 0.0, Date(), Date(), dc,
                                                     false, avgMethod, Null<Natural>(), 0, false, false);

        if (avgMethod == RateAveraging::Compound)
            onCoupon->setPricer(ext::make_shared<CompoundingOvernightIndexedCouponPricer>());
        else
            onCoupon->setPricer(ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>());

        return onCoupon;
    }

    ext::shared_ptr<CappedFlooredOvernightIndexedCoupon>
    makeCoupon(Date start,
               Date end,
               Rate cap = Null<Rate>(),
               Rate floor = Null<Rate>(),
               RateAveraging::Type avgMethod = RateAveraging::Compound)
    {
        auto onCoupon = makeBaseCoupon(start, end, avgMethod);

        return ext::make_shared<CappedFlooredOvernightIndexedCoupon>(onCoupon, cap, floor);
    }
};


struct CommonVarsONLeg
{
    Date today;
    Real notional = 1000000.0;
    ext::shared_ptr<OvernightIndex> sofr;
    RelinkableHandle<YieldTermStructure> forecastCurve;
    Schedule legSchedule;
    DayCounter dc;
    RelinkableHandle<OptionletVolatilityStructure> rateVolTS;

    ext::shared_ptr<OptionletVolatilityStructure> returnRateVolTS()
    {
        auto optionletVol = makeQuoteHandle(0.05);
        return ext::make_shared<ConstantOptionletVolatility>(today, TARGET(), Following, optionletVol, dc);
    }

    Leg makeLeg(Natural fixingDays = Null<Natural>(),
                Natural lockoutDays = 0,
                bool applyObservationShift = false,
                bool telescopicValueDates = false,
                RateAveraging::Type averaging = RateAveraging::Compound,
                const std::vector<Real>& gearings = std::vector<Real>(),
                const std::vector<Spread>& spreads = std::vector<Spread>(),
                const std::vector<Rate>& caps = std::vector<Rate>(),
                const std::vector<Rate>& floors = std::vector<Rate>())
    {

        OvernightLeg leg(legSchedule, sofr);
        leg.withNotionals(notional)
            .withPaymentDayCounter(dc)
            .withAveragingMethod(averaging)
            .withLockoutDays(lockoutDays)
            .withObservationShift(applyObservationShift)
            .withTelescopicValueDates(telescopicValueDates);

        if (fixingDays != Null<Natural>())
        {
            leg.withLookbackDays(fixingDays);
        }

        if (!gearings.empty())
        {
            leg.withGearings(gearings);
        }

        if (!spreads.empty())
        {
            leg.withSpreads(spreads);
        }

        if (!caps.empty())
        {
            leg.withCaps(caps);
        }

        if (!floors.empty())
        {
            leg.withFloors(floors);
        }

        if (!caps.empty() || !floors.empty())
        {
            rateVolTS.linkTo(returnRateVolTS());
            if (averaging == RateAveraging::Compound)
                leg.withCouponPricer(ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(rateVolTS));
            else
                leg.withCouponPricer(ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(rateVolTS));
        }

        return leg;
    }

    CommonVarsONLeg(const Date& evaluationDate)
    {
        today = evaluationDate;
        dc = Actual360();

        Settings::instance().evaluationDate() = today;

        sofr = ext::make_shared<Sofr>(forecastCurve);

        // Create a quarterly schedule for testing
        legSchedule = Schedule(Date(1, July, 2025), Date(1, July, 2026), Period(3, Months),
                               UnitedStates(UnitedStates::GovernmentBond), ModifiedFollowing, ModifiedFollowing,
                               DateGeneration::Forward, false);

        std::vector<Date> pastDates = {
            Date(2, June, 2025),  Date(3, June, 2025),  Date(4, June, 2025),  Date(5, June, 2025),
            Date(6, June, 2025),  Date(9, June, 2025),  Date(10, June, 2025), Date(11, June, 2025),
            Date(12, June, 2025), Date(13, June, 2025), Date(16, June, 2025), Date(17, June, 2025),
            Date(18, June, 2025), Date(20, June, 2025), Date(23, June, 2025), Date(24, June, 2025),
            Date(25, June, 2025), Date(26, June, 2025), Date(27, June, 2025), Date(30, June, 2025),
            Date(1, July, 2025),  Date(2, July, 2025),  Date(3, July, 2025),  Date(7, July, 2025),
            Date(8, July, 2025),  Date(9, July, 2025),  Date(10, July, 2025), Date(11, July, 2025),
            Date(14, July, 2025), Date(15, July, 2025), Date(16, July, 2025), Date(17, July, 2025),
            Date(18, July, 2025), Date(21, July, 2025), Date(22, July, 2025), Date(23, July, 2025),
            Date(24, July, 2025), Date(25, July, 2025), Date(28, July, 2025), Date(29, July, 2025),
            Date(30, July, 2025), Date(31, July, 2025), Date(1, August, 2025)};

        std::vector<Rate> pastRates = {0.0435, 0.0432, 0.0428, 0.0429, 0.0429, 0.0429, 0.0428, 0.0428, 0.0428,
                                       0.0428, 0.0432, 0.0431, 0.0428, 0.0429, 0.0429, 0.0430, 0.0436, 0.0440,
                                       0.0439, 0.0445, 0.0444, 0.0440, 0.0435, 0.0433, 0.0434, 0.0432, 0.0431,
                                       0.0431, 0.0433, 0.0437, 0.0434, 0.0434, 0.0430, 0.0428, 0.0428, 0.0428,
                                       0.0430, 0.0436, 0.0436, 0.0436, 0.0432, 0.0439, 0.0434};

        sofr->addFixings(pastDates.begin(), pastDates.end(), pastRates.begin());
    }

    void setupForecastCurve()
    {
        std::vector<Date> curveDates = {today,
                                        Date(30, July, 2025),
                                        Date(29, August, 2025),
                                        Date(30, September, 2025),
                                        Date(30, December, 2025),
                                        Date(30, March, 2026),
                                        Date(30, June, 2026)};

        std::vector<Rate> zeroRates = {0.0434, 0.0436, 0.0431, 0.0413, 0.0390, 0.0370, 0.0348};

        ext::shared_ptr<InterpolatedZeroCurve<Cubic>> zeroCurve(
            new InterpolatedZeroCurve<Cubic>(curveDates, zeroRates, dc, UnitedStates(UnitedStates::SOFR)));

        zeroCurve->enableExtrapolation();

        forecastCurve.linkTo(zeroCurve);
    }

    CommonVarsONLeg() : CommonVarsONLeg(Date(1, June, 2025)) {}
};

#define CHECK_OIS_COUPON_RESULT(what, calculated, expected, tolerance)                                             \
    if (std::fabs(calculated - expected) > tolerance)                                                              \
    {                                                                                                              \
        BOOST_ERROR("Failed to reproduce " what ":" << "\n    expected:   " << std::setprecision(12) << expected   \
                                                    << "\n    calculated: " << std::setprecision(12) << calculated \
                                                    << "\n    error:      " << std::setprecision(12)               \
                                                    << std::fabs(calculated - expected));                          \
    }

BOOST_AUTO_TEST_CASE(testPastCouponRate)
{
    BOOST_TEST_MESSAGE("Testing rate for past overnight-indexed coupon...");

    CommonVars vars;

    // coupon entirely in the past

    auto pastCoupon = vars.makeCoupon(Date(18, October, 2021), Date(18, November, 2021));

    // expected values here and below come from manual calculations based on past dates and rates
    Rate expectedRate = 0.000987136104;
    Real expectedAmount = vars.notional * expectedRate * 31.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon rate", pastCoupon->rate(), expectedRate, 1e-12);
    CHECK_OIS_COUPON_RESULT("coupon amount", pastCoupon->amount(), expectedAmount, 1e-8);
}

BOOST_AUTO_TEST_CASE(testPastSpreadedCouponRate)
{
    BOOST_TEST_MESSAGE("Testing rate for past overnight-indexed coupon with compounded spread...");

    CommonVars vars;

    // coupon entirely in the past
    auto pastCoupon = vars.makeSpreadedCoupon(Date(18, October, 2021), Date(18, November, 2021), 0.0001);
    auto pastCouponCompoundingSpread =
        vars.makeSpreadedCoupon(Date(18, October, 2021), Date(18, November, 2021), 0.0001, false);

    // expected values here and below come from manual calculations based on past dates and rates
    Rate expectedRate = 0.0010871445057780704;
    Real expectedAmount = vars.notional * expectedRate * 31.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon rate", pastCoupon->rate(), expectedRate, 1e-12);
    CHECK_OIS_COUPON_RESULT("coupon amount", pastCoupon->amount(), expectedAmount, 1e-8);

    expectedRate = 0.0010871361040194164;
    CHECK_OIS_COUPON_RESULT("coupon rate", pastCouponCompoundingSpread->rate(), expectedRate, 1e-12);
}

BOOST_AUTO_TEST_CASE(testCurrentCouponRate)
{
    BOOST_TEST_MESSAGE("Testing rate for current overnight-indexed coupon...");

    CommonVars vars;

    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    // coupon partly in the past, today not fixed

    auto currentCoupon = vars.makeCoupon(Date(10, November, 2021), Date(10, December, 2021));

    Rate expectedRate = 0.000926701551;
    Real expectedAmount = vars.notional * expectedRate * 30.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon rate", currentCoupon->rate(), expectedRate, 1e-12);
    CHECK_OIS_COUPON_RESULT("coupon amount", currentCoupon->amount(), expectedAmount, 1e-8);

    // coupon partly in the past, today fixed

    vars.sofr->addFixing(Date(23, November, 2021), 0.0007);

    expectedRate = 0.000916700760;
    expectedAmount = vars.notional * expectedRate * 30.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon rate", currentCoupon->rate(), expectedRate, 1e-12);
    CHECK_OIS_COUPON_RESULT("coupon amount", currentCoupon->amount(), expectedAmount, 1e-8);
}

BOOST_AUTO_TEST_CASE(testCouponStartingTodayRate)
{
    BOOST_TEST_MESSAGE("Testing rate for overnight-indexed coupon starting today...");

    CommonVars vars;

    // A curve at zero forecasts every fixing at zero, so the rate tells whether
    // today's fixing was read.  It is the coupon's first, so the pricers read
    // the index's history although no earlier fixing is due (#89).
    vars.forecastCurve.linkTo(flatRate(0.0, Actual360()));
    const Date end(23, December, 2021);
    const auto averagings = {RateAveraging::Compound, RateAveraging::Simple};

    for (auto averaging : averagings)
    {
        auto coupon = vars.makeCoupon(vars.today, end, Null<Natural>(), 0, false, false, averaging);
        CHECK_OIS_COUPON_RESULT("coupon rate", coupon->rate(), 0.0, 1e-12);
    }

    const Rate todaysFixing = 0.0007;
    vars.sofr->addFixing(vars.today, todaysFixing);

    for (auto averaging : averagings)
    {
        auto coupon = vars.makeCoupon(vars.today, end, Null<Natural>(), 0, false, false, averaging);
        const auto& valueDates = coupon->valueDates();
        Rate expectedRate =
            todaysFixing * vars.sofr->dayCounter().yearFraction(valueDates[0], valueDates[1]) / coupon->accrualPeriod();
        CHECK_OIS_COUPON_RESULT("coupon rate", coupon->rate(), expectedRate, 1e-12);
    }
}

BOOST_AUTO_TEST_CASE(testFutureCouponRate)
{
    BOOST_TEST_MESSAGE("Testing rate for future overnight-indexed coupon...");

    CommonVars vars;

    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    // coupon entirely in the future

    auto futureCoupon = vars.makeCoupon(Date(10, December, 2021), Date(10, January, 2022));

    Rate expectedRate = 0.001000043057;
    Real expectedAmount = vars.notional * expectedRate * 31.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon rate", futureCoupon->rate(), expectedRate, 1e-12);
    CHECK_OIS_COUPON_RESULT("coupon amount", futureCoupon->amount(), expectedAmount, 1e-8);
}

BOOST_AUTO_TEST_CASE(testRateWhenTodayIsHoliday)
{
    BOOST_TEST_MESSAGE("Testing rate for overnight-indexed coupon when today is a holiday...");

    CommonVars vars;

    Settings::instance().evaluationDate() = Date(20, November, 2021);

    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    auto coupon = vars.makeCoupon(Date(10, November, 2021), Date(10, December, 2021));

    Rate expectedRate = 0.000930035180;
    Real expectedAmount = vars.notional * expectedRate * 30.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon rate", coupon->rate(), expectedRate, 1e-12);
    CHECK_OIS_COUPON_RESULT("coupon amount", coupon->amount(), expectedAmount, 1e-8);
}

BOOST_AUTO_TEST_CASE(testAccruedAmountInThePast)
{
    BOOST_TEST_MESSAGE("Testing accrued amount in the past for overnight-indexed coupon...");

    CommonVars vars;

    auto coupon = vars.makeCoupon(Date(18, October, 2021), Date(18, January, 2022));

    Real expectedAmount = vars.notional * 0.000987136104 * 31.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon amount", coupon->accruedAmount(Date(18, November, 2021)), expectedAmount, 1e-8);
}

BOOST_AUTO_TEST_CASE(testAccruedAmountSpanningToday)
{
    BOOST_TEST_MESSAGE("Testing accrued amount spanning today for current overnight-indexed coupon...");

    CommonVars vars;

    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    // coupon partly in the past, today not fixed

    auto coupon = vars.makeCoupon(Date(10, November, 2021), Date(10, January, 2022));

    Real expectedAmount = vars.notional * 0.000926701551 * 30.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon amount", coupon->accruedAmount(Date(10, December, 2021)), expectedAmount, 1e-8);

    // coupon partly in the past, today fixed

    vars.sofr->addFixing(Date(23, November, 2021), 0.0007);

    expectedAmount = vars.notional * 0.000916700760 * 30.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon amount", coupon->accruedAmount(Date(10, December, 2021)), expectedAmount, 1e-8);
}

BOOST_AUTO_TEST_CASE(testAccruedAmountInTheFuture)
{
    BOOST_TEST_MESSAGE("Testing accrued amount in the future for overnight-indexed coupon...");

    CommonVars vars;

    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    // coupon entirely in the future

    auto coupon = vars.makeCoupon(Date(10, December, 2021), Date(10, March, 2022));

    Date accrualDate = Date(10, January, 2022);
    Rate expectedRate = 0.001000043057;
    Real expectedAmount = vars.notional * expectedRate * 31.0 / 360;
    CHECK_OIS_COUPON_RESULT("coupon amount", coupon->accruedAmount(accrualDate), expectedAmount, 1e-8);
}

BOOST_AUTO_TEST_CASE(testAccruedAmountOnPastHoliday)
{
    BOOST_TEST_MESSAGE("Testing accrued amount on a past holiday for overnight-indexed coupon...");

    CommonVars vars;

    // coupon entirely in the past

    auto coupon = vars.makeCoupon(Date(18, October, 2021), Date(18, January, 2022));

    Date accrualDate = Date(13, November, 2021);
    Real expectedAmount = vars.notional * 0.000074724810;
    CHECK_OIS_COUPON_RESULT("coupon amount", coupon->accruedAmount(accrualDate), expectedAmount, 1e-8);
}
BOOST_AUTO_TEST_CASE(testAccruedAmountOnFutureHoliday)
{
    BOOST_TEST_MESSAGE("Testing accrued amount on a future holiday for overnight-indexed coupon...");

    CommonVars vars;

    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    // coupon entirely in the future

    auto coupon = vars.makeCoupon(Date(10, December, 2021), Date(10, March, 2022));

    Date accrualDate = Date(15, January, 2022);
    Real expectedAmount = vars.notional * 0.000100005012;
    CHECK_OIS_COUPON_RESULT("coupon amount", coupon->accruedAmount(accrualDate), expectedAmount, 1e-8);
}
BOOST_AUTO_TEST_CASE(testPastCouponRateWithLookback)
{
    BOOST_TEST_MESSAGE("Testing rate for past overnight-indexed coupon with lookback period...");

    CommonVars vars;

    // coupon entirely in the past with lookback period
    // this unit test replicates an example available on NY FED website:
    // https://www.newyorkfed.org/medialibrary/Microsites/arrc/files/2020/ARRC-BWLG-Examples-Other-Lookback-Options.xlsx

    auto pastCoupon = vars.makeCoupon(Date(1, July, 2019), Date(15, July, 2019), 5);

    // expected values here and below come from manual calculations based on past dates and rates
    Rate expectedRate = 0.024781644454;

    CHECK_OIS_COUPON_RESULT("coupon rate", pastCoupon->rate(), expectedRate, 1e-12);
}

BOOST_AUTO_TEST_CASE(testPastCouponRateWithLookbackAndObservationShift)
{
    BOOST_TEST_MESSAGE("Testing rate for past overnight-indexed coupon with lookback period and "
                       "observation shift...");

    CommonVars vars;

    // coupon entirely in the past with lookback period with observation shift
    // this unit test replicates an example available on NY FED website:
    // https://www.newyorkfed.org/medialibrary/Microsites/arrc/files/2020/ARRC-BWLG-Examples-Other-Lookback-Options.xlsx

    auto pastCoupon = vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), 5, 0, true);

    // expected values here and below come from manual calculations based on past dates and rates
    Rate expectedRate = 0.024603611707;

    CHECK_OIS_COUPON_RESULT("coupon rate", pastCoupon->rate(), expectedRate, 1e-12);
}

#define CHECK_OIS_COUPON_DATES(what, actual, expected)                                  \
    if (actual != expected)                                                             \
    {                                                                                   \
        BOOST_ERROR("Failed to reproduce " what ":" << "\n    expected:   " << expected \
                                                    << "\n    actual: " << actual);     \
    }

BOOST_AUTO_TEST_CASE(testPastCouponRateWithLockout)
{
    BOOST_TEST_MESSAGE("Testing rate for past overnight-indexed coupon with lockout...");

    CommonVars vars;

    auto couponWithLockout = vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), Null<Natural>(), 3);
    const std::vector<Date>& fixingDates = couponWithLockout->fixingDates();
    const Size n = fixingDates.size();

    Date expectedLockoutDate = Date(25, July, 2019);
    CHECK_OIS_COUPON_DATES("lockout date", fixingDates[n - 4], expectedLockoutDate);
    CHECK_OIS_COUPON_DATES("day T - 2 fixing", fixingDates[n - 3], expectedLockoutDate);
    CHECK_OIS_COUPON_DATES("day T - 1 fixing", fixingDates[n - 2], expectedLockoutDate);
    CHECK_OIS_COUPON_DATES("day T fixing", fixingDates[n - 1], expectedLockoutDate);
}

BOOST_AUTO_TEST_CASE(testPastCouponRateWithLookbackObservationShiftAndLockout)
{
    BOOST_TEST_MESSAGE("Testing rate for past overnight-indexed coupon with lookback period, "
                       "observation shift and lockout...");

    CommonVars vars;

    // coupon entirely in the past with lookback period with observation shift
    // and lockout this unit test replicates an example available on NY FED website:
    // https://www.newyorkfed.org/medialibrary/Microsites/arrc/files/2020/ARRC-BWLG-Examples-Other-Lookback-Options.xlsx

    auto pastCoupon = vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), 5, 3, true);

    // expected values here and below come from manual calculations based on past dates and rates
    Rate expectedRate = 0.024693783702;

    CHECK_OIS_COUPON_RESULT("coupon rate", pastCoupon->rate(), expectedRate, 1e-12);
}

BOOST_AUTO_TEST_CASE(testIncorrectNumberOfLockoutDays)
{
    BOOST_TEST_MESSAGE("Testing incorrect number of lockout days...");

    CommonVars vars;

    auto couponWithoutLockout = vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019));
    const Size numberOfFixings = couponWithoutLockout->fixingDates().size();

    // Lockout days equal to the number of daily fixings
    BOOST_CHECK_THROW(vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), Null<Natural>(), numberOfFixings),
                      Error);

    // Negative number of lockout days
    BOOST_CHECK_THROW(vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), Null<Natural>(), -1), Error);
}

BOOST_AUTO_TEST_CASE(testFutureCouponRateWithLookback)
{
    BOOST_TEST_MESSAGE("Testing rate for future overnight-indexed coupon with lookback period...");

    CommonVars vars(Date(12, March, 2019));

    vars.forecastCurve.linkTo(flatRate(0.0250, Actual360()));

    auto coupon8July = vars.makeCoupon(Date(1, July, 2019), Date(8, July, 2019), 5, 0, false);
    Rate expectedRate8July = 0.0250050849311315;

    CHECK_OIS_COUPON_RESULT("coupon rate", coupon8July->rate(), expectedRate8July, 1e-12);

    auto coupon15July = vars.makeCoupon(Date(1, July, 2019), Date(15, July, 2019), 5, 0, false);
    Rate expectedRate15July = 0.0250118464503275;

    CHECK_OIS_COUPON_RESULT("coupon rate", coupon15July->rate(), expectedRate15July, 1e-12);
}

BOOST_AUTO_TEST_CASE(testFutureCouponRateWithLookbackAndObservationShift)
{
    BOOST_TEST_MESSAGE("Testing rate for future overnight-indexed coupon with lookback period and "
                       "observation shift...");

    CommonVars vars(Date(12, March, 2019));

    vars.forecastCurve.linkTo(flatRate(0.0250, Actual360()));

    auto futureCoupon = vars.makeCoupon(Date(1, July, 2019), Date(8, July, 2019), 5, 0, true);

    // The discrepancies between the results with and without observation shift,
    // especially in the short term horizon, show that interest-period weighted
    // overnight cumulative compounded annualized rates are considered as an
    // alternative.
    Rate expectedRate = 0.0142876985964208;

    CHECK_OIS_COUPON_RESULT("coupon rate", futureCoupon->rate(), expectedRate, 1e-12);
}

BOOST_AUTO_TEST_CASE(testFutureCouponRateWithLookout)
{
    BOOST_TEST_MESSAGE("Testing rate for future overnight-indexed coupon with lockout...");

    CommonVars vars(Date(12, March, 2019));

    vars.forecastCurve.linkTo(flatRate(0.0250, Actual360()));

    auto coupon15July = vars.makeCoupon(Date(1, July, 2019), Date(15, July, 2019), Null<Natural>(), 2, false);

    // expected = 0.025011784374543
    Rate lockoutFixing = vars.sofr->fixing(Date(10, July, 2019));
    Rate expectedRate15July =
        (vars.forecastCurve->discount(Date(1, July, 2019)) / vars.forecastCurve->discount(Date(11, July, 2019)) *
             (1.0 + 1.0 / 360.0 * lockoutFixing) * (1.0 + 3.0 / 360.0 * lockoutFixing) -
         1.0) *
        360.0 / 14;

    CHECK_OIS_COUPON_RESULT("coupon rate", coupon15July->rate(), expectedRate15July, 1e-12);
}

BOOST_AUTO_TEST_CASE(testPartiallyAccruedAmountOfFutureCouponWithLookout)
{
    BOOST_TEST_MESSAGE("Testing partially accrued amount for future overnight-indexed coupon with lockout...");

    CommonVars vars(Date(12, March, 2019));

    vars.forecastCurve.linkTo(flatRate(0.0250, Actual360()));

    auto coupon15July = vars.makeCoupon(Date(1, July, 2019), Date(15, July, 2019), Null<Natural>(), 2, false);

    Rate lockoutFixing = vars.sofr->fixing(Date(10, July, 2019));
    Rate expectedRate15July =
        (vars.forecastCurve->discount(Date(1, July, 2019)) / vars.forecastCurve->discount(Date(11, July, 2019)) *
             (1.0 + 1.0 / 360.0 * lockoutFixing) * (1.0 + 2.0 / 360.0 * lockoutFixing) -
         1.0) *
        360.0 / 13;

    Real expectedAccruedAmount =
        coupon15July->nominal() * coupon15July->accruedPeriod(Date(14, July, 2019)) * expectedRate15July;

    CHECK_OIS_COUPON_RESULT("accrued amount", coupon15July->accruedAmount(Date(14, July, 2019)), expectedAccruedAmount,
                            1e-12);
}

BOOST_AUTO_TEST_CASE(testTelescopicFormulaWhenLookbackWithObservationShiftAndNoIndexFixingDelay)
{
    BOOST_TEST_MESSAGE("Testing telescopic formula when lookback with observation shift is applied "
                       "and the index has no fixing delay...");

    CommonVars vars(Date(12, March, 2019));

    vars.forecastCurve.linkTo(flatRate(0.0250, Actual360()));

    auto coupon15July = vars.makeCoupon(Date(1, July, 2019), Date(15, July, 2019), 3, 0, true);

    Rate actualRate = coupon15July->rate();

    auto coupon15JulyWithTelescopicDates = vars.makeCoupon(Date(1, July, 2019), Date(15, July, 2019), 3, 0, true, true);

    CHECK_OIS_COUPON_RESULT("telescopic value dates coupon rate", actualRate, coupon15JulyWithTelescopicDates->rate(),
                            1e-12);

    Rate expectedRateTelescopicSeries =
        (vars.forecastCurve->discount(Date(26, June, 2019)) / vars.forecastCurve->discount(Date(10, July, 2019)) -
         1.0) *
        360.0 / 14;

    CHECK_OIS_COUPON_RESULT("coupon rate using telescopic formula", actualRate, expectedRateTelescopicSeries, 1e-12);

    const auto& fixingDates = coupon15July->fixingDates();
    const auto& dts = coupon15July->dt();
    Size n = fixingDates.size();

    Rate expectedRateIterativeFormula = 1.0;
    for (Size i = 0; i < n; ++i)
    {
        expectedRateIterativeFormula *= (1.0 + dts[i] * coupon15July->index()->fixing(fixingDates[i]));
    }
    expectedRateIterativeFormula -= 1.0;
    expectedRateIterativeFormula /= coupon15July->accrualPeriod();

    CHECK_OIS_COUPON_RESULT("coupon rate using iterative formula", actualRate, expectedRateIterativeFormula, 1e-12);
}

BOOST_AUTO_TEST_CASE(testErrorWhenTelescopicValueDatesEnforcedWithLookback)
{
    BOOST_TEST_MESSAGE("Testing error when telescopic value dates enforced with lookback...");

    CommonVars vars;

    BOOST_CHECK_THROW(vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), 2, 0, false, true), Error);
}

BOOST_AUTO_TEST_CASE(testErrorWhenLookbackOrLockoutAppliedForSimpleAveraging)
{
    BOOST_TEST_MESSAGE("Testing error when lookback or lockout applied for simple averaging...");

    CommonVars vars;

    BOOST_CHECK_THROW(
        vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), 2, 0, false, false, RateAveraging::Simple), Error);

    BOOST_CHECK_THROW(vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), Null<Natural>(), 2, false, false,
                                      RateAveraging::Simple),
                      Error);

    BOOST_CHECK_THROW(vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), Null<Natural>(), 0, true, false,
                                      RateAveraging::Simple),
                      Error);
}

BOOST_AUTO_TEST_CASE(testEffectiveSpreadAndIndexFixingErrors)
{
    BOOST_TEST_MESSAGE("Testing the errors of an overnight coupon's effective spread and index fixing...");

    CommonVars vars;

    auto averaged = vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), Null<Natural>(), 0, false, false,
                                    RateAveraging::Simple);
    BOOST_CHECK_EXCEPTION(averaged->effectiveIndexFixing(), Error,
                          ExpectedErrorMessage("OvernightIndexedCoupon::effectiveIndexFixing(): only a compounded "
                                               "coupon has an effective index fixing"));

    // a compounded coupon, its spread compounded daily, on the averaging pricer
    auto compounded = vars.makeSpreadedCoupon(Date(1, July, 2019), Date(31, July, 2019));
    compounded->setPricer(ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>());
    BOOST_CHECK_EXCEPTION(compounded->effectiveIndexFixing(), Error,
                          ExpectedErrorMessage("OvernightIndexedCoupon::effectiveIndexFixing() needs a "
                                               "CompoundingOvernightIndexedCouponPricer"));
    BOOST_CHECK_EXCEPTION(compounded->effectiveSpread(), Error,
                          ExpectedErrorMessage("OvernightIndexedCoupon::effectiveSpread() needs a "
                                               "CompoundingOvernightIndexedCouponPricer"));
}

BOOST_AUTO_TEST_CASE(testBlackOvernightIndexedCouponPricerCapletFloorlet)
{
    BOOST_TEST_MESSAGE("Testing Black compounding overnight-indexed coupon pricer...");

    BlackONPricerVars vars;
    Date start = Date(1, July, 2035);
    Date end = Date(1, October, 2035);

    // Vanilla
    auto vanillaCoupon = vars.makeBaseCoupon(start, end);
    Rate expectedRate = vanillaCoupon->rate();

    auto pricer = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vars.vol);
    vanillaCoupon->setPricer(pricer);

    Rate rate = vanillaCoupon->rate();
    CHECK_OIS_COUPON_RESULT("Base Rate", rate, expectedRate, 1e-8);

    // Caplet
    Rate cap = 0.045;
    auto cappedCoupon = vars.makeCoupon(start, end, cap, Null<Rate>());
    cappedCoupon->setPricer(pricer);

    rate = cappedCoupon->rate();
    expectedRate = 0.036604717;
    BOOST_CHECK(rate <= cap + 1e-8); // Should not exceed cap
    CHECK_OIS_COUPON_RESULT("Capped Rate", rate, expectedRate, 1e-8);

    // Floorlet
    Rate floor = 0.035;
    auto flooredCoupon = vars.makeCoupon(start, end, Null<Rate>(), floor);
    flooredCoupon->setPricer(pricer);
    BOOST_CHECK(!flooredCoupon->isCalculated());

    rate = flooredCoupon->rate();
    expectedRate = 0.042502070;
    BOOST_CHECK(rate >= floor - 1e-8); // Should not be below floor
    CHECK_OIS_COUPON_RESULT("Floored Rate", rate, expectedRate, 1e-8);

    // Capped and Floored
    auto cappedFlooredCoupon = vars.makeCoupon(start, end, cap, floor);
    cappedFlooredCoupon->setPricer(pricer);

    rate = cappedFlooredCoupon->rate();
    expectedRate = 0.039340869;
    BOOST_CHECK(rate <= cap + 1e-8 && rate >= floor - 1e-8);
    CHECK_OIS_COUPON_RESULT("Capped and Floored Rate", rate, expectedRate, 1e-8);
}

BOOST_AUTO_TEST_CASE(testBlackAverageONIndexedCouponPricerCapletFloorlet)
{
    BOOST_TEST_MESSAGE("Testing Black averaging overnight-indexed coupon pricer...");

    BlackONPricerVars vars;
    Date start = Date(1, July, 2035);
    Date end = Date(1, October, 2035);

    // Vanilla
    auto vanillaCoupon = vars.makeBaseCoupon(start, end, RateAveraging::Simple);
    Rate expectedRate = vanillaCoupon->rate();

    auto pricer = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vars.vol);
    vanillaCoupon->setPricer(pricer);

    Rate rate = vanillaCoupon->rate();
    CHECK_OIS_COUPON_RESULT("Base Rate", rate, expectedRate, 1e-8);

    // Caplet
    Rate cap = 0.045;
    auto cappedCoupon = vars.makeCoupon(start, end, cap, Null<Rate>(), RateAveraging::Simple);
    cappedCoupon->setPricer(pricer);

    rate = cappedCoupon->rate();
    expectedRate = 0.036488300;
    BOOST_CHECK(rate <= cap + 1e-8);
    CHECK_OIS_COUPON_RESULT("Capped Rate", rate, expectedRate, 1e-8);

    // Floorlet
    Rate floor = 0.035;
    auto flooredCoupon = vars.makeCoupon(start, end, Null<Rate>(), floor, RateAveraging::Simple);
    flooredCoupon->setPricer(pricer);

    rate = flooredCoupon->rate();
    expectedRate = 0.042362746;
    BOOST_CHECK(rate >= floor - 1e-8);
    CHECK_OIS_COUPON_RESULT("Capped Rate", rate, expectedRate, 1e-8);

    // Capped and Floored
    auto cappedFlooredCoupon = vars.makeCoupon(start, end, cap, floor, RateAveraging::Simple);
    cappedFlooredCoupon->setPricer(pricer);

    rate = cappedFlooredCoupon->rate();
    expectedRate = 0.039281553;
    BOOST_CHECK(rate <= cap + 1e-8 && rate >= floor - 1e-8);
    CHECK_OIS_COUPON_RESULT("Capped and Floored Rate", rate, expectedRate, 1e-8);
}

BOOST_AUTO_TEST_CASE(testBlackONPricerConsistencyWithNoVol)
{
    BOOST_TEST_MESSAGE("Testing Black compounding pricer with zero volatility (should match vanilla pricer)...");

    BlackONPricerVars vars;
    auto optionletVol = makeQuoteHandle(0.0);
    vars.vol.linkTo(
        ext::make_shared<ConstantOptionletVolatility>(vars.today, TARGET(), Following, optionletVol, vars.dc));
    Date start = Date(1, July, 2035);
    Date end = Date(1, October, 2035);

    auto cappedFlooredCoupon = vars.makeCoupon(start, end, 0.045, 0.035);
    auto blackPricer = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vars.vol);
    cappedFlooredCoupon->setPricer(blackPricer);
    Rate blackRate = cappedFlooredCoupon->rate();

    // Compare with standard compounding pricer
    auto baseONCoupon = vars.makeBaseCoupon(start, end);
    baseONCoupon->setPricer(ext::make_shared<CompoundingOvernightIndexedCouponPricer>());
    Rate vanillaRate = baseONCoupon->rate();

    CHECK_OIS_COUPON_RESULT("Zero capped coupon rate", blackRate, vanillaRate, 1e-10);

    baseONCoupon->setPricer(blackPricer);
    vanillaRate = baseONCoupon->rate();
    CHECK_OIS_COUPON_RESULT("Zero capped coupon rate (same pricer)", blackRate, vanillaRate, 1e-10);
}

BOOST_AUTO_TEST_CASE(testBlackONAveragingPricerConsistencyWithNoVol)
{
    BOOST_TEST_MESSAGE("Testing Black averaging pricer with zero volatility (should match vanilla pricer)...");

    BlackONPricerVars vars;
    auto optionletVol = makeQuoteHandle(0.0);
    vars.vol.linkTo(
        ext::make_shared<ConstantOptionletVolatility>(vars.today, TARGET(), Following, optionletVol, vars.dc));
    Date start = Date(1, July, 2035);
    Date end = Date(1, October, 2035);

    auto cappedFlooredCoupon = vars.makeCoupon(start, end, 0.045, 0.035, RateAveraging::Simple);
    auto blackPricer = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vars.vol);
    cappedFlooredCoupon->setPricer(blackPricer);
    Rate blackRate = cappedFlooredCoupon->rate();

    // Compare with standard compounding pricer
    auto baseONCoupon = vars.makeBaseCoupon(start, end, RateAveraging::Simple);
    baseONCoupon->setPricer(ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>());
    Rate vanillaRate = baseONCoupon->rate();

    CHECK_OIS_COUPON_RESULT("Zero capped coupon rate", blackRate, vanillaRate, 1e-10);

    baseONCoupon->setPricer(blackPricer);
    vanillaRate = baseONCoupon->rate();
    CHECK_OIS_COUPON_RESULT("Zero capped coupon rate (same pricer)", blackRate, vanillaRate, 1e-10);
}

BOOST_AUTO_TEST_CASE(testOvernightLegBasicFunctionality)
{
    BOOST_TEST_MESSAGE("Testing basic functionality of overnight leg...");

    CommonVarsONLeg vars;
    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    Leg leg = vars.makeLeg();

    // Check that we have the expected number of coupons (monthly over 1 year = 12 coupons)
    BOOST_CHECK_EQUAL(leg.size(), 4);

    // Check that all cash flows are OvernightIndexedCoupons
    for (const auto& cf : leg)
    {
        auto oisCoupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(cf);
        BOOST_CHECK(oisCoupon != nullptr);
        if (oisCoupon)
        {
            BOOST_CHECK_EQUAL(oisCoupon->nominal(), vars.notional);
            BOOST_CHECK_EQUAL(oisCoupon->averagingMethod(), RateAveraging::Compound);
            BOOST_CHECK_EQUAL(oisCoupon->lockoutDays(), 0);
            BOOST_CHECK_EQUAL(oisCoupon->applyObservationShift(), false);
        }
    }
}

BOOST_AUTO_TEST_CASE(testOvernightLegWithLookback)
{
    BOOST_TEST_MESSAGE("Testing overnight leg construction with lookback days...");

    CommonVarsONLeg vars;
    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    Natural lookbackDays = 5;
    Leg leg = vars.makeLeg(lookbackDays);

    for (const auto& cf : leg)
    {
        auto oisCoupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(cf);
        BOOST_CHECK(oisCoupon != nullptr);
        if (oisCoupon)
        {
            // The coupon should have lookback configured
            BOOST_CHECK(oisCoupon->fixingDays() == lookbackDays ||
                        oisCoupon->fixingDays() == oisCoupon->index()->fixingDays());
        }
    }
}

BOOST_AUTO_TEST_CASE(testOvernightLegWithLockout)
{
    BOOST_TEST_MESSAGE("Testing overnight leg construction with lockout days...");

    CommonVarsONLeg vars;
    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    Natural lockoutDays = 3;
    Leg leg = vars.makeLeg(Null<Natural>(), lockoutDays);

    for (const auto& cf : leg)
    {
        auto oisCoupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(cf);
        BOOST_CHECK(oisCoupon != nullptr);
        if (oisCoupon)
        {
            BOOST_CHECK_EQUAL(oisCoupon->lockoutDays(), lockoutDays);
        }
    }
}

BOOST_AUTO_TEST_CASE(testOvernightLegWithObservationShift)
{
    BOOST_TEST_MESSAGE("Testing overnight leg construction with observation shift...");

    CommonVarsONLeg vars;
    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    Leg leg = vars.makeLeg(Null<Natural>(), 0, true);

    for (const auto& cf : leg)
    {
        auto oisCoupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(cf);
        BOOST_CHECK(oisCoupon != nullptr);
        if (oisCoupon)
        {
            BOOST_CHECK_EQUAL(oisCoupon->applyObservationShift(), true);
        }
    }
}

BOOST_AUTO_TEST_CASE(testOvernightLegWithGearingsAndSpreads)
{
    BOOST_TEST_MESSAGE("Testing overnight leg construction with gearings and spreads...");

    CommonVarsONLeg vars;
    vars.setupForecastCurve();

    std::vector<Real> gearings = {1.0, 1.25, 2.0, 0.5};
    std::vector<Spread> spreads = {0.0001, 0.0001, 0.0002, 0.0002};

    Leg leg = vars.makeLeg(Null<Natural>(), 0, false, false, RateAveraging::Compound, gearings, spreads);

    BOOST_CHECK_EQUAL(leg.size(), 4);

    for (Size i = 0; i < leg.size(); ++i)
    {
        auto oisCoupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(leg[i]);
        BOOST_CHECK(oisCoupon != nullptr);
        if (oisCoupon)
        {
            QL_CHECK_CLOSE(oisCoupon->gearing(), gearings[i], 1e-12);
            QL_CHECK_CLOSE(oisCoupon->spread(), spreads[i], 1e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testOvernightLegNPV)
{
    BOOST_TEST_MESSAGE("Testing overnight leg NPV...");

    CommonVarsONLeg vars;
    vars.setupForecastCurve();

    Leg leg = vars.makeLeg(Null<Natural>(), 3, false, true, RateAveraging::Compound);

    Handle<YieldTermStructure> discountCurve(flatRate(0.0015, Actual360()));

    // Calculate NPV
    Real expectedNpv = 34883.949669756257;
    Real npv = 0.0;
    for (const auto& cf : leg)
    {
        npv += cf->amount() * discountCurve->discount(cf->date());
    }

    CHECK_OIS_COUPON_RESULT("OvernightLeg NPV", npv, expectedNpv, 1e-8);
}

BOOST_AUTO_TEST_CASE(testOvernightLegWithCapsAndFloors)
{
    BOOST_TEST_MESSAGE("Testing overnight leg with caps and floors...");

    CommonVarsONLeg vars;
    vars.setupForecastCurve();
    Handle<YieldTermStructure> discountCurve(flatRate(0.0015, Actual360()));

    std::vector<Rate> caps = {0.0435, 0.0435, 0.04, 0.04};
    std::vector<Rate> floors = {0.025, 0.025, 0.025, 0.025};

    Leg leg = vars.makeLeg(Null<Natural>(), 0, false, false, RateAveraging::Compound, std::vector<Real>(),
                           std::vector<Spread>(), caps, floors);

    BOOST_CHECK_EQUAL(leg.size(), 4);

    Real expectedNpv = 34648.328606210489;
    Real npv = 0.0;

    for (Size i = 0; i < leg.size(); ++i)
    {
        auto cappedFlooredCoupon = ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(leg[i]);
        BOOST_CHECK(cappedFlooredCoupon != nullptr);
        if (cappedFlooredCoupon)
        {
            QL_CHECK_CLOSE(cappedFlooredCoupon->cap(), caps[i], 1e-12);
            QL_CHECK_CLOSE(cappedFlooredCoupon->floor(), floors[i], 1e-12);
            BOOST_CHECK(cappedFlooredCoupon->isCapped());
            BOOST_CHECK(cappedFlooredCoupon->isFloored());
        }
        npv += cappedFlooredCoupon->amount() * discountCurve->discount(cappedFlooredCoupon->date());
    }

    CHECK_OIS_COUPON_RESULT("Capped-Floored OvernightLeg NPV", npv, expectedNpv, 1e-8);
}

BOOST_AUTO_TEST_CASE(testOvernightLegSimpleAveraging)
{
    BOOST_TEST_MESSAGE("Testing overnight leg construction with simple averaging...");

    CommonVarsONLeg vars;
    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    Leg leg = vars.makeLeg(Null<Natural>(), 0, false, false, RateAveraging::Simple);

    for (const auto& cf : leg)
    {
        auto oisCoupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(cf);
        BOOST_CHECK(oisCoupon != nullptr);
        if (oisCoupon)
        {
            BOOST_CHECK_EQUAL(oisCoupon->averagingMethod(), RateAveraging::Simple);
        }
    }
}

BOOST_AUTO_TEST_CASE(testOvernightLegErrorConditions)
{
    BOOST_TEST_MESSAGE("Testing error conditions for overnight leg...");

    CommonVarsONLeg vars;
    vars.forecastCurve.linkTo(flatRate(0.0010, Actual360()));

    // Test that lookback with simple averaging throws an error
    BOOST_CHECK_THROW(vars.makeLeg(5, 0, false, false, RateAveraging::Simple), Error);

    // Test that lockout with simple averaging throws an error
    BOOST_CHECK_THROW(vars.makeLeg(Null<Natural>(), 3, false, false, RateAveraging::Simple), Error);

    // Test that observation shift with simple averaging throws an error
    BOOST_CHECK_THROW(vars.makeLeg(Null<Natural>(), 0, true, false, RateAveraging::Simple), Error);
}

BOOST_AUTO_TEST_CASE(testOvernightIndexedCouponPaymentBeforeAccrualEnd)
{
    BOOST_TEST_MESSAGE("Testing that an overnight coupon with inconsistent dates throws...");

    Date accrualStart(18, September, 2025);
    Settings::instance().evaluationDate() = accrualStart;

    Handle<YieldTermStructure> h(ext::make_shared<FlatForward>(accrualStart, 0.05, Actual365Fixed()));
    ext::shared_ptr<OvernightIndex> estr = ext::make_shared<Estr>(h);

    Calendar cal = WeekendsOnly();
    Date accrualEnd = cal.advance(accrualStart, Period(6, Months));
    Date paymentDate = cal.advance(accrualEnd, Period(-1, Days));

    BOOST_CHECK_THROW(OvernightIndexedCoupon(paymentDate, 1.0, accrualStart, accrualEnd, estr), Error);
}

BOOST_AUTO_TEST_CASE(testArithmeticAveragedConvexityForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing the arithmetic averaging pricer's convexity adjustment at small mean reversions...");

    // A flat 3% curve and a volatility of 1%, for a 3-month SOFR coupon starting a year out.
    const Date today(9, November, 2026);
    Settings::instance().evaluationDate() = today;
    const Handle<YieldTermStructure> curve(flatRate(today, 0.03, Actual365Fixed()));
    const auto index = ext::make_shared<Sofr>(curve);
    const Date start(9, November, 2027), end(9, February, 2028);

    const auto rate = [&](Real meanReversion, Volatility volatility, bool byApprox)
    {
        OvernightIndexedCoupon coupon(end, 1.0, start, end, index, 1.0, 0.0, Date(), Date(), DayCounter(), false,
                                      RateAveraging::Simple);
        coupon.setPricer(
            ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>(meanReversion, volatility, byApprox));
        return coupon.rate();
    };

    struct Case
    {
        // what the adjustment adds to the rate, with the approximation and without it
        Real meanReversion, approximated, exact;
    };

    // The closed forms as they stood, in 400-digit arithmetic, with a zero standing for its limit. Without the
    // approximation, they're evaluated at the pricer's own forecasts.
    const Case cases[] = {
        {0.0, -1.3474454843875727e-05, -1.3135918970300649e-05},
        {1e-12, -1.3474454843859966e-05, -1.3135918970285237e-05},
        {1e-10, -1.3474454842299668e-05, -1.3135918968759429e-05},
        {1e-09, -1.3474454828115136e-05, -1.3135918954888446e-05},
        {1e-08, -1.3474454686269811e-05, -1.313591881617861e-05},
        {1e-07, -1.3474453267816676e-05, -1.3135917429080368e-05},
        {1e-06, -1.3474439083295923e-05, -1.3135903558108304e-05},
        {1e-05, -1.3474297239149018e-05, -1.3135764849424972e-05},
        {0.0001, -1.3472878903735099e-05, -1.3134377866315593e-05},
        {0.001, -1.3458706149109488e-05, -1.3120518401747693e-05},
        {0.01, -1.3318032580964994e-05, -1.2982954567398401e-05},
        {0.03, -1.3012170755952631e-05, -1.2683854993383518e-05},
        {0.5, -7.8873106630049586e-06, -7.672619778088567e-06},
        {-0.01, -1.3633257904520307e-05, -1.3291211838484178e-05},
    };

    const Rate approximated = rate(0.03, 0.0, true), exact = rate(0.03, 0.0, false);
    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("mean reversion = " << c.meanReversion)
        {
            BOOST_CHECK_SMALL((rate(c.meanReversion, 0.01, true) - approximated) / c.approximated - 1.0, 1.0e-11);
            // Without the approximation, each of the 62 fixings' terms ends by subtracting one, which can leave up
            // to 2e-9 of the adjustment in rounding.
            BOOST_CHECK_SMALL((rate(c.meanReversion, 0.01, false) - exact) / c.exact - 1.0, 1.0e-8);
        }
    }
}

namespace
{
    // a capped and floored overnight coupon whose underlying coupon a derived class has dropped
    class CouponWithoutUnderlying : public CappedFlooredOvernightIndexedCoupon
    {
      public:
        using CappedFlooredOvernightIndexedCoupon::CappedFlooredOvernightIndexedCoupon;
        void dropUnderlying() { underlying_.reset(); }
    };

    class DerivedOvernightIndexedCoupon : public OvernightIndexedCoupon
    {
      public:
        using OvernightIndexedCoupon::OvernightIndexedCoupon;
    };

    // the coupon an overnight pricer prices for a given coupon, found as the pricer found it before it asked the
    // coupon
    const OvernightIndexedCoupon* overnightCouponByCasts(const FloatingRateCoupon& coupon)
    {
        if (const auto* c = dynamic_cast<const CappedFlooredOvernightIndexedCoupon*>(&coupon))
            return c->underlying().get();
        return dynamic_cast<const OvernightIndexedCoupon*>(&coupon);
    }
}

BOOST_AUTO_TEST_CASE(testDailyCapsAndFloorsOnStoredFixings)
{
    BOOST_TEST_MESSAGE("Testing daily caps and floors on an overnight coupon's stored fixings...");

    CommonVars vars;
    // the coupon's fixings are all stored, so the pricers read no volatility
    Handle<OptionletVolatilityStructure> vol(ext::make_shared<ConstantOptionletVolatility>(
        vars.today, UnitedStates(UnitedStates::GovernmentBond), Following, 0.2, Actual360()));

    // its fixings run from 2.36 % to 2.59 %
    const Rate cap = 0.0245, floor = 0.0241;
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
    {
        ext::shared_ptr<OvernightIndexedCouponPricer> pricer;
        if (averaging == RateAveraging::Compound)
            pricer = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vol);
        else
            pricer = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vol);
        for (bool capped : {true, false})
        {
            auto underlying =
                vars.makeCoupon(Date(1, July, 2019), Date(31, July, 2019), Null<Natural>(), 0, false, false, averaging);
            auto coupon = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                underlying, capped ? cap : Null<Rate>(), capped ? Null<Rate>() : floor, false, true);
            coupon->setPricer(pricer);

            const std::vector<Date>& fixingDates = underlying->fixingDates();
            const std::vector<Time>& dt = underlying->dt();
            Real sum = averaging == RateAveraging::Compound ? 1.0 : 0.0;
            Size bound = 0;
            for (Size i = 0; i < dt.size(); ++i)
            {
                Rate f = vars.sofr->fixing(fixingDates[i]);
                Rate r = capped ? std::min(f, cap) : std::max(f, floor);
                if (r != f)
                    ++bound;
                if (averaging == RateAveraging::Compound)
                    sum *= 1.0 + r * dt[i];
                else
                    sum += r * dt[i];
            }
            Rate expected = (averaging == RateAveraging::Compound ? sum - 1.0 : sum) / underlying->accrualPeriod();

            const std::string what = std::string(averaging == RateAveraging::Compound ? "compounded" : "averaged") +
                                     (capped ? " coupon capped daily" : " coupon floored daily");
            // the cap or floor binds on some days, so the test can tell it from the plain coupon
            BOOST_REQUIRE(bound > 0 && bound < dt.size());
            if (std::fabs(coupon->rate() - expected) > 1e-12)
                BOOST_ERROR("failed to reproduce the rate of a past "
                            << what << std::setprecision(16) << ":\n    calculated: " << coupon->rate()
                            << "\n    expected:   " << expected << "\n    difference: " << coupon->rate() - expected
                            << "\n    plain rate: " << underlying->rate());
        }
    }
}

BOOST_AUTO_TEST_CASE(testDailyCapsAndFloorsOnForecastsAtZeroVolatility)
{
    BOOST_TEST_MESSAGE("Testing daily caps and floors on an overnight coupon's forecasts at zero volatility...");

    BlackONPricerVars vars;
    vars.vol.linkTo(ext::make_shared<ConstantOptionletVolatility>(vars.today, TARGET(), Following, 0.0, vars.dc));
    // the curve is flat at 4 % continuously compounded; at zero volatility the pricers cap or floor that rate over the
    // coupon's future days, and compound it daily or add it up
    const Rate forward = 0.04;

    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
    {
        ext::shared_ptr<OvernightIndexedCouponPricer> pricer;
        if (averaging == RateAveraging::Compound)
            pricer = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vars.vol);
        else
            pricer = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vars.vol);
        // a cap and a floor that bind, and a cap and a floor that don't
        for (auto [isCap, strike] : {std::pair<bool, Rate>{true, 0.035}, {false, 0.045}, {true, 0.045}, {false, 0.035}})
        {
            auto underlying = vars.makeBaseCoupon(Date(2, July, 2035), Date(2, October, 2035), averaging);
            auto coupon = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                underlying, isCap ? strike : Null<Rate>(), isCap ? Null<Rate>() : strike, false, true);
            coupon->setPricer(pricer);

            const std::vector<Date>& dates = underlying->valueDates();
            const Real days = dates.back() - dates.front();
            const Time span = vars.dc.yearFraction(dates.front(), dates.back());
            const Real dailyTau = span / days;
            const Rate bounded = isCap ? std::min(forward, strike) : std::max(forward, strike);
            Rate expected;
            if (averaging == RateAveraging::Compound)
                expected = (std::pow(1.0 + dailyTau * bounded, days) - std::pow(1.0 + dailyTau * forward, days)) /
                           underlying->accrualPeriod();
            else
                expected = (bounded - forward) * span / underlying->accrualPeriod();

            const Rate difference = coupon->rate() - underlying->rate();
            const std::string what = std::string(averaging == RateAveraging::Compound ? "compounded" : "averaged") +
                                     (isCap ? " coupon capped daily at " : " coupon floored daily at ") +
                                     std::to_string(strike);
            if (std::fabs(difference - expected) > 1e-12)
                BOOST_ERROR("failed to reproduce the option part of a future "
                            << what << std::setprecision(16) << ":\n    calculated: " << difference
                            << "\n    expected:   " << expected << "\n    difference: " << difference - expected);
        }
    }
}

namespace
{
    // an overnight index under SOFR's name, so it shares SOFR's fixings, whose pastFixing gives twice what it stores
    class IndexDoublingItsPastFixings : public OvernightIndex
    {
      public:
        explicit IndexDoublingItsPastFixings(const Handle<YieldTermStructure>& h)
        : OvernightIndex("SOFR", 0, USDCurrency(), UnitedStates(UnitedStates::SOFR), Actual360(), h)
        {
        }
        Real pastFixing(const Date& fixingDate) const override
        {
            const Real fixing = OvernightIndex::pastFixing(fixingDate);
            return fixing == Null<Real>() ? fixing : 2.0 * fixing;
        }
    };
}

BOOST_AUTO_TEST_CASE(testDailyCapsAndFloorsOnFixingsUpToToday)
{
    BOOST_TEST_MESSAGE("Testing daily caps and floors on an overnight coupon's fixings up to today...");

    BlackONPricerVars vars;
    vars.vol.linkTo(ext::make_shared<ConstantOptionletVolatility>(vars.today, TARGET(), Following, 0.0, vars.dc));
    // at zero volatility the pricers cap or floor the curve's flat 4 % over the days still to fix
    const Rate forward = 0.04;

    // June's fixings run from 3.40 % to 3.60 %, and today's would be 3.52 %
    std::map<Date, Rate> fixings;
    Size k = 0;
    for (Date d(2, June, 2025); d < vars.today; ++d)
        if (vars.sofr->isValidFixingDate(d))
            fixings[d] = 0.034 + 0.0005 * static_cast<Real>(k++ % 5);
    const Rate todaysFixing = 0.0352;
    const Rate cap = 0.035, floor = 0.0355;

    // the caplets and floorlets read the fixings the index stores, as the swaplet does, also on an index whose
    // pastFixing gives other rates
    const std::vector<ext::shared_ptr<OvernightIndex>> indexes = {
        vars.sofr, ext::make_shared<IndexDoublingItsPastFixings>(vars.forecastCurve)};
    for (Size j = 0; j < indexes.size(); ++j)
    {
        vars.sofr = indexes[j];
        for (bool todayStored : {false, true})
        {
            vars.sofr->clearFixings();
            for (const auto& [d, f] : fixings)
                vars.sofr->addFixing(d, f);
            if (todayStored)
                vars.sofr->addFixing(vars.today, todaysFixing);

            for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
            {
                const bool compound = averaging == RateAveraging::Compound;
                ext::shared_ptr<OvernightIndexedCouponPricer> pricer;
                if (compound)
                    pricer = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vars.vol);
                else
                    pricer = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vars.vol);
                // a coupon accruing since June, and one whose first fixing is today
                for (const Date& start : {Date(2, June, 2025), vars.today})
                {
                    for (bool capped : {true, false})
                    {
                        auto underlying = vars.makeBaseCoupon(start, start + 3 * Months, averaging);
                        auto coupon = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                            underlying, capped ? cap : Null<Rate>(), capped ? Null<Rate>() : floor, false, true);
                        coupon->setPricer(pricer);

                        auto bounded = [&](Rate r) { return capped ? std::min(r, cap) : std::max(r, floor); };
                        const std::vector<Date>& fixingDates = underlying->fixingDates();
                        const std::vector<Date>& dates = underlying->valueDates();
                        const std::vector<Time>& dt = underlying->dt();
                        // the fixed days, each capped or floored
                        Real sum = compound ? 1.0 : 0.0, rawSum = sum;
                        Size i = 0, fixed = 0, bound = 0;
                        for (; i < dt.size(); ++i)
                        {
                            Rate f;
                            if (fixingDates[i] < vars.today)
                                f = fixings.at(fixingDates[i]);
                            else if (fixingDates[i] == vars.today && todayStored)
                                f = todaysFixing;
                            else
                                break;
                            ++fixed;
                            if (bounded(f) != f)
                                ++bound;
                            if (compound)
                            {
                                sum *= 1.0 + bounded(f) * dt[i];
                                rawSum *= 1.0 + f * dt[i];
                            }
                            else
                            {
                                sum += bounded(f) * dt[i];
                                rawSum += f * dt[i];
                            }
                        }
                        // the days still to fix, at the forward rate
                        const Real days = dates.back() - dates[i];
                        const Time span = vars.dc.yearFraction(dates[i], dates.back());
                        if (compound)
                        {
                            sum *= std::pow(1.0 + span / days * bounded(forward), days);
                            rawSum *= std::pow(1.0 + span / days * forward, days);
                        }
                        else
                        {
                            sum += bounded(forward) * span;
                            rawSum += forward * span;
                        }
                        const Rate expected = (sum - rawSum) / underlying->accrualPeriod();

                        const std::string what = std::string(compound ? "compounded" : "averaged") +
                                                 (start < vars.today ? " coupon accruing" : " coupon starting today") +
                                                 (capped ? ", capped daily," : ", floored daily,") +
                                                 (todayStored ? " with" : " without") + " today's fixing" +
                                                 (j == 1 ? ", on an index whose pastFixing doubles its fixings" : "");
                        // the cap or floor binds on some of the accruing coupon's fixed days and not on others
                        if (start < vars.today)
                            BOOST_REQUIRE(bound > 0 && bound < fixed);
                        else
                            BOOST_REQUIRE(fixed == (todayStored ? 1U : 0U));
                        const Rate difference = coupon->rate() - underlying->rate();
                        if (std::fabs(difference - expected) > 1e-12)
                            BOOST_ERROR("failed to reproduce the option part of a "
                                        << what << std::setprecision(16) << ":\n    calculated: " << difference
                                        << "\n    expected:   " << expected
                                        << "\n    difference: " << difference - expected);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testOvernightIndexedCouponOfEachCouponType)
{
    BOOST_TEST_MESSAGE("Testing the overnight coupon each type of floating coupon gives its pricer...");

    BlackONPricerVars vars;
    const Date start(2, July, 2035), end(2, October, 2035);
    auto overnight = vars.makeBaseCoupon(start, end);
    auto euribor = ext::make_shared<Euribor6M>(vars.forecastCurve);
    auto ibor = ext::make_shared<IborCoupon>(end, vars.notional, start, end, 2, euribor);
    auto derived = ext::make_shared<DerivedOvernightIndexedCoupon>(end, vars.notional, start, end, vars.sofr);
    auto withoutUnderlying = ext::make_shared<CouponWithoutUnderlying>(overnight, 0.045, 0.035);
    withoutUnderlying->dropUnderlying();

    const std::vector<std::pair<std::string, ext::shared_ptr<FloatingRateCoupon>>> coupons = {
        {"OvernightIndexedCoupon", overnight},
        {"CappedFlooredOvernightIndexedCoupon",
         ext::make_shared<CappedFlooredOvernightIndexedCoupon>(overnight, 0.045)},
        {"CappedFlooredOvernightIndexedCoupon capped daily",
         ext::make_shared<CappedFlooredOvernightIndexedCoupon>(overnight, 0.045, Null<Rate>(), false, true)},
        {"a class derived from OvernightIndexedCoupon", derived},
        {"CappedFlooredOvernightIndexedCoupon on a derived coupon",
         ext::make_shared<CappedFlooredOvernightIndexedCoupon>(derived, Null<Rate>(), 0.035)},
        {"a class derived from CappedFlooredOvernightIndexedCoupon, without its underlying coupon", withoutUnderlying},
        {"IborCoupon", ibor},
        {"CappedFlooredIborCoupon",
         ext::make_shared<CappedFlooredIborCoupon>(end, vars.notional, start, end, 2, euribor, 1.0, 0.0, 0.045, 0.035)},
        {"DigitalIborCoupon", ext::make_shared<DigitalIborCoupon>(ibor, 0.04)},
        {"CappedFlooredCoupon on an overnight coupon", ext::make_shared<CappedFlooredCoupon>(overnight, 0.045)},
        {"DigitalCoupon on an overnight coupon", ext::make_shared<DigitalCoupon>(overnight, 0.04)}};

    for (const auto& [name, coupon] : coupons)
    {
        const OvernightIndexedCoupon* expected = overnightCouponByCasts(*coupon);
        if (coupon->overnightIndexedCoupon() != expected)
            BOOST_ERROR(name << " gives " << (coupon->overnightIndexedCoupon() == nullptr ? "no" : "another")
                             << " overnight coupon where the casts find " << (expected == nullptr ? "none" : "one"));
    }
    // and the casts find one where they should
    BOOST_CHECK(overnightCouponByCasts(*overnight) == overnight.get());
    BOOST_CHECK(overnightCouponByCasts(*derived) == derived.get());
    BOOST_CHECK(overnightCouponByCasts(*ibor) == nullptr);
}

BOOST_AUTO_TEST_CASE(testOvernightIndexOfCoupon)
{
    BOOST_TEST_MESSAGE("Testing that an overnight coupon's overnight index is its index...");

    BlackONPricerVars vars;
    const Date start(2, July, 2035), end(2, October, 2035);
    auto plain = ext::make_shared<OvernightIndex>("Plain", 0, USDCurrency(), UnitedStates(UnitedStates::SOFR),
                                                  Actual360(), vars.forecastCurve);
    for (const auto& index : {vars.sofr, plain})
    {
        auto coupon = ext::make_shared<OvernightIndexedCoupon>(end, vars.notional, start, end, index);
        BOOST_CHECK(coupon->overnightIndex().get() == index.get());
        BOOST_CHECK(coupon->index().get() == index.get());
    }
}

BOOST_AUTO_TEST_CASE(testOvernightPricersRefuseOtherCoupons)
{
    BOOST_TEST_MESSAGE("Testing that the overnight pricers refuse coupons they can't price...");

    BlackONPricerVars vars;
    const Date start(2, July, 2035), end(2, October, 2035);
    auto euribor = ext::make_shared<Euribor6M>(vars.forecastCurve);
    auto ibor = ext::make_shared<IborCoupon>(end, vars.notional, start, end, 2, euribor);
    auto withoutUnderlying = ext::make_shared<CouponWithoutUnderlying>(vars.makeBaseCoupon(start, end), 0.045, 0.035);
    withoutUnderlying->dropUnderlying();

    const std::vector<std::pair<std::string, ext::shared_ptr<FloatingRateCouponPricer>>> pricers = {
        {"CompoundingOvernightIndexedCouponPricer", ext::make_shared<CompoundingOvernightIndexedCouponPricer>()},
        {"ArithmeticAveragedOvernightIndexedCouponPricer",
         ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>()},
        {"BlackCompoundingOvernightIndexedCouponPricer",
         ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vars.vol)},
        {"BlackAveragingOvernightIndexedCouponPricer",
         ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vars.vol)}};

    for (const auto& [name, pricer] : pricers)
    {
        BOOST_CHECK_EXCEPTION(pricer->initialize(*ibor), Error,
                              ExpectedErrorMessage("OvernightIndexedCouponPricer: unsupported coupon type"));
        BOOST_CHECK_EXCEPTION(pricer->initialize(*withoutUnderlying), Error,
                              ExpectedErrorMessage("OvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon "
                                                   "underlying coupon not defined"));
    }
}

BOOST_AUTO_TEST_CASE(testCappedFlooredOvernightCouponPricerErrors)
{
    BOOST_TEST_MESSAGE("Testing the errors of a capped or floored overnight coupon without a pricer it can use...");

    BlackONPricerVars vars;
    const Date start(2, July, 2035), end(2, October, 2035);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
    {
        const bool compound = averaging == RateAveraging::Compound;
        ext::shared_ptr<FloatingRateCouponPricer> black;
        if (compound)
            black = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vars.vol);
        else
            black = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vars.vol);

        auto withoutPricer = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
            vars.makeBaseCoupon(start, end, averaging), 0.045, 0.035);
        BOOST_CHECK_EXCEPTION(withoutPricer->rate(), Error,
                              ExpectedErrorMessage("coupon pricer not an instance of OvernightIndexedCouponPricer"));

        // an Ibor pricer, which the coupon's own setPricer refuses
        auto withIborPricer = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
            vars.makeBaseCoupon(start, end, averaging), 0.045, 0.035);
        withIborPricer->FloatingRateCoupon::setPricer(ext::make_shared<BlackIborCouponPricer>());
        BOOST_CHECK_EXCEPTION(withIborPricer->rate(), Error,
                              ExpectedErrorMessage("coupon pricer not an instance of OvernightIndexedCouponPricer"));

        // an overnight pricer that doesn't price caps and floors
        auto withPlainPricer = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
            vars.makeBaseCoupon(start, end, averaging), 0.045, 0.035);
        if (compound)
            withPlainPricer->setPricer(ext::make_shared<CompoundingOvernightIndexedCouponPricer>());
        else
            withPlainPricer->setPricer(ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>());
        BOOST_CHECK_EXCEPTION(
            withPlainPricer->rate(), Error,
            ExpectedErrorMessage(std::string(compound ? "CompoundingOvernightIndexedCouponPricer" :
                                                        "ArithmeticAveragedOvernightIndexedCouponPricer") +
                                 "::floorletRate(Rate, bool) not implemented"));

        auto underlying = vars.makeBaseCoupon(start, end, averaging);
        underlying->setPricer(ext::shared_ptr<FloatingRateCouponPricer>());
        auto withoutUnderlyingPricer = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, 0.045, 0.035);
        withoutUnderlyingPricer->setPricer(black);
        BOOST_CHECK_EXCEPTION(withoutUnderlyingPricer->rate(), Error,
                              ExpectedErrorMessage("underlying coupon pricer not set"));
    }
}

namespace
{
    ext::shared_ptr<OvernightIndexedCouponPricer>
    makeBlackOvernightPricer(RateAveraging::Type averaging, const Handle<OptionletVolatilityStructure>& vol)
    {
        if (averaging == RateAveraging::Compound)
            return ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(vol);
        return ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(vol);
    }

    // the coupon's rate and effective volatilities equal, to the bit, those of the same coupon built alone with
    // pricers of its own
    void checkAsAlone(const CappedFlooredOvernightIndexedCoupon& coupon,
                      const CappedFlooredOvernightIndexedCoupon& alone,
                      const std::string& what)
    {
        const Real rate = coupon.rate(), aloneRate = alone.rate();
        if (rate != aloneRate)
            BOOST_ERROR(what << ": rate " << std::setprecision(17) << rate << ", alone " << aloneRate);
        const Real capletVol = coupon.effectiveCapletVolatility(), aloneCapletVol = alone.effectiveCapletVolatility();
        if (capletVol != aloneCapletVol)
            BOOST_ERROR(what << ": effective caplet volatility " << std::setprecision(17) << capletVol << ", alone "
                             << aloneCapletVol);
        const Real floorletVol = coupon.effectiveFloorletVolatility(),
                   aloneFloorletVol = alone.effectiveFloorletVolatility();
        if (floorletVol != aloneFloorletVol)
            BOOST_ERROR(what << ": effective floorlet volatility " << std::setprecision(17) << floorletVol << ", alone "
                             << aloneFloorletVol);
    }

    std::string describe(RateAveraging::Type averaging, bool dailyCapFloor)
    {
        return std::string(averaging == RateAveraging::Compound ? "compounded" : "averaged") +
               (dailyCapFloor ? " coupon, daily caps and floors" : " coupon");
    }

    // an overnight coupon whose rate is set rather than worked out by its pricer
    class PresetRateOvernightCoupon : public OvernightIndexedCoupon
    {
      public:
        PresetRateOvernightCoupon(const Date& start,
                                  const Date& end,
                                  const ext::shared_ptr<OvernightIndex>& index,
                                  RateAveraging::Type averaging,
                                  Rate presetRate)
        : OvernightIndexedCoupon(end, 1.0, start, end, index, 1.0, 0.0, Date(), Date(), Actual360(), false, averaging),
          presetRate_(presetRate)
        {
        }
        void performCalculations() const override { rate_ = presetRate_; }

      private:
        Rate presetRate_;
    };

    // a Black pricer that records the coupons it is initialized on
    template <class BlackPricer>
    class CouponTrackingPricer : public BlackPricer
    {
      public:
        using BlackPricer::BlackPricer;
        void initialize(const FloatingRateCoupon& coupon) override
        {
            coupons.push_back(&coupon);
            BlackPricer::initialize(coupon);
        }
        std::vector<const FloatingRateCoupon*> coupons;
    };

    // a capped and floored overnight coupon whose gearing is twice its underlying's
    class CappedCouponWithOwnGearing : public CappedFlooredOvernightIndexedCoupon
    {
      public:
        CappedCouponWithOwnGearing(const ext::shared_ptr<OvernightIndexedCoupon>& underlying,
                                   Rate cap,
                                   Rate floor,
                                   bool dailyCapFloor)
        : CappedFlooredOvernightIndexedCoupon(underlying, cap, floor, false, dailyCapFloor)
        {
            gearing_ *= 2.0;
        }
    };

    template <class BlackPricer>
    void checkCouponTrackingPricer(BlackONPricerVars& vars, RateAveraging::Type averaging, bool dailyCapFloor)
    {
        const Date start(2, October, 2025), end(2, January, 2026);
        vars.forecastCurve.linkTo(flatRate(vars.today, 0.04, vars.dc));
        auto pricer = ext::make_shared<CouponTrackingPricer<BlackPricer>>(vars.vol);
        auto underlying = vars.makeBaseCoupon(start, end, averaging);
        underlying->setPricer(pricer);
        auto coupon =
            ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, 0.045, 0.035, false, dailyCapFloor);
        coupon->setPricer(pricer);
        coupon->rate();
        vars.forecastCurve.linkTo(flatRate(vars.today, 0.042, vars.dc));
        coupon->rate();

        // the pricer is initialized on the capped coupon at each of its calculations
        Size onCoupon = 0;
        for (const FloatingRateCoupon* c : pricer->coupons)
            if (c == coupon.get())
                ++onCoupon;
        if (onCoupon != 2)
            BOOST_ERROR(describe(averaging, dailyCapFloor)
                        << ": the pricer was initialized " << onCoupon << " times on the capped coupon, not 2");

        auto aloneUnderlying = vars.makeBaseCoupon(start, end, averaging);
        aloneUnderlying->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
        CappedFlooredOvernightIndexedCoupon alone(aloneUnderlying, 0.045, 0.035, false, dailyCapFloor);
        alone.setPricer(makeBlackOvernightPricer(averaging, vars.vol));
        checkAsAlone(*coupon, alone, describe(averaging, dailyCapFloor));
    }
}

BOOST_AUTO_TEST_CASE(testCappedFlooredOvernightLegOnOneBlackPricer)
{
    BOOST_TEST_MESSAGE("Testing a capped and floored overnight leg on one Black pricer...");

    BlackONPricerVars vars;
    const Schedule schedule = MakeSchedule()
                                  .from(Date(2, October, 2025))
                                  .to(Date(2, October, 2026))
                                  .withTenor(3 * Months)
                                  .withCalendar(TARGET())
                                  .withConvention(ModifiedFollowing);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
        for (bool daily : {false, true})
        {
            auto makeLeg = [&](const ext::shared_ptr<OvernightIndexedCouponPricer>& pricer)
            {
                OvernightLeg leg(schedule, vars.sofr);
                leg.withNotionals(vars.notional)
                    .withAveragingMethod(averaging)
                    .withCaps(0.045)
                    .withFloors(0.035)
                    .withDailyCapFloor(daily);
                if (pricer)
                    leg.withCouponPricer(pricer);
                std::vector<ext::shared_ptr<CappedFlooredOvernightIndexedCoupon>> coupons;
                for (const auto& cf : Leg(leg))
                    coupons.push_back(ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(cf));
                return coupons;
            };
            // the leg's coupons priced in the leg's order (0), in reverse (1), after every underlying's rate (2), and
            // in the leg's order once more after the curve moves (3)
            for (int order = 0; order < 4; ++order)
            {
                vars.forecastCurve.linkTo(flatRate(vars.today, 0.04, vars.dc));
                const auto coupons = makeLeg(makeBlackOvernightPricer(averaging, vars.vol));
                const Size n = coupons.size();
                BOOST_REQUIRE(n == 4);
                if (order == 2)
                    for (const auto& c : coupons)
                        c->underlying()->rate();
                for (Size k = 0; k < n; ++k)
                    coupons[order == 1 ? n - 1 - k : k]->rate();
                if (order == 3)
                {
                    vars.forecastCurve.linkTo(flatRate(vars.today, 0.042, vars.dc));
                    for (const auto& c : coupons)
                        c->rate();
                }

                const auto alone = makeLeg(nullptr);
                for (Size i = 0; i < n; ++i)
                {
                    alone[i]->underlying()->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
                    alone[i]->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
                    checkAsAlone(*coupons[i], *alone[i],
                                 describe(averaging, daily) + " " + std::to_string(i) + ", order " +
                                     std::to_string(order));
                }
            }
        }
}

BOOST_AUTO_TEST_CASE(testCappedFlooredOvernightCouponOnAnotherPricer)
{
    BOOST_TEST_MESSAGE("Testing a capped and floored overnight coupon on a pricer other than its underlying's...");

    BlackONPricerVars vars;
    const Date start(2, October, 2025), end(2, January, 2026);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
        for (bool daily : {false, true})
        {
            vars.forecastCurve.linkTo(flatRate(vars.today, 0.04, vars.dc));
            auto underlying = vars.makeBaseCoupon(start, end, averaging);
            underlying->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            auto coupon = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, 0.045, 0.035, false, daily);
            coupon->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            coupon->rate();
            // priced again after the curve moves
            vars.forecastCurve.linkTo(flatRate(vars.today, 0.042, vars.dc));

            auto aloneUnderlying = vars.makeBaseCoupon(start, end, averaging);
            aloneUnderlying->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            CappedFlooredOvernightIndexedCoupon alone(aloneUnderlying, 0.045, 0.035, false, daily);
            alone.setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            checkAsAlone(*coupon, alone, describe(averaging, daily));
        }
}

BOOST_AUTO_TEST_CASE(testCappedFlooredOvernightCouponOnDerivedUnderlying)
{
    BOOST_TEST_MESSAGE("Testing a capped and floored overnight coupon on an underlying of a derived class...");

    BlackONPricerVars vars;
    const Date start(2, October, 2025), end(2, January, 2026);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
        for (bool daily : {false, true})
        {
            vars.forecastCurve.linkTo(flatRate(vars.today, 0.04, vars.dc));
            auto pricer = makeBlackOvernightPricer(averaging, vars.vol);
            auto underlying = ext::make_shared<PresetRateOvernightCoupon>(start, end, vars.sofr, averaging, 0.041);
            underlying->setPricer(pricer);
            auto coupon = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, 0.045, 0.035, false, daily);
            coupon->setPricer(pricer);
            coupon->rate();
            // priced again after the curve moves
            vars.forecastCurve.linkTo(flatRate(vars.today, 0.042, vars.dc));

            auto aloneUnderlying = ext::make_shared<PresetRateOvernightCoupon>(start, end, vars.sofr, averaging, 0.041);
            aloneUnderlying->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            CappedFlooredOvernightIndexedCoupon alone(aloneUnderlying, 0.045, 0.035, false, daily);
            alone.setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            checkAsAlone(*coupon, alone, describe(averaging, daily));
        }
}

BOOST_AUTO_TEST_CASE(testCappedFlooredOvernightCouponOnDerivedPricer)
{
    BOOST_TEST_MESSAGE("Testing a capped and floored overnight coupon on a pricer of a derived class...");

    BlackONPricerVars vars;
    for (bool daily : {false, true})
    {
        checkCouponTrackingPricer<BlackCompoundingOvernightIndexedCouponPricer>(vars, RateAveraging::Compound, daily);
        checkCouponTrackingPricer<BlackAveragingOvernightIndexedCouponPricer>(vars, RateAveraging::Simple, daily);
    }
}

BOOST_AUTO_TEST_CASE(testDerivedCappedFlooredOvernightCoupon)
{
    BOOST_TEST_MESSAGE("Testing a capped and floored overnight coupon of a derived class...");

    BlackONPricerVars vars;
    const Date start(2, October, 2025), end(2, January, 2026);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
        for (bool daily : {false, true})
        {
            auto pricer = makeBlackOvernightPricer(averaging, vars.vol);
            auto underlying = vars.makeBaseCoupon(start, end, averaging);
            underlying->setPricer(pricer);
            CappedCouponWithOwnGearing coupon(underlying, 0.045, 0.035, daily);
            coupon.setPricer(pricer);

            auto aloneUnderlying = vars.makeBaseCoupon(start, end, averaging);
            aloneUnderlying->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            CappedCouponWithOwnGearing alone(aloneUnderlying, 0.045, 0.035, daily);
            alone.setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            checkAsAlone(coupon, alone, describe(averaging, daily));
        }
}

BOOST_AUTO_TEST_CASE(testCappedAndFlooredOvernightCouponsOnOneUnderlying)
{
    BOOST_TEST_MESSAGE("Testing a capped and a floored overnight coupon on one underlying and one Black pricer...");

    BlackONPricerVars vars;
    const Date start(2, October, 2025), end(2, January, 2026);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
        for (bool daily : {false, true})
            for (bool cappedFirst : {true, false})
            {
                auto pricer = makeBlackOvernightPricer(averaging, vars.vol);
                auto underlying = vars.makeBaseCoupon(start, end, averaging);
                underlying->setPricer(pricer);
                CappedFlooredOvernightIndexedCoupon capped(underlying, 0.045, Null<Rate>(), false, daily);
                capped.setPricer(pricer);
                CappedFlooredOvernightIndexedCoupon floored(underlying, Null<Rate>(), 0.035, false, daily);
                floored.setPricer(pricer);
                if (cappedFirst)
                    capped.rate();
                floored.rate();
                capped.rate();

                auto alone = [&](Rate cap, Rate floor)
                {
                    auto aloneUnderlying = vars.makeBaseCoupon(start, end, averaging);
                    aloneUnderlying->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
                    auto c = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(aloneUnderlying, cap, floor, false,
                                                                                   daily);
                    c->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
                    return c;
                };
                const std::string what =
                    describe(averaging, daily) + (cappedFirst ? ", capped first" : ", floored first");
                checkAsAlone(capped, *alone(0.045, Null<Rate>()), what + ": capped");
                checkAsAlone(floored, *alone(Null<Rate>(), 0.035), what + ": floored");
            }
}

BOOST_AUTO_TEST_CASE(testCappedFlooredOvernightCouponOnFrozenUnderlying)
{
    BOOST_TEST_MESSAGE("Testing a capped and floored overnight coupon on a frozen underlying...");

    BlackONPricerVars vars;
    const Date start(2, October, 2025), end(2, January, 2026);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
        for (bool daily : {false, true})
        {
            vars.forecastCurve.linkTo(flatRate(vars.today, 0.04, vars.dc));
            auto pricer = makeBlackOvernightPricer(averaging, vars.vol);
            auto underlying = vars.makeBaseCoupon(start, end, averaging);
            underlying->setPricer(pricer);
            CappedFlooredOvernightIndexedCoupon coupon(underlying, 0.045, 0.035, false, daily);
            coupon.setPricer(pricer);

            auto aloneUnderlying = vars.makeBaseCoupon(start, end, averaging);
            aloneUnderlying->setPricer(makeBlackOvernightPricer(averaging, vars.vol));
            CappedFlooredOvernightIndexedCoupon alone(aloneUnderlying, 0.045, 0.035, false, daily);
            alone.setPricer(makeBlackOvernightPricer(averaging, vars.vol));

            coupon.rate();
            alone.rate();
            // both underlyings frozen, then the curve moved
            underlying->freeze();
            aloneUnderlying->freeze();
            vars.forecastCurve.linkTo(flatRate(vars.today, 0.042, vars.dc));
            checkAsAlone(coupon, alone, describe(averaging, daily) + ", frozen");
            // and both thawed
            underlying->unfreeze();
            aloneUnderlying->unfreeze();
            checkAsAlone(coupon, alone, describe(averaging, daily) + ", thawed");
        }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
