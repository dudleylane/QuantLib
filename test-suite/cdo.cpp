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
#include <iomanip>
#include <iostream>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CdoTests, *precondition(if_speed(Slow)))

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

using dataSets = boost::mpl::vector<dataSetOne, dataSetTwo, dataSetThree, dataSetFour, dataSetFive>;

BOOST_AUTO_TEST_CASE_TEMPLATE(testHW, T, dataSets)
{

    const int dataSet = T::dataset;

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

    explicit SharedModelVars(Real correlationValue, const Date& defaultDate = Date(), Real hazardValue = 0.01)
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
            if (i == 3 && defaultDate != Date())
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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
