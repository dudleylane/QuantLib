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

/*! \file modifiedbessel.cpp
    \brief modified Bessel functions of first and second kind
*/

#include <ql/math/distributions/gammadistribution.hpp>
#include <ql/math/modifiedbessel.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace QuantLib
{

    namespace
    {

        template <class T>
        struct I
        {
        };
        template <>
        struct I<Real>
        {
            Real value() { return 0.0; }
        };
        template <>
        struct I<std::complex<Real>>
        {
            std::complex<Real> value() { return std::complex<Real>(0.0, 1.0); }
        };
        template <class T>
        struct Unweighted
        {
            T weightSmallX(const T& x) { return 1.0; }
            T weight1LargeX(const T& x) { return std::exp(x); }
            T weight2LargeX(const T& x) { return std::exp(-x); }
        };
        template <class T>
        struct ExponentiallyWeighted
        {
            T weightSmallX(const T& x) { return std::exp(-x); }
            T weight1LargeX(const T& x) { return 1.0; }
            T weight2LargeX(const T& x) { return std::exp(-2.0 * x); }
        };

        template <class T, template <class> class W>
        T modifiedBesselFunction_i_impl(Real nu, const T& x)
        {
            if (std::abs(x) < 13.0)
            {
                // The series divides by zero at a negative integer order, where I_-n = I_n.
                if (nu < 0.0 && nu == std::floor(nu))
                    nu = -nu;

                const T alpha = std::pow(0.5 * x, nu) / GammaFunction().value(1.0 + nu);
                const T Y = 0.25 * x * x;
                Size k = 1;
                T sum = alpha, B_k = alpha;

                while (std::abs(B_k *= Y / (k * (k + nu))) > std::abs(sum) * QL_EPSILON)
                {
                    sum += B_k;
                    QL_REQUIRE(++k < 1000, "max iterations exceeded");
                }
                return sum * W<T>().weightSmallX(x);
            }
            else
            {
                Real na_k = 1.0, sign = 1.0;
                T da_k = T(1.0);

                T s1 = T(1.0), s2 = T(1.0);
                for (Size k = 1; k < 30; ++k)
                {
                    sign *= -1;
                    na_k *= (4.0 * nu * nu - (2.0 * static_cast<Real>(k) - 1.0) * (2.0 * static_cast<Real>(k) - 1.0));
                    da_k *= (8.0 * k) * x;
                    const T a_k = na_k / da_k;

                    s2 += a_k;
                    s1 += sign * a_k;
                }

                const T i = I<T>().value();
                return 1.0 / std::sqrt(2 * M_PI * x) *
                       (W<T>().weight1LargeX(x) * s1 + i * std::exp(i * nu * M_PI) * W<T>().weight2LargeX(x) * s2);
            }
        }

        // K_0 and K_1 from Abramowitz & Stegun 9.6.13 and 9.6.11, then the recurrence, which is
        // stable upwards.
        template <class T>
        T modifiedBesselFunction_k_integer(Size n, const T& x)
        {
            const T half = 0.5 * x;
            const T y = half * half;
            const T logHalf = std::log(half);

            // h is the sum of 1/j up to k, so psi(k + 1) = h - euler and
            // psi(k + 2) = psi(k + 1) + 1 / (k + 1).
            const Real euler = 0.57721566490153286060651209008240243;
            T sum0 = T(0.0), sum1 = T(0.0), term0 = T(1.0), term1 = T(1.0);
            Real h = 0.0;
            for (Size k = 0; k < 1000; ++k)
            {
                const T delta0 = term0 * (h - euler);
                const T delta1 = term1 * (2.0 * (h - euler) + 1.0 / (k + 1.0));
                sum0 += delta0;
                sum1 += delta1;
                if (std::abs(delta0) <= std::abs(sum0) * QL_EPSILON && std::abs(delta1) <= std::abs(sum1) * QL_EPSILON)
                    break;
                term0 *= y / (Real(k + 1) * (k + 1));
                term1 *= y / (Real(k + 1) * (k + 2));
                h += 1.0 / (k + 1.0);
            }

            T k0 = sum0 - logHalf * modifiedBesselFunction_i_impl<T, Unweighted>(0.0, x);
            if (n == 0)
                return k0;
            T k1 = 1.0 / x + logHalf * modifiedBesselFunction_i_impl<T, Unweighted>(1.0, x) - 0.5 * half * sum1;
            for (Size m = 1; m < n; ++m)
            {
                const T next = k0 + (2.0 * m / x) * k1;
                k0 = k1;
                k1 = next;
            }
            return k1;
        }

        template <class T, template <class> class W>
        T modifiedBesselFunction_k_impl(Real nu, const T& x)
        {
            // Past the switch in I's expansions the quotient below comes to this, apart from its
            // rounding, which grows as exp(2 x).
            if (std::abs(x) >= 13.0)
            {
                Real na_k = 1.0;
                T da_k = T(1.0), s = T(1.0);
                for (Size k = 1; k < 30; ++k)
                {
                    na_k *= (4.0 * nu * nu - (2.0 * static_cast<Real>(k) - 1.0) * (2.0 * static_cast<Real>(k) - 1.0));
                    da_k *= (8.0 * k) * x;
                    s += na_k / da_k;
                }
                return std::sqrt(M_PI / (2.0 * x)) * W<T>().weight2LargeX(x) * s;
            }

            // The quotient is 0/0 at an integer order.  Close to one its rounding goes as
            // QL_EPSILON * |I / K| / distance, while taking the integer order itself is off by about
            // the distance, so this is where the two meet, given that I / K goes as exp(2 x).
            const Real nearest = std::floor(std::abs(nu) + 0.5);
            if (std::abs(std::abs(nu) - nearest) < std::sqrt(QL_EPSILON) * std::exp(std::max(std::real(x), 0.0)))
                return modifiedBesselFunction_k_integer(static_cast<Size>(nearest), x) * W<T>().weightSmallX(x);

            return M_PI_2 * (modifiedBesselFunction_i_impl<T, W>(-nu, x) - modifiedBesselFunction_i_impl<T, W>(nu, x)) /
                   std::sin(M_PI * nu);
        }
    }

    Real modifiedBesselFunction_i(Real nu, Real x)
    {
        QL_REQUIRE(x >= 0.0, "negative argument requires complex version of "
                             "modifiedBesselFunction");
        return modifiedBesselFunction_i_impl<Real, Unweighted>(nu, x);
    }

    std::complex<Real> modifiedBesselFunction_i(Real nu, const std::complex<Real>& z)
    {
        if (z.imag() == 0.0 && z.real() >= 0.0)
            return std::complex<Real>(modifiedBesselFunction_i(nu, z.real()));

        return modifiedBesselFunction_i_impl<std::complex<Real>, Unweighted>(nu, z);
    }

    Real modifiedBesselFunction_k(Real nu, Real x)
    {
        QL_REQUIRE(x >= 0.0, "negative argument requires complex version of "
                             "modifiedBesselFunction");
        if (x == 0.0)
            return std::numeric_limits<Real>::infinity();

        using namespace boost::math::policies;
        return boost::math::cyl_bessel_k(nu, x, make_policy(overflow_error<ignore_error>()));
    }

    std::complex<Real> modifiedBesselFunction_k(Real nu, const std::complex<Real>& z)
    {
        if (z.imag() == 0.0 && z.real() >= 0.0)
            return std::complex<Real>(modifiedBesselFunction_k(nu, z.real()));

        return modifiedBesselFunction_k_impl<std::complex<Real>, Unweighted>(nu, z);
    }

    Real modifiedBesselFunction_i_exponentiallyWeighted(Real nu, Real x)
    {
        QL_REQUIRE(x >= 0.0, "negative argument requires complex version of "
                             "modifiedBesselFunction");
        return modifiedBesselFunction_i_impl<Real, ExponentiallyWeighted>(nu, x);
    }

    std::complex<Real> modifiedBesselFunction_i_exponentiallyWeighted(Real nu, const std::complex<Real>& z)
    {

        if (z.imag() == 0.0 && z.real() >= 0.0)
            return std::complex<Real>(modifiedBesselFunction_i_exponentiallyWeighted(nu, z.real()));

        return modifiedBesselFunction_i_impl<std::complex<Real>, ExponentiallyWeighted>(nu, z);
    }

    Real modifiedBesselFunction_k_exponentiallyWeighted(Real nu, Real x)
    {
        return modifiedBesselFunction_k(nu, x) * std::exp(-x);
    }

    std::complex<Real> modifiedBesselFunction_k_exponentiallyWeighted(Real nu, const std::complex<Real>& z)
    {

        if (z.imag() == 0.0 && z.real() >= 0.0)
            return std::complex<Real>(modifiedBesselFunction_k_exponentiallyWeighted(nu, z.real()));

        return modifiedBesselFunction_k_impl<std::complex<Real>, ExponentiallyWeighted>(nu, z);
    }

}
