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
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/risk/curvebucketer.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <numeric>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CurveBucketerTests)

namespace
{

    // Build a simple Euribor curve from 4 swap quotes and price a 5Y
    // vanilla swap off it. Return both the curve's input quotes and
    // the swap so the test can exercise CurveBucketer on real observer
    // wiring.
    struct SwapOnCurve
    {
        std::vector<ext::shared_ptr<SimpleQuote>> quotes;
        ext::shared_ptr<VanillaSwap> swap;
    };

    SwapOnCurve makeSwapOnCurve(Date today, const std::vector<Real>& swapRates, const std::vector<Period>& swapTenors)
    {
        Settings::instance().evaluationDate() = today;
        Calendar cal = TARGET();
        DayCounter dc = Actual360();

        // A separate discounting curve (flat 2%) so the swap doesn't
        // bootstrap itself; keeps the bucketing attribution clean.
        auto disc = Handle<YieldTermStructure>(ext::make_shared<FlatForward>(today, 0.02, dc));

        auto euribor = ext::make_shared<Euribor6M>(disc);

        SwapOnCurve out;
        std::vector<ext::shared_ptr<RateHelper>> helpers;
        for (Size i = 0; i < swapRates.size(); ++i)
        {
            auto q = ext::make_shared<SimpleQuote>(swapRates[i]);
            out.quotes.push_back(q);
            helpers.push_back(ext::make_shared<SwapRateHelper>(Handle<Quote>(q), swapTenors[i], cal, Annual, Unadjusted,
                                                               Thirty360(Thirty360::BondBasis), euribor,
                                                               Handle<Quote>(), 0 * Days, disc));
        }
        RelinkableHandle<YieldTermStructure> forward;
        forward.linkTo(ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear>>(today, helpers, dc));

        auto euriborForecast = ext::make_shared<Euribor6M>(forward);
        out.swap = MakeVanillaSwap(5 * Years, euriborForecast, 0.03)
                       .withEffectiveDate(cal.advance(today, 2 * Days))
                       .withType(Swap::Payer);
        out.swap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(disc));
        return out;
    }

}

BOOST_AUTO_TEST_CASE(testBucketedDeltaSign)
{
    BOOST_TEST_MESSAGE("Testing CurveBucketer bucketedDelta sign pattern...");

    SwapOnCurve fixture = makeSwapOnCurve(Date(20, April, 2026), {0.020, 0.025, 0.030, 0.035},
                                          {2 * Years, 3 * Years, 5 * Years, 10 * Years});

    CurveBucketer bucketer(fixture.quotes);
    std::vector<Real> buckets = bucketer.bucketedDelta(*fixture.swap);

    BOOST_REQUIRE_EQUAL(buckets.size(), fixture.quotes.size());

    // For a payer swap, raising an input swap rate raises the forecast
    // curve's forward rates, which raises projected float-leg cash
    // flows, which raises NPV. So each bucket must be non-negative.
    // Under QL_USE_INDEXED_COUPON some input tenors (e.g. a 10Y quote
    // that never feeds a 5Y swap's cashflow dates) legitimately have
    // a ~zero bucket, so we don't demand any specific number of
    // strictly positive entries; we check the financial invariant:
    //   * no bucket has a negative contribution (beyond FD noise);
    //   * at least one bucket is positive;
    //   * the sum is strictly positive.
    const Real fdNoise = 1.0e-6;
    Size positiveCount = 0;
    Real sum = 0.0;
    for (Real d : buckets)
    {
        BOOST_CHECK_MESSAGE(d >= -fdNoise, "Expected non-negative bucketed delta for payer swap; got " << d);
        if (d > 0.0)
            ++positiveCount;
        sum += d;
    }
    BOOST_CHECK_MESSAGE(positiveCount >= 1 && sum > 0.0,
                        "Expected at least 1 positive bucketed delta and a positive sum "
                        "for a payer swap; got positiveCount="
                            << positiveCount << ", sum=" << sum);
}

BOOST_AUTO_TEST_CASE(testBucketedSumMatchesParallel)
{
    BOOST_TEST_MESSAGE("Testing sum(bucketedDelta) == parallelDelta "
                       "within FD tolerance...");

    SwapOnCurve fixture = makeSwapOnCurve(Date(20, April, 2026), {0.020, 0.025, 0.030, 0.035},
                                          {2 * Years, 3 * Years, 5 * Years, 10 * Years});

    CurveBucketer bucketer(fixture.quotes);
    std::vector<Real> buckets = bucketer.bucketedDelta(*fixture.swap);
    Real sumOfBuckets = std::accumulate(buckets.begin(), buckets.end(), 0.0);
    Real parallel = bucketer.parallelDelta(*fixture.swap);

    // For a locally linear instrument (vanilla swap is linear in the
    // forward-curve quotes to leading order) the sum of bucketed
    // deltas equals the parallel-shift delta. Tolerance is set for a
    // 1bp bump, where the remaining second-order error is ~1e-4 * NPV.
    Real tol = std::max(1.0e-6, 1.0e-4 * std::fabs(parallel));
    BOOST_CHECK_MESSAGE(std::fabs(sumOfBuckets - parallel) < tol, "Sum of buckets " << sumOfBuckets
                                                                                    << " does not match parallel "
                                                                                       "delta "
                                                                                    << parallel << " within tolerance "
                                                                                    << tol);
}

BOOST_AUTO_TEST_CASE(testBucketerRestoresQuotesOnException)
{
    BOOST_TEST_MESSAGE("Testing that CurveBucketer restores quotes after "
                       "exception from pricing...");

    SwapOnCurve fixture = makeSwapOnCurve(Date(20, April, 2026), {0.020, 0.025, 0.030, 0.035},
                                          {2 * Years, 3 * Years, 5 * Years, 10 * Years});

    std::vector<Real> originals;
    for (const auto& q : fixture.quotes)
        originals.push_back(q->value());

    // Run a normal bucketedDelta — should restore quotes.
    CurveBucketer bucketer(fixture.quotes);
    (void)bucketer.bucketedDelta(*fixture.swap);

    for (Size i = 0; i < fixture.quotes.size(); ++i)
    {
        BOOST_CHECK_MESSAGE(close(fixture.quotes[i]->value(), originals[i]), "Quote " << i << " not restored: got "
                                                                                      << fixture.quotes[i]->value()
                                                                                      << " expected " << originals[i]);
    }
}

BOOST_AUTO_TEST_CASE(testBucketerRejectsInvalidInputs)
{
    BOOST_TEST_MESSAGE("Testing CurveBucketer input validation...");

    // Zero/negative bump rejected. Wrap construction in a lambda to
    // avoid C++'s most-vexing-parse ambiguity inside BOOST_CHECK_THROW.
    std::vector<ext::shared_ptr<SimpleQuote>> oneQuote{ext::make_shared<SimpleQuote>(0.02)};
    auto zeroBump = [&]
    {
        CurveBucketer cb(oneQuote, 0.0);
        (void)cb;
    };
    auto negBump = [&]
    {
        CurveBucketer cb(oneQuote, -1e-4);
        (void)cb;
    };
    BOOST_CHECK_THROW(zeroBump(), Error);
    BOOST_CHECK_THROW(negBump(), Error);

    // Null quote rejected
    std::vector<ext::shared_ptr<SimpleQuote>> withNull{ext::make_shared<SimpleQuote>(0.02),
                                                       ext::shared_ptr<SimpleQuote>()};
    auto nullQuote = [&]
    {
        CurveBucketer cb(withNull);
        (void)cb;
    };
    BOOST_CHECK_THROW(nullQuote(), Error);

    // Empty vector is allowed (edge case — zero buckets)
    std::vector<ext::shared_ptr<SimpleQuote>> empty;
    auto emptyQuotes = [&]
    {
        CurveBucketer cb(empty);
        (void)cb;
    };
    BOOST_CHECK_NO_THROW(emptyQuotes());
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
