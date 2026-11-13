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
#include <ql/experimental/credit/randomlosslatentmodel.hpp>
#include <ql/experimental/credit/spotlosslatentmodel.hpp>
#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/pricingengines/credit/integralcdsengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <bit>
#include <cmath>
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

// Eight names of 100 from 31 August 2006, with flat hazard rates, recoveries and a correlation: by
// default 1 % + 0.5 % i, 40 % and 0.3. One of them, the fourth by default, failed to pay on
// 15 September 2006, or isn't in the basket.
enum class OneName
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

std::vector<Real> risingHazards()
{
    std::vector<Real> hazards;
    for (Size i = 0; i < 8; ++i)
        hazards.push_back(0.01 + 0.005 * i);
    return hazards;
}

struct EightNames
{
    OneName oneName = OneName::alive;
    Size which = 3;
    std::vector<Real> hazards = risingHazards();
    std::vector<Real> recoveries = std::vector<Real>(8, 0.4);
    Real correlation = 0.3;
    LatentModelIntegrationType::LatentModelIntegrationType integration = LatentModelIntegrationType::GaussianQuadrature;
    Date curveDate = inception();
    // on that many paths of RandomDefaultLM over the latent model, or on ConstantLossModel if none
    Size paths = 0;
};

ext::shared_ptr<Basket> eightNames(const EightNames& spec = {})
{
    const NorthAmericaCorpDefaultKey curveKey(EURCurrency(), SeniorSec, Period(0, Weeks), 10.);
    const NorthAmericaCorpDefaultKey poolKey(EURCurrency(), SeniorSec, Period(), 1.);
    const auto pool = ext::make_shared<Pool>();
    std::vector<std::string> names;
    std::vector<Real> recoveries;
    for (Size i = 0; i < 8; ++i)
    {
        if (i == spec.which && spec.oneName == OneName::gone)
            continue;
        names.push_back("issuer-" + std::to_string(i));
        recoveries.push_back(spec.recoveries[i]);
        std::vector<std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities;
        probabilities.emplace_back(
            curveKey, Handle<DefaultProbabilityTermStructure>(ext::make_shared<FlatHazardRate>(
                          spec.curveDate, makeQuoteHandle(spec.hazards[i]), ActualActual(ActualActual::ISDA))));
        DefaultEventSet events;
        if (i == spec.which && spec.oneName == OneName::defaulted)
            events.insert(ext::make_shared<FailureToPayEvent>(Date(15, September, 2006), EURCurrency(), SeniorSec, 10.0,
                                                              Date(20, September, 2006), 0.4));
        pool->add(names.back(), Issuer(probabilities, events), poolKey);
    }
    const auto basket =
        ext::make_shared<Basket>(inception(), names, std::vector<Real>(names.size(), 100.0), pool, 0.0, 1.0);
    const auto latentModel = ext::make_shared<ConstantLossModel<GaussianCopulaPolicy>>(
        makeQuoteHandle(spec.correlation), recoveries, spec.integration, names.size(),
        GaussianCopulaPolicy::initTraits());
    if (spec.paths == 0)
        basket->setLossModel(latentModel);
    else
        basket->setLossModel(ext::make_shared<RandomDefaultLM<GaussianCopulaPolicy>>(latentModel, spec.paths));
    return basket;
}

// Protection sold on the nth default, for 2 % a year paid quarterly from 1 September 2006 to 1 September 2011, and
// valued on a flat 5 % curve, integrated in steps of three months unless given.
ext::shared_ptr<NthToDefault> nthToDefault(const ext::shared_ptr<Basket>& basket,
                                           Size rank,
                                           Rate upfront,
                                           Real nominal,
                                           const Handle<YieldTermStructure>& discountCurve,
                                           const Period& step = 3 * Months)
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
    contract->setPricingEngine(ext::make_shared<IntegralNtdEngine>(step, discountCurve));
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

    const auto all = eightNames({.oneName = OneName::defaulted});
    const auto left = eightNames({.oneName = OneName::gone});
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
    const auto basket = eightNames();

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

// Recoveries of 20 % + 5 % i, by name.
std::vector<Real> risingRecoveries()
{
    std::vector<Real> recoveries;
    for (Size i = 0; i < 8; ++i)
        recoveries.push_back(0.2 + 0.05 * i);
    return recoveries;
}

BOOST_AUTO_TEST_CASE(testNamesAlikeButForTheirRecoveries)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on names alike but for their recoveries...");

    // With the same hazard rate each name is as likely as any other to be the nth default, so the
    // protection is the one at their mean recovery, 37.5 % here, whatever the rank and the
    // correlation. Before #208 the engine paid every trigger at the first name's recovery, 20 %,
    // so the protection was 28 % high. The premium, and its accrual on default, don't depend on
    // the recoveries.
    Settings::instance().evaluationDate() = inception();
    const auto curve = flatCurveFrom(inception());
    const std::vector<Real> hazards(8, 0.02);
    for (Real correlation : {0.0, 0.3})
    {
        const auto differing =
            eightNames({.hazards = hazards, .recoveries = risingRecoveries(), .correlation = correlation});
        const auto atTheMean =
            eightNames({.hazards = hazards, .recoveries = std::vector<Real>(8, 0.375), .correlation = correlation});
        for (Size rank : {1, 2, 3, 8})
        {
            BOOST_TEST_CONTEXT("correlation " << correlation << ", rank " << rank)
            {
                const auto priced = nthToDefault(differing, rank, 0.0, 100.0, curve);
                const auto reference = nthToDefault(atTheMean, rank, 0.0, 100.0, curve);
                BOOST_CHECK_CLOSE_FRACTION(priced->protectionLegNPV(), reference->protectionLegNPV(), 1.0e-12);
                BOOST_CHECK_EQUAL(priced->premiumLegNPV(), reference->premiumLegNPV());
                BOOST_CHECK_SMALL((priced->NPV() - priced->premiumLegNPV() - priced->protectionLegNPV()) -
                                      (reference->NPV() - reference->premiumLegNPV() - reference->protectionLegNPV()),
                                  1.0e-12);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testFirstToDefaultWithoutCorrelation)
{
    BOOST_TEST_MESSAGE("Testing a first to default on uncorrelated names whose recoveries differ...");

    // Without correlation, on flat hazard rates, a name is the first to default with a probability
    // in proportion to its hazard rate, whenever the first default comes. So the protection is the
    // one on a loss of the whole notional, times each name's loss in proportion to its hazard rate:
    // 0.6 here, against the first name's 0.8 the engine paid before #208. The engine splits each
    // step's probability of the first default among the names by their probabilities at the step's
    // two ends, which is exact to second order in the step.
    Settings::instance().evaluationDate() = inception();
    const auto curve = flatCurveFrom(inception());
    const std::vector<Real> hazards = risingHazards(), recoveries = risingRecoveries();
    Real sumOfHazards = 0.0, loss = 0.0;
    for (Real h : hazards)
        sumOfHazards += h;
    for (Size i = 0; i < 8; ++i)
        loss += hazards[i] / sumOfHazards * (1.0 - recoveries[i]);
    const auto differing = eightNames({.recoveries = recoveries, .correlation = 0.0});
    const auto wholeLoss = eightNames({.recoveries = std::vector<Real>(8, 0.0), .correlation = 0.0});
    BOOST_CHECK_CLOSE_FRACTION(nthToDefault(differing, 1, 0.0, 100.0, curve)->protectionLegNPV(),
                               loss * nthToDefault(wholeLoss, 1, 0.0, 100.0, curve)->protectionLegNPV(), 2.0e-5);
}

BOOST_AUTO_TEST_CASE(testTwoNamesWithoutCorrelation)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on two uncorrelated names that can default...");

    // Two of the eight can default, at 2 % and 4 % a year, and without correlation. Each day, the
    // first of them defaults with a probability in proportion to its hazard rate, and the second
    // is the one whose default follows the other's:
    //     P(i is the first, on (t0, t1]) = h_i / H (e^{-H t0} - e^{-H t1}),
    //     P(i is the second, on (t0, t1]) = e^{-h_i t0} - e^{-h_i t1} - h_i / H (e^{-H t0} - e^{-H t1}),
    // with H the two hazard rates' sum. Before #208 the engine paid every trigger at the first
    // name's recovery, 20 %, where these two have 30 % and 50 %.
    Settings::instance().evaluationDate() = inception();
    const auto curve = flatCurveFrom(inception());
    std::vector<Real> hazards(8, 0.0);
    hazards[2] = 0.02;
    hazards[6] = 0.04;
    const std::vector<Real> recoveries = risingRecoveries();
    const auto basket = eightNames({.hazards = hazards, .recoveries = recoveries, .correlation = 0.0});

    // The engine integrates in steps of a day from 1 September 2006 to 1 September 2011.
    const DayCounter dayCounter = ActualActual(ActualActual::ISDA);
    const Real H = hazards[2] + hazards[6];
    Real first = 0.0, second = 0.0;
    for (Date d = Date(1, September, 2006) + 1; d <= Date(1, September, 2011); ++d)
    {
        const Time t0 = dayCounter.yearFraction(inception(), d - 1), t1 = dayCounter.yearFraction(inception(), d);
        const Real eitherFirst = std::exp(-H * t0) - std::exp(-H * t1);
        for (Size i : {2, 6})
        {
            const Real discountedLoss = 100.0 * (1.0 - recoveries[i]) * curve->discount(d);
            first += discountedLoss * hazards[i] / H * eitherFirst;
            second += discountedLoss *
                      (std::exp(-hazards[i] * t0) - std::exp(-hazards[i] * t1) - hazards[i] / H * eitherFirst);
        }
    }
    BOOST_CHECK_CLOSE_FRACTION(-nthToDefault(basket, 1, 0.0, 100.0, curve, 1 * Days)->protectionLegNPV(), first,
                               1.0e-8);
    BOOST_CHECK_CLOSE_FRACTION(-nthToDefault(basket, 2, 0.0, 100.0, curve, 1 * Days)->protectionLegNPV(), second,
                               1.0e-8);
    BOOST_CHECK_EQUAL(nthToDefault(basket, 3, 0.0, 100.0, curve)->protectionLegNPV(), 0.0);
}

BOOST_AUTO_TEST_CASE(testEachNameOverAllRanks)
{
    BOOST_TEST_MESSAGE("Testing nth to defaults of every rank against each name on its own...");

    // A name that defaults triggers exactly one nth to default, whatever its rank turns out to be.
    // So the eight ranks' protection together is each name's own: a first to default on a basket
    // where only that name can default, and every name recovers what it does. The engine splits
    // each step's probability of the nth default among the names to second order in the step, so
    // on steps of three months the two agree to a few parts in a million where the recoveries
    // differ, and to the last bits where they don't. Before #208 every trigger paid the first
    // name's recovery.
    Settings::instance().evaluationDate() = inception();
    const auto curve = flatCurveFrom(inception());
    const std::vector<Real> hazards = risingHazards();
    for (const std::vector<Real>& recoveries : {std::vector<Real>(8, 0.4), risingRecoveries()})
    {
        BOOST_TEST_CONTEXT("recoveries from " << recoveries.front() << " to " << recoveries.back())
        {
            const auto basket = eightNames({.recoveries = recoveries});
            Real ranks = 0.0, names = 0.0;
            for (Size rank = 1; rank <= 8; ++rank)
                ranks += nthToDefault(basket, rank, 0.0, 100.0, curve)->protectionLegNPV();
            for (Size i = 0; i < 8; ++i)
            {
                std::vector<Real> alone(8, 0.0);
                alone[i] = hazards[i];
                const std::vector<Real> itsOwn(8, recoveries[i]);
                names += nthToDefault(eightNames({.hazards = alone, .recoveries = itsOwn}), 1, 0.0, 100.0, curve)
                             ->protectionLegNPV();
            }
            BOOST_CHECK_CLOSE_FRACTION(ranks, names, recoveries.front() == recoveries.back() ? 1.0e-12 : 1.0e-5);
        }
    }
}

BOOST_AUTO_TEST_CASE(testRecoveriesApartByTheirLastBit)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on recoveries apart by their last bit...");

    // Where one recovery differs from the others by its last bit, the engine takes each name's
    // probability of being the nth default; where they're the same, the probability of the nth
    // default. The two have to agree, at every rank.
    Settings::instance().evaluationDate() = inception();
    const auto curve = flatCurveFrom(inception());
    std::vector<Real> recoveries(8, 0.4);
    recoveries[5] = std::nextafter(0.4, 1.0);
    const auto apart = eightNames({.recoveries = recoveries});
    const auto same = eightNames();
    for (Size rank = 1; rank <= 8; ++rank)
    {
        BOOST_TEST_CONTEXT("rank " << rank)
        {
            BOOST_CHECK_CLOSE_FRACTION(nthToDefault(apart, rank, 0.0, 100.0, curve)->protectionLegNPV(),
                                       nthToDefault(same, rank, 0.0, 100.0, curve)->protectionLegNPV(), 1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testAfterADefaultOnRecoveriesThatDiffer)
{
    BOOST_TEST_MESSAGE("Testing an nth to default after a default, on recoveries that differ...");

    // As in testAfterADefault, the nth to default on eight names, one of which has defaulted, is
    // the one before it on the seven left; here with recoveries that differ. The defaulted name's
    // recovery doesn't count, and the names left keep theirs, whatever their places in the pool.
    // Before #208 the engine paid every trigger at the first name's recovery even once it had
    // defaulted: at 10 % against the others' 40 %, the protection was 50 % high.
    const Date today(1, November, 2006);
    Settings::instance().evaluationDate() = today;
    const auto curve = flatCurveFrom(today);
    std::vector<Real> firstApart(8, 0.4);
    firstApart[0] = 0.1;
    struct Case
    {
        const char* description;
        Size defaulted;
        std::vector<Real> recoveries;
    };
    const Case cases[] = {{"the fourth defaulted, recoveries of 20 % + 5 % i", 3, risingRecoveries()},
                          {"the first defaulted at 10 %, the others at 40 %", 0, firstApart}};
    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.description)
        {
            const auto all =
                eightNames({.oneName = OneName::defaulted, .which = c.defaulted, .recoveries = c.recoveries});
            const auto left = eightNames({.oneName = OneName::gone, .which = c.defaulted, .recoveries = c.recoveries});
            for (Size rank = 2; rank <= 4; ++rank)
            {
                BOOST_TEST_CONTEXT("rank " << rank)
                {
                    BOOST_CHECK_CLOSE_FRACTION(nthToDefault(all, rank, 0.0, 100.0, curve)->protectionLegNPV(),
                                               nthToDefault(left, rank - 1, 0.0, 100.0, curve)->protectionLegNPV(),
                                               1.0e-12);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRecoveriesThatDifferWithoutVectorIntegration)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on recoveries that differ, on a model without vector "
                       "integration...");

    // Each name's probability of being the nth default comes from integrating a vector over the
    // latent factor, which the trapezoid integration can't do, so the engine refuses. Before #208
    // it priced at the first name's recovery instead.
    Settings::instance().evaluationDate() = inception();
    const auto basket =
        eightNames({.recoveries = risingRecoveries(), .integration = LatentModelIntegrationType::Trapezoid});
    BOOST_CHECK_THROW(nthToDefault(basket, 1, 0.0, 100.0, flatCurveFrom(inception()))->NPV(), Error);
}

BOOST_AUTO_TEST_CASE(testRandomDefaultModelUpToToday)
{
    BOOST_TEST_MESSAGE("Testing the random default model's nth-default statistics up to today...");

    // The default curves here are dated today, so on or before today no name left has defaulted
    // on the model's paths, and the probability of at least n defaults is 1 up to the defaults so
    // far and 0 above, and each name's of being the nth is 0. On older curves the paths have
    // defaults before today, and since #232 the statistics count them. Before #207 the model threw
    // at any of these dates ("Date for statistic must be in the future.") and gave these values
    // only from the day after today. The probabilities of being the nth come one per name in the
    // pool, the defaulted one's included; before #233, once the defaults so far reached n, they
    // came one per name left, seven here. At a rank above the eight names they're 0, here and at a
    // date the paths have defaults by, as the probability of at least that many defaults is;
    // before #251 that rank threw where no name had defaulted, and gave 0s after one default.
    const Date today(1, November, 2006);
    Settings::instance().evaluationDate() = today;
    for (OneName oneName : {OneName::alive, OneName::defaulted})
    {
        const auto basket = eightNames({.oneName = oneName, .curveDate = today, .paths = 2000});
        const Size soFar = oneName == OneName::defaulted ? 1 : 0;
        for (const Date& d : {today, today - 1 * Days, inception()})
        {
            for (Size n = 1; n <= 8; ++n)
            {
                BOOST_TEST_CONTEXT(soFar << " defaults so far, at " << d << ", n = " << n)
                {
                    BOOST_CHECK_EQUAL(basket->probAtLeastNEvents(n, d), n <= soFar ? 1.0 : 0.0);
                    const std::vector<Probability> nth = basket->probsBeingNthEvent(n, d);
                    // one per name in the pool, the defaulted one too (#233)
                    BOOST_CHECK_EQUAL(nth.size(), 8U);
                    for (Probability p : nth)
                        BOOST_CHECK_EQUAL(p, 0.0);
                }
            }
        }
        for (const Date& d : {today, Date(1, September, 2011)})
        {
            BOOST_TEST_CONTEXT(soFar << " defaults so far, at " << d << ", n = 9")
            {
                BOOST_CHECK_EQUAL(basket->probAtLeastNEvents(9, d), 0.0);
                const std::vector<Probability> nth = basket->probsBeingNthEvent(9, d);
                BOOST_CHECK_EQUAL(nth.size(), 8U);
                for (Probability p : nth)
                    BOOST_CHECK_EQUAL(p, 0.0);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomDefaultModelInsideACoupon)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on the random default model inside a coupon...");

    // Inside a coupon the engine asks for the probability of the nth default at the coupon's start
    // or at the discount curve's reference date, whichever is later: today, on a curve as of
    // today. Before #207 the random default model threw there, so an nth to default priced on it
    // only on a coupon's first day. The default curves here start at the basket's inception, two
    // months before today. The model's paths count each default's day from its curve's date, as
    // ConstantLossModel's probabilities do, and since #232 so do its statistics, so it gives
    // ConstantLossModel's values within its Monte Carlo error. Before #232 they counted from
    // today, and its first-to-default protection leg came out at -28.06 against
    // ConstantLossModel's -26.88. Each path's protection lies between -60 and 0, and its premium
    // between 0 and the coupons' sum, 10.14, so on 20,000 paths their standard errors are at most
    // 0.212 and 0.036; the checks allow three of them.
    const Date today(1, November, 2006);
    Settings::instance().evaluationDate() = today;
    const auto curve = flatCurveFrom(today);
    const Size paths = 20000;
    const auto random = eightNames({.paths = paths});
    const auto latent = eightNames();
    const Real coupons = 0.02 * 100.0 * Actual360().yearFraction(Date(1, September, 2006), Date(1, September, 2011));
    for (Size rank = 1; rank <= 3; ++rank)
    {
        BOOST_TEST_CONTEXT("rank " << rank)
        {
            const auto priced = nthToDefault(random, rank, 0.0, 100.0, curve);
            const auto reference = nthToDefault(latent, rank, 0.0, 100.0, curve);
            BOOST_CHECK_SMALL(priced->protectionLegNPV() - reference->protectionLegNPV(),
                              3.0 * 60.0 / (2.0 * std::sqrt(Real(paths))));
            BOOST_CHECK_SMALL(priced->premiumLegNPV() - reference->premiumLegNPV(),
                              3.0 * coupons / (2.0 * std::sqrt(Real(paths))));
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomDefaultModelOnRecoveriesThatDiffer)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on the random default model, on recoveries that differ...");

    // Where the names' recoveries differ, the engine takes each name's probability of being the
    // nth default in each step, which the random default model gives from its paths since #231;
    // before, the engine threw on it. Its protection is ConstantLossModel's within their Monte
    // Carlo error, before the first coupon, and inside it after a default, where the first to
    // default has been triggered and both give 0. Each path's protection lies between -80, at the
    // lowest recovery of 20 %, and 0, so on 10,000 paths its standard error is at most 0.4; the
    // checks allow three of them, and the largest gap is 0.394. The premium doesn't depend on the
    // recoveries.
    const Size paths = 10000;
    struct Case
    {
        const char* description;
        Date today;
        OneName oneName;
    };
    const Case cases[] = {
        {"before the first coupon", inception(), OneName::alive},
        {"inside the first coupon, the fourth name defaulted", Date(1, November, 2006), OneName::defaulted}};
    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT(c.description)
        {
            Settings::instance().evaluationDate() = c.today;
            const auto curve = flatCurveFrom(c.today);
            const auto random = eightNames({.oneName = c.oneName, .recoveries = risingRecoveries(), .paths = paths});
            const auto latent = eightNames({.oneName = c.oneName, .recoveries = risingRecoveries()});
            for (Size rank = 1; rank <= 3; ++rank)
            {
                BOOST_TEST_CONTEXT("rank " << rank)
                {
                    BOOST_CHECK_SMALL(nthToDefault(random, rank, 0.0, 100.0, curve)->protectionLegNPV() -
                                          nthToDefault(latent, rank, 0.0, 100.0, curve)->protectionLegNPV(),
                                      3.0 * 80.0 / (2.0 * std::sqrt(Real(paths))));
                }
            }
        }
    }
}

// The random loss model CdoTests' randomLossModel builds, on eight names: GaussianSpotLossLM with
// recoveries of 40 % as the means, a correlation of 0.3 on its 16 variables and its parameter 2.2.
ext::shared_ptr<DefaultLossModel> randomLossModel(Size paths)
{
    const std::vector<std::vector<Real>> factors(16, std::vector<Real>(1, std::sqrt(0.3)));
    return ext::make_shared<RandomLossLM<GaussianCopulaPolicy>>(
        ext::make_shared<GaussianSpotLossLM>(factors, std::vector<Real>(8, 0.4), 2.2,
                                             LatentModelIntegrationType::GaussianQuadrature,
                                             GaussianCopulaPolicy::initTraits()),
        paths);
}

BOOST_AUTO_TEST_CASE(testRandomModelsNthEventsInAPeriod)
{
    BOOST_TEST_MESSAGE("Testing the random models' probabilities of being the nth default in a period...");

    // The random default and loss models take each name's probability of being the nth default
    // in a period from the paths that give its probability of being the nth by a date, so name by
    // name it's their difference at the period's ends, but for rounding, and the names' sum is
    // the difference of the probability of at least n defaults. They give one per name in the
    // pool, 0 for the defaulted name, as ConstantLossModel does; it gives no probability of being
    // the nth by a date to compare with. A period starting before the default curves' date, the
    // basket's inception, counts from it on the random models, as their paths do;
    // ConstantLossModel reads its curves there, and they refuse a date before theirs. As on
    // ConstantLossModel, a rank above the names gives 0 for each, as does a period of no length,
    // and a period that ends before it starts throws. Before #231 the random models threw on all
    // of it.
    const Date today(1, November, 2006);
    Settings::instance().evaluationDate() = today;
    const std::vector<std::pair<Date, Date>> periods = {{inception() - 10 * Days, Date(1, December, 2006)},
                                                        {inception(), Date(1, March, 2007)},
                                                        {today, Date(1, February, 2007)},
                                                        {Date(1, March, 2007), Date(1, September, 2011)},
                                                        {Date(1, March, 2007), Date(1, March, 2007)}};
    for (OneName oneName : {OneName::alive, OneName::defaulted})
    {
        const Size soFar = oneName == OneName::defaulted ? 1 : 0;
        const auto randomLoss = eightNames({.oneName = oneName});
        randomLoss->setLossModel(randomLossModel(2000));
        struct Model
        {
            const char* name;
            ext::shared_ptr<Basket> basket;
            bool random;
        };
        const Model models[] = {
            {"RandomDefaultLM", eightNames({.oneName = oneName, .recoveries = risingRecoveries(), .paths = 2000}),
             true},
            {"RandomLossLM", randomLoss, true},
            {"ConstantLossModel", eightNames({.oneName = oneName, .recoveries = risingRecoveries()}), false}};
        for (const auto& [name, basket, random] : models)
        {
            for (Size n = 1; n <= 8; ++n)
            {
                for (const auto& [d0, d] : periods)
                {
                    if (!random && d0 < inception())
                        continue;
                    BOOST_TEST_CONTEXT(name << ", " << soFar << " defaults so far, n = " << n << ", from " << d0
                                            << " to " << d)
                    {
                        const std::vector<Probability> between = basket->probsBeingNthEventBetween(n, d0, d);
                        BOOST_REQUIRE_EQUAL(between.size(), 8U);
                        if (soFar == 1)
                            BOOST_CHECK_EQUAL(between[3], 0.0);
                        if (d0 == d)
                            for (Probability p : between)
                                BOOST_CHECK_EQUAL(p, 0.0);
                        if (random)
                        {
                            const std::vector<Probability> byEnd = basket->probsBeingNthEvent(n, d);
                            const std::vector<Probability> byStart = basket->probsBeingNthEvent(n, d0);
                            Real sum = 0.0;
                            for (Size i = 0; i < 8; ++i)
                            {
                                BOOST_CHECK_SMALL(between[i] - (byEnd[i] - byStart[i]), 1.0e-15);
                                sum += between[i];
                            }
                            BOOST_CHECK_SMALL(
                                sum - (basket->probAtLeastNEvents(n, d) - basket->probAtLeastNEvents(n, d0)), 1.0e-15);
                        }
                    }
                }
            }
            BOOST_TEST_CONTEXT(name << ", " << soFar << " defaults so far")
            {
                // the checks above aren't on zeros only: in four and a half years each name left can be the next
                const std::vector<Probability> next =
                    basket->probsBeingNthEventBetween(soFar + 1, Date(1, March, 2007), Date(1, September, 2011));
                for (Size i = 0; i < 8; ++i)
                    if (soFar == 0 || i != 3)
                        BOOST_CHECK_GT(next[i], 0.0);
                const std::vector<Probability> aboveTheNames =
                    basket->probsBeingNthEventBetween(9, today, today + 1 * Years);
                BOOST_CHECK_EQUAL(aboveTheNames.size(), 8U);
                for (Probability p : aboveTheNames)
                    BOOST_CHECK_EQUAL(p, 0.0);
                BOOST_CHECK_THROW(basket->probsBeingNthEventBetween(2, today + 1 * Years, today), Error);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomLossModelGivesNoExpectedRecovery)
{
    BOOST_TEST_MESSAGE("Testing an nth to default on the random loss model...");

    // The engine pays a trigger at the loss model's expected recovery for its name. The random
    // loss model draws a recovery with each default, and a trigger pays at the one its default
    // drew; the expected recovery given a name's default isn't the one given that it's the
    // trigger. So since #234 the model gives none, with a message that says why, and the engine
    // throws it. Before, both threw DefaultLossModel's "expected recovery Not implemented for this
    // model.", before the first coupon and inside it alike.
    const ExpectedErrorMessage why("draws a recovery with each default");
    for (const Date& today : {inception(), Date(1, November, 2006)})
    {
        BOOST_TEST_CONTEXT("today " << today)
        {
            Settings::instance().evaluationDate() = today;
            const auto basket = eightNames();
            basket->setLossModel(randomLossModel(2000));
            BOOST_CHECK_EXCEPTION(nthToDefault(basket, 1, 0.0, 100.0, flatCurveFrom(today))->NPV(), Error, why);
            BOOST_CHECK_EXCEPTION(basket->recoveryRate(today + 1 * Years, 0), Error, why);
        }
    }
}

// Names on flat hazard rates from 31 August 2006, each with its own correlation to one Gaussian
// factor, and the default latent model on them.
struct LatentNames
{
    Date today{31, August, 2006};
    NorthAmericaCorpDefaultKey key{EURCurrency(), SeniorSec, Period(), 1.};
    ext::shared_ptr<Pool> pool = ext::make_shared<Pool>();
    std::vector<std::string> names;
    ext::shared_ptr<GaussianDefProbLM> model;

    LatentNames(const std::vector<Real>& hazards, const std::vector<Real>& correlations)
    {
        std::vector<std::vector<Real>> loadings;
        for (Size i = 0; i < hazards.size(); ++i)
        {
            names.push_back("issuer-" + std::to_string(i));
            Handle<DefaultProbabilityTermStructure> curve(
                ext::make_shared<FlatHazardRate>(today, makeQuoteHandle(hazards[i]), ActualActual(ActualActual::ISDA)));
            pool->add(names.back(), Issuer(std::vector<Issuer::key_curve_pair>(1, std::make_pair(key, curve))), key);
            loadings.emplace_back(1, std::sqrt(correlations[i]));
        }
        model = ext::make_shared<GaussianDefProbLM>(loadings, LatentModelIntegrationType::GaussianQuadrature);
        model->resetBasket(
            ext::make_shared<Basket>(today, names, std::vector<Real>(names.size(), 100.0), pool, 0.0, 1.0));
    }

    Probability probability(Size i, const Date& d) const
    {
        return pool->get(names[i]).defaultProbability(key)->defaultProbability(d);
    }
};

BOOST_AUTO_TEST_CASE(testAtLeastNDefaultsAgainstEveryCombination)
{
    BOOST_TEST_MESSAGE("Testing the latent model's probability of n defaults or more against every combination...");

    /* Given the factor the names default independently, so the probability of n defaults or more
       is the sum, over every set of n names or more, of the probability that exactly they default.
       Until #264 the model summed so at each node, over all 2^L sets of its L names; it now builds
       the count one name at a time. This holds the count to the sum, which the old code passed
       too. The two agreed within 5.1e-16 relative with GCC 15 at -O3; the tolerance leaves room for
       other compilers' rounding. Name 4 can't default, and above the names' number the
       probability is 0. */
    std::vector<Real> hazards, correlations;
    for (Size i = 0; i < 10; ++i)
    {
        hazards.push_back(i == 4 ? 0.0 : 0.005 + 0.004 * i);
        correlations.push_back(0.05 + 0.06 * i);
    }
    const LatentNames names(hazards, correlations);
    Settings::instance().evaluationDate() = names.today;
    const auto& model = names.model;
    const Size size = hazards.size();
    for (const Period& tenor : {Period(1, Years), Period(5, Years)})
    {
        const Date d = names.today + tenor;
        std::vector<Probability> p(size);
        std::vector<Real> inverses(size, 0.0);
        for (Size i = 0; i < size; ++i)
        {
            p[i] = names.probability(i, d);
            if (p[i] >= 1.0e-10)
                inverses[i] = model->inverseCumulativeY(p[i], i);
        }
        for (Size n = 0; n <= size + 1; ++n)
        {
            const Real everySet = model->integratedExpectedValue(
                [&](const std::vector<Real>& m)
                {
                    std::vector<Probability> q(size);
                    for (Size i = 0; i < size; ++i)
                        q[i] = p[i] < 1.0e-10 ? 0.0 : model->conditionalDefaultProbabilityInvP(inverses[i], i, m);
                    Real sum = 0.0;
                    for (Size set = 0; set < (Size(1) << size); ++set)
                    {
                        if (Size(std::popcount(set)) < n)
                            continue;
                        Real product = 1.0;
                        for (Size i = 0; i < size; ++i)
                            product *= ((set >> i) & 1) != 0U ? q[i] : 1.0 - q[i];
                        sum += product;
                    }
                    return sum;
                });
            BOOST_TEST_CONTEXT("at least " << n << " by " << d)
            {
                BOOST_CHECK_CLOSE_FRACTION(model->probAtLeastNEvents(n, d), everySet, 1.0e-13);
            }
        }
        // Above the names' number the count is 0 at each node.
        BOOST_CHECK_EQUAL(model->probAtLeastNEvents(size + 1, d), 0.0);
    }
}

BOOST_AUTO_TEST_CASE(testAtLeastNDefaultsOnManyNames)
{
    BOOST_TEST_MESSAGE("Testing the latent model's probability of n defaults or more on 64 names and more...");

    /* Given the factor, the defaults among L names alike are binomial at their conditional
       probability q: at least one with probability 1 - (1 - q)^L, at least two with that less
       L q (1 - q)^(L - 1). Until #264 the model enumerated the 2^L sets of names in a 64-bit
       count, where 2^L is out of range from 64 names on, and gave 0 there. The count built one
       name at a time agreed with these within 1.7e-16 with GCC 15 at -O3. */
    for (Size size : {Size(64), Size(100)})
    {
        BOOST_TEST_CONTEXT(size << " names")
        {
            const LatentNames names(std::vector<Real>(size, 0.01), std::vector<Real>(size, 0.3));
            Settings::instance().evaluationDate() = names.today;
            const auto& model = names.model;
            const Date d = names.today + 5 * Years;
            const Real inverse = model->inverseCumulativeY(names.probability(0, d), 0);
            const auto q = [&](const std::vector<Real>& m)
            { return model->conditionalDefaultProbabilityInvP(inverse, 0, m); };
            const Real none = model->integratedExpectedValue([&](const std::vector<Real>& m)
                                                             { return std::pow(1.0 - q(m), Real(size)); });
            const Real one = model->integratedExpectedValue(
                [&](const std::vector<Real>& m) { return size * q(m) * std::pow(1.0 - q(m), Real(size - 1)); });
            const Real mass = model->integratedExpectedValue([](const std::vector<Real>&) { return 1.0; });
            BOOST_CHECK_SMALL(model->probAtLeastNEvents(1, d) - (mass - none), 1.0e-13);
            BOOST_CHECK_SMALL(model->probAtLeastNEvents(2, d) - (mass - none - one), 1.0e-13);
        }
    }
}

#endif

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
