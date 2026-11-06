/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004, 2005 StatPro Italia srl
 Copyright (C) 2007, 2012 Ferdinando Ametrano
 Copyright (C) 2007, 2009 Piter Dias

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
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/audlibor.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/instruments/bonds/fixedratebond.hpp>
#include <ql/instruments/bonds/floatingratebond.hpp>
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/pricingengines/bond/bondfunctions.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/pricingengines/bond/riskybondengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/australia.hpp>
#include <ql/time/calendars/brazil.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/southafrica.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedkingdom.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/business252.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/schedule.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BondsTests)

#define ASSERT_CLOSE(name, settlement, calculated, expected, tolerance)                                  \
    if (std::fabs(calculated - expected) > tolerance)                                                    \
    {                                                                                                    \
        BOOST_ERROR("Failed to reproduce " << name << " at " << settlement                               \
                                           << "\n    calculated: " << std::setprecision(9) << calculated \
                                           << "\n    expected:   " << std::setprecision(9) << expected); \
    }

struct CommonVars
{
    // common data
    Calendar calendar;
    Date today;
    Real faceAmount;

    // setup
    CommonVars()
    {
        calendar = TARGET();
        today = calendar.adjust(Date::todaysDate());
        Settings::instance().evaluationDate() = today;
        faceAmount = 1000000.0;
    }
};

void checkValue(Real value, Real expectedValue, Real tolerance, const std::string& msg)
{
    if (std::fabs(value - expectedValue) > tolerance)
    {
        BOOST_ERROR(msg << std::fixed << "\n    calculated: " << value << "\n    expected:   " << expectedValue
                        << "\n    tolerance:  " << tolerance << "\n    error:      " << value - expectedValue);
    }
}


BOOST_AUTO_TEST_CASE(testYield)
{

    BOOST_TEST_MESSAGE("Testing consistency of bond price/yield calculation...");

    CommonVars vars;

    Real tolerance = 1.0e-7;
    Size maxEvaluations = 100;

    Integer issueMonths[] = {-24, -18, -12, -6, 0, 6, 12, 18, 24};
    Integer lengths[] = {3, 5, 10, 15, 20};
    Natural settlementDays = 3;
    Real coupons[] = {0.02, 0.05, 0.08};
    Frequency frequencies[] = {Semiannual, Annual};
    DayCounter bondDayCount = Thirty360(Thirty360::BondBasis);
    BusinessDayConvention accrualConvention = Unadjusted;
    BusinessDayConvention paymentConvention = ModifiedFollowing;
    Real redemption = 100.0;

    Rate yields[] = {0.03, 0.04, 0.05, 0.06, 0.07};
    Compounding compounding[] = {Compounded, Continuous};

    for (int issueMonth : issueMonths)
    {
        for (int length : lengths)
        {
            for (Real& coupon : coupons)
            {
                for (auto& frequency : frequencies)
                {
                    for (auto& n : compounding)
                    {

                        Date dated = vars.calendar.advance(vars.today, issueMonth, Months);
                        Date issue = dated;
                        Date maturity = vars.calendar.advance(issue, length, Years);

                        Schedule sch(dated, maturity, Period(frequency), vars.calendar, accrualConvention,
                                     accrualConvention, DateGeneration::Backward, false);

                        FixedRateBond bond(settlementDays, vars.faceAmount, sch, std::vector<Rate>(1, coupon),
                                           bondDayCount, paymentConvention, redemption, issue);

                        for (Real m : yields)
                        {

                            Bond::Price price = {BondFunctions::cleanPrice(bond, m, bondDayCount, n, frequency),
                                                 Bond::Price::Clean};

                            Rate calculated = BondFunctions::yield(bond, price, bondDayCount, n, frequency, Date(),
                                                                   tolerance, maxEvaluations, 0.05);

                            if (std::fabs(m - calculated) > tolerance)
                            {
                                // the difference might not matter
                                Real price2 = BondFunctions::cleanPrice(bond, calculated, bondDayCount, n, frequency);
                                if (std::fabs(price.amount() - price2) / price.amount() > tolerance)
                                {
                                    BOOST_ERROR("\nyield recalculation failed:"
                                                "\n    issue:        "
                                                << issue << "\n    maturity:     " << maturity << "\n    coupon:       "
                                                << io::rate(coupon) << "\n    frequency:    " << frequency
                                                << "\n    yield:        " << io::rate(m)
                                                << (n == Compounded ? " compounded" : " continuous")
                                                << std::setprecision(7) << "\n    clean price:  " << price.amount()
                                                << "\n    yield':       " << io::rate(calculated)
                                                << "\n    clean price': " << price2);
                                }
                            }

                            price = {BondFunctions::dirtyPrice(bond, m, bondDayCount, n, frequency),
                                     Bond::Price::Dirty};

                            calculated = BondFunctions::yield(bond, price, bondDayCount, n, frequency, Date(),
                                                              tolerance, maxEvaluations, 0.05);

                            if (std::fabs(m - calculated) > tolerance)
                            {
                                // the difference might not matter
                                Real price2 = BondFunctions::dirtyPrice(bond, calculated, bondDayCount, n, frequency);
                                if (std::fabs(price.amount() - price2) / price.amount() > tolerance)
                                {
                                    BOOST_ERROR("\nyield recalculation failed:"
                                                "\n    issue:        "
                                                << issue << "\n    maturity:     " << maturity << "\n    coupon:       "
                                                << io::rate(coupon) << "\n    frequency:    " << frequency
                                                << "\n    yield:        " << io::rate(m)
                                                << (n == Compounded ? " compounded" : " continuous")
                                                << std::setprecision(7) << "\n    dirty price:  " << price.amount()
                                                << "\n    yield':       " << io::rate(calculated)
                                                << "\n    dirty price': " << price2);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testAtmRate)
{

    BOOST_TEST_MESSAGE("Testing consistency of bond price/ATM rate calculation...");

    CommonVars vars;

    Real tolerance = 1.0e-7;

    Integer issueMonths[] = {-24, -18, -12, -6, 0, 6, 12, 18, 24};
    Integer lengths[] = {3, 5, 10, 15, 20};
    Natural settlementDays = 3;
    Real coupons[] = {0.02, 0.05, 0.08};
    Frequency frequencies[] = {Semiannual, Annual};
    DayCounter bondDayCount = Thirty360(Thirty360::BondBasis);
    BusinessDayConvention accrualConvention = Unadjusted;
    BusinessDayConvention paymentConvention = ModifiedFollowing;
    Real redemption = 100.0;
    Handle<YieldTermStructure> disc(flatRate(vars.today, 0.03, Actual360()));
    ext::shared_ptr<PricingEngine> bondEngine(new DiscountingBondEngine(disc));

    for (int issueMonth : issueMonths)
    {
        for (int length : lengths)
        {
            for (Real& coupon : coupons)
            {
                for (auto& frequency : frequencies)
                {
                    Date dated = vars.calendar.advance(vars.today, issueMonth, Months);
                    Date issue = dated;
                    Date maturity = vars.calendar.advance(issue, length, Years);

                    Schedule sch(dated, maturity, Period(frequency), vars.calendar, accrualConvention,
                                 accrualConvention, DateGeneration::Backward, false);

                    FixedRateBond bond(settlementDays, vars.faceAmount, sch, std::vector<Rate>(1, coupon), bondDayCount,
                                       paymentConvention, redemption, issue);

                    bond.setPricingEngine(bondEngine);
                    Bond::Price price = {bond.cleanPrice(), Bond::Price::Clean};
                    Rate calculated = BondFunctions::atmRate(bond, **disc, bond.settlementDate(), price);

                    if (std::fabs(coupon - calculated) > tolerance)
                    {
                        BOOST_ERROR("\natm rate recalculation failed:"
                                    "\n today:           "
                                    << vars.today << "\n settlement date: " << bond.settlementDate()
                                    << "\n issue:           " << issue << "\n maturity:        " << maturity
                                    << "\n coupon:          " << io::rate(coupon) << "\n frequency:       " << frequency
                                    << "\n clean price:     " << price.amount()
                                    << "\n atm rate:        " << io::rate(calculated));
                    }

                    price = {bond.dirtyPrice(), Bond::Price::Dirty};
                    calculated = BondFunctions::atmRate(bond, **disc, bond.settlementDate(), price);

                    if (std::fabs(coupon - calculated) > tolerance)
                    {
                        BOOST_ERROR("\natm rate recalculation failed:"
                                    "\n today:           "
                                    << vars.today << "\n settlement date: " << bond.settlementDate()
                                    << "\n issue:           " << issue << "\n maturity:        " << maturity
                                    << "\n coupon:          " << io::rate(coupon) << "\n frequency:       " << frequency
                                    << "\n dirty price:     " << price.amount()
                                    << "\n atm rate:        " << io::rate(calculated));
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testZspread)
{

    BOOST_TEST_MESSAGE("Testing consistency of bond price/z-spread calculation...");

    CommonVars vars;

    Real tolerance = 1.0e-7;
    Size maxEvaluations = 100;

    Handle<YieldTermStructure> discountCurve(flatRate(vars.today, 0.03, Actual360()));

    Integer issueMonths[] = {-24, -18, -12, -6, 0, 6, 12, 18, 24};
    Integer lengths[] = {3, 5, 10, 15, 20};
    Natural settlementDays = 3;
    Real coupons[] = {0.02, 0.05, 0.08};
    Frequency frequencies[] = {Semiannual, Annual};
    DayCounter bondDayCount = Thirty360(Thirty360::BondBasis);
    BusinessDayConvention accrualConvention = Unadjusted;
    BusinessDayConvention paymentConvention = ModifiedFollowing;
    Real redemption = 100.0;

    Spread spreads[] = {-0.01, -0.005, 0.0, 0.005, 0.01};
    Compounding compounding[] = {Compounded, Continuous};

    for (int issueMonth : issueMonths)
    {
        for (int length : lengths)
        {
            for (Real& coupon : coupons)
            {
                for (auto& frequency : frequencies)
                {
                    for (auto& n : compounding)
                    {

                        Date dated = vars.calendar.advance(vars.today, issueMonth, Months);
                        Date issue = dated;
                        Date maturity = vars.calendar.advance(issue, length, Years);

                        Schedule sch(dated, maturity, Period(frequency), vars.calendar, accrualConvention,
                                     accrualConvention, DateGeneration::Backward, false);

                        FixedRateBond bond(settlementDays, vars.faceAmount, sch, std::vector<Rate>(1, coupon),
                                           bondDayCount, paymentConvention, redemption, issue);

                        for (Real spread : spreads)
                        {

                            // Clean price
                            Bond::Price price = {BondFunctions::cleanPrice(bond, *discountCurve, spread, n, frequency),
                                                 Bond::Price::Clean};
                            Spread calculated = BondFunctions::zSpread(bond, price, *discountCurve, n, frequency,
                                                                       Date(), tolerance, maxEvaluations);

                            if (std::fabs(spread - calculated) > tolerance)
                            {
                                // the difference might not matter
                                Real price2 = BondFunctions::cleanPrice(bond, *discountCurve, calculated, n, frequency);
                                if (std::fabs(price.amount() - price2) / price.amount() > tolerance)
                                {
                                    BOOST_ERROR("\nZ-spread recalculation failed:"
                                                "\n    issue:     "
                                                << issue << "\n    maturity:  " << maturity << "\n    coupon:    "
                                                << io::rate(coupon) << "\n    frequency: " << frequency
                                                << "\n    Z-spread:  " << io::rate(spread)
                                                << (n == Compounded ? " compounded" : " continuous")
                                                << std::setprecision(7) << "\n    clean price:  " << price.amount()
                                                << "\n    Z-spread': " << io::rate(calculated)
                                                << "\n    clean price': " << price2);
                                }
                            }

                            // Dirty price
                            price = {BondFunctions::dirtyPrice(bond, *discountCurve, spread, n, frequency),
                                     Bond::Price::Dirty};

                            calculated = BondFunctions::zSpread(bond, price, *discountCurve, n, frequency, Date(),
                                                                tolerance, maxEvaluations);

                            if (std::fabs(spread - calculated) > tolerance)
                            {
                                // the difference might not matter
                                Real price2 = BondFunctions::dirtyPrice(bond, *discountCurve, calculated, n, frequency);
                                if (std::fabs(price.amount() - price2) / price.amount() > tolerance)
                                {
                                    BOOST_ERROR("\nZ-spread recalculation failed:"
                                                "\n    issue:        "
                                                << issue << "\n    maturity:     " << maturity << "\n    coupon:       "
                                                << io::rate(coupon) << "\n    frequency:    " << frequency
                                                << "\n    Z-spread:     " << io::rate(spread)
                                                << (compounding[n] == Compounded ? " compounded" : " continuous")
                                                << std::setprecision(7) << "\n    dirty price:  " << price.amount()
                                                << "\n    Z-spread':    " << io::rate(calculated)
                                                << "\n    dirty price': " << price2);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testTheoretical)
{

    BOOST_TEST_MESSAGE("Testing theoretical bond price/yield calculation...");

    CommonVars vars;

    Real tolerance = 1.0e-7;
    Size maxEvaluations = 100;

    Size lengths[] = {3, 5, 10, 15, 20};
    Natural settlementDays = 3;
    Real coupons[] = {0.02, 0.05, 0.08};
    Frequency frequencies[] = {Semiannual, Annual};
    DayCounter bondDayCount = Actual360();
    BusinessDayConvention accrualConvention = Unadjusted;
    BusinessDayConvention paymentConvention = ModifiedFollowing;
    Real redemption = 100.0;

    Rate yields[] = {0.03, 0.04, 0.05, 0.06, 0.07};

    for (unsigned long length : lengths)
    {
        for (Real& coupon : coupons)
        {
            for (auto& frequency : frequencies)
            {

                Date dated = vars.today;
                Date issue = dated;
                Date maturity = vars.calendar.advance(issue, length, Years);

                ext::shared_ptr<SimpleQuote> rate(new SimpleQuote(0.0));
                Handle<YieldTermStructure> discountCurve(flatRate(vars.today, rate, bondDayCount));

                Schedule sch(dated, maturity, Period(frequency), vars.calendar, accrualConvention, accrualConvention,
                             DateGeneration::Backward, false);

                FixedRateBond bond(settlementDays, vars.faceAmount, sch, std::vector<Rate>(1, coupon), bondDayCount,
                                   paymentConvention, redemption, issue);

                ext::shared_ptr<PricingEngine> bondEngine(new DiscountingBondEngine(discountCurve));
                bond.setPricingEngine(bondEngine);

                for (Real m : yields)
                {

                    rate->setValue(m);

                    // Test yield vs clean price
                    Real price = BondFunctions::cleanPrice(bond, m, bondDayCount, Continuous, frequency);
                    Real calculatedPrice = bond.cleanPrice();

                    if (std::fabs(price - calculatedPrice) > tolerance)
                    {
                        BOOST_ERROR("price calculation failed:"
                                    << "\n    issue:       " << issue << "\n    maturity:    " << maturity
                                    << "\n    coupon:      " << io::rate(coupon) << "\n    frequency:   " << frequency
                                    << "\n    yield:       " << io::rate(m) << std::setprecision(7)
                                    << "\n    expected:    " << price << "\n    calculated': " << calculatedPrice
                                    << "\n    error':      " << price - calculatedPrice);
                    }

                    Rate calculatedYield =
                        BondFunctions::yield(bond, {calculatedPrice, Bond::Price::Clean}, bondDayCount, Continuous,
                                             frequency, bond.settlementDate(), tolerance, maxEvaluations);
                    if (std::fabs(m - calculatedYield) > tolerance)
                    {
                        BOOST_ERROR("yield calculation failed:"
                                    << "\n    issue:     " << issue << "\n    maturity:  " << maturity
                                    << "\n    coupon:    " << io::rate(coupon) << "\n    frequency: " << frequency
                                    << "\n    yield:     " << io::rate(m) << std::setprecision(7)
                                    << "\n    clean price: " << price
                                    << "\n    yield':    " << io::rate(calculatedYield));
                    }

                    // Test yield vs dirty price
                    price = BondFunctions::dirtyPrice(bond, m, bondDayCount, Continuous, frequency);
                    calculatedPrice = bond.dirtyPrice();

                    if (std::fabs(price - calculatedPrice) > tolerance)
                    {
                        BOOST_ERROR("price calculation failed:"
                                    << "\n    issue:       " << issue << "\n    maturity:    " << maturity
                                    << "\n    coupon:      " << io::rate(coupon) << "\n    frequency:   " << frequency
                                    << "\n    yield:       " << io::rate(m) << std::setprecision(7)
                                    << "\n    expected:    " << price << "\n    calculated': " << calculatedPrice
                                    << "\n    error':      " << price - calculatedPrice);
                    }

                    calculatedYield =
                        BondFunctions::yield(bond, {calculatedPrice, Bond::Price::Dirty}, bondDayCount, Continuous,
                                             frequency, bond.settlementDate(), tolerance, maxEvaluations, 0.05);
                    if (std::fabs(m - calculatedYield) > tolerance)
                    {
                        BOOST_ERROR("yield calculation failed:"
                                    << "\n    issue:       " << issue << "\n    maturity:    " << maturity
                                    << "\n    coupon:      " << io::rate(coupon) << "\n    frequency:   " << frequency
                                    << "\n    yield:       " << io::rate(m) << std::setprecision(7)
                                    << "\n    dirty price: " << price
                                    << "\n    yield':      " << io::rate(calculatedYield));
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testCached)
{

    BOOST_TEST_MESSAGE("Testing bond price/yield calculation against cached values...");

    CommonVars vars;

    // with implicit settlement calculation:

    Date today(22, November, 2004);
    Settings::instance().evaluationDate() = today;

    Calendar bondCalendar = NullCalendar();

    Natural settlementDays = 1;

    Handle<YieldTermStructure> discountCurve(flatRate(today, 0.03, Actual360()));

    // actual market values from the evaluation date

    Frequency freq = Semiannual;
    // This means that this bond has a short first coupon, as the
    // first coupon payment is april 30th and therefore the notional
    // first coupon is on October 30th 2004. Changing the EOM
    // convention to true will correct this so that the coupon starts
    // on October 31st and the first coupon is complete. This is
    // effectively assumed by the no-schedule daycounter.
    Schedule sch1(Date(31, October, 2004), Date(31, October, 2006), Period(freq), bondCalendar, Unadjusted, Unadjusted,
                  DateGeneration::Backward, true);
    DayCounter bondDayCount1 = ActualActual(ActualActual::ISMA, sch1);
    DayCounter bondDayCount1NoSchedule = ActualActual(ActualActual::ISMA);

    FixedRateBond bond1(settlementDays, vars.faceAmount, sch1, std::vector<Rate>(1, 0.025), bondDayCount1,
                        ModifiedFollowing, 100.0, Date(1, November, 2004));
    FixedRateBond bond1NoSchedule(settlementDays, vars.faceAmount, sch1, std::vector<Rate>(1, 0.025),
                                  bondDayCount1NoSchedule, ModifiedFollowing, 100.0, Date(1, November, 2004));

    ext::shared_ptr<PricingEngine> bondEngine(new DiscountingBondEngine(discountCurve));
    bond1.setPricingEngine(bondEngine);
    bond1NoSchedule.setPricingEngine(bondEngine);

    Real marketPrice1 = 99.203125;
    Rate marketYield1 = 0.02925;

    Schedule sch2(Date(15, November, 2004), Date(15, November, 2009), Period(freq), bondCalendar, Unadjusted,
                  Unadjusted, DateGeneration::Backward, false);
    DayCounter bondDayCount2 = ActualActual(ActualActual::ISMA, sch2);
    DayCounter bondDayCount2NoSchedule = ActualActual(ActualActual::ISMA);

    FixedRateBond bond2(settlementDays, vars.faceAmount, sch2, std::vector<Rate>(1, 0.035), bondDayCount2,
                        ModifiedFollowing, 100.0, Date(15, November, 2004));
    FixedRateBond bond2NoSchedule(settlementDays, vars.faceAmount, sch2, std::vector<Rate>(1, 0.035),
                                  bondDayCount2NoSchedule, ModifiedFollowing, 100.0, Date(15, November, 2004));

    bond2.setPricingEngine(bondEngine);
    bond2NoSchedule.setPricingEngine(bondEngine);

    Real marketPrice2 = 99.6875;
    Rate marketYield2 = 0.03569;

    // calculated values

    Real cachedPrice1a = 99.204505, cachedPrice2a = 99.687192;
    Real cachedPrice1b = 98.943393, cachedPrice2b = 101.986794;
    Rate cachedYield1a = 0.029257, cachedYield2a = 0.035689;
    Rate cachedYield1b = 0.029045, cachedYield2b = 0.035375;
    Rate cachedYield1c = 0.030423, cachedYield2c = 0.030432;

    // check
    Real tolerance = 1.0e-6;

    checkValue(BondFunctions::cleanPrice(bond1, marketYield1, bondDayCount1, Compounded, freq), cachedPrice1a,
               tolerance, "failed to reproduce cached price with schedule for bond 1:");
    checkValue(BondFunctions::cleanPrice(bond1NoSchedule, marketYield1, bondDayCount1NoSchedule, Compounded, freq),
               cachedPrice1a, tolerance, "failed to reproduce cached price with no schedule for bond 1:");
    checkValue(bond1.cleanPrice(), cachedPrice1b, tolerance,
               "failed to reproduce cached clean price with schedule for bond 1:");
    checkValue(bond1NoSchedule.cleanPrice(), cachedPrice1b, tolerance,
               "failed to reproduce cached clean price with no schdule for bond 1:");
    checkValue(BondFunctions::yield(bond1, {marketPrice1, Bond::Price::Clean}, bondDayCount1, Compounded, freq),
               cachedYield1a, tolerance, "failed to reproduce cached compounded yield with schedule for bond 1:");
    checkValue(BondFunctions::yield(bond1NoSchedule, {marketPrice1, Bond::Price::Clean}, bondDayCount1NoSchedule,
                                    Compounded, freq),
               cachedYield1a, tolerance, "failed to reproduce cached compounded yield with no schedule for bond 1:");
    checkValue(BondFunctions::yield(bond1, {marketPrice1, Bond::Price::Clean}, bondDayCount1, Continuous, freq),
               cachedYield1b, tolerance, "failed to reproduce cached continuous yield with schedule for bond 1:");
    checkValue(BondFunctions::yield(bond1NoSchedule, {marketPrice1, Bond::Price::Clean}, bondDayCount1NoSchedule,
                                    Continuous, freq),
               cachedYield1b, tolerance, "failed to reproduce cached continuous yield with no schedule for bond 1:");
    checkValue(BondFunctions::yield(bond1, {bond1.cleanPrice(), Bond::Price::Clean}, bondDayCount1, Continuous, freq,
                                    bond1.settlementDate()),
               cachedYield1c, tolerance, "failed to reproduce cached continuous yield with schedule for bond 1:");
    checkValue(BondFunctions::yield(bond1NoSchedule, {bond1NoSchedule.cleanPrice(), Bond::Price::Clean},
                                    bondDayCount1NoSchedule, Continuous, freq, bond1.settlementDate()),
               cachedYield1c, tolerance, "failed to reproduce cached continuous yield with no schedule for bond 1:");


    // Now bond 2
    checkValue(BondFunctions::cleanPrice(bond2, marketYield2, bondDayCount2, Compounded, freq), cachedPrice2a,
               tolerance, "failed to reproduce cached price with schedule for bond 2");
    checkValue(BondFunctions::cleanPrice(bond2NoSchedule, marketYield2, bondDayCount2NoSchedule, Compounded, freq),
               cachedPrice2a, tolerance, "failed to reproduce cached price with no schedule for bond 2:");
    checkValue(bond2.cleanPrice(), cachedPrice2b, tolerance,
               "failed to reproduce cached clean price with schedule for bond 2:");
    checkValue(bond2NoSchedule.cleanPrice(), cachedPrice2b, tolerance,
               "failed to reproduce cached clean price with no schedule for bond 2:");
    checkValue(BondFunctions::yield(bond2, {marketPrice2, Bond::Price::Clean}, bondDayCount2, Compounded, freq),
               cachedYield2a, tolerance, "failed to reproduce cached compounded yield with schedule for bond 2:");
    checkValue(BondFunctions::yield(bond2NoSchedule, {marketPrice2, Bond::Price::Clean}, bondDayCount2NoSchedule,
                                    Compounded, freq),
               cachedYield2a, tolerance, "failed to reproduce cached compounded yield with no schedule for bond 2:");
    checkValue(BondFunctions::yield(bond2, {marketPrice2, Bond::Price::Clean}, bondDayCount2, Continuous, freq),
               cachedYield2b, tolerance, "failed to reproduce chached continuous yield with schedule for bond 2:");
    checkValue(BondFunctions::yield(bond2NoSchedule, {marketPrice2, Bond::Price::Clean}, bondDayCount2NoSchedule,
                                    Continuous, freq),
               cachedYield2b, tolerance, "failed to reproduce cached continuous yield with schedule for bond 2:");
    checkValue(BondFunctions::yield(bond2, {bond2.cleanPrice(), Bond::Price::Clean}, bondDayCount2, Continuous, freq,
                                    bond2.settlementDate()),
               cachedYield2c, tolerance, "failed to reproduce cached continuous yield for bond 2 with schedule:");
    checkValue(BondFunctions::yield(bond2NoSchedule, {bond2NoSchedule.cleanPrice(), Bond::Price::Clean},
                                    bondDayCount2NoSchedule, Continuous, freq, bond2NoSchedule.settlementDate()),
               cachedYield2c, tolerance, "failed to reproduce cached continuous yield for bond 2 with no schedule:");


    // with explicit settlement date:

    Schedule sch3(Date(30, November, 2004), Date(30, November, 2006), Period(freq),
                  UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted, DateGeneration::Backward, false);
    DayCounter bondDayCount3 = ActualActual(ActualActual::ISMA, sch3);
    DayCounter bondDayCount3NoSchedule = ActualActual(ActualActual::ISMA);

    FixedRateBond bond3(settlementDays, vars.faceAmount, sch3, std::vector<Rate>(1, 0.02875), bondDayCount3,
                        ModifiedFollowing, 100.0, Date(30, November, 2004));
    FixedRateBond bond3NoSchedule(settlementDays, vars.faceAmount, sch3, std::vector<Rate>(1, 0.02875),
                                  bondDayCount3NoSchedule, ModifiedFollowing, 100.0, Date(30, November, 2004));

    bond3.setPricingEngine(bondEngine);
    bond3NoSchedule.setPricingEngine(bondEngine);

    Rate marketYield3 = 0.02997;

    Date settlementDate = Date(30, November, 2004);
    Real cachedPrice3 = 99.764759;

    checkValue(BondFunctions::cleanPrice(bond3, marketYield3, bondDayCount3, Compounded, freq, settlementDate),
               cachedPrice3, tolerance, "Failed to reproduce cached price for bond 3 with schedule");
    checkValue(BondFunctions::cleanPrice(bond3NoSchedule, marketYield3, bondDayCount3NoSchedule, Compounded, freq,
                                         settlementDate),
               cachedPrice3, tolerance, "Failed to reproduce cached price for bond 3 with no schedule");

    // this should give the same result since the issue date is the
    // earliest possible settlement date

    Settings::instance().evaluationDate() = Date(22, November, 2004);
    checkValue(
        BondFunctions::cleanPrice(bond3, marketYield3, bondDayCount3, Compounded, freq), cachedPrice3, tolerance,
        "Failed to reproduce the cached price for bond 3 with schedule and the earlierst possible settlment date");
    checkValue(
        BondFunctions::cleanPrice(bond3NoSchedule, marketYield3, bondDayCount3NoSchedule, Compounded, freq),
        cachedPrice3, tolerance,
        "Failed to reproduce the cached price for bond 3 with no schedule and the earlierst possible settlment date");
}

BOOST_AUTO_TEST_CASE(testCachedZero)
{

    BOOST_TEST_MESSAGE("Testing zero-coupon bond prices against cached values...");

    CommonVars vars;

    Date today(22, November, 2004);
    Settings::instance().evaluationDate() = today;

    Natural settlementDays = 1;

    Handle<YieldTermStructure> discountCurve(flatRate(today, 0.03, Actual360()));

    Real tolerance = 1.0e-6;

    // plain

    ZeroCouponBond bond1(settlementDays, UnitedStates(UnitedStates::GovernmentBond), vars.faceAmount,
                         Date(30, November, 2008), ModifiedFollowing, 100.0, Date(30, November, 2004));

    ext::shared_ptr<PricingEngine> bondEngine(new DiscountingBondEngine(discountCurve));
    bond1.setPricingEngine(bondEngine);

    Real cachedPrice1 = 88.551726;

    Real price = bond1.cleanPrice();
    if (std::fabs(price - cachedPrice1) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice1 << "\n"
                   << "    error:      " << price - cachedPrice1);
    }

    ZeroCouponBond bond2(settlementDays, UnitedStates(UnitedStates::GovernmentBond), vars.faceAmount,
                         Date(30, November, 2007), ModifiedFollowing, 100.0, Date(30, November, 2004));

    bond2.setPricingEngine(bondEngine);

    Real cachedPrice2 = 91.278949;

    price = bond2.cleanPrice();
    if (std::fabs(price - cachedPrice2) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice2 << "\n"
                   << "    error:      " << price - cachedPrice2);
    }

    ZeroCouponBond bond3(settlementDays, UnitedStates(UnitedStates::GovernmentBond), vars.faceAmount,
                         Date(30, November, 2006), ModifiedFollowing, 100.0, Date(30, November, 2004));

    bond3.setPricingEngine(bondEngine);

    Real cachedPrice3 = 94.098006;

    price = bond3.cleanPrice();
    if (std::fabs(price - cachedPrice3) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice3 << "\n"
                   << "    error:      " << price - cachedPrice3);
    }
}

BOOST_AUTO_TEST_CASE(testCachedFixed)
{

    BOOST_TEST_MESSAGE("Testing fixed-coupon bond prices against cached values...");

    CommonVars vars;

    Date today(22, November, 2004);
    Settings::instance().evaluationDate() = today;

    Natural settlementDays = 1;

    Handle<YieldTermStructure> discountCurve(flatRate(today, 0.03, Actual360()));

    Real tolerance = 1.0e-6;

    // plain

    Schedule sch(Date(30, November, 2004), Date(30, November, 2008), Period(Semiannual),
                 UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted, DateGeneration::Backward, false);

    FixedRateBond bond1(settlementDays, vars.faceAmount, sch, std::vector<Rate>(1, 0.02875),
                        ActualActual(ActualActual::ISMA), ModifiedFollowing, 100.0, Date(30, November, 2004));

    ext::shared_ptr<PricingEngine> bondEngine(new DiscountingBondEngine(discountCurve));
    bond1.setPricingEngine(bondEngine);

    Real cachedPrice1 = 99.298100;

    Real price = bond1.cleanPrice();
    if (std::fabs(price - cachedPrice1) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice1 << "\n"
                   << "    error:      " << price - cachedPrice1);
    }

    // varying coupons

    std::vector<Rate> couponRates(4);
    couponRates[0] = 0.02875;
    couponRates[1] = 0.03;
    couponRates[2] = 0.03125;
    couponRates[3] = 0.0325;

    FixedRateBond bond2(settlementDays, vars.faceAmount, sch, couponRates, ActualActual(ActualActual::ISMA),
                        ModifiedFollowing, 100.0, Date(30, November, 2004));

    bond2.setPricingEngine(bondEngine);

    Real cachedPrice2 = 100.334149;

    price = bond2.cleanPrice();
    if (std::fabs(price - cachedPrice2) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice2 << "\n"
                   << "    error:      " << price - cachedPrice2);
    }

    // stub date

    Schedule sch3(Date(30, November, 2004), Date(30, March, 2009), Period(Semiannual),
                  UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted, DateGeneration::Backward, false,
                  Date(), Date(30, November, 2008));

    FixedRateBond bond3(settlementDays, vars.faceAmount, sch3, couponRates, ActualActual(ActualActual::ISMA),
                        ModifiedFollowing, 100.0, Date(30, November, 2004));

    bond3.setPricingEngine(bondEngine);

    Real cachedPrice3 = 100.382794;

    price = bond3.cleanPrice();
    if (std::fabs(price - cachedPrice3) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice3 << "\n"
                   << "    error:      " << price - cachedPrice3);
    }
}

BOOST_AUTO_TEST_CASE(testCachedFloating)
{

    BOOST_TEST_MESSAGE("Testing floating-rate bond prices against cached values...");

    bool usingAtParCoupons = IborCoupon::Settings::instance().usingAtParCoupons();

    CommonVars vars;

    Date today(22, November, 2004);
    Settings::instance().evaluationDate() = today;

    Natural settlementDays = 1;

    Handle<YieldTermStructure> riskFreeRate(flatRate(today, 0.025, Actual360()));
    Handle<YieldTermStructure> discountCurve(flatRate(today, 0.03, Actual360()));

    ext::shared_ptr<IborIndex> index(new USDLibor(6 * Months, riskFreeRate));
    Natural fixingDays = 1;

    Real tolerance = 1.0e-6;

    ext::shared_ptr<IborCouponPricer> pricer(new BlackIborCouponPricer(Handle<OptionletVolatilityStructure>()));

    // plain

    Schedule sch(Date(30, November, 2004), Date(30, November, 2008), Period(Semiannual),
                 UnitedStates(UnitedStates::GovernmentBond), ModifiedFollowing, ModifiedFollowing,
                 DateGeneration::Backward, false);

    FloatingRateBond bond1(settlementDays, vars.faceAmount, sch, index, ActualActual(ActualActual::ISMA),
                           ModifiedFollowing, fixingDays, std::vector<Real>(), std::vector<Spread>(),
                           std::vector<Rate>(), std::vector<Rate>(), false, 100.0, Date(30, November, 2004));

    ext::shared_ptr<PricingEngine> bondEngine(new DiscountingBondEngine(riskFreeRate));
    bond1.setPricingEngine(bondEngine);

    setCouponPricer(bond1.cashflows(), pricer);

    Real cachedPrice1 = usingAtParCoupons ? 99.874646 : 99.874645;

    Real price = bond1.cleanPrice();
    if (std::fabs(price - cachedPrice1) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice1 << "\n"
                   << "    error:      " << price - cachedPrice1);
    }

    // different risk-free and discount curve

    FloatingRateBond bond2(settlementDays, vars.faceAmount, sch, index, ActualActual(ActualActual::ISMA),
                           ModifiedFollowing, fixingDays, std::vector<Rate>(), std::vector<Spread>(),
                           std::vector<Rate>(), std::vector<Rate>(), false, 100.0, Date(30, November, 2004));

    ext::shared_ptr<PricingEngine> bondEngine2(new DiscountingBondEngine(discountCurve));
    bond2.setPricingEngine(bondEngine2);

    setCouponPricer(bond2.cashflows(), pricer);

    Real cachedPrice2 = 97.955904;

    price = bond2.cleanPrice();
    if (std::fabs(price - cachedPrice2) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice2 << "\n"
                   << "    error:      " << price - cachedPrice2);
    }

    // varying spread

    std::vector<Rate> spreads(4);
    spreads[0] = 0.001;
    spreads[1] = 0.0012;
    spreads[2] = 0.0014;
    spreads[3] = 0.0016;

    FloatingRateBond bond3(settlementDays, vars.faceAmount, sch, index, ActualActual(ActualActual::ISMA),
                           ModifiedFollowing, fixingDays, std::vector<Real>(), spreads, std::vector<Rate>(),
                           std::vector<Rate>(), false, 100.0, Date(30, November, 2004));

    bond3.setPricingEngine(bondEngine2);

    setCouponPricer(bond3.cashflows(), pricer);

    Real cachedPrice3 = usingAtParCoupons ? 98.495459 : 98.495458;

    price = bond3.cleanPrice();
    if (std::fabs(price - cachedPrice3) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice3 << "\n"
                   << "    error:      " << price - cachedPrice3);
    }

    Schedule sch2(Date(26, November, 2003), Date(26, November, 2007), Period(Semiannual),
                  UnitedStates(UnitedStates::GovernmentBond), ModifiedFollowing, ModifiedFollowing,
                  DateGeneration::Backward, false);
    FloatingRateBond bond4(settlementDays, vars.faceAmount, sch2, index, ActualActual(ActualActual::ISMA),
                           ModifiedFollowing, fixingDays, std::vector<Real>(), spreads, std::vector<Rate>(),
                           std::vector<Rate>(), false, 100.0, Date(29, October, 2004), Period(6 * Days));

    index->addFixing(Date(25, May, 2004), 0.0402);
    bond4.setPricingEngine(bondEngine2);

    setCouponPricer(bond4.cashflows(), pricer);

    Real cachedPrice4 = usingAtParCoupons ? 98.892055 : 98.892346;

    price = bond4.cleanPrice();
    if (std::fabs(price - cachedPrice4) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << price << "\n"
                   << "    expected:   " << cachedPrice4 << "\n"
                   << "    error:      " << price - cachedPrice4);
    }
}

BOOST_AUTO_TEST_CASE(testBrazilianCached)
{

    BOOST_TEST_MESSAGE("Testing Brazilian public bond prices against Andima cached values...");

    CommonVars vars;

    Natural settlementDays = 1;
    Real faceAmount = 1000.0;
    Date today(6, June, 2007);
    Date issueDate(1, January, 2007);

    // The tolerance is high because Andima truncate yields
    Real tolerance = 1.0e-4;

    // Reset evaluation date
    Settings::instance().evaluationDate() = today;

    // NTN-F maturity dates
    std::vector<Date> maturityDates(6);
    maturityDates[0] = Date(1, January, 2008);
    maturityDates[1] = Date(1, January, 2010);
    maturityDates[2] = Date(1, July, 2010);
    maturityDates[3] = Date(1, January, 2012);
    maturityDates[4] = Date(1, January, 2014);
    maturityDates[5] = Date(1, January, 2017);

    // Andima NTN-F yields
    std::vector<Rate> yields(6);
    yields[0] = 0.114614;
    yields[1] = 0.105726;
    yields[2] = 0.105328;
    yields[3] = 0.104283;
    yields[4] = 0.103218;
    yields[5] = 0.102948;

    // Andima NTN-F prices
    std::vector<Rate> prices(6);
    prices[0] = 1034.63031372;
    prices[1] = 1030.09919487;
    prices[2] = 1029.98307160;
    prices[3] = 1028.13585068;
    prices[4] = 1028.33383817;
    prices[5] = 1026.19716497;

    std::vector<InterestRate> couponRates(1);
    couponRates[0] = InterestRate(0.1, Thirty360(Thirty360::BondBasis), Compounded, Annual);

    for (Size bondIndex = 0; bondIndex < maturityDates.size(); bondIndex++)
    {

        InterestRate yield(yields[bondIndex], Business252(Brazil()), Compounded, Annual);

        Schedule schedule(Date(1, January, 2007), maturityDates[bondIndex], Period(Semiannual),
                          Brazil(Brazil::Settlement), Unadjusted, Unadjusted, DateGeneration::Backward, false);

        Leg coupons = FixedRateLeg(schedule)
                          .withNotionals(faceAmount)
                          .withCouponRates(couponRates)
                          .withPaymentAdjustment(Following);

        Bond bond(settlementDays, schedule.calendar(), issueDate, coupons);

        Real cachedPrice = prices[bondIndex];
        Real price = faceAmount *
                     (BondFunctions::cleanPrice(bond, yield.rate(), yield.dayCounter(), yield.compounding(),
                                                yield.frequency(), today) +
                      bond.accruedAmount(today)) /
                     100.0;

        if (std::fabs(price - cachedPrice) > tolerance)
        {
            BOOST_ERROR("failed to reproduce Andima cached price:\n"
                        << std::fixed << "    calculated: " << price << "\n"
                        << "    expected:   " << cachedPrice << "\n"
                        << "    error:      " << price - cachedPrice << "\n");
        }
    }
}

BOOST_AUTO_TEST_CASE(testExCouponGilt)
{
    BOOST_TEST_MESSAGE("Testing ex-coupon UK Gilt price against market values...");
    /* UK Gilts have an exCouponDate 7 business days before the coupon
       is due (see <http://www.dmo.gov.uk/index.aspx?page=Gilts/Gilt_Faq>).
       On the exCouponDate the bond still trades cum-coupon so we use
       6 days below and UK calendar

       Output verified with Bloomberg:

       ISIN: GB0009997999
       Issue Date: February 29th, 1996
       Interest Accrue: February 29th, 1996
       First Coupon: June 7th, 1996
       Maturity: June 7th, 2021
       coupon: 8
       period: 6M

       Settlement date: May 29th, 2013
       Test Price : 103
       Accrued : 38021.97802
       NPV : 106.8021978
       Yield : 7.495180593
       Yield->NPV : 106.8021978
       Yield->NPV->Price : 103
       Mod duration : 5.676044458
       Convexity : 0.4215314859
       PV 0.01 : 0.0606214023

       Settlement date: May 30th, 2013
       Test Price : 103
       Accrued : -1758.241758
       NPV : 102.8241758
       Yield : 7.496183543
       Yield->NPV : 102.8241758
       Yield->NPV->Price : 103
       Mod duration : 5.892816328
       Convexity : 0.4375621862
       PV 0.01 : 0.06059239822

       Settlement date: May 31st, 2013
       Test Price : 103
       Accrued : -1538.461538
       NPV : 102.8461538
       Yield : 7.495987492
       Yield->NPV : 102.8461539
       Yield->NPV->Price : 103
       Mod duration : 5.890186028
       Convexity : 0.4372394381
       PV 0.01 : 0.06057829784
    */
    struct test_case
    {
        Date settlementDate;
        Real testPrice;
        Real accruedAmount;
        Real NPV;
        Rate yield;
        Real duration;
        Real convexity;
    };

    Calendar calendar = UnitedKingdom();

    Natural settlementDays = 3;

    Date issueDate(29, February, 1996);
    Date startDate(29, February, 1996);
    Date firstCouponDate(07, June, 1996);
    Date maturityDate(07, June, 2021);

    Rate coupon = 0.08;

    Period tenor = 6 * Months;
    Period exCouponPeriod = 6 * Days;

    Compounding comp = Compounded;
    Frequency freq = Semiannual;

    Schedule schedule = Schedule(startDate, maturityDate, tenor, NullCalendar(), Unadjusted, Unadjusted,
                                 DateGeneration::Forward, true, firstCouponDate);
    DayCounter dc = ActualActual(ActualActual::ISMA, schedule);

    FixedRateBond bond(settlementDays, 100.0, schedule, std::vector<Rate>(1, coupon), dc, Unadjusted, 100.0, issueDate,
                       calendar, exCouponPeriod, calendar);

    const Leg& leg = bond.cashflows();

    test_case cases[] = {{Date(29, May, 2013), 103.0, 3.8021978, 106.8021978, 0.0749518, 5.6760445, 42.1531486},
                         {Date(30, May, 2013), 103.0, -0.1758242, 102.8241758, 0.0749618, 5.8928163, 43.7562186},
                         {Date(31, May, 2013), 103.0, -0.1538462, 102.8461538, 0.0749599, 5.8901860, 43.7239438}};

    for (auto& i : cases)
    {
        Real accrued = bond.accruedAmount(i.settlementDate);
        ASSERT_CLOSE("accrued amount", i.settlementDate, accrued, i.accruedAmount, 1e-6);

        Real npv = i.testPrice + accrued;
        ASSERT_CLOSE("NPV", i.settlementDate, npv, i.NPV, 1e-6);

        Rate yield = CashFlows::yield(leg, npv, dc, comp, freq, false, i.settlementDate);
        ASSERT_CLOSE("yield", i.settlementDate, yield, i.yield, 1e-6);

        Time duration = CashFlows::duration(leg, yield, dc, comp, freq, Duration::Modified, false, i.settlementDate);
        ASSERT_CLOSE("duration", i.settlementDate, duration, i.duration, 1e-6);

        Real convexity = CashFlows::convexity(leg, yield, dc, comp, freq, false, i.settlementDate);
        ASSERT_CLOSE("convexity", i.settlementDate, convexity, i.convexity, 1e-6);

        Real calcnpv = CashFlows::npv(leg, yield, dc, comp, freq, false, i.settlementDate);
        ASSERT_CLOSE("NPV from yield", i.settlementDate, calcnpv, i.NPV, 1e-6);

        Real calcprice = calcnpv - accrued;
        ASSERT_CLOSE("price from yield", i.settlementDate, calcprice, i.testPrice, 1e-6);
    }
}

BOOST_AUTO_TEST_CASE(testExCouponAustralianBond)
{
    BOOST_TEST_MESSAGE("Testing ex-coupon Australian bond price against market values...");
    /* Australian Government Bonds have an exCouponDate 7 calendar
       days before the coupon is due.  On the exCouponDate the bond
       trades ex-coupon so we use 7 days below and NullCalendar.
       AGB accrued interest is rounded to 3dp.

       Output verified with Bloomberg:

       ISIN: AU300TB01208
       Issue Date: June 10th, 2004
       Interest Accrue: February 15th, 2004
       First Coupon: August 15th, 2004
       Maturity: February 15th, 2017
       coupon: 6
       period: 6M

       Settlement date: August 7th, 2014
       Test Price : 103
       Accrued : 28670
       NPV : 105.867
       Yield : 4.723814867
       Yield->NPV : 105.867
       Yield->NPV->Price : 103
       Mod duration : 2.262763296
       Convexity : 0.0654870275
       PV 0.01 : 0.02395519619

       Settlement date: August 8th, 2014
       Test Price : 103
       Accrued : -1160
       NPV : 102.884
       Yield : 4.72354833
       Yield->NPV : 102.884
       Yield->NPV->Price : 103
       Mod duration : 2.325360055
       Convexity : 0.06725307785
       PV 0.01 : 0.02392423439

       Settlement date: August 11th, 2014
       Test Price : 103
       Accrued : -660
       NPV : 102.934
       Yield : 4.719277687
       Yield->NPV : 102.934
       Yield->NPV->Price : 103
       Mod duration : 2.317320093
       Convexity : 0.06684074058
       PV 0.01 : 0.02385310264
    */
    struct test_case
    {
        Date settlementDate;
        Real testPrice;
        Real accruedAmount;
        Real NPV;
        Rate yield;
        Real duration;
        Real convexity;
    };

    Calendar calendar = Australia();

    Natural settlementDays = 3;

    Date issueDate(10, June, 2004);
    Date startDate(15, February, 2004);
    Date firstCouponDate(15, August, 2004);
    Date maturityDate(15, February, 2017);

    Rate coupon = 0.06;

    Period tenor = 6 * Months;
    Period exCouponPeriod = 7 * Days;

    Compounding comp = Compounded;
    Frequency freq = Semiannual;

    Schedule schedule = Schedule(startDate, maturityDate, tenor, NullCalendar(), Unadjusted, Unadjusted,
                                 DateGeneration::Forward, true, firstCouponDate);
    DayCounter dc = ActualActual(ActualActual::ISMA, schedule);

    FixedRateBond bond(settlementDays, 100.0, schedule, std::vector<Rate>(1, coupon), dc, Unadjusted, 100.0, issueDate,
                       calendar, exCouponPeriod, NullCalendar());

    const Leg& leg = bond.cashflows();

    test_case cases[] = {{Date(7, August, 2014), 103.0, 2.8670, 105.867, 0.04723, 2.26276, 6.54870},
                         {Date(8, August, 2014), 103.0, -0.1160, 102.884, 0.047235, 2.32536, 6.72531},
                         {Date(11, August, 2014), 103.0, -0.0660, 102.934, 0.04719, 2.31732, 6.68407}};

    for (auto& i : cases)
    {
        Real accrued = bond.accruedAmount(i.settlementDate);
        ASSERT_CLOSE("accrued amount", i.settlementDate, accrued, i.accruedAmount, 1e-3);

        Real npv = i.testPrice + accrued;
        ASSERT_CLOSE("NPV", i.settlementDate, npv, i.NPV, 1e-3);

        Rate yield = CashFlows::yield(leg, npv, dc, comp, freq, false, i.settlementDate);
        ASSERT_CLOSE("yield", i.settlementDate, yield, i.yield, 1e-5);

        Time duration = CashFlows::duration(leg, yield, dc, comp, freq, Duration::Modified, false, i.settlementDate);
        ASSERT_CLOSE("duration", i.settlementDate, duration, i.duration, 1e-5);

        Real convexity = CashFlows::convexity(leg, yield, dc, comp, freq, false, i.settlementDate);
        ASSERT_CLOSE("convexity", i.settlementDate, convexity, i.convexity, 1e-4);

        Real calcnpv = CashFlows::npv(leg, yield, dc, comp, freq, false, i.settlementDate);
        ASSERT_CLOSE("NPV from yield", i.settlementDate, calcnpv, i.NPV, 1e-3);

        Real calcprice = calcnpv - accrued;
        ASSERT_CLOSE("price from yield", i.settlementDate, calcprice, i.testPrice, 1e-3);
    }
}

/// <summary>
/// Test calculation of South African R2048 bond
/// This requires the use of the Schedule to be constructed
/// with a custom date vector
/// </summary>
BOOST_AUTO_TEST_CASE(testBondFromScheduleWithDateVector)
{
    BOOST_TEST_MESSAGE("Testing South African R2048 bond price using Schedule constructor with Date vector...");
    // When pricing bond from Yield To Maturity, use NullCalendar()
    Calendar calendar = NullCalendar();

    Natural settlementDays = 3;

    Date issueDate(29, June, 2012);
    Date today(7, September, 2015);
    Date evaluationDate = calendar.adjust(today);
    Date settlementDate = calendar.advance(evaluationDate, settlementDays * Days);
    Settings::instance().evaluationDate() = evaluationDate;

    // For the schedule to generate correctly for Feb-28's, make maturity date on Feb 29
    Date maturityDate(29, February, 2048);

    Rate coupon = 0.0875;
    Compounding comp = Compounded;
    Frequency freq = Semiannual;


    Period tenor = 6 * Months;
    Period exCouponPeriod = 10 * Days;

    // Generate coupon dates for 31 Aug and end of Feb each year
    // For leap years, this will generate 29 Feb, but the bond
    // actually pays coupons on 28 Feb, regardsless of whether
    // it is a leap year or not.
    Schedule schedule(issueDate, maturityDate, tenor, NullCalendar(), Unadjusted, Unadjusted, DateGeneration::Backward,
                      true);

    // Adjust the 29 Feb's to 28 Feb
    std::vector<Date> dates;
    for (Size i = 0; i < schedule.size(); ++i)
    {
        Date d = schedule.date(i);
        if (d.month() == February && d.dayOfMonth() == 29)
            dates.emplace_back(28, February, d.year());
        else
            dates.push_back(d);
    }

    schedule = Schedule(dates, schedule.calendar(), schedule.businessDayConvention(),
                        schedule.terminationDateBusinessDayConvention(), schedule.tenor(), schedule.rule(),
                        schedule.endOfMonth(), schedule.isRegular());
    DayCounter dc = ActualActual(ActualActual::Bond, schedule);
    FixedRateBond bond(0, 100.0, schedule, std::vector<Rate>(1, coupon), dc, Following, 100.0, issueDate, calendar,
                       exCouponPeriod, calendar, Unadjusted, false);

    // Yield as quoted in market
    InterestRate yield(0.09185, dc, comp, freq);

    Real calculatedPrice = BondFunctions::dirtyPrice(bond, yield, settlementDate);
    Real expectedPrice = 95.75706;
    Real tolerance = 1e-5;
    if (std::fabs(calculatedPrice - expectedPrice) > tolerance)
    {
        BOOST_FAIL("failed to reproduce R2048 dirty price" << std::fixed << std::setprecision(5) << "\n  expected:   "
                                                           << expectedPrice << "\n  calculated: " << calculatedPrice);
    }
}

BOOST_AUTO_TEST_CASE(testFixedBondWithGivenDates)
{

    BOOST_TEST_MESSAGE("Testing fixed-coupon bond built on schedule with given dates...");

    CommonVars vars;

    Date today(22, November, 2004);
    Settings::instance().evaluationDate() = today;

    Natural settlementDays = 1;

    Handle<YieldTermStructure> discountCurve(flatRate(today, 0.03, Actual360()));

    Real tolerance = 1.0e-6;

    ext::shared_ptr<PricingEngine> bondEngine(new DiscountingBondEngine(discountCurve));
    // plain

    Schedule sch1(Date(30, November, 2004), Date(30, November, 2008), Period(Semiannual),
                  UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted, DateGeneration::Backward, false);
    FixedRateBond bond1(settlementDays, vars.faceAmount, sch1, std::vector<Rate>(1, 0.02875),
                        ActualActual(ActualActual::ISMA), ModifiedFollowing, 100.0, Date(30, November, 2004));
    bond1.setPricingEngine(bondEngine);

    Schedule sch1_copy(sch1.dates(), UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted,
                       Period(Semiannual), DateGeneration::Backward, false, std::vector<bool>(sch1.size() - 1, true));
    FixedRateBond bond1_copy(settlementDays, vars.faceAmount, sch1_copy, std::vector<Rate>(1, 0.02875),
                             ActualActual(ActualActual::ISMA), ModifiedFollowing, 100.0, Date(30, November, 2004));
    bond1_copy.setPricingEngine(bondEngine);

    Real expected = bond1.cleanPrice();
    Real calculated = bond1_copy.cleanPrice();
    if (std::fabs(expected - calculated) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << calculated << "\n"
                   << "    expected:   " << expected << "\n"
                   << "    error:      " << expected - calculated);
    }

    // varying coupons

    std::vector<Rate> couponRates(4);
    couponRates[0] = 0.02875;
    couponRates[1] = 0.03;
    couponRates[2] = 0.03125;
    couponRates[3] = 0.0325;

    FixedRateBond bond2(settlementDays, vars.faceAmount, sch1, couponRates, ActualActual(ActualActual::ISMA),
                        ModifiedFollowing, 100.0, Date(30, November, 2004));
    bond2.setPricingEngine(bondEngine);

    FixedRateBond bond2_copy(settlementDays, vars.faceAmount, sch1_copy, couponRates, ActualActual(ActualActual::ISMA),
                             ModifiedFollowing, 100.0, Date(30, November, 2004));
    bond2_copy.setPricingEngine(bondEngine);

    expected = bond2.cleanPrice();
    calculated = bond2_copy.cleanPrice();
    if (std::fabs(expected - calculated) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << calculated << "\n"
                   << "    expected:   " << expected << "\n"
                   << "    error:      " << expected - calculated);
    }


    // stub date

    Schedule sch3(Date(30, November, 2004), Date(30, March, 2009), Period(Semiannual),
                  UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted, DateGeneration::Backward, false,
                  Date(), Date(30, November, 2008));
    FixedRateBond bond3(settlementDays, vars.faceAmount, sch3, couponRates, Actual360(), ModifiedFollowing, 100.0,
                        Date(30, November, 2004));
    bond3.setPricingEngine(bondEngine);

    Schedule sch3_copy(sch3.dates(), UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted,
                       Period(Semiannual), DateGeneration::Backward, false, std::vector<bool>(sch3.size() - 1, true));
    FixedRateBond bond3_copy(settlementDays, vars.faceAmount, sch3_copy, couponRates, Actual360(), ModifiedFollowing,
                             100.0, Date(30, November, 2004));
    bond3_copy.setPricingEngine(bondEngine);

    expected = bond3.cleanPrice();
    calculated = bond3_copy.cleanPrice();
    if (std::fabs(expected - calculated) > tolerance)
    {
        BOOST_FAIL("failed to reproduce cached price:\n"
                   << std::fixed << "    calculated: " << calculated << "\n"
                   << "    expected:   " << expected << "\n"
                   << "    error:      " << expected - calculated);
    }
}

BOOST_AUTO_TEST_CASE(testRiskyBondWithGivenDates)
{

    BOOST_TEST_MESSAGE("Testing risky bond engine...");

    CommonVars vars;

    Date today(22, November, 2005);
    Settings::instance().evaluationDate() = today;

    // Probability Structure
    Handle<Quote> hazardRate(ext::shared_ptr<Quote>(new SimpleQuote(0.1)));
    Handle<DefaultProbabilityTermStructure> defaultProbability(
        ext::shared_ptr<DefaultProbabilityTermStructure>(new FlatHazardRate(0, TARGET(), hazardRate, Actual360())));

    // Yield term structure
    RelinkableHandle<YieldTermStructure> riskFree;
    riskFree.linkTo(ext::shared_ptr<YieldTermStructure>(new FlatForward(today, 0.02, Actual360())));
    Schedule sch1(Date(30, November, 2004), Date(30, November, 2008), Period(Semiannual),
                  UnitedStates(UnitedStates::GovernmentBond), Unadjusted, Unadjusted, DateGeneration::Backward, false);

    // Create Bond
    Natural settlementDays = 1;
    std::vector<Real> notionals = {0.0167, 0.023, 0.03234, 0.034, 0.038, 0.042, 0.047, 0.053};

    std::vector<Rate> couponRates(4);
    couponRates[0] = 0.02875;
    couponRates[1] = 0.03;
    couponRates[2] = 0.03125;
    couponRates[3] = 0.0325;
    Real recoveryRate = 0.4;

    FixedRateBond bond(settlementDays, vars.faceAmount, sch1, couponRates, ActualActual(ActualActual::ISMA),
                       ModifiedFollowing, 100.0, Date(20, November, 2004));

    // Create Engine
    ext::shared_ptr<PricingEngine> bondEngine(new RiskyBondEngine(defaultProbability, recoveryRate, riskFree));
    bond.setPricingEngine(bondEngine);

    // Calculate and validate NPV and price
    Real expected = 888458.819055;
    Real calculated = bond.NPV();
    Real tolerance = 1.0e-6;
    if (std::fabs(expected - calculated) > tolerance)
    {
        BOOST_FAIL("Failed to reproduce risky bond NPV:\n"
                   << std::fixed << "    calculated: " << calculated << "\n"
                   << "    expected:   " << expected << "\n"
                   << "    error:      " << expected - calculated);
    }

    expected = 87.407883;
    calculated = bond.cleanPrice();
    if (std::fabs(expected - calculated) > tolerance)
    {
        BOOST_FAIL("Failed to reproduce risky bond price:\n"
                   << std::fixed << "    calculated: " << calculated << "\n"
                   << "    expected:   " << expected << "\n"
                   << "    error:      " << expected - calculated);
    }
}

BOOST_AUTO_TEST_CASE(testFixedRateBondWithArbitrarySchedule)
{
    BOOST_TEST_MESSAGE("Testing fixed-rate bond with arbitrary schedule...");
    Calendar calendar = NullCalendar();

    Natural settlementDays = 3;

    Date today(1, January, 2019);
    Settings::instance().evaluationDate() = today;

    // For the schedule to generate correctly for Feb-28's, make maturity date on Feb 29
    std::vector<Date> dates(4);
    dates[0] = Date(1, February, 2019);
    dates[1] = Date(7, February, 2019);
    dates[2] = Date(1, April, 2019);
    dates[3] = Date(27, May, 2019);

    Schedule schedule(dates, calendar, Unadjusted);

    Rate coupon = 0.01;
    DayCounter dc = Actual365Fixed();

    FixedRateBond bond(settlementDays, 100.0, schedule, std::vector<Rate>(1, coupon), dc, Following, 100.0);

    if (bond.frequency() != NoFrequency)
    {
        BOOST_ERROR("unexpected frequency: " << bond.frequency());
    }

    Handle<YieldTermStructure> discountCurve(flatRate(today, 0.03, Actual360()));
    bond.setPricingEngine(ext::shared_ptr<PricingEngine>(new DiscountingBondEngine(discountCurve)));

    BOOST_CHECK_NO_THROW(bond.cleanPrice());
}

BOOST_AUTO_TEST_CASE(testThirty360BondWithSettlementOn31st)
{
    BOOST_TEST_MESSAGE("Testing Thirty/360 bond with settlement on 31st of the month...");

    // cusip 3130A0X70, data is from Bloomberg
    Settings::instance().evaluationDate() = Date(28, July, 2017);

    Date datedDate(13, February, 2014);
    Date settlement(31, July, 2017);
    Date maturity(13, August, 2018);

    DayCounter dayCounter = Thirty360(Thirty360::USA);
    Compounding compounding = Compounded;

    Schedule fixedBondSchedule(datedDate, maturity, Period(Semiannual), UnitedStates(UnitedStates::GovernmentBond),
                               Unadjusted, Unadjusted, DateGeneration::Forward, false);

    FixedRateBond fixedRateBond(1, 100, fixedBondSchedule, std::vector<Rate>(1, 0.015), dayCounter, Unadjusted, 100.0);

    Bond::Price cleanPrice(100.0, Bond::Price::Clean);

    Real yield = BondFunctions::yield(fixedRateBond, cleanPrice, dayCounter, compounding, Semiannual, settlement);
    ASSERT_CLOSE("yield", settlement, yield, 0.015, 1e-4);

    Real duration = BondFunctions::duration(fixedRateBond, InterestRate(yield, dayCounter, compounding, Semiannual),
                                            Duration::Macaulay, settlement);
    ASSERT_CLOSE("duration", settlement, duration, 1.022, 1e-3);

    Real convexity =
        BondFunctions::convexity(fixedRateBond, InterestRate(yield, dayCounter, compounding, Semiannual), settlement) /
        100;
    ASSERT_CLOSE("convexity", settlement, convexity, 0.015, 1e-3);

    Real accrued = BondFunctions::accruedAmount(fixedRateBond, settlement);
    ASSERT_CLOSE("accrued", settlement, accrued, 0.7, 1e-6);
}

BOOST_AUTO_TEST_CASE(testBasisPointValue)
{

    BOOST_TEST_MESSAGE("Testing consistency of bond basisPointValue and yieldValueBasisPoint calculations...");

    CommonVars vars;

    Date today(29, January, 2024);
    Settings::instance().evaluationDate() = today;

    Date datedDate(15, November, 2023);
    Date maturity(15, August, 2033);

    DayCounter dayCounter = Thirty360(Thirty360::USA);
    Compounding compounding = Compounded;
    Frequency frequency = Semiannual;
    auto period = Period(frequency);

    Schedule fixedBondSchedule(datedDate, maturity, period, UnitedStates(UnitedStates::GovernmentBond), Unadjusted,
                               Unadjusted, DateGeneration::Forward, false);

    FixedRateBond fixedRateBond(1, vars.faceAmount, fixedBondSchedule, std::vector<Rate>(1, 0.045), dayCounter,
                                Unadjusted, 100.0);

    Date defaultSettlement = fixedRateBond.settlementDate();
    Bond::Price cleanPrice(102.890625, Bond::Price::Clean);

    Real tolerance = 1e-6;
    Real repriceTolerance = 1e-3;

    Real yield = BondFunctions::yield(fixedRateBond, cleanPrice, dayCounter, compounding, frequency);
    ASSERT_CLOSE("yield", defaultSettlement, yield, 0.041301, tolerance);

    struct test_case
    {
        Date settlement;
        Real bpv;
        Real yvbp;
    };
    test_case cases[] = {
        {Date(), -795.098043, -0.0012571287},
        {defaultSettlement, -795.098043, -0.0012571287},
        {Date(12, February, 2024), -792.789400, -0.0012607913},
    };

    for (auto& i : cases)
    {
        Real bvp1 =
            BondFunctions::basisPointValue(fixedRateBond, yield, dayCounter, compounding, frequency, i.settlement);
        ASSERT_CLOSE("basisPointValue from yield", i.settlement, bvp1, i.bpv, tolerance);
        Real bvp2 = BondFunctions::basisPointValue(
            fixedRateBond, InterestRate(yield, dayCounter, compounding, frequency), i.settlement);
        ASSERT_CLOSE("basisPointValue from InterestRate", i.settlement, bvp2, i.bpv, tolerance);

        Real price = BondFunctions::cleanPrice(fixedRateBond, yield, dayCounter, compounding, frequency, i.settlement);
        Real bumpedPrice =
            BondFunctions::cleanPrice(fixedRateBond, yield + 0.0001, dayCounter, compounding, frequency, i.settlement);
        Real reprice = (bumpedPrice - price) * vars.faceAmount / 100.0;
        ASSERT_CLOSE("basisPointValue against a 1bp reprice", i.settlement, bvp1, reprice, repriceTolerance);

        Real yvbp1 =
            BondFunctions::yieldValueBasisPoint(fixedRateBond, yield, dayCounter, compounding, frequency, i.settlement);
        yvbp1 *= vars.faceAmount;
        ASSERT_CLOSE("yieldValueBasisPoint from yield", i.settlement, yvbp1, i.yvbp, tolerance);
        Real yvbp2 = BondFunctions::yieldValueBasisPoint(
            fixedRateBond, InterestRate(yield, dayCounter, compounding, frequency), i.settlement);
        yvbp2 *= vars.faceAmount;
        ASSERT_CLOSE("yieldValueBasisPoint from InterestRate", i.settlement, yvbp2, i.yvbp, tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testFixingConvention)
{

    BOOST_TEST_MESSAGE("Testing floating-rate bond fixing convention...");

    // June 22, 2024 is a Saturday. With an unadjusted quarterly schedule
    // and fixingDays=0, the fixing date depends on the convention:
    //   Preceding  → Friday June 21, 2024
    //   Following  → Monday June 24, 2024

    Date today(1, January, 2024);
    Settings::instance().evaluationDate() = today;

    Calendar calendar = Australia();
    Natural settlementDays = 0;
    Real faceAmount = 100.0;

    Schedule schedule(Date(22, March, 2024), Date(22, December, 2024), Period(Quarterly), calendar, Unadjusted,
                      Unadjusted, DateGeneration::Forward, false);

    Handle<YieldTermStructure> curve(flatRate(today, 0.05, Actual365Fixed()));
    auto index = ext::make_shared<AUDLibor>(3 * Months, curve);

    // Default (Preceding) convention
    FloatingRateBond bondPreceding(settlementDays, faceAmount, schedule, index, Actual365Fixed(), Following, 0);

    // Following convention
    FloatingRateBond bondFollowing(settlementDays, faceAmount, schedule, index, Actual365Fixed(), Following, 0,
                                   std::vector<Real>{1.0}, std::vector<Spread>{0.0}, std::vector<Rate>(),
                                   std::vector<Rate>(), false, 100.0, Date(), Period(), Calendar(), Unadjusted, false,
                                   Following);

    // Extract the coupon whose accrual start date is June 22, 2024
    auto findCouponStarting = [](const FloatingRateBond& bond, const Date& d)
    {
        for (const auto& cf : bond.cashflows())
        {
            auto coupon = ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
            if (coupon && coupon->accrualStartDate() == d)
                return coupon;
        }
        return ext::shared_ptr<FloatingRateCoupon>();
    };

    Date june22(22, June, 2024); // Saturday
    auto couponP = findCouponStarting(bondPreceding, june22);
    auto couponF = findCouponStarting(bondFollowing, june22);

    BOOST_REQUIRE(couponP != nullptr);
    BOOST_REQUIRE(couponF != nullptr);

    Date expectedPreceding(21, June, 2024); // Friday
    Date expectedFollowing(24, June, 2024); // Monday

    BOOST_CHECK_EQUAL(couponP->fixingDate(), expectedPreceding);
    BOOST_CHECK_EQUAL(couponF->fixingDate(), expectedFollowing);
}

BOOST_AUTO_TEST_CASE(testMixedCompoundingInLastPeriods)
{
    BOOST_TEST_MESSAGE("Testing mixed compounding rules in the last periods of a bond...");

    // 2.125% semiannual, with 94 of the 182 days of its last period run.
    Date issue(1, June, 2016), maturity(1, June, 2026), settlement(5, March, 2026);
    Settings::instance().evaluationDate() = settlement;

    DayCounter dayCounter = ActualActual(ActualActual::ISMA);
    Schedule schedule(issue, maturity, Period(Semiannual), NullCalendar(), Unadjusted, Unadjusted,
                      DateGeneration::Backward, false);
    FixedRateBond bond(0, 100.0, schedule, std::vector<Rate>(1, 0.02125), dayCounter, Unadjusted, 100.0, issue);

    const Real coupon = 100.0 * 0.02125 / 2.0;
    const Real cleanPrice = 100.185;
    const Real dirtyPrice = cleanPrice + coupon * 94.0 / 182.0;
    const Time t = 0.5 * 88.0 / 182.0;

    // One interval is left, which is both the first and the final one, so the
    // rules agree on discounting it simply.
    const Rate simpleYield = ((100.0 + coupon) / dirtyPrice - 1.0) / t;
    for (Compounding rule : {SimpleThenCompounded, CompoundedThenSimple})
    {
        Rate calculated =
            BondFunctions::yield(bond, {cleanPrice, Bond::Price::Clean}, dayCounter, rule, Semiannual, settlement);
        ASSERT_CLOSE("yield in the last period under " << rule, settlement, calculated, simpleYield, 1.0e-9);
    }

    // With the redemption three days after the last coupon, two intervals are
    // left and each rule discounts a different one simply.
    Leg leg = FixedRateLeg(schedule)
                  .withNotionals(100.0)
                  .withCouponRates(0.02125, dayCounter)
                  .withPaymentAdjustment(Unadjusted);
    Date redemptionDate = maturity + 3;
    leg.push_back(ext::make_shared<Redemption>(100.0, redemptionDate));
    Bond lateRedemption(0, NullCalendar(), 100.0, redemptionDate, issue, leg);

    const Rate y = 0.05;
    const Time t2 = 3.0 / 365.0;
    const Real expectedFirstSimple = (coupon + 100.0 * std::pow(1.0 + y / 2.0, -2.0 * t2)) / (1.0 + y * t);
    const Real expectedFinalSimple = std::pow(1.0 + y / 2.0, -2.0 * t) * (coupon + 100.0 / (1.0 + y * t2));
    Real firstSimple = BondFunctions::dirtyPrice(
        lateRedemption, InterestRate(y, dayCounter, SimpleThenCompounded, Semiannual), settlement);
    Real finalSimple = BondFunctions::dirtyPrice(
        lateRedemption, InterestRate(y, dayCounter, CompoundedThenSimple, Semiannual), settlement);
    ASSERT_CLOSE("price under SimpleThenCompounded", settlement, firstSimple, expectedFirstSimple, 1.0e-9);
    ASSERT_CLOSE("price under CompoundedThenSimple", settlement, finalSimple, expectedFinalSimple, 1.0e-9);

    // Solved from the same closed forms.
    Rate firstSimpleYield = BondFunctions::yield(lateRedemption, {cleanPrice, Bond::Price::Clean}, dayCounter,
                                                 SimpleThenCompounded, Semiannual, settlement);
    Rate finalSimpleYield = BondFunctions::yield(lateRedemption, {cleanPrice, Bond::Price::Clean}, dayCounter,
                                                 CompoundedThenSimple, Semiannual, settlement);
    ASSERT_CLOSE("yield under SimpleThenCompounded", settlement, firstSimpleYield, 0.013059382713, 1.0e-9);
    ASSERT_CLOSE("yield under CompoundedThenSimple", settlement, finalSimpleYield, 0.013079316928, 1.0e-9);

    // After the last coupon only the redemption is left, two days away.
    Date afterLastCoupon(2, June, 2026);
    const Rate lastYield = (100.0 / 99.99 - 1.0) / (2.0 / 365.0);
    for (Compounding rule : {SimpleThenCompounded, CompoundedThenSimple})
    {
        Rate calculated = BondFunctions::yield(lateRedemption, {99.99, Bond::Price::Clean}, dayCounter, rule,
                                               Semiannual, afterLastCoupon);
        ASSERT_CLOSE("yield after the last coupon under " << rule, afterLastCoupon, calculated, lastYield, 1.0e-9);
    }
}

BOOST_AUTO_TEST_CASE(testMixedCompoundingWithLongFirstCoupon)
{
    BOOST_TEST_MESSAGE("Testing mixed compounding rules on a bond with a long first coupon...");

    // A year to the first coupon and semiannual after that, with a 5% coupon.
    // At a 5% yield what's left at the first coupon date is worth exactly 105
    // there, so each rule comes down to one step over the time to that date.
    Date issue(1, January, 2026);
    Settings::instance().evaluationDate() = issue;

    const std::vector<Date> dates = {Date(1, January, 2026), Date(1, January, 2027), Date(1, July, 2027),
                                     Date(1, January, 2028), Date(1, July, 2028),    Date(1, January, 2029)};
    const Rate y = 0.05;

    auto check =
        [&](const Schedule& schedule, const DayCounter& dayCounter, const Date& settlement, Time t, Real accrued)
    {
        FixedRateBond bond(0, 100.0, schedule, std::vector<Rate>(1, y), dayCounter, Unadjusted, 100.0, issue);
        Real calculatedAccrued = BondFunctions::accruedAmount(bond, settlement);
        ASSERT_CLOSE("accrued amount", settlement, calculatedAccrued, accrued, 1.0e-9);

        const std::pair<Compounding, Real> cases[] = {
            {SimpleThenCompounded, 105.0 / (1.0 + y * t) - accrued},
            {CompoundedThenSimple, 105.0 * std::pow(1.0 + y / 2.0, -2.0 * t) - accrued},
        };
        for (const auto& [rule, expected] : cases)
        {
            Real price = BondFunctions::cleanPrice(bond, InterestRate(y, dayCounter, rule, Semiannual), settlement);
            ASSERT_CLOSE("clean price under " << rule, settlement, price, expected, 1.0e-9);

            // The guess is off the root, so that the solver has to get there.
            Rate calculated = BondFunctions::yield(bond, {expected, Bond::Price::Clean}, dayCounter, rule, Semiannual,
                                                   settlement, 1.0e-10, 100, 0.02);
            ASSERT_CLOSE("yield under " << rule, settlement, calculated, y, 1.0e-9);
        }
    };

    // From bare dates, the day counter has no quasi-coupon periods to go by.
    Schedule bareDates(dates);
    DayCounter plain = ActualActual(ActualActual::ISMA);
    check(bareDates, plain, issue, 1.0, 0.0);
    check(bareDates, plain, Date(1, April, 2026), 275.0 / 365.0, 100.0 * y * 90.0 / 365.0);

    // With them, 90 of the 181 days of the first half-year have run.
    Schedule tagged(dates, NullCalendar(), Unadjusted, Unadjusted, Period(Semiannual), DateGeneration::Backward, false,
                    std::vector<bool>{false, true, true, true, true});
    check(tagged, ActualActual(ActualActual::ISMA, tagged), Date(1, April, 2026), 1.0 - 90.0 / 362.0,
          100.0 * y * 90.0 / 362.0);
}

BOOST_AUTO_TEST_CASE(testYieldSensitivitiesAreThoseOfTheNpv)
{
    BOOST_TEST_MESSAGE("Testing that yield-based sensitivities are those of the NPV...");

    Date issue(1, January, 2026);
    Settings::instance().evaluationDate() = issue;

    const Compounding rules[] = {Simple, Compounded, Continuous, SimpleThenCompounded, CompoundedThenSimple};
    const Rate r = 0.04;
    const Real h = 1.0e-4;

    auto checkDerivatives = [&](const Leg& leg, const DayCounter& dayCounter, const Date& settlement)
    {
        for (Compounding rule : rules)
        {
            auto npv = [&](Rate y)
            { return CashFlows::npv(leg, InterestRate(y, dayCounter, rule, Semiannual), false, settlement); };
            const Real P = npv(r), up = npv(r + h), down = npv(r - h);
            const Real expectedDuration = -(up - down) / (2.0 * h * P);
            const Real expectedConvexity = (up - 2.0 * P + down) / (h * h * P);

            const InterestRate y(r, dayCounter, rule, Semiannual);
            Real duration = CashFlows::duration(leg, y, Duration::Modified, false, settlement);
            Real convexity = CashFlows::convexity(leg, y, false, settlement);
            ASSERT_CLOSE("modified duration under " << rule, settlement, duration, expectedDuration, 1.0e-6);
            ASSERT_CLOSE("convexity under " << rule, settlement, convexity, expectedConvexity, 1.0e-5);
        }
    };

    // A long first coupon, then a late redemption in the last period.
    const std::vector<Date> dates = {Date(1, January, 2026), Date(1, January, 2027), Date(1, July, 2027),
                                     Date(1, January, 2028), Date(1, July, 2028),    Date(1, January, 2029)};
    DayCounter isma = ActualActual(ActualActual::ISMA);
    Leg longFirst = FixedRateLeg(Schedule(dates))
                        .withNotionals(100.0)
                        .withCouponRates(0.05, isma)
                        .withPaymentAdjustment(Unadjusted);
    checkDerivatives(longFirst, isma, issue);

    Schedule regular(Date(1, June, 2016), Date(1, June, 2026), Period(Semiannual), NullCalendar(), Unadjusted,
                     Unadjusted, DateGeneration::Backward, false);
    Leg lateRedemption =
        FixedRateLeg(regular).withNotionals(100.0).withCouponRates(0.02125, isma).withPaymentAdjustment(Unadjusted);
    lateRedemption.push_back(ext::make_shared<Redemption>(100.0, Date(4, June, 2026)));
    checkDerivatives(lateRedemption, isma, Date(5, March, 2026));

    // A coupon-rate bump moves the npv by what bps says, whatever the flows
    // around the coupons and whichever date the npv is at.
    DayCounter thirty = Thirty360(Thirty360::BondBasis);
    Schedule schedule(Date(15, June, 2026), Date(15, June, 2031), Period(Semiannual), NullCalendar(), Unadjusted,
                      Unadjusted, DateGeneration::Backward, false);
    auto bondLeg = [&](Rate coupon, bool exCoupon, bool leadingFlow)
    {
        FixedRateLeg maker(schedule);
        maker.withNotionals(100.0).withCouponRates(coupon, thirty).withPaymentAdjustment(Unadjusted);
        if (exCoupon)
            maker.withExCouponPeriod(10 * Days, NullCalendar(), Unadjusted);
        Leg coupons = maker;
        Leg result;
        if (leadingFlow)
            result.push_back(ext::make_shared<SimpleCashFlow>(5.0, Date(1, September, 2026)));
        result.insert(result.end(), coupons.begin(), coupons.end());
        return result;
    };

    struct BpsCase
    {
        bool exCoupon, leadingFlow;
        Date settlement, npvDate;
    };
    const BpsCase bpsCases[] = {
        {false, false, Date(15, August, 2026), Date()},
        {true, false, Date(10, December, 2026), Date()},
        {false, true, Date(15, August, 2026), Date()},
        {false, false, Date(15, August, 2026), Date(1, July, 2026)},
    };
    for (const auto& c : bpsCases)
    {
        for (Compounding rule : rules)
        {
            const InterestRate y(r, thirty, rule, Semiannual);
            Real bps = CashFlows::bps(bondLeg(0.03, c.exCoupon, c.leadingFlow), y, false, c.settlement, c.npvDate);
            Real bumped =
                CashFlows::npv(bondLeg(0.03 + 1.0e-4, c.exCoupon, c.leadingFlow), y, false, c.settlement, c.npvDate);
            Real base = CashFlows::npv(bondLeg(0.03, c.exCoupon, c.leadingFlow), y, false, c.settlement, c.npvDate);
            Real expected = bumped - base;
            ASSERT_CLOSE("bps under " << rule, c.settlement, bps, expected, 1.0e-10);
        }
    }

    // Two flows a year apart, by hand: the simple duration weights the time
    // of each with the discount the npv gives it.
    Leg twoFlows = {ext::make_shared<SimpleCashFlow>(10.0, Date(1, January, 2027)),
                    ext::make_shared<SimpleCashFlow>(110.0, Date(1, January, 2028))};
    DayCounter act365 = Actual365Fixed();
    const DiscountFactor d1 = 1.0 / (1.0 + r), d2 = d1 * d1;
    const Time expectedSimpleDuration = (1.0 * 10.0 * d1 + 2.0 * 110.0 * d2) / (10.0 * d1 + 110.0 * d2);
    Time simpleDuration =
        CashFlows::duration(twoFlows, InterestRate(r, act365, Simple, NoFrequency), Duration::Simple, false, issue);
    ASSERT_CLOSE("simple duration under Simple", issue, simpleDuration, expectedSimpleDuration, 1.0e-12);
}

// CashFlows::yield takes any solver, which is the way to the private function
// it solves for.  This one checks the slope that function reports instead.
struct SlopeCheckingSolver
{
    template <class F>
    Real solve(const F& f, Real, Real guess, Real) const
    {
        const Real h = 1.0e-5;
        const Real expected = (f(guess + h) - f(guess - h)) / (2.0 * h);
        const Real calculated = f.derivative(guess);
        BOOST_CHECK_MESSAGE(std::fabs(calculated - expected) <= 1.0e-4 * std::fabs(expected),
                            "slope at " << guess << ": " << calculated << " against a finite difference of "
                                        << expected);
        return guess;
    }
};

BOOST_AUTO_TEST_CASE(testYieldSolverSlope)
{
    BOOST_TEST_MESSAGE("Testing the slope of the function solved for a yield...");

    Date settlement(15, June, 2026);
    Settings::instance().evaluationDate() = settlement;

    DayCounter dayCounter = Thirty360(Thirty360::BondBasis);
    Schedule schedule(settlement, Date(15, June, 2036), Period(Semiannual), NullCalendar(), Unadjusted, Unadjusted,
                      DateGeneration::Backward, false);
    FixedRateBond bond(0, 100.0, schedule, std::vector<Rate>(1, 0.05), dayCounter);
    const Leg& leg = bond.cashflows();

    for (Compounding rule : {Simple, Compounded, Continuous, SimpleThenCompounded, CompoundedThenSimple})
    {
        for (Real price : {90.0, 100.0, 120.0})
        {
            for (Rate guess : {0.02, 0.04, 0.08})
                CashFlows::yield(SlopeCheckingSolver(), leg, price, dayCounter, rule, Semiannual, false, settlement,
                                 settlement, 1.0e-10, guess);

            // With the right slope Newton's steps converge in a few evaluations.
            BOOST_CHECK_NO_THROW(CashFlows::yield(leg, price, dayCounter, rule, Semiannual, false, settlement,
                                                  settlement, 1.0e-10, 10, 0.04));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
