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

#include <ql/errors.hpp>
#include <ql/math/modifiedbessel.hpp>
#include <ql/mathconstants.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/cos_pi.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/special_functions/sin_pi.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace QuantLib
{

    namespace
    {

        typedef std::complex<Real> Complex;

        // sin(pi x) and cos(pi x), exactly 0 and +-1 at multiples of 1/2, without the promotion to long double that
        // Boost's default policy makes.  That would cost four times as much and move no result by more than an ulp.
        typedef boost::math::policies::policy<boost::math::policies::promote_double<false>> BesselTrigPolicy;

        Real besselSinPi(Real x)
        {
            return boost::math::sin_pi(x, BesselTrigPolicy());
        }

        Real besselCosPi(Real x)
        {
            return boost::math::cos_pi(x, BesselTrigPolicy());
        }

        // max(|Re|, |Im|), within a factor sqrt(2) of the modulus, which is all a stopping rule needs.
        Real besselSize(Real x)
        {
            return std::fabs(x);
        }

        Real besselSize(const Complex& z)
        {
            return std::max(std::fabs(z.real()), std::fabs(z.imag()));
        }

        // e^(i nu arg z) for Re z >= 0.  Nearer the imaginary axis than the real one, arg z is +-pi/2 - atan(Re z /
        // Im z), and the quarter turn is taken exactly.
        Complex besselPhase(Real nu, const Complex& z)
        {
            const Real x = z.real(), y = z.imag();
            if (std::fabs(y) <= x)
            {
                const Real t = nu * std::atan(y / x);
                return Complex(std::cos(t), std::sin(t));
            }
            const Real quarter = std::signbit(y) ? -0.5 * nu : 0.5 * nu, t = -nu * std::atan(x / y);
            return Complex(besselCosPi(quarter), besselSinPi(quarter)) * Complex(std::cos(t), std::sin(t));
        }

        // (z / 2)^nu / Gamma(1 + nu)
        Real besselSeriesPrefix(Real nu, Real x)
        {
            return std::pow(0.5 * x, nu) / std::tgamma(1.0 + nu);
        }

        Complex besselSeriesPrefix(Real nu, const Complex& z)
        {
            return std::pow(0.5 * std::abs(z), nu) / std::tgamma(1.0 + nu) * besselPhase(nu, z);
        }

        // I's power series.  It converges everywhere, but without cancellation only while |z|^2 / 4 is small
        // against the order, or out to sqrt(reach2) when the caller knows it has none.  Gamma(1 + nu) overflows
        // from nu = 170.6.
        template <class T>
        bool besselSeries(Real nu, const T& z, T& result, Real reach2 = 0.0)
        {
            if (std::fabs(nu) >= 169.0)
                return false;
            const Real r2 = std::norm(z);
            if (r2 > 4.0 * (std::fabs(nu) + 1.0) && r2 > reach2)
                return false;

            const T alpha = besselSeriesPrefix(nu, z);
            const T Y = 0.25 * z * z;
            Size k = 1;
            T sum = alpha, B_k = alpha;

            while (besselSize(B_k *= Y / (k * (k + nu))) > besselSize(sum) * QL_EPSILON)
            {
                sum += B_k;
                QL_REQUIRE(++k < 1000, "max iterations exceeded");
            }
            result = sum;
            return true;
        }

        // The sum of the large-argument expansions, sum_k sign^k a_k(nu) / z^k (DLMF 10.40.2 and 10.40.5), where
        // it converges cleanly: a term falls below QL_EPSILON of the sum before the terms, once falling, grow
        // again, and none is larger than four times the sum, which bounds the cancellation.  The terms may rise
        // at first, as they do while nu^2 is large against |z|.
        template <class T>
        bool besselExpansion(Real nu, const T& z, Real sign, T& result)
        {
            const Real mu = 4.0 * nu * nu;
            const T zinv = 1.0 / z;
            T term = T(1.0), sum = T(1.0);
            Real largest = 1.0, previous = 1.0;
            bool falling = false;
            for (Size k = 1; k < 200; ++k)
            {
                const Real odd = 2.0 * k - 1.0;
                term *= (sign * (mu - odd * odd) / (8.0 * k)) * zinv;
                const Real size = std::norm(term);
                if (size > previous)
                {
                    if (falling || size > 1.6e11)
                        return false;
                }
                else
                    falling = true;
                previous = size;
                sum += term;
                largest = std::max(largest, size);
                const Real total = std::norm(sum);
                if (size <= QL_EPSILON * QL_EPSILON * total)
                {
                    if (largest > 16.0 * total)
                        return false;
                    result = sum;
                    return true;
                }
            }
            return false;
        }

        // e^z K_u(z) and e^z K_u+1(z) for |u| <= 1/2, Re z >= 0, z != 0, as in Thompson and Barnett's algorithm
        // (J. Comput. Phys. 64, 490, 1986) and Boost's bessel_ik: Temme's series up to |z| = 2, Steed's continued
        // fraction above.
        void besselScaledKAtFraction(Real u, const Complex& z, Complex& Ku, Complex& Ku1)
        {
            if (std::abs(z) <= 2.0)
            {
                const Real gp = boost::math::tgamma1pm1(u), gm = boost::math::tgamma1pm1(-u);
                const Complex a = std::log(z / 2.0);
                const Complex b = std::exp(u * a);
                const Complex sigma = -a * u;
                const Real c = std::fabs(u) < QL_EPSILON ? 1.0 : besselSinPi(u) / (u * M_PI);
                const Complex d = std::abs(sigma) < QL_EPSILON ? Complex(1.0) : Complex(std::sinh(sigma) / sigma);
                const Real gamma1 = std::fabs(u) < QL_EPSILON ? Real(-boost::math::constants::euler<Real>()) :
                                                                Real(0.5 / u * (gp - gm) * c);
                const Real gamma2 = (2.0 + gp + gm) * c / 2.0;
                Complex p = (gp + 1.0) / (2.0 * b), q = (1.0 + gm) * b / 2.0;
                Complex f = (std::cosh(sigma) * gamma1 + d * (-a) * gamma2) / c;
                Complex h = p, coef = Complex(1.0), sum = f, sum1 = h;
                for (int k = 1;; ++k)
                {
                    QL_REQUIRE(k < 1000, "max iterations exceeded");
                    const Real rk = k;
                    f = (rk * f + p + q) / (rk * rk - u * u);
                    p /= rk - u;
                    q /= rk + u;
                    h = p - rk * f;
                    coef *= z * z / (4.0 * rk);
                    sum += coef * f;
                    sum1 += coef * h;
                    if (besselSize(coef * f) < besselSize(sum) * QL_EPSILON)
                        break;
                }
                const Complex e = std::exp(z);
                Ku = sum * e;
                Ku1 = 2.0 * sum1 / z * e;
            }
            else
            {
                Real a = u * u - 0.25;
                Complex b = 2.0 * (z + 1.0), D = 1.0 / b, f = D, delta = D, prev = Complex(0.0), current = Complex(1.0),
                        C = Complex(-a), Q = C, S = 1.0 + Q * delta;
                for (int k = 2;; ++k)
                {
                    QL_REQUIRE(k < 100000, "max iterations exceeded");
                    a -= 2.0 * (k - 1);
                    b += 2.0;
                    D = 1.0 / (b + a * D);
                    delta *= b * D - 1.0;
                    f += delta;
                    const Complex q = (prev - (b - 2.0) * current) / a;
                    prev = current;
                    current = q;
                    C *= -a / k;
                    Q += C * q;
                    S += Q * delta;
                    // q falls as C grows; rescale both before either leaves the range of a Real
                    if (besselSize(q) < QL_EPSILON)
                    {
                        C *= q;
                        prev /= q;
                        current /= q;
                    }
                    if (besselSize(Q * delta) < besselSize(S) * QL_EPSILON)
                        break;
                }
                Ku = std::sqrt(M_PI / (2.0 * z)) / S;
                Ku1 = Ku * (0.5 + u + z + (u * u - 0.25) * f) / z;
            }
        }

        // e^z K_nu(z) and e^z K_nu+1(z) for nu >= 0, as (Kv, Kv1) / scale: K at nu's distance from the nearest
        // integer, then K's recurrence upwards, which is stable, rescaled before it can overflow.
        void besselScaledKUpwards(Real nu, const Complex& z, Complex& Kv, Complex& Kv1, Real& scale)
        {
            const Real n = std::floor(nu + 0.5), u = nu - n;
            besselScaledKAtFraction(u, z, Kv, Kv1);
            scale = 1.0;
            const Real modulus = std::abs(z);
            for (Real k = 1.0; k <= n; k += 1.0)
            {
                const Real factor = 2.0 * (u + k) / modulus;
                const Real size = std::abs(Kv1);
                if (size > 1e300 / std::max(factor, 1.0))
                {
                    Kv /= size;
                    Kv1 /= size;
                    scale /= size;
                }
                const Complex next = 2.0 * (u + k) / z * Kv1 + Kv;
                Kv = Kv1;
                Kv1 = next;
            }
        }

        // I_nu+1(z) / I_nu(z) from its continued fraction, by the modified Lentz method, as Boost's CF1_ik.  It
        // takes about |z| terms where |z| exceeds the order.
        Complex besselRatioOfI(Real nu, const Complex& z)
        {
            const Real tiny = std::sqrt(QL_MIN_POSITIVE_REAL);
            const Complex zinv = 1.0 / z;
            Complex C = tiny, D = 0.0, f = tiny;
            for (int k = 1;; ++k)
            {
                QL_REQUIRE(k < 1000000, "max iterations exceeded");
                const Complex b = (2.0 * (nu + k)) * zinv;
                C = b + 1.0 / C;
                D = b + D;
                if (C == Complex(0.0))
                    C = tiny;
                if (D == Complex(0.0))
                    D = tiny;
                D = 1.0 / D;
                const Complex delta = C * D;
                f *= delta;
                if (besselSize(delta - 1.0) <= QL_EPSILON)
                    break;
            }
            return f;
        }

        // e^-z I_nu(z) and e^z K_nu(z) for any real nu, Re z >= 0, z != 0: I from the Wronskian
        // I_nu K_nu+1 + I_nu+1 K_nu = 1 / z, and I_-nu = I_nu + 2 / pi sin(nu pi) K_nu.
        void besselScaledIAndK(Real nu, const Complex& z, Complex& I, Complex& K)
        {
            const Real v = std::fabs(nu);
            Complex Kv, Kv1;
            Real scale;
            besselScaledKUpwards(v, z, Kv, Kv1, scale);
            const Complex f = besselRatioOfI(v, z);
            I = scale / (z * (Kv * f + Kv1));
            K = Kv / scale;
            if (nu < 0.0)
                I += 2.0 / M_PI * besselSinPi(v) * K * std::exp(-2.0 * z);
        }

        // e^z K_nu(z) alone, as above
        Complex besselScaledK(Real nu, const Complex& z)
        {
            Complex Kv, Kv1;
            Real scale;
            besselScaledKUpwards(std::fabs(nu), z, Kv, Kv1, scale);
            return Kv / scale;
        }

        // e^(i pi nu) above the real axis and on it from above (Im z = +0), e^(-i pi nu) below it, as std::arg
        // takes its branches.
        Complex besselReflection(Real nu, const Complex& z)
        {
            const Real side = std::signbit(z.imag()) ? -1.0 : 1.0;
            return Complex(besselCosPi(nu), side * besselSinPi(nu));
        }

        // e exp(w), without overflowing where the product doesn't
        Complex besselTimesExp(const Complex& e, const Complex& w)
        {
            if (w.real() < 700.0)
                return e * std::exp(w);
            const Complex h = std::exp(0.5 * w);
            return (e * h) * h;
        }

        // e^-z I_nu(z) for Re z >= 0, z != 0, past the power series: DLMF 10.40.5, whose second exponential takes
        // i e^(i nu pi) above the real axis and -i e^(-i nu pi) below it, where both sums converge cleanly, else
        // Thompson and Barnett's algorithm.
        Complex besselScaledI(Real nu, const Complex& z)
        {
            Complex s1, s2;
            if (besselExpansion(nu, z, -1.0, s1) && besselExpansion(nu, z, 1.0, s2))
            {
                const Real side = std::signbit(z.imag()) ? -1.0 : 1.0;
                const Complex phase = side * Complex(0.0, 1.0) * besselReflection(nu, z);
                return (s1 + phase * std::exp(-2.0 * z) * s2) / std::sqrt(2.0 * M_PI * z);
            }
            Complex I, K;
            besselScaledIAndK(nu, z, I, K);
            return I;
        }

        template <bool weighted>
        Real modifiedBesselFunction_i_impl(Real nu, Real x)
        {
            // The series divides by zero at a negative integer order, where I_-n = I_n.
            if (nu < 0.0 && nu == std::floor(nu))
                nu = -nu;

            Real s;
            if (besselSeries(nu, x, s))
                return weighted ? Real(s * std::exp(-x)) : s;

            // On the real axis the expansion leaves out a term of relative size e^-2x, which is below half of
            // QL_EPSILON from x = 18.5.  At a half-integer order the sum is finite and converges at any x, so
            // the sum can't be what decides it.
            if (x >= 18.5 && besselExpansion(nu, x, -1.0, s))
            {
                s /= std::sqrt(2.0 * M_PI * x);
                if (weighted)
                    return s;
                if (x < 700.0)
                    return s * std::exp(x);
                const Real h = std::exp(0.5 * x);
                return (s * h) * h;
            }

            // Above order -1 the series has no cancellation on the real axis, and up to x = 20 it costs less than
            // Boost.
            if (nu > -1.0 && besselSeries(nu, x, s, 400.0))
                return weighted ? Real(s * std::exp(-x)) : s;

            if (!weighted || x <= 700.0)
            {
                using namespace boost::math::policies;
                const Real i = boost::math::cyl_bessel_i(nu, x, make_policy(overflow_error<ignore_error>()));
                return weighted ? Real(i * std::exp(-x)) : i;
            }

            // where exp(x) would overflow
            Complex I, K;
            besselScaledIAndK(nu, Complex(x, 0.0), I, K);
            return I.real();
        }

        template <bool weighted>
        Complex modifiedBesselFunction_i_impl(Real nu, const Complex& z)
        {
            if (nu < 0.0 && nu == std::floor(nu))
                nu = -nu;

            // I_nu(z) = e^(+-i pi nu) I_nu(-z) on the left half-plane (DLMF 10.34.1)
            const bool left = z.real() < 0.0;
            const Complex w = left ? Complex(-z) : z;
            Complex s;
            if (besselSeries(nu, w, s))
            {
                if (!left)
                    return weighted ? Complex(s * std::exp(-w)) : s;
                const Complex r = besselReflection(nu, z) * s;
                return weighted ? besselTimesExp(r, w) : r;
            }

            const Complex e = besselScaledI(nu, w);
            if (!left)
                return weighted ? e : besselTimesExp(e, w);
            const Complex r = besselReflection(nu, z) * e;
            return weighted ? besselTimesExp(besselTimesExp(r, w), w) : besselTimesExp(r, w);
        }

        template <bool weighted>
        Complex modifiedBesselFunction_k_impl(Real nu, const Complex& z)
        {
            nu = std::fabs(nu);

            const bool left = z.real() < 0.0;
            const Complex w = left ? Complex(-z) : z;
            Complex K, I, s;
            const bool expansion = besselExpansion(nu, w, 1.0, s);
            if (expansion)
                K = std::sqrt(M_PI / (2.0 * w)) * s;
            if (!left)
            {
                if (!expansion)
                    K = besselScaledK(nu, w);
                const Complex e = std::exp(-z);
                return weighted ? Complex((K * e) * e) : Complex(K * e);
            }

            Complex s1;
            if (expansion && besselExpansion(nu, w, -1.0, s1))
            {
                const Real sideW = std::signbit(w.imag()) ? -1.0 : 1.0;
                I = (s1 + sideW * Complex(0.0, 1.0) * besselReflection(nu, w) * std::exp(-2.0 * w) * s) /
                    std::sqrt(2.0 * M_PI * w);
            }
            else
            {
                besselScaledIAndK(nu, w, I, K);
            }

            // K_nu(z) = e^(-+i pi nu) K_nu(w) -+ i pi I_nu(w) with w = -z (DLMF 10.34.2, m = +-1)
            const Real side = std::signbit(z.imag()) ? -1.0 : 1.0;
            const Complex k = std::conj(besselReflection(nu, z)) * K * std::exp(-w) -
                              besselTimesExp(side * Complex(0.0, M_PI) * I, w);
            return weighted ? besselTimesExp(k, w) : k;
        }
    }

    Real modifiedBesselFunction_i(Real nu, Real x)
    {
        QL_REQUIRE(x >= 0.0, "negative argument requires complex version of "
                             "modifiedBesselFunction");
        return modifiedBesselFunction_i_impl<false>(nu, x);
    }

    std::complex<Real> modifiedBesselFunction_i(Real nu, const std::complex<Real>& z)
    {
        if (z.imag() == 0.0 && z.real() >= 0.0)
            return std::complex<Real>(modifiedBesselFunction_i(nu, z.real()));

        return modifiedBesselFunction_i_impl<false>(nu, z);
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

        return modifiedBesselFunction_k_impl<false>(nu, z);
    }

    Real modifiedBesselFunction_i_exponentiallyWeighted(Real nu, Real x)
    {
        QL_REQUIRE(x >= 0.0, "negative argument requires complex version of "
                             "modifiedBesselFunction");
        return modifiedBesselFunction_i_impl<true>(nu, x);
    }

    std::complex<Real> modifiedBesselFunction_i_exponentiallyWeighted(Real nu, const std::complex<Real>& z)
    {

        if (z.imag() == 0.0 && z.real() >= 0.0)
            return std::complex<Real>(modifiedBesselFunction_i_exponentiallyWeighted(nu, z.real()));

        return modifiedBesselFunction_i_impl<true>(nu, z);
    }

    Real modifiedBesselFunction_k_exponentiallyWeighted(Real nu, Real x)
    {
        return modifiedBesselFunction_k(nu, x) * std::exp(-x);
    }

    std::complex<Real> modifiedBesselFunction_k_exponentiallyWeighted(Real nu, const std::complex<Real>& z)
    {

        if (z.imag() == 0.0 && z.real() >= 0.0)
            return std::complex<Real>(modifiedBesselFunction_k_exponentiallyWeighted(nu, z.real()));

        return modifiedBesselFunction_k_impl<true>(nu, z);
    }

}
