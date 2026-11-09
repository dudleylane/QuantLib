/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2002, 2003 Ferdinando Ametrano
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003 StatPro Italia srl
 Copyright (C) 2005 Gary Kennedy

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

#include <ql/math/distributions/bivariatenormaldistribution.hpp>
#include <ql/math/integrals/gaussianquadratures.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace QuantLib
{

    // Drezner 1978

    const Real BivariateCumulativeNormalDistributionDr78::x_[] = {0.24840615, 0.39233107, 0.21141819, 0.03324666,
                                                                  0.00082485334};

    const Real BivariateCumulativeNormalDistributionDr78::y_[] = {0.10024215, 0.48281397, 1.06094980, 1.77972940,
                                                                  2.66976040000};

    BivariateCumulativeNormalDistributionDr78::BivariateCumulativeNormalDistributionDr78(Real rho)
    : rho_(rho), rho2_(rho * rho)
    {

        QL_REQUIRE(rho >= -1.0, "rho must be >= -1.0 (" << rho << " not allowed)");
        QL_REQUIRE(rho <= 1.0, "rho must be <= 1.0 (" << rho << " not allowed)");
    }

    Real BivariateCumulativeNormalDistributionDr78::operator()(Real a, Real b) const
    {

        CumulativeNormalDistribution cumNormalDist;
        Real CumNormDistA = cumNormalDist(a);
        Real CumNormDistB = cumNormalDist(b);
        Real MaxCumNormDistAB = std::max(CumNormDistA, CumNormDistB);
        Real MinCumNormDistAB = std::min(CumNormDistA, CumNormDistB);

        // Y = X and Y = -X, where the formulas below would divide by sqrt(1 - rho^2).
        if (rho_ == 1.0)
            return MinCumNormDistAB;

        if (rho_ == -1.0)
            return std::max<Real>(0.0, CumNormDistA + CumNormDistB - 1.0);

        if (1.0 - MaxCumNormDistAB < 1e-15)
            return MinCumNormDistAB;

        if (MinCumNormDistAB < 1e-15)
            return MinCumNormDistAB;

        Real a1 = a / std::sqrt(2.0 * (1.0 - rho2_));
        Real b1 = b / std::sqrt(2.0 * (1.0 - rho2_));

        Real result = -1.0;
        const Real signA = a > 0.0 ? 1.0 : -1.0, signB = b > 0.0 ? 1.0 : -1.0;

        if (a <= 0.0 && b <= 0 && rho_ <= 0)
        {
            Real sum = 0.0;
            for (Size i = 0; i < 5; i++)
            {
                for (Size j = 0; j < 5; j++)
                {
                    sum += x_[i] * x_[j] *
                           std::exp(a1 * (2.0 * y_[i] - a1) + b1 * (2.0 * y_[j] - b1) +
                                    2.0 * rho_ * (y_[i] - a1) * (y_[j] - b1));
                }
            }
            result = std::sqrt(1.0 - rho2_) / M_PI * sum;
        }
        else if (a <= 0 && b >= 0 && rho_ >= 0)
        {
            BivariateCumulativeNormalDistributionDr78 bivCumNormalDist(-rho_);
            result = CumNormDistA - bivCumNormalDist(a, -b);
        }
        else if (a >= 0.0 && b <= 0.0 && rho_ >= 0.0)
        {
            BivariateCumulativeNormalDistributionDr78 bivCumNormalDist(-rho_);
            result = CumNormDistB - bivCumNormalDist(-a, b);
        }
        else if (a >= 0.0 && b >= 0.0 && rho_ <= 0.0)
        {
            result = CumNormDistA + CumNormDistB - 1.0 + (*this)(-a, -b);
        }
        // By their signs, since a * b * rho can underflow to zero.
        else if (signA * signB * rho_ > 0.0)
        {
            // D^2 = (rho a - b)^2 + a^2 (1 - rho^2), and likewise for b. These forms don't
            // cancel near |rho| = 1, and the results can't round past 1 either.
            const Real sqrtOneMinusRho2 = std::sqrt((1.0 - rho_) * (1.0 + rho_));
            Real rho1 = (rho_ * a - b) * signA / std::hypot(rho_ * a - b, a * sqrtOneMinusRho2);
            BivariateCumulativeNormalDistributionDr78 bivCumNormalDist(rho1);

            Real rho2 = (rho_ * b - a) * signB / std::hypot(rho_ * b - a, b * sqrtOneMinusRho2);
            BivariateCumulativeNormalDistributionDr78 CBND2(rho2);

            Real delta = (1.0 - signA * signB) / 4.0;

            result = bivCumNormalDist(a, 0.0) + CBND2(b, 0.0) - delta;
        }
        else
        {
            QL_FAIL("case not handled");
        }

        return result;
    }


    // West 2004

    namespace
    {

        class eqn3
        { /* Relates to eqn3 Genz 2004 */
          public:
            eqn3(Real h, Real k, Real asr) : hk_(h * k), asr_(asr), hs_((h * h + k * k) / 2) {}

            Real operator()(Real x) const
            {
                Real sn = std::sin(asr_ * (-x + 1) * 0.5);
                return std::exp((sn * hk_ - hs_) / (1.0 - sn * sn));
            }

          private:
            Real hk_, asr_, hs_;
        };

        class eqn6
        { /* Relates to eqn6 Genz 2004 */
          public:
            eqn6(Real a, Real c, Real d, Real bs, Real hk) : a_(a), c_(c), d_(d), bs_(bs), hk_(hk) {}
            Real operator()(Real x) const
            {
                Real xs = a_ * (-x + 1);
                xs = std::fabs(xs * xs);
                Real rs = std::sqrt(1 - xs);
                Real asr = -(bs_ / xs + hk_) / 2;
                if (asr > -100.0)
                {
                    return (a_ * std::exp(asr) *
                            (std::exp(-hk_ * (1 - rs) / (2 * (1 + rs))) / rs - (1 + c_ * xs * (1 + d_ * xs))));
                }
                else
                {
                    return 0.0;
                }
            }

          private:
            Real a_, c_, d_, bs_, hk_;
        };

    }

    BivariateCumulativeNormalDistributionWe04DP::BivariateCumulativeNormalDistributionWe04DP(Real rho)
    : correlation_(rho)
    {

        QL_REQUIRE(rho >= -1.0, "rho must be >= -1.0 (" << rho << " not allowed)");
        QL_REQUIRE(rho <= 1.0, "rho must be <= 1.0 (" << rho << " not allowed)");
    }


    Real BivariateCumulativeNormalDistributionWe04DP::operator()(Real x, Real y) const
    {
        // A normal tail past 40 holds less than 1e-349, under the smallest double, so the limits are exact there. The
        // formulas below would take infinity from infinity, or overflow, at infinite or very large arguments.
        if (x < -40.0 || y < -40.0)
            return 0.0;
        if (x > 40.0)
            return cumnorm_(y);
        if (y > 40.0)
            return cumnorm_(x);

        /* The implementation is described at section 2.4 "Hybrid
           Numerical Integration Algorithms" of "Numerical Computation
           of Rectangular Bivariate an Trivariate Normal and t
           Probabilities", Genz (2004), Statistics and Computing 14,
           151-160. (available at
           www.sci.wsu.edu/math/faculty/henz/homepage)

           The Gauss-Legendre quadrature have been extracted to
           TabulatedGaussLegendre (x,w zero-based)

           Tthe functions ot be integrated numerically have been moved
           to classes eqn3 and eqn6

           Change some magic numbers to M_PI */

        TabulatedGaussLegendre gaussLegendreQuad(20);
        if (std::fabs(correlation_) < 0.3)
        {
            gaussLegendreQuad.order(6);
        }
        else if (std::fabs(correlation_) < 0.75)
        {
            gaussLegendreQuad.order(12);
        }

        Real h = -x;
        Real k = -y;
        Real hk = h * k;
        Real BVN = 0.0;

        if (std::fabs(correlation_) < 0.925)
        {
            if (std::fabs(correlation_) > 0)
            {
                Real asr = std::asin(correlation_);
                eqn3 f(h, k, asr);
                BVN = gaussLegendreQuad(f);
                BVN *= asr * (0.25 / M_PI);
            }
            BVN += cumnorm_(-h) * cumnorm_(-k);
        }
        else
        {
            if (correlation_ < 0)
            {
                k *= -1;
                hk *= -1;
            }
            if (std::fabs(correlation_) < 1)
            {
                Real Ass = (1 - correlation_) * (1 + correlation_);
                Real a = std::sqrt(Ass);
                Real bs = (h - k) * (h - k);
                Real c = (4 - hk) / 8;
                Real d = (12 - hk) / 16;
                Real asr = -(bs / Ass + hk) / 2;
                if (asr > -100)
                {
                    BVN = a * std::exp(asr) * (1 - c * (bs - Ass) * (1 - d * bs / 5) / 3 + c * d * Ass * Ass / 5);
                }
                if (-hk < 100)
                {
                    Real B = std::sqrt(bs);
                    BVN -=
                        std::exp(-hk / 2) * 2.506628274631 * cumnorm_(-B / a) * B * (1 - c * bs * (1 - d * bs / 5) / 3);
                }
                a /= 2;
                eqn6 f(a, c, d, bs, hk);
                BVN += gaussLegendreQuad(f);
                BVN /= (-2.0 * M_PI);
            }

            if (correlation_ > 0)
            {
                BVN += cumnorm_(-std::max(h, k));
            }
            else
            {
                BVN *= -1;
                if (k > h)
                {
                    // evaluate cumnorm where it is most precise, that
                    // is in the lower tail because of double accuracy
                    // around 0.0 vs around 1.0
                    if (h >= 0)
                    {
                        BVN += cumnorm_(-h) - cumnorm_(-k);
                    }
                    else
                    {
                        BVN += cumnorm_(k) - cumnorm_(h);
                    }
                }
            }
        }
        return BVN;
    }

    namespace
    {

        // log(N(b) - N(a)) for a < b, from whichever tails keep the difference's digits.
        Real logNormalInterval(const CumulativeNormalDistribution& f, Real a, Real b)
        {
            if (a >= 0.0)
                return f.logValue(-a) + std::log1p(-std::exp(f.logValue(-b) - f.logValue(-a)));
            if (b <= 0.0)
                return f.logValue(b) + std::log1p(-std::exp(f.logValue(a) - f.logValue(b)));
            return std::log1p(-(f(a) + f(-b)));
        }

        // 1 - 1/z^2 + 3/z^4 - ..., so that N(z) = phi(z) / (-z) times it, for z below -37, where
        // CumulativeNormalDistribution::logValue takes N from the same series ((26.2.12) in Abramowitz and Stegun).
        Real normalTailSeries(Real z)
        {
            Real sum = 1.0, term = 1.0, zsqr = z * z;
            for (int i = 1; i < 30 && std::fabs(term) >= 1e-17 * sum; ++i)
            {
                term *= -(2.0 * i - 1.0) / zsqr;
                sum += term;
            }
            return sum;
        }

        // phi(z) / N(z). Below -37 from the series: the quotient of the exponentials would lose the digits of z^2 / 2.
        Real inverseMillsRatio(const CumulativeNormalDistribution& f, Real z)
        {
            if (z > -37.0)
                return std::exp(-0.5 * z * z - 0.5 * std::log(M_TWOPI) - f.logValue(z));
            return -z / normalTailSeries(z);
        }

        // log N(z + d) - log N(z), keeping its digits where both logarithms are large.
        Real logNormalStep(const CumulativeNormalDistribution& f, Real z, Real d)
        {
            Real z1 = z + d;
            if (z < -37.0 && z1 < -37.0)
                return -d * (z + 0.5 * d) - std::log1p(d / z) + std::log(normalTailSeries(z1) / normalTailSeries(z));
            return f.logValue(z1) - f.logValue(z);
        }

    }

    Real BivariateCumulativeNormalDistributionWe04DP::logValue(Real x, Real y) const
    {
        const Real inf = std::numeric_limits<Real>::infinity();
        const CumulativeNormalDistribution& f = cumnorm_;
        // M is at most N(x) and N(y), so where the logarithm of either is out of range, so is M's.
        const Real logNx = f.logValue(x), logNy = f.logValue(y);
        if (logNx == -inf || logNy == -inf)
            return -inf;
        if (x == inf)
            return logNy;
        if (y == inf)
            return logNx;
        if (correlation_ == 1.0)
            return std::min(logNx, logNy);
        if (correlation_ == -1.0)
            return x + y > 0.0 ? logNormalInterval(f, -y, x) : -inf;
        if (correlation_ == 0.0)
            return logNx + logNy;
        // M = N(y) - P(X > x, Y <= y), and the latter is at most N(-x): where that's below 1e-17 N(y), M is N(y).
        if (x > 0.0 && f.logValue(-x) < logNy - 39.0)
            return logNy;
        if (y > 0.0 && f.logValue(-y) < logNx - 39.0)
            return logNx;

        // M = int_{-inf}^{x} phi(u) N((y - rho u) / s) du, s = sqrt(1 - rho^2), with every term of the integrand
        // positive, so a quadrature of it keeps relative accuracy. Its log is concave, with curvature
        // 1 + k^2 kappa(z), k = rho / s and kappa = lambda (z + lambda) in (0, 1), monotonic in z: it has one maximum
        // on (-inf, x], and from there it falls off at least as fast as a Gaussian.
        const Real rho = correlation_, s = std::sqrt((1.0 - rho) * (1.0 + rho)), k = rho / s;
        // the log's slope at u, given z there, and its curvature through c
        auto slopeAt = [&](Real u, Real z, Real& c)
        {
            Real lambda = inverseMillsRatio(f, z);
            c = 1.0 + k * k * std::min(std::max(lambda * (z + lambda), Real(0.0)), Real(1.0));
            return -u - k * lambda;
        };
        auto slope = [&](Real u, Real& c) { return slopeAt(u, (y - rho * u) / s, c); };

        Real c, mode = x;
        if (Real g = slope(x, c); g < 0.0)
        {
            // The slope falls by between 1 and 1 / s^2 per unit, so the maximum is in [x + g, x + s^2 g]. Rounding can
            // move those ends past it; check them.
            Real lo = x + g, hi = x + s * s * g;
            if (!(hi < x) || slope(hi, c) > 0.0)
                hi = x;
            for (Real step = -2.0 * g; slope(lo, c) < 0.0; step *= 2.0)
                lo = x - step;
            // Newton's method, kept inside the bracket, until the step is negligible against the integrand's width
            Real u = hi;
            for (int i = 0; i < 200; ++i)
            {
                Real gu = slope(u, c);
                if (gu > 0.0)
                    lo = u;
                else
                    hi = u;
                Real next = u + gu / c;
                if (!(next > lo && next < hi))
                    next = lo + 0.5 * (hi - lo);
                bool done = std::fabs(next - u) * std::sqrt(c) < 1e-9 || !(hi - lo > 4e-16 * std::fabs(u));
                u = next;
                if (done)
                    break;
            }
            mode = u;
        }

        // From here on in offsets t from the maximum, so that the log's differences keep their digits however large
        // the arguments are.
        const Real z0 = (y - rho * mode) / s, logMax = -0.5 * mode * mode + f.logValue(z0);
        if (logMax == -inf)
            return -inf;
        auto logRatio = [&](Real t) { return -t * (mode + 0.5 * t) + logNormalStep(f, z0, -k * t); };
        auto slopeT = [&](Real t, Real& c) { return slopeAt(mode + t, z0 - k * t, c); };

        // From the maximum outwards, in panels over which the log falls by about delta, until the tail beyond, at most
        // the integrand over its log's slope since the log is concave, is negligible. Over a panel the curvature is
        // largest at one of its ends, so the log falls by at most |slope(a)| h + max(c(a), c(b)) h^2 / 2.
        const Real delta = 8.0;
        const TabulatedGaussLegendre gaussLegendre(20);
        Real sum = 0.0;
        auto march = [&](Real end, int dir)
        {
            Real a = 0.0, ca, ga = slopeT(0.0, ca);
            auto width = [&](Real cmax)
            {
                Real gs = std::fabs(ga);
                return 2.0 * delta / (gs + std::sqrt(gs * gs + 2.0 * cmax * delta));
            };
            auto fall = [&](Real h, Real cb) { return std::fabs(ga) * h + std::max(ca, cb) * h * h / 2; };
            for (int n = 0; n < 400; ++n)
            {
                Real h = width(ca);
                // Where N goes from 0 to 1, for |z| up to 10, the integrand changes on a scale of 1 in z, which the
                // curvature at a panel's ends can miss when k is large. So a panel in that region spans at most 4 in
                // z, and one heading into it stops at its edge, or spans 4 if that is further.
                Real za = z0 - k * a, outside = std::max(std::fabs(za) - 10.0, Real(0.0));
                if (outside == 0.0 || (za > 0.0) != (k * dir < 0.0))
                    h = std::min(h, std::max(outside, Real(4.0)) / std::fabs(k));
                bool last = dir > 0 && a + h >= end;
                if (last)
                    h = end - a;
                Real b = a + dir * h, cb, gb = slopeT(b, cb);
                if (fall(h, cb) > delta * (1.0 + 1e-9))
                {
                    // Too steep at the far end. The width for the curvature there passes; find the widest that does.
                    last = false;
                    Real passes = width(std::max(ca, cb)), fails = h;
                    b = a + dir * passes;
                    gb = slopeT(b, cb);
                    while (fails > 1.25 * passes)
                    {
                        Real mid = std::sqrt(passes * fails), cm, gm = slopeT(a + dir * mid, cm);
                        if (fall(mid, cm) <= delta)
                        {
                            passes = mid;
                            b = a + dir * mid;
                            gb = gm;
                            cb = cm;
                        }
                        else
                        {
                            fails = mid;
                        }
                    }
                }
                Real centre = 0.5 * (a + b), half = 0.5 * (b - a);
                sum += std::fabs(half) * gaussLegendre([&](Real t) { return std::exp(logRatio(centre + half * t)); });
                if (last || (dir * gb < 0.0 && std::exp(logRatio(b)) < 1e-17 * sum * std::fabs(gb)))
                    break;
                a = b;
                ga = gb;
                ca = cb;
            }
        };
        if (mode < x)
            march(x - mode, 1);
        march(-inf, -1);
        return logMax + std::log(sum) - 0.5 * std::log(M_TWOPI);
    }

}
