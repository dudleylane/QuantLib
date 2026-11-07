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
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
