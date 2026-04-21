/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 StatPro Italia srl

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
#include <ql/indexes/bmaindex.hpp>
#include <ql/indexes/fallbackiborindex.hpp>
#include <ql/indexes/ibor/custom.hpp>
#include <ql/indexes/ibor/cdi.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/ibor/sofr.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/bespokecalendar.hpp>
#include <ql/time/calendars/brazil.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <ql/quotes/simplequote.hpp>
#include <boost/algorithm/string/case_conv.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(IndexTests)

BOOST_AUTO_TEST_CASE(testFixingObservability) {
    BOOST_TEST_MESSAGE("Testing observability of index fixings...");

    ext::shared_ptr<InterestRateIndex> i1 = ext::make_shared<Euribor6M>();
    ext::shared_ptr<InterestRateIndex> i2 = ext::make_shared<BMAIndex>();

    Flag f1;
    f1.registerWith(i1);
    f1.lower();

    Flag f2;
    f2.registerWith(i2);
    f2.lower();

    Date today = Date::todaysDate();

    ext::shared_ptr<Index> euribor = ext::make_shared<Euribor6M>();

    Date d1 = today;
    while (!euribor->isValidFixingDate(d1))
        d1++;

    euribor->addFixing(d1, -0.003);
    if (!f1.isUp())
        BOOST_FAIL("Observer was not notified of added Euribor fixing");

    ext::shared_ptr<Index> bma = ext::make_shared<BMAIndex>();

    Date d2 = today;
    while (!bma->isValidFixingDate(d2))
        d2++;

    bma->addFixing(d2, 0.01);
    if (!f2.isUp())
        BOOST_FAIL("Observer was not notified of added BMA fixing");
}

BOOST_AUTO_TEST_CASE(testFixingHasHistoricalFixing) {
    BOOST_TEST_MESSAGE("Testing if index has historical fixings...");

    auto testCase = [](const std::string& indexName, const bool& expected, const bool& testResult) {
        if (expected != testResult) {
            BOOST_FAIL("Historical fixing " << (testResult ? "" : "not ") << "found for "
                                            << indexName << ".");
        }
    };

    std::string name;
    auto fixingFound = true;
    auto fixingNotFound = false;

    auto euribor3M = ext::make_shared<Euribor3M>();
    auto euribor6M = ext::make_shared<Euribor6M>();
    auto euribor6M_a = ext::make_shared<Euribor6M>();

    Date today = Settings::instance().evaluationDate();
    while (!euribor6M->isValidFixingDate(today))
        today--;

    euribor6M->addFixing(today, 0.01);

    name = euribor3M->name();
    testCase(name, fixingNotFound, euribor3M->hasHistoricalFixing(today));

    name = euribor6M->name();
    testCase(name, fixingFound, euribor6M->hasHistoricalFixing(today));
    testCase(name, fixingFound, euribor6M_a->hasHistoricalFixing(today));

    IndexManager::instance().clearHistories();

    name = euribor3M->name();
    testCase(name, fixingNotFound, euribor3M->hasHistoricalFixing(today));

    name = euribor6M->name();
    testCase(name, fixingNotFound, euribor6M->hasHistoricalFixing(today));
    testCase(name, fixingNotFound, euribor6M_a->hasHistoricalFixing(today));
}

BOOST_AUTO_TEST_CASE(testTenorNormalization) {
    BOOST_TEST_MESSAGE("Testing that interest-rate index tenor is normalized correctly...");

    auto i12m = IborIndex("foo", 12*Months, 2, Currency(),
                          TARGET(), Following, false, Actual360());
    auto i1y = IborIndex("foo", 1*Years, 2, Currency(),
                         TARGET(), Following, false, Actual360());

    if (i12m.name() != i1y.name())
        BOOST_ERROR("12M index and 1Y index yield different names");


    auto i6d = IborIndex("foo", 6*Days, 2, Currency(),
                         TARGET(), Following, false, Actual360());
    auto i7d = IborIndex("foo", 7*Days, 2, Currency(),
                         TARGET(), Following, false, Actual360());

    Date testDate(28, April, 2023);
    Date maturity6d = i6d.maturityDate(testDate);
    Date maturity7d = i7d.maturityDate(testDate);

    if (maturity6d >= maturity7d) {
        BOOST_ERROR("inconsistent maturity dates and tenors"
                    << "\n  maturity date for 6-days index: " << maturity6d
                    << "\n  maturity date for 7-days index: " << maturity7d);
    }
}

BOOST_AUTO_TEST_CASE(testCustomIborIndex) {
    BOOST_TEST_MESSAGE("Testing CustomIborIndex...");

    auto fixCal = BespokeCalendar("Fixings");
    fixCal.addHoliday(Date(8, January, 2025));

    auto valCal = BespokeCalendar("Value");
    valCal.addHoliday(Date(21, January, 2025));

    auto matCal = BespokeCalendar("Maturity");
    matCal.addHoliday(Date(7, January, 2025));
    matCal.addHoliday(Date(15, January, 2025));
    matCal.addHoliday(Date(23, April, 2025));
    matCal.addHoliday(Date(30, April, 2025));

    auto ibor = CustomIborIndex(
        "Custom Ibor", 3*Months, 2, Currency(), fixCal, valCal, matCal, // NOLINT(cppcoreguidelines-slicing)
        ModifiedFollowing, true, Actual360()
    );
    auto iborClone = ibor.clone(Handle<YieldTermStructure>());

    for (IborIndex* index : {static_cast<IborIndex*>(&ibor), iborClone.get()}) {
        auto* as_custom = dynamic_cast<CustomIborIndex*>(index);
        BOOST_CHECK_EQUAL(index->fixingCalendar(), fixCal);
        BOOST_CHECK_EQUAL(as_custom->valueCalendar(), valCal);
        BOOST_CHECK_EQUAL(as_custom->maturityCalendar(), matCal);

        BOOST_CHECK_EXCEPTION(
            index->valueDate(Date(8, January, 2025)), Error,
            ExpectedErrorMessage("Fixing date January 8th, 2025 is not valid"));

        BOOST_CHECK_EQUAL(index->valueDate(Date(7, January, 2025)),
                          Date(9, January, 2025));
        BOOST_CHECK_EQUAL(index->valueDate(Date(13, January, 2025)),
                          Date(16, January, 2025));
        BOOST_CHECK_EQUAL(index->valueDate(Date(20, January, 2025)),
                          Date(23, January, 2025));

        BOOST_CHECK_EQUAL(index->fixingDate(Date(23, January, 2025)),
                          Date(20, January, 2025));
        BOOST_CHECK_EQUAL(index->fixingDate(Date(16, January, 2025)),
                          Date(14, January, 2025));
        BOOST_CHECK_EQUAL(index->fixingDate(Date(10, January, 2025)),
                          Date(7, January, 2025));

        BOOST_CHECK_EQUAL(index->maturityDate(Date(23, January, 2025)),
                          Date(24, April, 2025));
        BOOST_CHECK_EQUAL(index->maturityDate(Date(30, January, 2025)),
                          Date(29, April, 2025));
        BOOST_CHECK_EQUAL(index->maturityDate(Date(28, February, 2025)),
                          Date(31, May, 2025));
    }
}

BOOST_AUTO_TEST_CASE(testCdiIndex) {
    BOOST_TEST_MESSAGE("Testing Brazil CDI forecastFixing...");
    Date today = Settings::instance().evaluationDate();
    auto flatRate = ext::make_shared<SimpleQuote>(0.05);
    Handle<YieldTermStructure> ts(
        ext::make_shared<FlatForward>(today, Handle<Quote>(flatRate), Business252()));


    auto cdi = ext::make_shared<Cdi>(ts);
    auto testFixingDate = Brazil(Brazil::Settlement).advance(today, Period(1, Months));
    auto forecast = cdi->forecastFixing(testFixingDate);

    DiscountFactor discountStart = ts->discount(testFixingDate);
    DiscountFactor discountEnd = ts->discount(
        Brazil(Brazil::Settlement).advance(testFixingDate, Period(1, Days)));
    
    auto approx = pow(discountStart / discountEnd, 252.0) - 1.0;
    QL_ASSERT(std::fabs(0.05127 - forecast) < 1e-5, "discrepancy in fixing forecast computation\n");
    QL_ASSERT(std::fabs(approx - forecast) < 1e-6, "discrepancy in fixing forecast computation with approximation\n");
}

BOOST_AUTO_TEST_CASE(testFallbackIborIndex) {
    BOOST_TEST_MESSAGE("Testing FallbackIborIndex pre- and post-cessation behavior...");

    // Build a 3% flat curve for the legacy and a 2.5% flat curve for the RFR.
    Date today(15, April, 2023);
    Settings::instance().evaluationDate() = today;
    DayCounter dc = Actual360();

    auto legacyQuote = ext::make_shared<SimpleQuote>(0.03);
    Handle<YieldTermStructure> legacyCurve(
        ext::make_shared<FlatForward>(today, Handle<Quote>(legacyQuote), dc));
    auto rfrQuote = ext::make_shared<SimpleQuote>(0.025);
    Handle<YieldTermStructure> rfrCurve(
        ext::make_shared<FlatForward>(today, Handle<Quote>(rfrQuote), dc));

    auto usdLibor = ext::make_shared<USDLibor>(3 * Months, legacyCurve);
    auto sofr = ext::make_shared<Sofr>(rfrCurve);
    Date cessation(30, June, 2023);
    Spread isdaSpread = 0.0026161; // indicative ISDA USD 3M → SOFR spread

    auto fallback = ext::make_shared<FallbackIborIndex>(
        usdLibor, sofr, cessation, isdaSpread);

    // Pre-cessation: fallback must match the legacy USDLibor forecast exactly.
    Date preCess(15, May, 2023);
    Rate legacyRate = usdLibor->forecastFixing(preCess);
    Rate fallbackPre = fallback->forecastFixing(preCess);
    BOOST_CHECK_MESSAGE(std::fabs(legacyRate - fallbackPre) < 1e-12,
        "Pre-cessation: fallback rate " << fallbackPre
        << " does not match legacy " << legacyRate);

    // Post-cessation: the fallback rate should equal the compounded SOFR
    // over the 3M period plus the spread adjustment. For a flat 2.5% SOFR
    // curve the compounded rate is approximately 2.5% (simply-compounded
    // over 3M), so fallback ≈ 0.025 + 0.0026161 = 0.0276161.
    Date postCess(15, August, 2023);
    Rate fallbackPost = fallback->forecastFixing(postCess);
    BOOST_CHECK_MESSAGE(fallbackPost > 0.025,
        "Post-cessation fallback " << fallbackPost
        << " should exceed the raw SOFR rate 0.025 after adding the spread");
    BOOST_CHECK_MESSAGE(std::fabs(fallbackPost - (0.025 + isdaSpread)) < 5e-4,
        "Post-cessation fallback " << fallbackPost
        << " differs from expected " << (0.025 + isdaSpread)
        << " by more than 5 bps");

    // Inspectors.
    BOOST_CHECK_EQUAL(fallback->cessationDate(), cessation);
    BOOST_CHECK_CLOSE(fallback->spreadAdjustment(), isdaSpread, 1e-12);
    BOOST_CHECK(fallback->legacyIndex());
    BOOST_CHECK(fallback->rfrIndex());

    // Missing-RFR-curve guard.
    auto orphanSofr = ext::make_shared<Sofr>();
    auto fallbackNoCurve = ext::make_shared<FallbackIborIndex>(
        usdLibor, orphanSofr, cessation, isdaSpread);
    BOOST_CHECK_EXCEPTION(fallbackNoCurve->forecastFixing(postCess), Error,
        ExpectedErrorMessage("no forwarding term structure"));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
