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
#include <ql/risk/xvacalculator.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(XvaCalculatorTests)

namespace {

    struct Fixture {
        Date today{20, April, 2026};
        DayCounter dc = Actual365Fixed();
        std::vector<Date> dates;
        std::vector<Real> epe, ene;
        Handle<DefaultProbabilityTermStructure> cptyPd, ownPd;
        Handle<YieldTermStructure> discount;

        Fixture(Real flatEpe = 100.0, Real flatEne = 0.0,
                Rate cptyHaz = 0.02, Rate ownHaz = 0.01,
                Rate r = 0.03) {
            Settings::instance().evaluationDate() = today;
            dates = {today,
                     today + Period(1, Years),
                     today + Period(2, Years),
                     today + Period(3, Years),
                     today + Period(4, Years),
                     today + Period(5, Years)};
            epe.assign(dates.size(), flatEpe);
            ene.assign(dates.size(), flatEne);

            cptyPd = Handle<DefaultProbabilityTermStructure>(
                ext::make_shared<FlatHazardRate>(today, cptyHaz, dc));
            ownPd = Handle<DefaultProbabilityTermStructure>(
                ext::make_shared<FlatHazardRate>(today, ownHaz, dc));
            discount = Handle<YieldTermStructure>(
                ext::make_shared<FlatForward>(today, r, dc));
        }
    };

}

BOOST_AUTO_TEST_CASE(testCvaIsPositiveForPositiveExposure) {
    BOOST_TEST_MESSAGE("CVA > 0 when EPE > 0 and counterparty can default...");

    Fixture f(/*EPE*/ 100.0, /*ENE*/ 0.0);
    XvaCalculator x(f.dates, f.epe, f.ene, f.cptyPd, f.ownPd,
                    /*cptyRec*/ 0.4, /*ownRec*/ 0.4,
                    /*fund*/ 0.005, /*cap*/ 0.01, /*margin*/ 0.003,
                    f.discount);
    BOOST_CHECK(x.cva() > 0.0);
    BOOST_CHECK_EQUAL(x.dva(), 0.0);   // ENE = 0
    BOOST_CHECK(x.fva() > 0.0);
    BOOST_CHECK(x.kva() > 0.0);
    BOOST_CHECK_EQUAL(x.mva(), 0.0);   // no IM provided
}

BOOST_AUTO_TEST_CASE(testCvaScalesWithLGD) {
    BOOST_TEST_MESSAGE("CVA ~ (1 - recovery) when other inputs held fixed...");

    Fixture f(/*EPE*/ 100.0);
    XvaCalculator lowRec(f.dates, f.epe, f.ene, f.cptyPd, f.ownPd,
                         /*cptyRec*/ 0.0, /*ownRec*/ 0.4,
                         0.0, 0.0, 0.0, f.discount);
    XvaCalculator highRec(f.dates, f.epe, f.ene, f.cptyPd, f.ownPd,
                          /*cptyRec*/ 0.8, /*ownRec*/ 0.4,
                          0.0, 0.0, 0.0, f.discount);
    // LGD = 1.0 vs 0.2 -> CVA ratio should be 5.0 exactly.
    Real ratio = lowRec.cva() / highRec.cva();
    BOOST_CHECK_MESSAGE(std::fabs(ratio - 5.0) < 1e-12,
        "CVA LGD scaling ratio " << ratio << " != 5.0");
}

BOOST_AUTO_TEST_CASE(testDvaSymmetricToCva) {
    BOOST_TEST_MESSAGE("DVA with ENE and own hazard mirrors CVA structure...");

    // Set EPE = ENE, cpty and own hazard equal, cpty and own recovery
    // equal. CVA and DVA must then coincide.
    Date today{20, April, 2026};
    Settings::instance().evaluationDate() = today;
    DayCounter dc = Actual365Fixed();
    std::vector<Date> dates = {today, today + Period(1, Years),
                               today + Period(2, Years),
                               today + Period(3, Years)};
    std::vector<Real> exposures(dates.size(), 100.0);
    auto pd = Handle<DefaultProbabilityTermStructure>(
        ext::make_shared<FlatHazardRate>(today, 0.02, dc));
    auto discount = Handle<YieldTermStructure>(
        ext::make_shared<FlatForward>(today, 0.03, dc));

    XvaCalculator x(dates, exposures, exposures, pd, pd, 0.4, 0.4,
                    0.0, 0.0, 0.0, discount);
    BOOST_CHECK_MESSAGE(std::fabs(x.cva() - x.dva()) < 1e-10,
        "symmetric-input CVA=" << x.cva() << " vs DVA=" << x.dva());
}

BOOST_AUTO_TEST_CASE(testMvaZeroWithoutInitialMargin) {
    BOOST_TEST_MESSAGE("MVA is zero when no initial-margin profile is given...");

    Fixture f;
    XvaCalculator noIm(f.dates, f.epe, f.ene, f.cptyPd, f.ownPd,
                       0.4, 0.4, 0.0, 0.0, 0.01, f.discount);
    BOOST_CHECK_EQUAL(noIm.mva(), 0.0);

    std::vector<Real> im(f.dates.size(), 50.0);
    XvaCalculator withIm(f.dates, f.epe, f.ene, f.cptyPd, f.ownPd,
                         0.4, 0.4, 0.0, 0.0, 0.01, f.discount, im);
    BOOST_CHECK(withIm.mva() > 0.0);
}

BOOST_AUTO_TEST_CASE(testConstructorRejectsInvalidInputs) {
    Fixture f;
    auto badRecovery = [&]{
        XvaCalculator x(f.dates, f.epe, f.ene, f.cptyPd, f.ownPd,
                        1.5, 0.4, 0.0, 0.0, 0.0, f.discount);
        (void)x;
    };
    BOOST_CHECK_THROW(badRecovery(), Error);

    std::vector<Real> shortEpe{100.0, 100.0};
    auto badSize = [&]{
        XvaCalculator x(f.dates, shortEpe, f.ene, f.cptyPd, f.ownPd,
                        0.4, 0.4, 0.0, 0.0, 0.0, f.discount);
        (void)x;
    };
    BOOST_CHECK_THROW(badSize(), Error);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
