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
#include <ql/experimental/credit/basecorrelationlossmodel.hpp>
#include <ql/experimental/credit/basecorrelationstructure.hpp>
#include <ql/experimental/credit/binomiallossmodel.hpp>
#include <ql/experimental/credit/cdo.hpp>
#include <ql/experimental/credit/defaultevent.hpp>
#include <ql/experimental/credit/gaussianlhplossmodel.hpp>
#include <ql/experimental/credit/homogeneouspooldef.hpp>
#include <ql/experimental/credit/inhomogeneouspooldef.hpp>
#include <ql/experimental/credit/integralcdoengine.hpp>
#include <ql/experimental/credit/midpointcdoengine.hpp>
#include <ql/experimental/credit/pool.hpp>
#include <ql/experimental/credit/randomdefaultlatentmodel.hpp>
#include <ql/experimental/credit/randomlosslatentmodel.hpp>
#include <ql/experimental/credit/recursivelossmodel.hpp>
#include <ql/experimental/credit/saddlepointlossmodel.hpp>
#include <ql/experimental/credit/spotlosslatentmodel.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <boost/mpl/vector.hpp>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CdoTests)

#ifndef QL_PATCH_SOLARIS

Real hwAttachment[] = {0.00, 0.03, 0.06, 0.10};
Real hwDetachment[] = {0.03, 0.06, 0.10, 1.00};

struct hwDatum
{
    Real correlation;
    Integer nm;
    Integer nz;
    Real trancheSpread[4];
};

// HW Table 7
// corr, Nm, Nz, 0-3, 3-6, 6-10, 10-100
hwDatum hwData7[] = {{0.1, -1, -1, {2279, 450, 89, 1}},
                     {0.3, -1, -1, {1487, 472, 203, 7}},
                     // Opening the T, T&G tests too. The convolution is analytical
                     //   now so it runs it a time comparable to the gaussian tests and
                     //   has enough precission to pass the tests.
                     // Below the T models are integrated with a quadrature, even if this
                     //   is incorrect the test pass good enough, the quadrature gets to
                     //   be worst as the kernel deviates from a normal, this is low
                     //   orders of the T; here 5 is enough, 3 would not be.
                     {0.3, -1, 5, {1766, 420, 161, 6}},
                     {0.3, 5, -1, {1444, 408, 171, 10}},
                     {0.3, 5, 5, {1713, 359, 136, 9}}};

void check(int i, int j, const std::string& desc, Real found, Real expected, Real bpTolerance, Real relativeTolerance)
{
    /* Uncomment to display the full show if your debugging:
       std::cout<< "Case: "<< i << " " << j << " " << found << " :: "
       << expected  <<  " ("<< desc << ") " << std::endl;
    */
    Real absDiff = found - expected;
    Real relDiff = absDiff / expected;
    BOOST_CHECK_MESSAGE(fabs(relDiff) < relativeTolerance || fabs(absDiff) < bpTolerance,
                        "case " << i << " " << j << " (" << desc << "): " << found << " vs. " << expected);
}


struct dataSetOne
{
    static const int dataset{0};
};
struct dataSetTwo
{
    static const int dataset{1};
};
struct dataSetThree
{
    static const int dataset{2};
};
struct dataSetFour
{
    static const int dataset{3};
};
struct dataSetFive
{
    static const int dataset{4};
};

void testHullWhiteValues(const int dataSet)
{
    BOOST_TEST_MESSAGE("Testing CDO premiums against Hull-White values"
                       " for data set "
                       << dataSet << "...");

    Size poolSize = 100;
    Real lambda = 0.01;

    // nBuckets and period determine the computation time
    Size nBuckets = 200;
    // Period period = 1*Months;
    // for MC engines
    Size numSims = 5000;

    Real rate = 0.05;
    DayCounter daycount = Actual360();
    Compounding cmp = Continuous; // Simple;

    Real recovery = 0.4;
    std::vector<Real> nominals(poolSize, 100.0);
    Real premium = 0.02;
    Period maxTerm(5, Years);
    Schedule schedule = MakeSchedule()
                            .from(Date(1, September, 2006))
                            .to(Date(1, September, 2011))
                            .withTenor(Period(3, Months))
                            .withCalendar(TARGET());

    Date asofDate = Date(31, August, 2006);

    Settings::instance().evaluationDate() = asofDate;

    ext::shared_ptr<YieldTermStructure> yieldPtr(new FlatForward(asofDate, rate, daycount, cmp));
    Handle<YieldTermStructure> yieldHandle(yieldPtr);

    Handle<Quote> hazardRate(ext::shared_ptr<Quote>(new SimpleQuote(lambda)));
    std::vector<Handle<DefaultProbabilityTermStructure>> basket;
    ext::shared_ptr<DefaultProbabilityTermStructure> ptr(
        new FlatHazardRate(asofDate, hazardRate, ActualActual(ActualActual::ISDA)));
    ext::shared_ptr<Pool> pool(new Pool());
    std::vector<std::string> names;
    // probability key items
    std::vector<Issuer> issuers;
    std::vector<std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities;
    probabilities.emplace_back(NorthAmericaCorpDefaultKey(EURCurrency(), SeniorSec, Period(0, Weeks), 10.),
                               Handle<DefaultProbabilityTermStructure>(ptr));

    for (Size i = 0; i < poolSize; ++i)
    {
        std::ostringstream o;
        o << "issuer-" << i;
        names.push_back(o.str());
        basket.emplace_back(ptr);
        issuers.emplace_back(probabilities);
        pool->add(names.back(), issuers.back(),
                  NorthAmericaCorpDefaultKey(EURCurrency(), QuantLib::SeniorSec, Period(), 1.));
    }

    ext::shared_ptr<SimpleQuote> correlation(new SimpleQuote(0.0));
    Handle<Quote> hCorrelation(correlation);
    QL_REQUIRE(std::size(hwAttachment) == std::size(hwDetachment), "data length does not match");

    ext::shared_ptr<PricingEngine> midPCDOEngine(new MidPointCDOEngine(yieldHandle));
    ext::shared_ptr<PricingEngine> integralCDOEngine(new IntegralCDOEngine(yieldHandle));

    const Size i = dataSet;
    correlation->setValue(hwData7[i].correlation);
    QL_REQUIRE(std::size(hwAttachment) == std::size(hwData7[i].trancheSpread), "data length does not match");
    std::vector<ext::shared_ptr<DefaultLossModel>> basketModels;
    std::vector<std::string> modelNames;
    std::vector<Real> relativeToleranceMidp, relativeTolerancePeriod, absoluteTolerance;

    if (hwData7[i].nm == -1 && hwData7[i].nz == -1)
    {
        ext::shared_ptr<GaussianConstantLossLM> gaussKtLossLM(new GaussianConstantLossLM(
            hCorrelation, std::vector<Real>(poolSize, recovery), LatentModelIntegrationType::GaussianQuadrature,
            poolSize, GaussianCopulaPolicy::initTraits()));

        // 1.-Inhomogeneous gaussian
        modelNames.emplace_back("Inhomogeneous gaussian");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new IHGaussPoolLossModel(gaussKtLossLM, nBuckets, 5., -5, 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 2.-homogeneous gaussian
        modelNames.emplace_back("Homogeneous gaussian");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new HomogGaussPoolLossModel(gaussKtLossLM, nBuckets, 5., -5, 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 3.-random default gaussian
        modelNames.emplace_back("Random default gaussian");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new RandomDefaultLM<GaussianCopulaPolicy>(gaussKtLossLM, numSims)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.07);
        relativeTolerancePeriod.push_back(0.07);
        // SECOND MC
        // gaussian LHP
        modelNames.emplace_back("Gaussian LHP");
        basketModels.push_back(ext::shared_ptr<DefaultLossModel>(
            new GaussianLHPLossModel(hCorrelation, std::vector<Real>(poolSize, recovery))));
        absoluteTolerance.push_back(10.);
        relativeToleranceMidp.push_back(0.5);
        relativeTolerancePeriod.push_back(0.5);
        // Binomial...
        // Saddle point...
        // Recursive ...
    }
    else if (hwData7[i].nm > 0 && hwData7[i].nz > 0)
    {
        TCopulaPolicy::initTraits initTG;
        initTG.tOrders.push_back(hwData7[i].nm);
        initTG.tOrders.push_back(hwData7[i].nz);
        ext::shared_ptr<TConstantLossLM> TKtLossLM(
            new TConstantLossLM(hCorrelation, std::vector<Real>(poolSize, recovery),
                                LatentModelIntegrationType::GaussianQuadrature, poolSize, initTG));
        // 1.-inhomogeneous studentT
        modelNames.emplace_back("Inhomogeneous student");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new IHStudentPoolLossModel(TKtLossLM, nBuckets, 5., -5., 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 2.-homogeneous student T
        modelNames.emplace_back("Homogeneous student");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new HomogTPoolLossModel(TKtLossLM, nBuckets, 5., -5., 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 3.-random default student T
        modelNames.emplace_back("Random default studentT");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new RandomDefaultLM<TCopulaPolicy>(TKtLossLM, numSims)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.07);
        relativeTolerancePeriod.push_back(0.07);
        // SECOND MC
        // Binomial...
        // Saddle point...
        // Recursive ...
    }
    else if (hwData7[i].nm > 0 && hwData7[i].nz == -1)
    {
        TCopulaPolicy::initTraits initTG;
        initTG.tOrders.push_back(hwData7[i].nm);
        initTG.tOrders.push_back(45);
        /* T_{55} is pretty close to a gaussian. Probably theres no need to
        be this conservative as the polynomial convolution gets shorter and
        faster as the order decreases.
        */
        ext::shared_ptr<TConstantLossLM> TKtLossLM(
            new TConstantLossLM(hCorrelation, std::vector<Real>(poolSize, recovery),
                                LatentModelIntegrationType::GaussianQuadrature, poolSize, initTG));
        // 1.-inhomogeneous
        modelNames.emplace_back("Inhomogeneous student-gaussian");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new IHStudentPoolLossModel(TKtLossLM, nBuckets, 5., -5., 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 2.-homogeneous
        modelNames.emplace_back("Homogeneous student-gaussian");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new HomogTPoolLossModel(TKtLossLM, nBuckets, 5., -5., 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 3.-random default
        modelNames.emplace_back("Random default student-gaussian");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new RandomDefaultLM<TCopulaPolicy>(TKtLossLM, numSims)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.07);
        relativeTolerancePeriod.push_back(0.07);
        // SECOND MC
        // Binomial...
        // Saddle point...
        // Recursive ...
    }
    else if (hwData7[i].nm == -1 && hwData7[i].nz > 0)
    {
        TCopulaPolicy::initTraits initTG;
        initTG.tOrders.push_back(45); // pretty close to gaussian
        initTG.tOrders.push_back(hwData7[i].nz);
        ext::shared_ptr<TConstantLossLM> TKtLossLM(
            new TConstantLossLM(hCorrelation, std::vector<Real>(poolSize, recovery),
                                LatentModelIntegrationType::GaussianQuadrature, poolSize, initTG));
        // 1.-inhomogeneous gaussian
        modelNames.emplace_back("Inhomogeneous gaussian-student");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new IHStudentPoolLossModel(TKtLossLM, nBuckets, 5., -5., 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 2.-homogeneous gaussian
        modelNames.emplace_back("Homogeneous gaussian-student");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new HomogTPoolLossModel(TKtLossLM, nBuckets, 5., -5., 15)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.04);
        relativeTolerancePeriod.push_back(0.04);
        // 3.-random default gaussian
        modelNames.emplace_back("Random default gaussian-student");
        basketModels.push_back(
            ext::shared_ptr<DefaultLossModel>(new RandomDefaultLM<TCopulaPolicy>(TKtLossLM, numSims)));
        absoluteTolerance.push_back(1.);
        relativeToleranceMidp.push_back(0.07);
        relativeTolerancePeriod.push_back(0.07);
        // SECOND MC
        // Binomial...
        // Saddle point...
        // Recursive ...
    }
    else
    {
        return;
    }

    for (Size j = 0; j < std::size(hwAttachment); j++)
    {
        ext::shared_ptr<Basket> basketPtr(
            new Basket(asofDate, names, nominals, pool, hwAttachment[j], hwDetachment[j]));
        std::ostringstream trancheId;
        trancheId << "[" << hwAttachment[j] << " , " << hwDetachment[j] << "]";
        SyntheticCDO cdoe(basketPtr, Protection::Seller, schedule, 0.0, premium, daycount, Following);

        for (Size im = 0; im < basketModels.size(); im++)
        {

            basketPtr->setLossModel(basketModels[im]);

            cdoe.setPricingEngine(midPCDOEngine);
            check(i, j, modelNames[im] + std::string(" with midp integration on ") + trancheId.str(),
                  cdoe.fairPremium() * 1e4, hwData7[i].trancheSpread[j], absoluteTolerance[im],
                  relativeToleranceMidp[im]);

            cdoe.setPricingEngine(integralCDOEngine);
            check(i, j, modelNames[im] + std::string(" with step integration on ") + trancheId.str(),
                  cdoe.fairPremium() * 1e4, hwData7[i].trancheSpread[j], absoluteTolerance[im],
                  relativeTolerancePeriod[im]);
        }
    }
}

// The Gaussian data sets take about 2 s each, and those with a Student t factor about 10 s.
using gaussianDataSets = boost::mpl::vector<dataSetOne, dataSetTwo>;
using studentTDataSets = boost::mpl::vector<dataSetThree, dataSetFour, dataSetFive>;

BOOST_TEST_DECORATOR(*precondition(if_speed(Fast)))
BOOST_AUTO_TEST_CASE_TEMPLATE(testHW, T, gaussianDataSets)
{
    testHullWhiteValues(T::dataset);
}

BOOST_TEST_DECORATOR(*precondition(if_speed(Slow)))
BOOST_AUTO_TEST_CASE_TEMPLATE(testHWStudentT, T, studentTDataSets)
{
    testHullWhiteValues(T::dataset);
}
#endif

struct SharedModelVars
{
    static constexpr Size poolSize = 20;
    static constexpr Real recovery = 0.4;

    Date today{31, August, 2006};
    ext::shared_ptr<SimpleQuote> correlation, hazard, lastNameHazard;
    Handle<Quote> hCorrelation;
    ext::shared_ptr<Pool> pool;
    std::vector<std::string> names;
    Handle<YieldTermStructure> discountCurve;
    Schedule schedule;

    explicit SharedModelVars(Real correlationValue,
                             const Date& defaultDate = Date(),
                             Real hazardValue = 0.01,
                             bool bankruptcy = false)
    : correlation(ext::make_shared<SimpleQuote>(correlationValue)), hazard(ext::make_shared<SimpleQuote>(hazardValue)),
      lastNameHazard(ext::make_shared<SimpleQuote>(hazardValue)), hCorrelation(correlation),
      pool(ext::make_shared<Pool>()), discountCurve(ext::make_shared<FlatForward>(today, 0.05, Actual360(), Continuous))
    {
        Handle<DefaultProbabilityTermStructure> hazardCurve(
            ext::make_shared<FlatHazardRate>(today, Handle<Quote>(hazard), ActualActual(ActualActual::ISDA)));
        Handle<DefaultProbabilityTermStructure> lastNameCurve(
            ext::make_shared<FlatHazardRate>(today, Handle<Quote>(lastNameHazard), ActualActual(ActualActual::ISDA)));
        for (Size i = 0; i < poolSize; ++i)
        {
            names.push_back("issuer-" + std::to_string(i));
            std::vector<std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities;
            probabilities.emplace_back(NorthAmericaCorpDefaultKey(EURCurrency(), SeniorSec, Period(0, Weeks), 10.),
                                       i + 1 == poolSize ? lastNameCurve : hazardCurve);
            DefaultEventSet events;
            if (i == 3 && defaultDate != Date() && bankruptcy)
                events.insert(ext::make_shared<BankruptcyEvent>(defaultDate, EURCurrency(), SeniorSec, defaultDate + 5,
                                                                recovery));
            else if (i == 3 && defaultDate != Date())
                events.insert(ext::make_shared<FailureToPayEvent>(defaultDate, EURCurrency(), SeniorSec, 10.0,
                                                                  defaultDate + 5, recovery));
            pool->add(names.back(), Issuer(probabilities, events),
                      NorthAmericaCorpDefaultKey(EURCurrency(), SeniorSec, Period(), 1.));
        }
        schedule = MakeSchedule()
                       .from(Date(1, September, 2006))
                       .to(Date(1, September, 2011))
                       .withTenor(Period(3, Months))
                       .withCalendar(TARGET());
    }

    void moveHazard(Real value) const
    {
        hazard->setValue(value);
        lastNameHazard->setValue(value);
    }

    ext::shared_ptr<GaussianConstantLossLM> latentModel() const
    {
        return ext::make_shared<GaussianConstantLossLM>(hCorrelation, std::vector<Real>(poolSize, recovery),
                                                        LatentModelIntegrationType::GaussianQuadrature, poolSize,
                                                        GaussianCopulaPolicy::initTraits());
    }

    ext::shared_ptr<DefaultLossModel> lhpModel() const
    {
        return ext::make_shared<GaussianLHPLossModel>(hCorrelation, std::vector<Real>(poolSize, recovery));
    }

    ext::shared_ptr<DefaultLossModel> homogeneousModel() const
    {
        return ext::make_shared<HomogGaussPoolLossModel>(latentModel(), 100, 5., -5., 15);
    }

    ext::shared_ptr<DefaultLossModel> inhomogeneousModel() const
    {
        return ext::make_shared<IHGaussPoolLossModel>(latentModel(), 100, 5., -5., 15);
    }

    ext::shared_ptr<DefaultLossModel> binomialModel() const
    {
        return ext::make_shared<GaussianBinomialLossModel>(latentModel());
    }

    ext::shared_ptr<DefaultLossModel> recursiveModel() const
    {
        return ext::make_shared<RecursiveGaussLossModel>(latentModel());
    }

    ext::shared_ptr<DefaultLossModel> saddlePointModel() const
    {
        return ext::make_shared<SaddlePointLossModel<GaussianCopulaPolicy>>(latentModel());
    }

    ext::shared_ptr<DefaultLossModel> randomDefaultModel() const
    {
        return ext::make_shared<RandomDefaultLM<GaussianCopulaPolicy>>(latentModel(), 2000);
    }

    ext::shared_ptr<DefaultLossModel> randomLossModel() const
    {
        const std::vector<std::vector<Real>> weights(2 * poolSize,
                                                     std::vector<Real>(1, std::sqrt(correlation->value())));
        auto latent = ext::make_shared<GaussianSpotLossLM>(weights, std::vector<Real>(poolSize, recovery), 2.2,
                                                           LatentModelIntegrationType::GaussianQuadrature,
                                                           GaussianCopulaPolicy::initTraits());
        return ext::make_shared<RandomLossLM<GaussianCopulaPolicy>>(latent, 2000, 1.0e-6, 2863311530UL);
    }

    ext::shared_ptr<DefaultLossModel> baseCorrelationModel() const
    {
        const std::vector<std::vector<Handle<Quote>>> quotes(2, std::vector<Handle<Quote>>(2, hCorrelation));
        auto surface = ext::make_shared<BaseCorrelationTermStructure<BilinearInterpolation>>(
            0, TARGET(), Unadjusted, std::vector<Period>{Period(1, Years), Period(10, Years)},
            std::vector<Real>{0.03, 0.5}, quotes, Actual365Fixed());
        surface->enableExtrapolation();
        return ext::make_shared<GaussianLHPFlatBCLM>(
            Handle<BaseCorrelationTermStructure<BilinearInterpolation>>(surface), std::vector<Real>(poolSize, recovery),
            GaussianCopulaPolicy::initTraits());
    }

    ext::shared_ptr<Basket> basket(Real attachment, Real detachment) const
    {
        return basket(attachment, detachment, std::vector<Real>(poolSize, 100.0));
    }

    ext::shared_ptr<Basket> basket(Real attachment, Real detachment, const std::vector<Real>& notionals) const
    {
        return ext::make_shared<Basket>(today, names, notionals, pool, attachment, detachment);
    }

    ext::shared_ptr<SyntheticCDO> cdo(const ext::shared_ptr<Basket>& onBasket) const
    {
        auto tranche =
            ext::make_shared<SyntheticCDO>(onBasket, Protection::Seller, schedule, 0.0, 0.02, Actual360(), Following);
        tranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(discountCurve));
        return tranche;
    }
};

using LossModelFactory = ext::shared_ptr<DefaultLossModel> (SharedModelVars::*)() const;

struct LossModelCase
{
    std::string name;
    LossModelFactory factory;
    bool followsCorrelationQuote;
};

std::vector<LossModelCase> everyLossModel()
{
    return {{"LHP", &SharedModelVars::lhpModel, true},
            {"homogeneous", &SharedModelVars::homogeneousModel, true},
            {"inhomogeneous", &SharedModelVars::inhomogeneousModel, true},
            {"binomial", &SharedModelVars::binomialModel, true},
            {"recursive", &SharedModelVars::recursiveModel, true},
            {"saddle point", &SharedModelVars::saddlePointModel, true},
            {"random default", &SharedModelVars::randomDefaultModel, true},
            {"random loss", &SharedModelVars::randomLossModel, false},
            {"base correlation", &SharedModelVars::baseCorrelationModel, true}};
}

Real premiumOnNewModel(LossModelFactory factory, Real correlation, Real hazard, Real attachment, Real detachment)
{
    SharedModelVars vars(correlation, Date(), hazard);
    auto basket = vars.basket(attachment, detachment);
    basket->setLossModel((vars.*factory)());
    return vars.cdo(basket)->fairPremium();
}

BOOST_AUTO_TEST_CASE(testRandomDefaultModelOnTwoTranches)
{
    BOOST_TEST_MESSAGE("Testing two tranches on one random-default loss model...");

    SharedModelVars vars(0.3);
    Settings::instance().evaluationDate() = vars.today;

    auto first = vars.basket(0.0, 0.1);
    auto second = vars.basket(0.1, 0.3);
    auto model = vars.randomDefaultModel();
    first->setLossModel(model);
    second->setLossModel(model);
    auto firstTranche = vars.cdo(first);
    auto secondTranche = vars.cdo(second);

    auto premiumOnItsOwnModel = [](const SharedModelVars& onVars, Real attachment, Real detachment)
    {
        auto basket = onVars.basket(attachment, detachment);
        basket->setLossModel(onVars.randomDefaultModel());
        return onVars.cdo(basket)->fairPremium();
    };

    const Real tolerance = 1.0e-12;

    const Real firstPremium = firstTranche->fairPremium();
    const Real secondPremium = secondTranche->fairPremium();
    BOOST_CHECK_SMALL(firstPremium - premiumOnItsOwnModel(vars, 0.0, 0.1), tolerance);
    BOOST_CHECK_SMALL(secondPremium - premiumOnItsOwnModel(vars, 0.1, 0.3), tolerance);

    // The second tranche has the model now, so the first has to take it back.
    firstTranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(vars.discountCurve));
    BOOST_CHECK_SMALL(firstTranche->fairPremium() - firstPremium, tolerance);
    secondTranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(vars.discountCurve));
    BOOST_CHECK_SMALL(secondTranche->fairPremium() - secondPremium, tolerance);

    // A move in the correlation has to reach both of them, whichever has the model.
    vars.correlation->setValue(0.6);
    SharedModelVars moved(0.6);
    BOOST_CHECK_SMALL(firstTranche->fairPremium() - premiumOnItsOwnModel(moved, 0.0, 0.1), tolerance);
    BOOST_CHECK_SMALL(secondTranche->fairPremium() - premiumOnItsOwnModel(moved, 0.1, 0.3), tolerance);
}

BOOST_AUTO_TEST_CASE(testBasketDoesNotInvalidateItselfTakingItsModel)
{
    BOOST_TEST_MESSAGE("Testing that a basket isn't invalidated by taking its loss model...");

    SharedModelVars vars(0.3);
    Settings::instance().evaluationDate() = vars.today;

    auto basket = vars.basket(0.0, 0.2);
    auto model = vars.randomDefaultModel();
    basket->setLossModel(model);
    auto tranche = vars.cdo(basket);
    const Real premium = tranche->fairPremium();

    // The model is calculated and the basket isn't, so the model tells its baskets as it resets.
    basket->setLossModel(model);
    Flag flag;
    flag.registerWith(basket);

    BOOST_CHECK_SMALL(tranche->fairPremium() - premium, 1.0e-12);
    BOOST_CHECK(!flag.isUp());
}

BOOST_AUTO_TEST_CASE(testLossModelsAfterANameDefaults)
{
    BOOST_TEST_MESSAGE("Testing that loss models follow their basket when a name defaults...");

    const Date defaultDate(15, September, 2006);
    const Date afterDefault(1, November, 2006);

    using ModelFactory = ext::shared_ptr<DefaultLossModel> (SharedModelVars::*)() const;
    const std::vector<std::pair<std::string, ModelFactory>> models = {
        {"LHP", &SharedModelVars::lhpModel},
        {"homogeneous", &SharedModelVars::homogeneousModel},
        {"binomial", &SharedModelVars::binomialModel},
        {"recursive", &SharedModelVars::recursiveModel},
        {"saddle point", &SharedModelVars::saddlePointModel},
        {"random default", &SharedModelVars::randomDefaultModel},
        {"random loss", &SharedModelVars::randomLossModel}};

    for (const auto& model : models)
    {
        BOOST_TEST_CONTEXT(model.first)
        {
            SharedModelVars vars(0.3, defaultDate);
            Settings::instance().evaluationDate() = vars.today;
            auto basket = vars.basket(0.0, 0.2);
            basket->setLossModel((vars.*model.second)());
            auto tranche = vars.cdo(basket);
            const Real before = tranche->fairPremium();

            Settings::instance().evaluationDate() = afterDefault;
            const Real after = tranche->fairPremium();

            SharedModelVars freshVars(0.3, defaultDate);
            auto freshBasket = freshVars.basket(0.0, 0.2);
            freshBasket->setLossModel((freshVars.*model.second)());
            BOOST_CHECK_SMALL(after - freshVars.cdo(freshBasket)->fairPremium(), 1.0e-12);
            BOOST_CHECK_GT(std::fabs(after - before), 1.0e-4);
        }
    }
}

BOOST_AUTO_TEST_CASE(testEveryLossModelOnTwoTranches)
{
    BOOST_TEST_MESSAGE("Testing two tranches on one loss model, for every model...");

    const Real tolerance = 1.0e-12;

    for (const auto& model : everyLossModel())
    {
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars vars(0.3);
            Settings::instance().evaluationDate() = vars.today;
            auto first = vars.basket(0.0, 0.1);
            auto second = vars.basket(0.1, 0.3);
            auto shared = (vars.*model.factory)();
            first->setLossModel(shared);
            second->setLossModel(shared);
            auto firstTranche = vars.cdo(first);
            auto secondTranche = vars.cdo(second);

            const Real firstPremium = premiumOnNewModel(model.factory, 0.3, 0.01, 0.0, 0.1);
            const Real secondPremium = premiumOnNewModel(model.factory, 0.3, 0.01, 0.1, 0.3);
            BOOST_CHECK_SMALL(firstTranche->fairPremium() - firstPremium, tolerance);
            BOOST_CHECK_SMALL(secondTranche->fairPremium() - secondPremium, tolerance);

            // The second tranche has the model now, so the first has to take it back.
            firstTranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(vars.discountCurve));
            BOOST_CHECK_SMALL(firstTranche->fairPremium() - firstPremium, tolerance);
            secondTranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(vars.discountCurve));
            BOOST_CHECK_SMALL(secondTranche->fairPremium() - secondPremium, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCorrelationMoveOnModelSharedByTwoTranches)
{
    BOOST_TEST_MESSAGE("Testing a move in the correlation on two tranches sharing a loss model...");

    for (const auto& model : everyLossModel())
    {
        if (!model.followsCorrelationQuote)
            continue;
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars vars(0.3);
            Settings::instance().evaluationDate() = vars.today;
            auto first = vars.basket(0.0, 0.1);
            auto second = vars.basket(0.1, 0.3);
            auto shared = (vars.*model.factory)();
            first->setLossModel(shared);
            second->setLossModel(shared);
            auto firstTranche = vars.cdo(first);
            auto secondTranche = vars.cdo(second);
            firstTranche->fairPremium();
            secondTranche->fairPremium();

            vars.correlation->setValue(0.6);

            BOOST_CHECK_SMALL(firstTranche->fairPremium() - premiumOnNewModel(model.factory, 0.6, 0.01, 0.0, 0.1),
                              1.0e-12);
            BOOST_CHECK_SMALL(secondTranche->fairPremium() - premiumOnNewModel(model.factory, 0.6, 0.01, 0.1, 0.3),
                              1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testEveryLossModelOnTwoPools)
{
    BOOST_TEST_MESSAGE("Testing baskets on two pools sharing a loss model, for every model...");

    const Real tolerance = 1.0e-12;

    for (const auto& model : everyLossModel())
    {
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars safer(0.3, Date(), 0.01);
            SharedModelVars riskier(0.3, Date(), 0.03);
            Settings::instance().evaluationDate() = safer.today;
            auto saferBasket = safer.basket(0.0, 0.2);
            auto riskierBasket = riskier.basket(0.0, 0.2);
            auto shared = (safer.*model.factory)();
            saferBasket->setLossModel(shared);
            riskierBasket->setLossModel(shared);
            auto saferTranche = safer.cdo(saferBasket);
            auto riskierTranche = riskier.cdo(riskierBasket);

            const Real saferPremium = premiumOnNewModel(model.factory, 0.3, 0.01, 0.0, 0.2);
            BOOST_CHECK_SMALL(saferTranche->fairPremium() - saferPremium, tolerance);
            BOOST_CHECK_SMALL(riskierTranche->fairPremium() - premiumOnNewModel(model.factory, 0.3, 0.03, 0.0, 0.2),
                              tolerance);

            saferTranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(safer.discountCurve));
            BOOST_CHECK_SMALL(saferTranche->fairPremium() - saferPremium, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testEveryLossModelOnTwoSetsOfNotionals)
{
    BOOST_TEST_MESSAGE("Testing baskets with different notionals sharing a loss model, for every model...");

    std::vector<Real> notionals;
    for (Size i = 0; i < SharedModelVars::poolSize; ++i)
        notionals.push_back(50.0 + 15.0 * i);

    for (const auto& model : everyLossModel())
    {
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars vars(0.3);
            Settings::instance().evaluationDate() = vars.today;
            auto equal = vars.basket(0.0, 0.2);
            auto different = vars.basket(0.0, 0.2, notionals);
            auto shared = (vars.*model.factory)();
            equal->setLossModel(shared);
            different->setLossModel(shared);
            vars.cdo(equal)->fairPremium();
            const Real premium = vars.cdo(different)->fairPremium();

            SharedModelVars freshVars(0.3);
            auto freshBasket = freshVars.basket(0.0, 0.2, notionals);
            freshBasket->setLossModel((freshVars.*model.factory)());
            BOOST_CHECK_SMALL(premium - freshVars.cdo(freshBasket)->fairPremium(), 1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBasketRefusedByASharedModel)
{
    BOOST_TEST_MESSAGE("Testing that a model refusing a basket leaves the other baskets to take it back...");

    SharedModelVars vars(0.3);
    Settings::instance().evaluationDate() = vars.today;
    auto basket = vars.basket(0.0, 0.2);
    auto shared = vars.binomialModel();
    basket->setLossModel(shared);
    auto tranche = vars.cdo(basket);
    const Real premium = tranche->fairPremium();

    auto smallerPool = ext::make_shared<Pool>();
    const std::vector<std::string> smallerNames(vars.names.begin(), vars.names.begin() + 10);
    for (const auto& name : smallerNames)
        smallerPool->add(name, vars.pool->get(name), vars.pool->defaultKey(name));
    auto smaller =
        ext::make_shared<Basket>(vars.today, smallerNames, std::vector<Real>(10, 100.0), smallerPool, 0.0, 0.2);
    smaller->setLossModel(shared);
    BOOST_CHECK_EXCEPTION(smaller->expectedTrancheLoss(vars.schedule.dates().back()), Error,
                          ExpectedErrorMessage("Incompatible new basket and model sizes"));

    tranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(vars.discountCurve));
    BOOST_CHECK_SMALL(tranche->fairPremium() - premium, 1.0e-12);
}

BOOST_AUTO_TEST_CASE(testCorrelationMoveOnOneBasket)
{
    BOOST_TEST_MESSAGE("Testing that a move in the correlation reaches a tranche, for every model...");

    for (const auto& model : everyLossModel())
    {
        if (!model.followsCorrelationQuote)
            continue;
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars vars(0.3);
            Settings::instance().evaluationDate() = vars.today;
            auto basket = vars.basket(0.0, 0.2);
            basket->setLossModel((vars.*model.factory)());
            auto tranche = vars.cdo(basket);
            const Real before = tranche->fairPremium();

            Flag flag;
            flag.registerWith(tranche);
            vars.correlation->setValue(0.6);

            BOOST_CHECK(flag.isUp());
            const Real after = tranche->fairPremium();
            BOOST_CHECK_SMALL(after - premiumOnNewModel(model.factory, 0.6, 0.01, 0.0, 0.2), 1.0e-12);
            BOOST_CHECK_GT(std::fabs(after - before), 1.0e-4);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCorrelationMoveAfterABasketHasGone)
{
    BOOST_TEST_MESSAGE("Testing a move in the correlation when the basket a loss model last held has gone...");

    // Reading the basket that has gone would show under a sanitizer, not in the values.
    for (const auto& model : everyLossModel())
    {
        if (!model.followsCorrelationQuote)
            continue;
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars vars(0.3);
            Settings::instance().evaluationDate() = vars.today;
            auto staying = vars.basket(0.0, 0.2);
            auto shared = (vars.*model.factory)();
            staying->setLossModel(shared);
            auto tranche = vars.cdo(staying);
            tranche->fairPremium();
            {
                auto going = vars.basket(0.1, 0.3);
                going->setLossModel(shared);
                vars.cdo(going)->fairPremium();
            }

            vars.correlation->setValue(0.6);

            BOOST_CHECK_SMALL(tranche->fairPremium() - premiumOnNewModel(model.factory, 0.6, 0.01, 0.0, 0.2), 1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBaseCorrelationModelBeforeItsFirstBasket)
{
    BOOST_TEST_MESSAGE("Testing a base-correlation model told of a move before any basket has taken it...");

    SharedModelVars vars(0.3);
    Settings::instance().evaluationDate() = vars.today;
    auto model = vars.baseCorrelationModel();

    BOOST_CHECK_NO_THROW(vars.correlation->setValue(0.6));

    auto basket = vars.basket(0.0, 0.2);
    basket->setLossModel(model);
    BOOST_CHECK_SMALL(vars.cdo(basket)->fairPremium() -
                          premiumOnNewModel(&SharedModelVars::baseCorrelationModel, 0.6, 0.01, 0.0, 0.2),
                      1.0e-12);
}

BOOST_AUTO_TEST_CASE(testHazardMoveOnEveryLossModel)
{
    BOOST_TEST_MESSAGE("Testing a move in the credit curves on every loss model...");

    for (const auto& model : everyLossModel())
    {
        for (Real hazard : {0.03, 0.004})
        {
            BOOST_TEST_CONTEXT(model.name << " moving to a hazard rate of " << hazard)
            {
                SharedModelVars vars(0.3);
                Settings::instance().evaluationDate() = vars.today;
                auto basket = vars.basket(0.0, 0.2);
                basket->setLossModel((vars.*model.factory)());
                auto tranche = vars.cdo(basket);
                const Real before = tranche->fairPremium();

                vars.moveHazard(hazard);

                const Real after = tranche->fairPremium();
                BOOST_CHECK_SMALL(after - premiumOnNewModel(model.factory, 0.3, hazard, 0.0, 0.2), 1.0e-12);
                BOOST_CHECK_GT(std::fabs(after - before), 1.0e-4);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testHazardMoveOfOneNameOnEveryLossModel)
{
    BOOST_TEST_MESSAGE("Testing a move in the credit curve of one name on every loss model...");

    for (const auto& model : everyLossModel())
    {
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars vars(0.3);
            Settings::instance().evaluationDate() = vars.today;
            auto basket = vars.basket(0.0, 0.2);
            basket->setLossModel((vars.*model.factory)());
            auto tranche = vars.cdo(basket);
            const Real before = tranche->fairPremium();

            vars.lastNameHazard->setValue(0.2);

            SharedModelVars freshVars(0.3);
            freshVars.lastNameHazard->setValue(0.2);
            auto freshBasket = freshVars.basket(0.0, 0.2);
            freshBasket->setLossModel((freshVars.*model.factory)());

            const Real after = tranche->fairPremium();
            BOOST_CHECK_SMALL(after - freshVars.cdo(freshBasket)->fairPremium(), 1.0e-12);
            BOOST_CHECK_GT(std::fabs(after - before), 1.0e-4);
        }
    }
}

BOOST_AUTO_TEST_CASE(testHazardMoveOfAPoolALossModelHasLeft)
{
    BOOST_TEST_MESSAGE("Testing a move in the credit curves of a pool a loss model has left...");

    for (const auto& model : everyLossModel())
    {
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars left(0.3, Date(), 0.01);
            SharedModelVars holding(0.3, Date(), 0.03);
            Settings::instance().evaluationDate() = left.today;
            auto shared = (left.*model.factory)();
            auto leftBasket = left.basket(0.0, 0.2);
            auto holdingBasket = holding.basket(0.0, 0.2);
            leftBasket->setLossModel(shared);
            holdingBasket->setLossModel(shared);
            auto leftTranche = left.cdo(leftBasket);
            auto holdingTranche = holding.cdo(holdingBasket);
            leftTranche->fairPremium();
            const Real held = holdingTranche->fairPremium();

            left.moveHazard(0.004);

            BOOST_CHECK_SMALL(holdingTranche->fairPremium() - held, 1.0e-12);
            BOOST_CHECK_SMALL(leftTranche->fairPremium() - premiumOnNewModel(model.factory, 0.3, 0.004, 0.0, 0.2),
                              1.0e-12);
            BOOST_CHECK_SMALL(holdingTranche->fairPremium() - held, 1.0e-12);
        }
    }
}

// Twenty names, or the nineteen left once name 3 has defaulted, as a pool on its own. Name 3 can
// also be joined by all the others, or not default at all. They differ in what a model reads per
// name, so that reading one name's data for another's shows. The homogeneous models need every
// name to lose the same on default.
struct NamesAroundADefault
{
    static constexpr Size poolSize = 20;
    static constexpr Size defaultedName = 3;

    enum Defaults
    {
        oneName,
        everyName,
        noName
    };

    Date inception{31, August, 2006};
    Date beforeTheDefault{1, September, 2006};
    Date defaultDate{15, September, 2006};
    Date afterTheDefault{1, November, 2006};
    Date horizon{1, September, 2011};

    Handle<Quote> correlation;
    ext::shared_ptr<Pool> pool;
    std::vector<std::string> names;
    std::vector<Real> notionals, hazards, recoveries;
    std::vector<std::vector<Real>> factors;

    // Under twoKeys every third name is on a key of its own, with a far-off curve under the other one.
    // Names here don't sort as they're added: "issuer-10" comes before "issuer-2".
    NamesAroundADefault(bool sameLossOnDefault, bool onlyWhatIsLeft, Defaults defaults = oneName, bool twoKeys = false)
    : correlation(ext::make_shared<SimpleQuote>(0.3)), pool(ext::make_shared<Pool>())
    {
        const NorthAmericaCorpDefaultKey curveKey(EURCurrency(), SeniorSec, Period(0, Weeks), 10.);
        const NorthAmericaCorpDefaultKey poolKey(EURCurrency(), SeniorSec, Period(), 1.);
        const NorthAmericaCorpDefaultKey otherCurveKey(EURCurrency(), SubUpperTier2, Period(0, Weeks), 10.);
        const NorthAmericaCorpDefaultKey otherPoolKey(EURCurrency(), SubUpperTier2, Period(), 1.);
        for (Size i = 0; i < poolSize; ++i)
        {
            if (onlyWhatIsLeft && i == defaultedName)
                continue;
            names.push_back("issuer-" + std::to_string(i));
            notionals.push_back(sameLossOnDefault ? 100.0 : 100.0 + 8.0 * i);
            hazards.push_back(0.004 + 0.002 * i);
            recoveries.push_back(sameLossOnDefault ? 0.4 : 0.2 + 0.025 * i);
            factors.emplace_back(1, std::sqrt(0.05 + 0.04 * i));

            const bool onTheOtherKey = twoKeys && i % 3 == 0;
            std::vector<std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities;
            probabilities.emplace_back(
                onTheOtherKey ? otherCurveKey : curveKey,
                Handle<DefaultProbabilityTermStructure>(ext::make_shared<FlatHazardRate>(
                    inception, makeQuoteHandle(hazards.back()), ActualActual(ActualActual::ISDA))));
            if (twoKeys)
                probabilities.emplace_back(onTheOtherKey ? curveKey : otherCurveKey,
                                           Handle<DefaultProbabilityTermStructure>(ext::make_shared<FlatHazardRate>(
                                               inception, makeQuoteHandle(0.5), ActualActual(ActualActual::ISDA))));
            DefaultEventSet events;
            if ((i == defaultedName && defaults == oneName) || defaults == everyName)
                events.insert(ext::make_shared<FailureToPayEvent>(defaultDate, EURCurrency(),
                                                                  onTheOtherKey ? SubUpperTier2 : SeniorSec, 10.0,
                                                                  defaultDate + 5, recoveries.back()));
            pool->add(names.back(), Issuer(probabilities, events), onTheOtherKey ? otherPoolKey : poolKey);
        }
    }

    ext::shared_ptr<GaussianConstantLossLM> latentModel() const
    {
        return ext::make_shared<GaussianConstantLossLM>(
            factors, recoveries, LatentModelIntegrationType::GaussianQuadrature, GaussianCopulaPolicy::initTraits());
    }

    static TCopulaPolicy::initTraits tTraits()
    {
        TCopulaPolicy::initTraits traits;
        traits.tOrders = std::vector<Integer>(2, 3);
        return traits;
    }

    // Under a T copula each name has a distribution of its own, where the Gaussian one is shared.
    ext::shared_ptr<TConstantLossLM> tLatentModel() const
    {
        return ext::make_shared<TConstantLossLM>(factors, recoveries, LatentModelIntegrationType::GaussianQuadrature,
                                                 tTraits());
    }

    ext::shared_ptr<DefaultLossModel> lhpModel() const
    {
        return ext::make_shared<GaussianLHPLossModel>(correlation, recoveries);
    }

    // With an odd number of buckets, so that no tranche limit used here falls on an edge
    // between two, where the last bit of the limit would decide the bucket.
    ext::shared_ptr<DefaultLossModel> homogeneousModel() const
    {
        return ext::make_shared<HomogGaussPoolLossModel>(latentModel(), 101, 5., -5., 15);
    }

    ext::shared_ptr<DefaultLossModel> inhomogeneousModel() const
    {
        return ext::make_shared<IHGaussPoolLossModel>(latentModel(), 101, 5., -5., 15);
    }

    ext::shared_ptr<DefaultLossModel> binomialModel() const
    {
        return ext::make_shared<GaussianBinomialLossModel>(latentModel());
    }

    ext::shared_ptr<DefaultLossModel> recursiveModel() const
    {
        return ext::make_shared<RecursiveGaussLossModel>(latentModel(), 4);
    }

    ext::shared_ptr<DefaultLossModel> saddlePointModel() const
    {
        return ext::make_shared<SaddlePointLossModel<GaussianCopulaPolicy>>(latentModel());
    }

    ext::shared_ptr<DefaultLossModel> tHomogeneousModel() const
    {
        return ext::make_shared<HomogTPoolLossModel>(tLatentModel(), 101, 5., -5., 15);
    }

    ext::shared_ptr<DefaultLossModel> tInhomogeneousModel() const
    {
        return ext::make_shared<IHStudentPoolLossModel>(tLatentModel(), 101, 5., -5., 15);
    }

    ext::shared_ptr<DefaultLossModel> randomDefaultModel() const
    {
        return ext::make_shared<RandomDefaultLM<GaussianCopulaPolicy>>(latentModel(), 2000);
    }

    ext::shared_ptr<DefaultLossModel> randomLossModel() const
    {
        std::vector<std::vector<Real>> doubled(factors);
        doubled.insert(doubled.end(), factors.begin(), factors.end());
        const auto latent = ext::make_shared<GaussianSpotLossLM>(doubled, recoveries, 2.2,
                                                                 LatentModelIntegrationType::GaussianQuadrature,
                                                                 GaussianCopulaPolicy::initTraits());
        return ext::make_shared<RandomLossLM<GaussianCopulaPolicy>>(latent, 2000, 1.0e-6, 2863311530UL);
    }

    ext::shared_ptr<DefaultLossModel> tBinomialModel() const
    {
        return ext::make_shared<TBinomialLossModel>(tLatentModel());
    }

    ext::shared_ptr<DefaultLossModel> tRecursiveModel() const
    {
        return ext::make_shared<RecursiveLossModel<TCopulaPolicy>>(tLatentModel(), 4);
    }

    ext::shared_ptr<DefaultLossModel> tSaddlePointModel() const
    {
        return ext::make_shared<SaddlePointLossModel<TCopulaPolicy>>(tLatentModel());
    }

    // By default correlation goes up with the loss level, so that the levels a model reads show
    // in what it says.
    template <class Model>
    ext::shared_ptr<DefaultLossModel> baseCorrelationModel(const typename Model::copulaType::initTraits& traits,
                                                           Real atLowLevel = 0.2,
                                                           Real atHighLevel = 0.4) const
    {
        const Handle<Quote> low = makeQuoteHandle(atLowLevel);
        const Handle<Quote> high = makeQuoteHandle(atHighLevel);
        const std::vector<std::vector<Handle<Quote>>> quotes = {{low, low}, {high, high}};
        auto surface = ext::make_shared<BaseCorrelationTermStructure<BilinearInterpolation>>(
            0, TARGET(), Unadjusted, std::vector<Period>{Period(1, Years), Period(10, Years)},
            std::vector<Real>{0.03, 0.5}, quotes, Actual365Fixed());
        surface->enableExtrapolation();
        return ext::make_shared<BaseCorrelationLossModel<Model, BilinearInterpolation>>(
            Handle<BaseCorrelationTermStructure<BilinearInterpolation>>(surface), recoveries, traits);
    }

    ext::shared_ptr<DefaultLossModel> baseCorrelationOnLhp() const
    {
        return baseCorrelationModel<GaussianLHPLossModel>(GaussianCopulaPolicy::initTraits());
    }

    ext::shared_ptr<DefaultLossModel> baseCorrelationOnBinomial() const
    {
        return baseCorrelationModel<GaussianBinomialLossModel>(GaussianCopulaPolicy::initTraits());
    }

    ext::shared_ptr<DefaultLossModel> baseCorrelationOnTBinomial() const
    {
        return baseCorrelationModel<TBinomialLossModel>(tTraits());
    }

    ext::shared_ptr<DefaultLossModel> baseCorrelationOnInhomogeneous() const
    {
        return baseCorrelationModel<IHGaussPoolLossModel>(GaussianCopulaPolicy::initTraits());
    }

    ext::shared_ptr<Basket> basket(Real attachment, Real detachment) const
    {
        return ext::make_shared<Basket>(inception, names, notionals, pool, attachment, detachment);
    }
};

// What a model expects a basket to lose, leaving out what it has lost already.
Real lossOnLiveNames(const Basket& basket, const Date& d)
{
    return basket.expectedTrancheLoss(d) - (basket.trancheNotional() - basket.remainingTrancheNotional());
}

struct ModelAroundADefault
{
    std::string name;
    ext::shared_ptr<DefaultLossModel> (NamesAroundADefault::*factory)() const;
    bool needsTheSameLossOnDefault;
};

std::vector<ModelAroundADefault> modelsAroundADefault()
{
    return {{"LHP", &NamesAroundADefault::lhpModel, false},
            {"homogeneous", &NamesAroundADefault::homogeneousModel, true},
            {"binomial", &NamesAroundADefault::binomialModel, false},
            {"recursive", &NamesAroundADefault::recursiveModel, false},
            {"saddle point", &NamesAroundADefault::saddlePointModel, false},
            {"inhomogeneous", &NamesAroundADefault::inhomogeneousModel, false},
            {"T homogeneous", &NamesAroundADefault::tHomogeneousModel, true},
            {"T binomial", &NamesAroundADefault::tBinomialModel, false},
            {"T recursive", &NamesAroundADefault::tRecursiveModel, false},
            {"T saddle point", &NamesAroundADefault::tSaddlePointModel, false},
            {"T inhomogeneous", &NamesAroundADefault::tInhomogeneousModel, false}};
}

// These have no percentile or shortfall to ask for.
std::vector<ModelAroundADefault> baseCorrelationModels()
{
    return {{"base correlation on LHP", &NamesAroundADefault::baseCorrelationOnLhp, false},
            {"base correlation on binomial", &NamesAroundADefault::baseCorrelationOnBinomial, false},
            {"base correlation on T binomial", &NamesAroundADefault::baseCorrelationOnTBinomial, false},
            {"base correlation on inhomogeneous", &NamesAroundADefault::baseCorrelationOnInhomogeneous, false}};
}

BOOST_AUTO_TEST_CASE(testBasketDataOnTheNamesLeftAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing what a basket says about the names left after a default...");

    const NamesAroundADefault all(false, false);
    const NamesAroundADefault left(false, true);
    Settings::instance().evaluationDate() = all.afterTheDefault;

    const auto basket = all.basket(0.0, 0.1);
    const std::vector<Real> notionals = basket->remainingNotionals(all.afterTheDefault);
    const std::vector<Probability> probabilities = basket->remainingProbabilities(all.horizon);

    BOOST_REQUIRE_EQUAL(notionals.size(), left.notionals.size());
    BOOST_REQUIRE_EQUAL(probabilities.size(), left.notionals.size());
    for (Size i = 0; i < notionals.size(); ++i)
    {
        BOOST_TEST_CONTEXT("name " << left.names[i])
        {
            BOOST_CHECK_EQUAL(basket->remainingNames(all.afterTheDefault)[i], left.names[i]);
            BOOST_CHECK_EQUAL(basket->remainingNotionals()[i], left.notionals[i]);
            BOOST_CHECK_EQUAL(notionals[i], left.notionals[i]);
            const Probability expected = left.pool->get(left.names[i])
                                             .defaultProbability(left.pool->defaultKey(left.names[i]))
                                             ->defaultProbability(all.horizon, true);
            BOOST_CHECK_EQUAL(probabilities[i], expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLossModelsOnTheNamesLeftAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing loss models on the names left after a default...");

    for (const auto& model : modelsAroundADefault())
    {
        for (bool sameLoss : {true, false})
        {
            if (model.needsTheSameLossOnDefault && !sameLoss)
                continue;
            for (auto tranche : {std::make_pair(0.0, 0.1), std::make_pair(0.1, 0.3)})
            {
                BOOST_TEST_CONTEXT(model.name << (sameLoss ? ", the same loss on default" : ", different names")
                                              << ", tranche " << tranche.first << " to " << tranche.second)
                {
                    const NamesAroundADefault all(sameLoss, false);
                    const NamesAroundADefault left(sameLoss, true);
                    Settings::instance().evaluationDate() = all.afterTheDefault;

                    const auto basket = all.basket(tranche.first, tranche.second);
                    const Date today = all.afterTheDefault;
                    const Real notional = basket->remainingNotional(today);
                    const auto reference = left.basket(basket->remainingAttachmentAmount(today) / notional,
                                                       basket->remainingDetachmentAmount(today) / notional);

                    basket->setLossModel((all.*model.factory)());
                    reference->setLossModel((left.*model.factory)());

                    const Real tolerance = 1.0e-9;
                    BOOST_CHECK_SMALL(lossOnLiveNames(*basket, all.horizon) - lossOnLiveNames(*reference, all.horizon),
                                      tolerance);
                    // What an engine asks for a coupon that started before the default.
                    BOOST_CHECK_SMALL(lossOnLiveNames(*basket, all.beforeTheDefault) -
                                          lossOnLiveNames(*reference, all.beforeTheDefault),
                                      tolerance);
                    BOOST_CHECK_SMALL(basket->percentile(all.horizon, 0.95) - reference->percentile(all.horizon, 0.95),
                                      tolerance);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testLossModelsBeforeADefaultStillToCome)
{
    BOOST_TEST_MESSAGE("Testing loss models with a default dated after the evaluation date...");

    // A failure to pay doesn't match a key until it has happened, so a bankruptcy shows this.
    const Date defaultDate(15, September, 2006);

    for (const auto& model : everyLossModel())
    {
        BOOST_TEST_CONTEXT(model.name)
        {
            SharedModelVars programmed(0.3, defaultDate, 0.01, true);
            SharedModelVars none(0.3);
            Settings::instance().evaluationDate() = none.today;

            auto withEvent = programmed.basket(0.0, 0.2);
            withEvent->setLossModel((programmed.*model.factory)());
            auto withoutEvent = none.basket(0.0, 0.2);
            withoutEvent->setLossModel((none.*model.factory)());

            BOOST_CHECK_EQUAL(withEvent->remainingSize(programmed.schedule.endDate()), Size(19));
            BOOST_CHECK_SMALL(programmed.cdo(withEvent)->fairPremium() - none.cdo(withoutEvent)->fairPremium(),
                              1.0e-12);
        }
    }

    using Factory = ext::shared_ptr<DefaultLossModel> (SharedModelVars::*)() const;
    const std::vector<std::pair<std::string, Factory>> withStatistics = {
        {"LHP", &SharedModelVars::lhpModel},
        {"homogeneous", &SharedModelVars::homogeneousModel},
        {"inhomogeneous", &SharedModelVars::inhomogeneousModel},
        {"binomial", &SharedModelVars::binomialModel},
        {"recursive", &SharedModelVars::recursiveModel},
        {"saddle point", &SharedModelVars::saddlePointModel}};
    for (const auto& model : withStatistics)
    {
        BOOST_TEST_CONTEXT(model.first)
        {
            SharedModelVars programmed(0.3, defaultDate, 0.01, true);
            SharedModelVars none(0.3);
            Settings::instance().evaluationDate() = none.today;

            auto withEvent = programmed.basket(0.0, 0.2);
            withEvent->setLossModel((programmed.*model.second)());
            auto withoutEvent = none.basket(0.0, 0.2);
            withoutEvent->setLossModel((none.*model.second)());

            const Date horizon = programmed.schedule.endDate();
            BOOST_CHECK_SMALL(withEvent->percentile(horizon, 0.95) - withoutEvent->percentile(horizon, 0.95), 1.0e-9);
            BOOST_CHECK_SMALL(
                withEvent->expectedShortfall(horizon, 0.95) - withoutEvent->expectedShortfall(horizon, 0.95), 1.0e-9);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLhpProbabilityOverALossBeforeADefaultStillToCome)
{
    BOOST_TEST_MESSAGE("Testing the LHP model's probability of a loss with a default still to come...");

    SharedModelVars programmed(0.3, Date(15, September, 2006), 0.01, true);
    SharedModelVars none(0.3);
    Settings::instance().evaluationDate() = none.today;

    auto withEvent = programmed.basket(0.0, 0.2);
    withEvent->setLossModel(programmed.lhpModel());
    auto withoutEvent = none.basket(0.0, 0.2);
    withoutEvent->setLossModel(none.lhpModel());

    const Date horizon = programmed.schedule.endDate();
    BOOST_CHECK_SMALL(withEvent->probOverLoss(horizon, 0.5) - withoutEvent->probOverLoss(horizon, 0.5), 1.0e-12);
}

// A tranche of NamesAroundADefault's basket, with what the default leaves it worked out by
// hand. Name 3 of 20 costs 100 * (1 - 0.4) = 60 of 2000 with the same loss on default, and
// 124 * (1 - 0.275) = 89.9 of 3520 with different names.
struct TrancheAfterADefault
{
    Real attachment, detachment;
    Real remainingAttachment, remainingDetachment, realized;
};

std::vector<TrancheAfterADefault> tranchesAfterADefault(bool sameLossOnDefault)
{
    if (sameLossOnDefault)
        return {{0.0, 0.02, 0.0, 0.0, 40.0},   // wiped out
                {0.0, 0.1, 0.0, 140.0, 60.0},  // the first to lose
                {0.02, 0.1, 0.0, 140.0, 20.0}, // the default reaches into it
                {0.05, 0.1, 40.0, 140.0, 0.0}, // the default falls short of it
                {0.1, 0.3, 140.0, 540.0, 0.0}};
    return {{0.0, 0.02, 0.0, 0.0, 70.4},
            {0.0, 0.1, 0.0, 262.1, 89.9},
            {0.02, 0.1, 0.0, 262.1, 19.5},
            {0.05, 0.1, 86.1, 262.1, 0.0},
            {0.1, 0.3, 262.1, 966.1, 0.0}};
}

// The tranche as it stands on the names left, with no default among them. Null if it's wiped out.
ext::shared_ptr<Basket> whatIsLeftOf(const TrancheAfterADefault& tranche, const NamesAroundADefault& left)
{
    if (tranche.remainingDetachment == 0.0)
        return nullptr;
    const Real notional = std::accumulate(left.notionals.begin(), left.notionals.end(), Real(0.0));
    return left.basket(tranche.remainingAttachment / notional, tranche.remainingDetachment / notional);
}

BOOST_AUTO_TEST_CASE(testBasketAmountsAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing the amounts a default leaves a tranche...");

    const Real tolerance = 1.0e-9;
    for (bool sameLoss : {true, false})
    {
        const NamesAroundADefault all(sameLoss, false);
        const Date today = all.afterTheDefault;
        Settings::instance().evaluationDate() = today;
        for (const auto& tranche : tranchesAfterADefault(sameLoss))
        {
            BOOST_TEST_CONTEXT((sameLoss ? "the same loss on default" : "different names")
                               << ", tranche " << tranche.attachment << " to " << tranche.detachment)
            {
                const auto basket = all.basket(tranche.attachment, tranche.detachment);

                // Before anything is calculated, with no model.
                BOOST_CHECK_SMALL(basket->remainingAttachmentAmount() - tranche.remainingAttachment, tolerance);
                BOOST_CHECK_SMALL(basket->remainingDetachmentAmount() - tranche.remainingDetachment, tolerance);

                BOOST_CHECK_SMALL(basket->remainingAttachmentAmount(today) - tranche.remainingAttachment, tolerance);
                BOOST_CHECK_SMALL(basket->remainingDetachmentAmount(today) - tranche.remainingDetachment, tolerance);

                // Nothing was lost before the default.
                BOOST_CHECK_EQUAL(basket->remainingAttachmentAmount(all.beforeTheDefault), basket->attachmentAmount());
                BOOST_CHECK_EQUAL(basket->remainingDetachmentAmount(all.beforeTheDefault), basket->detachmentAmount());

                basket->setLossModel(all.lhpModel());
                BOOST_CHECK_SMALL(basket->remainingTrancheNotional() -
                                      (tranche.remainingDetachment - tranche.remainingAttachment),
                                  tolerance);
                BOOST_CHECK_SMALL(basket->trancheNotional() - basket->remainingTrancheNotional() - tranche.realized,
                                  tolerance);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testExpectedTrancheLossAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing what loss models expect a tranche to lose after a default...");

    const Real tolerance = 1.0e-9;
    for (const auto& model : modelsAroundADefault())
    {
        for (bool sameLoss : {true, false})
        {
            if (model.needsTheSameLossOnDefault && !sameLoss)
                continue;
            for (const auto& tranche : tranchesAfterADefault(sameLoss))
            {
                BOOST_TEST_CONTEXT(model.name << (sameLoss ? ", the same loss on default" : ", different names")
                                              << ", tranche " << tranche.attachment << " to " << tranche.detachment)
                {
                    const NamesAroundADefault all(sameLoss, false);
                    const NamesAroundADefault left(sameLoss, true);
                    Settings::instance().evaluationDate() = all.afterTheDefault;

                    const auto basket = all.basket(tranche.attachment, tranche.detachment);
                    basket->setLossModel((all.*model.factory)());
                    const auto reference = whatIsLeftOf(tranche, left);
                    if (reference != nullptr)
                        reference->setLossModel((left.*model.factory)());

                    // At the second date, as an engine asks for a coupon that started before the default.
                    for (const Date& d : {all.horizon, all.beforeTheDefault})
                    {
                        const Real onNamesLeft = reference != nullptr ? reference->expectedTrancheLoss(d) : 0.0;
                        BOOST_CHECK_SMALL(basket->expectedTrancheLoss(d) - tranche.realized - onNamesLeft, tolerance);
                    }

                    // These are of what the tranche can still lose.
                    const Real percentile = reference != nullptr ? reference->percentile(all.horizon, 0.95) : 0.0;
                    BOOST_CHECK_SMALL(basket->percentile(all.horizon, 0.95) - percentile, tolerance);
                    if (reference == nullptr)
                        BOOST_CHECK_EQUAL(basket->expectedShortfall(all.horizon, 0.95), 0.0);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testProbabilityOverALossAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing the probability of a tranche losing an amount after a default...");

    using Factory = ext::shared_ptr<DefaultLossModel> (NamesAroundADefault::*)() const;
    const std::vector<std::pair<std::string, Factory>> models = {
        {"LHP", &NamesAroundADefault::lhpModel}, {"saddle point", &NamesAroundADefault::saddlePointModel}};
    for (const auto& model : models)
    {
        for (bool sameLoss : {true, false})
        {
            const Real settledLoss = sameLoss ? 60.0 : 89.9;
            for (const auto& tranche : tranchesAfterADefault(sameLoss))
            {
                for (Real fraction : {0.0, 0.1, 0.25, 0.5, 0.75, 1.0})
                {
                    BOOST_TEST_CONTEXT(model.first << (sameLoss ? ", the same loss on default" : ", different names")
                                                   << ", tranche " << tranche.attachment << " to " << tranche.detachment
                                                   << ", a fraction " << fraction << " of it")
                    {
                        const NamesAroundADefault all(sameLoss, false);
                        const NamesAroundADefault left(sameLoss, true);
                        Settings::instance().evaluationDate() = all.afterTheDefault;

                        const auto basket = all.basket(tranche.attachment, tranche.detachment);
                        basket->setLossModel((all.*model.second)());

                        const Real level = basket->attachmentAmount() +
                                           fraction * (basket->detachmentAmount() - basket->attachmentAmount());
                        Probability expected = 1.0;
                        if (level > settledLoss)
                        {
                            const auto reference = whatIsLeftOf(tranche, left);
                            reference->setLossModel((left.*model.second)());
                            expected = reference->probOverLoss(
                                all.horizon, (level - settledLoss - tranche.remainingAttachment) /
                                                 (tranche.remainingDetachment - tranche.remainingAttachment));
                        }
                        BOOST_CHECK_SMALL(basket->probOverLoss(all.horizon, fraction) - expected, 1.0e-9);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testSaddlePointProbabilityOverALossOnNamesThatDiffer)
{
    BOOST_TEST_MESSAGE("Testing the saddle point's probability of a loss on names that differ...");

    // Before #160 the saddle point's search was bracketed by bounds worked out from the name
    // with the largest loss alone, which aren't bounds where names differ: on the 19 names left
    // after a default, which lose 80 to 99 each, the probability of a loss on 0 to 10 % came
    // out at up to 1.88e7, and the percentile at 0.3 at 87.45. Up to the smallest loss, the
    // loss reaches a level if any name defaults, so the probability is exact there.
    const NamesAroundADefault left(false, true);
    Settings::instance().evaluationDate() = left.afterTheDefault;
    const auto basket = left.basket(0.0, 0.1);
    basket->setLossModel(left.saddlePointModel());

    const auto latent = left.latentModel();
    std::vector<Real> inverse = basket->remainingProbabilities(left.horizon);
    for (Size i = 0; i < inverse.size(); ++i)
        inverse[i] = latent->inverseCumulativeY(inverse[i], i);
    const Probability anyLoss = latent->integratedExpectedValue(
        [&](const std::vector<Real>& factor)
        {
            Probability none = 1.0;
            for (Size i = 0; i < inverse.size(); ++i)
                none *= 1.0 - latent->conditionalDefaultProbabilityInvP(inverse[i], i, factor);
            return 1.0 - none;
        });
    Real smallestLoss = QL_MAX_REAL;
    for (Size i = 0; i < left.notionals.size(); ++i)
        smallestLoss = std::min(smallestLoss, left.notionals[i] * (1.0 - left.recoveries[i]));

    Probability previous = 1.0;
    for (Real fraction : {0.0025, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 1.0})
    {
        BOOST_TEST_CONTEXT("a fraction " << fraction << " of the tranche")
        {
            const Probability probability = basket->probOverLoss(left.horizon, fraction);
            BOOST_CHECK_GE(probability, 0.0);
            BOOST_CHECK_LE(probability, previous);
            if (fraction * basket->remainingDetachmentAmount() <= smallestLoss)
                BOOST_CHECK_CLOSE_FRACTION(probability, anyLoss, 1.0e-12);
            previous = probability;
        }
    }
    // the probability of no loss is 1 - anyLoss, about 0.45
    BOOST_CHECK_EQUAL(basket->percentile(left.horizon, 0.3), 0.0);
}

BOOST_AUTO_TEST_CASE(testCdoAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing a CDO tranche after a default...");

    const Date firstCouponStillToPay(1, November, 2006);
    const Date firstCouponPaid(15, January, 2007);
    const Real tolerance = 1.0e-8;

    const Schedule schedule = MakeSchedule()
                                  .from(Date(1, September, 2006))
                                  .to(Date(1, September, 2011))
                                  .withFrequency(Quarterly)
                                  .withCalendar(TARGET())
                                  .withConvention(Following)
                                  .withRule(DateGeneration::Backward);
    const Handle<YieldTermStructure> discountCurve(
        ext::make_shared<FlatForward>(Date(31, August, 2006), 0.05, Actual360(), Continuous));

    using Factory = ext::shared_ptr<DefaultLossModel> (NamesAroundADefault::*)() const;
    const std::vector<std::pair<std::string, Factory>> models = {
        {"LHP", &NamesAroundADefault::lhpModel},
        {"binomial", &NamesAroundADefault::binomialModel},
        {"base correlation", &NamesAroundADefault::baseCorrelationOnLhp}};
    const auto price = [&](const ext::shared_ptr<Basket>& basket, const ext::shared_ptr<PricingEngine>& engine)
    {
        auto tranche =
            ext::make_shared<SyntheticCDO>(basket, Protection::Seller, schedule, 0.02, 0.05, Actual360(), Following);
        tranche->setPricingEngine(engine);
        return tranche;
    };

    for (const auto& model : models)
    {
        for (bool sameLoss : {true, false})
        {
            for (const auto& tranche : tranchesAfterADefault(sameLoss))
            {
                for (const Date& today : {firstCouponStillToPay, firstCouponPaid})
                {
                    for (bool midPoint : {true, false})
                    {
                        BOOST_TEST_CONTEXT(model.first
                                           << (sameLoss ? ", the same loss on default" : ", different names")
                                           << ", tranche " << tranche.attachment << " to " << tranche.detachment
                                           << ", today " << today << (midPoint ? ", mid-point" : ", integral")
                                           << " engine")
                        {
                            const NamesAroundADefault all(sameLoss, false);
                            const NamesAroundADefault left(sameLoss, true);
                            Settings::instance().evaluationDate() = today;

                            ext::shared_ptr<PricingEngine> engine;
                            if (midPoint)
                                engine = ext::make_shared<MidPointCDOEngine>(discountCurve);
                            else
                                engine = ext::make_shared<IntegralCDOEngine>(discountCurve);

                            const auto basket = all.basket(tranche.attachment, tranche.detachment);
                            basket->setLossModel((all.*model.second)());
                            const auto cdo = price(basket, engine);

                            BOOST_CHECK_SMALL(cdo->remainingNotional() -
                                                  (tranche.remainingDetachment - tranche.remainingAttachment),
                                              tolerance);
                            if (today == firstCouponPaid)
                                BOOST_CHECK_SMALL(cdo->expectedTrancheLoss().front() - tranche.realized, tolerance);

                            const auto reference = whatIsLeftOf(tranche, left);
                            if (reference == nullptr)
                            {
                                BOOST_CHECK_EQUAL(cdo->premiumValue(), 0.0);
                                BOOST_CHECK_EQUAL(cdo->protectionValue(), 0.0);
                                BOOST_CHECK_EQUAL(cdo->NPV(), 0.0);
                                BOOST_CHECK_EXCEPTION(cdo->fairUpfrontPremium(), Error,
                                                      ExpectedErrorMessage("no remaining notional"));
                            }
                            else
                            {
                                reference->setLossModel((left.*model.second)());
                                const auto expected = price(reference, engine);
                                BOOST_CHECK_SMALL(cdo->premiumValue() - expected->premiumValue(), tolerance);
                                BOOST_CHECK_SMALL(cdo->protectionValue() - expected->protectionValue(), tolerance);
                                BOOST_CHECK_SMALL(cdo->NPV() - expected->NPV(), tolerance);
                                BOOST_CHECK_SMALL(cdo->fairPremium() - expected->fairPremium(), tolerance);
                                BOOST_CHECK_SMALL(cdo->fairUpfrontPremium() - expected->fairUpfrontPremium(),
                                                  tolerance);
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testUpfront)
{
    BOOST_TEST_MESSAGE("Testing a CDO tranche's upfront...");

    // The engines take the upfront as paid on the first coupon's accrual start, on the tranche's
    // remaining notional, and count it until that coupon is paid, unless it falls before the
    // discount curve's reference date. Before #206, inside the first coupon on a discount curve
    // as of today, they discounted the upfront from before the curve's reference date, and
    // threw. They still throw there on default curves as of today (#228).
    const Date accrualStart(1, September, 2006);
    const Date insideTheFirstCoupon(1, November, 2006);
    const Date afterTheFirstPayment(4, December, 2006);
    const Rate upfront = 0.02;
    const NamesAroundADefault names(true, false, NamesAroundADefault::noName);

    const Schedule schedule = MakeSchedule()
                                  .from(accrualStart)
                                  .to(names.horizon)
                                  .withFrequency(Quarterly)
                                  .withCalendar(TARGET())
                                  .withConvention(Following)
                                  .withRule(DateGeneration::Backward);
    const auto curveFrom = [](const Date& referenceDate)
    { return Handle<YieldTermStructure>(ext::make_shared<FlatForward>(referenceDate, 0.05, Actual360(), Continuous)); };
    const auto price = [&](Rate upfrontRate, const Handle<YieldTermStructure>& discountCurve, bool midPoint)
    {
        const auto basket = names.basket(0.0, 0.1);
        basket->setLossModel(names.lhpModel());
        auto tranche = ext::make_shared<SyntheticCDO>(basket, Protection::Seller, schedule, upfrontRate, 0.05,
                                                      Actual360(), Following);
        if (midPoint)
            tranche->setPricingEngine(ext::make_shared<MidPointCDOEngine>(discountCurve));
        else
            tranche->setPricingEngine(ext::make_shared<IntegralCDOEngine>(discountCurve));
        return tranche;
    };

    struct Valuation
    {
        Date today, curveDate;
        bool upfrontCounts;
    };
    // Inside the first coupon, on a curve from inception, the upfront still counts though it has
    // been paid: #206 changed only what threw (#227).
    const Valuation valuations[] = {
        {names.inception, names.inception, true},       {accrualStart, accrualStart, true},
        {insideTheFirstCoupon, names.inception, true},  {insideTheFirstCoupon, insideTheFirstCoupon, false},
        {afterTheFirstPayment, names.inception, false}, {afterTheFirstPayment, afterTheFirstPayment, false}};
    for (const auto& valuation : valuations)
    {
        for (bool midPoint : {true, false})
        {
            BOOST_TEST_CONTEXT("today " << valuation.today << ", curve from " << valuation.curveDate
                                        << (midPoint ? ", mid-point" : ", integral") << " engine")
            {
                Settings::instance().evaluationDate() = valuation.today;
                const auto curve = curveFrom(valuation.curveDate);
                const auto with = price(upfront, curve, midPoint);
                const auto without = price(0.0, curve, midPoint);
                if (valuation.upfrontCounts)
                    BOOST_CHECK_CLOSE_FRACTION(with->NPV() - without->NPV(),
                                               200.0 * upfront * curve->discount(accrualStart), 1.0e-12);
                else
                    BOOST_CHECK_EQUAL(with->NPV(), without->NPV());
            }
        }
    }

    // Inside the first coupon, the premium on a discount curve as of today is the one on a curve
    // from inception, carried forward to today, and so is all of the integral engine's value, which
    // takes the same dates on either curve; the mid-point engine takes its first period's
    // losses as of the middle of what's left of it after the curve's reference date.
    Settings::instance().evaluationDate() = insideTheFirstCoupon;
    const auto fromInception = curveFrom(names.inception);
    const auto asOfToday = curveFrom(insideTheFirstCoupon);
    for (bool midPoint : {true, false})
    {
        BOOST_TEST_CONTEXT((midPoint ? "mid-point" : "integral") << " engine")
        {
            const auto today = price(upfront, asOfToday, midPoint);
            const auto reference = price(0.0, fromInception, midPoint);
            const DiscountFactor carry = fromInception->discount(insideTheFirstCoupon);
            BOOST_CHECK_CLOSE_FRACTION(today->premiumValue(), reference->premiumValue() / carry, 1.0e-12);
            if (!midPoint)
                BOOST_CHECK_CLOSE_FRACTION(today->NPV(), reference->NPV() / carry, 1.0e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testTopTrancheAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing a tranche reaching beyond what's left of the basket after a default...");

    using Factory = ext::shared_ptr<DefaultLossModel> (NamesAroundADefault::*)() const;
    const std::vector<std::pair<std::string, Factory>> models = {
        {"LHP", &NamesAroundADefault::lhpModel},
        {"binomial", &NamesAroundADefault::binomialModel},
        {"recursive", &NamesAroundADefault::recursiveModel},
        {"saddle point", &NamesAroundADefault::saddlePointModel},
        {"base correlation on LHP", &NamesAroundADefault::baseCorrelationOnLhp},
        {"base correlation on binomial", &NamesAroundADefault::baseCorrelationOnBinomial},
        {"base correlation on T binomial", &NamesAroundADefault::baseCorrelationOnTBinomial},
        {"base correlation on inhomogeneous", &NamesAroundADefault::baseCorrelationOnInhomogeneous}};
    for (const auto& model : models)
    {
        BOOST_TEST_CONTEXT(model.first)
        {
            const NamesAroundADefault all(true, false);
            const NamesAroundADefault left(true, true);
            Settings::instance().evaluationDate() = all.afterTheDefault;

            // 600 to 2000, less the 60 lost, on the 1900 left. No loss on those can reach 1900.
            const auto basket = all.basket(0.3, 1.0);
            BOOST_CHECK_SMALL(basket->remainingAttachmentAmount() - 540.0, 1.0e-9);
            BOOST_CHECK_SMALL(basket->remainingDetachmentAmount() - 1940.0, 1.0e-9);
            basket->setLossModel((all.*model.second)());
            const auto reference = left.basket(540.0 / 1900.0, 1.0);
            reference->setLossModel((left.*model.second)());

            BOOST_CHECK_SMALL(basket->expectedTrancheLoss(all.horizon) - reference->expectedTrancheLoss(all.horizon),
                              1.0e-9);

            // 1980 to 2000, less the 60, starts above the 1900 left, so it can't lose anything.
            const auto beyond = all.basket(0.99, 1.0);
            BOOST_CHECK_SMALL(beyond->remainingAttachmentAmount() - 1920.0, 1.0e-9);
            beyond->setLossModel((all.*model.second)());
            BOOST_CHECK_SMALL(beyond->expectedTrancheLoss(all.horizon), 1.0e-9);
        }
    }
}

BOOST_AUTO_TEST_CASE(testTopTranchePercentileAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing the percentile of a tranche reaching beyond what's left of the basket...");

    using Factory = ext::shared_ptr<DefaultLossModel> (NamesAroundADefault::*)() const;
    const std::vector<std::pair<std::string, Factory>> models = {
        {"LHP", &NamesAroundADefault::lhpModel},
        {"binomial", &NamesAroundADefault::binomialModel},
        {"recursive", &NamesAroundADefault::recursiveModel},
        {"saddle point", &NamesAroundADefault::saddlePointModel}};
    for (const auto& model : models)
    {
        BOOST_TEST_CONTEXT(model.first)
        {
            const NamesAroundADefault all(true, false);
            const NamesAroundADefault left(true, true);
            Settings::instance().evaluationDate() = all.afterTheDefault;

            // 540 to 1940 on the 1900 left loses what 540 to 1900 does, as in testTopTrancheAfterADefault.
            // The saddle point took the percentile as a fraction of the first and returned it of the
            // second (#159).
            const auto basket = all.basket(0.3, 1.0);
            basket->setLossModel((all.*model.second)());
            const auto reference = left.basket(540.0 / 1900.0, 1.0);
            reference->setLossModel((left.*model.second)());
            for (Real level : {0.999, 0.9999})
            {
                BOOST_TEST_CONTEXT("level " << level)
                {
                    BOOST_CHECK_SMALL(
                        basket->percentile(all.horizon, level) - reference->percentile(all.horizon, level), 1.0e-9);
                    BOOST_CHECK_SMALL(basket->expectedShortfall(all.horizon, level) -
                                          reference->expectedShortfall(all.horizon, level),
                                      1.0e-9);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testTrancheWithNoNamesLeft)
{
    BOOST_TEST_MESSAGE("Testing a tranche once every name in its basket has defaulted...");

    struct NoNamesLeft
    {
        Real attachment, detachment, remaining, realized;
    };
    // Each name has cost 60 of its 100, so the basket has lost 1200 of 2000.
    const Real settledLoss = 1200.0;
    const std::vector<NoNamesLeft> tranches = {
        {0.0, 0.1, 0.0, 200.0}, {0.5, 1.0, 800.0, 200.0}, {0.7, 1.0, 600.0, 0.0}};
    const Real tolerance = 1.0e-9;

    const Schedule schedule = MakeSchedule()
                                  .from(Date(1, September, 2006))
                                  .to(Date(1, September, 2011))
                                  .withFrequency(Quarterly)
                                  .withCalendar(TARGET())
                                  .withConvention(Following)
                                  .withRule(DateGeneration::Backward);
    const Handle<YieldTermStructure> discountCurve(
        ext::make_shared<FlatForward>(Date(31, August, 2006), 0.05, Actual360(), Continuous));

    std::vector<ModelAroundADefault> models = modelsAroundADefault();
    models.push_back({"random default", &NamesAroundADefault::randomDefaultModel, false});
    models.push_back({"random loss", &NamesAroundADefault::randomLossModel, false});
    for (const auto& model : baseCorrelationModels())
        models.push_back(model);
    for (const auto& model : models)
    {
        for (const auto& tranche : tranches)
        {
            BOOST_TEST_CONTEXT(model.name << ", tranche " << tranche.attachment << " to " << tranche.detachment)
            {
                const NamesAroundADefault all(true, false, NamesAroundADefault::everyName);
                const Date today = all.afterTheDefault;
                Settings::instance().evaluationDate() = today;

                const auto basket = all.basket(tranche.attachment, tranche.detachment);
                basket->setLossModel((all.*model.factory)());

                BOOST_CHECK_EQUAL(basket->remainingSize(), Size(0));
                BOOST_CHECK_EQUAL(basket->remainingNotional(), 0.0);
                BOOST_CHECK_SMALL(basket->remainingTrancheNotional() - tranche.remaining, tolerance);
                BOOST_CHECK_SMALL(basket->expectedTrancheLoss(all.horizon) - tranche.realized, tolerance);
                BOOST_CHECK_EQUAL(basket->percentile(all.horizon, 0.95), 0.0);
                BOOST_CHECK_EQUAL(basket->expectedShortfall(all.horizon, 0.95), 0.0);
                for (Real fraction : {0.0, 0.25, 0.5, 0.75, 1.0})
                {
                    const Real level = basket->attachmentAmount() +
                                       fraction * (basket->detachmentAmount() - basket->attachmentAmount());
                    BOOST_CHECK_EQUAL(basket->probOverLoss(all.horizon, fraction), level <= settledLoss ? 1.0 : 0.0);
                }

                Real annuity = 0.0;
                for (Size i = 1; i < schedule.size(); ++i)
                    if (schedule[i] > today)
                        annuity += Actual360().yearFraction(schedule[i - 1], schedule[i]) *
                                   discountCurve->discount(schedule[i]);
                const Real upfront = tranche.remaining * 0.02 * discountCurve->discount(schedule[0]);

                for (bool midPoint : {true, false})
                {
                    ext::shared_ptr<PricingEngine> engine;
                    if (midPoint)
                        engine = ext::make_shared<MidPointCDOEngine>(discountCurve);
                    else
                        engine = ext::make_shared<IntegralCDOEngine>(discountCurve);
                    const auto cdo = ext::make_shared<SyntheticCDO>(basket, Protection::Seller, schedule, 0.02, 0.05,
                                                                    Actual360(), Following);
                    cdo->setPricingEngine(engine);

                    // the integral engine discounts each step of a coupon by itself
                    const Real premiumTolerance = midPoint ? tolerance : 0.01;
                    BOOST_CHECK_SMALL(cdo->remainingNotional() - tranche.remaining, tolerance);
                    BOOST_CHECK_SMALL(cdo->protectionValue(), tolerance);
                    BOOST_CHECK_SMALL(cdo->premiumValue() - tranche.remaining * 0.05 * annuity, premiumTolerance);
                    BOOST_CHECK_SMALL(cdo->NPV() - (tranche.remaining * 0.05 * annuity + upfront), premiumTolerance);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomLossModelsAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing random loss models on what a default leaves of a tranche...");

    using Factory = ext::shared_ptr<DefaultLossModel> (NamesAroundADefault::*)() const;
    const std::vector<std::pair<std::string, Factory>> models = {
        {"random default", &NamesAroundADefault::randomDefaultModel},
        {"random loss", &NamesAroundADefault::randomLossModel}};
    const Real tolerance = 1.0e-9;

    for (const auto& model : models)
    {
        for (bool sameLoss : {true, false})
        {
            for (const auto& tranche : tranchesAfterADefault(sameLoss))
            {
                BOOST_TEST_CONTEXT(model.first << (sameLoss ? ", the same loss on default" : ", different names")
                                               << ", tranche " << tranche.attachment << " to " << tranche.detachment)
                {
                    const NamesAroundADefault all(sameLoss, false);
                    const Date horizon = all.horizon;
                    Settings::instance().evaluationDate() = all.afterTheDefault;

                    const auto basket = all.basket(tranche.attachment, tranche.detachment);
                    basket->setLossModel((all.*model.second)());

                    // Second to default among all twenty is the first among the names left, and
                    // the one gone can't be that.
                    BOOST_CHECK_EQUAL(basket->probsBeingNthEvent(2, horizon)[NamesAroundADefault::defaultedName], 0.0);

                    if (tranche.remainingDetachment <= tranche.remainingAttachment)
                    {
                        BOOST_CHECK_SMALL(basket->expectedTrancheLoss(horizon) - tranche.realized, tolerance);
                        BOOST_CHECK_EQUAL(basket->percentile(horizon, 0.95), 0.0);
                        BOOST_CHECK_EQUAL(basket->expectedShortfall(horizon, 0.95), 0.0);
                        continue;
                    }

                    // The same draws on the same twenty names, with none defaulted and nothing
                    // to lose on the one that did.
                    const NamesAroundADefault noDefault(sameLoss, false, NamesAroundADefault::noName);
                    std::vector<Real> notionals = noDefault.notionals;
                    notionals[NamesAroundADefault::defaultedName] = 0.0;
                    const Real left = std::accumulate(notionals.begin(), notionals.end(), Real(0.0));
                    const auto reference = ext::make_shared<Basket>(noDefault.inception, noDefault.names, notionals,
                                                                    noDefault.pool, tranche.remainingAttachment / left,
                                                                    tranche.remainingDetachment / left);
                    reference->setLossModel((noDefault.*model.second)());

                    BOOST_CHECK_SMALL(basket->expectedTrancheLoss(horizon) - tranche.realized -
                                          reference->expectedTrancheLoss(horizon),
                                      tolerance);
                    BOOST_CHECK_SMALL(basket->percentile(horizon, 0.95) - reference->percentile(horizon, 0.95),
                                      tolerance);
                    BOOST_CHECK_SMALL(basket->expectedShortfall(horizon, 0.95) -
                                          reference->expectedShortfall(horizon, 0.95),
                                      tolerance);

                    const auto distribution = basket->lossDistribution(horizon);
                    const auto expectedDistribution = reference->lossDistribution(horizon);
                    BOOST_REQUIRE_EQUAL(distribution.size(), expectedDistribution.size());
                    auto expectedPoint = expectedDistribution.begin();
                    for (const auto& point : distribution)
                    {
                        BOOST_CHECK_SMALL(point.first - expectedPoint->first, tolerance);
                        BOOST_CHECK_SMALL(point.second - expectedPoint->second, tolerance);
                        ++expectedPoint;
                    }

                    // The split comes as fractions of the level, so a level of 0 would say nothing.
                    const std::vector<Real> split = basket->splitVaRLevel(horizon, 1.0);
                    const std::vector<Real> expected = reference->splitVaRLevel(horizon, 1.0);
                    const std::vector<Size>& live = basket->liveList();
                    BOOST_REQUIRE_EQUAL(split.size(), live.size());
                    for (Size i = 0; i < live.size(); ++i)
                        BOOST_CHECK_SMALL(split[i] - expected[live[i]], tolerance);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomModelsNthDefaultOnTheSameDay)
{
    BOOST_TEST_MESSAGE("Testing the random models' nth default where names default on the same day...");

    // Before #162 a path's defaults were kept in a map keyed by the day, so a second name
    // defaulting on a day was dropped and every later default moved up a place. At a
    // correlation of 0.9 the twenty names often default together: on RandomDefaultLM's paths
    // the probabilities of each being the 20th to default added up to 0.0057, where twenty
    // defaults have a probability of 0.0102. Every path with n defaults or more now counts once
    // across the names, in equal shares where the nth falls on a day with others.
    const NamesAroundADefault all(true, false, NamesAroundADefault::noName);
    Settings::instance().evaluationDate() = all.afterTheDefault;
    const Size names = NamesAroundADefault::poolSize;
    const std::vector<std::vector<Real>> weights(names, std::vector<Real>(1, std::sqrt(0.9)));
    const std::vector<std::vector<Real>> doubled(2 * names, std::vector<Real>(1, std::sqrt(0.9)));
    const std::vector<std::pair<std::string, ext::shared_ptr<DefaultLossModel>>> models = {
        {"random default", ext::make_shared<RandomDefaultLM<GaussianCopulaPolicy>>(
                               ext::make_shared<GaussianConstantLossLM>(weights, all.recoveries,
                                                                        LatentModelIntegrationType::GaussianQuadrature,
                                                                        GaussianCopulaPolicy::initTraits()),
                               20000)},
        {"random loss", ext::make_shared<RandomLossLM<GaussianCopulaPolicy>>(
                            ext::make_shared<GaussianSpotLossLM>(doubled, all.recoveries, 2.2,
                                                                 LatentModelIntegrationType::GaussianQuadrature,
                                                                 GaussianCopulaPolicy::initTraits()),
                            20000, 1.0e-6, 2863311530UL)}};

    for (const auto& model : models)
    {
        const auto basket = all.basket(0.0, 0.1);
        basket->setLossModel(model.second);
        for (Size n : {2, 5, 12, 20})
        {
            BOOST_TEST_CONTEXT(model.first << ", n = " << n)
            {
                const std::vector<Probability> nth = basket->probsBeingNthEvent(n, all.horizon);
                BOOST_CHECK_SMALL(std::accumulate(nth.begin(), nth.end(), Real(0.0)) -
                                      basket->probAtLeastNEvents(n, all.horizon),
                                  1.0e-12);
            }
        }
    }
}

// Names of 100 at the given hazard rates and recoveries, with a random default model at a correlation
// so close to 1 that, on its paths up to Sep 1, 2011, names with the same hazard rate default on the
// same day or not at all.
ext::shared_ptr<Basket> basketDefaultingTogether(const std::vector<Real>& hazards,
                                                 const std::vector<Real>& recoveries,
                                                 Real attachment,
                                                 Real detachment)
{
    const Date today = Settings::instance().evaluationDate();
    const NorthAmericaCorpDefaultKey key(EURCurrency(), SeniorSec, Period(), 1.);
    auto pool = ext::make_shared<Pool>();
    std::vector<std::string> names;
    for (Real hazard : hazards)
    {
        names.push_back("name-" + std::to_string(names.size()));
        std::vector<std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> curves;
        curves.emplace_back(key, Handle<DefaultProbabilityTermStructure>(ext::make_shared<FlatHazardRate>(
                                     today, makeQuoteHandle(hazard), ActualActual(ActualActual::ISDA))));
        pool->add(names.back(), Issuer(curves, DefaultEventSet()), key);
    }
    const Real notional = 100.0 * Real(hazards.size());
    auto basket = ext::make_shared<Basket>(today, names, std::vector<Real>(hazards.size(), 100.0), pool,
                                           attachment / notional, detachment / notional);
    basket->setLossModel(ext::make_shared<RandomDefaultLM<GaussianCopulaPolicy>>(
        ext::make_shared<GaussianConstantLossLM>(makeQuoteHandle(1.0 - 1.0e-15), recoveries,
                                                 LatentModelIntegrationType::GaussianQuadrature, hazards.size(),
                                                 GaussianCopulaPolicy::initTraits()),
        2000));
    return basket;
}

BOOST_AUTO_TEST_CASE(testRandomDefaultModelSplitOnTheSameDay)
{
    BOOST_TEST_MESSAGE("Testing the random default model's split of a tranche's loss among names defaulting on the "
                       "same day...");

    // Before #209 a path's defaults were sorted by the day alone, and whichever of a day's names
    // std::sort put first took the loss up to the detachment, or from the attachment. Each now takes
    // what it adds to the tranche's loss averaged over the orders of the day's defaults. Names of 100
    // at recoveries of 20 %, 40 % and 60 % lose 80, 60 and 40. Up to a detachment of 100, in the
    // order 80, 60, 40 the first two take 80 and 20; over the six orders, 43 1/3, 33 1/3 and 23 1/3.
    // Every path whose tranche loss is over the level has all three default on one day, so the split
    // of the level is that, in proportion.
    Settings::instance().evaluationDate() = Date(1, November, 2006);
    const Date horizon(1, September, 2011);
    const Real level = 50.0;
    const Real tolerance = 1.0e-10;
    const std::vector<Real> recoveries = {0.2, 0.4, 0.6};

    struct Case
    {
        std::string what;
        Real attachment, detachment;
        std::vector<Real> shares;
    };
    const std::vector<Case> cases = {{"the detachment among their losses", 0.0, 100.0, {130.0, 100.0, 70.0}},
                                     {"the attachment among their losses", 100.0, 200.0, {110.0, 80.0, 50.0}},
                                     {"both among their losses", 50.0, 150.0, {155.0, 95.0, 50.0}}};
    for (const Case& c : cases)
    {
        BOOST_TEST_CONTEXT(c.what)
        {
            const auto basket =
                basketDefaultingTogether(std::vector<Real>(3, 0.05), recoveries, c.attachment, c.detachment);
            const std::vector<Real> split = basket->splitVaRLevel(horizon, level);
            const Real total = std::accumulate(c.shares.begin(), c.shares.end(), Real(0.0));
            BOOST_REQUIRE_EQUAL(split.size(), c.shares.size());
            for (Size i = 0; i < split.size(); ++i)
                BOOST_CHECK_SMALL(split[i] - level * c.shares[i] / total, tolerance);
        }
    }

    // At hazard rates of 30 %, 10 % and 3 % each name defaults on a day of its own, in that order, and
    // takes what it adds: losing 50, 80 and 60 up to a detachment of 100, the first two take 50 each
    // on every path whose tranche loss is over the level.
    BOOST_TEST_CONTEXT("names alone on their days")
    {
        const auto basket = basketDefaultingTogether({0.3, 0.1, 0.03}, {0.5, 0.2, 0.4}, 0.0, 100.0);
        const std::vector<Real> split = basket->splitVaRLevel(horizon, level);
        const std::vector<Real> expected = {level / 2.0, level / 2.0, 0.0};
        BOOST_REQUIRE_EQUAL(split.size(), expected.size());
        for (Size i = 0; i < split.size(); ++i)
            BOOST_CHECK_SMALL(split[i] - expected[i], tolerance);
    }

    // Two names at 30 % lose 50 each on a day before the three, so the three's day starts from the
    // attachment of 100. The two take nothing, and the three the same as from 0 to a detachment of 100.
    BOOST_TEST_CONTEXT("the three defaulting after two others")
    {
        const auto basket =
            basketDefaultingTogether({0.3, 0.3, 0.03, 0.03, 0.03}, {0.5, 0.5, 0.2, 0.4, 0.6}, 100.0, 200.0);
        const std::vector<Real> split = basket->splitVaRLevel(horizon, level);
        const std::vector<Real> expected = {0.0, 0.0, level * 130.0 / 300.0, level * 100.0 / 300.0,
                                            level * 70.0 / 300.0};
        BOOST_REQUIRE_EQUAL(split.size(), expected.size());
        for (Size i = 0; i < split.size(); ++i)
            BOOST_CHECK_SMALL(split[i] - expected[i], tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testSameDayTrancheLosses)
{
    BOOST_TEST_MESSAGE("Testing the shares of a tranche's loss among names defaulting on the same day...");

    const Real tolerance = 1.0e-12;
    std::vector<Real> shares;

    // Up to detachment 30, below every loss, the first name in each order takes all 30. Each is first
    // in as many orders, so they share equally, though one loses 80 and the others 60, up to
    // detail::maxSameDayNamesToAverage names. Past that the names share in proportion to their losses.
    for (Size names : {detail::maxSameDayNamesToAverage, detail::maxSameDayNamesToAverage + 1})
    {
        BOOST_TEST_CONTEXT(names << " names")
        {
            std::vector<Real> losses(names, 60.0);
            losses[0] = 80.0;
            detail::sameDayTrancheLosses(0.0, losses, 0.0, 30.0, shares);
            const Real total = 80.0 + 60.0 * Real(names - 1);
            for (Size i = 0; i < names; ++i)
            {
                const Real expected =
                    names <= detail::maxSameDayNamesToAverage ? 30.0 / Real(names) : 30.0 * losses[i] / total;
                BOOST_CHECK_SMALL(shares[i] - expected, tolerance);
            }
        }
    }

    // A name with no loss takes nothing, and leaves the others theirs: 80, 60 and 40 from 0 to 100,
    // as in testRandomDefaultModelSplitOnTheSameDay. Nor does it count among the names.
    BOOST_TEST_CONTEXT("a name with no loss")
    {
        detail::sameDayTrancheLosses(0.0, {80.0, 0.0, 60.0, 40.0}, 0.0, 100.0, shares);
        const std::vector<Real> expected = {130.0 / 3.0, 0.0, 100.0 / 3.0, 70.0 / 3.0};
        for (Size i = 0; i < expected.size(); ++i)
            BOOST_CHECK_SMALL(shares[i] - expected[i], tolerance);

        std::vector<Real> losses(detail::maxSameDayNamesToAverage, 60.0);
        losses[0] = 80.0;
        losses.push_back(0.0);
        detail::sameDayTrancheLosses(0.0, losses, 0.0, 30.0, shares);
        for (Size i = 0; i < detail::maxSameDayNamesToAverage; ++i)
            BOOST_CHECK_SMALL(shares[i] - 30.0 / Real(detail::maxSameDayNamesToAverage), tolerance);
        BOOST_CHECK_EQUAL(shares.back(), 0.0);
    }

    // Where the day's losses cross neither the attachment nor the detachment, each name's loss is in
    // full in the tranche, or none of it is.
    BOOST_TEST_CONTEXT("within the tranche")
    {
        detail::sameDayTrancheLosses(120.0, {80.0, 60.0, 40.0}, 100.0, 400.0, shares);
        const std::vector<Real> expected = {80.0, 60.0, 40.0};
        for (Size i = 0; i < expected.size(); ++i)
            BOOST_CHECK_SMALL(shares[i] - expected[i], tolerance);
        detail::sameDayTrancheLosses(400.0, {80.0, 60.0, 40.0}, 100.0, 400.0, shares);
        for (Real share : shares)
            BOOST_CHECK_EQUAL(share, 0.0);
    }
}

// Names of 100 on flat hazard rates from their own curves' dates, at recoveries of 40 %, for the random models' days
// and records (#232, #244-#247). Each name's default, and its recovery under RandomLossLM, takes one factor of
// sqrt(0.05 + 0.04 (i % 8)), as NamesAroundADefault's do, and ConstantLossModel on the same factors gives the
// probabilities the random models' paths should show.
struct NamesOnCurves
{
    ext::shared_ptr<Pool> pool = ext::make_shared<Pool>();
    std::vector<std::string> names;
    std::vector<Handle<DefaultProbabilityTermStructure>> curves;
    std::vector<std::vector<Real>> factors;

    NamesOnCurves(const std::vector<Date>& curveDates, Real hazard)
    {
        const NorthAmericaCorpDefaultKey curveKey(EURCurrency(), SeniorSec, Period(0, Weeks), 10.);
        const NorthAmericaCorpDefaultKey poolKey(EURCurrency(), SeniorSec, Period(), 1.);
        for (const Date& curveDate : curveDates)
        {
            const Size i = names.size();
            names.push_back("issuer-" + std::to_string(i));
            factors.emplace_back(1, std::sqrt(0.05 + 0.04 * Real(i % 8)));
            curves.emplace_back(
                ext::make_shared<FlatHazardRate>(curveDate, makeQuoteHandle(hazard), ActualActual(ActualActual::ISDA)));
            std::vector<std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities;
            probabilities.emplace_back(curveKey, curves.back());
            pool->add(names.back(), Issuer(probabilities, DefaultEventSet()), poolKey);
        }
    }

    ext::shared_ptr<Basket> basket(const Date& inception,
                                   const ext::shared_ptr<DefaultLossModel>& model = nullptr) const
    {
        auto basket =
            ext::make_shared<Basket>(inception, names, std::vector<Real>(names.size(), 100.0), pool, 0.0, 1.0);
        if (model)
            basket->setLossModel(model);
        return basket;
    }

    ext::shared_ptr<ConstantLossModel<GaussianCopulaPolicy>> latentModel() const
    {
        return ext::make_shared<ConstantLossModel<GaussianCopulaPolicy>>(factors, std::vector<Real>(names.size(), 0.4),
                                                                         LatentModelIntegrationType::GaussianQuadrature,
                                                                         GaussianCopulaPolicy::initTraits());
    }

    ext::shared_ptr<DefaultLossModel> randomDefaultModel(Size paths) const
    {
        return ext::make_shared<RandomDefaultLM<GaussianCopulaPolicy>>(latentModel(), paths);
    }

    ext::shared_ptr<GaussianSpotLossLM> spotLossModel(Real recovery = 0.4) const
    {
        std::vector<std::vector<Real>> doubled(factors);
        doubled.insert(doubled.end(), factors.begin(), factors.end());
        return ext::make_shared<GaussianSpotLossLM>(doubled, std::vector<Real>(names.size(), recovery), 2.2,
                                                    LatentModelIntegrationType::GaussianQuadrature,
                                                    GaussianCopulaPolicy::initTraits());
    }

    ext::shared_ptr<DefaultLossModel> randomLossModel(Size paths, Real recovery = 0.4) const
    {
        return ext::make_shared<RandomLossLM<GaussianCopulaPolicy>>(spotLossModel(recovery), paths);
    }

    // RandomDefaultLM's and RandomLossLM's baskets, both on that many paths
    std::vector<std::pair<std::string, ext::shared_ptr<Basket>>> randomModels(const Date& inception, Size paths) const
    {
        return {{"random default", basket(inception, randomDefaultModel(paths))},
                {"random loss", basket(inception, randomLossModel(paths))}};
    }
};

// A probability on the paths, within three binomial standard errors of the expected one.
void checkOnPaths(Probability onPaths, Probability expected, Size paths)
{
    BOOST_CHECK_SMALL(onPaths - expected, 3.0 * std::sqrt(expected * (1.0 - expected) / Real(paths)));
}

BOOST_AUTO_TEST_CASE(testRandomModelsOnTheirFirstDays)
{
    BOOST_TEST_MESSAGE("Testing the random models' defaults over their first days...");

    // A path's default falls on the day k after its curve's date with P(k) <= p < P(k + 1), where P(k) is the
    // probability of a default within k days and p the path's draw. Before #247 Brent's solver looked for P(t) = p
    // on P read at whole days, a step function, and recorded about half of each day's defaults on the next: at a
    // hazard rate of 1 on 20,000 paths, both models gave a probability of 0.0107 of a default within a day, against
    // ConstantLossModel's 0.0210, 10.2 standard errors low, and within two days 6.4 low. They now find k itself.
    const Date today(31, August, 2006);
    Settings::instance().evaluationDate() = today;
    const Size paths = 20000;
    const NamesOnCurves names(std::vector<Date>(8, today), 1.0);
    const auto latent = names.basket(today, names.latentModel());
    for (const auto& [model, random] : names.randomModels(today, paths))
    {
        for (Integer days : {1, 2, 7, 30})
        {
            BOOST_TEST_CONTEXT(model << ", " << days << " days on")
            {
                checkOnPaths(random->probAtLeastNEvents(1, today + days), latent->probAtLeastNEvents(1, today + days),
                             paths);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomModelsOnCurvesOfTwoDates)
{
    BOOST_TEST_MESSAGE("Testing the random models on default curves of two dates...");

    // The paths count each default's day from its curve's date, and ConstantLossModel counts the probability of a
    // default by a date from it too. Before #232 the statistics read the paths' days as from today: on curves of
    // September 30th and October 15th, with today November 1st, they gave no default by today, where
    // ConstantLossModel gives 0.0954, and 0.114 by December 1st against 0.192. They now read them from the earliest
    // curve date, with each name's offset from it; without the offsets they'd be 6.4 to 11.9 standard errors high.
    const Date today(1, November, 2006);
    Settings::instance().evaluationDate() = today;
    const Size paths = 20000;
    std::vector<Date> curveDates(4, Date(30, September, 2006));
    curveDates.insert(curveDates.end(), 4, Date(15, October, 2006));
    const NamesOnCurves names(curveDates, 0.2);
    const auto latent = names.basket(curveDates.front(), names.latentModel());
    for (const auto& [model, random] : names.randomModels(curveDates.front(), paths))
    {
        for (const Date& d : {today, today + 30})
        {
            for (Size n : {1, 2})
            {
                BOOST_TEST_CONTEXT(model << ", at " << d << ", n = " << n)
                {
                    checkOnPaths(random->probAtLeastNEvents(n, d), latent->probAtLeastNEvents(n, d), paths);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomModelsAsTodayMoves)
{
    BOOST_TEST_MESSAGE("Testing the random models' statistics as today moves...");

    // With no default between, moving today on from the curves' date leaves what the names can lose by a later date
    // as it was, and so every statistic there, on the same paths. Before #232 the statistics read the paths' days,
    // counted from the curves' date, as from today: a month on, 31 of RandomDefaultLM's 42 values here moved, by up
    // to 60, and 33 of RandomLossLM's.
    const Date curveDate(31, August, 2006);
    const NamesOnCurves names(std::vector<Date>(8, curveDate), 0.05);
    const std::vector<Date> dates = {Date(1, December, 2006), Date(1, March, 2007), Date(1, September, 2011)};
    for (Integer daysOn : {31})
    {
        for (bool loss : {false, true})
        {
            BOOST_TEST_CONTEXT((loss ? "random loss" : "random default") << ", today " << daysOn << " days on")
            {
                std::vector<Real> values[2];
                for (Integer k : {0, 1})
                {
                    Settings::instance().evaluationDate() = curveDate + k * daysOn;
                    const auto basket =
                        names.basket(curveDate, loss ? names.randomLossModel(2000) : names.randomDefaultModel(2000));
                    for (const Date& d : dates)
                    {
                        values[k].push_back(basket->probAtLeastNEvents(1, d));
                        values[k].push_back(basket->probAtLeastNEvents(3, d));
                        values[k].push_back(basket->expectedTrancheLoss(d));
                        values[k].push_back(basket->percentile(d, 0.99));
                        values[k].push_back(basket->expectedShortfall(d, 0.99));
                        const std::vector<Probability> second = basket->probsBeingNthEvent(2, d);
                        values[k].insert(values[k].end(), second.begin(), second.end());
                        values[k].push_back(basket->defaultCorrelation(d, 0, 7));
                    }
                }
                BOOST_REQUIRE_EQUAL(values[0].size(), values[1].size());
                for (Size i = 0; i < values[0].size(); ++i)
                    BOOST_CHECK_EQUAL(values[0][i], values[1][i]);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testRandomModelsOnCurvesFromAfterToday)
{
    BOOST_TEST_MESSAGE("Testing the random models on default curves starting after today...");

    // On curves from two days after today, no name defaults on or before then. Before #232 the statistics read the
    // paths' days as from today, so the curves' first day's defaults came by tomorrow: RandomDefaultLM gave a
    // probability of 0.0038 of a default by then, and 0.0083 by the curves' date. Both models now give none by
    // that date, and ConstantLossModel's probabilities after it.
    const Date today(31, August, 2006), curveDate = today + 2;
    Settings::instance().evaluationDate() = today;
    const Size paths = 20000;
    const NamesOnCurves names(std::vector<Date>(8, curveDate), 0.2);
    const auto latent = names.basket(today, names.latentModel());
    for (const auto& [model, random] : names.randomModels(today, paths))
    {
        for (const Date& d : {today + 1, curveDate})
        {
            BOOST_TEST_CONTEXT(model << ", at " << d)
            {
                BOOST_CHECK_EQUAL(random->probAtLeastNEvents(1, d), 0.0);
                BOOST_CHECK_EQUAL(random->expectedTrancheLoss(d), 0.0);
            }
        }
        BOOST_TEST_CONTEXT(model << ", a week after the curves' date")
        {
            checkOnPaths(random->probAtLeastNEvents(1, curveDate + 7), latent->probAtLeastNEvents(1, curveDate + 7),
                         paths);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBaseCorrelationAfterADefault)
{
    BOOST_TEST_MESSAGE("Testing base correlation models on what a default leaves of a tranche...");

    const Real tolerance = 1.0e-9;
    for (const auto& model : baseCorrelationModels())
    {
        for (bool sameLoss : {true, false})
        {
            for (const auto& tranche : tranchesAfterADefault(sameLoss))
            {
                BOOST_TEST_CONTEXT(model.name << (sameLoss ? ", the same loss on default" : ", different names")
                                              << ", tranche " << tranche.attachment << " to " << tranche.detachment)
                {
                    const NamesAroundADefault all(sameLoss, false);
                    const NamesAroundADefault left(sameLoss, true);
                    Settings::instance().evaluationDate() = all.afterTheDefault;

                    const auto basket = all.basket(tranche.attachment, tranche.detachment);
                    basket->setLossModel((all.*model.factory)());
                    const auto reference = whatIsLeftOf(tranche, left);
                    if (reference != nullptr)
                        reference->setLossModel((left.*model.factory)());

                    // At the second date, as an engine asks for a coupon that started before the default.
                    for (const Date& d : {all.horizon, all.beforeTheDefault})
                    {
                        const Real onNamesLeft = reference != nullptr ? reference->expectedTrancheLoss(d) : 0.0;
                        BOOST_CHECK_SMALL(basket->expectedTrancheLoss(d) - tranche.realized - onNamesLeft, tolerance);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testBaseCorrelationAsANameDefaults)
{
    BOOST_TEST_MESSAGE("Testing base correlation models when a name defaults under them...");

    const Real tolerance = 1.0e-9;
    for (const auto& model : baseCorrelationModels())
    {
        for (bool sameLoss : {true, false})
        {
            for (const auto& tranche : tranchesAfterADefault(sameLoss))
            {
                BOOST_TEST_CONTEXT(model.name << (sameLoss ? ", the same loss on default" : ", different names")
                                              << ", tranche " << tranche.attachment << " to " << tranche.detachment)
                {
                    const NamesAroundADefault all(sameLoss, false);
                    const NamesAroundADefault left(sameLoss, true);

                    // The model sets its own models up on all twenty, and then again on the names left.
                    Settings::instance().evaluationDate() = all.inception;
                    const auto basket = all.basket(tranche.attachment, tranche.detachment);
                    basket->setLossModel((all.*model.factory)());
                    BOOST_CHECK_GE(basket->expectedTrancheLoss(all.horizon), 0.0);

                    Settings::instance().evaluationDate() = all.afterTheDefault;
                    const auto reference = whatIsLeftOf(tranche, left);
                    if (reference != nullptr)
                        reference->setLossModel((left.*model.factory)());
                    const Real onNamesLeft = reference != nullptr ? reference->expectedTrancheLoss(all.horizon) : 0.0;
                    BOOST_CHECK_SMALL(basket->expectedTrancheLoss(all.horizon) - tranche.realized - onNamesLeft,
                                      tolerance);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testBaseCorrelationOnAFlatSurface)
{
    BOOST_TEST_MESSAGE("Testing base correlation models against the models under them on a flat surface...");

    using Factory = std::function<ext::shared_ptr<DefaultLossModel>(const NamesAroundADefault&)>;
    const Handle<Quote> correlation = makeQuoteHandle(0.3);
    const auto integration = LatentModelIntegrationType::GaussianQuadrature;
    const GaussianCopulaPolicy::initTraits gaussian{};
    const TCopulaPolicy::initTraits t = NamesAroundADefault::tTraits();

    struct FlatCase
    {
        std::string name;
        Factory onSurface, onItsOwn;
    };
    const std::vector<FlatCase> models = {
        {"LHP", [&](const NamesAroundADefault& names)
         { return names.baseCorrelationModel<GaussianLHPLossModel>(gaussian, 0.3, 0.3); },
         [&](const NamesAroundADefault& names) -> ext::shared_ptr<DefaultLossModel>
         { return ext::make_shared<GaussianLHPLossModel>(correlation, names.recoveries); }},
        {"binomial", [&](const NamesAroundADefault& names)
         { return names.baseCorrelationModel<GaussianBinomialLossModel>(gaussian, 0.3, 0.3); },
         [&](const NamesAroundADefault& names) -> ext::shared_ptr<DefaultLossModel>
         {
             return ext::make_shared<GaussianBinomialLossModel>(ext::make_shared<GaussianConstantLossLM>(
                 correlation, names.recoveries, integration, names.recoveries.size(), gaussian));
         }},
        {"T binomial",
         [&](const NamesAroundADefault& names) { return names.baseCorrelationModel<TBinomialLossModel>(t, 0.3, 0.3); },
         [&](const NamesAroundADefault& names) -> ext::shared_ptr<DefaultLossModel>
         {
             return ext::make_shared<TBinomialLossModel>(ext::make_shared<TConstantLossLM>(
                 correlation, names.recoveries, integration, names.recoveries.size(), t));
         }},
        {"inhomogeneous", [&](const NamesAroundADefault& names)
         { return names.baseCorrelationModel<IHGaussPoolLossModel>(gaussian, 0.3, 0.3); },
         [&](const NamesAroundADefault& names) -> ext::shared_ptr<DefaultLossModel>
         {
             return ext::make_shared<IHGaussPoolLossModel>(
                 ext::make_shared<GaussianConstantLossLM>(correlation, names.recoveries, integration,
                                                          names.recoveries.size(), gaussian),
                 500);
         }}};
    const std::vector<std::pair<Real, Real>> tranches = {{0.0, 0.03}, {0.0, 0.1}, {0.03, 0.1}, {0.1, 0.3}, {0.0, 1.0}};

    for (const auto& model : models)
    {
        for (bool sameLoss : {true, false})
        {
            for (const auto& tranche : tranches)
            {
                BOOST_TEST_CONTEXT(model.name << (sameLoss ? ", the same loss on default" : ", different names")
                                              << ", tranche " << tranche.first << " to " << tranche.second)
                {
                    const NamesAroundADefault names(sameLoss, false, NamesAroundADefault::noName);
                    Settings::instance().evaluationDate() = names.afterTheDefault;

                    const auto basket = names.basket(tranche.first, tranche.second);
                    basket->setLossModel(model.onSurface(names));

                    // What base correlation is: the difference of two tranches from zero.
                    const auto expectedOnFirstLoss = [&](Real detachment)
                    {
                        const auto firstLoss = names.basket(0.0, detachment);
                        firstLoss->setLossModel(model.onItsOwn(names));
                        return firstLoss->expectedTrancheLoss(names.horizon);
                    };
                    const Real expected = expectedOnFirstLoss(tranche.second) -
                                          (tranche.first > 0.0 ? expectedOnFirstLoss(tranche.first) : 0.0);
                    BOOST_CHECK_SMALL(basket->expectedTrancheLoss(names.horizon) - expected, 1.0e-9);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testPoolKeysInTheOrderOfItsNames)
{
    BOOST_TEST_MESSAGE("Testing that a pool's keys come in the order of its names...");

    const NorthAmericaCorpDefaultKey senior(EURCurrency(), SeniorSec, Period(), 1.);
    const NorthAmericaCorpDefaultKey subordinated(EURCurrency(), SubUpperTier2, Period(), 1.);

    Pool pool;
    pool.add("zeta", Issuer(), senior);
    pool.add("alpha", Issuer(), subordinated);
    pool.add("mike", Issuer(), senior);
    // a name that's already there keeps its key
    pool.add("alpha", Issuer(), senior);

    BOOST_REQUIRE_EQUAL(pool.defaultKeys().size(), pool.names().size());
    BOOST_CHECK(pool.defaultKeys()[0] == senior);
    BOOST_CHECK(pool.defaultKeys()[1] == subordinated);
    BOOST_CHECK(pool.defaultKeys()[2] == senior);
    for (Size i = 0; i < pool.size(); ++i)
        BOOST_CHECK(pool.defaultKeys()[i] == pool.defaultKey(pool.names()[i]));

    pool.clear();
    BOOST_CHECK(pool.defaultKeys().empty());

    pool.add("beta", Issuer(), subordinated);
    BOOST_REQUIRE_EQUAL(pool.defaultKeys().size(), Size(1));
    BOOST_CHECK(pool.defaultKeys()[0] == subordinated);
}

BOOST_AUTO_TEST_CASE(testBasketOnKeysThatDiffer)
{
    BOOST_TEST_MESSAGE("Testing that a basket reads each name on its own key...");

    const NamesAroundADefault onOneKey(false, false);
    const NamesAroundADefault onTwoKeys(false, false, NamesAroundADefault::oneName, true);
    Settings::instance().evaluationDate() = onOneKey.afterTheDefault;

    const auto basket = onTwoKeys.basket(0.0, 0.1);
    const auto reference = onOneKey.basket(0.0, 0.1);

    BOOST_CHECK_EQUAL(basket->remainingSize(), Size(NamesAroundADefault::poolSize - 1));
    BOOST_CHECK(basket->liveList() == reference->liveList());
    BOOST_CHECK_EQUAL(basket->settledLoss(), reference->settledLoss());
    BOOST_CHECK_EQUAL(basket->remainingNotional(), reference->remainingNotional());

    const std::vector<Probability> probabilities = basket->probabilities(onOneKey.horizon);
    const std::vector<Probability> expected = reference->probabilities(onOneKey.horizon);
    const std::vector<Probability> left = basket->remainingProbabilities(onOneKey.horizon);
    const std::vector<Probability> expectedLeft = reference->remainingProbabilities(onOneKey.horizon);
    BOOST_REQUIRE_EQUAL(probabilities.size(), expected.size());
    BOOST_REQUIRE_EQUAL(left.size(), expectedLeft.size());
    for (Size i = 0; i < probabilities.size(); ++i)
        BOOST_CHECK_EQUAL(probabilities[i], expected[i]);
    for (Size i = 0; i < left.size(); ++i)
        BOOST_CHECK_EQUAL(left[i], expectedLeft[i]);
}

BOOST_AUTO_TEST_CASE(testEveryLossModelOnKeysThatDiffer)
{
    BOOST_TEST_MESSAGE("Testing that every loss model reads each name on its own key...");

    std::vector<ModelAroundADefault> models = modelsAroundADefault();
    const std::vector<ModelAroundADefault> onSurfaces = baseCorrelationModels();
    models.insert(models.end(), onSurfaces.begin(), onSurfaces.end());
    models.push_back({"random default", &NamesAroundADefault::randomDefaultModel, false});
    models.push_back({"random loss", &NamesAroundADefault::randomLossModel, false});

    for (const auto& model : models)
    {
        for (bool sameLoss : {true, false})
        {
            if (model.needsTheSameLossOnDefault && !sameLoss)
                continue;
            for (auto defaults : {NamesAroundADefault::noName, NamesAroundADefault::oneName})
            {
                BOOST_TEST_CONTEXT(model.name << (sameLoss ? ", the same loss on default" : ", different names")
                                              << (defaults == NamesAroundADefault::oneName ? ", one default" : ""))
                {
                    const NamesAroundADefault onOneKey(sameLoss, false, defaults);
                    const NamesAroundADefault onTwoKeys(sameLoss, false, defaults, true);
                    Settings::instance().evaluationDate() = onOneKey.afterTheDefault;

                    const auto basket = onTwoKeys.basket(0.0, 0.1);
                    basket->setLossModel((onTwoKeys.*model.factory)());
                    const auto reference = onOneKey.basket(0.0, 0.1);
                    reference->setLossModel((onOneKey.*model.factory)());

                    BOOST_CHECK_SMALL(basket->expectedTrancheLoss(onOneKey.horizon) -
                                          reference->expectedTrancheLoss(onOneKey.horizon),
                                      1.0e-9);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testCdoObservesEachNameOnItsOwnKey)
{
    BOOST_TEST_MESSAGE("Testing that a CDO tranche observes each name's curve on its own key...");

    const NamesAroundADefault all(false, false, NamesAroundADefault::noName, true);
    Settings::instance().evaluationDate() = all.inception;

    const NorthAmericaCorpDefaultKey senior(EURCurrency(), SeniorSec, Period(), 1.);
    const NorthAmericaCorpDefaultKey subordinated(EURCurrency(), SubUpperTier2, Period(), 1.);

    const Schedule schedule = MakeSchedule()
                                  .from(Date(1, September, 2006))
                                  .to(Date(1, September, 2011))
                                  .withFrequency(Quarterly)
                                  .withCalendar(TARGET())
                                  .withConvention(Following)
                                  .withRule(DateGeneration::Backward);
    const auto basket = all.basket(0.0, 0.1);
    basket->setLossModel(all.lhpModel());
    auto cdo = ext::make_shared<SyntheticCDO>(basket, Protection::Seller, schedule, 0.02, 0.05, Actual360(), Following);
    cdo->setPricingEngine(ext::make_shared<MidPointCDOEngine>(
        Handle<YieldTermStructure>(ext::make_shared<FlatForward>(all.inception, 0.05, Actual360(), Continuous))));

    Flag priceMayHaveMoved;
    priceMayHaveMoved.registerWith(cdo);
    for (const std::string& name : all.names)
    {
        BOOST_TEST_CONTEXT("name " << name)
        {
            const Issuer& issuer = all.pool->get(name);
            const DefaultProbKey& own = all.pool->defaultKey(name);

            cdo->NPV();
            priceMayHaveMoved.lower();
            issuer.defaultProbability(own)->update();
            BOOST_CHECK(priceMayHaveMoved.isUp());

            cdo->NPV();
            priceMayHaveMoved.lower();
            issuer.defaultProbability(own == senior ? subordinated : senior)->update();
            BOOST_CHECK(!priceMayHaveMoved.isUp());
        }
    }
}

BOOST_AUTO_TEST_CASE(testExpectedShortfallBetweenPercentileAndTranche)
{
    BOOST_TEST_MESSAGE("Testing that expected shortfall lies between the percentile and the tranche...");

    // The mean of the losses beyond a level is at least the level's and at most the tranche,
    // and it can't fall as the level rises. Before #159 the saddle point returned 80 above a
    // percentile of 400 and 117 on a tranche of 100, and the binomial and recursive models read
    // before their first point below the probability of no loss, 3307 on a tranche of 400.
    // Before #160 the saddle point's percentile didn't solve below about 0.26 on an equity
    // tranche. Its shortfall still falls a little where the level crosses the loss of one
    // default, by 0.22 between 0.6 and 0.605 on 0 to 10 % (#205), which the rise to 0.8 covers.
    SharedModelVars vars(0.3);
    Settings::instance().evaluationDate() = vars.today;
    const Date horizon = vars.schedule.endDate();

    using Factory = ext::shared_ptr<DefaultLossModel> (SharedModelVars::*)() const;
    const std::vector<std::pair<std::string, Factory>> models = {{"binomial", &SharedModelVars::binomialModel},
                                                                 {"recursive", &SharedModelVars::recursiveModel},
                                                                 {"saddle point", &SharedModelVars::saddlePointModel}};
    const std::vector<std::pair<Real, Real>> tranches = {{0.0, 0.03}, {0.0, 0.1}, {0.05, 0.1}, {0.0, 0.2},
                                                         {0.1, 0.3},  {0.3, 0.6}, {0.5, 1.0}};
    const std::vector<Real> levels = {0.05, 0.1, 0.2, 0.3, 0.5, 0.6, 0.8, 0.9, 0.95, 0.975, 0.99, 0.999, 1.0};
    const Real tolerance = 1.0e-9;
    for (const auto& model : models)
    {
        for (const auto& tranche : tranches)
        {
            const auto basket = vars.basket(tranche.first, tranche.second);
            basket->setLossModel((vars.*model.second)());
            const Real trancheAmount = basket->remainingTrancheNotional();
            Real previous = 0.0;
            for (Real level : levels)
            {
                BOOST_TEST_CONTEXT(model.first << ", tranche " << tranche.first << " to " << tranche.second
                                               << ", level " << level)
                {
                    const Real percentile = basket->percentile(horizon, level);
                    const Real shortfall = basket->expectedShortfall(horizon, level);
                    BOOST_CHECK_GE(shortfall, percentile - tolerance);
                    BOOST_CHECK_LE(shortfall, trancheAmount + tolerance);
                    BOOST_CHECK_GE(shortfall, previous - tolerance);
                    previous = shortfall;
                }
            }
            // nothing more is lost by today
            BOOST_TEST_CONTEXT(model.first << ", tranche " << tranche.first << " to " << tranche.second)
            {
                BOOST_CHECK_EQUAL(basket->expectedShortfall(vars.today, 0.95), 0.0);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testExpectedShortfallOfALossDistribution)
{
    BOOST_TEST_MESSAGE("Testing expected shortfall against the mean of the percentiles beyond it...");

    // The binomial and recursive models give the tranche's loss at a level from points of the
    // portfolio's loss distribution, linear in the probability in between. Their shortfall is
    // the mean of that above the level, taken here on a fine grid. Before #159 it drew a chord
    // where the portfolio's loss crosses the attachment or the detachment, and the recursive
    // model's percentile returned probabilities at levels 0 and 1, and the binomial model's the
    // portfolio's loss at 1.
    SharedModelVars vars(0.3);
    Settings::instance().evaluationDate() = vars.today;
    const Date horizon = vars.schedule.endDate();

    using Factory = ext::shared_ptr<DefaultLossModel> (SharedModelVars::*)() const;
    const std::vector<std::pair<std::string, Factory>> models = {{"binomial", &SharedModelVars::binomialModel},
                                                                 {"recursive", &SharedModelVars::recursiveModel}};
    const std::vector<std::pair<Real, Real>> tranches = {{0.0, 0.1}, {0.05, 0.1}, {0.0, 0.2}, {0.1, 0.3}};
    for (const auto& model : models)
    {
        for (const auto& tranche : tranches)
        {
            BOOST_TEST_CONTEXT(model.first << ", tranche " << tranche.first << " to " << tranche.second)
            {
                const auto basket = vars.basket(tranche.first, tranche.second);
                basket->setLossModel((vars.*model.second)());
                const Real attachment = basket->remainingAttachmentAmount();
                const Real detachment = basket->remainingDetachmentAmount();
                auto trancheLoss = [&](Real loss)
                { return std::min(std::max(loss - attachment, 0.0), detachment - attachment); };

                std::vector<std::pair<Probability, Real>> points;
                for (const auto& point : basket->lossDistribution(horizon))
                    points.emplace_back(std::min(point.second, 1.0), point.first);
                auto lossAt = [&](Probability level)
                {
                    if (level <= points.front().first)
                        return trancheLoss(points.front().second);
                    for (Size i = 1; i < points.size(); ++i)
                    {
                        if (points[i].first > level)
                            return trancheLoss(points[i - 1].second + (points[i].second - points[i - 1].second) *
                                                                          (level - points[i - 1].first) /
                                                                          (points[i].first - points[i - 1].first));
                    }
                    return trancheLoss(points.back().second);
                };

                BOOST_CHECK_EQUAL(basket->percentile(horizon, 0.0), trancheLoss(points.front().second));
                BOOST_CHECK_EQUAL(basket->percentile(horizon, 1.0), trancheLoss(points.back().second));
                for (Real level : {0.3, 0.5, 0.9, 0.95, 0.99})
                {
                    BOOST_TEST_CONTEXT("level " << level)
                    {
                        BOOST_CHECK_SMALL(basket->percentile(horizon, level) - lossAt(level), 1.0e-9);

                        const Size steps = 100000;
                        Real mean = 0.0;
                        for (Size i = 0; i < steps; ++i)
                            mean += lossAt(level + (i + 0.5) * (1.0 - level) / steps);
                        mean /= steps;
                        BOOST_CHECK_SMALL(basket->expectedShortfall(horizon, level) - mean, 1.0e-5);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
