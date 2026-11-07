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

#include <ql/math/expm1.hpp>
#include <ql/math/functional.hpp>
#include <cmath>

namespace QuantLib
{
    std::complex<Real> expm1(const std::complex<Real>& z)
    {
        if (std::abs(z) < 1.0)
        {
            const Real a = z.real(), b = z.imag();
            const Real exp_1 = std::expm1(a);
            const Real cos_1 = -2 * squared(std::sin(0.5 * b));

            return std::complex<Real>(exp_1 * cos_1 + exp_1 + cos_1, std::sin(b) * std::exp(a));
        }
        else
        {
            return std::exp(z) - 1.0;
        }
    }

    std::complex<Real> log1p(const std::complex<Real>& z)
    {
        const Real a = z.real(), b = z.imag();
        if (std::abs(a) < 0.5 && std::abs(b) < 0.5)
        {
            return std::complex<Real>(0.5 * std::log1p(a * a + 2 * a + b * b), std::arg(1.0 + z));
        }
        else
        {
            return std::log(1.0 + z);
        }
    }

    namespace detail
    {

        Real decayRatioDifference(Real u, Real v)
        {
            const Real s = u + v;
            if (std::fabs(s) > 2.0)
                return (decayRatio(u) - std::exp(-u) * decayRatio(v)) / s;
            // Here u and u + v are too far apart for the difference to cancel, and a large u would
            // take the series below too many terms.  This needs u and v of opposite signs.
            if (std::fabs(v) > 2.0)
                return (decayRatio(u) - decayRatio(s)) / v;

            // sum_(k>=1) (-1)^(k+1) R_k / (k+1)!, where R_k = ((u+v)^k - u^k) / v,
            // so that R_1 = 1 and R_(k+1) = (u+v) R_k + u^k
            Real R = 1.0, uPower = u, factorial = 2.0, sign = 1.0, sum = 0.0;
            for (Size k = 1; k < 60; ++k)
            {
                sum += sign * R / factorial;
                // At u + v = -u, every R_k with k even vanishes, so one small term doesn't end the series:
                // this looks at what the next one is made of as well.
                if ((std::fabs(R) + std::fabs(uPower)) / factorial < 0.01 * QL_EPSILON * std::fabs(sum))
                    break;
                R = s * R + uPower;
                uPower *= u;
                factorial *= k + 2;
                sign = -sign;
            }
            return sum;
        }

        void decayPhiAndH(Real x, Real& phi, Real& h)
        {
            if (std::fabs(x) < 1.0)
            {
                // Their Taylor series, in terms of p_k = (-x)^k / (k+3)!:
                //     phi(x) = sum_k (k+3) p_k
                //     h(x)   = sum_k (2^(k+3) - 4) p_k
                Real p = 1.0 / 6.0;
                Real twoPower = 8.0;
                phi = h = 0.0;
                for (Size k = 0; k < 40; ++k)
                {
                    const Real hTerm = (twoPower - 4.0) * p;
                    phi += (k + 3.0) * p;
                    h += hTerm;
                    if (std::fabs(hTerm) <= QL_EPSILON * std::fabs(h))
                        break;
                    p *= -x / (k + 4.0);
                    twoPower *= 2.0;
                }
            }
            else
            {
                const Real e1 = std::expm1(-x);
                const Real e2 = std::expm1(-2.0 * x);
                phi = (e1 + x) / (x * x);
                h = (4.0 * e1 - e2 + 2.0 * x) / (x * x * x);
            }
        }

    }
}
