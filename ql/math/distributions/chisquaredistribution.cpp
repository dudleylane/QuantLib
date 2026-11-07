/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2007 Klaus Spanderen

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

#include <ql/math/distributions/chisquaredistribution.hpp>
#include <ql/math/distributions/gammadistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/functional.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <boost/math/special_functions/log1p.hpp>

namespace QuantLib
{

    Real CumulativeChiSquareDistribution::operator()(Real x) const
    {
        return CumulativeGammaDistribution(0.5 * df_)(0.5 * x);
    }

    namespace
    {
        // log(y^a exp(-y) / Gamma(a + 1)).  For large a the terms of the plain expression are
        // huge and nearly cancel, and logValue is only good to about 1e-10, so this goes through
        // Stirling's series and log1p(r) - r, in which nothing large is subtracted (Loader, 2000).
        Real logPoissonWeight(Real a, Real y)
        {
            if (a < 10.0)
                return a * std::log(y) - y - GammaFunction().logValue(a + 1.0);

            const Real r = (y - a) / a;
            const Real s = 1.0 / (a * a);
            const Real stirling =
                (1.0 / 12 -
                 s * (1.0 / 360 - s * (1.0 / 1260 - s * (1.0 / 1680 - s * (1.0 / 1188 - s * 691.0 / 360360))))) /
                a;
            return (r > -0.9 ? a * boost::math::log1pmx(r) : a * (std::log(y / a) - r)) -
                   0.5 * std::log(2.0 * M_PI * a) - stirling;
        }
    }

    Real NonCentralCumulativeChiSquareDistribution::operator()(Real x) const
    {
        if (x <= 0.0)
            return 0.0;

        const Real errmax = 1e-12;
        const Size itrmax = 1000000;
        const Real minLog = std::log(QL_MIN_POSITIVE_REAL);
        const Real lam = 0.5 * ncp_;
        const Real x2 = 0.5 * x;
        const Real f2 = 0.5 * df_;

        // The series sums v(n) t(n), where v(n) is the probability of at most n for a Poisson
        // variable of mean lam and t(n) = x2^(f2 + n) exp(-x2) / Gamma(f2 + n + 1).  Far from
        // their means the first of these underflow and the sum would stay at zero, so it starts
        // further on.  A Poisson variable is d below its mean with probability at most
        // exp(-d^2 / (2 mean)), so what comes before 10 standard deviations is below exp(-50).
        Real logT = logPoissonWeight(f2, x2);
        if (logT < minLog && x > df_ + ncp_)
        {
            // Chernoff's bound on the upper tail, which saves walking up to terms that can be
            // far away only to find that they sum to 1.
            const Real w = (df_ + std::sqrt(df_ * df_ + 4.0 * x * ncp_)) / (2.0 * x);
            const Real logBound = -0.5 * (1.0 - w) * x - 0.5 * df_ * std::log(w) + 0.5 * ncp_ * (1.0 - w) / w;
            if (logBound < std::log(errmax))
                return 1.0;
        }

        Size n = 0;
        Real u = std::exp(-lam);
        if (-lam < minLog)
        {
            QL_REQUIRE(lam < QL_MAX_INTEGER, "non-centrality parameter too large");
            n = Size(lam - 10.0 * std::sqrt(lam));
            u = std::exp(logPoissonWeight(Real(n), lam));
            logT = logPoissonWeight(f2 + n, x2);
        }
        Real v = u;

        if (logT < minLog && x2 - f2 > n)
        {
            const Real first = x2 - f2 - 10.0 * std::sqrt(x2);
            QL_REQUIRE(first - n <= itrmax, "didn't converge");
            for (const Size last = Size(first); n < last;)
            {
                ++n;
                u *= lam / n;
                v += u;
            }
            logT = logPoissonWeight(f2 + n, x2);
        }

        Real t = std::exp(logT);
        Real ans = v * t;

        ++n;
        const Size nMax = n + itrmax;
        Real f_2n = df_ + 2.0 * n;
        Real f_x_2n = df_ - x + 2.0 * n;

        for (;;)
        {
            // Beyond the peak of t(n), what's left of the sum is below a geometric series.
            if (f_x_2n > 0.0 && t * x / f_x_2n <= errmax)
                return ans;
            QL_REQUIRE(n <= nMax, "didn't converge");

            u *= lam / n;
            v += u;
            t *= x / f_2n;
            ans += v * t;
            n++;
            f_2n += 2.0;
            f_x_2n += 2.0;
        }
    }

    Real NonCentralCumulativeChiSquareSankaranApprox::operator()(Real x) const
    {

        const Real h = 1 - 2 * (df_ + ncp_) * (df_ + 3 * ncp_) / (3 * squared(df_ + 2 * ncp_));
        const Real p = (df_ + 2 * ncp_) / squared(df_ + ncp_);
        const Real m = (h - 1) * (1 - 3 * h);

        const Real u = (std::pow(x / (df_ + ncp_), h) - (1 + h * p * (h - 1 - 0.5 * (2 - h) * m * p))) /
                       (h * std::sqrt(2 * p) * (1 + 0.5 * m * p));

        return CumulativeNormalDistribution()(u);
    }

    InverseNonCentralCumulativeChiSquareDistribution::InverseNonCentralCumulativeChiSquareDistribution(
        Real df, Real ncp, Size maxEvaluations, Real accuracy)
    : nonCentralDist_(df, ncp), guess_(df + ncp), maxEvaluations_(maxEvaluations), accuracy_(accuracy)
    {
    }

    Real InverseNonCentralCumulativeChiSquareDistribution::operator()(Real x) const
    {

        // first find the right side of the interval
        Real upper = guess_;
        Size evaluations = maxEvaluations_;
        while (nonCentralDist_(upper) < x && evaluations > 0)
        {
            upper *= 2.0;
            --evaluations;
        }

        // use a Brent solver for the rest
        Brent solver;
        solver.setMaxEvaluations(evaluations);
        return solver.solve([&](Real y) { return nonCentralDist_(y) - x; }, accuracy_, 0.75 * upper,
                            (evaluations == maxEvaluations_) ? 0.0 : Real(0.5 * upper), upper);
    }
}
