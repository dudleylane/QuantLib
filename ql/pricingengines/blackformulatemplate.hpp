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

#include <ql/errors.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/option.hpp>
#include <cmath>
#include <type_traits>

namespace QuantLib
{

    //! Black-1976 lognormal call / put price, scalar-templated.
    /*! \ingroup engines */
    /*! For `T = Real`, returns the same value as
        `blackFormula(optionType, strike, forward, stddev, discount, displacement)`
        from `ql/pricingengines/blackformula.hpp` (same cumulative-normal
        approximation; same displaced-lognormal handling; same `stddev==0`
        short-circuit). For `T = AADReal`, the returned value is
        differentiable via the active CoDi tape. The AAD branch uses
        `std::erf` (CoDi provides an overload) rather than
        `CumulativeNormalDistribution`, which is not tape-aware; the
        Real branch uses `CumulativeNormalDistribution` to match
        `blackFormula()` bit-for-bit.

        Inputs:
          optionType   : Option::Call or Option::Put
          strike       : strike price (kept as Real; not differentiable
                         in this MVP — extending the template to
                         differentiate w.r.t. K is a follow-up)
          forward      : forward of the underlying (T)
          stddev       : sigma * sqrt(T) (T)
          discount     : discount factor for the payoff date (T,
                         defaulted to 1.0)
          displacement : displaced-lognormal shift (Real, defaulted to 0).
                         Non-zero enables negative-rate / shifted-SABR
                         pricing. Matches upstream semantics:
                         `f' = f + s`, `k' = k + s`.

        Preconditions (enforced):
          stddev >= 0 (stddev == 0 short-circuits to discount * intrinsic)
          strike + displacement >= 0
          forward + displacement > 0 (when stddev > 0)

        Pricing convention matches `blackFormula(...)` exactly for
        `T = Real`.
    */
    template <class T>
    T blackFormulaT(
        Option::Type optionType, Real strike, T forward, T stddev, T discount = T(1.0), Real displacement = 0.0)
    {
        QL_REQUIRE(strike + displacement >= 0.0, "blackFormulaT: strike + displacement must be non-negative "
                                                 "(strike="
                                                     << strike << ", displacement=" << displacement << ")");

        // stddev == 0: return discount * intrinsic. Displacements cancel in
        // the intrinsic (f+s) - (k+s) = f-k, so we use the undisplaced diff.
        if (stddev == T(0.0))
        {
            if (optionType == Option::Call)
            {
                T diff = forward - strike;
                return discount * ((diff > T(0.0)) ? diff : T(0.0));
            }
            else
            {
                T diff = strike - forward;
                return discount * ((diff > T(0.0)) ? diff : T(0.0));
            }
        }
        QL_REQUIRE(stddev > T(0.0), "blackFormulaT: stddev must be non-negative");

        T forward_adj = forward + displacement;
        T strike_adj = strike + displacement;
        QL_REQUIRE(forward_adj > T(0.0), "blackFormulaT: forward + displacement must be positive "
                                         "(forward="
                                             << forward << ", displacement=" << displacement << ")");

        using std::log;
        T d1 = log(forward_adj / strike_adj) / stddev + 0.5 * stddev;
        T d2 = d1 - stddev;

        T N1, N2;
        if constexpr (std::is_same_v<T, Real>)
        {
            // Real path: match blackFormula()'s CumulativeNormalDistribution
            // approximation bit-for-bit.
            CumulativeNormalDistribution N;
            N1 = N(d1);
            N2 = N(d2);
        }
        else
        {
            // AAD path: inline 0.5*(1 + erf(x/sqrt(2))) so std::erf's CoDi
            // overload propagates derivatives through the tape.
            using std::erf;
            const Real invSqrt2 = 0.7071067811865475;
            N1 = 0.5 * (1.0 + erf(d1 * invSqrt2));
            N2 = 0.5 * (1.0 + erf(d2 * invSqrt2));
        }

        if (optionType == Option::Call)
            return discount * (forward_adj * N1 - strike_adj * N2);
        else
            return discount * (strike_adj * (T(1.0) - N2) - forward_adj * (T(1.0) - N1));
    }

}

#endif
