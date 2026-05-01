/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 dudleylane

 This file is new in the dudleylane fork of QuantLib and is licensed
 under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
 See the LICENSE file in the repository root for the full license text.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/instruments/autocallablenote.hpp>
#include <ql/pricingengines/autocallable/mcautocallablenoteengine.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AutocallableNoteTests)

namespace
{

    struct Fixture
    {
        Date today{20, April, 2026};
        std::vector<Date> obs;
        Real initialSpot = 100.0;

        ext::shared_ptr<SimpleQuote> spotQ, volQ, rQ, qQ;
        ext::shared_ptr<GeneralizedBlackScholesProcess> process;

        Fixture(Volatility vol = 0.25, Rate r = 0.02, Rate q = 0.0)
        {
            Settings::instance().evaluationDate() = today;
            DayCounter dc = Actual365Fixed();
            Calendar cal = NullCalendar();

            obs = {today + Period(1, Years), today + Period(2, Years), today + Period(3, Years),
                   today + Period(4, Years)};

            spotQ = ext::make_shared<SimpleQuote>(initialSpot);
            volQ = ext::make_shared<SimpleQuote>(vol);
            rQ = ext::make_shared<SimpleQuote>(r);
            qQ = ext::make_shared<SimpleQuote>(q);

            auto rTS = ext::make_shared<FlatForward>(today, Handle<Quote>(rQ), dc);
            auto qTS = ext::make_shared<FlatForward>(today, Handle<Quote>(qQ), dc);
            auto volTS = ext::make_shared<BlackConstantVol>(today, cal, Handle<Quote>(volQ), dc);

            process = ext::make_shared<BlackScholesMertonProcess>(Handle<Quote>(spotQ), Handle<YieldTermStructure>(qTS),
                                                                  Handle<YieldTermStructure>(rTS),
                                                                  Handle<BlackVolTermStructure>(volTS));
        }
    };

}

BOOST_AUTO_TEST_CASE(testAutocallTriggersAtFirstObservationWithLowBarrier)
{
    BOOST_TEST_MESSAGE("Autocallable with barrier=0 should redeem at "
                       "first observation with one-period coupon...");

    Fixture f(/*vol*/ 0.25, /*r*/ 0.03, /*q*/ 0.0);

    // Autocall barriers effectively zero -> guaranteed autocall on
    // the first observation date. Coupon 2%.
    std::vector<Real> zeroBarriers{1e-9, 1e-9, 1e-9, 1e-9};
    Real notional = 100.0, couponRate = 0.02, protection = 0.7;
    auto note = ext::make_shared<AutocallableNote>(notional, f.initialSpot, f.obs, zeroBarriers, couponRate, protection,
                                                   f.obs.back());
    note->setPricingEngine(
        ext::make_shared<MCAutocallableNoteEngine<>>(f.process, /*samples*/ Size(5000), /*seed*/ BigNatural(42)));

    // First observation at t=1: guaranteed autocall; PV should be
    //   notional * (1 + couponRate) * df(1Y) = 102 * exp(-0.03) = 98.984...
    Real expected = notional * (1.0 + couponRate) * std::exp(-0.03);
    Real actual = note->NPV();
    BOOST_CHECK_MESSAGE(std::fabs(actual - expected) < 1e-6, "NPV " << actual << " vs expected " << expected);
}

BOOST_AUTO_TEST_CASE(testHighBarrierApproachesEuropeanMaturityPayoff)
{
    BOOST_TEST_MESSAGE("Autocallable with unreachable barriers should "
                       "price close to the European protection payoff...");

    Fixture f(/*vol*/ 0.20, /*r*/ 0.01, /*q*/ 0.0);

    // Unreachable barriers -> never autocalls; instrument reduces to
    // the maturity payoff. Check NPV is positive and below notional
    // plus all coupons (i.e. the no-knock-in outcome).
    std::vector<Real> highBarriers{10.0, 10.0, 10.0, 10.0};
    Real notional = 100.0, couponRate = 0.015, protection = 0.70;
    auto note = ext::make_shared<AutocallableNote>(notional, f.initialSpot, f.obs, highBarriers, couponRate, protection,
                                                   f.obs.back());
    note->setPricingEngine(
        ext::make_shared<MCAutocallableNoteEngine<>>(f.process, /*samples*/ Size(20000), /*seed*/ BigNatural(7)));

    Real maxUpside = notional * (1.0 + couponRate * 4.0) * std::exp(-0.01 * 4.0);
    Real npv = note->NPV();
    BOOST_CHECK_MESSAGE(npv > 0.0 && npv < maxUpside + 1e-6,
                        "NPV " << npv << " outside plausible (0, " << maxUpside << "] range for never-autocall case");
}

BOOST_AUTO_TEST_CASE(testLowerBarriersGiveHigherNPV)
{
    BOOST_TEST_MESSAGE("Lowering autocall barriers should (weakly) raise "
                       "NPV for a coupon-bearing note...");

    // Two identical notes except for barrier aggressiveness. Lower
    // barriers trigger earlier autocall, paying coupons sooner,
    // which -- for a coupon rate above the risk-free rate (positive
    // carry trade) -- raises NPV. Use a common fixture + seed so the
    // MC paths are the same for both instruments.
    Fixture f(/*vol*/ 0.20, /*r*/ 0.005, /*q*/ 0.0);

    Real notional = 100.0, couponRate = 0.04, protection = 0.70;
    Size samples = 50000;
    BigNatural seed = 2026;

    std::vector<Real> lowBarriers{0.80, 0.80, 0.80, 0.80};
    std::vector<Real> highBarriers{1.10, 1.10, 1.10, 1.10};

    auto noteLow = ext::make_shared<AutocallableNote>(notional, f.initialSpot, f.obs, lowBarriers, couponRate,
                                                      protection, f.obs.back());
    noteLow->setPricingEngine(ext::make_shared<MCAutocallableNoteEngine<>>(f.process, samples, seed));

    auto noteHigh = ext::make_shared<AutocallableNote>(notional, f.initialSpot, f.obs, highBarriers, couponRate,
                                                       protection, f.obs.back());
    noteHigh->setPricingEngine(ext::make_shared<MCAutocallableNoteEngine<>>(f.process, samples, seed));

    Real npvLow = noteLow->NPV();
    Real npvHigh = noteHigh->NPV();
    BOOST_CHECK_MESSAGE(npvLow > npvHigh, "Expected lower barriers -> higher NPV under positive carry; "
                                          "got low="
                                              << npvLow << " high=" << npvHigh);
}

BOOST_AUTO_TEST_CASE(testConstructorRejectsInvalidInputs)
{
    BOOST_TEST_MESSAGE("AutocallableNote constructor input validation...");

    Date today{20, April, 2026};
    std::vector<Date> obs{today + Period(1, Years), today + Period(2, Years)};

    // Negative notional
    auto bad1 = [&]
    {
        AutocallableNote n(-1.0, 100.0, obs, {0.9, 0.8}, 0.02, 0.7, obs.back());
        (void)n;
    };
    BOOST_CHECK_THROW(bad1(), Error);

    // Size mismatch between observations and barriers
    auto bad2 = [&]
    {
        AutocallableNote n(100.0, 100.0, obs, {0.9}, 0.02, 0.7, obs.back());
        (void)n;
    };
    BOOST_CHECK_THROW(bad2(), Error);

    // Non-monotonic observation dates
    std::vector<Date> badObs{obs[1], obs[0]};
    auto bad3 = [&]
    {
        AutocallableNote n(100.0, 100.0, badObs, {0.9, 0.8}, 0.02, 0.7, badObs.back());
        (void)n;
    };
    BOOST_CHECK_THROW(bad3(), Error);
}

BOOST_AUTO_TEST_CASE(testAdaptiveConvergenceHitsTolerance)
{
    BOOST_TEST_MESSAGE("MCAutocallableNoteEngine in adaptive mode adds "
                       "paths until stderr <= tolerance (or throws on "
                       "maxSamples)...");

    Fixture f(/*vol*/ 0.25, /*r*/ 0.03, /*q*/ 0.0);

    std::vector<Real> barriers{1.10, 1.00, 0.95, 0.90};
    auto note = ext::make_shared<AutocallableNote>(100.0, f.initialSpot, f.obs, barriers,
                                                   /*coupon*/ 0.05, /*protect*/ 0.7, f.obs.back());

    // Tight tolerance + enough maxSamples: must converge.
    auto engine = ext::make_shared<MCAutocallableNoteEngine<>>(f.process, /*tolerance*/ 0.5,
                                                               /*maxSamples*/ Size(200000),
                                                               /*minSamples*/ Size(1024),
                                                               /*seed*/ BigNatural(42));
    note->setPricingEngine(engine);
    Real npv = note->NPV();
    Real err = note->errorEstimate();
    BOOST_CHECK(npv > 0.0);
    BOOST_CHECK_MESSAGE(err <= 0.5, "Adaptive engine did not converge: stderr=" << err);
}

BOOST_AUTO_TEST_CASE(testAdaptiveThrowsOnMaxSamples)
{
    BOOST_TEST_MESSAGE("MCAutocallableNoteEngine throws if maxSamples is "
                       "hit before stderr tolerance is reached...");

    Fixture f(/*vol*/ 0.40); // high vol -> slower convergence

    std::vector<Real> barriers{1.10, 1.00, 0.95, 0.90};
    auto note = ext::make_shared<AutocallableNote>(100.0, f.initialSpot, f.obs, barriers, 0.05, 0.7, f.obs.back());

    // Impossibly tight tolerance vs budget.
    auto engine = ext::make_shared<MCAutocallableNoteEngine<>>(f.process, /*tolerance*/ 0.001,
                                                               /*maxSamples*/ Size(2048),
                                                               /*minSamples*/ Size(1024),
                                                               /*seed*/ BigNatural(7));
    note->setPricingEngine(engine);
    BOOST_CHECK_EXCEPTION([&] { note->NPV(); }(), Error, ExpectedErrorMessage("hit maxSamples"));
}

BOOST_AUTO_TEST_CASE(testIsExpiredBoundary)
{
    BOOST_TEST_MESSAGE("AutocallableNote::isExpired() is true on the "
                       "maturity date itself and false the day before...");

    Date today{20, April, 2026};
    std::vector<Date> obs{today + Period(1, Years), today + Period(2, Years)};
    Date maturity = obs.back();
    AutocallableNote note(100.0, 100.0, obs, {0.9, 0.9}, 0.02, 0.7, maturity);

    Settings::instance().evaluationDate() = maturity - 1;
    BOOST_CHECK(!note.isExpired());

    Settings::instance().evaluationDate() = maturity;
    BOOST_CHECK(note.isExpired());

    Settings::instance().evaluationDate() = maturity + 1;
    BOOST_CHECK(note.isExpired());
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
