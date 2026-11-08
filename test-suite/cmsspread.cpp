/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2018 Peter Caspers

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
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>
#include <ql/currencies/america.hpp>
#include <ql/experimental/coupons/cmsspreadcoupon.hpp>
#include <ql/experimental/coupons/lognormalcmsspreadpricer.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/math/randomnumbers/sobolrsg.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace boost::accumulators;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CmsSpreadTests)

struct TestData
{
    TestData()
    {
        refDate = Date(23, February, 2018);
        Settings::instance().evaluationDate() = refDate;

        yts2 = Handle<YieldTermStructure>(ext::make_shared<FlatForward>(refDate, 0.02, Actual365Fixed()));

        swLn = Handle<SwaptionVolatilityStructure>(ext::make_shared<ConstantSwaptionVolatility>(
            refDate, TARGET(), Following, 0.20, Actual365Fixed(), ShiftedLognormal, 0.0));
        swSln = Handle<SwaptionVolatilityStructure>(ext::make_shared<ConstantSwaptionVolatility>(
            refDate, TARGET(), Following, 0.10, Actual365Fixed(), ShiftedLognormal, 0.01));
        swN = Handle<SwaptionVolatilityStructure>(ext::make_shared<ConstantSwaptionVolatility>(
            refDate, TARGET(), Following, 0.0075, Actual365Fixed(), Normal, 0.01));

        reversion = Handle<Quote>(ext::make_shared<SimpleQuote>(0.01));
        cmsPricerLn = ext::make_shared<LinearTsrPricer>(swLn, reversion, yts2);
        cmsPricerSln = ext::make_shared<LinearTsrPricer>(swSln, reversion, yts2);
        cmsPricerN = ext::make_shared<LinearTsrPricer>(swN, reversion, yts2);

        correlation = Handle<Quote>(ext::make_shared<SimpleQuote>(0.6));
        cmsspPricerLn = ext::make_shared<LognormalCmsSpreadPricer>(cmsPricerLn, correlation, yts2, 32);
        cmsspPricerSln = ext::make_shared<LognormalCmsSpreadPricer>(cmsPricerSln, correlation, yts2, 32);
        cmsspPricerN = ext::make_shared<LognormalCmsSpreadPricer>(cmsPricerN, correlation, yts2, 32);
    }

    Date refDate;
    Handle<YieldTermStructure> yts2;
    Handle<SwaptionVolatilityStructure> swLn, swSln, swN;
    Handle<Quote> reversion, correlation;
    ext::shared_ptr<CmsCouponPricer> cmsPricerLn, cmsPricerSln, cmsPricerN;
    ext::shared_ptr<CmsSpreadCouponPricer> cmsspPricerLn, cmsspPricerSln, cmsspPricerN;
};


BOOST_AUTO_TEST_CASE(testFixings)
{
    BOOST_TEST_MESSAGE("Testing fixings of cms spread indices...");

    TestData d;

    ext::shared_ptr<SwapIndex> cms10y = ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, d.yts2, d.yts2);
    ext::shared_ptr<SwapIndex> cms2y = ext::make_shared<EuriborSwapIsdaFixA>(2 * Years, d.yts2, d.yts2);
    ext::shared_ptr<SwapSpreadIndex> cms10y2y = ext::make_shared<SwapSpreadIndex>("cms10y2y", cms10y, cms2y);

    Settings::instance().enforcesTodaysHistoricFixings() = false;

    BOOST_CHECK_THROW(cms10y2y->fixing(d.refDate - 1), QuantLib::Error);
    BOOST_REQUIRE_NO_THROW(cms10y2y->fixing(d.refDate));
    BOOST_CHECK_EQUAL(cms10y2y->fixing(d.refDate), cms10y->fixing(d.refDate) - cms2y->fixing(d.refDate));
    cms10y->addFixing(d.refDate, 0.05);
    BOOST_CHECK_EQUAL(cms10y2y->fixing(d.refDate), cms10y->fixing(d.refDate) - cms2y->fixing(d.refDate));
    cms2y->addFixing(d.refDate, 0.04);
    BOOST_CHECK_EQUAL(cms10y2y->fixing(d.refDate), cms10y->fixing(d.refDate) - cms2y->fixing(d.refDate));
    Date futureFixingDate = TARGET().adjust(d.refDate + 1 * Years);
    BOOST_CHECK_EQUAL(cms10y2y->fixing(futureFixingDate),
                      cms10y->fixing(futureFixingDate) - cms2y->fixing(futureFixingDate));
    IndexManager::instance().clearHistories();

    Settings::instance().enforcesTodaysHistoricFixings() = true;
    BOOST_CHECK_THROW(cms10y2y->fixing(d.refDate), QuantLib::Error);
    cms10y->addFixing(d.refDate, 0.05);
    BOOST_CHECK_THROW(cms10y2y->fixing(d.refDate), QuantLib::Error);
    cms2y->addFixing(d.refDate, 0.04);
    BOOST_CHECK_EQUAL(cms10y2y->fixing(d.refDate), cms10y->fixing(d.refDate) - cms2y->fixing(d.refDate));
}


Real mcReferenceValue(const ext::shared_ptr<CmsCoupon>& cpn1,
                      const ext::shared_ptr<CmsCoupon>& cpn2,
                      const Real cap,
                      const Real floor,
                      const Handle<SwaptionVolatilityStructure>& vol,
                      const Real correlation)
{
    Size samples = 1000000;
    accumulator_set<Real, stats<tag::mean>> acc;
    Matrix Cov(2, 2);
    Cov(0, 0) = vol->blackVariance(cpn1->fixingDate(), cpn1->index()->tenor(), cpn1->indexFixing());
    Cov(1, 1) = vol->blackVariance(cpn2->fixingDate(), cpn2->index()->tenor(), cpn2->indexFixing());
    Cov(0, 1) = Cov(1, 0) = std::sqrt(Cov(0, 0) * Cov(1, 1)) * correlation;
    Matrix C = pseudoSqrt(Cov);

    Array atmRate(2), adjRate(2), avg(2), volShift(2);
    atmRate[0] = cpn1->indexFixing();
    atmRate[1] = cpn2->indexFixing();
    adjRate[0] = cpn1->adjustedFixing();
    adjRate[1] = cpn2->adjustedFixing();
    if (vol->volatilityType() == ShiftedLognormal)
    {
        volShift[0] = vol->shift(cpn1->fixingDate(), cpn1->index()->tenor());
        volShift[1] = vol->shift(cpn2->fixingDate(), cpn2->index()->tenor());
        avg[0] = std::log((adjRate[0] + volShift[0]) / (atmRate[0] + volShift[0])) - 0.5 * Cov(0, 0);
        avg[1] = std::log((adjRate[1] + volShift[1]) / (atmRate[1] + volShift[1])) - 0.5 * Cov(1, 1);
    }
    else
    {
        avg[0] = adjRate[0];
        avg[1] = adjRate[1];
    }

    InverseCumulativeNormal icn;
    SobolRsg sb_(2, 42);
    Array w(2), z(2);
    for (Size i = 0; i < samples; ++i)
    {
        std::vector<Real> seq = sb_.nextSequence().value;
        std::transform(seq.begin(), seq.end(), w.begin(), icn);
        z = C * w + avg;
        for (Size i = 0; i < 2; ++i)
        {
            if (vol->volatilityType() == ShiftedLognormal)
            {
                z[i] = (atmRate[i] + volShift[i]) * std::exp(z[i]) - volShift[i];
            }
        }
        acc(std::min(std::max(z[0] - z[1], floor), cap));
    }
    return mean(acc);
}


BOOST_AUTO_TEST_CASE(testCouponPricing)
{
    BOOST_TEST_MESSAGE("Testing pricing of cms spread coupons...");

    TestData d;
    Real tol = 1E-6; // abs tolerance coupon rate

    ext::shared_ptr<SwapIndex> cms10y = ext::make_shared<EuriborSwapIsdaFixA>(10 * Years, d.yts2, d.yts2);
    ext::shared_ptr<SwapIndex> cms2y = ext::make_shared<EuriborSwapIsdaFixA>(2 * Years, d.yts2, d.yts2);
    ext::shared_ptr<SwapSpreadIndex> cms10y2y = ext::make_shared<SwapSpreadIndex>("cms10y2y", cms10y, cms2y);

    Date valueDate = cms10y2y->valueDate(d.refDate);
    Date payDate = valueDate + 1 * Years;
    ext::shared_ptr<CmsCoupon> cpn1a =
        ext::make_shared<CmsCoupon>(payDate, 10000.0, valueDate, payDate, cms10y->fixingDays(), cms10y, 1.0, 0.0,
                                    Date(), Date(), Actual360(), false);
    ext::shared_ptr<CmsCoupon> cpn1b = ext::make_shared<CmsCoupon>(
        payDate, 10000.0, valueDate, payDate, cms2y->fixingDays(), cms2y, 1.0, 0.0, Date(), Date(), Actual360(), false);
    ext::shared_ptr<CmsSpreadCoupon> cpn1 =
        ext::make_shared<CmsSpreadCoupon>(payDate, 10000.0, valueDate, payDate, cms10y2y->fixingDays(), cms10y2y, 1.0,
                                          0.0, Date(), Date(), Actual360(), false);
    BOOST_CHECK(cpn1->fixingDate() == d.refDate);
    cpn1a->setPricer(d.cmsPricerLn);
    cpn1b->setPricer(d.cmsPricerLn);
    cpn1->setPricer(d.cmsspPricerLn);

#ifndef __FAST_MATH__
    constexpr double eqTol = 100 * QL_EPSILON;
#else
    constexpr double eqTol = 1e-13;
#endif
    QL_CHECK_CLOSE(cpn1->rate(), cpn1a->rate() - cpn1b->rate(), eqTol);
    cms10y->addFixing(d.refDate, 0.05);
    QL_CHECK_CLOSE(cpn1->rate(), cpn1a->rate() - cpn1b->rate(), eqTol);
    cms2y->addFixing(d.refDate, 0.03);
    QL_CHECK_CLOSE(cpn1->rate(), cpn1a->rate() - cpn1b->rate(), eqTol);
    IndexManager::instance().clearHistories();

    ext::shared_ptr<CmsCoupon> cpn2a =
        ext::make_shared<CmsCoupon>(Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                    Date(23, February, 2029), 2, cms10y, 1.0, 0.0, Date(), Date(), Actual360(), false);
    ext::shared_ptr<CmsCoupon> cpn2b =
        ext::make_shared<CmsCoupon>(Date(23, February, 2029), 10000.0, Date(23, February, 2028),
                                    Date(23, February, 2029), 2, cms2y, 1.0, 0.0, Date(), Date(), Actual360(), false);

    ext::shared_ptr<CappedFlooredCmsSpreadCoupon> plainCpn = ext::make_shared<CappedFlooredCmsSpreadCoupon>(
        Date(23, February, 2029), 10000.0, Date(23, February, 2028), Date(23, February, 2029), 2, cms10y2y, 1.0, 0.0,
        Null<Rate>(), Null<Rate>(), Date(), Date(), Actual360(), false);
    ext::shared_ptr<CappedFlooredCmsSpreadCoupon> cappedCpn = ext::make_shared<CappedFlooredCmsSpreadCoupon>(
        Date(23, February, 2029), 10000.0, Date(23, February, 2028), Date(23, February, 2029), 2, cms10y2y, 1.0, 0.0,
        0.03, Null<Rate>(), Date(), Date(), Actual360(), false);
    ext::shared_ptr<CappedFlooredCmsSpreadCoupon> flooredCpn = ext::make_shared<CappedFlooredCmsSpreadCoupon>(
        Date(23, February, 2029), 10000.0, Date(23, February, 2028), Date(23, February, 2029), 2, cms10y2y, 1.0, 0.0,
        Null<Rate>(), 0.01, Date(), Date(), Actual360(), false);
    ext::shared_ptr<CappedFlooredCmsSpreadCoupon> collaredCpn = ext::make_shared<CappedFlooredCmsSpreadCoupon>(
        Date(23, February, 2029), 10000.0, Date(23, February, 2028), Date(23, February, 2029), 2, cms10y2y, 1.0, 0.0,
        0.03, 0.01, Date(), Date(), Actual360(), false);

    cpn2a->setPricer(d.cmsPricerLn);
    cpn2b->setPricer(d.cmsPricerLn);
    plainCpn->setPricer(d.cmsspPricerLn);
    cappedCpn->setPricer(d.cmsspPricerLn);
    flooredCpn->setPricer(d.cmsspPricerLn);
    collaredCpn->setPricer(d.cmsspPricerLn);

    QL_CHECK_SMALL(std::abs(plainCpn->rate() -
                            mcReferenceValue(cpn2a, cpn2b, QL_MAX_REAL, -QL_MAX_REAL, d.swLn, d.correlation->value())),
                   tol);
    QL_CHECK_SMALL(std::abs(cappedCpn->rate() -
                            mcReferenceValue(cpn2a, cpn2b, 0.03, -QL_MAX_REAL, d.swLn, d.correlation->value())),
                   tol);
    QL_CHECK_SMALL(std::abs(flooredCpn->rate() -
                            mcReferenceValue(cpn2a, cpn2b, QL_MAX_REAL, 0.01, d.swLn, d.correlation->value())),

                   tol);
    QL_CHECK_SMALL(
        std::abs(collaredCpn->rate() - mcReferenceValue(cpn2a, cpn2b, 0.03, 0.01, d.swLn, d.correlation->value())),
        tol);

    cpn2a->setPricer(d.cmsPricerSln);
    cpn2b->setPricer(d.cmsPricerSln);
    plainCpn->setPricer(d.cmsspPricerSln);
    cappedCpn->setPricer(d.cmsspPricerSln);
    flooredCpn->setPricer(d.cmsspPricerSln);
    collaredCpn->setPricer(d.cmsspPricerSln);

    QL_CHECK_SMALL(std::abs(plainCpn->rate() -
                            mcReferenceValue(cpn2a, cpn2b, QL_MAX_REAL, -QL_MAX_REAL, d.swSln, d.correlation->value())),
                   tol);
    QL_CHECK_SMALL(std::abs(cappedCpn->rate() -
                            mcReferenceValue(cpn2a, cpn2b, 0.03, -QL_MAX_REAL, d.swSln, d.correlation->value())),
                   tol);
    QL_CHECK_SMALL(std::abs(flooredCpn->rate() -
                            mcReferenceValue(cpn2a, cpn2b, QL_MAX_REAL, 0.01, d.swSln, d.correlation->value())),

                   tol);
    QL_CHECK_SMALL(
        std::abs(collaredCpn->rate() - mcReferenceValue(cpn2a, cpn2b, 0.03, 0.01, d.swSln, d.correlation->value())),
        tol);

    cpn2a->setPricer(d.cmsPricerN);
    cpn2b->setPricer(d.cmsPricerN);
    plainCpn->setPricer(d.cmsspPricerN);
    cappedCpn->setPricer(d.cmsspPricerN);
    flooredCpn->setPricer(d.cmsspPricerN);
    collaredCpn->setPricer(d.cmsspPricerN);

    QL_CHECK_SMALL(std::abs(plainCpn->rate() -
                            mcReferenceValue(cpn2a, cpn2b, QL_MAX_REAL, -QL_MAX_REAL, d.swN, d.correlation->value())),
                   tol);
    QL_CHECK_SMALL(
        std::abs(cappedCpn->rate() - mcReferenceValue(cpn2a, cpn2b, 0.03, -QL_MAX_REAL, d.swN, d.correlation->value())),
        tol);
    QL_CHECK_SMALL(
        std::abs(flooredCpn->rate() - mcReferenceValue(cpn2a, cpn2b, QL_MAX_REAL, 0.01, d.swN, d.correlation->value())),

        tol);
    QL_CHECK_SMALL(
        std::abs(collaredCpn->rate() - mcReferenceValue(cpn2a, cpn2b, 0.03, 0.01, d.swN, d.correlation->value())), tol);
}

// E[max(w (g1 X1 + g2 X2 - k), 0)] for lognormal X1 and X2 with these means and standard deviations
// of their logarithms: a Black formula in X1 for each value of X2, with the trapezoid rule over X2,
// which converges very fast on a Gaussian's whole range.
Real twoLognormalsOptionValue(
    Option::Type type, Real g1, Real g2, Real k, Real mean1, Real mean2, Real stdDev1, Real stdDev2, Real rho)
{
    const Real rest = stdDev1 * std::sqrt(1.0 - rho * rho);
    const Real dz = 0.05;
    Real sum = 0.0;
    for (Integer i = -240; i <= 240; ++i)
    {
        const Real z = i * dz;
        const Real x2 = mean2 * std::exp(stdDev2 * z - 0.5 * stdDev2 * stdDev2);
        const Real forward = mean1 * std::exp(rho * stdDev1 * z - 0.5 * rho * rho * stdDev1 * stdDev1);
        const Real strike = (k - g2 * x2) / g1;
        const Real value = strike > 0.0 ? blackFormula(type, strike, forward, rest) :
                                          std::max(Integer(type) * (forward - strike), 0.0);
        sum += g1 * value * std::exp(-0.5 * z * z) * M_1_SQRTPI * M_SQRT1_2;
    }
    return sum * dz;
}

BOOST_AUTO_TEST_CASE(testShiftedLognormalWithDisplacedStrikes)
{
    BOOST_TEST_MESSAGE("Testing shifted-lognormal CMS spread options whose strike and displaced strike differ...");

    const Date refDate(21, September, 2026);
    Settings::instance().evaluationDate() = refDate;
    const DayCounter curveDayCounter = Actual365Fixed();
    const Calendar calendar = NullCalendar();
    const Handle<YieldTermStructure> curve(ext::make_shared<FlatForward>(refDate, 0.03, curveDayCounter));
    const auto ibor = ext::make_shared<IborIndex>("I6M", Period(6, Months), 0, USDCurrency(), calendar, Unadjusted,
                                                  false, Actual360(), curve);
    const auto swapIndex = [&](Integer years)
    {
        return ext::make_shared<SwapIndex>("S", Period(years, Years), 0, USDCurrency(), calendar, Period(1, Years),
                                           Unadjusted, Thirty360(Thirty360::European), ibor);
    };
    const auto swap10y = swapIndex(10);
    const auto swap2y = swapIndex(2);
    const Date fixingDate = refDate + Period(5, Years);
    const Date paymentDate = fixingDate + Period(1, Years);
    const Real rho = 0.5;

    const std::pair<Real, Real> gearings[] = {{1.0, -1.0}, {1.0, -0.5}, {2.0, -1.0}};
    const std::pair<Real, Real> shiftPairs[] = {{0.01, 0.01}, {0.0, 0.01}, {0.01, 0.0}, {0.0, 0.0}, {0.02, 0.005}};

    for (const auto& [gearing1, gearing2] : gearings)
    {
        const auto spreadIndex = ext::make_shared<SwapSpreadIndex>("SP", swap10y, swap2y, gearing1, gearing2);

        for (const auto& [shift1, shift2] : shiftPairs)
        {
            Matrix shifts(2, 2);
            shifts[0][0] = shifts[1][0] = shift2;
            shifts[0][1] = shifts[1][1] = shift1;
            const auto volatility = ext::make_shared<SwaptionVolatilityMatrix>(
                calendar, Following, std::vector<Period>{1 * Years, 10 * Years},
                std::vector<Period>{2 * Years, 10 * Years}, Matrix(2, 2, 0.20), curveDayCounter, false,
                ShiftedLognormal, shifts);
            volatility->enableExtrapolation();
            const ext::shared_ptr<CmsCouponPricer> cmsPricer = ext::make_shared<LinearTsrPricer>(
                Handle<SwaptionVolatilityStructure>(volatility), Handle<Quote>(ext::make_shared<SimpleQuote>(0.0)));
            const auto spreadPricer = ext::make_shared<LognormalCmsSpreadPricer>(
                cmsPricer, Handle<Quote>(ext::make_shared<SimpleQuote>(rho)));

            const auto coupon =
                ext::make_shared<CmsSpreadCoupon>(paymentDate, 1e6, fixingDate, paymentDate, 0, spreadIndex, 1.0, 0.0,
                                                  Date(), Date(), Actual360(), false);
            coupon->setPricer(spreadPricer);
            const Rate swaplet = coupon->rate();

            const auto cmsCoupon = [&](const ext::shared_ptr<SwapIndex>& index)
            {
                auto cms = ext::make_shared<CmsCoupon>(paymentDate, 1e6, fixingDate, paymentDate, 0, index, 1.0, 0.0,
                                                       Date(), Date(), Actual360(), false);
                cms->setPricer(cmsPricer);
                return cms;
            };
            const Real mean1 = cmsCoupon(swap10y)->adjustedFixing() + shift1;
            const Real mean2 = cmsCoupon(swap2y)->adjustedFixing() + shift2;
            const Real stdDev = 0.20 * std::sqrt(volatility->timeFromReference(fixingDate));

            const Real shiftOfStrike = gearing1 * shift1 + gearing2 * shift2;
            for (Real strike :
                 {-0.02, -0.002, 0.0, 0.002, 0.02, -shiftOfStrike - 0.01, -shiftOfStrike - 0.002, -shiftOfStrike - 1e-6,
                  -shiftOfStrike, -shiftOfStrike + 1e-6, -shiftOfStrike + 0.002, -shiftOfStrike + 0.01})
            {
                BOOST_TEST_CONTEXT("gearings " << gearing1 << " and " << gearing2 << ", shifts " << shift1 << " and "
                                               << shift2 << ", strike " << strike)
                {
                    const Real k = strike + shiftOfStrike;
                    const Rate cap = spreadPricer->capletRate(strike);
                    const Rate floor = spreadPricer->floorletRate(strike);
                    BOOST_CHECK_SMALL(cap - twoLognormalsOptionValue(Option::Call, gearing1, gearing2, k, mean1, mean2,
                                                                     stdDev, stdDev, rho),
                                      1e-12);
                    BOOST_CHECK_SMALL(floor - twoLognormalsOptionValue(Option::Put, gearing1, gearing2, k, mean1, mean2,
                                                                       stdDev, stdDev, rho),
                                      1e-12);
                    BOOST_CHECK_SMALL(cap - floor - (swaplet - strike), 1e-12);
                }
            }

            // From 40 digits, conditioning on X1 rather than on X2 as the reference above and the pricer do.
            if (gearing1 == 1.0 && gearing2 == -1.0 && shift1 == 0.0 && shift2 == 0.01)
            {
                BOOST_CHECK_SMALL(spreadPricer->capletRate(-0.002) - 0.0080778180, 1e-9);
                BOOST_CHECK_SMALL(spreadPricer->capletRate(0.002) - 0.0058457681, 1e-9);
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
