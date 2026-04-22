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
#include <ql/pricingengines/blackformulatemplate.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/math/aad.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackFormulaTemplateTests)

BOOST_AUTO_TEST_CASE(testRealInstantiationMatchesBlackFormula) {
    BOOST_TEST_MESSAGE("blackFormulaT<Real> matches blackFormula() to "
                       "machine precision...");

    const Real strike = 100.0;
    const Real forward = 105.0;
    const Real stddev = 0.20 * std::sqrt(0.5);
    const DiscountFactor df = std::exp(-0.03 * 0.5);

    Real callT = blackFormulaT<Real>(Option::Call, strike, forward, stddev, df);
    Real putT  = blackFormulaT<Real>(Option::Put,  strike, forward, stddev, df);

    Real callRef = blackFormula(Option::Call, strike, forward, stddev) * df;
    Real putRef  = blackFormula(Option::Put,  strike, forward, stddev) * df;

    BOOST_CHECK_SMALL(std::fabs(callT - callRef), 1.0e-12);
    BOOST_CHECK_SMALL(std::fabs(putT  - putRef),  1.0e-12);
}

BOOST_AUTO_TEST_CASE(testPutCallParity) {
    BOOST_TEST_MESSAGE("blackFormulaT respects put-call parity for "
                       "any (forward, strike) ...");

    const Real strike = 95.0;
    const Real forward = 100.0;
    const Real stddev = 0.30;
    const DiscountFactor df = 0.98;

    Real call = blackFormulaT<Real>(Option::Call, strike, forward, stddev, df);
    Real put  = blackFormulaT<Real>(Option::Put,  strike, forward, stddev, df);
    // Parity: Call - Put = df * (F - K)
    Real lhs = call - put;
    Real rhs = df * (forward - strike);
    BOOST_CHECK_SMALL(std::fabs(lhs - rhs), 1.0e-12);
}

#ifdef QL_ENABLE_AAD

BOOST_AUTO_TEST_CASE(testAadDeltaMatchesAnalyticDelta) {
    BOOST_TEST_MESSAGE("blackFormulaT<AADReal> with reverse-mode AAD "
                       "produces dPrice/dForward = df * N(d1)...");

    const Real strike = 100.0;
    const Real forward = 105.0;
    const Real stddev = 0.20 * std::sqrt(0.5);
    const DiscountFactor df = std::exp(-0.03 * 0.5);

    // Differentiate the Black formula w.r.t. the forward.
    auto pricer = [&](AADReal F) {
        // strike, stddev, df held as constants (deduce the
        // appropriate AADReal types where the templated function
        // requires them).
        AADReal sd = stddev;
        AADReal d  = df;
        return blackFormulaT<AADReal>(Option::Call, strike, F, sd, d);
    };
    Real aadDelta = aadDerivative(pricer, forward);

    // Analytic Black delta on the forward measure: df * N(d1)
    Real d1 = std::log(forward / strike) / stddev + 0.5 * stddev;
    CumulativeNormalDistribution N;
    Real analyticDelta = df * N(d1);

    BOOST_CHECK_SMALL(std::fabs(aadDelta - analyticDelta), 1.0e-10);
}

#endif // QL_ENABLE_AAD

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
