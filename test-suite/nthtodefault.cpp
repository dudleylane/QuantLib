/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Roland Lichters

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
#include <ql/currencies/europe.hpp>
#include <ql/experimental/credit/constantlosslatentmodel.hpp>
#include <ql/experimental/credit/integralntdengine.hpp>
#include <ql/experimental/credit/nthtodefault.hpp>
#include <ql/experimental/credit/pool.hpp>
#include <ql/experimental/credit/randomdefaultlatentmodel.hpp>
#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/pricingengines/credit/integralcdsengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <iostream>
#include <string>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(NthToDefaultTests)

#ifndef QL_PATCH_SOLARIS

struct hwDatum
{
    Size rank;
    Real spread[3];
};

/* Spread (bp p.a.) to buy protection for the nth to default from
   a basket of 10 names. All pairs have same correlation, 0 in
   column 2, 0.3 in column 3, 0.6 in column 4. Default intensity
   for all names is constant at 0.01, maturity 5 years, equal
   notional amounts.
*/
hwDatum hwData[] = {{1, {603, 440, 293}}, {2, {98, 139, 137}}, {3, {12, 53, 79}}, {4, {1, 21, 49}}, {5, {0, 8, 31}},
                    {6, {0, 3, 19}},      {7, {0, 1, 12}},     {8, {0, 0, 7}},    {9, {0, 0, 3}},   {10, {0, 0, 1}}};


Real hwCorrelation[] = {0.0, 0.3, 0.6};


struct hwDatumDist
{
    Size rank;
    Real spread[4];
};

// HW Table 3, Nth to Default Basket
// corr = 0.3
// NM/NZ
// rank inf/inf 5/inf inf/5 5/5
hwDatumDist hwDataDist[] = {{1, {440, 419, 474, 455}}, {2, {139, 127, 127, 116}}, {3, {53, 51, 44, 44}},
                            {4, {21, 24, 18, 22}},     {5, {8, 13, 7, 13}},       {6, {3, 8, 3, 8}},
                            {7, {1, 5, 1, 5}},         {8, {0, 3, 0, 4}},         {9, {0, 2, 0, 2}},
                            {10, {0, 1, 0, 1}}};


BOOST_AUTO_TEST_CASE(testGauss, *precondition(if_speed(Slow)))
{
    BOOST_TEST_MESSAGE("Testing nth-to-default against Hull-White values "
                       "with Gaussian copula...");

    /*************************
     * Tolerances
     */
    Real relTolerance = 0.015; // relative difference
    Real absTolerance = 1;     // absolute difference in bp

    Period timeUnit = 1 * Weeks; // required to reach accuracy

    Size names = 10;
    QL_REQUIRE(std::size(hwData) == names, "hwData length does not match");

    Real rate = 0.05;
    DayCounter dc = Actual365Fixed();
    Compounding cmp = Continuous; // Simple;

    Real recovery = 0.4;
    std::vector<Real> lambda(names, 0.01);

    Real namesNotional = 100.0;

    Schedule schedule = MakeSchedule()
                            .from(Date(1, September, 2006))
                            .to(Date(1, September, 2011))
                            .withTenor(3 * Months)
                            .withCalendar(TARGET());

    Date asofDate(31, August, 2006);

    Settings::instance().evaluationDate() = asofDate;

    std::vector<Date> gridDates = {asofDate, TARGET().advance(asofDate, Period(1, Years)),
                                   TARGET().advance(asofDate, Period(5, Years)),
                                   TARGET().advance(asofDate, Period(7, Years))};

    ext::shared_ptr<YieldTermStructure> yieldPtr(new FlatForward(asofDate, rate, dc, cmp));
    Handle<YieldTermStructure> yieldHandle(yieldPtr);

    std::vector<Handle<DefaultProbabilityTermStructure>> probabilities;
    Period maxTerm(10, Years);
    for (Real i : lambda)
    {
        Handle<Quote> h(ext::shared_ptr<Quote>(new SimpleQuote(i)));
        ext::shared_ptr<DefaultProbabilityTermStructure> ptr(new FlatHazardRate(asofDate, h, dc));
        probabilities.emplace_back(ptr);
    }

    ext::shared_ptr<SimpleQuote> simpleQuote(new SimpleQuote(0.0));
    Handle<Quote> correlationHandle(simpleQuote);

    ext::shared_ptr<DefaultLossModel> copula(new ConstantLossModel<GaussianCopulaPolicy>(
        correlationHandle, std::vector<Real>(names, recovery), LatentModelIntegrationType::GaussianQuadrature, names,
        GaussianCopulaPolicy::initTraits()));

    /* If you like the action you can price with the simulation engine below
    instead below. But you need at least 1e6 simulations to pass the pricing
    error tests
    */
    // ext::shared_ptr<GaussianDefProbLM> gLM(
    //     ext::make_shared<GaussianDefProbLM>(correlationHandle, names,
    //     LatentModelIntegrationType::GaussianQuadrature,
    //     // g++ requires this when using make_shared
    //     GaussianCopulaPolicy::initTraits()));
    // Size numSimulations = 1000000;
    //// Size numCoresUsed = 4; use your are in the multithread branch
    //// Sobol, many cores
    // ext::shared_ptr<RandomDefaultLM<GaussianCopulaPolicy> > copula(
    //     new RandomDefaultLM<GaussianCopulaPolicy>(gLM,
    //         std::vector<Real>(names, recovery), numSimulations, 1.e-6,
    //         2863311530));

    // Set up pool and basket
    std::vector<std::string> namesIds;
    namesIds.reserve(names);
    for (Size i = 0; i < names; i++)
        namesIds.push_back(std::string("Name") + std::to_string(i));

    std::vector<Issuer> issuers;
    for (Size i = 0; i < names; i++)
    {
        std::vector<QuantLib::Issuer::key_curve_pair> curves(
            1, std::make_pair(
                   NorthAmericaCorpDefaultKey(EURCurrency(), QuantLib::SeniorSec, Period(), 1. // amount threshold
                                              ),
                   probabilities[i]));
        issuers.emplace_back(curves);
    }

    ext::shared_ptr<Pool> thePool = ext::make_shared<Pool>();
    for (Size i = 0; i < names; i++)
        thePool->add(namesIds[i], issuers[i],
                     NorthAmericaCorpDefaultKey(EURCurrency(), QuantLib::SeniorSec, Period(), 1.));

    std::vector<DefaultProbKey> defaultKeys(probabilities.size(),
                                            NorthAmericaCorpDefaultKey(EURCurrency(), SeniorSec, Period(), 1.));

    ext::shared_ptr<Basket> basket(
        new Basket(asofDate, namesIds, std::vector<Real>(names, namesNotional / names), thePool, 0., 1.));

    ext::shared_ptr<PricingEngine> engine(new IntegralNtdEngine(timeUnit, yieldHandle));

    std::vector<NthToDefault> ntd;
    for (Size i = 1; i <= probabilities.size(); i++)
    {
        ntd.emplace_back(basket, i, Protection::Seller, schedule, 0.0, 0.02, Actual360(), namesNotional * names, true);
        ntd.back().setPricingEngine(engine);
    }

    static_assert(std::size(hwCorrelation) == 3, "correlation length does not match");

    Real diff, maxDiff = 0;

    basket->setLossModel(copula);

    for (Size j = 0; j < std::size(hwCorrelation); j++)
    {
        simpleQuote->setValue(hwCorrelation[j]);
        for (Size i = 0; i < ntd.size(); i++)
        {
            QL_REQUIRE(ntd[i].rank() == hwData[i].rank, "rank does not match");
            diff = 1e4 * ntd[i].fairPremium() - hwData[i].spread[j];
            maxDiff = std::max(maxDiff, fabs(diff));
            BOOST_CHECK_MESSAGE(fabs(diff / hwData[i].spread[j]) < relTolerance || fabs(diff) < absTolerance,
                                "tolerance " << relTolerance << "|" << absTolerance << " exceeded");
        }
    }
}
BOOST_AUTO_TEST_CASE(testStudent, *precondition(if_speed(Slow)))
{

    BOOST_TEST_MESSAGE("Testing nth-to-default against Hull-White values "
                       "with Student copula...");

    /*************************
     * Tolerances
     */
    Real relTolerance = 0.017; // relative difference
    Real absTolerance = 1;     // absolute difference in bp

    Period timeUnit = 1 * Weeks; // required to reach accuracy

    Size names = 10;
    QL_REQUIRE(std::size(hwDataDist) == names, "hwDataDist length does not match");

    Real rate = 0.05;
    DayCounter dc = Actual365Fixed();
    Compounding cmp = Continuous; // Simple;


    Real recovery = 0.4;
    std::vector<Real> lambda(names, 0.01);

    Real namesNotional = 100.0;

    Schedule schedule = MakeSchedule()
                            .from(Date(1, September, 2006))
                            .to(Date(1, September, 2011))
                            .withTenor(3 * Months)
                            .withCalendar(TARGET());

    Date asofDate(31, August, 2006);

    Settings::instance().evaluationDate() = asofDate;

    std::vector<Date> gridDates{asofDate, TARGET().advance(asofDate, Period(1, Years)),
                                TARGET().advance(asofDate, Period(5, Years)),
                                TARGET().advance(asofDate, Period(7, Years))};

    ext::shared_ptr<YieldTermStructure> yieldPtr(new FlatForward(asofDate, rate, dc, cmp));
    Handle<YieldTermStructure> yieldHandle(yieldPtr);

    std::vector<Handle<DefaultProbabilityTermStructure>> probabilities;
    Period maxTerm(10, Years);
    for (Real i : lambda)
    {
        Handle<Quote> h(ext::shared_ptr<Quote>(new SimpleQuote(i)));
        ext::shared_ptr<DefaultProbabilityTermStructure> ptr(new FlatHazardRate(asofDate, h, dc));
        probabilities.emplace_back(ptr);
    }

    ext::shared_ptr<SimpleQuote> simpleQuote(new SimpleQuote(0.0));
    Handle<Quote> correlationHandle(simpleQuote);

    TCopulaPolicy::initTraits iniT;
    iniT.tOrders = std::vector<QuantLib::Integer>(2, 5);
    ext::shared_ptr<DefaultLossModel> copula(
        new ConstantLossModel<TCopulaPolicy>(correlationHandle, std::vector<Real>(names, recovery),
                                             LatentModelIntegrationType::GaussianQuadrature, names, iniT));

    // Set up pool and basket
    std::vector<std::string> namesIds;
    namesIds.reserve(names);
    for (Size i = 0; i < names; i++)
        namesIds.push_back(std::string("Name") + std::to_string(i));

    std::vector<Issuer> issuers;
    for (Size i = 0; i < names; i++)
    {
        std::vector<QuantLib::Issuer::key_curve_pair> curves(
            1, std::make_pair(
                   NorthAmericaCorpDefaultKey(EURCurrency(), QuantLib::SeniorSec, Period(), 1. // amount threshold
                                              ),
                   probabilities[i]));
        issuers.emplace_back(curves);
    }

    ext::shared_ptr<Pool> thePool = ext::make_shared<Pool>();
    for (Size i = 0; i < names; i++)
        thePool->add(namesIds[i], issuers[i],
                     NorthAmericaCorpDefaultKey(EURCurrency(), QuantLib::SeniorSec, Period(), 1.));

    std::vector<DefaultProbKey> defaultKeys(probabilities.size(),
                                            NorthAmericaCorpDefaultKey(EURCurrency(), SeniorSec, Period(), 1.));

    ext::shared_ptr<Basket> basket(
        new Basket(asofDate, namesIds, std::vector<Real>(names, namesNotional / names), thePool, 0., 1.));

    ext::shared_ptr<PricingEngine> engine(new IntegralNtdEngine(timeUnit, yieldHandle));

    std::vector<NthToDefault> ntd;
    for (Size i = 1; i <= probabilities.size(); i++)
    {
        ntd.emplace_back(basket, i, Protection::Seller, schedule, 0.0, 0.02, Actual360(), namesNotional * names, true);
        ntd.back().setPricingEngine(engine);
    }

    static_assert(std::size(hwCorrelation) == 3, "correlation length does not match");

    Real maxDiff = 0;

    basket->setLossModel(copula);

    // This is the necessary code, but a proper hwData for the t copula is needed.
    // Real diff;
    // for (Size j = 0; j < std::size(hwCorrelation); j++) {
    //     simpleQuote->setValue (hwCorrelation[j]);
    //     for (Size i = 0; i < ntd.size(); i++) {
    //         QL_REQUIRE (ntd[i].rank() == hwData[i].rank, "rank does not match");
    //         static_assert(std::size(hwCorrelation) == std::size(hwData[i].spread),
    //                     "vector length does not match");
    //         diff = 1e4 * ntd[i].fairPremium() - hwData[i].spread[j];
    //         maxDiff = std::max(maxDiff, fabs (diff));
    //         BOOST_CHECK_MESSAGE (fabs(diff/hwData[i].spread[j]) < relTolerance
    //                              || fabs(diff) < absTolerance,
    //                              "tolerance2 " << relTolerance << "|"
    //                              << absTolerance << " exceeded";
    //     }
    // }

    // instead of this BEGIN
    simpleQuote->setValue(0.3);

    for (Size i = 0; i < ntd.size(); i++)
    {
        QL_REQUIRE(ntd[i].rank() == hwDataDist[i].rank, "rank does not match");

        Real diff = 1e4 * ntd[i].fairPremium() - hwDataDist[i].spread[3];
        maxDiff = std::max(maxDiff, fabs(diff));
        BOOST_CHECK_MESSAGE(fabs(diff / hwDataDist[i].spread[3]) < relTolerance || fabs(diff) < absTolerance,
                            "tolerance " << relTolerance << "|" << absTolerance << " exceeded" << i << "|" << abs(diff)
                                         << "|" << hwDataDist[i].spread[3]);
    }
    // END
}

// Eight names of 100 from 31 August 2006, with flat hazard rates of 1 % + 0.5 % i, recoveries of 40 % and a
// correlation of 0.3. The fourth failed to pay on 15 September 2006, or isn't in the basket.
enum class FourthName
{
    alive,
    defaulted,
    gone
};

// The basket's inception, which its default curves start from.
Date inception()
{
    return {31, August, 2006};
}

ext::shared_ptr<Basket> eightNames(FourthName fourthName)
{
    const Size fourth = 3;
    const NorthAmericaCorpDefaultKey curveKey(EURCurrency(), SeniorSec, Period(0, Weeks), 10.);
    const NorthAmericaCorpDefaultKey poolKey(EURCurrency(), SeniorSec, Period(), 1.);
    const auto pool = ext::make_shared<Pool>();
    std::vector<std::string> names;
    for (Size i = 0; i < 8; ++i)
    {
        if (i == fourth && fourthName == FourthName::gone)
            continue;
        names.push_back("issuer-" + std::to_string(i));
        std::vector<std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities;
        probabilities.emplace_back(
            curveKey, Handle<DefaultProbabilityTermStructure>(ext::make_shared<FlatHazardRate>(
                          inception(), makeQuoteHandle(0.01 + 0.005 * i), ActualActual(ActualActual::ISDA))));
        DefaultEventSet events;
        if (i == fourth && fourthName == FourthName::defaulted)
            events.insert(ext::make_shared<FailureToPayEvent>(Date(15, September, 2006), EURCurrency(), SeniorSec, 10.0,
                                                              Date(20, September, 2006), 0.4));
        pool->add(names.back(), Issuer(probabilities, events), poolKey);
    }
    const auto basket =
        ext::make_shared<Basket>(inception(), names, std::vector<Real>(names.size(), 100.0), pool, 0.0, 1.0);
    basket->setLossModel(ext::make_shared<ConstantLossModel<GaussianCopulaPolicy>>(
        makeQuoteHandle(0.3), std::vector<Real>(names.size(), 0.4), LatentModelIntegrationType::GaussianQuadrature,
        names.size(), GaussianCopulaPolicy::initTraits()));
    return basket;
}

// Protection sold on the nth default, for 2 % a year paid quarterly from 1 September 2006 to 1 September 2011, and
// valued on a flat 5 % curve.
ext::shared_ptr<NthToDefault> nthToDefault(const ext::shared_ptr<Basket>& basket,
                                           Size rank,
                                           Rate upfront,
                                           Real nominal,
                                           const Handle<YieldTermStructure>& discountCurve)
{
    const Schedule schedule = MakeSchedule()
                                  .from(Date(1, September, 2006))
                                  .to(Date(1, September, 2011))
                                  .withFrequency(Quarterly)
                                  .withCalendar(TARGET())
                                  .withConvention(Following)
                                  .withRule(DateGeneration::Backward);
    auto contract = ext::make_shared<NthToDefault>(basket, rank, Protection::Seller, schedule, upfront, 0.02,
                                                   Actual360(), nominal, true);
    contract->setPricingEngine(ext::make_shared<IntegralNtdEngine>(3 * Months, discountCurve));
    return contract;
}

Handle<YieldTermStructure> flatCurveFrom(const Date& referenceDate)
{
    return Handle<YieldTermStructure>(ext::make_shared<FlatForward>(referenceDate, 0.05, Actual360()));
}

BOOST_AUTO_TEST_CASE(testAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on a basket with a default...");

    // Eight names, the fourth of which has failed to pay, against the seven left as a pool of
    // their own. Before #161 the basket asked its model for the nth default without taking off
    // the one so far, and the model still counted the defaulted name as one that could default:
    // a second to default on the eight priced at 0.0353, where as a first to default on the
    // seven it's 0.0836, and a first to default, already triggered, at 0.0908. And before #206
    // nothing priced inside the first coupon on a discount curve as of today.
    const Date today(1, November, 2006);
    const Date horizon(1, September, 2011);
    Settings::instance().evaluationDate() = today;

    const auto all = eightNames(FourthName::defaulted);
    const auto left = eightNames(FourthName::gone);
    BOOST_REQUIRE_EQUAL(all->remainingSize(), left->remainingSize());

    BOOST_CHECK_EQUAL(all->probAtLeastNEvents(1, horizon), 1.0);
    for (Size n = 2; n <= 4; ++n)
    {
        BOOST_TEST_CONTEXT("at least " << n << " defaults")
        {
            BOOST_CHECK_CLOSE_FRACTION(all->probAtLeastNEvents(n, horizon), left->probAtLeastNEvents(n - 1, horizon),
                                       1.0e-12);
        }
    }

    for (const Date& curveDate : {inception(), today})
    {
        BOOST_TEST_CONTEXT("curve from " << curveDate)
        {
            const auto curve = flatCurveFrom(curveDate);
            for (Size rank = 2; rank <= 4; ++rank)
            {
                BOOST_TEST_CONTEXT("rank " << rank)
                {
                    BOOST_CHECK_CLOSE_FRACTION(nthToDefault(all, rank, 0.0, 100.0, curve)->fairPremium(),
                                               nthToDefault(left, rank - 1, 0.0, 100.0, curve)->fairPremium(), 1.0e-12);
                }
            }

            // The default so far has triggered a first to default, which has nothing left to pay.
            const auto triggered = nthToDefault(all, 1, 0.0, 100.0, curve);
            BOOST_CHECK_EQUAL(triggered->premiumLegNPV(), 0.0);
            BOOST_CHECK_EQUAL(triggered->protectionLegNPV(), 0.0);
            BOOST_CHECK_THROW(triggered->fairPremium(), Error);
        }
    }
}

BOOST_AUTO_TEST_CASE(testUpfront)
{
    BOOST_TEST_MESSAGE("Testing an nth to default's upfront...");

    // The upfront is taken as paid on the first coupon's accrual start, and counts until that
    // coupon is paid, unless it falls before the discount curve's reference date. Before #206 the
    // engine took it on the basket's remaining notional, 800 here, where the premium and the
    // protection are on the contract's: an upfront of 1 % came to 7.9989 on a contract of 100,
    // and on one of 1,000 alike. And inside the first coupon, on a discount curve as of today,
    // it discounted the upfront from before the curve's reference date, and threw.
    const Date accrualStart(1, September, 2006);
    const Date insideTheFirstCoupon(1, November, 2006);
    const Date afterTheFirstPayment(4, December, 2006);
    const Rate upfront = 0.01;
    const auto basket = eightNames(FourthName::alive);

    struct Valuation
    {
        Date today, curveDate;
        Real nominal;
        bool upfrontCounts;
    };
    // The upfront doesn't depend on the rank, so these take a first to default. Inside the first
    // coupon, on a curve from inception, it still counts though it has been paid: #206 changed
    // only what threw (#227).
    const Valuation valuations[] = {
        {inception(), inception(), 100.0, true},           {inception(), inception(), 1000.0, true},
        {accrualStart, accrualStart, 100.0, true},         {accrualStart, accrualStart, 1000.0, true},
        {insideTheFirstCoupon, inception(), 100.0, true},  {insideTheFirstCoupon, insideTheFirstCoupon, 100.0, false},
        {afterTheFirstPayment, inception(), 100.0, false}, {afterTheFirstPayment, afterTheFirstPayment, 100.0, false},
    };
    for (const auto& valuation : valuations)
    {
        BOOST_TEST_CONTEXT("today " << valuation.today << ", curve from " << valuation.curveDate << ", nominal "
                                    << valuation.nominal)
        {
            Settings::instance().evaluationDate() = valuation.today;
            const auto curve = flatCurveFrom(valuation.curveDate);
            const auto with = nthToDefault(basket, 1, upfront, valuation.nominal, curve);
            const auto without = nthToDefault(basket, 1, 0.0, valuation.nominal, curve);
            if (valuation.upfrontCounts)
                BOOST_CHECK_CLOSE_FRACTION(with->NPV() - without->NPV(),
                                           valuation.nominal * upfront * curve->discount(accrualStart), 1.0e-12);
            else
                BOOST_CHECK_EQUAL(with->NPV(), without->NPV());
        }
    }

    // Inside the first coupon, the premium on a discount curve as of today is the one on a curve
    // from inception, carried forward to today.
    Settings::instance().evaluationDate() = insideTheFirstCoupon;
    const auto fromInception = flatCurveFrom(inception());
    BOOST_CHECK_CLOSE_FRACTION(
        nthToDefault(basket, 1, upfront, 100.0, flatCurveFrom(insideTheFirstCoupon))->premiumLegNPV(),
        nthToDefault(basket, 1, 0.0, 100.0, fromInception)->premiumLegNPV() /
            fromInception->discount(insideTheFirstCoupon),
        1.0e-12);
}

#endif

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
