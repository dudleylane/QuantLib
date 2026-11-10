/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Klaus Spanderen

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

/*! \file modifiedbessel.hpp
    \brief modified Bessel functions of first and second kind
*/

#ifndef quantlib_modified_bessel_hpp
#define quantlib_modified_bessel_hpp

#include <ql/types.hpp>
#include <complex>

namespace QuantLib
{

    /*  Compute modified Bessel functions I_nv(x) and K_nv(x)

        I_nv comes from its power series while |z|^2 <= 4 (|nv| + 1), and
        otherwise from the large-argument expansions (DLMF 10.40.2 and
        10.40.5) where they converge cleanly.  Failing that, I_nv of a real
        argument comes from Boost.Math, and a complex argument goes to
        Thompson and Barnett's algorithm (J. Comput. Phys. 64, 490, 1986):
        K at nv's distance from the nearest integer from Temme's series or
        Steed's continued fraction, K's recurrence up to nv, and I_nv from
        the continued fraction for I_nv+1 / I_nv and the Wronskian, all
        scaled by exp(z) so that nothing overflows early.  K_nv of a complex
        argument comes from the expansion or the same algorithm.  On the
        left half-plane both come from their values at -z (DLMF 10.34.1
        and 10.34.2), on the side of the cut that the sign of Im z gives.

        Measured against mpmath at orders -50.5 to 300 and |z| from 1e-3
        to 1000, the relative error is within 22 QL_EPSILON times the
        condition number max(1, |z f'(z) / f(z)|), and within 6.1 times it
        away from Thompson and Barnett's algorithm.  A real value too large
        to hold is infinite, and a complex one has an infinite modulus.

        K_nv of a real argument comes from Boost.Math.  It's infinite at
        zero, and a negative argument needs the complex version.

        The exponentially weighted versions return the function value
        times exp(-x) resp exp(-z)
     */
    Real modifiedBesselFunction_i(Real nu, Real x);
    Real modifiedBesselFunction_k(Real nu, Real x);
    Real modifiedBesselFunction_i_exponentiallyWeighted(Real nu, Real x);
    Real modifiedBesselFunction_k_exponentiallyWeighted(Real nu, Real x);

    std::complex<Real> modifiedBesselFunction_i(Real nu, const std::complex<Real>& z);
    std::complex<Real> modifiedBesselFunction_k(Real nu, const std::complex<Real>& z);
    std::complex<Real> modifiedBesselFunction_i_exponentiallyWeighted(Real nu, const std::complex<Real>& z);
    std::complex<Real> modifiedBesselFunction_k_exponentiallyWeighted(Real nu, const std::complex<Real>& z);
}
#endif
