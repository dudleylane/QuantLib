/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Marco Bianchetti
 Copyright (C) 2007 Giorgio Facchinetti
 Copyright (C) 2006 Chiara Fornarola
 Copyright (C) 2005 StatPro Italia srl
 Copyright (C) 2013 Peter Caspers

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
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/discretizedasset.hpp>
#include <ql/experimental/shortrate/generalizedhullwhite.hpp>
#include <ql/experimental/shortrate/generalizedornsteinuhlenbeckprocess.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/math/expm1.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/optimization/simplex.hpp>
#include <ql/methods/lattices/trinomialtree.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/models/shortrate/onefactormodels/blackkarasinski.hpp>
#include <ql/models/shortrate/onefactormodels/coxingersollross.hpp>
#include <ql/models/shortrate/onefactormodels/extendedcoxingersollross.hpp>
#include <ql/models/shortrate/onefactormodels/hullwhite.hpp>
#include <ql/models/shortrate/onefactormodels/vasicek.hpp>
#include <ql/models/shortrate/twofactormodels/g2.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swap/treeswapengine.hpp>
#include <ql/pricingengines/swaption/g2swaptionengine.hpp>
#include <ql/pricingengines/swaption/jamshidianswaptionengine.hpp>
#include <ql/processes/g2process.hpp>
#include <ql/processes/hullwhiteprocess.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/schedule.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ShortRateModelTests)

struct CalibrationData
{
    Integer start;
    Integer length;
    Volatility volatility;
};


BOOST_AUTO_TEST_CASE(testCachedHullWhite)
{
    BOOST_TEST_MESSAGE("Testing Hull-White calibration against cached values using swaptions with start delay...");

    bool usingAtParCoupons = IborCoupon::Settings::instance().usingAtParCoupons();

    Date today(15, February, 2002);
    Date settlement(19, February, 2002);
    Settings::instance().evaluationDate() = today;
    Handle<YieldTermStructure> termStructure(flatRate(settlement, 0.04875825, Actual365Fixed()));
    ext::shared_ptr<HullWhite> model(new HullWhite(termStructure));
    CalibrationData data[] = {{1, 5, 0.1148}, {2, 4, 0.1108}, {3, 3, 0.1070}, {4, 2, 0.1021}, {5, 1, 0.1000}};
    ext::shared_ptr<IborIndex> index(new Euribor6M(termStructure));

    ext::shared_ptr<PricingEngine> engine(new JamshidianSwaptionEngine(model));

    std::vector<ext::shared_ptr<CalibrationHelper>> swaptions;
    for (auto& i : data)
    {
        ext::shared_ptr<Quote> vol(new SimpleQuote(i.volatility));
        ext::shared_ptr<BlackCalibrationHelper> helper(
            new SwaptionHelper(Period(i.start, Years), Period(i.length, Years), Handle<Quote>(vol), index,
                               Period(1, Years), Thirty360(Thirty360::BondBasis), Actual360(), termStructure));
        helper->setPricingEngine(engine);
        swaptions.push_back(helper);
    }

    // Set up the optimization problem
    // Real simplexLambda = 0.1;
    // Simplex optimizationMethod(simplexLambda);
    LevenbergMarquardt optimizationMethod(1.0e-8, 1.0e-8, 1.0e-8);
    EndCriteria endCriteria(10000, 100, 1e-6, 1e-8, 1e-8);

    // Optimize
    model->calibrate(swaptions, optimizationMethod, endCriteria);
    EndCriteria::Type ecType = model->endCriteria();

    // Check and print out results
    Real cachedA, cachedSigma;
    if (!usingAtParCoupons)
    {
        cachedA = 0.0463679, cachedSigma = 0.00579831;
    }
    else
    {
        cachedA = 0.0464041, cachedSigma = 0.00579912;
    }

    Real tolerance = 1.3e-5;
    Array xMinCalculated = model->params();
    Real yMinCalculated = model->value(xMinCalculated, swaptions);
    Array xMinExpected(2);
    xMinExpected[0] = cachedA;
    xMinExpected[1] = cachedSigma;
    Real yMinExpected = model->value(xMinExpected, swaptions);
    if (std::fabs(xMinCalculated[0] - cachedA) > tolerance || std::fabs(xMinCalculated[1] - cachedSigma) > tolerance)
    {
        BOOST_ERROR("Failed to reproduce cached calibration results:\n"
                    << "calculated: a = " << xMinCalculated[0] << ", "
                    << "sigma = " << xMinCalculated[1] << ", "
                    << "f(a) = " << yMinCalculated << ",\n"
                    << "expected:   a = " << xMinExpected[0] << ", "
                    << "sigma = " << xMinExpected[1] << ", "
                    << "f(a) = " << yMinExpected << ",\n"
                    << "difference: a = " << xMinCalculated[0] - xMinExpected[0] << ", "
                    << "sigma = " << xMinCalculated[1] - xMinExpected[1] << ", "
                    << "f(a) = " << yMinCalculated - yMinExpected << ",\n"
                    << "end criteria = " << ecType);
    }
}

BOOST_AUTO_TEST_CASE(testCachedHullWhiteFixedReversion)
{
    BOOST_TEST_MESSAGE("Testing Hull-White calibration with fixed reversion against cached values...");

    bool usingAtParCoupons = IborCoupon::Settings::instance().usingAtParCoupons();

    Date today(15, February, 2002);
    Date settlement(19, February, 2002);
    Settings::instance().evaluationDate() = today;
    Handle<YieldTermStructure> termStructure(flatRate(settlement, 0.04875825, Actual365Fixed()));
    ext::shared_ptr<HullWhite> model(new HullWhite(termStructure, 0.05, 0.01));
    CalibrationData data[] = {{1, 5, 0.1148}, {2, 4, 0.1108}, {3, 3, 0.1070}, {4, 2, 0.1021}, {5, 1, 0.1000}};
    ext::shared_ptr<IborIndex> index(new Euribor6M(termStructure));

    ext::shared_ptr<PricingEngine> engine(new JamshidianSwaptionEngine(model));

    std::vector<ext::shared_ptr<CalibrationHelper>> swaptions;
    for (auto& i : data)
    {
        ext::shared_ptr<Quote> vol(new SimpleQuote(i.volatility));
        ext::shared_ptr<BlackCalibrationHelper> helper(
            new SwaptionHelper(Period(i.start, Years), Period(i.length, Years), Handle<Quote>(vol), index,
                               Period(1, Years), Thirty360(Thirty360::BondBasis), Actual360(), termStructure));
        helper->setPricingEngine(engine);
        swaptions.push_back(helper);
    }

    // Set up the optimization problem
    // Real simplexLambda = 0.1;
    // Simplex optimizationMethod(simplexLambda);
    LevenbergMarquardt optimizationMethod; //(1.0e-18,1.0e-18,1.0e-18);
    EndCriteria endCriteria(1000, 500, 1E-8, 1E-8, 1E-8);

    // Optimize
    model->calibrate(swaptions, optimizationMethod, endCriteria, Constraint(), std::vector<Real>(),
                     HullWhite::FixedReversion());
    EndCriteria::Type ecType = model->endCriteria();

    // Check and print out results
    Real cachedA, cachedSigma;
    if (!usingAtParCoupons)
    {
        cachedA = 0.05, cachedSigma = 0.00585835;
    }
    else
    {
        cachedA = 0.05, cachedSigma = 0.00585858;
    }

    Real tolerance = 1.0e-5;
    Array xMinCalculated = model->params();
    Real yMinCalculated = model->value(xMinCalculated, swaptions);
    Array xMinExpected(2);
    xMinExpected[0] = cachedA;
    xMinExpected[1] = cachedSigma;
    Real yMinExpected = model->value(xMinExpected, swaptions);
    if (std::fabs(xMinCalculated[0] - cachedA) > tolerance || std::fabs(xMinCalculated[1] - cachedSigma) > tolerance)
    {
        BOOST_ERROR("Failed to reproduce cached calibration results:\n"
                    << "calculated: a = " << xMinCalculated[0] << ", "
                    << "sigma = " << xMinCalculated[1] << ", "
                    << "f(a) = " << yMinCalculated << ",\n"
                    << "expected:   a = " << xMinExpected[0] << ", "
                    << "sigma = " << xMinExpected[1] << ", "
                    << "f(a) = " << yMinExpected << ",\n"
                    << "difference: a = " << xMinCalculated[0] - xMinExpected[0] << ", "
                    << "sigma = " << xMinCalculated[1] - xMinExpected[1] << ", "
                    << "f(a) = " << yMinCalculated - yMinExpected << ",\n"
                    << "end criteria = " << ecType);
    }
}

BOOST_AUTO_TEST_CASE(testCachedHullWhite2)
{
    BOOST_TEST_MESSAGE("Testing Hull-White calibration against cached "
                       "values using swaptions without start delay...");

    bool usingAtParCoupons = IborCoupon::Settings::instance().usingAtParCoupons();

    Date today(15, February, 2002);
    Date settlement(19, February, 2002);
    Settings::instance().evaluationDate() = today;
    Handle<YieldTermStructure> termStructure(flatRate(settlement, 0.04875825, Actual365Fixed()));
    ext::shared_ptr<HullWhite> model(new HullWhite(termStructure));
    CalibrationData data[] = {{1, 5, 0.1148}, {2, 4, 0.1108}, {3, 3, 0.1070}, {4, 2, 0.1021}, {5, 1, 0.1000}};
    ext::shared_ptr<IborIndex> index(new Euribor6M(termStructure));
    ext::shared_ptr<IborIndex> index0(new IborIndex(index->familyName(), index->tenor(), 0, index->currency(),
                                                    index->fixingCalendar(), index->businessDayConvention(),
                                                    index->endOfMonth(), index->dayCounter(),
                                                    termStructure)); // Euribor 6m with zero fixing days

    ext::shared_ptr<PricingEngine> engine(new JamshidianSwaptionEngine(model));

    std::vector<ext::shared_ptr<CalibrationHelper>> swaptions;
    for (auto& i : data)
    {
        ext::shared_ptr<Quote> vol(new SimpleQuote(i.volatility));
        ext::shared_ptr<BlackCalibrationHelper> helper(
            new SwaptionHelper(Period(i.start, Years), Period(i.length, Years), Handle<Quote>(vol), index0,
                               Period(1, Years), Thirty360(Thirty360::BondBasis), Actual360(), termStructure));
        helper->setPricingEngine(engine);
        swaptions.push_back(helper);
    }

    // Set up the optimization problem
    // Real simplexLambda = 0.1;
    // Simplex optimizationMethod(simplexLambda);
    LevenbergMarquardt optimizationMethod(1.0e-8, 1.0e-8, 1.0e-8);
    EndCriteria endCriteria(10000, 100, 1e-6, 1e-8, 1e-8);

    // Optimize
    model->calibrate(swaptions, optimizationMethod, endCriteria);
    EndCriteria::Type ecType = model->endCriteria();

    // Check and print out results
    // The cached values were produced with an older version of the
    // JamshidianEngine not accounting for the delay between option
    // expiry and underlying start
    Real cachedA, cachedSigma;
    if (!usingAtParCoupons)
        cachedA = 0.0481608, cachedSigma = 0.00582493;
    else
        cachedA = 0.0482063, cachedSigma = 0.00582687;

    Real tolerance = 1.0e-5;
    Array xMinCalculated = model->params();
    Real yMinCalculated = model->value(xMinCalculated, swaptions);
    Array xMinExpected(2);
    xMinExpected[0] = cachedA;
    xMinExpected[1] = cachedSigma;
    Real yMinExpected = model->value(xMinExpected, swaptions);
    if (std::fabs(xMinCalculated[0] - cachedA) > tolerance || std::fabs(xMinCalculated[1] - cachedSigma) > tolerance)
    {
        BOOST_ERROR("Failed to reproduce cached calibration results:\n"
                    << "calculated: a = " << xMinCalculated[0] << ", "
                    << "sigma = " << xMinCalculated[1] << ", "
                    << "f(a) = " << yMinCalculated << ",\n"
                    << "expected:   a = " << xMinExpected[0] << ", "
                    << "sigma = " << xMinExpected[1] << ", "
                    << "f(a) = " << yMinExpected << ",\n"
                    << "difference: a = " << xMinCalculated[0] - xMinExpected[0] << ", "
                    << "sigma = " << xMinCalculated[1] - xMinExpected[1] << ", "
                    << "f(a) = " << yMinCalculated - yMinExpected << ",\n"
                    << "end criteria = " << ecType);
    }
}

BOOST_AUTO_TEST_CASE(testSwaps)
{
    BOOST_TEST_MESSAGE("Testing Hull-White swap pricing against known values...");

    bool usingAtParCoupons = IborCoupon::Settings::instance().usingAtParCoupons();

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    today = calendar.adjust(today);
    Settings::instance().evaluationDate() = today;

    Date settlement = calendar.advance(today, 2, Days);

    std::vector<Date> dates = {settlement,
                               calendar.advance(settlement, 1, Weeks),
                               calendar.advance(settlement, 1, Months),
                               calendar.advance(settlement, 3, Months),
                               calendar.advance(settlement, 6, Months),
                               calendar.advance(settlement, 9, Months),
                               calendar.advance(settlement, 1, Years),
                               calendar.advance(settlement, 2, Years),
                               calendar.advance(settlement, 3, Years),
                               calendar.advance(settlement, 5, Years),
                               calendar.advance(settlement, 10, Years),
                               calendar.advance(settlement, 15, Years)};
    std::vector<DiscountFactor> discounts = {1.0,      0.999258, 0.996704, 0.990809, 0.981798, 0.972570,
                                             0.963430, 0.929532, 0.889267, 0.803693, 0.596903, 0.433022};

    Handle<YieldTermStructure> termStructure(
        ext::shared_ptr<YieldTermStructure>(new DiscountCurve(dates, discounts, Actual365Fixed())));

    ext::shared_ptr<HullWhite> model(new HullWhite(termStructure));

    Integer start[] = {-3, 0, 3};
    Integer length[] = {2, 5, 10};
    Rate rates[] = {0.02, 0.04, 0.06};
    ext::shared_ptr<IborIndex> euribor(new Euribor6M(termStructure));

    ext::shared_ptr<PricingEngine> engine(new TreeVanillaSwapEngine(model, 120));

    Real tolerance = usingAtParCoupons ? 1.0e-8 : 4.0e-3;

    for (Size i = 0; i < std::size(start); i++)
    {

        Date startDate = calendar.advance(settlement, start[i], Months);
        if (startDate < today)
        {
            Date fixingDate = calendar.advance(startDate, -2, Days);
            euribor->addFixing(fixingDate, 0.03);
        }

        for (Size j = 0; j < std::size(length); j++)
        {

            Date maturity = calendar.advance(startDate, length[i], Years);
            Schedule fixedSchedule(startDate, maturity, Period(Annual), calendar, Unadjusted, Unadjusted,
                                   DateGeneration::Forward, false);
            Schedule floatSchedule(startDate, maturity, Period(Semiannual), calendar, Following, Following,
                                   DateGeneration::Forward, false);
            for (Real rate : rates)
            {

                VanillaSwap swap(Swap::Payer, 1000000.0, fixedSchedule, rate, Thirty360(Thirty360::BondBasis),
                                 floatSchedule, euribor, 0.0, Actual360());
                swap.setPricingEngine(ext::shared_ptr<PricingEngine>(new DiscountingSwapEngine(termStructure)));
                Real expected = swap.NPV();
                swap.setPricingEngine(engine);
                Real calculated = swap.NPV();

                Real error = std::fabs((expected - calculated) / expected);
                if (error > tolerance)
                {
                    BOOST_ERROR("Failed to reproduce swap NPV:" << std::fixed << std::setprecision(9)
                                                                << "\n    calculated: " << calculated
                                                                << "\n    expected:   " << expected << std::scientific
                                                                << "\n    rel. error: " << error);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testFuturesConvexityBias)
{
    BOOST_TEST_MESSAGE("Testing Hull-White futures convexity bias...");

    // G. Kirikos, D. Novak, "Convexity Conundrums", Risk Magazine, March 1997
    Real futureQuote = 94.0;
    Real sigma = 0.015;
    Time t = 5.0;
    Real tolerance = 0.0000001;

    for (auto [T, a, expectedForward] : {
             std::tuple{5.25, 0.03, 0.0573037},
             std::tuple{5.25, 1e-4, 0.0568627},
             std::tuple{5.25, 0.0, 0.0568611},
             std::tuple{5.001, 0.03, 0.0575736},
             std::tuple{5.0, 0.03, 0.0575747},
         })
    {
        Rate futureImpliedRate = (100.0 - futureQuote) / 100.0;
        Rate calculatedForward = futureImpliedRate - HullWhite::convexityBias(futureQuote, t, T, sigma, a);

        Real error = std::fabs(calculatedForward - expectedForward);
        if (!(error < tolerance))
        {
            BOOST_ERROR("Failed to reproduce convexity bias:"
                        << "\ncalculated: " << calculatedForward << "\n  expected: " << expectedForward
                        << std::scientific << "\n     error: " << error << "\n tolerance: " << tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testExtendedCoxIngersollRossDiscountFactor)
{
    BOOST_TEST_MESSAGE("Testing zero-bond pricing for extended CIR model...");

    const Date today = Settings::instance().evaluationDate();

    const Rate rate = 0.1;
    const Handle<YieldTermStructure> rTS(flatRate(today, rate, Actual365Fixed()));

    const Time now = 1.5;
    const Time maturity = 2.5;

    const ExtendedCoxIngersollRoss cirModel(rTS, rate, 1.0, 1e-4, rate);

    const Real expected = rTS->discount(maturity) / rTS->discount(now);
    const Real calculated = cirModel.discountBond(now, maturity, rate);

    const Real tol = 1e-6;
    const Real diff = std::fabs(expected - calculated);

    if (diff > tol)
    {
        BOOST_ERROR("Failed to reproduce zero bound price:" << "\n  calculated: " << calculated
                                                            << "\n  expected  : " << expected << std::scientific
                                                            << "\n  difference: " << diff << "\n  tolerance : " << tol);
    }
}
BOOST_AUTO_TEST_CASE(testHullWhiteR0FollowsTheCurve)
{
    BOOST_TEST_MESSAGE("Testing Hull-White's r0 when the term structure is relinked...");

    const Date today(19, May, 2026);
    Settings::instance().evaluationDate() = today;

    RelinkableHandle<YieldTermStructure> curve;
    curve.linkTo(flatRate(today, 0.02, Actual365Fixed()));
    const HullWhite model(curve);

    curve.linkTo(flatRate(today, 0.05, Actual365Fixed()));

    const Rate expected = curve->forwardRate(0.0, 0.0, Continuous, NoFrequency);
    BOOST_CHECK_SMALL(model.r0() - expected, 1.0e-12);
    BOOST_CHECK_SMALL(static_cast<const Vasicek&>(model).r0() - expected, 1.0e-12);
}

BOOST_AUTO_TEST_CASE(testHullWhiteLeavesItsCurveLazy)
{
    BOOST_TEST_MESSAGE("Testing that a Hull-White model doesn't recalculate its curve when notified...");

    const Date today(19, May, 2026);
    Settings::instance().evaluationDate() = today;

    auto quote = ext::make_shared<SimpleQuote>(0.03);
    std::vector<ext::shared_ptr<RateHelper>> helpers;
    for (Integer months : {1, 6, 12})
        helpers.push_back(ext::make_shared<DepositRateHelper>(Handle<Quote>(quote), months * Months, 2, TARGET(),
                                                              ModifiedFollowing, false, Actual360()));
    Handle<YieldTermStructure> curve(
        ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear>>(today, helpers, Actual365Fixed()));
    const HullWhite model(curve);

    // A curve can't be bootstrapped from this, which is news for whoever asks
    // for it next, and not for whoever happens to set the quote.
    BOOST_CHECK_NO_THROW(quote->setValue(-500.0));
    BOOST_CHECK_THROW(model.r0(), Error);

    quote->setValue(0.04);
    BOOST_CHECK_SMALL(model.r0() - curve->forwardRate(0.0, 0.0, Continuous, NoFrequency), 1.0e-12);
}

BOOST_AUTO_TEST_CASE(testVasicekDiscountBondForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing Vasicek's discount bonds with mean reversion close to zero...");

    struct Case
    {
        Rate r0;
        Real a, b;
        Volatility sigma;
        Real lambda;
        Time maturity;
        Real expected;
    };

    // The model's closed form, evaluated in 120-digit arithmetic.
    const Case cases[] = {
        {0.03, 1e-12, 0.03, 0.01, 0.0, 5.0, 0.86250298719625288},
        {0.03, 1e-09, 0.03, 0.01, 0.0, 5.0, 0.86250298718952131},
        {0.03, 1.5e-08, 0.03, 0.01, 0.0, 5.0, 0.86250298709518505},
        {0.03, 2e-08, 0.03, 0.01, 0.0, 5.0, 0.86250298706149353},
        {0.03, 1e-07, 0.03, 0.01, 0.0, 5.0, 0.86250298652242932},
        {0.03, 1e-06, 0.03, 0.01, 0.0, 5.0, 0.86250298045797078},
        {0.03, 1e-05, 0.03, 0.01, 0.0, 5.0, 0.86250291981478862},
        {0.03, 0.0001, 0.03, 0.01, 0.0, 5.0, 0.862502313523263},
        {0.03, 0.001, 0.03, 0.01, 0.0, 5.0, 0.86249626461254694},
        {0.03, 0.05, 0.03, 0.01, 0.0, 5.0, 0.8622021651453047},
        {0.03, 0.4, 0.03, 0.01, 0.0, 5.0, 0.86122019135105516},
        {0.03, 2.0, 0.03, 0.01, 0.0, 5.0, 0.8607537032393716},
        {0.05, 1e-12, 0.04, 0.02, -0.3, 10.0, 0.87517331904207225},
        {0.05, 1e-09, 0.04, 0.02, -0.3, 10.0, 0.87517331816777411},
        {0.05, 1.5e-08, 0.04, 0.02, -0.3, 10.0, 0.87517330591534836},
        {0.05, 2e-08, 0.04, 0.02, -0.3, 10.0, 0.87517330153948233},
        {0.05, 1e-07, 0.04, 0.02, -0.3, 10.0, 0.87517323152564761},
        {0.05, 1e-06, 0.04, 0.02, -0.3, 10.0, 0.87517244387283734},
        {0.05, 1e-05, 0.04, 0.02, -0.3, 10.0, 0.87516456763064294},
        {0.05, 0.0001, 0.04, 0.02, -0.3, 10.0, 0.87508583378979537},
        {0.05, 0.001, 0.04, 0.02, -0.3, 10.0, 0.87430134378833068},
        {0.05, 0.05, 0.04, 0.02, -0.3, 10.0, 0.83826103733717051},
        {0.05, 0.4, 0.04, 0.02, -0.3, 10.0, 0.73828243526215487},
        {0.05, 2.0, 0.04, 0.02, -0.3, 10.0, 0.68657658594684234},
    };

    const Real tolerance = 1.0e-14;
    for (const auto& c : cases)
    {
        const Vasicek model(c.r0, c.a, c.b, c.sigma, c.lambda);
        const Real calculated = model.discountBond(0.0, c.maturity, c.r0);
        const Real error = std::fabs(calculated / c.expected - 1.0);
        if (error > tolerance)
            BOOST_ERROR("failed to reproduce the discount bond:"
                        << "\n  a:          " << c.a << "\n  maturity:   " << c.maturity << std::setprecision(17)
                        << "\n  calculated: " << calculated << "\n  expected:   " << c.expected);
    }
}

BOOST_AUTO_TEST_CASE(testVasicekBondOptionForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing Vasicek's bond options with mean reversion close to zero...");

    const Rate r0 = 0.05;
    const Real b = 0.05, lambda = 0.1, strike = 0.9;
    const Volatility sigma = 0.02;
    const Time maturity = 2.0, bondMaturity = 7.0;

    struct Case
    {
        Real a, call, put;
    };

    // The model's closed form, evaluated in 120-digit arithmetic.
    const Case cases[] = {
        {1e-12, 0.006120869238960353, 0.13114554827073832}, {1e-09, 0.0061208691653279077, 0.13114554820253985},
        {1e-07, 0.0061208618684224447, 0.1311455414441356}, {1e-05, 0.006120132213223885, 0.13114486562595755},
        {0.1, 0.001556016960097005, 0.12618440799746223},
    };

    const Real tolerance = 1.0e-13;
    for (const auto& c : cases)
    {
        const Vasicek model(r0, c.a, b, sigma, lambda);
        const Real call = model.discountBondOption(Option::Call, strike, maturity, bondMaturity);
        const Real put = model.discountBondOption(Option::Put, strike, maturity, bondMaturity);
        if (std::fabs(call - c.call) > tolerance || std::fabs(put - c.put) > tolerance)
            BOOST_ERROR("failed to reproduce the bond options:" << "\n  a:    " << c.a << std::setprecision(17)
                                                                << "\n  call: " << call << " against " << c.call
                                                                << "\n  put:  " << put << " against " << c.put);
    }
}

BOOST_AUTO_TEST_CASE(testHullWhiteBondOptionsForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing Hull-White's bond options with mean reversion close to zero...");

    // A flat 3% curve and sigma = 1%; calls struck at 0.9, with a 5-year maturity, on a
    // zero-coupon bond running from 5 to 10 years, and from 7 to 10 years for the forward start.
    const Date today(6, October, 2026);
    const Handle<YieldTermStructure> curve(flatRate(today, 0.03, Actual365Fixed()));
    const Volatility sigma = 0.01;

    struct Case
    {
        Real a, spotStart, forwardStart;
    };

    // The model's closed form, evaluated in 120-digit arithmetic.
    const Case cases[] = {
        {1e-16, 0.019523877439481272, 0.025830331691153781}, {1e-12, 0.019523877439325531, 0.025830331691038868},
        {1e-10, 0.019523877423905679, 0.025830331679661309}, {1e-08, 0.019523875881920469, 0.025830330541905427},
        {1e-07, 0.019523861863878036, 0.025830320198673814}, {1e-06, 0.019523721683941603, 0.025830216766722602},
        {1e-05, 0.01952231993336683, 0.025829182483700516},  {0.0001, 0.019508307305488621, 0.025818843301606947},
        {0.001, 0.019368667730306146, 0.025715815418306644}, {0.01, 0.018019878169050826, 0.024721072199122754},
        {0.1, 0.0083936019065510994, 0.017614510032769543},
    };

    const Real tolerance = 1.0e-13;
    for (const auto& c : cases)
    {
        const HullWhite model(curve, c.a, sigma);
        const Real spotStart = model.discountBondOption(Option::Call, 0.9, 5.0, 10.0);
        const Real forwardStart = model.discountBondOption(Option::Call, 0.9, 5.0, 7.0, 10.0);
        if (std::fabs(spotStart - c.spotStart) > tolerance || std::fabs(forwardStart - c.forwardStart) > tolerance)
            BOOST_ERROR("failed to reproduce the bond options:" << "\n  a:             " << c.a << std::setprecision(17)
                                                                << "\n  spot start:    " << spotStart << " against "
                                                                << c.spotStart << "\n  forward start: " << forwardStart
                                                                << " against " << c.forwardStart);
    }
}

BOOST_AUTO_TEST_CASE(testG2DiscountBondForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing G2's discount bonds with mean reversion close to zero...");

    struct Case
    {
        Rate r;
        Real a;
        Volatility sigma;
        Real b;
        Volatility eta;
        Real rho;
        Time t, T;
        Real x, y, expected;
    };

    // The model's closed form, evaluated in 120-digit arithmetic.
    const Case cases[] = {
        {0.03, 1e-12, 0.01, 0.1, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88003659065225545},
        {0.03, 1e-09, 0.01, 0.1, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88003659072582035},
        {0.03, 1e-07, 0.01, 0.1, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88003659801603423},
        {0.03, 1e-05, 0.01, 0.1, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88003732702749102},
        {0.03, 0.001, 0.01, 0.1, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88011012895729353},
        {0.03, 0.1, 0.01, 1e-09, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.89274066161481982},
        {0.03, 0.1, 0.01, 1e-05, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.89273994952677022},
        {0.03, 1e-07, 0.01, 2e-07, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88625548898935952},
        {0.03, 1e-06, 0.01, 1e-06, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.8862554988312159},
        {0.03, 0.2, 0.01, 0.3, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88288328570041449},
        {0.03, 0.45, 0.01, 0.04, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.90323235067173138},
        {0.03, 1.0, 0.01, 0.03, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.91184613030830664},
        {0.03, 3.0, 0.01, 5.0, 0.005, -0.5, 1.0, 5.0, 0.01, -0.01, 0.88573682792113109},
        {0.05, 1e-06, 0.02, 0.05, 0.01, 0.8, 0.5, 10.0, -0.02, 0.015, 0.6600529015892483},
        {0.05, 0.03, 0.02, 1e-07, 0.01, -0.9, 2.0, 30.0, 0.0, 0.0, 0.23888590049897576},
    };

    const Date today(19, May, 2026);
    const Real tolerance = 1.0e-14;
    for (const auto& c : cases)
    {
        const Handle<YieldTermStructure> curve(flatRate(today, c.r, Actual365Fixed()));
        const G2 model(curve, c.a, c.sigma, c.b, c.eta, c.rho);
        const Real calculated = model.discountBond(c.t, c.T, c.x, c.y);
        const Real error = std::fabs(calculated / c.expected - 1.0);
        if (error > tolerance)
            BOOST_ERROR("failed to reproduce the discount bond:" << "\n  a, b:       " << c.a << ", " << c.b
                                                                 << std::setprecision(17) << "\n  calculated: "
                                                                 << calculated << "\n  expected:   " << c.expected);
    }
}

BOOST_AUTO_TEST_CASE(testG2BondOptionForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing G2's bond options with mean reversion close to zero...");

    struct Case
    {
        Real a, b, call;
    };

    // The model's closed form, evaluated in 120-digit arithmetic.
    const Case cases[] = {
        {1e-12, 0.1, 0.0071177351427567712}, {1e-09, 0.1, 0.0071177350835808549},   {1e-07, 0.1, 0.0071177292193028407},
        {0.1, 1e-09, 0.0031411825321220064}, {1e-08, 1e-08, 0.0069013899019556634}, {0.05, 0.1, 0.0046187790906571704},
    };

    const Date today(19, May, 2026);
    const Handle<YieldTermStructure> curve(flatRate(today, 0.03, Actual365Fixed()));
    const Real tolerance = 1.0e-13;
    for (const auto& c : cases)
    {
        const G2 model(curve, c.a, 0.01, c.b, 0.005, -0.5);
        const Real calculated = model.discountBondOption(Option::Call, 0.9, 2.0, 7.0);
        if (std::fabs(calculated - c.call) > tolerance)
            BOOST_ERROR("failed to reproduce the bond option:" << "\n  a, b:       " << c.a << ", " << c.b
                                                               << std::setprecision(17) << "\n  calculated: "
                                                               << calculated << "\n  expected:   " << c.call);
    }
}

BOOST_AUTO_TEST_CASE(testG2SwaptionForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing G2's swaptions with mean reversion close to zero...");

    const Date today(19, May, 2026);
    Settings::instance().evaluationDate() = today;
    const Handle<YieldTermStructure> curve(flatRate(today, 0.03, Actual365Fixed()));
    const auto swapIndex = ext::make_shared<EuriborSwapIsdaFixA>(5 * Years, curve);

    const auto npv = [&](Real a)
    {
        const auto model = ext::make_shared<G2>(curve, a, 0.01, 0.1, 0.005, -0.5);
        const ext::shared_ptr<Swaption> swaption =
            MakeSwaption(swapIndex, 2 * Years, 0.03)
                .withPricingEngine(ext::make_shared<G2SwaptionEngine>(model, 6.0, 64));
        return swaption->NPV();
    };

    // The usual expressions of the swaption's terms don't cancel at this reversion,
    // and this is what they give.
    BOOST_CHECK_SMALL(npv(0.05) - 0.019439987717127, 1.0e-12);

    // The price moves smoothly towards its limit, by about 0.08 per unit of a.
    const Real limit = npv(1.0e-12);
    for (Real a : {1.0e-10, 1.0e-9, 1.0e-8, 1.0e-7, 1.0e-6, 1.0e-5})
    {
        const Real calculated = npv(a);
        if (std::fabs(calculated - limit) > 0.1 * a + 1.0e-9)
            BOOST_ERROR("swaption price not converging as the reversion goes to zero:"
                        << "\n  a:          " << a << std::setprecision(17) << "\n  calculated: " << calculated
                        << "\n  limit:      " << limit);
    }
}

BOOST_AUTO_TEST_CASE(testHullWhiteProcessesForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing the Hull-White processes with mean reversion close to zero...");

    // A flat 3% curve and sigma = 1%, stepping from 1 to 2 years under the 10-year forward measure.
    const Date today(6, October, 2026);
    const Handle<YieldTermStructure> curve(flatRate(today, 0.03, Actual365Fixed()));
    const Volatility sigma = 0.01;
    const Time T = 10.0;

    struct Case
    {
        // B(2,10); what alpha adds to the forward at 1 and 2; M_T(1,2,10); what the drift adds at 2; a year's variance
        Real a, B, A1, A2, M, D, variance;
    };

    // The closed forms, evaluated in 120-digit arithmetic.
    const Case cases[] = {
        {0.0, 8, 5.0000000000000002e-05, 0.00020000000000000001, 0.00084999999999999995, 0.00020000000000000001,
         0.0001},
        {1e-16, 7.9999999999999964, 4.9999999999999996e-05, 0.00019999999999999996, 0.00084999999999999963,
         0.00019999999999999996, 9.9999999999999991e-05},
        {1e-15, 7.999999999999968, 4.9999999999999948e-05, 0.0001999999999999996, 0.00084999999999999594,
         0.0001999999999999996, 9.9999999999999896e-05},
        {1e-14, 7.9999999999996803, 4.9999999999999501e-05, 0.000199999999999996, 0.00084999999999995951,
         0.000199999999999996, 9.9999999999999002e-05},
        {1e-13, 7.9999999999967999, 4.9999999999995002e-05, 0.00019999999999996, 0.000849999999999595,
         0.00019999999999996, 9.9999999999990003e-05},
        {1e-12, 7.9999999999679998, 4.999999999995e-05, 0.00019999999999959999, 0.00084999999999595002,
         0.00019999999999959999, 9.9999999999900001e-05},
        {1e-11, 7.99999999968, 4.9999999999500002e-05, 0.00019999999999600001, 0.00084999999995950001,
         0.00019999999999600001, 9.9999999999000004e-05},
        {1e-10, 7.9999999967999997, 4.9999999995000001e-05, 0.00019999999996, 0.00084999999959500003, 0.00019999999996,
         9.9999999990000002e-05},
        {1e-09, 7.999999968, 4.999999995e-05, 0.00019999999959999999, 0.00084999999595000003, 0.00019999999959999999,
         9.99999999e-05},
        {1e-08, 7.9999996800000082, 4.9999999500000004e-05, 0.00019999999600000005, 0.0008499999595000012,
         0.00019999999600000005, 9.9999999000000009e-05},
        {1e-07, 7.9999968000008534, 4.9999995000000293e-05, 0.00019999996000000467, 0.000849999595000123,
         0.00019999996000000535, 9.9999990000000668e-05},
        {1e-06, 7.9999680000853335, 4.9999950000029164e-05, 0.00019999960000046668, 0.00084999595001229581,
         0.00019999960000053333, 9.9999900000066663e-05},
        {1e-05, 7.9996800085331623, 4.9999500002916654e-05, 0.00019999600004666626, 0.00084995950122955532,
         0.0001999960000533328, 9.999900000666664e-05},
        {0.0001, 7.9968008531626937, 4.9995000291654164e-05, 0.0001999600046662667, 0.00084959512293032594,
         0.00019996000533280004, 9.999000066663333e-05},
        {0.01, 7.6883653613364213, 4.9502904209597538e-05, 0.00019604626940630251, 0.0008107020754938416,
         0.00019605280423838395, 9.9006633466223494e-05},
        {0.1, 5.5067103588277844, 4.5279585030313559e-05, 0.00016429269939837791, 0.00054437820491150788,
         0.00016483997698218034, 9.063462346100907e-05},
        {1.0, 0.99966453737209748, 1.9978820044686403e-05, 3.7382253620775439e-05, 6.3197552747948769e-05,
         4.9084218055563291e-05, 4.3233235838169365e-05},
        {-1e-12, 8.0000000000319993, 5.0000000000049998e-05, 0.0002000000000004, 0.00085000000000404999,
         0.0002000000000004, 0.0001000000000001},
        {-1e-08, 8.0000003200000087, 5.00000005e-05, 0.00020000000400000005, 0.0008500000405000012,
         0.00020000000400000005, 0.000100000001},
        {-0.0001, 8.003200853504028, 5.000500029167917e-05, 0.00020004000466706669, 0.00085040512298635096,
         0.00020004000533386672, 0.0001000100006667},
        {-0.1, 12.255409284924676, 5.5304610044372925e-05, 0.00024509590660465325, 0.0014119953190764119,
         0.00024591234882063518, 0.00011070137908008492},
    };

    const Real f1 = curve->forwardRate(1.0, 1.0, Continuous, NoFrequency);
    const Real f2 = curve->forwardRate(2.0, 2.0, Continuous, NoFrequency);
    // the processes differentiate the forward over this step, and its rounding is in their drift
    const Real shift = 0.0001;
    const Real fPrime = (curve->forwardRate(2.0 + shift, 2.0 + shift, Continuous, NoFrequency) - f2) / shift;

    const Real relative = 1.0e-13, absolute = 1.0e-15;
    const Real x0 = 0.031, x = 0.029;

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("a = " << c.a)
        {
            const Real decay = std::exp(-c.a);
            const Real common = x0 * decay + (f2 + c.A2) - (f1 + c.A1) * decay;
            const Real drift = c.a * (f2 - x) + c.D + fPrime;

            HullWhiteForwardProcess forward(curve, c.a, sigma);
            forward.setForwardMeasureTime(T);
            BOOST_CHECK_SMALL(forward.B(2.0, T) / c.B - 1.0, relative);
            BOOST_CHECK_SMALL(forward.M_T(1.0, 2.0, T) / c.M - 1.0, relative);
            BOOST_CHECK_SMALL((forward.alpha(2.0) - f2) / c.A2 - 1.0, relative);
            BOOST_CHECK_SMALL(forward.expectation(1.0, x0, 1.0) - (common - c.M), absolute);
            BOOST_CHECK_SMALL(forward.drift(2.0, x) - (drift - c.B * sigma * sigma), absolute);

            BOOST_CHECK_SMALL(OrnsteinUhlenbeckProcess(c.a, sigma).variance(0.0, 0.0, 1.0) / c.variance - 1.0,
                              relative);
            const GeneralizedOrnsteinUhlenbeckProcess generalized([&](Time) { return c.a; },
                                                                  [&](Time) { return sigma; });
            BOOST_CHECK_SMALL(generalized.variance(0.0, 0.0, 1.0) / c.variance - 1.0, relative);

            const GeneralizedHullWhite model(curve, c.a, sigma);
            BOOST_CHECK_SMALL((model.HWdynamics()->shortRate(2.0, 0.0) - f2) / c.A2 - 1.0, relative);

            // HullWhiteProcess refuses a negative mean reversion.
            if (c.a >= 0.0)
            {
                HullWhiteProcess spot(curve, c.a, sigma);
                BOOST_CHECK_SMALL((spot.alpha(2.0) - f2) / c.A2 - 1.0, relative);
                BOOST_CHECK_SMALL(spot.expectation(1.0, x0, 1.0) - common, absolute);
                BOOST_CHECK_SMALL(spot.drift(2.0, x) - drift, absolute);
            }

            // These two refuse anything but a positive one.  With eta = sigma, b = a and rho = -1/2,
            // G2's fitting parameter adds just what Hull-White's does.
            if (c.a > 0.0)
            {
                const HullWhite hullWhite(curve, c.a, sigma);
                BOOST_CHECK_SMALL((hullWhite.dynamics()->shortRate(2.0, 0.0) - f2) / c.A2 - 1.0, relative);

                const G2 g2(curve, c.a, sigma, c.a, sigma, -0.5);
                BOOST_CHECK_SMALL((g2.dynamics()->shortRate(2.0, 0.0, 0.0) - f2) / c.A2 - 1.0, relative);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testDecayRatioDifferenceForAnySigns)
{
    BOOST_TEST_MESSAGE("Testing the difference of decay ratios for arguments of any sign...");

    struct Case
    {
        Real u, v, expected;
    };

    // (f(u) - f(u+v)) / v with f(x) = (1 - exp(-x)) / x, in 200-digit arithmetic, or its limit at v = 0.
    const Case cases[] = {
        {0.0, 0.0, 0.5},
        {1.0, 0.0, 0.26424111765711533},
        {-1.5, 0.0, 1.4403753489640143},
        {0.001, 0.002, 0.49933387466683465},
        {0.5, 0.5, 0.30963624349235097},
        {1.0, 1.0, 0.19978820044686402},
        {0.05, -0.1, 0.50010417534760976},
        {1.0, -2.0, 0.54308063481524382},
        {2.0, -4.0, 0.69054892277090785},
        {-1.0, -1.0, 1.4762462210062799},
        {4.0, -2.0, 0.093455634051938602},
        {-3.0, 1.5, 2.6938130627803418},
        {3.0, 3.0, 0.050161367524496583},
        {0.001, 5.0, 0.16017722638318752},
        {5.0, 0.001, 0.038375891112154871},
        {-5.0, 0.5, 19.402094174132056},
        {-30.0, 0.001, 344175382695.53986},
        {-5.0, -7.0, 1933.3333313947351},
        {5.0, -8.0, 0.77039915380779667},
        {-20.0, 25.0, 970330.38087348419},
        {3.0, -3.0, 0.22775411870754045},
        {-3.0, 3.0, 1.7872818803541852},
        {10.0, -20.0, 110.12232920103322},
        {25.0, -25.0, 0.038400000000022222},
        {-25.0, 25.5, 112948861.67327437},
        {30.0, -30.0, 0.032222222222222326},
        {40.0, -39.0, 0.015567193816116863},
        {40.0, -60.0, 404304.32825815858},
        {-60.0, -60.0, 1.8113623311022671e+48},
    };

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("u = " << c.u << ", v = " << c.v)
        {
            BOOST_CHECK_SMALL(QuantLib::detail::decayRatioDifference(c.u, c.v) / c.expected - 1.0, 1.0e-14);
        }
    }
}

BOOST_AUTO_TEST_CASE(testG2ProcessesForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing the G2 processes with mean reversion close to zero...");

    // Stepping from 1 to 2 years under the 10-year forward measure.
    const Real sigma = 0.01, eta = 0.005, rho = -0.3;
    const Time T = 10.0;

    struct Case
    {
        // the mean reversions; the expectation from zero at 2; the drift at zero at 2; the step's standard deviation
        Real a, b, ex, ey, dx, dy, s00, s10, s11;
    };

    // The closed forms as they stood, in 300-digit arithmetic, with a zero or a zero sum standing for its limit.
    const Case cases[] = {
        {0.0, 0.1, -0.00076413895656493374, -1.4881192630361761e-05, -0.00071739934461758324, -1.76677589706946e-05,
         0.01, -0.0014274387294606063, 0.004541043331536064},
        {1e-16, 0.1, -0.00076413895656493331, -1.4881192630361811e-05, -0.00071739934461758292, -1.7667758970694647e-05,
         0.01, -0.0014274387294606063, 0.004541043331536064},
        {1e-12, 0.1, -0.00076413895656092718, -1.4881192630877007e-05, -0.00071739934461438322, -1.76677589711746e-05,
         0.0099999999999950007, -0.0014274387294606183, 0.0045410433315360606},
        {1e-10, 0.1, -0.00076413895616428017, -1.4881192681886387e-05, -0.00071739934429758325, -1.7667759018694598e-05,
         0.0099999999995000009, -0.0014274387294617957, 0.0045410433315356902},
        {1e-08, 0.1, -0.00076413891649958416, -1.4881197782824329e-05, -0.00071739931261758413, -1.7667763770694471e-05,
         0.0099999999499999995, -0.0014274387295795399, 0.0045410433314986781},
        {1e-06, 0.1, -0.0007641349500421389, -1.4881707875171369e-05, -0.00071739614462611656, -1.76682389694146e-05,
         0.0099999950000020828, -0.0014274387413538876, 0.0045410433277975112},
        {0.0001, 0.1, -0.00076373842584189465, -1.4932702642608214e-05, -0.00071707942993385265,
         -1.7715746173254189e-05, 0.0099995000208327085, -0.0014274399182001042, 0.004541042957865643},
        {0.01, 0.1, -0.00072527422729988815, -1.989053690281783e-05, -0.00068623588075122542, -2.2342278550648273e-05,
         0.0099502077097025216, -0.0014275517165412653, 0.004541007813454229},
        {0.1, 0.0, -0.0004231648463139927, -0.00012663895656493372, -0.00043067103588277839, -0.00011739934461758324,
         0.0095202218178469487, -0.0014993754943657706, 0.0047698923600952882},
        {0.1, 1e-16, -0.00042316484631399275, -0.00012663895656493362, -0.00043067103588277845, -0.00011739934461758316,
         0.0095202218178469487, -0.0014993754943657704, 0.0047698923600952882},
        {0.1, 1e-12, -0.00042316484631450791, -0.00012663895656396469, -0.00043067103588325842, -0.00011739934461678324,
         0.0095202218178469487, -0.0014993754943650333, 0.0047698923600928995},
        {0.1, 1e-10, -0.00042316484636551729, -0.0001266389564680302, -0.00043067103593077841, -0.00011739934453758324,
         0.0095202218178469487, -0.0014993754942920509, 0.0047698923598564012},
        {0.1, 1e-08, -0.00042316485146645525, -0.00012663894687458326, -0.00043067104068277826, -0.00011739933661758345,
         0.0095202218178469487, -0.0014993754869938201, 0.0047698923362065569},
        {0.1, 1e-06, -0.0004231653615588023, -0.00012663798753291707, -0.00043067151588149842, -0.00011739854461971657,
         0.0095202218178469487, -0.0014993747571709808, 0.0047698899712231447},
        {0.1, 0.0001, -0.00042321635632623915, -0.00012654208364415017, -0.00043071902308533798,
         -0.00011731936594665059, 0.0095202218178469487, -0.0014993017772990771, 0.0047696534829320845},
        {0.0, 0.0, -0.00072250000000000005, -8.5000000000000006e-05, -0.00068000000000000005, -8.0000000000000007e-05,
         0.01, -0.0015, 0.0047696960070847281},
        {1e-12, 1e-12, -0.00072249999999655749, -8.4999999999595002e-05, -0.00067999999999728, -7.9999999999680004e-05,
         0.0099999999999950007, -0.00149999999999925, 0.0047696960070823437},
        {1e-08, 1e-08, -0.00072249996557500107, -8.4999995950000128e-05, -0.00067999997280000074,
         -7.999999680000009e-05, 0.0099999999499999995, -0.0014999999925000001, 0.0047696959832362485},
        {1e-06, 1e-06, -0.00072249655751045142, -8.4999595001229578e-05, -0.00067999728000725328,
         -7.9999680000853333e-05, 0.0099999950000020828, -0.0014999992500003125, 0.0047696936222377181},
        {0.0001, 0.0001, -0.00072215585449077711, -8.4959512293032591e-05, -0.00067972807251882894,
         -7.9968008531626934e-05, 0.0099995000208327085, -0.0014999250031249062, 0.0047694575322209422},
        {0.1, 0.1, -0.00046272147417478172, -5.4437820491150791e-05, -0.00046807038050036164, -5.5067103588277841e-05,
         0.0095202218178469487, -0.0014280332726770424, 0.0045408563991145507},
        {1.0, 0.5, -4.4518513332097441e-05, -2.6966575333613374e-05, -7.0515922903871772e-05, -3.4089249994981827e-05,
         0.0065751985398289966, -0.0011815154099845241, 0.0037956600620554842},
        {0.03, 3.0, -0.00073392383993303863, 3.2194451475509515e-05, -0.00070624046311167738, 9.8352736133704553e-05,
         0.0098518582355268808, -0.00047821523711726624, 0.0019818296393954},
        {0.05, -0.1, -0.00047902924970861008, -0.0002436943376406049, -0.00047552876865485127, -0.00020748124593380869,
         0.0097551310582708432, -0.001576742415958238, 0.0050188871399683222},
        {-0.1, 0.05, -0.0013026908269479138, 2.7247220424881508e-05, -0.0011266369423031594, 1.8991162291689797e-05,
         0.010521472286713724, -0.0014618989143021747, 0.0046533318230459588},
        {1.0, -2.0, 114.51624880954876, -1488.3720524381324, 66.645721437355306, -111.07635401138035,
         0.0065751985398289966, -0.0039199162231770418, 0.017877994700480949},
        {-0.03, 0.03, -0.00086910443950603475, -4.1662261782885024e-05, -0.000797477764937959, -4.2185540617169816e-05,
         0.010151891986665969, -0.0014775570917915392, 0.0046991065867594698},
        {-1.0, -1.0, -0.80928802787459297, -0.095210356220540346, -0.25329642889854692, -0.029799579870417283,
         0.017873242709327608, -0.0026809864063991411, 0.0085249934384336121},
    };

    const Real relative = 1.0e-13;
    const Array x0(2, 0.0);

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("a = " << c.a << ", b = " << c.b)
        {
            G2ForwardProcess forward(c.a, sigma, c.b, eta, rho);
            forward.setForwardMeasureTime(T);

            const Array mean = forward.expectation(1.0, x0, 1.0);
            BOOST_CHECK_SMALL(mean[0] / c.ex - 1.0, relative);
            BOOST_CHECK_SMALL(mean[1] / c.ey - 1.0, relative);

            const Array drift = forward.drift(2.0, x0);
            BOOST_CHECK_SMALL(drift[0] / c.dx - 1.0, relative);
            BOOST_CHECK_SMALL(drift[1] / c.dy - 1.0, relative);

            for (const Matrix& m :
                 {forward.stdDeviation(1.0, x0, 1.0), G2Process(c.a, sigma, c.b, eta, rho).stdDeviation(1.0, x0, 1.0)})
            {
                BOOST_CHECK_SMALL(m[0][0] / c.s00 - 1.0, relative);
                BOOST_CHECK_SMALL(m[1][0] / c.s10 - 1.0, relative);
                BOOST_CHECK_SMALL(m[1][1] / c.s11 - 1.0, relative);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testFuturesConvexityBiasForSmallMeanReversion)
{
    BOOST_TEST_MESSAGE("Testing Hull-White's futures convexity bias with mean reversion close to zero...");

    // The closed form at a futures price of 94 from 5 to 5.25 years, evaluated in 120-digit arithmetic.
    const std::tuple<Real, Volatility, Rate> cases[] = {
        {0.0, 0.015, 0.0031389422057161594},    {1e-16, 0.015, 0.0031389422057161581},
        {1e-15, 0.015, 0.0031389422057161434},  {1e-14, 0.015, 0.0031389422057159985},
        {1e-12, 0.015, 0.003138942205700043},   {1e-10, 0.015, 0.0031389422041045081},
        {1e-08, 0.015, 0.00313894204455102},    {1e-06, 0.015, 0.0031389260892502229},
        {0.0001, 0.015, 0.0031373310391677677}, {0.03, 0.015, 0.0026963448010039874},
        {0.03, 1e-06, 1.1987735785397523e-11},  {1e-10, 1e-06, 1.3956249992807574e-11},
        {0.0, 1e-07, 1.3956249999999759e-13},
    };

    for (const auto& [a, sigma, expected] : cases)
    {
        BOOST_TEST_CONTEXT("a = " << a << ", sigma = " << sigma)
        {
            BOOST_CHECK_SMALL(HullWhite::convexityBias(94.0, 5.0, 5.25, sigma, a) / expected - 1.0, 1.0e-13);
        }
    }
}

BOOST_AUTO_TEST_CASE(testExtendedCoxIngersollRossObservesTheCurve)
{
    BOOST_TEST_MESSAGE("Testing that the extended CIR model hears about its term structure...");

    const Date today(19, May, 2026);
    Settings::instance().evaluationDate() = today;

    RelinkableHandle<YieldTermStructure> curve;
    curve.linkTo(flatRate(today, 0.02, Actual365Fixed()));
    auto model = ext::make_shared<ExtendedCoxIngersollRoss>(curve, 0.02, 1.0, 1.0e-4, 0.02);

    Flag flag;
    flag.registerWith(model);

    curve.linkTo(flatRate(today, 0.05, Actual365Fixed()));

    BOOST_CHECK(flag.isUp());
}

class DiscountBondOption : public DiscretizedDiscountBond
{
  public:
    void exercise(Real strike)
    {
        for (Size i = 0; i < values_.size(); ++i)
            values_[i] = std::max(values_[i] - strike, 0.0);
    }
};

Real bondOptionOnTree(
    const ShortRateModel& model, const std::vector<Time>& times, Time expiry, Time maturity, Real strike)
{
    DiscountBondOption option;
    option.initialize(model.tree(TimeGrid(times.begin(), times.end())), maturity);
    option.rollback(expiry);
    option.exercise(strike);
    option.rollback(0.0);
    return option.presentValue();
}

BOOST_AUTO_TEST_CASE(testTrinomialTreeOverShortSteps)
{
    BOOST_TEST_MESSAGE("Testing that a trinomial tree's nodes don't multiply over short steps...");

    const auto process = ext::make_shared<OrnsteinUhlenbeckProcess>(0.1, 0.01);

    for (Real gap : {1.0e-3, 1.0e-5, 1.0e-7})
    {
        BOOST_TEST_CONTEXT("gap of " << gap)
        {
            const std::vector<Time> times = {1.0, 1.0 + gap, 2.0 + gap, 3.0 + gap};
            const TrinomialTree tree(process, TimeGrid(times.begin(), times.end()));

            // Each step adds no more than a node at each end.
            for (Size i = 0; i < times.size() + 1; ++i)
                BOOST_CHECK_LE(tree.size(i), 2 * i + 1);

            // The second step keeps the first one's spacing.
            BOOST_CHECK_EQUAL(tree.dx(2), tree.dx(1));
            BOOST_CHECK_CLOSE(tree.widening(1), tree.dx(2) / (std::sqrt(3.0 * process->variance(1.0, 0.0, gap))),
                              1.0e-6);

            // It has to diffuse by just as much, around the expected value.
            const Real v2 = process->variance(1.0, 0.0, gap);
            for (Size j = 0; j < tree.size(1); ++j)
            {
                Real total = 0.0, mean = 0.0, variance = 0.0;
                const Real x = tree.underlying(1, j);
                const Real expected = process->expectation(1.0, x, gap);
                for (Size b = 0; b < 3; ++b)
                {
                    const Real p = tree.probability(1, j, b);
                    const Real next = tree.underlying(2, tree.descendant(1, j, b));
                    total += p;
                    mean += p * next;
                    variance += p * (next - expected) * (next - expected);
                }
                BOOST_CHECK_CLOSE(total, 1.0, 1.0e-10);
                BOOST_CHECK_SMALL(mean - expected, 1.0e-14);
                BOOST_CHECK_CLOSE(variance, v2, 1.0e-6);
            }
        }
    }

    // This one is short, but not that short, so nothing changes.
    const std::vector<Time> times = {1.0, 1.02, 2.02};
    const TrinomialTree tree(process, TimeGrid(times.begin(), times.end()));
    BOOST_CHECK_CLOSE(tree.dx(2), std::sqrt(3.0 * process->variance(1.0, 0.0, 0.02)), 1.0e-10);
    BOOST_CHECK_EQUAL(tree.widening(1), 1.0);
}

BOOST_AUTO_TEST_CASE(testTrinomialTreeKeepsItsNodesOverLongStretches)
{
    BOOST_TEST_MESSAGE("Testing that a trinomial tree refines its nodes over a long stretch of short steps...");

    const Date today(15, August, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();
    const Handle<YieldTermStructure> curve(flatRate(today, 0.03, dc));
    const HullWhite model(curve, 0.1, 0.01);
    const auto process = ext::make_shared<OrnsteinUhlenbeckProcess>(0.1, 0.01);

    // Each of these steps diffuses over less than a tenth of the spacing the first one
    // sets, but all of them together don't, so the option's kink at their end is resolved.
    std::vector<Time> times = {1.0};
    for (Size i = 1; i <= 100; ++i)
        times.push_back(1.0 + i * 0.001);
    times.push_back(2.1);

    const TrinomialTree tree(process, TimeGrid(times.begin(), times.end()));
    BOOST_CHECK_CLOSE(tree.dx(2), std::sqrt(3.0 * process->variance(1.0, 0.0, 0.001)), 1.0e-6);

    const Real strike = curve->discount(2.1) / curve->discount(1.1);
    const Real expected = model.discountBondOption(Option::Call, strike, 1.1, 2.1);
    BOOST_CHECK_SMALL(bondOptionOnTree(model, times, 1.1, 2.1, strike) / expected - 1.0, 0.06);

    // A long stretch before a long step doesn't come into it either.
    times.clear();
    for (Size i = 1; i <= 1000; ++i)
        times.push_back(i * 0.001);
    times.push_back(3.0);
    const TrinomialTree tree2(process, TimeGrid(times.begin(), times.end()));
    for (Size i = 1; i < 1000; ++i)
        BOOST_CHECK_EQUAL(tree2.widening(i), 1.0);
    const Real strike2 = curve->discount(3.0) / curve->discount(1.0);
    const Real expected2 = model.discountBondOption(Option::Call, strike2, 1.0, 3.0);
    BOOST_CHECK_SMALL(bondOptionOnTree(model, times, 1.0, 3.0, strike2) / expected2 - 1.0, 0.11);
}

BOOST_AUTO_TEST_CASE(testShortRateTreesOverShortSteps)
{
    BOOST_TEST_MESSAGE("Testing that short-rate trees give the same values with or without a short step...");

    const Date today(15, August, 2026);
    Settings::instance().evaluationDate() = today;
    const DayCounter dc = Actual365Fixed();
    const Handle<YieldTermStructure> curve(flatRate(today, 0.03, dc));

    const std::pair<std::string, ext::shared_ptr<ShortRateModel>> models[] = {
        {"Hull-White", ext::make_shared<HullWhite>(curve, 0.1, 0.01)},
        {"Black-Karasinski", ext::make_shared<BlackKarasinski>(curve, 0.1, 0.1)},
        {"Cox-Ingersoll-Ross", ext::make_shared<CoxIngersollRoss>(0.03, 0.03, 0.5, 0.08)},
        {"G2", ext::make_shared<G2>(curve, 0.1, 0.01, 0.2, 0.008, -0.75)},
        {"G2 with a positive correlation", ext::make_shared<G2>(curve, 0.1, 0.01, 0.2, 0.008, 0.75)}};

    const Real strike = curve->discount(3.0) / curve->discount(2.0);

    for (const auto& [name, model] : models)
    {
        // The long steps have different lengths, so that the spacing kept isn't the widest.
        const Real expected = bondOptionOnTree(*model, {0.5, 1.0, 2.0, 3.0}, 2.0, 3.0, strike);

        for (Real gap : {1.0 / 365.0, 1.0e-4})
        {
            BOOST_TEST_CONTEXT(name << ", gap of " << gap)
            {
                const Real calculated = bondOptionOnTree(*model, {0.5, 1.0, 1.0 + gap, 2.0, 3.0}, 2.0, 3.0, strike);
                BOOST_CHECK_SMALL(calculated / expected - 1.0, 1.0e-3);
            }
        }
    }

    // A two-factor lattice has the product of its trees' nodes.
    const std::vector<Time> times = {0.5, 1.0, 1.0001, 2.0, 3.0};
    const G2 g2(curve, 0.1, 0.01, 0.2, 0.008, -0.75);
    const auto lattice =
        ext::dynamic_pointer_cast<TwoFactorModel::ShortRateTree>(g2.tree(TimeGrid(times.begin(), times.end())));
    BOOST_REQUIRE(lattice);
    for (Size i = 0; i < times.size() + 1; ++i)
        BOOST_CHECK_LE(lattice->size(i), (2 * i + 1) * (2 * i + 1));
}

BOOST_AUTO_TEST_CASE(testCoxIngersollRossShortExpiryBondOptions)
{
    BOOST_TEST_MESSAGE("Testing short-expiry bond options in the Cox-Ingersoll-Ross model...");

    const CoxIngersollRoss model(0.03, 0.04, 0.3, 0.1);
    const Time bondMaturity = 1.0;

    // The closed formula, with the distribution's series summed at 50 digits.
    struct Case
    {
        Time expiry;
        Real strike, call, put;
    };
    const Case cases[] = {
        {1.0 / 52, 0.9, 0.06968543429919652, 0.0},
        {1.0 / 52, 0.95, 0.01971429978338451, 8.330985234252416e-22},
        {1.0 / 52, 0.97, 0.000654330058501688, 0.0009284840814420261},
        {1.0 / 52, 0.98, 5.972335615126306e-13, 0.010268380926699997},
        {1.0 / 365, 0.9, 0.06923983527536709, 0.0},
        {1.0 / 365, 0.95, 0.019243945258231213, 0.0},
        {1.0 / 365, 0.97, 6.19091004229057e-05, 0.0008163198490460851},
        {1.0 / 365, 0.98, 0.0, 0.010753588752050375},
        {1.0 / 3650, 0.9, 0.0691732529150161, 0.0},
        {1.0 / 3650, 0.95, 0.01917366387786073, 0.0},
        {1.0 / 3650, 0.97, 1.5354938358444237e-08, 0.0008261870919398224},
        {1.0 / 3650, 0.98, 0.0, 0.01082608954443256},
    };

    // Each of the two distributions in the formula stops within 1e-12.
    const Real tolerance = 3e-12;
    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("expiry: " << c.expiry << ", strike: " << c.strike)
        {
            BOOST_CHECK_SMALL(model.discountBondOption(Option::Call, c.strike, c.expiry, bondMaturity) - c.call,
                              tolerance);
            BOOST_CHECK_SMALL(model.discountBondOption(Option::Put, c.strike, c.expiry, bondMaturity) - c.put,
                              tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testExtendedCoxIngersollRossShortExpiryBondOptions)
{
    BOOST_TEST_MESSAGE("Testing short-expiry bond options in the extended Cox-Ingersoll-Ross model...");

    const Date today(15, May, 2026);
    Settings::instance().evaluationDate() = today;
    const Handle<YieldTermStructure> curve(flatRate(today, 0.04, Actual365Fixed()));
    const ExtendedCoxIngersollRoss model(curve, 0.04, 0.3, 0.1, 0.03);

    const Time expiry = 1.0 / 365, bondMaturity = 1.0;
    const Real forward = curve->discount(bondMaturity), discount = curve->discount(expiry);

    // Far enough from the money for the options to be worth their intrinsic values.
    for (Real strike : {0.90, 0.92})
    {
        BOOST_TEST_CONTEXT("strike: " << strike)
        {
            BOOST_CHECK_SMALL(model.discountBondOption(Option::Call, strike, expiry, bondMaturity) -
                                  (forward - strike * discount),
                              3e-12);
            BOOST_CHECK_SMALL(model.discountBondOption(Option::Put, strike, expiry, bondMaturity), 3e-12);
        }
    }
    for (Real strike : {1.0, 1.02})
    {
        BOOST_TEST_CONTEXT("strike: " << strike)
        {
            BOOST_CHECK_SMALL(model.discountBondOption(Option::Call, strike, expiry, bondMaturity), 3e-12);
            BOOST_CHECK_SMALL(model.discountBondOption(Option::Put, strike, expiry, bondMaturity) -
                                  (strike * discount - forward),
                              3e-12);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
