/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2023 Klaus Spanderen

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

/*! \file expm1.hpp
   \brief complex versions of expm1 and logp1, and functions of exp(-x) that cancel for small x
*/

#ifndef quantlib_expm1_hpp
#define quantlib_expm1_hpp

#include <ql/types.hpp>
#include <cmath>
#include <complex>

namespace QuantLib
{
    std::complex<Real> expm1(const std::complex<Real>& z);
    std::complex<Real> log1p(const std::complex<Real>& z);

    namespace detail
    {

        // f(x) = (1 - exp(-x)) / x
        inline Real decayRatio(Real x)
        {
            return x == 0.0 ? 1.0 : -std::expm1(-x) / x;
        }

        // (1 - exp(-c x)) / c
        inline Real decayIntegral(Real c, Real x)
        {
            return x * decayRatio(c * x);
        }

        // (f(u) - f(u+v)) / v, which is also the integral of w exp(-u w) f(v w)
        // between 0 and 1.  The difference cancels for small u and v.
        Real decayRatioDifference(Real u, Real v);

        // phi(x) = (exp(-x) - 1 + x) / x^2
        // h(x)   = (4 exp(-x) - exp(-2x) - 3 + 2x) / x^3
        // These closed forms cancel catastrophically for small x.
        void decayPhiAndH(Real x, Real& phi, Real& h);

    }
}
#endif
