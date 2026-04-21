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

/*! \file aad.hpp
    \brief Optional reverse-mode AAD support via CoDi-Pack

    Enabled with the CMake option `QL_ENABLE_AAD=ON`, which in turn
    makes CoDi-Pack available (either through an installed copy or
    via `FetchContent`). When enabled, `QL_ENABLE_AAD` is set as a
    preprocessor macro and the `codi.hpp` header is on the include
    path.

    MVP scope: a single utility `aadDerivative(f, x)` that computes
    df/dx via reverse-mode tape for an arbitrary `AADReal -> AADReal`
    functor. It is NOT a full retrofit of the QuantLib pricing stack
    to use AAD types; that is a multi-week effort that touches every
    engine. This utility lets users wrap their own pricing functions
    (or use CoDi's operator overloading directly on `double`-like
    scalars) without forking every QuantLib class. Extending the
    scope to whole-engine AAD is a tracked follow-up.

    When `QL_ENABLE_AAD` is OFF the header still compiles, but
    `aadDerivative` throws at runtime with a message pointing the
    user at the configure flag. This keeps downstream code
    unconditionally compilable.
*/

#ifndef quantlib_aad_hpp
#define quantlib_aad_hpp

#include <ql/types.hpp>
#include <ql/errors.hpp>

#ifdef QL_ENABLE_AAD

#include <codi.hpp>

namespace QuantLib {

    //! Reverse-mode AAD scalar type (alias for CoDi's RealReverse).
    using AADReal = codi::RealReverse;

    //! Compute f'(x) via reverse-mode AAD tape.
    /*! `F` must be callable with a single `AADReal` argument and return
        an `AADReal`. The tape is set active, the input registered,
        `f(x)` evaluated, the output registered, the tape deactivated
        and evaluated, and the input's gradient read back. The tape is
        reset before return so consecutive calls remain independent.

        Not thread-safe (CoDi's default tape is thread_local on recent
        versions, but this utility does not attempt to lock across
        user-provided functor bodies).
    */
    template <class F>
    Real aadDerivative(F&& f, Real x) {
        auto& tape = AADReal::getTape();
        tape.setActive();
        AADReal xa = x;
        tape.registerInput(xa);
        AADReal y = f(xa);
        tape.registerOutput(y);
        tape.setPassive();
        y.setGradient(Real(1.0));
        tape.evaluate();
        Real grad = xa.getGradient();
        tape.reset();
        return grad;
    }

}

#else // QL_ENABLE_AAD not defined

namespace QuantLib {

    //! Dummy AAD type when AAD is disabled (falls back to Real).
    using AADReal = Real;

    template <class F>
    Real aadDerivative(F&&, Real) {
        QL_FAIL("AAD is not enabled in this build. Reconfigure with "
                "-DQL_ENABLE_AAD=ON to pull in CoDi-Pack and compile "
                "the reverse-mode tape.");
    }

}

#endif // QL_ENABLE_AAD

#endif
