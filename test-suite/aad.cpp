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
#include <ql/math/aad.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AadTests)

#ifdef QL_ENABLE_AAD

BOOST_AUTO_TEST_CASE(testDerivativeOfSquare)
{
    BOOST_TEST_MESSAGE("AAD: d/dx (x^2) at x=3 -> 6...");
    auto f = [](AADReal x) { return x * x; };
    Real d = aadDerivative(f, 3.0);
    BOOST_CHECK_SMALL(std::fabs(d - 6.0), 1e-12);
}

BOOST_AUTO_TEST_CASE(testDerivativeOfExp)
{
    BOOST_TEST_MESSAGE("AAD: d/dx exp(x) at x=1 -> e...");
    // `exp` unqualified to let ADL pick CoDi's overload for AADReal.
    // `std::exp` alone would only accept floating types.
    auto f = [](AADReal x)
    {
        using std::exp;
        return exp(x);
    };
    Real d = aadDerivative(f, 1.0);
    BOOST_CHECK_SMALL(std::fabs(d - std::exp(1.0)), 1e-12);
}

BOOST_AUTO_TEST_CASE(testDerivativeOfInverseChain)
{
    BOOST_TEST_MESSAGE("AAD: d/dx (1/x + x^3) at x=2 -> -0.25 + 12 = 11.75...");
    // A composed function that exercises both reciprocal and power
    // rules on the AAD tape without pulling in std::log/std::erf
    // (which have ADL conflicts with boost::unit_test::log in the
    // test-suite translation unit).
    auto f = [](AADReal x) { return 1.0 / x + x * x * x; };
    Real d = aadDerivative(f, 2.0);
    BOOST_CHECK_SMALL(std::fabs(d - 11.75), 1e-12);
}

#else // QL_ENABLE_AAD disabled

BOOST_AUTO_TEST_CASE(testDisabledPathThrows)
{
    BOOST_TEST_MESSAGE("AAD disabled: aadDerivative() must throw with "
                       "a clear message...");
    auto f = [](Real x) { return x * x; };
    BOOST_CHECK_EXCEPTION(aadDerivative(f, 3.0), Error, ExpectedErrorMessage("AAD is not enabled"));
}

#endif // QL_ENABLE_AAD

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
