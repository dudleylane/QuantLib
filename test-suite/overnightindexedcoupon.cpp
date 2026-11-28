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
#include <ql/termstructures/volatility/flatsmilesection.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/forwardcurve.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <functional>
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

    // With observation shift the fixings compound over the observation period, from 24 to 28 June, 4 days, and the
    // rate is annualised over it, though the coupon accrues for 7 days, from 1 to 8 July.
    Rate expectedRate =
        (vars.forecastCurve->discount(Date(24, June, 2019)) / vars.forecastCurve->discount(Date(28, June, 2019)) -
         1.0) *
        360.0 / 4;

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

BOOST_AUTO_TEST_CASE(testTodaysFixingRequiredWhenEnforced)
{
    BOOST_TEST_MESSAGE("Testing that overnight coupons need today's fixing where it is enforced...");

    const Date today(10, November, 2026);
    Settings::instance().evaluationDate() = today;

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    const Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(today, 0.025, dayCounter));
    const auto index = ext::make_shared<OvernightIndex>("T420", 0, USDCurrency(), calendar, dayCounter, curve);
    const Handle<OptionletVolatilityStructure> volatility(
        ext::make_shared<ConstantOptionletVolatility>(today, calendar, Following, 0.05, dayCounter));

    // a coupon of 25 business days whose first fixing is today
    const auto coupon = [&](RateAveraging::Type averaging, Natural lookbackDays)
    {
        const Date start = calendar.advance(today, lookbackDays, Days);
        const Date end = calendar.advance(start, 25, Days);
        return ext::make_shared<OvernightIndexedCoupon>(end, 100.0, start, end, index, 1.0, 0.0, Date(), Date(),
                                                        DayCounter(), false, averaging,
                                                        lookbackDays == 0 ? Null<Natural>() : lookbackDays);
    };
    const auto priced = [&](RateAveraging::Type averaging, Natural lookbackDays,
                            const ext::shared_ptr<FloatingRateCouponPricer>& pricer)
    {
        ext::shared_ptr<FloatingRateCoupon> c = coupon(averaging, lookbackDays);
        c->setPricer(pricer);
        return c;
    };
    // capped at 3 %, or a naked cap at 3 %, on a Black pricer
    const auto capped = [&](RateAveraging::Type averaging, bool naked, bool daily)
    {
        ext::shared_ptr<FloatingRateCoupon> c = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
            coupon(averaging, 0), 0.03, Null<Rate>(), naked, daily);
        if (averaging == RateAveraging::Compound)
            c->setPricer(ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(volatility));
        else
            c->setPricer(ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(volatility));
        return c;
    };

    // each row builds a new coupon, so that its rate is worked out under the setting in force
    const std::vector<std::pair<std::string, std::function<ext::shared_ptr<FloatingRateCoupon>()>>> rows = {
        {"compounded", [&]
         { return priced(RateAveraging::Compound, 0, ext::make_shared<CompoundingOvernightIndexedCouponPricer>()); }},
        {"averaged by approximation",
         [&]
         {
             return priced(RateAveraging::Simple, 0,
                           ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>(true));
         }},
        {"compounded, capped at 3 %", [&] { return capped(RateAveraging::Compound, false, false); }},
        {"compounded, naked daily cap at 3 %", [&] { return capped(RateAveraging::Compound, true, true); }},
        {"compounded with a lookback of 2 days", [&]
         { return priced(RateAveraging::Compound, 2, ext::make_shared<CompoundingOvernightIndexedCouponPricer>()); }},
        {"averaged exactly",
         [&]
         {
             return priced(RateAveraging::Simple, 0,
                           ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>(false));
         }},
        {"averaged, naked daily cap at 3 %", [&] { return capped(RateAveraging::Simple, true, true); }},
    };

    // today's fixing missing: each row forecasts it with the setting off, and refuses with it on
    const std::string missing = "Missing T420ON Actual/360 fixing for November 10th, 2026";
    for (const auto& [name, make] : rows)
    {
        BOOST_TEST_CONTEXT(name)
        {
            Settings::instance().enforcesTodaysHistoricFixings() = false;
            BOOST_CHECK_NO_THROW(make()->rate());
            Settings::instance().enforcesTodaysHistoricFixings() = true;
            BOOST_CHECK_EXCEPTION(make()->rate(), Error, ExpectedErrorMessage(missing));
        }
    }

    // today's fixing stored: each row takes it, with the setting on as with it off
    index->addFixing(today, 0.021);
    for (const auto& [name, make] : rows)
    {
        BOOST_TEST_CONTEXT(name)
        {
            Settings::instance().enforcesTodaysHistoricFixings() = false;
            const Rate off = make()->rate();
            Settings::instance().enforcesTodaysHistoricFixings() = true;
            BOOST_CHECK_EQUAL(make()->rate(), off);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLockoutFixedTodayWithoutForwardingCurve)
{
    BOOST_TEST_MESSAGE("Testing a lockout coupon on the day its lockout fixes, without a forwarding curve...");

    const Date today(10, November, 2026);
    Settings::instance().evaluationDate() = today;

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    // an index with no forwarding curve
    const auto index = ext::make_shared<OvernightIndex>("T421", 0, USDCurrency(), calendar, dayCounter);
    index->addFixing(Date(9, November, 2026), 0.02);
    index->addFixing(today, 0.03);
    const Handle<OptionletVolatilityStructure> volatility(
        ext::make_shared<ConstantOptionletVolatility>(today, calendar, Following, 0.05, dayCounter));

    // from Monday 9 to Friday 13 November with a lockout of 2 days, so with fixing dates 9, 10, 10 and 10 November,
    // every one of them stored
    const Date start(9, November, 2026), end(13, November, 2026);
    const Rate fixedRate =
        ((1.0 + 0.02 / 360) * (1.0 + 0.03 / 360) * (1.0 + 0.03 / 360) * (1.0 + 0.03 / 360) - 1.0) / (4.0 / 360);
    const Real tolerance = 1.0e-13;

    struct Row
    {
        std::string name;
        ext::shared_ptr<FloatingRateCoupon> coupon;
        Rate expected;
    };
    for (bool telescopicValueDates : {false, true})
    {
        const auto coupon = [&]
        {
            return ext::make_shared<OvernightIndexedCoupon>(end, 100.0, start, end, index, 1.0, 0.0, Date(), Date(),
                                                            DayCounter(), telescopicValueDates, RateAveraging::Compound,
                                                            Null<Natural>(), 2);
        };
        // capped or floored on the coupon's rate, on the Black compounding pricer
        const auto capped = [&](Rate cap, Rate floor)
        {
            auto c = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(coupon(), cap, floor);
            c->setPricer(ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(volatility));
            return c;
        };
        const std::vector<Row> rows = {
            {"compounded", coupon(), fixedRate},
            {"capped at 2.7 %", capped(0.027, Null<Rate>()), 0.027},
            {"floored at 2.8 %", capped(Null<Rate>(), 0.028), 0.028},
        };
        for (const auto& [name, c, expected] : rows)
        {
            BOOST_TEST_CONTEXT(name << (telescopicValueDates ? ", telescopic value dates" : ""))
            {
                Rate calculated = Null<Rate>();
                BOOST_CHECK_NO_THROW(calculated = c->rate());
                if (calculated != Null<Rate>())
                {
                    CHECK_OIS_COUPON_RESULT("rate", calculated, expected, tolerance);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testLockoutFixedTodayDailyCapsAndFloors)
{
    BOOST_TEST_MESSAGE("Testing daily caps and floors on a lockout coupon on the day its lockout fixes...");

    const Date today(10, November, 2026);
    Settings::instance().evaluationDate() = today;

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    const Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(today, 0.025, dayCounter));
    const Handle<OptionletVolatilityStructure> volatility(
        ext::make_shared<ConstantOptionletVolatility>(today, calendar, Following, 0.05, dayCounter));
    // fixings of 2 % for 9 November and 3 % for 10 November, on an index with a forwarding curve and on one without
    const auto withCurve = ext::make_shared<OvernightIndex>("T430", 0, USDCurrency(), calendar, dayCounter, curve);
    const auto withoutCurve = ext::make_shared<OvernightIndex>("T430N", 0, USDCurrency(), calendar, dayCounter);
    for (const auto& index : {withCurve, withoutCurve})
    {
        index->addFixing(Date(9, November, 2026), 0.02);
        index->addFixing(today, 0.03);
    }

    // from Monday 9 to Friday 13 November with a lockout of 2 days, so with fixing dates 9, 10, 10 and 10 November
    const Date start(9, November, 2026), end(13, November, 2026);
    const auto coupon = [&](const ext::shared_ptr<OvernightIndex>& index, bool telescopicValueDates)
    {
        return ext::make_shared<OvernightIndexedCoupon>(end, 100.0, start, end, index, 1.0, 0.0, Date(), Date(),
                                                        DayCounter(), telescopicValueDates, RateAveraging::Compound,
                                                        Null<Natural>(), 2);
    };
    // capped or floored on each day's rate, on the Black compounding pricer
    const auto daily = [&](const ext::shared_ptr<OvernightIndexedCoupon>& underlying, Rate cap, Rate floor, bool naked)
    {
        auto c = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, cap, floor, naked, true);
        c->setPricer(ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(volatility));
        return c;
    };
    // the rate compounding one rate on the first day and another on the other three
    const auto compounded = [](Rate first, Rate others)
    {
        return ((1.0 + first / 360) * (1.0 + others / 360) * (1.0 + others / 360) * (1.0 + others / 360) - 1.0) /
               (4.0 / 360);
    };
    const Rate fixedRate = compounded(0.02, 0.03), cappedRate = compounded(0.02, 0.028),
               flooredRate = compounded(0.032, 0.032);
    // each of these rates is a compound factor less 1 over 4/360, which leaves up to about 7e-14 of rounding, and a
    // row's rate and its expected value take up to four of them
    const Real tolerance = 1.0e-12;
    const Real none = Null<Real>();

    struct Row
    {
        std::string name;
        ext::shared_ptr<CappedFlooredOvernightIndexedCoupon> coupon;
        Rate expected;
    };
    for (const auto& index : {withCurve, withoutCurve})
    {
        for (bool telescopicValueDates : {false, true})
        {
            const std::vector<Row> rows = {
                {"daily cap at 2.8 %", daily(coupon(index, telescopicValueDates), 0.028, Null<Rate>(), false),
                 cappedRate},
                {"naked daily cap at 2.8 %", daily(coupon(index, telescopicValueDates), 0.028, Null<Rate>(), true),
                 fixedRate - cappedRate},
                {"daily floor at 3.2 %", daily(coupon(index, telescopicValueDates), Null<Rate>(), 0.032, false),
                 flooredRate},
                {"naked daily floor at 3.2 %", daily(coupon(index, telescopicValueDates), Null<Rate>(), 0.032, true),
                 flooredRate - fixedRate},
            };
            for (const auto& [name, c, expected] : rows)
            {
                BOOST_TEST_CONTEXT(name << (index == withCurve ? "" : ", without a forwarding curve")
                                        << (telescopicValueDates ? ", telescopic value dates" : ""))
                {
                    Rate calculated = Null<Rate>();
                    BOOST_CHECK_NO_THROW(calculated = c->rate());
                    if (calculated != Null<Rate>())
                    {
                        CHECK_OIS_COUPON_RESULT("rate", calculated, expected, tolerance);
                        // no day is left to forecast, so neither effective volatility is given
                        BOOST_CHECK_EQUAL(c->effectiveCapletVolatility(), none);
                        BOOST_CHECK_EQUAL(c->effectiveFloorletVolatility(), none);
                    }
                }
            }
        }
    }

    // today's fixing missing, on an index with a forwarding curve: a daily cap that binds on no day gives the
    // coupon's rate
    const auto unfixed = ext::make_shared<OvernightIndex>("T430U", 0, USDCurrency(), calendar, dayCounter, curve);
    unfixed->addFixing(Date(9, November, 2026), 0.02);
    for (bool telescopicValueDates : {false, true})
    {
        BOOST_TEST_CONTEXT("daily cap at 2.8 %, today's fixing missing"
                           << (telescopicValueDates ? ", telescopic value dates" : ""))
        {
            const auto underlying = coupon(unfixed, telescopicValueDates);
            const auto c = daily(underlying, 0.028, Null<Rate>(), false);
            Rate calculated = Null<Rate>();
            BOOST_CHECK_NO_THROW(calculated = c->rate());
            if (calculated != Null<Rate>())
            {
                CHECK_OIS_COUPON_RESULT("rate", calculated, underlying->rate(), tolerance);
            }
        }
    }
}

namespace
{
    // an optionlet volatility linear in time, v0 + slope t, so that the time a volatility is taken at shows in it
    class LinearOptionletVolatility : public OptionletVolatilityStructure
    {
      public:
        LinearOptionletVolatility(
            Natural settlementDays, const Calendar& calendar, Volatility v0, Real slope, const DayCounter& dayCounter)
        : OptionletVolatilityStructure(settlementDays, calendar, Following, dayCounter), v0_(v0), slope_(slope)
        {
        }
        LinearOptionletVolatility(const Date& referenceDate,
                                  const Calendar& calendar,
                                  Volatility v0,
                                  Real slope,
                                  const DayCounter& dayCounter)
        : OptionletVolatilityStructure(referenceDate, calendar, Following, dayCounter), v0_(v0), slope_(slope)
        {
        }
        Date maxDate() const override { return Date::maxDate(); }
        Rate minStrike() const override { return QL_MIN_REAL; }
        Rate maxStrike() const override { return QL_MAX_REAL; }

      protected:
        ext::shared_ptr<SmileSection> smileSectionImpl(Time t) const override
        {
            return ext::make_shared<FlatSmileSection>(t, volatilityImpl(t, 0.0), dayCounter());
        }
        Volatility volatilityImpl(Time t, Rate) const override { return v0_ + slope_ * t; }

      private:
        Volatility v0_;
        Real slope_;
    };
}

BOOST_AUTO_TEST_CASE(testDailyCapsAndFloorsFiniteEffectiveVolatilities)
{
    BOOST_TEST_MESSAGE("Testing daily caps' and floors' effective volatilities where the volatility's reference date "
                       "is on or after the coupon's last fixing date...");

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    const Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(0, calendar, 0.025, dayCounter));
    const auto index = ext::make_shared<OvernightIndex>("T431", 0, USDCurrency(), calendar, dayCounter, curve);
    const Rate cap = 0.028, floor = 0.032;
    const auto linear = [&](Natural settlementDays)
    { return ext::make_shared<LinearOptionletVolatility>(settlementDays, calendar, 0.05, 2.0, dayCounter); };

    // coupons from Monday 9 November 2026, with fixings of 2 % for 9 November and 3 % for each later day before the
    // evaluation date; coupon A ends on Friday 13 November, so its last fixing date is 12 November, and coupon B on
    // Monday 16 November, so its last fixing date is 13 November
    const Date start(9, November, 2026), endA(13, November, 2026), endB(16, November, 2026);
    struct Coupons
    {
        ext::shared_ptr<CappedFlooredOvernightIndexedCoupon> capped, floored;
    };
    const auto coupons = [&](const Date& today, const Date& end, Natural lockoutDays, RateAveraging::Type averaging,
                             const ext::shared_ptr<OptionletVolatilityStructure>& volatility)
    {
        Settings::instance().evaluationDate() = today;
        index->clearFixings();
        for (Date d = start; d < today; ++d)
        {
            if (calendar.isBusinessDay(d))
                index->addFixing(d, d == start ? 0.02 : 0.03);
        }
        const Handle<OptionletVolatilityStructure> handle(volatility);
        ext::shared_ptr<FloatingRateCouponPricer> pricer;
        if (averaging == RateAveraging::Compound)
            pricer = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(handle);
        else
            pricer = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(handle);
        // the pricer set on each coupon, as OvernightLeg does
        const auto underlying =
            ext::make_shared<OvernightIndexedCoupon>(end, 100.0, start, end, index, 1.0, 0.0, Date(), Date(),
                                                     DayCounter(), false, averaging, Null<Natural>(), lockoutDays);
        underlying->setPricer(pricer);
        Coupons result = {
            ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, cap, Null<Rate>(), false, true),
            ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, Null<Rate>(), floor, false, true)};
        result.capped->setPricer(pricer);
        result.floored->setPricer(pricer);
        return result;
    };
    const auto method = [](RateAveraging::Type averaging)
    { return averaging == RateAveraging::Compound ? ", compounding" : ", averaging"; };

    // Where the time from the volatility's reference date to the coupon's last fixing date isn't positive, the
    // effective volatility is the volatility at the time halfway through the days still to fix, up to any lockout, at
    // which the pricer takes their standard deviation.
    struct Row
    {
        std::string name;
        Date today, end;
        Natural lockoutDays;
        ext::shared_ptr<OptionletVolatilityStructure> volatility;
        // the first value date of the days still to fix and the value date they run to, up to any lockout
        Date from, to;
    };
    const std::vector<Row> rows = {
        {"coupon A on its last fixing date, not fixed", Date(12, November, 2026), endA, 0, linear(0),
         Date(12, November, 2026), endA},
        // fixing dates 9, 10, 10 and 10 November
        {"coupon A with a lockout of 2 days on 10 November, not fixed", Date(10, November, 2026), endA, 2, linear(0),
         Date(10, November, 2026), Date(11, November, 2026)},
        {"coupon B on 12 November, the volatility's reference date on its last fixing date", Date(12, November, 2026),
         endB, 0, linear(1), Date(12, November, 2026), endB},
        // halfway through the days still to fix is the volatility's reference date too
        {"coupon A on 11 November, the volatility's reference date on its last fixing date", Date(11, November, 2026),
         endA, 0, linear(1), Date(11, November, 2026), endA},
        {"coupon B on 13 November, the volatility's reference date after its last fixing date",
         Date(13, November, 2026), endB, 0,
         ext::make_shared<LinearOptionletVolatility>(Date(14, November, 2026), calendar, 0.05, 2.0, dayCounter),
         Date(13, November, 2026), endB},
    };
    for (const auto& [name, today, end, lockoutDays, volatility, from, to] : rows)
    {
        for (RateAveraging::Type averaging : {RateAveraging::Compound, RateAveraging::Simple})
        {
            // the coupon's constructor refuses a lockout with simple averaging
            if (lockoutDays > 0 && averaging == RateAveraging::Simple)
                continue;
            BOOST_TEST_CONTEXT(name << method(averaging))
            {
                const auto [capped, floored] = coupons(today, end, lockoutDays, averaging, volatility);
                const Time midPoint = (volatility->timeFromReference(from) + volatility->timeFromReference(to)) / 2.0;
                Real caplet = Null<Real>(), floorlet = Null<Real>();
                BOOST_CHECK_NO_THROW(caplet = capped->effectiveCapletVolatility());
                BOOST_CHECK_NO_THROW(floorlet = floored->effectiveFloorletVolatility());
                BOOST_CHECK_EQUAL(caplet, volatility->volatility(midPoint, cap));
                BOOST_CHECK_EQUAL(floorlet, volatility->volatility(midPoint, floor));
            }
        }
    }

    // Where that time is positive, the effective volatility is the standard deviation over its square root. On 10
    // November, with that day's fixing not stored, the days still to fix run from 10 to 13 November and the last fixing
    // date is 12 November.
    for (RateAveraging::Type averaging : {RateAveraging::Compound, RateAveraging::Simple})
    {
        BOOST_TEST_CONTEXT("coupon A on 10 November, not fixed" << method(averaging))
        {
            const auto volatility = linear(0);
            const auto [capped, floored] = coupons(Date(10, November, 2026), endA, 0, averaging, volatility);
            const Time midPoint =
                (volatility->timeFromReference(Date(10, November, 2026)) + volatility->timeFromReference(endA)) / 2.0;
            const Time timeToLastFixing = volatility->timeFromReference(Date(12, November, 2026));
            Real caplet = Null<Real>(), floorlet = Null<Real>();
            BOOST_CHECK_NO_THROW(caplet = capped->effectiveCapletVolatility());
            BOOST_CHECK_NO_THROW(floorlet = floored->effectiveFloorletVolatility());
            BOOST_CHECK_EQUAL(caplet, volatility->volatility(midPoint, cap) * std::sqrt(midPoint) /
                                          std::sqrt(timeToLastFixing));
            BOOST_CHECK_EQUAL(floorlet, volatility->volatility(midPoint, floor) * std::sqrt(midPoint) /
                                            std::sqrt(timeToLastFixing));
        }
    }

    // On 9 November, nothing fixed, the volatility's reference date, two business days on, is 11 November, halfway
    // through the days still to fix, from 9 to 13 November: the standard deviation is 0, and so is the effective
    // volatility.
    for (RateAveraging::Type averaging : {RateAveraging::Compound, RateAveraging::Simple})
    {
        BOOST_TEST_CONTEXT("coupon A on 9 November, the volatility's reference date on 11 November"
                           << method(averaging))
        {
            const auto [capped, floored] = coupons(start, endA, 0, averaging, linear(2));
            Real caplet = Null<Real>(), floorlet = Null<Real>();
            BOOST_CHECK_NO_THROW(caplet = capped->effectiveCapletVolatility());
            BOOST_CHECK_NO_THROW(floorlet = floored->effectiveFloorletVolatility());
            BOOST_CHECK_EQUAL(caplet, 0.0);
            BOOST_CHECK_EQUAL(floorlet, 0.0);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCapsAndFloorsOnTheRateWithLateVolatilityReferenceDates)
{
    BOOST_TEST_MESSAGE("Testing caps and floors on an overnight coupon's rate where the volatility's reference date is "
                       "on or after the coupon's last fixing date...");

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    const Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(0, calendar, 0.025, dayCounter));
    const auto index = ext::make_shared<OvernightIndex>("T449", 0, USDCurrency(), calendar, dayCounter, curve);
    const Rate cap = 0.028, floor = 0.032;
    const auto linear = [&](Natural settlementDays)
    { return ext::make_shared<LinearOptionletVolatility>(settlementDays, calendar, 0.05, 2.0, dayCounter); };
    const auto fixedOn = [&](const Date& referenceDate)
    { return ext::make_shared<LinearOptionletVolatility>(referenceDate, calendar, 0.05, 2.0, dayCounter); };

    // coupons from Monday 9 November 2026, with fixings of 2 % for 9 November and 3 % for each later day before the
    // evaluation date; coupon A ends on Friday 13 November, so its last fixing date is 12 November, and coupon B on
    // Monday 16 November, so its last fixing date is 13 November
    const Date start(9, November, 2026), endA(13, November, 2026), endB(16, November, 2026);
    struct Coupons
    {
        ext::shared_ptr<CappedFlooredOvernightIndexedCoupon> capped, floored;
    };
    const auto coupons = [&](const Date& today, const Date& end, Natural lockoutDays, RateAveraging::Type averaging,
                             bool effectiveVolatilityInput,
                             const ext::shared_ptr<OptionletVolatilityStructure>& volatility)
    {
        Settings::instance().evaluationDate() = today;
        index->clearFixings();
        for (Date d = start; d < today; ++d)
        {
            if (calendar.isBusinessDay(d))
                index->addFixing(d, d == start ? 0.02 : 0.03);
        }
        const Handle<OptionletVolatilityStructure> handle(volatility);
        ext::shared_ptr<FloatingRateCouponPricer> pricer;
        if (averaging == RateAveraging::Compound)
            pricer = ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(handle, effectiveVolatilityInput);
        else
            pricer = ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(handle, effectiveVolatilityInput);
        // the pricer set on each coupon, as OvernightLeg does
        const auto underlying =
            ext::make_shared<OvernightIndexedCoupon>(end, 100.0, start, end, index, 1.0, 0.0, Date(), Date(),
                                                     DayCounter(), false, averaging, Null<Natural>(), lockoutDays);
        underlying->setPricer(pricer);
        Coupons result = {ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, cap, Null<Rate>()),
                          ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, Null<Rate>(), floor)};
        result.capped->setPricer(pricer);
        result.floored->setPricer(pricer);
        return result;
    };
    const auto method = [](RateAveraging::Type averaging, bool effectiveVolatilityInput)
    {
        return std::string(averaging == RateAveraging::Compound ? ", compounding" : ", averaging") +
               (effectiveVolatilityInput ? ", effective volatility input" : "");
    };

    struct Row
    {
        std::string name;
        Date today, end;
        Natural lockoutDays;
        ext::shared_ptr<OptionletVolatilityStructure> volatility;
        Date lastFixingDate;
    };

    // Where the volatility's reference date is the coupon's last fixing date, the time from one to the other is 0, and
    // the effective volatility is the volatility the standard deviation was taken at: with an effective volatility
    // input, the volatility at the last fixing date, and without one, the volatility at the later of the coupon's first
    // fixing date and the day after the volatility's reference date.
    const std::vector<Row> onTheLastFixingDate = {
        {"coupon A on 11 November, the volatility's reference date, one business day on, on 12 November",
         Date(11, November, 2026), endA, 0, linear(1), Date(12, November, 2026)},
        {"coupon A on 10 November, the volatility's reference date, two business days on, on 12 November",
         Date(10, November, 2026), endA, 0, linear(2), Date(12, November, 2026)},
        {"coupon A on 10 November, the volatility's reference date fixed on 12 November", Date(10, November, 2026),
         endA, 0, fixedOn(Date(12, November, 2026)), Date(12, November, 2026)},
        {"coupon B on 12 November, the volatility's reference date, one business day on, on 13 November",
         Date(12, November, 2026), endB, 0, linear(1), Date(13, November, 2026)},
        {"coupon B on 11 November, the volatility's reference date, two business days on, on 13 November",
         Date(11, November, 2026), endB, 0, linear(2), Date(13, November, 2026)},
        // fixing dates 9, 10, 10 and 10 November
        {"coupon A with a lockout of 2 days on 9 November, the volatility's reference date, one business day on, on 10 "
         "November",
         start, endA, 2, linear(1), Date(10, November, 2026)},
    };
    for (const auto& [name, today, end, lockoutDays, volatility, lastFixingDate] : onTheLastFixingDate)
    {
        for (RateAveraging::Type averaging : {RateAveraging::Compound, RateAveraging::Simple})
        {
            // the coupon's constructor refuses a lockout with simple averaging
            if (lockoutDays > 0 && averaging == RateAveraging::Simple)
                continue;
            for (bool effectiveVolatilityInput : {false, true})
            {
                BOOST_TEST_CONTEXT(name << method(averaging, effectiveVolatilityInput))
                {
                    const auto [capped, floored] =
                        coupons(today, end, lockoutDays, averaging, effectiveVolatilityInput, volatility);
                    const Date volatilityDate =
                        effectiveVolatilityInput ? lastFixingDate : std::max(start, volatility->referenceDate() + 1);
                    Real caplet = Null<Real>(), floorlet = Null<Real>();
                    BOOST_CHECK_NO_THROW(caplet = capped->effectiveCapletVolatility());
                    BOOST_CHECK_NO_THROW(floorlet = floored->effectiveFloorletVolatility());
                    BOOST_CHECK_EQUAL(caplet, volatility->volatility(volatilityDate, cap));
                    BOOST_CHECK_EQUAL(floorlet, volatility->volatility(volatilityDate, floor));
                }
            }
        }
    }

    // Where the volatility's reference date is after the coupon's last fixing date, the pricers refuse it, with and
    // without an effective volatility input, naming both dates.
    const std::vector<Row> afterTheLastFixingDate = {
        {"coupon A on 11 November, the volatility's reference date, two business days on, on 13 November",
         Date(11, November, 2026), endA, 0, linear(2), Date(12, November, 2026)},
        {"coupon A on 10 November, the volatility's reference date fixed on 13 November", Date(10, November, 2026),
         endA, 0, fixedOn(Date(13, November, 2026)), Date(12, November, 2026)},
        {"coupon B on 12 November, the volatility's reference date, two business days on, on 16 November",
         Date(12, November, 2026), endB, 0, linear(2), Date(13, November, 2026)},
    };
    for (const auto& [name, today, end, lockoutDays, volatility, lastFixingDate] : afterTheLastFixingDate)
    {
        for (RateAveraging::Type averaging : {RateAveraging::Compound, RateAveraging::Simple})
        {
            for (bool effectiveVolatilityInput : {false, true})
            {
                BOOST_TEST_CONTEXT(name << method(averaging, effectiveVolatilityInput))
                {
                    const auto [capped, floored] =
                        coupons(today, end, lockoutDays, averaging, effectiveVolatilityInput, volatility);
                    std::ostringstream message;
                    message << (averaging == RateAveraging::Compound ? "BlackCompoundingOvernightIndexedCouponPricer" :
                                                                       "BlackAveragingOvernightIndexedCouponPricer")
                            << ": the coupon's last fixing date (" << lastFixingDate
                            << ") is before the optionlet volatility's reference date (" << volatility->referenceDate()
                            << ")";
                    BOOST_CHECK_EXCEPTION(capped->rate(), Error, ExpectedErrorMessage(message.str()));
                    BOOST_CHECK_EXCEPTION(floored->rate(), Error, ExpectedErrorMessage(message.str()));
                }
            }
        }
    }

    // Where that time is positive, the effective volatility is the standard deviation over its square root. On 10
    // November, with the volatility's reference date the evaluation date, coupon A's fixing dates run from 9 to 12
    // November; without an effective volatility input, the pricer takes the volatility at 11 November, the day after
    // the reference date, and the standard deviation over a time that starts at the reference date and is extended
    // towards the last fixing date.
    for (RateAveraging::Type averaging : {RateAveraging::Compound, RateAveraging::Simple})
    {
        for (bool effectiveVolatilityInput : {false, true})
        {
            BOOST_TEST_CONTEXT("coupon A on 10 November, the volatility's reference date today"
                               << method(averaging, effectiveVolatilityInput))
            {
                const auto volatility = linear(0);
                const auto [capped, floored] =
                    coupons(Date(10, November, 2026), endA, 0, averaging, effectiveVolatilityInput, volatility);
                const Time firstFixingTime = volatility->timeFromReference(start);
                const Time lastFixingTime = volatility->timeFromReference(Date(12, November, 2026));
                const auto expected = [&](Rate strike)
                {
                    if (effectiveVolatilityInput)
                        return volatility->volatility(Date(12, November, 2026), strike) * std::sqrt(lastFixingTime) /
                               std::sqrt(lastFixingTime);
                    Time t = std::max(firstFixingTime, 0.0);
                    t += std::pow(lastFixingTime - t, 3.0) / std::pow(lastFixingTime - firstFixingTime, 2.0) / 3.0;
                    return volatility->volatility(Date(11, November, 2026), strike) * std::sqrt(t) /
                           std::sqrt(lastFixingTime);
                };
                Real caplet = Null<Real>(), floorlet = Null<Real>();
                BOOST_CHECK_NO_THROW(caplet = capped->effectiveCapletVolatility());
                BOOST_CHECK_NO_THROW(floorlet = floored->effectiveFloorletVolatility());
                BOOST_CHECK_EQUAL(caplet, expected(cap));
                BOOST_CHECK_EQUAL(floorlet, expected(floor));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testSpreadCompoundedDailyOnForecastDays)
{
    BOOST_TEST_MESSAGE("Testing a spread compounded daily on the days still to fix...");

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    // a flat curve of 3 %, continuously compounded on Actual/360
    const Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(Date(1, September, 2026), 0.03, dayCounter));
    const auto index = ext::make_shared<OvernightIndex>("T419", 0, USDCurrency(), calendar, dayCounter, curve);
    // coupons from Thursday 1 October to Friday 20 November 2026, with fixings of 2.1 % stored from 1 to 20 October
    const Date start(1, October, 2026), end(20, November, 2026);
    for (Date d = start; d < Date(21, October, 2026); d = calendar.advance(d, 1, Days))
        index->addFixing(d, 0.021);
    const Real tolerance = 1.0e-12;

    const auto coupon =
        [&](Spread spread, bool telescopicValueDates, Natural lockoutDays, RateAveraging::Type averagingMethod)
    {
        return ext::make_shared<OvernightIndexedCoupon>(end, 100.0, start, end, index, 1.0, spread, Date(), Date(),
                                                        DayCounter(), telescopicValueDates, averagingMethod,
                                                        Null<Natural>(), lockoutDays, false, true);
    };
    const auto checkRate = [&](const std::string& name, const ext::shared_ptr<Coupon>& c, Rate expected)
    {
        BOOST_TEST_CONTEXT(name)
        {
            Rate calculated = Null<Rate>();
            BOOST_CHECK_NO_THROW(calculated = c->rate());
            if (calculated != Null<Rate>())
            {
                CHECK_OIS_COUPON_RESULT("rate", calculated, expected, tolerance);
            }
        }
    };
    const auto checkSameDates =
        [](const std::string& name, const std::vector<Date>& calculated, const std::vector<Date>& expected)
    {
        BOOST_CHECK_MESSAGE(calculated == expected, name << ": " << calculated.size() << " dates where "
                                                         << expected.size() << " were expected");
    };

    // with every day still to fix, and with the fixings stored up to the day before today
    for (const Date& today : {Date(1, September, 2026), Date(21, October, 2026)})
    {
        Settings::instance().evaluationDate() = today;
        BOOST_TEST_CONTEXT("today " << today)
        {
            // the coupon's rate compounding each day's fixing plus the spread: the stored fixing before today, the
            // curve's forecast from today on
            const auto expectedRate = [&](Spread spread)
            {
                Real compoundFactor = 1.0;
                for (Date d = start; d < end; d = calendar.advance(d, 1, Days))
                {
                    const Date next = calendar.advance(d, 1, Days);
                    const Time span = dayCounter.yearFraction(d, next);
                    const Rate fixing = d < today ? 0.021 : (curve->discount(d) / curve->discount(next) - 1.0) / span;
                    compoundFactor *= 1.0 + (fixing + spread) * span;
                }
                return (compoundFactor - 1.0) / dayCounter.yearFraction(start, end);
            };
            const Rate withSpread = expectedRate(0.005), withoutSpread = expectedRate(0.0);

            // a spread of 50 bp compounded daily, with telescopic value dates or not
            const auto full = coupon(0.005, false, 0, RateAveraging::Compound);
            const auto telescopic = coupon(0.005, true, 0, RateAveraging::Compound);
            checkRate("50 bp", full, withSpread);
            checkRate("50 bp, telescopic value dates", telescopic, withSpread);
            // capped at 100 % on the Black compounding pricer
            auto capped = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(full, 1.0);
            capped->setPricer(
                ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(Handle<OptionletVolatilityStructure>(
                    ext::make_shared<ConstantOptionletVolatility>(today, calendar, Following, 0.05, dayCounter))));
            checkRate("50 bp, capped at 100 %", capped, withSpread);

            // the telescopic coupons build all their value dates, so with a lockout of 2 days the lockout's fixing
            // date is the one of the coupon without telescopic value dates
            checkSameDates("telescopic value dates", telescopic->valueDates(), full->valueDates());
            const auto fullLockout = coupon(0.005, false, 2, RateAveraging::Compound);
            const auto telescopicLockout = coupon(0.005, true, 2, RateAveraging::Compound);
            checkSameDates("telescopic value dates with a lockout", telescopicLockout->valueDates(),
                           fullLockout->valueDates());
            checkSameDates("telescopic fixing dates with a lockout", telescopicLockout->fixingDates(),
                           fullLockout->fixingDates());

            // a zero spread compounded daily, and an averaged coupon, keep their telescopic value dates
            const auto zeroSpread = coupon(0.0, true, 0, RateAveraging::Compound);
            BOOST_CHECK_LT(zeroSpread->valueDates().size(), full->valueDates().size());
            checkRate("zero spread, telescopic value dates", zeroSpread, withoutSpread);
            const auto averaged = coupon(0.005, true, 0, RateAveraging::Simple);
            BOOST_CHECK_LT(averaged->valueDates().size(), full->valueDates().size());
        }
    }
}

BOOST_AUTO_TEST_CASE(testLockoutOnTelescopicValueDates)
{
    BOOST_TEST_MESSAGE("Testing a lockout on telescopic value dates...");

    const Real tolerance = 1.0e-14;
    const auto checkSameDates =
        [](const std::string& name, const std::vector<Date>& calculated, const std::vector<Date>& expected)
    {
        BOOST_TEST_CONTEXT(name)
        {
            BOOST_CHECK_EQUAL_COLLECTIONS(calculated.begin(), calculated.end(), expected.begin(), expected.end());
        }
    };
    // the first and the last dates of a vector
    const auto firstAndLast = [](const std::vector<Date>& dates, Size first, Size last)
    {
        std::vector<Date> result(dates.begin(), dates.begin() + first);
        result.insert(result.end(), dates.end() - last, dates.end());
        return result;
    };

    struct Convention
    {
        Natural lookbackDays, lockoutDays;
        bool applyObservationShift;
    };
    const std::vector<Convention> conventions = {{Null<Natural>(), 0, false}, {Null<Natural>(), 1, false},
                                                 {Null<Natural>(), 2, false}, {Null<Natural>(), 3, false},
                                                 {Null<Natural>(), 5, false}, {2, 2, true}};

    // the leg's coupons, from July 2025 to July 2026, forward-starting, and on 15 July 2025 with SOFR's fixings
    // stored before it
    for (const Date& today : {Date(1, June, 2025), Date(15, July, 2025)})
    {
        CommonVarsONLeg vars(today);
        vars.setupForecastCurve();
        const Handle<OptionletVolatilityStructure> volatility(vars.returnRateVolTS());
        const auto capped = [&](const ext::shared_ptr<OvernightIndexedCoupon>& underlying, bool dailyCapFloor)
        {
            auto c = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(underlying, 0.036, Null<Rate>(), false,
                                                                           dailyCapFloor);
            c->setPricer(ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(volatility));
            return c;
        };

        for (const auto& convention : conventions)
        {
            const Natural lockoutDays = convention.lockoutDays;
            const Leg telescopicLeg = vars.makeLeg(convention.lookbackDays, lockoutDays,
                                                   convention.applyObservationShift, true, RateAveraging::Compound);
            const Leg fullLeg = vars.makeLeg(convention.lookbackDays, lockoutDays, convention.applyObservationShift,
                                             false, RateAveraging::Compound);
            BOOST_REQUIRE_EQUAL(telescopicLeg.size(), fullLeg.size());
            for (Size k = 0; k < fullLeg.size(); ++k)
            {
                const auto telescopic = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(telescopicLeg[k]);
                const auto full = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(fullLeg[k]);
                BOOST_REQUIRE(telescopic && full);
                BOOST_TEST_CONTEXT("today " << today << ", lockout of " << lockoutDays << " days"
                                            << (convention.lookbackDays == Null<Natural>() ?
                                                    "" :
                                                    ", 2-day lookback with the observation shift")
                                            << ", coupon " << k)
                {
                    // the telescopic coupon keeps the other's first value dates and its last lockoutDays + 2, and
                    // so the fixing date its lockout repeats
                    const std::vector<Date>& telescopicDates = telescopic->valueDates();
                    const std::vector<Date>& fullDates = full->valueDates();
                    BOOST_CHECK_LT(telescopicDates.size(), fullDates.size());
                    if (telescopicDates.size() >= lockoutDays + 2 && telescopicDates.size() <= fullDates.size())
                        checkSameDates(
                            "value dates", telescopicDates,
                            firstAndLast(fullDates, telescopicDates.size() - lockoutDays - 2, lockoutDays + 2));
                    const std::vector<Date>& telescopicFixings = telescopic->fixingDates();
                    const std::vector<Date>& fullFixings = full->fixingDates();
                    if (telescopicFixings.size() >= lockoutDays + 1 && telescopicFixings.size() <= fullFixings.size())
                        checkSameDates(
                            "fixing dates", telescopicFixings,
                            firstAndLast(fullFixings, telescopicFixings.size() - lockoutDays - 1, lockoutDays + 1));

                    // the two coupons have the same rate, and so the same rate capped at 3.6 % on the Black
                    // compounding pricer, on the coupon's rate and on each day's
                    CHECK_OIS_COUPON_RESULT("rate", telescopic->rate(), full->rate(), tolerance);
                    if (lockoutDays == 3 && convention.lookbackDays == Null<Natural>())
                    {
                        CHECK_OIS_COUPON_RESULT("rate capped on the coupon's rate", capped(telescopic, false)->rate(),
                                                capped(full, false)->rate(), tolerance);
                        CHECK_OIS_COUPON_RESULT("rate capped on each day's rate", capped(telescopic, true)->rate(),
                                                capped(full, true)->rate(), tolerance);
                    }
                }
            }
        }
    }
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
    expectedRate = 0.036862168;
    BOOST_CHECK(rate <= cap + 1e-8); // Should not exceed cap
    CHECK_OIS_COUPON_RESULT("Capped Rate", rate, expectedRate, 1e-8);

    // Floorlet
    Rate floor = 0.035;
    auto flooredCoupon = vars.makeCoupon(start, end, Null<Rate>(), floor);
    flooredCoupon->setPricer(pricer);
    BOOST_CHECK(!flooredCoupon->isCalculated());

    rate = flooredCoupon->rate();
    expectedRate = 0.042816203;
    BOOST_CHECK(rate >= floor - 1e-8); // Should not be below floor
    CHECK_OIS_COUPON_RESULT("Floored Rate", rate, expectedRate, 1e-8);

    // Capped and Floored
    auto cappedFlooredCoupon = vars.makeCoupon(start, end, cap, floor);
    cappedFlooredCoupon->setPricer(pricer);

    rate = cappedFlooredCoupon->rate();
    expectedRate = 0.039473179;
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
    expectedRate = 0.036745802;
    BOOST_CHECK(rate <= cap + 1e-8);
    CHECK_OIS_COUPON_RESULT("Capped Rate", rate, expectedRate, 1e-8);

    // Floorlet
    Rate floor = 0.035;
    auto flooredCoupon = vars.makeCoupon(start, end, Null<Rate>(), floor, RateAveraging::Simple);
    flooredCoupon->setPricer(pricer);

    rate = flooredCoupon->rate();
    expectedRate = 0.042671405;
    BOOST_CHECK(rate >= floor - 1e-8);
    CHECK_OIS_COUPON_RESULT("Capped Rate", rate, expectedRate, 1e-8);

    // Capped and Floored
    auto cappedFlooredCoupon = vars.makeCoupon(start, end, cap, floor, RateAveraging::Simple);
    cappedFlooredCoupon->setPricer(pricer);

    rate = cappedFlooredCoupon->rate();
    expectedRate = 0.039412858;
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

    Handle<YieldTermStructure> discountCurve(flatRate(0.0015, Actual360()));

    // the leg with a lockout of 3 days, with telescopic value dates and without
    Real expectedNpv = 34638.786842593545;
    for (bool telescopicValueDates : {true, false})
    {
        Leg leg = vars.makeLeg(Null<Natural>(), 3, false, telescopicValueDates, RateAveraging::Compound);

        // Calculate NPV
        Real npv = 0.0;
        for (const auto& cf : leg)
        {
            npv += cf->amount() * discountCurve->discount(cf->date());
        }

        BOOST_TEST_CONTEXT("telescopic value dates: " << std::boolalpha << telescopicValueDates)
        {
            CHECK_OIS_COUPON_RESULT("OvernightLeg NPV", npv, expectedNpv, 1e-8);
        }
    }
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

BOOST_AUTO_TEST_CASE(testOvernightPricersRefuseTheOtherAveragingMethod)
{
    BOOST_TEST_MESSAGE("Testing that the overnight pricers refuse a coupon of the other averaging method...");

    const Date today(9, November, 2026);
    Settings::instance().evaluationDate() = today;
    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    const Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(today, 0.025, dayCounter, Continuous));
    const auto index = ext::make_shared<OvernightIndex>("T432", 0, USDCurrency(), calendar, dayCounter, curve);
    index->addFixing(today, 0.02);
    const Handle<OptionletVolatilityStructure> volatility(
        ext::make_shared<ConstantOptionletVolatility>(today, calendar, Following, 0.05, dayCounter));
    // a coupon from 9 to 13 November, fixing on the day it applies
    const auto coupon = [&](RateAveraging::Type averaging, Natural lockoutDays)
    {
        const Date end(13, November, 2026);
        return ext::make_shared<OvernightIndexedCoupon>(end, 100.0, today, end, index, 1.0, 0.0, Date(), Date(),
                                                        DayCounter(), false, averaging, Null<Natural>(), lockoutDays);
    };
    struct Row
    {
        std::string pricer;
        RateAveraging::Type averaging;
        bool black;
        std::function<ext::shared_ptr<OvernightIndexedCouponPricer>()> make;
    };
    const std::vector<Row> rows = {
        {"CompoundingOvernightIndexedCouponPricer", RateAveraging::Compound, false,
         [] { return ext::make_shared<CompoundingOvernightIndexedCouponPricer>(); }},
        {"BlackCompoundingOvernightIndexedCouponPricer", RateAveraging::Compound, true,
         [&] { return ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(volatility); }},
        {"ArithmeticAveragedOvernightIndexedCouponPricer", RateAveraging::Simple, false,
         [] { return ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>(false); }},
        {"ArithmeticAveragedOvernightIndexedCouponPricer", RateAveraging::Simple, false,
         [] { return ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>(true); }},
        {"BlackAveragingOvernightIndexedCouponPricer", RateAveraging::Simple, true,
         [&] { return ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(volatility); }},
    };
    for (const auto& row : rows)
    {
        const bool compound = row.averaging == RateAveraging::Compound;
        const RateAveraging::Type other = compound ? RateAveraging::Simple : RateAveraging::Compound;
        const std::string message =
            std::string("Averaging method required to be ") + (compound ? "compound" : "simple") + " for " + row.pricer;
        BOOST_TEST_CONTEXT(row.pricer)
        {
            // a coupon of the pricer's own method is priced
            auto own = coupon(row.averaging, 0);
            own->setPricer(row.make());
            BOOST_CHECK_NO_THROW(own->rate());

            // one of the other method is refused, for its rate and for its accrued amount
            auto mismatched = coupon(other, 0);
            mismatched->setPricer(row.make());
            BOOST_CHECK_EXCEPTION(mismatched->rate(), Error, ExpectedErrorMessage(message));
            BOOST_CHECK_EXCEPTION(mismatched->accruedAmount(Date(11, November, 2026)), Error,
                                  ExpectedErrorMessage(message));

            // a compounded coupon with a lockout, which an averaged coupon can't have
            if (!compound)
            {
                auto lockout = coupon(RateAveraging::Compound, 2);
                lockout->setPricer(row.make());
                BOOST_CHECK_EXCEPTION(lockout->rate(), Error, ExpectedErrorMessage(message));
            }

            // a cap on the coupon's rate, and on each day's, set with the capped coupon's own setPricer
            if (row.black)
            {
                for (bool daily : {false, true})
                {
                    auto ownCapped = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                        coupon(row.averaging, 0), 0.03, Null<Rate>(), false, daily);
                    ownCapped->setPricer(row.make());
                    BOOST_CHECK_NO_THROW(ownCapped->rate());
                    auto capped = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(coupon(other, 0), 0.03,
                                                                                        Null<Rate>(), false, daily);
                    capped->setPricer(row.make());
                    BOOST_CHECK_EXCEPTION(capped->rate(), Error, ExpectedErrorMessage(message));
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRateOnItsIndexDayCounter)
{
    BOOST_TEST_MESSAGE("Testing an overnight coupon's rate on its index's day counter, whatever its own...");

    const WeekendsOnly calendar;
    const Actual360 indexDayCounter;
    const Actual365Fixed couponDayCounter;
    RelinkableHandle<YieldTermStructure> curve;
    const auto index = ext::make_shared<OvernightIndex>("T425", 0, USDCurrency(), calendar, indexDayCounter, curve);
    const Date start(1, October, 2026), end(2, November, 2026), middle(16, October, 2026);
    const Real nominal = 10000.0;
    const Time period = indexDayCounter.yearFraction(start, end),
               toMiddle = indexDayCounter.yearFraction(start, middle);

    const auto makeCoupon = [&](const ext::shared_ptr<OvernightIndex>& overnightIndex, const DayCounter& dayCounter,
                                RateAveraging::Type averaging, Natural lockoutDays = 0, Spread spread = 0.0)
    {
        const auto coupon = ext::make_shared<OvernightIndexedCoupon>(
            end, nominal, start, end, overnightIndex, 1.0, spread, Date(), Date(), dayCounter, false, averaging,
            Null<Natural>(), lockoutDays, false, spread != 0.0);
        if (averaging == RateAveraging::Compound)
            coupon->setPricer(ext::make_shared<CompoundingOvernightIndexedCouponPricer>());
        else
            coupon->setPricer(ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>());
        return coupon;
    };
    // a coupon's first n fixings, each at the given rate, compounded or added up over their periods
    const auto accrued = [](const OvernightIndexedCoupon& coupon, Size n, Rate fixing)
    {
        Real compounded = 1.0, added = 0.0;
        for (Size i = 0; i < n; ++i)
        {
            compounded *= 1.0 + fixing * coupon.dt()[i];
            added += fixing * coupon.dt()[i];
        }
        return coupon.averagingMethod() == RateAveraging::Compound ? compounded - 1.0 : added;
    };

    // every fixing stored at 3 %: rates on the index's day counter, amounts on the coupon's
    const Date afterEnd(1, December, 2026);
    Settings::instance().evaluationDate() = afterEnd;
    for (Date d = start; d < end; d = calendar.advance(d, 1, Days))
        index->addFixing(d, 0.03);
    const Handle<OptionletVolatilityStructure> volatility(
        ext::make_shared<ConstantOptionletVolatility>(afterEnd, calendar, Following, 0.2, indexDayCounter));
    for (const DayCounter& dayCounter : std::vector<DayCounter>{couponDayCounter, indexDayCounter})
    {
        BOOST_TEST_CONTEXT("coupon day counter " << dayCounter.name())
        {
            for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
            {
                BOOST_TEST_CONTEXT((averaging == RateAveraging::Compound ? "compounded" : "averaged"))
                {
                    const auto coupon = makeCoupon(index, dayCounter, averaging);
                    const Size n = coupon->dt().size();
                    BOOST_REQUIRE(n == 22 && coupon->valueDates()[11] == middle);
                    const Rate rate = accrued(*coupon, n, 0.03) / period;
                    CHECK_OIS_COUPON_RESULT("rate", coupon->rate(), rate, 1e-12);
                    CHECK_OIS_COUPON_RESULT("amount", coupon->amount(),
                                            nominal * rate * dayCounter.yearFraction(start, end), 1e-9);
                    CHECK_OIS_COUPON_RESULT(
                        "accrued amount", coupon->accruedAmount(middle),
                        nominal * accrued(*coupon, 11, 0.03) / toMiddle * dayCounter.yearFraction(start, middle), 1e-9);

                    // capped daily at 2.9 %, and floored daily at 3.1 %, on the Black pricer
                    for (const Rate bound : {0.029, 0.031})
                    {
                        const bool isCap = bound < 0.03;
                        BOOST_TEST_CONTEXT((isCap ? "capped daily at " : "floored daily at ") << bound)
                        {
                            const auto underlying = makeCoupon(index, dayCounter, averaging);
                            CappedFlooredOvernightIndexedCoupon bounded(underlying, isCap ? bound : Null<Rate>(),
                                                                        isCap ? Null<Rate>() : bound, false, true);
                            bounded.setPricer(makeBlackOvernightPricer(averaging, volatility));
                            CHECK_OIS_COUPON_RESULT("rate", bounded.rate(), accrued(*underlying, n, bound) / period,
                                                    1e-12);
                        }
                    }
                }
            }

            // a spread of 50 bp compounded daily, and the rate's split
            const auto withSpread = makeCoupon(index, dayCounter, RateAveraging::Compound, 0, 0.005);
            const Real compounded = accrued(*withSpread, 22, 0.03 + 0.005),
                       withoutSpread = accrued(*withSpread, 22, 0.03);
            CHECK_OIS_COUPON_RESULT("rate with a spread compounded daily", withSpread->rate(), compounded / period,
                                    1e-12);
            CHECK_OIS_COUPON_RESULT("effective spread", withSpread->effectiveSpread(),
                                    (compounded - withoutSpread) / period, 1e-12);
            CHECK_OIS_COUPON_RESULT("effective index fixing", withSpread->effectiveIndexFixing(),
                                    withoutSpread / period, 1e-12);
        }
    }

    // every fixing forecast: each rate, capped or floored daily or on the period or not at all, is its twin's on the
    // index's day counter
    const Date today(1, September, 2026);
    Settings::instance().evaluationDate() = today;
    index->clearFixings();
    curve.linkTo(ext::make_shared<FlatForward>(today, 0.03, indexDayCounter));
    const Handle<OptionletVolatilityStructure> forwardVolatility(
        ext::make_shared<ConstantOptionletVolatility>(today, calendar, Following, 0.2, indexDayCounter));
    const auto fixingADayBefore =
        ext::make_shared<OvernightIndex>("T425F", 1, USDCurrency(), calendar, indexDayCounter, curve);
    struct Row
    {
        std::string name;
        ext::shared_ptr<OvernightIndex> index;
        RateAveraging::Type averaging;
        Natural lockoutDays;
    };
    const std::vector<Row> rows = {
        {"compounded", index, RateAveraging::Compound, 0},
        {"compounded, lockout of 2 days", index, RateAveraging::Compound, 2},
        {"averaged", index, RateAveraging::Simple, 0},
        {"averaged, fixing a day before", fixingADayBefore, RateAveraging::Simple, 0},
    };
    for (const auto& row : rows)
        for (const Rate bound : {0.029, 0.031})
            for (bool daily : {true, false})
            {
                const bool isCap = bound < 0.03;
                BOOST_TEST_CONTEXT(row.name << (isCap ? ", cap " : ", floor ") << bound
                                            << (daily ? " daily" : " on the period"))
                {
                    const auto make = [&](const DayCounter& dayCounter)
                    {
                        const auto underlying = makeCoupon(row.index, dayCounter, row.averaging, row.lockoutDays);
                        const auto bounded = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                            underlying, isCap ? bound : Null<Rate>(), isCap ? Null<Rate>() : bound, false, daily);
                        bounded->setPricer(makeBlackOvernightPricer(row.averaging, forwardVolatility));
                        return bounded;
                    };
                    const auto coupon = make(couponDayCounter), twin = make(indexDayCounter);
                    BOOST_CHECK_EQUAL(coupon->underlying()->rate(), twin->underlying()->rate());
                    BOOST_CHECK_EQUAL(coupon->rate(), twin->rate());
                }
            }
}

BOOST_AUTO_TEST_CASE(testRateOverTheObservationPeriodUnderObservationShift)
{
    BOOST_TEST_MESSAGE("Testing an overnight coupon's rate over its observation period under observation shift...");

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    RelinkableHandle<YieldTermStructure> curve;
    const auto index = ext::make_shared<OvernightIndex>("T424", 0, USDCurrency(), calendar, dayCounter, curve);
    const auto fixingTwoDaysBefore =
        ext::make_shared<OvernightIndex>("T424F", 2, USDCurrency(), calendar, dayCounter, curve);
    // accruing from 1 October to 2 November 2026 with a lookback of 2 days, a coupon observes the fixings from
    // 29 September to 29 October
    const Date start(1, October, 2026), end(2, November, 2026);
    const Date observationStart(29, September, 2026), observationEnd(29, October, 2026);
    const Real nominal = 10000.0;

    const auto makeCoupon = [&](const ext::shared_ptr<OvernightIndex>& overnightIndex, const Date& from, const Date& to,
                                Natural lookbackDays, bool shift, Natural lockoutDays = 0, Spread spread = 0.0,
                                bool telescopicValueDates = false)
    {
        const auto coupon = ext::make_shared<OvernightIndexedCoupon>(
            to, nominal, from, to, overnightIndex, 1.0, spread, Date(), Date(), dayCounter, telescopicValueDates,
            RateAveraging::Compound, lookbackDays, lockoutDays, shift, spread != 0.0);
        coupon->setPricer(ext::make_shared<CompoundingOvernightIndexedCouponPricer>());
        return coupon;
    };
    // the coupon with a lookback of 2 days and observation shift
    const auto shifted = [&](Natural lockoutDays = 0, Spread spread = 0.0, bool telescopicValueDates = false)
    { return makeCoupon(index, start, end, 2, true, lockoutDays, spread, telescopicValueDates); };
    // the coupon from 29 September to o without a lookback; to 29 October, the shifted coupon's twin
    const auto observing = [&](const Date& o, Natural lockoutDays = 0, Spread spread = 0.0)
    { return makeCoupon(index, observationStart, o, Null<Natural>(), false, lockoutDays, spread); };
    // the observation date of the first accrual day on or after d
    const auto observed = [&](const Date& d) { return calendar.advance(calendar.adjust(d), -2, Days); };
    // every fixing at the given rate, compounded over the observation period's days and annualised over them
    const auto compoundedRate = [&](Rate fixing)
    {
        Real compounded = 1.0;
        for (Date d = observationStart; d < observationEnd; d = calendar.advance(d, 1, Days))
            compounded *= 1.0 + fixing * dayCounter.yearFraction(d, calendar.advance(d, 1, Days));
        return (compounded - 1.0) / dayCounter.yearFraction(observationStart, observationEnd);
    };
    // capped at a bound below 3 %, or floored at one above, on the Black pricer
    const auto bounded = [](const ext::shared_ptr<OvernightIndexedCoupon>& underlying, Rate bound, bool daily,
                            const Handle<OptionletVolatilityStructure>& volatility)
    {
        const bool isCap = bound < 0.03;
        const auto coupon = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
            underlying, isCap ? bound : Null<Rate>(), isCap ? Null<Rate>() : bound, false, daily);
        coupon->setPricer(makeBlackOvernightPricer(RateAveraging::Compound, volatility));
        return coupon;
    };

    // every fixing stored at 3 %
    const Date afterEnd(1, December, 2026);
    Settings::instance().evaluationDate() = afterEnd;
    for (Date d(28, September, 2026); d < end; d = calendar.advance(d, 1, Days))
    {
        index->addFixing(d, 0.03);
        fixingTwoDaysBefore->addFixing(d, 0.03);
    }
    const Handle<OptionletVolatilityStructure> volatility(
        ext::make_shared<ConstantOptionletVolatility>(afterEnd, calendar, Following, 0.2, dayCounter));
    BOOST_TEST_CONTEXT("every fixing stored")
    {
        // the shifted coupon has its twin's rate, and accrues to each day what the coupon over the observation
        // period to that day's observation date pays
        const auto coupon = shifted();
        BOOST_CHECK_EQUAL(coupon->rate(), observing(observationEnd)->rate());
        CHECK_OIS_COUPON_RESULT("rate", coupon->rate(), compoundedRate(0.03), 1e-12);
        CHECK_OIS_COUPON_RESULT("amount", coupon->amount(),
                                nominal * compoundedRate(0.03) * dayCounter.yearFraction(start, end), 1e-9);
        for (Date d(2, October, 2026); d <= end; ++d)
        {
            BOOST_TEST_CONTEXT("accrued to " << d)
            {
                BOOST_CHECK_EQUAL(coupon->accruedAmount(d),
                                  nominal * observing(observed(d))->rate() * coupon->accruedPeriod(d));
            }
        }

        // a spread of 50 bp compounded daily, and the rate's split
        const auto withSpread = shifted(0, 0.005), twinWithSpread = observing(observationEnd, 0, 0.005);
        BOOST_CHECK_EQUAL(withSpread->rate(), twinWithSpread->rate());
        BOOST_CHECK_EQUAL(withSpread->effectiveSpread(), twinWithSpread->effectiveSpread());
        BOOST_CHECK_EQUAL(withSpread->effectiveIndexFixing(), twinWithSpread->effectiveIndexFixing());
        CHECK_OIS_COUPON_RESULT("rate with a spread compounded daily", withSpread->rate(), compoundedRate(0.03 + 0.005),
                                1e-12);
        CHECK_OIS_COUPON_RESULT("effective spread", withSpread->effectiveSpread(),
                                (compoundedRate(0.03 + 0.005) - compoundedRate(0.03)), 1e-12);

        // capped daily at 2.9 %, and floored daily at 3.1 %
        for (const Rate bound : {0.029, 0.031})
        {
            BOOST_TEST_CONTEXT((bound < 0.03 ? "capped daily at " : "floored daily at ") << bound)
            {
                const auto boundedCoupon = bounded(shifted(), bound, true, volatility);
                BOOST_CHECK_EQUAL(boundedCoupon->rate(),
                                  bounded(observing(observationEnd), bound, true, volatility)->rate());
                CHECK_OIS_COUPON_RESULT("rate", boundedCoupon->rate(), compoundedRate(bound), 1e-12);
            }
        }

        // with a lockout of 2 days
        BOOST_CHECK_EQUAL(shifted(2)->rate(), observing(observationEnd, 2)->rate());

        // on an index fixing two days before the day its rate is for, a lookback of 2 days and the shift leave the
        // coupon as it is without the shift on the days its fixings are for
        const auto withShift = makeCoupon(fixingTwoDaysBefore, start, end, 2, true),
                   withoutShift = makeCoupon(fixingTwoDaysBefore, start, end, 2, false);
        BOOST_CHECK_EQUAL(withShift->rate(), withoutShift->rate());
        for (Date d(2, October, 2026); d <= end; d = calendar.advance(d, 1, Days))
        {
            BOOST_TEST_CONTEXT("index fixing two days before, accrued to " << d)
            {
                BOOST_CHECK_EQUAL(withShift->accruedAmount(d), withoutShift->accruedAmount(d));
            }
        }

        // without a lookback, or with a lookback of 0 days on an index fixing two days before, the shift leaves the
        // coupon as it is without it
        struct Row
        {
            std::string name;
            ext::shared_ptr<OvernightIndex> index;
            Natural lookbackDays;
        };
        const std::vector<Row> rows = {
            {"no lookback", index, Null<Natural>()},
            {"index fixing two days before, lookback of 0 days", fixingTwoDaysBefore, 0},
        };
        for (const auto& row : rows)
        {
            BOOST_TEST_CONTEXT(row.name)
            {
                const auto withShift = makeCoupon(row.index, start, end, row.lookbackDays, true),
                           withoutShift = makeCoupon(row.index, start, end, row.lookbackDays, false);
                BOOST_CHECK_EQUAL(withShift->rate(), withoutShift->rate());
                for (Date d(2, October, 2026); d <= end; ++d)
                {
                    BOOST_TEST_CONTEXT("accrued to " << d)
                    {
                        BOOST_CHECK_EQUAL(withShift->accruedAmount(d), withoutShift->accruedAmount(d));
                    }
                }
                for (const Rate bound : {0.029, 0.031})
                {
                    BOOST_TEST_CONTEXT((bound < 0.03 ? "capped daily at " : "floored daily at ") << bound)
                    {
                        BOOST_CHECK_EQUAL(bounded(withShift, bound, true, volatility)->rate(),
                                          bounded(withoutShift, bound, true, volatility)->rate());
                    }
                }
            }
        }

        // starting on a Saturday, its accrued amount to the Sunday is finite
        const auto fromSaturday = makeCoupon(index, Date(3, October, 2026), end, 2, true);
        BOOST_CHECK(std::isfinite(fromSaturday->accruedAmount(Date(4, October, 2026))));
    }

    // today 30 September, its fixing and the one before stored
    BOOST_TEST_CONTEXT("today 30 September")
    {
        const Date today(30, September, 2026), sunday(4, October, 2026);
        Settings::instance().evaluationDate() = today;
        index->clearFixings();
        index->addFixing(Date(29, September, 2026), 0.03);
        index->addFixing(today, 0.03);
        curve.linkTo(ext::make_shared<FlatForward>(today, 0.03, dayCounter));
        const auto coupon = shifted();
        BOOST_CHECK_EQUAL(coupon->accruedAmount(sunday),
                          nominal * observing(observed(sunday))->rate() * coupon->accruedPeriod(sunday));
    }

    // every fixing forecast from a flat curve
    const Date today(1, September, 2026);
    Settings::instance().evaluationDate() = today;
    index->clearFixings();
    curve.linkTo(ext::make_shared<FlatForward>(today, 0.03, dayCounter));
    const Handle<OptionletVolatilityStructure> forwardVolatility(
        ext::make_shared<ConstantOptionletVolatility>(today, calendar, Following, 0.2, dayCounter));
    BOOST_TEST_CONTEXT("every fixing forecast")
    {
        const auto coupon = shifted(), twin = observing(observationEnd);
        BOOST_CHECK_EQUAL(coupon->rate(), twin->rate());
        CHECK_OIS_COUPON_RESULT(
            "rate", coupon->rate(),
            (curve->discount(observationStart) / curve->discount(observationEnd) - 1.0) * 360.0 / 30, 1e-12);
        for (const Date& d : {Date(2, October, 2026), Date(5, October, 2026), Date(9, October, 2026),
                              Date(16, October, 2026), Date(30, October, 2026), end})
        {
            BOOST_TEST_CONTEXT("accrued to " << d)
            {
                const Date o = observed(d);
                BOOST_CHECK_EQUAL(coupon->accruedAmount(d), nominal * observing(o)->rate() * coupon->accruedPeriod(d));
                CHECK_OIS_COUPON_RESULT("accrued amount", coupon->accruedAmount(d),
                                        nominal * (curve->discount(observationStart) / curve->discount(o) - 1.0) /
                                            dayCounter.yearFraction(observationStart, o) *
                                            dayCounter.yearFraction(start, d),
                                        1e-12);
            }
        }
        // to a Sunday, within rounding of what the coupon over the observation period to 1 October pays
        const Date sunday(4, October, 2026);
        CHECK_OIS_COUPON_RESULT("accrued amount to a Sunday", coupon->accruedAmount(sunday),
                                nominal * observing(observed(sunday))->rate() * coupon->accruedPeriod(sunday), 1e-12);

        // capped at 2.9 %, and floored at 3.1 %, on each day's rate and on the period's
        for (const Rate bound : {0.029, 0.031})
            for (bool daily : {true, false})
            {
                BOOST_TEST_CONTEXT((bound < 0.03 ? "cap " : "floor ") << bound << (daily ? " daily" : " on the period"))
                {
                    BOOST_CHECK_EQUAL(bounded(coupon, bound, daily, forwardVolatility)->rate(),
                                      bounded(twin, bound, daily, forwardVolatility)->rate());
                }
            }

        // with a lockout of 2 days: the rate, and the accrued amount to the Sunday before the end
        const auto lockedOut = shifted(2), lockedOutTwin = observing(observationEnd, 2);
        const Date lastSunday(1, November, 2026);
        BOOST_CHECK_EQUAL(lockedOut->rate(), lockedOutTwin->rate());
        BOOST_CHECK_EQUAL(lockedOut->accruedAmount(lastSunday),
                          nominal * lockedOutTwin->rate() * lockedOut->accruedPeriod(lastSunday));

        // with telescopic value dates
        BOOST_CHECK_EQUAL(shifted(0, 0.0, true)->rate(), coupon->rate());
    }
}

BOOST_AUTO_TEST_CASE(testAveragedForecastUpToTheDate)
{
    BOOST_TEST_MESSAGE("Testing that an averaged overnight coupon accrues its forecast fixings only up to the date...");

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    RelinkableHandle<YieldTermStructure> curve;
    const Date start(1, October, 2026), end(20, November, 2026), sunday(1, November, 2026);
    const Real nominal = 10000.0;

    // on an index fixing on the day its rate is for, and on one fixing 2 days before it
    for (const Natural fixingDays : {0U, 2U})
    {
        const auto index = ext::make_shared<OvernightIndex>(fixingDays == 0 ? "T434" : "T434F", fixingDays,
                                                            USDCurrency(), calendar, dayCounter, curve);

        // each fixing, stored or forecast, over its period up to the date
        const auto accrued = [&](const Date& date)
        {
            Real sum = 0.0;
            for (Date d = start; d < date; d = calendar.advance(d, 1, Days))
                sum += index->fixing(index->fixingDate(d)) *
                       dayCounter.yearFraction(d, std::min(calendar.advance(d, 1, Days), date));
            return nominal * sum;
        };

        // no fixing stored; the fixings from Thursday 24 September up to Thursday 15 October stored at 2.1 %; and up to
        // Thursday 29 October
        for (const Date& today : {Date(1, September, 2026), Date(16, October, 2026), Date(30, October, 2026)})
        {
            Settings::instance().evaluationDate() = today;
            // a forward of 2 % up to Monday 2 November and 3 % after it
            curve.linkTo(ext::make_shared<ForwardCurve>(
                std::vector<Date>{today, Date(2, November, 2026), Date(1, January, 2027)},
                std::vector<Rate>{0.02, 0.02, 0.03}, dayCounter));
            index->clearFixings();
            for (Date d = calendar.advance(start, -5, Days); d < today; d = calendar.advance(d, 1, Days))
                index->addFixing(d, 0.021);
            for (bool telescopicValueDates : {false, true})
            {
                BOOST_TEST_CONTEXT("fixing " << (fixingDays == 0 ? "on the day" : "2 days before") << ", today "
                                             << today << (telescopicValueDates ? ", telescopic" : ", dense")
                                             << " value dates")
                {
                    // Telescopic value dates take the forecast from the log of a ratio of discount factors, which
                    // falls short of the forecasts over their periods by up to 2.0e-3 on these coupons.
                    const Real tolerance = telescopicValueDates ? 1e-2 : 1e-9;
                    const OvernightIndexedCoupon coupon(end, nominal, start, end, index, 1.0, 0.0, Date(), Date(),
                                                        dayCounter, telescopicValueDates, RateAveraging::Simple);
                    for (Date d = start + 1; d <= end; ++d)
                    {
                        BOOST_TEST_CONTEXT("accrued to " << d)
                        {
                            CHECK_OIS_COUPON_RESULT("accrued amount", coupon.accruedAmount(d), accrued(d), tolerance);
                        }
                    }

                    // a coupon ending on Sunday 1 November
                    const OvernightIndexedCoupon toSunday(sunday, nominal, start, sunday, index, 1.0, 0.0, Date(),
                                                          Date(), dayCounter, telescopicValueDates,
                                                          RateAveraging::Simple);
                    CHECK_OIS_COUPON_RESULT("amount to a Sunday", toSunday.amount(), accrued(sunday), tolerance);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testDailyCapsAndFloorsOverTheCouponsDays)
{
    BOOST_TEST_MESSAGE("Testing daily caps and floors on an overnight coupon over the days it accrues its fixings...");

    const Actual360 dayCounter;
    RelinkableHandle<YieldTermStructure> curve;
    const WeekendsOnly weekends;
    const auto estr = ext::make_shared<Estr>(curve);
    // on a weekends-only calendar, Actual/360, fixing on the day it applies and a day before it
    const auto sameDay = ext::make_shared<OvernightIndex>("T438", 0, USDCurrency(), weekends, dayCounter, curve);
    const auto dayBefore = ext::make_shared<OvernightIndex>("T438D", 1, USDCurrency(), weekends, dayCounter, curve);

    // Every fixing, stored at 3 % or forecast from a flat curve of 3 %, is above the cap and below the floor, so at
    // zero volatility a coupon capped or floored on each day's rate has the rate of the same coupon with every fixing
    // at the cap or floor: that coupon's, on an index with the same calendar and fixing days storing them, priced
    // after its end.
    const Rate fixing = 0.03, cap = 0.025, floor = 0.035;

    struct Row
    {
        std::string name;
        ext::shared_ptr<OvernightIndex> index;
        Date start, end;
        RateAveraging::Type averaging;
        Natural lookbackDays, lockoutDays;
        bool applyObservationShift;
    };
    const Natural none = Null<Natural>();
    const auto compound = RateAveraging::Compound, simple = RateAveraging::Simple;
    const Date october(1, October, 2026), sunday(1, November, 2026), monday(2, November, 2026),
        saturday(31, October, 2026), december(1, December, 2026), christmas(25, December, 2026);
    const std::vector<Row> rows = {
        // on ESTR, to a TARGET holiday, to a Sunday and to a fixing day
        {"compounded to Friday 25 December", estr, december, christmas, compound, none, 0, false},
        {"averaged to Friday 25 December", estr, december, christmas, simple, none, 0, false},
        {"compounded to Sunday 1 November", estr, october, sunday, compound, none, 0, false},
        {"averaged to Sunday 1 November", estr, october, sunday, simple, none, 0, false},
        {"compounded to Monday 2 November", estr, october, monday, compound, none, 0, false},
        {"averaged to Monday 2 November", estr, october, monday, simple, none, 0, false},
        // with a lookback of 2 days, without and with the observation shift
        {"lookback", sameDay, october, monday, compound, 2, 0, false},
        {"lookback to Sunday 1 November", sameDay, october, sunday, compound, 2, 0, false},
        {"lookback and lockout", sameDay, october, monday, compound, 2, 2, false},
        {"lookback and shift", sameDay, october, monday, compound, 2, 0, true},
        {"lookback and shift to Sunday 1 November", sameDay, october, sunday, compound, 2, 0, true},
        // from a Saturday
        {"lockout from Saturday 31 October", sameDay, saturday, december, compound, none, 2, false},
        {"averaged from Saturday 31 October, fixing a day before", dayBefore, saturday, december, simple, none, 0,
         false}};

    const auto coupon = [&](const Row& r, const ext::shared_ptr<OvernightIndex>& index)
    {
        return ext::make_shared<OvernightIndexedCoupon>(r.end, 10000.0, r.start, r.end, index, 1.0, 0.0, Date(), Date(),
                                                        dayCounter, false, r.averaging, r.lookbackDays, r.lockoutDays,
                                                        r.applyObservationShift);
    };
    const auto store = [](const ext::shared_ptr<OvernightIndex>& index, const Date& until, Rate rate)
    {
        index->clearFixings();
        for (Date d(1, September, 2026); d <= until; ++d)
            if (index->isValidFixingDate(d))
                index->addFixing(d, rate);
    };

    for (const auto& r : rows)
    {
        const auto twin =
            ext::make_shared<OvernightIndex>(r.index->familyName() + "B", r.index->fixingDays(), r.index->currency(),
                                             r.index->fixingCalendar(), r.index->dayCounter());
        Settings::instance().evaluationDate() = Date(1, March, 2027);
        store(twin, r.end, cap);
        const Rate capped = coupon(r, twin)->rate();
        store(twin, r.end, floor);
        const Rate floored = coupon(r, twin)->rate();

        // today 1 September, no fixing stored: the pricers take the fixings to come as one average rate, compounded or
        // added up daily; the day of the last fixing, with it to come and stored; and after the end
        struct Pricing
        {
            Date today, storedUntil;
            Real tolerance;
        };
        const Date last = coupon(r, r.index)->fixingDates().back();
        const Date after = r.index->fixingCalendar().advance(r.end, 2, Days);
        for (const auto& [today, storedUntil, tolerance] :
             {Pricing{Date(1, September, 2026), Date(31, August, 2026), 1.0e-5}, Pricing{last, last - 1, 1.0e-6},
              Pricing{last, last, 1.0e-12}, Pricing{after, after, 1.0e-12}})
        {
            Settings::instance().evaluationDate() = today;
            curve.linkTo(ext::make_shared<FlatForward>(today, fixing, dayCounter, Continuous));
            store(r.index, storedUntil, fixing);
            const Handle<OptionletVolatilityStructure> volatility(
                ext::make_shared<ConstantOptionletVolatility>(today, weekends, Following, 0.0, dayCounter));
            for (bool isCap : {true, false})
            {
                BOOST_TEST_CONTEXT(r.name << (isCap ? ", capped" : ", floored") << " daily, today " << today
                                          << ", fixings stored up to " << storedUntil)
                {
                    auto c = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                        coupon(r, r.index), isCap ? cap : Null<Rate>(), isCap ? Null<Rate>() : floor, false, true);
                    if (r.averaging == compound)
                        c->setPricer(ext::make_shared<BlackCompoundingOvernightIndexedCouponPricer>(volatility));
                    else
                        c->setPricer(ext::make_shared<BlackAveragingOvernightIndexedCouponPricer>(volatility));
                    const Rate expected = isCap ? capped : floored;
                    CHECK_OIS_COUPON_RESULT("coupon rate", c->rate(), expected, tolerance);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRateFromAStartThatIsNotAFixingDay)
{
    BOOST_TEST_MESSAGE("Testing overnight coupons starting on a day that isn't a fixing day...");

    const TARGET calendar;
    const Actual360 dayCounter;
    RelinkableHandle<YieldTermStructure> curve;
    const auto index = ext::make_shared<Estr>(curve);
    const Real nominal = 10000.0;

    // each fixing, from the one on the fixing day before the start, accrues from its day or the start, whichever is
    // later, to the next fixing day or the date, whichever is earlier
    const auto accrue = [&](const Date& start, const Date& date, bool compound)
    {
        Real factor = 1.0, sum = 0.0;
        for (Date d = calendar.adjust(start, Preceding); d < date; d = calendar.advance(d, 1, Days))
        {
            const Time t = dayCounter.yearFraction(std::max(d, start), std::min(calendar.advance(d, 1, Days), date));
            factor *= 1.0 + index->fixing(d) * t;
            sum += index->fixing(d) * t;
        }
        return (compound ? factor - 1.0 : sum) / dayCounter.yearFraction(start, date);
    };
    const auto makeCoupon = [&](const Date& start, const Date& end, RateAveraging::Type averaging, bool telescopic,
                                Natural lookbackDays = Null<Natural>(), bool applyObservationShift = false)
    {
        return ext::make_shared<OvernightIndexedCoupon>(end, nominal, start, end, index, 1.0, 0.0, Date(), Date(),
                                                        dayCounter, telescopic, averaging, lookbackDays, 0,
                                                        applyObservationShift);
    };
    const auto check = [](const std::string& what, Real calculated, Real expected, Real tolerance)
    {
        if (!(std::fabs(calculated - expected) <= tolerance))
            BOOST_ERROR("Failed to reproduce " << what << ":\n    expected:   " << std::setprecision(15) << expected
                                               << "\n    calculated: " << calculated
                                               << "\n    error:      " << std::fabs(calculated - expected));
    };

    // Friday 1 May 2026 and Good Friday, 3 April, aren't TARGET business days
    const Date may(1, May, 2026), june(1, June, 2026), goodFriday(3, April, 2026), mayMonday(4, May, 2026);
    for (const Date& today : {Date(1, April, 2026), Date(15, May, 2026)})
    {
        Settings::instance().evaluationDate() = today;
        curve.linkTo(ext::make_shared<FlatForward>(today, 0.03, dayCounter));
        index->clearFixings();
        for (Date d(1, April, 2026); d < today; d = calendar.advance(d, 1, Days))
            index->addFixing(d, 0.029);

        for (const auto& [startDate, endDate] : {std::pair(may, june), std::pair(goodFriday, mayMonday)})
        {
            std::ostringstream label;
            label << "the coupon from " << startDate << " to " << endDate << " on " << today;
            const auto dense = makeCoupon(startDate, endDate, RateAveraging::Compound, false);
            const auto telescopic = makeCoupon(startDate, endDate, RateAveraging::Compound, true);
            const auto averaged = makeCoupon(startDate, endDate, RateAveraging::Simple, false);
            const auto approximated = makeCoupon(startDate, endDate, RateAveraging::Simple, true);
            const Rate compounded = accrue(startDate, endDate, true), simple = accrue(startDate, endDate, false);
            check(label.str() + ", its rate", dense->rate(), compounded, 1e-12);
            check(label.str() + " on telescopic value dates, its rate", telescopic->rate(), compounded, 1e-12);
            check(label.str() + " averaged, its rate", averaged->rate(), simple, 1e-12);
            check(label.str() + " averaged by approximation, its rate", approximated->rate(), simple, 1e-5);
            for (Date d = startDate + 1; d <= endDate; ++d)
            {
                std::ostringstream at;
                at << " at " << d;
                const Time period = dayCounter.yearFraction(startDate, d);
                const Real compoundedAmount = nominal * accrue(startDate, d, true) * period,
                           simpleAmount = nominal * accrue(startDate, d, false) * period;
                check(label.str() + ", its accrued amount" + at.str(), dense->accruedAmount(d), compoundedAmount, 1e-9);
                check(label.str() + " averaged, its accrued amount" + at.str(), averaged->accruedAmount(d),
                      simpleAmount, 1e-9);
                if (d <= dense->interestDates()[1])
                {
                    check(label.str() + " on telescopic value dates, its accrued amount" + at.str(),
                          telescopic->accruedAmount(d), compoundedAmount, 1e-9);
                    check(label.str() + " averaged by approximation, its accrued amount" + at.str(),
                          approximated->accruedAmount(d), simpleAmount, 1e-9);
                }
            }
        }
    }

    // with a lookback of 2 days and the observation shift, the coupon from 1 May has the rate of its observation
    // period, from Wednesday 29 April to Thursday 28 May, and accrues nothing up to its first business day
    const Date today(1, April, 2026);
    Settings::instance().evaluationDate() = today;
    curve.linkTo(ext::make_shared<FlatForward>(today, 0.03, dayCounter));
    index->clearFixings();
    const auto shifted = makeCoupon(may, june, RateAveraging::Compound, false, 2, true);
    const Date observationStart(29, April, 2026), observationEnd(28, May, 2026);
    BOOST_CHECK_EQUAL(shifted->rate(),
                      makeCoupon(observationStart, observationEnd, RateAveraging::Compound, false)->rate());
    for (Date d = may + 1; d <= june; ++d)
    {
        const Date observed = calendar.advance(calendar.adjust(d), -2, Days);
        const Real expected = observed == observationStart ?
                                  0.0 :
                                  nominal *
                                      makeCoupon(observationStart, observed, RateAveraging::Compound, false)->rate() *
                                      shifted->accruedPeriod(d);
        std::ostringstream at;
        at << "the shifted coupon's accrued amount at " << d;
        check(at.str(), shifted->accruedAmount(d), expected, 1e-9);
    }

    // from Saturday 2 May to Monday 4 May a coupon holds no fixing day: it takes the fixing of Thursday 30 April, the
    // fixing day before its start, and with a lookback of 2 days the fixing of Tuesday 28 April
    const Date saturday(2, May, 2026), sunday(3, May, 2026);
    struct Kind
    {
        std::string name;
        RateAveraging::Type averaging;
        bool telescopic;
        Natural lookbackDays;
        Date fixingDate;
    };
    const std::vector<Kind> kinds = {
        {"", RateAveraging::Compound, false, Null<Natural>(), Date(30, April, 2026)},
        {" on telescopic value dates", RateAveraging::Compound, true, Null<Natural>(), Date(30, April, 2026)},
        {" averaged", RateAveraging::Simple, false, Null<Natural>(), Date(30, April, 2026)},
        {" averaged by approximation", RateAveraging::Simple, true, Null<Natural>(), Date(30, April, 2026)},
        {" with a lookback of 2 days", RateAveraging::Compound, false, 2, Date(28, April, 2026)}};
    for (const Date& evaluationDate : {Date(1, April, 2026), Date(15, May, 2026)})
    {
        Settings::instance().evaluationDate() = evaluationDate;
        curve.linkTo(ext::make_shared<FlatForward>(evaluationDate, 0.03, dayCounter));
        index->clearFixings();
        for (Date d(1, April, 2026); d < evaluationDate; d = calendar.advance(d, 1, Days))
            index->addFixing(d, 0.029);
        for (const auto& k : kinds)
        {
            std::ostringstream label;
            label << "the coupon from " << saturday << " to " << mayMonday << k.name << " on " << evaluationDate;
            ext::shared_ptr<OvernightIndexedCoupon> coupon;
            BOOST_CHECK_NO_THROW(coupon = makeCoupon(saturday, mayMonday, k.averaging, k.telescopic, k.lookbackDays));
            if (!coupon)
                continue;
            const Rate fixing = index->fixing(k.fixingDate);
            check(label.str() + ", its rate", coupon->rate(), fixing, 1e-12);
            for (const Date& d : {sunday, mayMonday})
            {
                std::ostringstream at;
                at << ", its accrued amount at " << d;
                check(label.str() + at.str(), coupon->accruedAmount(d),
                      nominal * fixing * dayCounter.yearFraction(saturday, d), 1e-9);
            }
        }
    }

    // the coupon from Friday 1 May takes the fixing of Thursday 30 April for its first three days: priced on 15 May
    // with the fixings stored from Monday 4 May only, it misses that fixing
    const Date fifteenth(15, May, 2026);
    Settings::instance().evaluationDate() = fifteenth;
    curve.linkTo(ext::make_shared<FlatForward>(fifteenth, 0.03, dayCounter));
    index->clearFixings();
    for (Date d = mayMonday; d < fifteenth; d = calendar.advance(d, 1, Days))
        index->addFixing(d, 0.029);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
        for (bool telescopic : {false, true})
            BOOST_CHECK_EXCEPTION(makeCoupon(may, june, averaging, telescopic)->rate(), Error,
                                  ExpectedErrorMessage("fixing for April 30th, 2026"));
}

BOOST_AUTO_TEST_CASE(testRateOverTheRateComputationPeriod)
{
    BOOST_TEST_MESSAGE("Testing overnight coupons over a rate computation period other than their accrual period...");

    const TARGET calendar;
    const Actual360 dayCounter;
    // 2 % up to 1 October 2026, then 3 %
    const Handle<YieldTermStructure> curve(ext::make_shared<ForwardCurve>(
        std::vector<Date>{Date(1, April, 2026), Date(1, October, 2026), Date(1, April, 2027), Date(1, April, 2028)},
        std::vector<Rate>{0.02, 0.02, 0.03, 0.04}, dayCounter));
    const auto index = ext::make_shared<Estr>(curve);
    const Real nominal = 10000.0;
    const Schedule schedule = MakeSchedule()
                                  .from(Date(4, May, 2026))
                                  .to(Date(4, May, 2027))
                                  .withTenor(3 * Months)
                                  .withCalendar(calendar)
                                  .withConvention(Following);
    const auto overThePeriod = [&](const Date& start, const Date& end, RateAveraging::Type averaging, bool telescopic)
    {
        return ext::make_shared<OvernightIndexedCoupon>(end, nominal, start, end, index, 1.0, 0.0, Date(), Date(),
                                                        dayCounter, telescopic, averaging);
    };
    const auto accrue = [&](const Date& start, const Date& date, bool compound)
    {
        Real factor = 1.0, sum = 0.0;
        for (Date d = start; d < date; d = calendar.advance(d, 1, Days))
        {
            const Time t = dayCounter.yearFraction(d, std::min(calendar.advance(d, 1, Days), date));
            factor *= 1.0 + index->fixing(d) * t;
            sum += index->fixing(d) * t;
        }
        return (compound ? factor - 1.0 : sum) / dayCounter.yearFraction(start, date);
    };
    const auto check = [](const std::string& what, Real calculated, Real expected, Real tolerance)
    {
        if (!(std::fabs(calculated - expected) <= tolerance))
            BOOST_ERROR("Failed to reproduce " << what << ":\n    expected:   " << std::setprecision(15) << expected
                                               << "\n    calculated: " << calculated
                                               << "\n    error:      " << std::fabs(calculated - expected));
    };

    // the second coupon, from 4 August to 4 November 2026
    struct Row
    {
        std::string name;
        bool inArrears;
        ext::optional<Period> lastRecentPeriod;
        Date rateStart, rateEnd;
    };
    const std::vector<Row> rows = {
        {"fixed in advance", false, ext::nullopt, Date(4, May, 2026), Date(4, August, 2026)},
        {"over a last recent period of one month", true, 1 * Months, Date(5, October, 2026), Date(4, November, 2026)}};
    for (const Date& today : {Date(1, April, 2026), Date(1, July, 2026)})
    {
        Settings::instance().evaluationDate() = today;
        index->clearFixings();
        for (Date d(4, May, 2026); d < today; d = calendar.advance(d, 1, Days))
            index->addFixing(d, 0.021);
        for (const auto& r : rows)
            for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
                for (bool telescopic : {false, true})
                {
                    const Leg leg = OvernightLeg(schedule, index)
                                        .withNotionals(nominal)
                                        .withPaymentDayCounter(dayCounter)
                                        .withAveragingMethod(averaging)
                                        .withTelescopicValueDates(telescopic)
                                        .inArrears(r.inArrears)
                                        .withLastRecentPeriod(r.lastRecentPeriod);
                    const auto coupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(leg[1]);
                    BOOST_REQUIRE(coupon);
                    std::ostringstream label;
                    label << "the coupon " << r.name << (telescopic ? " on telescopic value dates" : "")
                          << (averaging == RateAveraging::Compound ? ", compounded," : ", averaged,") << " on "
                          << today;
                    BOOST_CHECK_EQUAL(coupon->rateComputationStartDate(), r.rateStart);
                    BOOST_CHECK_EQUAL(coupon->rateComputationEndDate(), r.rateEnd);
                    const Rate expected = overThePeriod(r.rateStart, r.rateEnd, averaging, telescopic)->rate();
                    if (coupon->rate() != expected)
                        BOOST_ERROR("Failed to reproduce " << label.str()
                                                           << ", its rate:\n    expected:   " << std::setprecision(17)
                                                           << expected << "\n    calculated: " << coupon->rate());
                    // on telescopic value dates, an averaged rate is approximated
                    const Real tolerance = telescopic && averaging == RateAveraging::Simple ? 1e-5 : 1e-12;
                    check(label.str() + ", its rate from its fixings", coupon->rate(),
                          accrue(r.rateStart, r.rateEnd, averaging == RateAveraging::Compound), tolerance);
                    for (Date d = coupon->accrualStartDate() + 1; d <= coupon->accrualEndDate(); ++d)
                    {
                        // on telescopic value dates, at the dates outside the rate computation period
                        if (telescopic && d > r.rateStart && d < r.rateEnd)
                            continue;
                        std::ostringstream at;
                        at << ", its accrued amount at " << d;
                        const Real amount =
                            d <= r.rateStart ?
                                0.0 :
                                nominal *
                                    overThePeriod(r.rateStart, std::min(d, r.rateEnd), averaging, telescopic)->rate() *
                                    coupon->accruedPeriod(d);
                        check(label.str() + at.str(), coupon->accruedAmount(d), amount, 1e-9);
                    }
                }
    }

    // on telescopic value dates, a coupon whose rate computation period ends on Sunday 1 November 2026, before its
    // accrual end, has from then on the rate of a coupon over that period
    Settings::instance().evaluationDate() = Date(1, April, 2026);
    index->clearFixings();
    const Date october(1, October, 2026), sunday(1, November, 2026);
    for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
    {
        const auto coupon = ext::make_shared<OvernightIndexedCoupon>(
            Date(4, November, 2026), nominal, Date(4, August, 2026), Date(4, November, 2026), index, 1.0, 0.0, Date(),
            Date(), dayCounter, true, averaging, Null<Natural>(), 0, false, false, october, sunday);
        std::ostringstream label;
        label << "the coupon over a rate computation period to " << sunday
              << (averaging == RateAveraging::Compound ? ", compounded" : ", averaged");
        const Rate expected = overThePeriod(october, sunday, averaging, true)->rate();
        if (coupon->rate() != expected)
            BOOST_ERROR("Failed to reproduce " << label.str()
                                               << ", its rate:\n    expected:   " << std::setprecision(17) << expected
                                               << "\n    calculated: " << coupon->rate());
        for (Date d = sunday + 1; d <= coupon->accrualEndDate(); ++d)
        {
            std::ostringstream at;
            at << ", its accrued amount at " << d;
            check(label.str() + at.str(), coupon->accruedAmount(d), nominal * expected * coupon->accruedPeriod(d),
                  1e-9);
        }
    }

    // a rate computation period that ends where it starts
    BOOST_CHECK_EXCEPTION(ext::make_shared<OvernightIndexedCoupon>(
                              Date(4, November, 2026), nominal, Date(4, August, 2026), Date(4, November, 2026), index,
                              1.0, 0.0, Date(), Date(), dayCounter, false, RateAveraging::Compound, Null<Natural>(), 0,
                              false, false, Date(4, August, 2026), Date(4, August, 2026)),
                          Error, ExpectedErrorMessage("must be earlier than its end date"));

    // and one that ends after the accrual end
    BOOST_CHECK_EXCEPTION(ext::make_shared<OvernightIndexedCoupon>(
                              Date(4, November, 2026), nominal, Date(4, August, 2026), Date(4, November, 2026), index,
                              1.0, 0.0, Date(), Date(), dayCounter, false, RateAveraging::Compound, Null<Natural>(), 0,
                              false, false, Date(4, September, 2026), Date(4, December, 2026)),
                          Error, ExpectedErrorMessage("must not be later than the accrual end date"));
}

BOOST_AUTO_TEST_CASE(testDailyCapsAndFloorsOverTheRateComputationPeriod)
{
    BOOST_TEST_MESSAGE("Testing daily caps and floors on overnight coupons over their rate computation period...");

    const TARGET calendar;
    const Actual360 dayCounter;
    // 2 % up to 1 October 2026, then 3 %
    const Handle<YieldTermStructure> curve(ext::make_shared<ForwardCurve>(
        std::vector<Date>{Date(1, April, 2026), Date(1, October, 2026), Date(1, April, 2027), Date(1, April, 2028)},
        std::vector<Rate>{0.02, 0.02, 0.03, 0.04}, dayCounter));
    const auto index = ext::make_shared<Estr>(curve);
    const Real nominal = 10000.0;
    const Schedule schedule = MakeSchedule()
                                  .from(Date(4, May, 2026))
                                  .to(Date(4, May, 2027))
                                  .withTenor(3 * Months)
                                  .withCalendar(calendar)
                                  .withConvention(Following);
    const Handle<OptionletVolatilityStructure> volatility(
        ext::make_shared<ConstantOptionletVolatility>(0, calendar, Following, 0.0, dayCounter));
    const auto check = [](const std::string& what, Real calculated, Real expected, Real tolerance)
    {
        if (!(std::fabs(calculated - expected) <= tolerance))
            BOOST_ERROR("Failed to reproduce " << what << ":\n    expected:   " << std::setprecision(15) << expected
                                               << "\n    calculated: " << calculated
                                               << "\n    error:      " << std::fabs(calculated - expected));
    };

    // the second coupon, from 4 August to 4 November 2026; no fixing over its rate computation period, at 2 % in
    // advance and at 3 % over the last month, reaches its cap or floor
    struct Row
    {
        std::string name;
        bool inArrears;
        ext::optional<Period> lastRecentPeriod;
        Rate cap, floor;
    };
    const std::vector<Row> rows = {{"fixed in advance", false, ext::nullopt, 0.021, 0.015},
                                   {"over a last recent period of one month", true, 1 * Months, 0.035, 0.025}};
    for (const Date& today : {Date(1, April, 2026), Date(1, September, 2026)})
    {
        Settings::instance().evaluationDate() = today;
        index->clearFixings();
        for (Date d(4, May, 2026); d < today; d = calendar.advance(d, 1, Days))
            index->addFixing(d, 0.02);
        for (const auto& r : rows)
            for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
                for (bool isCap : {true, false})
                {
                    const Leg leg = OvernightLeg(schedule, index)
                                        .withNotionals(nominal)
                                        .withPaymentDayCounter(dayCounter)
                                        .withAveragingMethod(averaging)
                                        .inArrears(r.inArrears)
                                        .withLastRecentPeriod(r.lastRecentPeriod);
                    const auto coupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(leg[1]);
                    BOOST_REQUIRE(coupon);
                    const auto capped = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                        coupon, isCap ? r.cap : Null<Rate>(), isCap ? Null<Rate>() : r.floor, false, true);
                    capped->setPricer(makeBlackOvernightPricer(averaging, volatility));
                    std::ostringstream label;
                    label << "the coupon " << r.name
                          << (averaging == RateAveraging::Compound ? ", compounded" : ", averaged")
                          << (isCap ? ", capped" : ", floored") << " each day, on " << today << ", its rate";
                    // forecast, it differs by the Black pricer's approximation
                    const Real tolerance = coupon->rateComputationEndDate() <= today ? 1e-12 : 1e-5;
                    check(label.str(), capped->rate(), coupon->rate(), tolerance);
                }
    }

    // capped below or floored above each of its fixings, at 2 % in advance and at 3 % over the last month, forecast on
    // 1 April and 1 September and known on 5 November: the rate of the cap or floor held over its rate computation
    // period
    const auto held = [&](Rate level, const Date& start, const Date& end, bool compound)
    {
        Real factor = 1.0, sum = 0.0;
        for (Date d = start; d < end; d = calendar.advance(d, 1, Days))
        {
            const Time t = dayCounter.yearFraction(d, std::min(calendar.advance(d, 1, Days), end));
            factor *= 1.0 + level * t;
            sum += level * t;
        }
        return (compound ? factor - 1.0 : sum) / dayCounter.yearFraction(start, end);
    };
    struct Held
    {
        std::string name;
        bool inArrears;
        ext::optional<Period> lastRecentPeriod;
        Rate cap, floor;
        Date start, end;
    };
    const std::vector<Held> heldRows = {
        {"fixed in advance", false, ext::nullopt, 0.019, 0.021, Date(4, May, 2026), Date(4, August, 2026)},
        {"over a last recent period of one month", true, 1 * Months, 0.029, 0.031, Date(5, October, 2026),
         Date(4, November, 2026)}};
    for (const Date& today : {Date(1, April, 2026), Date(1, September, 2026), Date(5, November, 2026)})
    {
        Settings::instance().evaluationDate() = today;
        index->clearFixings();
        for (Date d(4, May, 2026); d < today; d = calendar.advance(d, 1, Days))
            index->addFixing(d, d < Date(1, October, 2026) ? 0.02 : 0.03);
        for (const auto& r : heldRows)
            for (auto averaging : {RateAveraging::Compound, RateAveraging::Simple})
                for (bool isCap : {true, false})
                {
                    const Rate strike = isCap ? r.cap : r.floor;
                    const Leg leg = OvernightLeg(schedule, index)
                                        .withNotionals(nominal)
                                        .withPaymentDayCounter(dayCounter)
                                        .withAveragingMethod(averaging)
                                        .inArrears(r.inArrears)
                                        .withLastRecentPeriod(r.lastRecentPeriod);
                    const auto coupon = ext::dynamic_pointer_cast<OvernightIndexedCoupon>(leg[1]);
                    BOOST_REQUIRE(coupon);
                    const auto capped = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(
                        coupon, isCap ? strike : Null<Rate>(), isCap ? Null<Rate>() : strike, false, true);
                    capped->setPricer(makeBlackOvernightPricer(averaging, volatility));
                    std::ostringstream label;
                    label << "the coupon " << r.name
                          << (averaging == RateAveraging::Compound ? ", compounded" : ", averaged")
                          << (isCap ? ", capped" : ", floored") << " each day at " << strike << ", on " << today
                          << ", its rate";
                    // forecast, it differs by the Black pricer's approximation
                    const Real tolerance = r.end <= today ? 1e-12 : 1e-5;
                    check(label.str(), capped->rate(),
                          held(strike, r.start, r.end, averaging == RateAveraging::Compound), tolerance);
                }
    }
}

BOOST_AUTO_TEST_CASE(testAccruedAmountWithLockoutBeforeItStarts)
{
    BOOST_TEST_MESSAGE("Testing a compounded overnight coupon's accrued amount with a lockout before it starts...");

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    RelinkableHandle<YieldTermStructure> curve;
    const auto index = ext::make_shared<OvernightIndex>("T426", 0, USDCurrency(), calendar, dayCounter, curve);
    const Real nominal = 10000.0;
    const Natural lockoutDays = 4;

    struct Scenario
    {
        Date today, start, end;
    };
    // today Tuesday 10 November 2026, a coupon from Monday 2 to Monday 30 November 2026 whose first fixings are stored;
    // and today Monday 2 November 2026, a coupon from Wednesday 4 November to Friday 4 December 2026, still to fix
    for (const auto& [today, start, end] :
         {Scenario{Date(10, November, 2026), Date(2, November, 2026), Date(30, November, 2026)},
          Scenario{Date(2, November, 2026), Date(4, November, 2026), Date(4, December, 2026)}})
    {
        Settings::instance().evaluationDate() = today;
        // a flat forward of 3 % from today, where a forwarding curve usually starts, and fixings of 2 % stored before
        // today
        curve.linkTo(ext::make_shared<FlatForward>(today, 0.03, dayCounter, Continuous));
        index->clearFixings();
        for (Date d(20, October, 2026); d < today; d = calendar.advance(d, 1, Days))
            index->addFixing(d, 0.02);

        // each day's fixing, stored or forecast, compounded over its period up to the date; the lockout's days take the
        // fixing of the business day before them
        const Date lockoutStart = calendar.advance(end, -static_cast<Integer>(lockoutDays), Days);
        const Date lastFixing = calendar.advance(lockoutStart, -1, Days);
        const auto expected = [&](const Date& date)
        {
            Real growth = 1.0;
            for (Date d = start; d < date; d = calendar.advance(d, 1, Days))
                growth *= 1.0 + index->fixing(std::min(d, lastFixing)) *
                                    dayCounter.yearFraction(d, std::min(calendar.advance(d, 1, Days), date));
            return nominal * (growth - 1.0);
        };

        for (bool telescopicValueDates : {false, true})
        {
            // telescopic value dates are the dense ones in their front stub, up to 7 business days after today
            const Date last = telescopicValueDates ? calendar.advance(today, 7, Days) : end;
            // without a lookback, and under the observation shift with a lookback of 2 days
            for (bool observationShift : {false, true})
            {
                const Natural lookbackDays = observationShift ? 2 : Null<Natural>();
                const OvernightIndexedCoupon coupon(end, nominal, start, end, index, 1.0, 0.0, Date(), Date(),
                                                    dayCounter, telescopicValueDates, RateAveraging::Compound,
                                                    lookbackDays, lockoutDays, observationShift);
                const OvernightIndexedCoupon withoutLockout(end, nominal, start, end, index, 1.0, 0.0, Date(), Date(),
                                                            dayCounter, telescopicValueDates, RateAveraging::Compound,
                                                            lookbackDays, 0, observationShift);
                for (Date d = start + 1; d <= last; ++d)
                {
                    BOOST_TEST_CONTEXT("today " << today << (telescopicValueDates ? ", telescopic" : ", dense")
                                                << " value dates" << (observationShift ? ", observation shift" : "")
                                                << ", accrued to " << d)
                    {
                        Real accrued = Null<Real>();
                        BOOST_CHECK_NO_THROW(accrued = coupon.accruedAmount(d));
                        if (accrued != Null<Real>())
                        {
                            if (!observationShift)
                                CHECK_OIS_COUPON_RESULT("accrued amount", accrued, expected(d), 1e-9);
                            // until its lockout starts, the coupon accrues as it would without one
                            if (d <= lockoutStart)
                                CHECK_OIS_COUPON_RESULT("accrued amount without a lockout", accrued,
                                                        withoutLockout.accruedAmount(d), 0.0);
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testAccruedAmountBetweenTelescopicValueDates)
{
    BOOST_TEST_MESSAGE("Testing a compounded overnight coupon's accrued amount between its telescopic value dates...");

    const WeekendsOnly calendar;
    const Actual360 dayCounter;
    RelinkableHandle<YieldTermStructure> curve;
    // an index fixing on the day its rate is for, and one fixing 2 days before it
    const auto index = ext::make_shared<OvernightIndex>("T433", 0, USDCurrency(), calendar, dayCounter, curve);
    const auto index2 = ext::make_shared<OvernightIndex>("T433F", 2, USDCurrency(), calendar, dayCounter, curve);
    const Real nominal = 10000.0;

    // A coupon with telescopic value dates accrues, to each calendar day, what the same coupon with dense ones does.
    const auto check = [&](const ext::shared_ptr<OvernightIndex>& on, const Date& start, const Date& end,
                           Natural lookbackDays, Natural lockoutDays, bool observationShift)
    {
        const OvernightIndexedCoupon dense(end, nominal, start, end, on, 1.0, 0.0, Date(), Date(), dayCounter, false,
                                           RateAveraging::Compound, lookbackDays, lockoutDays, observationShift);
        const OvernightIndexedCoupon telescopic(end, nominal, start, end, on, 1.0, 0.0, Date(), Date(), dayCounter,
                                                true, RateAveraging::Compound, lookbackDays, lockoutDays,
                                                observationShift);
        for (Date d = start + 1; d <= end; ++d)
        {
            BOOST_TEST_CONTEXT(on->name() << ", today " << Settings::instance().evaluationDate() << ", coupon from "
                                          << start << " to " << end << ", lookback "
                                          << (lookbackDays == Null<Natural>() ? on->fixingDays() : lookbackDays)
                                          << ", lockout " << lockoutDays
                                          << (observationShift ? ", observation shift" : "") << ", accrued to " << d)
            {
                CHECK_OIS_COUPON_RESULT("accrued amount", telescopic.accruedAmount(d), dense.accruedAmount(d), 1e-9);
            }
        }
    };

    // today Tuesday 1 September 2026, coupons from Thursday 1 October to Friday 20 November 2026, forecast from a flat
    // forward of 3 %
    Settings::instance().evaluationDate() = Date(1, September, 2026);
    curve.linkTo(ext::make_shared<FlatForward>(Date(1, September, 2026), 0.03, dayCounter, Continuous));
    const Date start(1, October, 2026), end(20, November, 2026);
    for (bool observationShift : {false, true})
    {
        const Natural lookbackDays = observationShift ? 2 : Null<Natural>();
        check(index, start, end, lookbackDays, 0, observationShift);
        check(index, start, end, lookbackDays, 2, observationShift);
        // from Saturday 3 October
        check(index, Date(3, October, 2026), end, lookbackDays, 0, observationShift);
        // on the index fixing 2 days before, whose fixing days the coupon takes
        check(index2, start, end, Null<Natural>(), 0, observationShift);
    }
    // to Sunday 22 November
    check(index, start, Date(22, November, 2026), Null<Natural>(), 0, false);

    // today Wednesday 1 April 2026, a coupon from Monday 1 June 2026 to Tuesday 1 June 2027, forecast from a forward
    // of 2 % up to 1 October 2026, 3 % up to 1 April 2027 and 4 % after
    Settings::instance().evaluationDate() = Date(1, April, 2026);
    curve.linkTo(ext::make_shared<ForwardCurve>(
        std::vector<Date>{Date(1, April, 2026), Date(1, October, 2026), Date(1, April, 2027), Date(1, April, 2028)},
        std::vector<Rate>{0.01, 0.02, 0.03, 0.04}, dayCounter));
    const Date yearStart(1, June, 2026), yearEnd(1, June, 2027);
    check(index, yearStart, yearEnd, Null<Natural>(), 0, false);
    check(index, yearStart, yearEnd, 2, 0, true);
    // the same, today Thursday 4 June 2026 with the fixings before it stored at 1.5 %
    Settings::instance().evaluationDate() = Date(4, June, 2026);
    index->clearFixings();
    for (Date d(27, May, 2026); d < Date(4, June, 2026); d = calendar.advance(d, 1, Days))
        index->addFixing(d, 0.015);
    check(index, yearStart, yearEnd, Null<Natural>(), 0, false);
    check(index, yearStart, yearEnd, 2, 0, true);

    // today Tuesday 10 November 2026, a coupon from Monday 2 to Monday 30 November 2026 whose first fixings are stored
    // at 2 %, forecast from a flat forward of 3 %
    Settings::instance().evaluationDate() = Date(10, November, 2026);
    curve.linkTo(ext::make_shared<FlatForward>(Date(10, November, 2026), 0.03, dayCounter, Continuous));
    index->clearFixings();
    for (Date d(20, October, 2026); d < Date(10, November, 2026); d = calendar.advance(d, 1, Days))
        index->addFixing(d, 0.02);
    check(index, Date(2, November, 2026), Date(30, November, 2026), Null<Natural>(), 0, false);
    check(index, Date(2, November, 2026), Date(30, November, 2026), 2, 0, true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
