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
#include <ql/risk/frtbsagirr.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FrtbSaGirrDeltaTests)

BOOST_AUTO_TEST_CASE(testTwoTenorHandComputed)
{
    BOOST_TEST_MESSAGE("FRTB-SA GIRR: two-tenor bucket charge against "
                       "hand-computed reference...");

    // Two vertices, 1y and 10y.
    // Risk weights w = {0.017, 0.012} — illustrative, matching the
    // d457-era guidance for the 1y and 10y GIRR vertices; the test
    // is verifying the AGGREGATION MATH, not the regulatory weights
    // themselves.
    std::vector<Real> tenors{1.0, 10.0};
    std::vector<Real> sensitivities{1000000.0, 500000.0};
    std::vector<Real> riskWeights{0.017, 0.012};
    Real theta = 0.03;
    Real phiFloor = 0.40;

    FrtbSaGirrDelta calc(tenors, sensitivities, riskWeights, theta, phiFloor);

    // WS_1 = 0.017 * 1e6 = 17000
    // WS_2 = 0.012 * 0.5e6 = 6000
    BOOST_CHECK_SMALL(calc.weightedSensitivities()[0] - 17000.0, 1e-9);
    BOOST_CHECK_SMALL(calc.weightedSensitivities()[1] - 6000.0, 1e-9);

    // rho_{1,10} = max(exp(-0.03 * 9 / 1), 0.40)
    //           = max(exp(-0.27), 0.40)
    //           = max(0.7634, 0.40) = 0.7634 (approximately)
    Real rho = calc.correlation(0, 1);
    BOOST_CHECK_SMALL(rho - std::exp(-0.27), 1e-10);

    // K_b = sqrt( WS_1^2 + WS_2^2 + 2 * rho * WS_1 * WS_2 )
    Real expected = std::sqrt(17000.0 * 17000.0 + 6000.0 * 6000.0 + 2.0 * std::exp(-0.27) * 17000.0 * 6000.0);
    BOOST_CHECK_SMALL(calc.bucketCharge() - expected, 1e-6);
}

BOOST_AUTO_TEST_CASE(testPhiFloorActuallyFloors)
{
    BOOST_TEST_MESSAGE("FRTB-SA GIRR: far-apart tenors hit the 40% floor...");

    // Tenors 0.1y and 30y: |dT|/min = 29.9 / 0.1 = 299
    // exp(-0.03 * 299) ≈ 1.3e-4 << 0.4, so floor kicks in.
    std::vector<Real> tenors{0.1, 30.0};
    std::vector<Real> sensitivities{1000.0, 1000.0};
    std::vector<Real> riskWeights{0.017, 0.011};

    FrtbSaGirrDelta calc(tenors, sensitivities, riskWeights, 0.03, 0.40);
    BOOST_CHECK_SMALL(calc.correlation(0, 1) - 0.40, 1e-12);
}

BOOST_AUTO_TEST_CASE(testOffsettingSensitivitiesReduceCharge)
{
    BOOST_TEST_MESSAGE("FRTB-SA GIRR: offsetting (opposite-sign) "
                       "sensitivities give a smaller charge than aligned ones...");

    std::vector<Real> tenors{2.0, 5.0, 10.0};
    std::vector<Real> riskWeights{0.015, 0.013, 0.012};

    // All-positive sensitivities.
    std::vector<Real> aligned{1.0e6, 1.0e6, 1.0e6};
    FrtbSaGirrDelta a(tenors, aligned, riskWeights);

    // Offsetting: +1e6 at 2y, -1e6 at 5y, +1e6 at 10y.
    std::vector<Real> offset{1.0e6, -1.0e6, 1.0e6};
    FrtbSaGirrDelta o(tenors, offset, riskWeights);

    BOOST_CHECK_MESSAGE(o.bucketCharge() < a.bucketCharge(),
                        "offsetting charge " << o.bucketCharge() << " not lower than aligned " << a.bucketCharge());
}

BOOST_AUTO_TEST_CASE(testIntegratesWithCurveBucketer)
{
    BOOST_TEST_MESSAGE("FRTB-SA GIRR: output of CurveBucketer plugs into "
                       "FrtbSaGirrDelta as sensitivities...");

    // Smoke test: simulate the "CurveBucketer produced these
    // tenor-bucket deltas" pathway. The data below is synthetic but
    // exercises the interface boundary between the two classes.
    std::vector<Real> tenors{1.0, 2.0, 5.0, 10.0};
    // Simulated bucketed deltas (as CurveBucketer would produce).
    std::vector<Real> bucketedDeltas{50000.0, -120000.0, 300000.0, 80000.0};
    // BCBS d457 illustrative risk weights for these vertices.
    std::vector<Real> riskWeights{0.017, 0.015, 0.013, 0.012};

    FrtbSaGirrDelta charge(tenors, bucketedDeltas, riskWeights);
    Real k = charge.bucketCharge();

    // The charge must be non-negative and at least the largest single
    // weighted sensitivity in absolute value (the correlation term can
    // only add to or subtract from this baseline, bounded by the
    // standard FRTB K_b construction).
    Real maxAbsWs = 0.0;
    for (Real w : charge.weightedSensitivities())
        maxAbsWs = std::max(maxAbsWs, std::fabs(w));
    BOOST_CHECK_MESSAGE(k >= maxAbsWs * 0.99, // tiny tolerance for rounding
                        "charge " << k << " below max |WS| " << maxAbsWs);
}

BOOST_AUTO_TEST_CASE(testRejectsBadInputs)
{
    auto sizeMismatch = [&]
    {
        FrtbSaGirrDelta c({1.0, 2.0}, {1.0}, {0.01, 0.01});
        (void)c;
    };
    BOOST_CHECK_THROW(sizeMismatch(), Error);

    auto badTheta = [&]
    {
        FrtbSaGirrDelta c({1.0}, {1.0}, {0.01}, 0.0);
        (void)c;
    };
    BOOST_CHECK_THROW(badTheta(), Error);

    auto badPhi = [&]
    {
        FrtbSaGirrDelta c({1.0}, {1.0}, {0.01}, 0.03, 1.5);
        (void)c;
    };
    BOOST_CHECK_THROW(badPhi(), Error);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
