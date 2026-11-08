/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Ferdinando Ametrano
 Copyright (C) 2006 Giorgio Facchinetti
 Copyright (C) 2006 Mario Pucci
 Copyright (C) 2014 Peter Caspers

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
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/conundrumpricer.hpp>
#include <ql/cashflows/lineartsrpricer.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makecms.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/math/optimization/endcriteria.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/optimization/simplex.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/swaption/cmsmarket.hpp>
#include <ql/termstructures/volatility/swaption/cmsmarketcalibration.hpp>
#include <ql/termstructures/volatility/swaption/interpolatedswaptionvolatilitycube.hpp>
#include <ql/termstructures/volatility/swaption/sabrswaptionvolatilitycube.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolmatrix.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/schedule.hpp>
#include <ql/utilities/dataformatters.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CmsTests)

struct CommonVars
{
    // global data
    RelinkableHandle<YieldTermStructure> termStructure;

    ext::shared_ptr<IborIndex> iborIndex;

    Handle<SwaptionVolatilityStructure> atmVol;
    Handle<SwaptionVolatilityStructure> SabrVolCube1;
    Handle<SwaptionVolatilityStructure> SabrVolCube2;

    std::vector<GFunctionFactory::YieldCurveModel> yieldCurveModels;
    std::vector<ext::shared_ptr<CmsCouponPricer>> numericalPricers;
    std::vector<ext::shared_ptr<CmsCouponPricer>> analyticPricers;

    // setup
    CommonVars()
    {

        Calendar calendar = TARGET();

        Date referenceDate = calendar.adjust(Date::todaysDate());
        Settings::instance().evaluationDate() = referenceDate;

        termStructure.linkTo(flatRate(referenceDate, 0.05, Actual365Fixed()));

        // ATM Volatility structure
        std::vector<Period> atmOptionTenors = {1 * Months, 6 * Months, 1 * Years, 5 * Years, 10 * Years, 30 * Years};

        std::vector<Period> atmSwapTenors = {1 * Years, 5 * Years, 10 * Years, 30 * Years};

        Matrix m(atmOptionTenors.size(), atmSwapTenors.size());
        m[0][0] = 0.1300;
        m[0][1] = 0.1560;
        m[0][2] = 0.1390;
        m[0][3] = 0.1220;
        m[1][0] = 0.1440;
        m[1][1] = 0.1580;
        m[1][2] = 0.1460;
        m[1][3] = 0.1260;
        m[2][0] = 0.1600;
        m[2][1] = 0.1590;
        m[2][2] = 0.1470;
        m[2][3] = 0.1290;
        m[3][0] = 0.1640;
        m[3][1] = 0.1470;
        m[3][2] = 0.1370;
        m[3][3] = 0.1220;
        m[4][0] = 0.1400;
        m[4][1] = 0.1300;
        m[4][2] = 0.1250;
        m[4][3] = 0.1100;
        m[5][0] = 0.1130;
        m[5][1] = 0.1090;
        m[5][2] = 0.1070;
        m[5][3] = 0.0930;

        atmVol = Handle<SwaptionVolatilityStructure>(ext::shared_ptr<SwaptionVolatilityStructure>(
            new SwaptionVolatilityMatrix(calendar, Following, atmOptionTenors, atmSwapTenors, m, Actual365Fixed())));

        // Vol cubes
        std::vector<Period> optionTenors = {{1, Years}, {10, Years}, {30, Years}};
        std::vector<Period> swapTenors = {{2, Years}, {10, Years}, {30, Years}};
        std::vector<Spread> strikeSpreads = {-0.020, -0.005, 0.000, 0.005, 0.020};

        Size nRows = optionTenors.size() * swapTenors.size();
        Size nCols = strikeSpreads.size();
        Matrix volSpreadsMatrix(nRows, nCols);
        volSpreadsMatrix[0][0] = 0.0599;
        volSpreadsMatrix[0][1] = 0.0049;
        volSpreadsMatrix[0][2] = 0.0000;
        volSpreadsMatrix[0][3] = -0.0001;
        volSpreadsMatrix[0][4] = 0.0127;

        volSpreadsMatrix[1][0] = 0.0729;
        volSpreadsMatrix[1][1] = 0.0086;
        volSpreadsMatrix[1][2] = 0.0000;
        volSpreadsMatrix[1][3] = -0.0024;
        volSpreadsMatrix[1][4] = 0.0098;

        volSpreadsMatrix[2][0] = 0.0738;
        volSpreadsMatrix[2][1] = 0.0102;
        volSpreadsMatrix[2][2] = 0.0000;
        volSpreadsMatrix[2][3] = -0.0039;
        volSpreadsMatrix[2][4] = 0.0065;

        volSpreadsMatrix[3][0] = 0.0465;
        volSpreadsMatrix[3][1] = 0.0063;
        volSpreadsMatrix[3][2] = 0.0000;
        volSpreadsMatrix[3][3] = -0.0032;
        volSpreadsMatrix[3][4] = -0.0010;

        volSpreadsMatrix[4][0] = 0.0558;
        volSpreadsMatrix[4][1] = 0.0084;
        volSpreadsMatrix[4][2] = 0.0000;
        volSpreadsMatrix[4][3] = -0.0050;
        volSpreadsMatrix[4][4] = -0.0057;

        volSpreadsMatrix[5][0] = 0.0576;
        volSpreadsMatrix[5][1] = 0.0083;
        volSpreadsMatrix[5][2] = 0.0000;
        volSpreadsMatrix[5][3] = -0.0043;
        volSpreadsMatrix[5][4] = -0.0014;

        volSpreadsMatrix[6][0] = 0.0437;
        volSpreadsMatrix[6][1] = 0.0059;
        volSpreadsMatrix[6][2] = 0.0000;
        volSpreadsMatrix[6][3] = -0.0030;
        volSpreadsMatrix[6][4] = -0.0006;

        volSpreadsMatrix[7][0] = 0.0533;
        volSpreadsMatrix[7][1] = 0.0078;
        volSpreadsMatrix[7][2] = 0.0000;
        volSpreadsMatrix[7][3] = -0.0045;
        volSpreadsMatrix[7][4] = -0.0046;

        volSpreadsMatrix[8][0] = 0.0545;
        volSpreadsMatrix[8][1] = 0.0079;
        volSpreadsMatrix[8][2] = 0.0000;
        volSpreadsMatrix[8][3] = -0.0042;
        volSpreadsMatrix[8][4] = -0.0020;

        std::vector<std::vector<Handle<Quote>>> volSpreads(nRows);
        for (Size i = 0; i < nRows; ++i)
        {
            volSpreads[i] = std::vector<Handle<Quote>>(nCols);
            for (Size j = 0; j < nCols; ++j)
            {
                volSpreads[i][j] = Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(volSpreadsMatrix[i][j])));
            }
        }

        iborIndex = ext::shared_ptr<IborIndex>(new Euribor6M(termStructure));
        ext::shared_ptr<SwapIndex> swapIndexBase(new EuriborSwapIsdaFixA(10 * Years, termStructure));
        ext::shared_ptr<SwapIndex> shortSwapIndexBase(new EuriborSwapIsdaFixA(2 * Years, termStructure));

        bool vegaWeightedSmileFit = false;

        SabrVolCube2 = Handle<SwaptionVolatilityStructure>(ext::make_shared<InterpolatedSwaptionVolatilityCube>(
            atmVol, optionTenors, swapTenors, strikeSpreads, volSpreads, swapIndexBase, shortSwapIndexBase,
            vegaWeightedSmileFit));
        SabrVolCube2->enableExtrapolation();

        std::vector<std::vector<Handle<Quote>>> guess(nRows);
        for (Size i = 0; i < nRows; ++i)
        {
            guess[i] = std::vector<Handle<Quote>>(4);
            guess[i][0] = Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(0.2)));
            guess[i][1] = Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(0.5)));
            guess[i][2] = Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(0.4)));
            guess[i][3] = Handle<Quote>(ext::shared_ptr<Quote>(new SimpleQuote(0.0)));
        }
        std::vector<bool> isParameterFixed(4, false);
        isParameterFixed[1] = true;

        // FIXME
        bool isAtmCalibrated = false;

        SabrVolCube1 = Handle<SwaptionVolatilityStructure>(ext::make_shared<SabrSwaptionVolatilityCube>(
            atmVol, optionTenors, swapTenors, strikeSpreads, volSpreads, swapIndexBase, shortSwapIndexBase,
            vegaWeightedSmileFit, guess, isParameterFixed, isAtmCalibrated));
        SabrVolCube1->enableExtrapolation();

        yieldCurveModels = {GFunctionFactory::Standard, GFunctionFactory::ExactYield, GFunctionFactory::ParallelShifts,
                            GFunctionFactory::NonParallelShifts, GFunctionFactory::NonParallelShifts};

        Handle<Quote> zeroMeanRev(ext::make_shared<SimpleQuote>(0.0));

        numericalPricers.clear();
        analyticPricers.clear();
        for (Size j = 0; j < yieldCurveModels.size(); ++j)
        {
            if (j < yieldCurveModels.size() - 1)
                numericalPricers.push_back(
                    ext::shared_ptr<CmsCouponPricer>(new NumericHaganPricer(atmVol, yieldCurveModels[j], zeroMeanRev)));
            else
                numericalPricers.push_back(ext::shared_ptr<CmsCouponPricer>(new LinearTsrPricer(atmVol, zeroMeanRev)));

            analyticPricers.push_back(
                ext::shared_ptr<CmsCouponPricer>(new AnalyticHaganPricer(atmVol, yieldCurveModels[j], zeroMeanRev)));
        }
    }
};


BOOST_AUTO_TEST_CASE(testFairRate)
{

    BOOST_TEST_MESSAGE("Testing Hagan-pricer flat-vol equivalence for coupons (lognormal case)...");

    CommonVars vars;

    ext::shared_ptr<SwapIndex> swapIndex(new SwapIndex("EuriborSwapIsdaFixA", 10 * Years, vars.iborIndex->fixingDays(),
                                                       vars.iborIndex->currency(), vars.iborIndex->fixingCalendar(),
                                                       1 * Years, Unadjusted,
                                                       vars.iborIndex->dayCounter(), //??
                                                       vars.iborIndex));
    // FIXME
    // ext::shared_ptr<SwapIndex> swapIndex(new
    //    EuriborSwapIsdaFixA(10*Years, vars.iborIndex->termStructure()));
    Date startDate = vars.termStructure->referenceDate() + 20 * Years;
    Date paymentDate = startDate + 1 * Years;
    Date endDate = paymentDate;
    Real nominal = 1.0;
    Rate infiniteCap = Null<Real>();
    Rate infiniteFloor = Null<Real>();
    Real gearing = 1.0;
    Spread spread = 0.0;
    CappedFlooredCmsCoupon coupon(paymentDate, nominal, startDate, endDate, swapIndex->fixingDays(), swapIndex, gearing,
                                  spread, infiniteCap, infiniteFloor, startDate, endDate, vars.iborIndex->dayCounter());
    for (Size j = 0; j < vars.yieldCurveModels.size(); ++j)
    {
        vars.numericalPricers[j]->setSwaptionVolatility(vars.atmVol);
        coupon.setPricer(vars.numericalPricers[j]);
        Rate rate0 = coupon.rate();

        vars.analyticPricers[j]->setSwaptionVolatility(vars.atmVol);
        coupon.setPricer(vars.analyticPricers[j]);
        Rate rate1 = coupon.rate();

        Spread difference = std::fabs(rate1 - rate0);
        Spread tol = 2.0e-4;
        bool linearTsr = j == vars.yieldCurveModels.size() - 1;

        if (difference > tol)
            BOOST_FAIL(
                "\nCoupon payment date: "
                << paymentDate << "\nCoupon start date:   " << startDate
                << "\nCoupon floor:        " << io::rate(infiniteFloor)
                << "\nCoupon gearing:      " << io::rate(gearing) << "\nCoupon swap index:   " << swapIndex->name()
                << "\nCoupon spread:       " << io::rate(spread) << "\nCoupon cap:          " << io::rate(infiniteCap)
                << "\nCoupon DayCounter:   " << vars.iborIndex->dayCounter()
                << "\nYieldCurve Model:    " << vars.yieldCurveModels[j] << "\nNumerical Pricer:    " << io::rate(rate0)
                << (linearTsr ? " (Linear TSR Model)" : "") << "\nAnalytic Pricer:     " << io::rate(rate1)
                << "\ndifference:          " << io::rate(difference) << "\ntolerance:           " << io::rate(tol));
    }
}

BOOST_AUTO_TEST_CASE(testCmsSwap)
{

    BOOST_TEST_MESSAGE("Testing Hagan-pricer flat-vol equivalence for swaps (lognormal case)...");

    CommonVars vars;

    ext::shared_ptr<SwapIndex> swapIndex(new SwapIndex("EuriborSwapIsdaFixA", 10 * Years, vars.iborIndex->fixingDays(),
                                                       vars.iborIndex->currency(), vars.iborIndex->fixingCalendar(),
                                                       1 * Years, Unadjusted,
                                                       vars.iborIndex->dayCounter(), //??
                                                       vars.iborIndex));
    // FIXME
    // ext::shared_ptr<SwapIndex> swapIndex(new
    //    EuriborSwapIsdaFixA(10*Years, vars.iborIndex->termStructure()));
    Spread spread = 0.0;
    std::vector<Size> swapLengths = {1, 5, 6, 10};
    Size n = swapLengths.size();
    std::vector<ext::shared_ptr<Swap>> cms(n);
    for (Size i = 0; i < n; ++i)
        // no cap, floor
        // no gearing, spread
        cms[i] = MakeCms(Period(swapLengths[i], Years), swapIndex, vars.iborIndex, spread, 10 * Days);

    for (Size j = 0; j < vars.yieldCurveModels.size(); ++j)
    {
        vars.numericalPricers[j]->setSwaptionVolatility(vars.atmVol);
        vars.analyticPricers[j]->setSwaptionVolatility(vars.atmVol);
        for (Size sl = 0; sl < n; ++sl)
        {
            setCouponPricer(cms[sl]->leg(0), vars.numericalPricers[j]);
            Real priceNum = cms[sl]->NPV();
            setCouponPricer(cms[sl]->leg(0), vars.analyticPricers[j]);
            Real priceAn = cms[sl]->NPV();

            Real difference = std::fabs(priceNum - priceAn);
            Real tol = 2.0e-4;
            bool linearTsr = j == vars.yieldCurveModels.size() - 1;
            if (difference > tol)
                BOOST_FAIL(
                    "\nLength in Years:  " << swapLengths[sl] <<
                    //"\nfloor:            " << io::rate(infiniteFloor) <<
                    //"\ngearing:          " << io::rate(gearing) <<
                    "\nswap index:       " << swapIndex->name() << "\nibor index:       " << vars.iborIndex->name()
                                           << "\nspread:           " << io::rate(spread) <<
                    //"\ncap:              " << io::rate(infiniteCap) <<
                    "\nYieldCurve Model: " << vars.yieldCurveModels[j] << "\nNumerical Pricer: " << io::rate(priceNum)
                                           << (linearTsr ? " (Linear TSR Model)" : "") << "\nAnalytic Pricer:  "
                                           << io::rate(priceAn) << "\ndifference:       " << io::rate(difference)
                                           << "\ntolerance:        " << io::rate(tol));
        }
    }
}

BOOST_AUTO_TEST_CASE(testParity)
{

    BOOST_TEST_MESSAGE("Testing put-call parity for capped-floored CMS coupons (lognormal case)...");

    CommonVars vars;

    std::vector<Handle<SwaptionVolatilityStructure>> swaptionVols = {vars.atmVol, vars.SabrVolCube1, vars.SabrVolCube2};

    ext::shared_ptr<SwapIndex> swapIndex(
        new EuriborSwapIsdaFixA(10 * Years, vars.iborIndex->forwardingTermStructure()));
    Date startDate = vars.termStructure->referenceDate() + 20 * Years;
    Date paymentDate = startDate + 1 * Years;
    Date endDate = paymentDate;
    Real nominal = 1.0;
    Rate infiniteCap = Null<Real>();
    Rate infiniteFloor = Null<Real>();
    Real gearing = 1.0;
    Spread spread = 0.0;
    DiscountFactor discount = vars.termStructure->discount(paymentDate);
    CappedFlooredCmsCoupon swaplet(paymentDate, nominal, startDate, endDate, swapIndex->fixingDays(), swapIndex,
                                   gearing, spread, infiniteCap, infiniteFloor, startDate, endDate,
                                   vars.iborIndex->dayCounter());
    for (Rate strike = .02; strike < .12; strike += 0.05)
    {
        CappedFlooredCmsCoupon caplet(paymentDate, nominal, startDate, endDate, swapIndex->fixingDays(), swapIndex,
                                      gearing, spread, strike, infiniteFloor, startDate, endDate,
                                      vars.iborIndex->dayCounter());
        CappedFlooredCmsCoupon floorlet(paymentDate, nominal, startDate, endDate, swapIndex->fixingDays(), swapIndex,
                                        gearing, spread, infiniteCap, strike, startDate, endDate,
                                        vars.iborIndex->dayCounter());

        for (auto& swaptionVol : swaptionVols)
        {
            for (Size j = 0; j < vars.yieldCurveModels.size(); ++j)
            {
                vars.numericalPricers[j]->setSwaptionVolatility(swaptionVol);
                vars.analyticPricers[j]->setSwaptionVolatility(swaptionVol);
                std::vector<ext::shared_ptr<CmsCouponPricer>> pricers(2);
                pricers[0] = vars.numericalPricers[j];
                pricers[1] = vars.analyticPricers[j];
                for (Size k = 0; k < pricers.size(); ++k)
                {
                    swaplet.setPricer(pricers[k]);
                    caplet.setPricer(pricers[k]);
                    floorlet.setPricer(pricers[k]);
                    Real swapletPrice =
                        swaplet.price(vars.termStructure) + nominal * swaplet.accrualPeriod() * strike * discount;
                    Real capletPrice = caplet.price(vars.termStructure);
                    Real floorletPrice = floorlet.price(vars.termStructure);
                    Real difference = std::fabs(capletPrice + floorletPrice - swapletPrice);
                    Real tol = 2.0e-5;
                    bool linearTsr = k == 0 && j == vars.yieldCurveModels.size() - 1;
                    if (linearTsr)
                        tol = 1.0e-7;
                    if (difference > tol)
                        BOOST_FAIL("\nCoupon payment date: "
                                   << paymentDate << "\nCoupon start date:   " << startDate << "\nCoupon gearing:      "
                                   << io::rate(gearing) << "\nCoupon swap index:   " << swapIndex->name()
                                   << "\nCoupon spread:       " << io::rate(spread) << "\nstrike:              "
                                   << io::rate(strike) << "\nCoupon DayCounter:   " << vars.iborIndex->dayCounter()
                                   << "\nYieldCurve Model:    " << vars.yieldCurveModels[j]
                                   << (k == 0 ? "\nNumerical Pricer" : "\nAnalytic Pricer")
                                   << (linearTsr ? " (Linear TSR Model)" : "") << "\nSwaplet price:       "
                                   << io::rate(swapletPrice) << "\nCaplet price:        " << io::rate(capletPrice)
                                   << "\nFloorlet price:      " << io::rate(floorletPrice) << "\ndifference:          "
                                   << difference << "\ntolerance:           " << io::rate(tol));
                }
            }
        }
    }
}

// Leaves a problem where it started, so that a calibration should report its own guess.
class StayAtGuess : public OptimizationMethod
{
  public:
    EndCriteria::Type minimize(Problem&, const EndCriteria&) override { return EndCriteria::None; }
};

// A CMS market on two of the cube's swap tenors.
struct CmsCalibrationVars
{
    CommonVars vars;
    std::vector<Period> swapLengths = {1 * Years, 5 * Years, 10 * Years, 20 * Years};
    std::vector<Period> swapTenors = {2 * Years, 10 * Years};
    ext::shared_ptr<CmsMarket> market;
    Matrix weights = Matrix(4, 2, 1.0);
    ext::shared_ptr<EndCriteria> endCriteria = ext::make_shared<EndCriteria>(20, 10, 1e-6, 1e-6, 1e-6);
    ext::shared_ptr<OptimizationMethod> stay = ext::make_shared<StayAtGuess>();

    CmsCalibrationVars()
    {
        // Calculated first, so that these tests don't depend on what
        // testSabrCubeRecalibrationBeforeFirstUse checks.
        vars.SabrVolCube1->volatility(1 * Years, 2 * Years, 0.05);

        std::vector<ext::shared_ptr<SwapIndex>> swapIndexes;
        std::vector<ext::shared_ptr<CmsCouponPricer>> pricers;
        for (const Period& tenor : swapTenors)
        {
            swapIndexes.push_back(ext::make_shared<EuriborSwapIsdaFixA>(tenor, vars.termStructure));
            // The one of the four that reads the mean reversion.
            pricers.push_back(vars.analyticPricers[3]);
        }
        std::vector<std::vector<Handle<Quote>>> bidAskSpreads(swapLengths.size());
        for (Size i = 0; i < swapLengths.size(); ++i)
        {
            for (Size j = 0; j < swapTenors.size(); ++j)
            {
                const Real spread = 0.01 + 0.002 * i + 0.001 * j;
                bidAskSpreads[i].emplace_back(ext::make_shared<SimpleQuote>(spread));
                bidAskSpreads[i].emplace_back(ext::make_shared<SimpleQuote>(spread + 0.001));
            }
        }
        market = ext::make_shared<CmsMarket>(swapLengths, swapIndexes, vars.iborIndex, bidAskSpreads, pricers,
                                             vars.termStructure);
    }

    // The market's error with these betas: one row per swap tenor, one column per swap length.
    Real errorAt(const std::vector<std::vector<Real>>& betas, Real meanReversion)
    {
        const auto cube = ext::dynamic_pointer_cast<SabrSwaptionVolatilityCube>(*vars.SabrVolCube1);
        for (Size i = 0; i < swapTenors.size(); ++i)
            cube->recalibration(swapLengths, betas[i], swapTenors[i]);
        market->reprice(vars.SabrVolCube1, meanReversion);
        return market->weightedSpreadError(weights);
    }

    // The same, with a beta term structure as a calibration reports it.
    Real errorAtTermStructure(const Matrix& parameters, Real meanReversion)
    {
        std::vector<std::vector<Real>> betas(swapTenors.size(), std::vector<Real>(swapLengths.size()));
        for (Size i = 0; i < swapTenors.size(); ++i)
        {
            for (Size j = 0; j < swapLengths.size(); ++j)
                betas[i][j] = parameters[j][i];
        }
        return errorAt(betas, meanReversion);
    }

    // The same, with a parametric beta term structure: betaInf, beta0 and decay per swap tenor.
    Real errorAtParametric(const Matrix& parameters, Real meanReversion)
    {
        const Handle<SwaptionVolatilityStructure>& cube = vars.SabrVolCube1;
        std::vector<std::vector<Real>> betas(swapTenors.size());
        for (Size i = 0; i < swapTenors.size(); ++i)
        {
            for (const Period& length : swapLengths)
            {
                const Time t = cube->timeFromReference(cube->optionDateFromTenor(length));
                betas[i].push_back(parameters[0][i] +
                                   (parameters[1][i] - parameters[0][i]) * std::exp(-parameters[2][i] * t));
            }
        }
        return errorAt(betas, meanReversion);
    }
};

BOOST_AUTO_TEST_CASE(testCmsCalibrationOnConstantBetaFromItsGuess)
{
    BOOST_TEST_MESSAGE("Testing that a CMS market calibration on constant betas starts from its guess...");

    CmsCalibrationVars data;
    CmsMarketCalibration calibration(data.vars.SabrVolCube1, data.market, data.weights, CmsMarketCalibration::OnSpread);

    const std::vector<std::vector<Real>> betas = {std::vector<Real>(4, 0.4), std::vector<Real>(4, 0.6)};

    for (bool isReversionFixed : {true, false})
    {
        BOOST_TEST_CONTEXT("mean reversion " << (isReversionFixed ? "fixed" : "free"))
        {
            Array guess(3);
            guess[0] = 0.4;
            guess[1] = 0.6;
            guess[2] = 0.01;
            const Array result = calibration.compute(data.endCriteria, data.stay, guess, isReversionFixed);
            BOOST_REQUIRE_EQUAL(result.size(), Size(3));
            for (Size i = 0; i < 3; ++i)
                BOOST_CHECK_SMALL(result[i] - guess[i], 1e-14);
            BOOST_CHECK_SMALL(data.errorAt(betas, 0.01) - calibration.error(), 1e-10);
        }
    }

    Array guess(2);
    guess[0] = 0.4;
    guess[1] = 0.6;
    const Array result = calibration.compute(data.endCriteria, data.stay, guess, true);
    BOOST_REQUIRE_EQUAL(result.size(), Size(2));
    BOOST_CHECK_SMALL(result[0] - 0.4, 1e-14);
    BOOST_CHECK_SMALL(result[1] - 0.6, 1e-14);
    BOOST_CHECK_SMALL(data.errorAt(betas, Null<Real>()) - calibration.error(), 1e-10);
}

BOOST_AUTO_TEST_CASE(testCmsCalibrationOnBetaTermStructureFromItsGuess)
{
    BOOST_TEST_MESSAGE("Testing that a CMS market calibration on a beta term structure starts from its guess...");

    CmsCalibrationVars data;
    CmsMarketCalibration calibration(data.vars.SabrVolCube1, data.market, data.weights, CmsMarketCalibration::OnSpread);

    Matrix guess(4, 2);
    for (Size j = 0; j < 4; ++j)
    {
        guess[j][0] = 0.3 + 0.05 * j;
        guess[j][1] = 0.5 + 0.05 * j;
    }

    for (bool isReversionFixed : {true, false})
    {
        BOOST_TEST_CONTEXT("mean reversion " << (isReversionFixed ? "fixed" : "free"))
        {
            const Matrix result = calibration.compute(data.endCriteria, data.stay, guess, isReversionFixed, 0.01);
            BOOST_REQUIRE_EQUAL(result.rows(), Size(4));
            BOOST_REQUIRE_EQUAL(result.columns(), Size(3));
            for (Size j = 0; j < 4; ++j)
            {
                BOOST_CHECK_SMALL(result[j][0] - guess[j][0], 1e-14);
                BOOST_CHECK_SMALL(result[j][1] - guess[j][1], 1e-14);
                BOOST_CHECK_SMALL(result[j][2] - 0.01, 1e-14);
            }
            BOOST_CHECK_SMALL(data.errorAtTermStructure(guess, 0.01) - calibration.error(), 1e-10);
        }
    }

    const Matrix result = calibration.compute(data.endCriteria, data.stay, guess, true, Null<Real>());
    BOOST_REQUIRE_EQUAL(result.columns(), Size(2));
    BOOST_CHECK_SMALL(data.errorAtTermStructure(guess, Null<Real>()) - calibration.error(), 1e-10);
}

BOOST_AUTO_TEST_CASE(testCmsCalibrationOnParametricBetaFromItsGuess)
{
    BOOST_TEST_MESSAGE("Testing that a CMS market calibration on a parametric beta starts from its guess...");

    CmsCalibrationVars data;
    CmsMarketCalibration calibration(data.vars.SabrVolCube1, data.market, data.weights, CmsMarketCalibration::OnSpread);

    Matrix guess(3, 2);
    guess[0][0] = 0.3;
    guess[1][0] = 0.4;
    guess[2][0] = 0.1;
    guess[0][1] = 0.5;
    guess[1][1] = 0.6;
    guess[2][1] = 0.2;

    for (bool isReversionFixed : {true, false})
    {
        BOOST_TEST_CONTEXT("mean reversion " << (isReversionFixed ? "fixed" : "free"))
        {
            const Matrix result =
                calibration.computeParametric(data.endCriteria, data.stay, guess, isReversionFixed, 0.01);
            BOOST_REQUIRE_EQUAL(result.rows(), Size(3));
            BOOST_REQUIRE_EQUAL(result.columns(), Size(3));
            for (Size j = 0; j < 3; ++j)
            {
                BOOST_CHECK_SMALL(result[j][0] - guess[j][0], 1e-14);
                BOOST_CHECK_SMALL(result[j][1] - guess[j][1], 1e-14);
                BOOST_CHECK_SMALL(result[j][2] - 0.01, 1e-14);
            }
            BOOST_CHECK_SMALL(data.errorAtParametric(guess, 0.01) - calibration.error(), 1e-10);
        }
    }

    const Matrix result = calibration.computeParametric(data.endCriteria, data.stay, guess, true, Null<Real>());
    BOOST_REQUIRE_EQUAL(result.columns(), Size(2));
    BOOST_CHECK_SMALL(data.errorAtParametric(guess, Null<Real>()) - calibration.error(), 1e-10);
}

BOOST_AUTO_TEST_CASE(testCmsCalibrationImprovesOnItsGuess, *precondition(if_speed(Fast)))
{
    BOOST_TEST_MESSAGE("Testing that a CMS market calibration improves on its guess...");

    CmsCalibrationVars data;
    CmsMarketCalibration calibration(data.vars.SabrVolCube1, data.market, data.weights, CmsMarketCalibration::OnSpread);

    ext::shared_ptr<OptimizationMethod> levenbergMarquardt = ext::make_shared<LevenbergMarquardt>(1e-8, 1e-8, 1e-8);
    // With a free mean reversion, a beta per quote leaves one variable more than errors.
    ext::shared_ptr<OptimizationMethod> simplex = ext::make_shared<Simplex>(0.1);

    const Matrix guess(4, 2, 0.5);
    calibration.compute(data.endCriteria, data.stay, guess, true, 0.01);
    const Real errorAtGuess = calibration.error();

    const Matrix fixedResult = calibration.compute(data.endCriteria, levenbergMarquardt, guess, true, 0.01);
    BOOST_CHECK_LT(calibration.error(), errorAtGuess);
    BOOST_CHECK_SMALL(data.errorAtTermStructure(fixedResult, 0.01) - calibration.error(), 1e-10);

    const Matrix freeResult = calibration.compute(data.endCriteria, simplex, guess, false, 0.01);
    BOOST_CHECK_LT(calibration.error(), errorAtGuess);
    BOOST_CHECK_SMALL(data.errorAtTermStructure(freeResult, freeResult[0][2]) - calibration.error(), 1e-10);

    Matrix parametricGuess(3, 2, 0.5);
    parametricGuess[2][0] = parametricGuess[2][1] = 0.1;
    calibration.computeParametric(data.endCriteria, data.stay, parametricGuess, true, 0.01);
    const Real errorAtParametricGuess = calibration.error();

    const Matrix fixedParametric =
        calibration.computeParametric(data.endCriteria, levenbergMarquardt, parametricGuess, true, 0.01);
    BOOST_CHECK_LT(calibration.error(), errorAtParametricGuess);
    BOOST_CHECK_SMALL(data.errorAtParametric(fixedParametric, 0.01) - calibration.error(), 1e-10);

    const Matrix freeParametric =
        calibration.computeParametric(data.endCriteria, levenbergMarquardt, parametricGuess, false, 0.01);
    BOOST_CHECK_LT(calibration.error(), errorAtParametricGuess);
    BOOST_CHECK_SMALL(data.errorAtParametric(freeParametric, freeParametric[0][2]) - calibration.error(), 1e-10);
}

BOOST_AUTO_TEST_CASE(testCmsCalibrationTypes)
{
    BOOST_TEST_MESSAGE("Testing the errors a CMS market calibration can work on...");

    CmsCalibrationVars data;

    Array guess(3);
    guess[0] = 0.4;
    guess[1] = 0.6;
    guess[2] = 0.01;

    const std::vector<std::vector<Real>> betas = {std::vector<Real>(4, 0.4), std::vector<Real>(4, 0.6)};

    for (CmsMarketCalibration::CalibrationType type :
         {CmsMarketCalibration::OnSpread, CmsMarketCalibration::OnPrice, CmsMarketCalibration::OnForwardCmsPrice})
    {
        CmsMarketCalibration calibration(data.vars.SabrVolCube1, data.market, data.weights, type);
        calibration.compute(data.endCriteria, data.stay, guess, false);
        data.errorAt(betas, 0.01);
        const Real expected = type == CmsMarketCalibration::OnSpread ? data.market->weightedSpreadError(data.weights) :
                              type == CmsMarketCalibration::OnPrice  ? data.market->weightedSpotNpvError(data.weights) :
                                                                       data.market->weightedFwdNpvError(data.weights);
        BOOST_CHECK_SMALL(calibration.error() - expected, 1e-10);
    }
}

BOOST_AUTO_TEST_CASE(testCmsCalibrationRefusals)
{
    BOOST_TEST_MESSAGE("Testing what a CMS market calibration refuses...");

    CmsCalibrationVars data;
    Handle<SwaptionVolatilityStructure>& cube = data.vars.SabrVolCube1;

    {
        ExpectedErrorMessage expected("weights number of rows");
        BOOST_CHECK_EXCEPTION(
            CmsMarketCalibration(cube, data.market, Matrix(3, 2, 1.0), CmsMarketCalibration::OnSpread), Error,
            expected);
    }
    {
        ExpectedErrorMessage expected("weights number of columns");
        BOOST_CHECK_EXCEPTION(
            CmsMarketCalibration(cube, data.market, Matrix(4, 1, 1.0), CmsMarketCalibration::OnSpread), Error,
            expected);
    }

    CmsMarketCalibration calibration(cube, data.market, data.weights, CmsMarketCalibration::OnSpread);
    {
        ExpectedErrorMessage expected("if mean reversion is not fixed, a guess must be provided");
        BOOST_CHECK_EXCEPTION(calibration.compute(data.endCriteria, data.stay, Array(2, 0.5), false), Error, expected);
        BOOST_CHECK_EXCEPTION(calibration.compute(data.endCriteria, data.stay, Matrix(4, 2, 0.5), false, Null<Real>()),
                              Error, expected);
        BOOST_CHECK_EXCEPTION(
            calibration.computeParametric(data.endCriteria, data.stay, Matrix(3, 2, 0.5), false, Null<Real>()), Error,
            expected);
    }
    {
        ExpectedErrorMessage expected("guess size (1)");
        BOOST_CHECK_EXCEPTION(calibration.compute(data.endCriteria, data.stay, Array(1, 0.5), true), Error, expected);
    }
    {
        ExpectedErrorMessage expected("number of swap lengths (4)");
        BOOST_CHECK_EXCEPTION(calibration.compute(data.endCriteria, data.stay, Matrix(5, 2, 0.5), true, 0.01), Error,
                              expected);
    }
    {
        ExpectedErrorMessage expected("number of swap tenors (2)");
        BOOST_CHECK_EXCEPTION(calibration.compute(data.endCriteria, data.stay, Matrix(4, 1, 0.5), true, 0.01), Error,
                              expected);
        BOOST_CHECK_EXCEPTION(calibration.computeParametric(data.endCriteria, data.stay, Matrix(3, 1, 0.5), true, 0.01),
                              Error, expected);
    }
    {
        ExpectedErrorMessage expected("number of parameters (3)");
        BOOST_CHECK_EXCEPTION(calibration.computeParametric(data.endCriteria, data.stay, Matrix(2, 2, 0.5), true, 0.01),
                              Error, expected);
    }
}

BOOST_AUTO_TEST_CASE(testSabrCubeRecalibrationBeforeFirstUse)
{
    BOOST_TEST_MESSAGE("Testing a SABR cube's recalibration before anything has calculated it...");

    CommonVars vars;
    const auto cube = ext::dynamic_pointer_cast<SabrSwaptionVolatilityCube>(*vars.SabrVolCube1);

    cube->recalibration(std::vector<Real>(cube->optionTenors().size(), 0.5), cube->swapTenors().front());

    BOOST_CHECK(std::isfinite(cube->volatility(1 * Years, cube->swapTenors().front(), 0.05)));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
