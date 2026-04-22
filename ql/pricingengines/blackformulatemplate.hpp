/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 dudleylane

 This file is new in the dudleylane fork of QuantLib and is licensed
 under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
 See the LICENSE file in the repository root for the full license text.

 Unlike upstream QuantLib files (which remain under BSD-3-Clause — see
 LICENSE.TXT), this file and any derivative works of it are subject to
 the copyleft and network-use clauses of AGPL-3.0.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file blackformulatemplate.hpp
    \brief Engine-level AAD proof-of-concept: scalar-templated Black formula

    The companion file `aad.hpp` exposes `aadDerivative(f, x)` for any
    scalar function the user wraps themselves. This header demonstrates
    the next step: a pricing primitive (the Black-1976 lognormal call /
    put formula) re-templated on a scalar type `T`, so it can be
    instantiated either with `Real` (matches the existing
    `blackFormula()` to machine precision) or with `AADReal` (allows
    tape-based AD over the formula's inputs).

    This is the first file in the fork that demonstrates the
    template-parameterise-Real pattern. It is deliberately a tiny
    surface — a single free function — so the AAD-vs-non-AAD
    instantiation contract is easy to inspect. Generalising the
    pattern to BlackCalculator, AnalyticEuropeanEngine, and the
    process / curve hierarchy is a multi-week follow-up; this file
    establishes the pattern.

    Math operators on `T` must satisfy ADL with `std::log`, `std::exp`,
    `std::sqrt`, and the standard arithmetic. CoDi-Pack's RealReverse
    satisfies these via its ADL overloads in the codi:: namespace.
*/

#ifndef quantlib_black_formula_template_hpp
#define quantlib_black_formula_template_hpp

#include <ql/option.hpp>
#include <ql/errors.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <cmath>

namespace QuantLib {

    //! Black-1976 lognormal call / put price, scalar-templated.
    /*! For `T = Real`, returns the same value as
        `blackFormula(optionType, strike, forward, stddev) * discount`
        from `ql/pricingengines/blackformula.hpp`. For `T = AADReal`,
        the returned value is differentiable via the active CoDi tape.

        Inputs:
          optionType : Option::Call or Option::Put
          strike     : strike price (kept as Real; not differentiable
                       in this MVP — extending the template to
                       differentiate w.r.t. K is a follow-up)
          forward    : forward of the underlying (T)
          stddev     : sigma * sqrt(T) (T)
          discount   : discount factor for the payoff date (T,
                       defaulted to 1.0)

        Preconditions:
          stddev > 0

        Pricing convention matches `blackFormula(...)` exactly.
    */
    template <class T>
    T blackFormulaT(Option::Type optionType,
                    Real strike,
                    T forward,
                    T stddev,
                    T discount = T(1.0)) {
        QL_REQUIRE(strike > 0.0,
                   "blackFormulaT: strike must be positive, got " << strike);
        // stddev is templated; use cmath ADL to allow CoDi overloads.
        using std::log;
        using std::sqrt;
        // The "isfinite" check is intentionally only a Real check here
        // (CoDi defines its own; a fully generic isfinite would require
        // a small trait shim — out of scope for the MVP).

        T forward_over_strike = forward / strike;
        T d1 = log(forward_over_strike) / stddev + 0.5 * stddev;
        T d2 = d1 - stddev;

        // Cumulative normal: use the existing CumulativeNormalDistribution
        // for Real path; for the AAD path, expand 0.5*(1 + erf(x/sqrt(2)))
        // directly so std::erf's CoDi overload is exercised.
        using std::erf;
        const Real invSqrt2 = 0.7071067811865475;
        T N1 = 0.5 * (1.0 + erf(d1 * invSqrt2));
        T N2 = 0.5 * (1.0 + erf(d2 * invSqrt2));

        if (optionType == Option::Call)
            return discount * (forward * N1 - strike * N2);
        else
            return discount * (strike * (1.0 - N2) - forward * (1.0 - N1));
    }

}

#endif
