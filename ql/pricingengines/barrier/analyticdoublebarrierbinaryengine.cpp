/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Thema Consulting SA

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

#include <ql/exercise.hpp>
#include <ql/pricingengines/americanpayoffathit.hpp>
#include <ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.hpp>
#include <cmath>
#include <utility>

namespace QuantLib
{

    namespace
    {

        /* The log of the underlying over L starts at x = ln(S / L), inside (0, Z) with Z = ln(U / L), and moves by
           mu t + vol W_t, with mu = r - q - vol^2 / 2.  V = vol^2 T is the variance to expiry.  Hui's series are
           Fourier series, whose i-th terms fall like exp(-(i pi)^2 V / (2 Z^2)).  At low volatility they need ever
           more terms, and their terms grow, up to exp(Z^2 / (2V)), while the value doesn't.  There the values are
           sums over images instead, the reflections of x in the barriers at x + 2nZ and 2nZ - x, whose n-th terms
           are at most exp(-2 (|n| - 1)^2 Z^2 / V) of the leading ones.  The no-touch probability's images are of
           order 1, though, and cancel as it falls, losing about two digits by sqrt(V) = Z.  So it takes images up to
           sqrt(V) = Z / 2, where neither form loses one, and the value paid at a hit, whose images don't cancel,
           takes them up to sqrt(V) = Z. */

        // N(v), from erfc, which keeps relative accuracy below the median
        Real binaryCumulative(Real v)
        {
            return 0.5 * std::erfc(-v * M_SQRT1_2);
        }

        // R(v) = N(v) / n(v), the Mills ratio, for v <= 0
        Real binaryMillsRatio(Real v)
        {
            if (v >= -10.0)
                return M_SQRT2 * M_SQRTPI * binaryCumulative(v) * std::exp(0.5 * v * v);
            // the asymptotic series, which reaches machine precision before it diverges here
            const Real v2 = v * v;
            Real term = 1.0, sum = 1.0;
            for (Size i = 1; std::fabs(term) >= QL_EPSILON * std::fabs(sum); ++i)
            {
                term *= -(2.0 * i - 1.0) / v2;
                sum += term;
            }
            return -sum / v;
        }

        // w N(v) for v <= 0, given e = ln(w) - v^2 / 2 instead of the weight w, which overflows at low volatility
        // where w N(v) doesn't.  The callers form e so that it neither overflows nor cancels.
        Real binaryWeightedCumulative(Real v, Real e)
        {
            return binaryMillsRatio(v) * std::exp(e) * M_SQRT1_2 * M_1_SQRTPI;
        }

        // Hui's series' terms after the J-th are at most j^2 exp(-(j^2 - 1) pi^2 V / (2 Z^2)) of the first, where
        // j = J + 1.  With this J that is below 1e-18; J is 6 at sqrt(V) = Z / 2, 3 at Z, and fewer above.
        int binaryFourierCount(Real stdDevOverZ)
        {
            const Real c = 0.5 * M_PI * M_PI * stdDevOverZ * stdDevOverZ;
            int J = 1;
            while ((J + 1.0) * (J + 1.0) * std::exp(-((J + 1.0) * (J + 1.0) - 1.0) * c) >= 1.0e-18)
                ++J;
            return J;
        }

        /* The probability that the log stays inside (0, Z) up to T, by images.  The image at c adds
           exp(k (c - x)) (N(a) - N(b)), with k = mu / vol^2, a = (Z - c - mu T) / sqrt(V) and
           b = (-c - mu T) / sqrt(V), counted positively at c = x + 2nZ and negatively at c = 2nZ - x.  Each
           weighted N(v) is exp(e) R(v) / sqrt(2 pi), with e = -[(c - x)(c + x - 2y) + (x + mu T - y)^2] / (2V) at
           y = Z for a and y = 0 for b.  For every image, (c - x)(c + x - 2y) >= 0 for y in [0, Z], so e <= 0 and
           nothing in it cancels.  Where a > 0 > b, the weight is exp(e) at y = c + mu T, which is inside, so it is
           at most 1. */
        Real binaryNoTouchByImages(Real x, Real Z, Real mu, Real T, Real V)
        {
            const Real stdDev = std::sqrt(V), muT = mu * T;
            // Images n = -N...N leave out at most exp(-2 N^2 Z^2 / V) of the leading terms, below 1e-18 with this N.
            const int N = std::max(1, int(std::ceil(4.6 * stdDev / Z)));
            Real sum = 0.0;
            for (int n = -N; n <= N; ++n)
            {
                for (int reflected = 0; reflected < 2; ++reflected)
                {
                    // c, c - x, c + x and c + x - 2Z, each without the rounding of the others
                    const Real c = reflected != 0 ? 2.0 * n * Z - x : x + 2.0 * n * Z;
                    const Real cMinusX = reflected != 0 ? 2.0 * (n * Z - x) : 2.0 * n * Z;
                    const Real cPlusX = reflected != 0 ? 2.0 * n * Z : 2.0 * (x + n * Z);
                    const Real cPlusXLess2Z = reflected != 0 ? 2.0 * (n - 1) * Z : 2.0 * (x + (n - 1) * Z);
                    const Real a = (Z - c - muT) / stdDev, b = (-c - muT) / stdDev;
                    const Real eZ = -(cMinusX * cPlusXLess2Z + (x + muT - Z) * (x + muT - Z)) / (2.0 * V);
                    const Real e0 = -(cMinusX * cPlusX + (x + muT) * (x + muT)) / (2.0 * V);
                    Real term;
                    if (a <= 0.0)
                        term = binaryWeightedCumulative(a, eZ) - binaryWeightedCumulative(b, e0);
                    else if (b >= 0.0)
                        // N(a) - N(b) = N(-b) - N(-a)
                        term = binaryWeightedCumulative(-b, e0) - binaryWeightedCumulative(-a, eZ);
                    else
                        term = std::exp(cMinusX * muT / V) * (binaryCumulative(a) - binaryCumulative(b));
                    sum += reflected != 0 ? -term : term;
                }
            }
            return sum;
        }

        /* The same probability by Hui's series: 2 pi / Z^2 times the sum over i of
           i (A - (-1)^i B) / (alpha^2 + w^2) sin(w x) exp(-w^2 V / 2), with w = i pi / Z and alpha = -mu / vol^2.
           A = (S / L)^alpha exp(-mu^2 T / (2 vol^2)) and B = (S / U)^alpha exp(-mu^2 T / (2 vol^2)) are formed so that
           neither overflows; both are at most exp(Z^2 / (2V)). */
        Real binaryNoTouchByFourier(Real x, Real Z, Real mu, Real T, Real V)
        {
            const Real alpha = -mu * T / V, muT = mu * T;
            const Real A = std::exp(-muT * (2.0 * x + muT) / (2.0 * V));
            const Real B = std::exp(-muT * (2.0 * (x - Z) + muT) / (2.0 * V));
            const int J = binaryFourierCount(std::sqrt(V) / Z);
            Real sum = 0.0;
            for (int i = 1; i <= J; ++i)
            {
                const Real w = i * M_PI / Z;
                sum += i * (i % 2 == 0 ? A - B : A + B) / (alpha * alpha + w * w) * std::sin(w * x) *
                       std::exp(-0.5 * w * w * V);
            }
            return 2.0 * M_PI / (Z * Z) * sum;
        }

        /* What 1 paid when the log first reaches 0, if it does so by T and before it reaches Z, is worth, by images
           of the exit time's density.  That's the sum over n of sign(a_n) exp(-mu x / vol^2) L(|a_n|), with
           a_n = x + 2nZ, where L(a) = exp(-a theta) N(c1) + exp(a theta) N(c2) is the transform of the time a
           driftless Brownian motion takes to travel a, up to T: theta = rho / vol^2 with rho^2 = mu^2 + 2 r vol^2,
           c1 = (rho T - a) / sqrt(V) and c2 = -(rho T + a) / sqrt(V).  Both weights overflow at low volatility, but
           with them each N(c) is exp(e) R(c) / sqrt(2 pi), with the same e = -rT - [(x + mu T)^2 + (a - x)(a + x)] /
           (2V), and a >= x.  Where c1 > 0, N(c1) has no tail to offset its weight, which is then
           exp(x e_- - (a - x) theta), with e_- = -(mu + rho) / vol^2 formed as -2r / (rho - mu) where mu < 0, since
           mu + rho cancels there.  Where rho^2 < 0, which takes a negative rate, theta is imaginary, and
           detail::unitPayoffAtHit gives each term, times exp(-mu (a - x) / vol^2). */
        Real binaryLowerHitByImages(Real x, Real Z, Real mu, Real r, Real T, Real V)
        {
            const Real stdDev = std::sqrt(V), volSquared = V / T;
            const Real rhoSquared = mu * mu + 2.0 * r * volSquared;
            // Images n = -N...N leave out at most exp(-2 N (N + 1) Z^2 / V) of the leading term.  With this N, that
            // is below 1e-18.
            const Real stdDevOverZ = stdDev / Z;
            const int N = std::max(1, int(std::ceil(0.5 * (std::sqrt(1.0 + 84.0 * stdDevOverZ * stdDevOverZ) - 1.0))));
            Real sum = 0.0;
            for (int n = -N; n <= N; ++n)
            {
                // a = |x + 2nZ|, a - x and a + x, each without the rounding of the others
                const Real a = n >= 0 ? x + 2.0 * n * Z : -2.0 * n * Z - x;
                const Real aMinusX = n >= 0 ? 2.0 * n * Z : -2.0 * (n * Z + x);
                const Real aPlusX = n >= 0 ? 2.0 * (x + n * Z) : -2.0 * n * Z;
                Real term;
                if (rhoSquared >= 0.0)
                {
                    const Real rho = std::sqrt(rhoSquared);
                    const Real e = -r * T - ((x + mu * T) * (x + mu * T) + aMinusX * aPlusX) / (2.0 * V);
                    const Real c1 = (rho * T - a) / stdDev, c2 = -(rho * T + a) / stdDev;
                    term = binaryWeightedCumulative(c2, e);
                    if (c1 <= 0.0)
                    {
                        term += binaryWeightedCumulative(c1, e);
                    }
                    else
                    {
                        const Real eMinus = mu < 0.0 ? Real(-2.0 * r / (rho - mu)) : Real(-(mu + rho) / volSquared);
                        term += std::exp(x * eMinus - aMinusX * rho / volSquared) * binaryCumulative(c1);
                    }
                }
                else
                {
                    const Real m = mu / volSquared;
                    const Real value = detail::unitPayoffAtHit(-a, m, r / volSquared, V, false).value;
                    term = value > 0.0 ? std::exp(std::log(value) + m * aMinusX) : Real(0.0);
                }
                sum += n >= 0 ? term : -term;
            }
            return sum;
        }

        /* The same value by Hui's series.  The part of its terms that doesn't depend on T falls only like 1 / i^3,
           but it sums to the value with no expiry, u = exp(-mu x / vol^2) sinh(rho (Z - x) / vol^2) / sinh(rho Z /
           vol^2), or sin(kappa (Z - x) / vol^2) / sin(kappa Z / vol^2) with kappa = sqrt(-rho^2) where rho^2 < 0.
           The rest is the value of a hit after T, the sum over i of exp(-mu x / vol^2) 2 / (i pi) w^2 / f
           exp(-f V / 2) sin(w x), with w = i pi / Z and f = w^2 + rho^2 / vol^4, whose terms fall like
           exp(-w^2 V / 2).  Its factor exp(-mu x / vol^2 - (rho^2 / vol^4) V / 2) is at most exp(x^2 / (2V) - rT). */
        Real binaryLowerHitByFourier(Real x, Real Z, Real mu, Real r, Real T, Real V)
        {
            const Real volSquared = V / T, muT = mu * T;
            const Real rhoSquared = mu * mu + 2.0 * r * volSquared;
            Real noExpiry;
            if (rhoSquared > 0.0)
            {
                const Real rho = std::sqrt(rhoSquared);
                const Real muPlusRho = mu < 0.0 ? Real(2.0 * r * volSquared / (rho - mu)) : Real(mu + rho);
                noExpiry = std::exp(-muPlusRho * x / volSquared) * std::expm1(-2.0 * rho * (Z - x) / volSquared) /
                           std::expm1(-2.0 * rho * Z / volSquared);
            }
            else if (rhoSquared < 0.0)
            {
                const Real kappa = std::sqrt(-rhoSquared);
                noExpiry = std::exp(-mu * x / volSquared) * std::sin(kappa * (Z - x) / volSquared) /
                           std::sin(kappa * Z / volSquared);
            }
            else
            {
                noExpiry = std::exp(-mu * x / volSquared) * (Z - x) / Z;
            }
            const int J = binaryFourierCount(std::sqrt(V) / Z);
            Real afterExpiry = 0.0;
            for (int i = 1; i <= J; ++i)
            {
                const Real w = i * M_PI / Z, f = w * w + rhoSquared / (volSquared * volSquared);
                afterExpiry += 2.0 / (i * M_PI) * (w * w / f) * std::exp(-0.5 * w * w * V) * std::sin(w * x);
            }
            return noExpiry - std::exp(-muT * (2.0 * x + muT) / (2.0 * V) - r * T) * afterExpiry;
        }

        Real binaryLowerHit(Real x, Real Z, Real mu, Real r, Real T, Real V)
        {
            // Hui's series needs u finite, short of the resonance at kappa Z / vol^2 = pi.  Up to pi / 2, its f stays
            // above 3/4 of w^2 and nothing in it cancels much.  Past that, which takes |r| T > pi^2 / 8 above
            // sqrt(V) = Z, the images still hold.
            const Real volSquared = V / T, rhoSquared = mu * mu + 2.0 * r * volSquared;
            if (std::sqrt(V) > Z && (rhoSquared >= 0.0 || std::sqrt(-rhoSquared) * Z <= M_PI_2 * volSquared))
                return binaryLowerHitByFourier(x, Z, mu, r, T, V);
            return binaryLowerHitByImages(x, Z, mu, r, T, V);
        }

    }

    namespace detail
    {

        // By images up to sqrt(V) = Z / 2, and by Hui's series above, as said at the top of this file.
        Real doubleBarrierNoTouchProbability(Real spot, Real lower, Real upper, Real carry, Time T, Real variance)
        {
            if (spot <= lower || spot >= upper)
                return 0.0;
            const Real x = std::log(spot / lower), Z = std::log(upper / lower);
            Real noTouch;
            if (variance < QL_EPSILON)
            {
                // The underlying follows its forward, and stays inside if the forward does.
                Real forward = x + carry * T;
                noTouch = forward > 0.0 && forward < Z ? 1.0 : 0.0;
            }
            else if (std::sqrt(variance) <= 0.5 * Z)
            {
                noTouch = binaryNoTouchByImages(x, Z, carry - 0.5 * variance / T, T, variance);
            }
            else
            {
                noTouch = binaryNoTouchByFourier(x, Z, carry - 0.5 * variance / T, T, variance);
            }
            return std::min(std::max(noTouch, 0.0), 1.0);
        }

    }

    AnalyticDoubleBarrierBinaryEngine::AnalyticDoubleBarrierBinaryEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process))
    {
        registerWith(process_);
    }

    void AnalyticDoubleBarrierBinaryEngine::calculate() const
    {

        if (arguments_.barrierType == DoubleBarrier::KIKO || arguments_.barrierType == DoubleBarrier::KOKI)
        {
            ext::shared_ptr<AmericanExercise> ex = ext::dynamic_pointer_cast<AmericanExercise>(arguments_.exercise);
            QL_REQUIRE(ex, "KIKO/KOKI options must have American exercise");
            QL_REQUIRE(ex->dates()[0] <= process_->blackVolatility()->referenceDate(),
                       "American option with window exercise not handled yet");
        }
        else
        {
            ext::shared_ptr<EuropeanExercise> ex = ext::dynamic_pointer_cast<EuropeanExercise>(arguments_.exercise);
            QL_REQUIRE(ex, "non-European exercise given");
        }
        ext::shared_ptr<CashOrNothingPayoff> payoff = ext::dynamic_pointer_cast<CashOrNothingPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "a cash-or-nothing payoff must be given");

        Real spot = process_->stateVariable()->value();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");

        Real variance = process_->blackVolatility()->blackVariance(arguments_.exercise->lastDate(), payoff->strike());
        Real barrier_lo = arguments_.barrier_lo;
        Real barrier_hi = arguments_.barrier_hi;
        DoubleBarrier::Type barrierType = arguments_.barrierType;
        QL_REQUIRE(barrier_lo > 0.0, "positive low barrier value required");
        QL_REQUIRE(barrier_hi > 0.0, "positive high barrier value required");
        QL_REQUIRE(barrier_lo < barrier_hi, "barrier_lo must be < barrier_hi");
        QL_REQUIRE(barrierType == DoubleBarrier::KnockIn || barrierType == DoubleBarrier::KnockOut ||
                       barrierType == DoubleBarrier::KIKO || barrierType == DoubleBarrier::KOKI,
                   "Unsupported barrier type");

        // degenerate cases
        switch (barrierType)
        {
            case DoubleBarrier::KnockOut:
                if (spot <= barrier_lo || spot >= barrier_hi)
                {
                    // knocked out, no value
                    results_.value = 0;
                    results_.delta = 0;
                    results_.gamma = 0;
                    results_.vega = 0;
                    results_.rho = 0;
                    return;
                }
                break;

            case DoubleBarrier::KnockIn:
                if (spot <= barrier_lo || spot >= barrier_hi)
                {
                    // knocked in, pays at expiry
                    const Date& expiry = arguments_.exercise->lastDate();
                    results_.value = payoff->cashPayoff() * process_->riskFreeRate()->discount(expiry);
                    results_.delta = 0;
                    results_.gamma = 0;
                    results_.vega = 0;
                    results_.rho = -process_->time(expiry) * results_.value;
                    return;
                }
                break;

            case DoubleBarrier::KIKO:
                if (spot >= barrier_hi)
                {
                    // knocked out, no value
                    results_.value = 0;
                    results_.delta = 0;
                    results_.gamma = 0;
                    results_.vega = 0;
                    results_.rho = 0;
                    return;
                }
                else if (spot <= barrier_lo)
                {
                    // knocked in, pays
                    results_.value = payoff->cashPayoff();
                    results_.delta = 0;
                    results_.gamma = 0;
                    results_.vega = 0;
                    results_.rho = 0;
                    return;
                }
                break;

            case DoubleBarrier::KOKI:
                if (spot <= barrier_lo)
                {
                    // knocked out, no value
                    results_.value = 0;
                    results_.delta = 0;
                    results_.gamma = 0;
                    results_.vega = 0;
                    results_.rho = 0;
                    return;
                }
                else if (spot >= barrier_hi)
                {
                    // knocked in, pays
                    results_.value = payoff->cashPayoff();
                    results_.delta = 0;
                    results_.gamma = 0;
                    results_.vega = 0;
                    results_.rho = 0;
                    return;
                }
                break;
        }

        Time residualTime = process_->time(arguments_.exercise->lastDate());
        QL_REQUIRE(residualTime > 0.0, "expiration time must be > 0");
        QL_REQUIRE(variance >= 0.0, "negative variance not allowed");
        Real cash = payoff->cashPayoff();
        Rate r = process_->riskFreeRate()->zeroRate(residualTime, Continuous, NoFrequency);
        Rate q = process_->dividendYield()->zeroRate(residualTime, Continuous, NoFrequency);
        Real x = std::log(spot / barrier_lo);
        Real Z = std::log(barrier_hi / barrier_lo);
        Real mu = r - q - 0.5 * variance / residualTime;

        switch (barrierType)
        {
            case DoubleBarrier::KnockOut:
            case DoubleBarrier::KnockIn:
            {
                DiscountFactor discount = process_->riskFreeRate()->discount(arguments_.exercise->lastDate());
                QL_REQUIRE(discount > 0.0, "positive discount required");
                Real noTouch = detail::doubleBarrierNoTouchProbability(spot, barrier_lo, barrier_hi, r - q,
                                                                       residualTime, variance);
                // The probability's error is absolute, and so is the knock-in's: a small knock-in loses its relative
                // accuracy.  With the spot at 100, barriers at 80 and 120, r at 5% and 2% volatility a year from
                // expiry, the knock-in is worth about 2.6e-11 of the cash.
                results_.value = cash * discount * (barrierType == DoubleBarrier::KnockOut ? noTouch : 1.0 - noTouch);
                break;
            }

            case DoubleBarrier::KIKO:
            case DoubleBarrier::KOKI:
            {
                // A KOKI is a KIKO on ln(U / S_t), whose drift is -mu.
                bool reflected = barrierType == DoubleBarrier::KOKI;
                Real distance = reflected ? std::log(barrier_hi / spot) : x;
                if (variance < QL_EPSILON)
                {
                    // The underlying follows its forward, and pays when the forward reaches the knock-in barrier,
                    // if it does by expiry.
                    Real drift = reflected ? q - r : r - q;
                    results_.value = drift < 0.0 && distance + drift * residualTime <= 0.0 ?
                                         Real(cash * std::exp(r * distance / drift)) :
                                         Real(0.0);
                }
                else
                {
                    results_.value = std::max(
                        cash * binaryLowerHit(distance, Z, reflected ? -mu : mu, r, residualTime, variance), 0.0);
                }
                break;
            }
        }
    }

}
