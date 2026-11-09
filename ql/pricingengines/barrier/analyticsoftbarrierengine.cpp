/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 William Day

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
#include <ql/instruments/barrieroption.hpp>
#include <ql/mathconstants.hpp>
#include <ql/pricingengines/barrier/analyticbarrierengine.hpp>
#include <ql/pricingengines/barrier/analyticsoftbarrierengine.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>


namespace QuantLib
{

    namespace
    {

        /* The knock-in is the average over [L, U] of single-barrier knock-ins, and each piece of it is a sum of
           integrals of e^{ku} N(w) over u = ln(H/S), with w = alpha u + beta.  At low volatility k, alpha and beta
           grow like 1/vol^2 and 1/vol, and k vanishes at b = 0 or b = -vol^2, so the helpers below evaluate those
           integrals through R = N/n, the Mills ratio, and through e^{ku} n(w), which the reflection identities give
           without either. */

        // The Mills-ratio forms are used while their arguments are at most this.
        constexpr Real softMillsLimit = 5.0;

        Real softCumulative(Real x)
        {
            return 0.5 * std::erfc(-x * M_SQRT1_2);
        }

        Real softComplement(Real x)
        {
            return 0.5 * std::erfc(x * M_SQRT1_2);
        }

        Real softDensity(Real x)
        {
            return M_SQRT1_2 * M_1_SQRTPI * std::exp(-0.5 * x * x);
        }

        Real softLogDensity(Real x)
        {
            return -0.5 * x * x - 0.5 * std::log(M_TWOPI);
        }

        // R(v) = N(v) / n(v), for v up to softMillsLimit
        Real millsRatio(Real v)
        {
            if (v >= -10.0)
                return M_SQRT2 * M_SQRTPI * softCumulative(v) * std::exp(0.5 * v * v);
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

        // R'(v) = 1 + v R(v)
        Real millsRatioDerivative(Real v)
        {
            if (v >= -10.0)
                return 1.0 + v * millsRatio(v);
            const Real v2 = v * v;
            Real term = 1.0 / v2, sum = term;
            for (Size i = 2; std::fabs(term) >= QL_EPSILON * std::fabs(sum); ++i)
            {
                term *= -(2.0 * i - 1.0) / v2;
                sum += term;
            }
            return sum;
        }

        // The average of f over [centre - half, centre + half], by 5-point Gauss-Legendre.
        template <class F>
        Real gaussLegendre5Average(const F& f, Real centre, Real half)
        {
            const Real x1 = 0.5384693101056830910, x2 = 0.9061798459386639928;
            const Real w0 = 0.5688888888888888889, w1 = 0.4786286704993664680, w2 = 0.2369268850561890875;
            return 0.5 * (w0 * f(centre) + w1 * (f(centre - x1 * half) + f(centre + x1 * half)) +
                          w2 * (f(centre - x2 * half) + f(centre + x2 * half)));
        }

        // (R(x) - R(y)) / (x - y)
        Real millsRatioDividedDifference(Real x, Real y)
        {
            const Real d = x - y, m = 0.5 * (x + y);
            if (d == 0.0)
                return millsRatioDerivative(m);
            // Where the difference would cancel, average R' instead.  R varies at a relative rate of about
            // 1/|m| below -1 and about max(1, m) above.
            const Real rate = m < -1.0 ? Real(-1.0 / m) : std::max(1.0, m);
            if (std::fabs(d) * rate <= 0.05)
                return gaussLegendre5Average(millsRatioDerivative, m, 0.5 * d);
            return (millsRatio(x) - millsRatio(y)) / d;
        }

        // (N(w) - N(w - delta)) / delta
        Real cumulativeDividedDifference(Real w, Real delta)
        {
            if (delta == 0.0)
                return softDensity(w);
            if (std::fabs(delta) * std::max(1.0, std::fabs(w)) <= 0.1)
                return gaussLegendre5Average(softDensity, w - 0.5 * delta, 0.5 * delta);
            if (w > 0.0 && w - delta > 0.0)
                return (softComplement(w - delta) - softComplement(w)) / delta;
            return (softCumulative(w) - softCumulative(w - delta)) / delta;
        }

        Real relativeExpm1(Real x)
        {
            return x == 0.0 ? 1.0 : std::expm1(x) / x;
        }

        // e^p N(w), given the log of e^p n(w) computed without cancellation
        Real expTimesCumulative(Real logPDensity, Real p, Real w)
        {
            return w <= softMillsLimit ? Real(std::exp(logPDensity) * millsRatio(w)) :
                                         Real(std::exp(p) * softCumulative(w));
        }

        // One end of an integral of e^{ku} N(w), w = alpha u + beta: w, the log of e^{ku} n(w) computed without
        // cancellation, and ku.
        struct SoftIntegralEnd
        {
            Real w, logP, ku;
        };

        // The integral of e^{ku} N(alpha u + beta) between two ends, where delta = k / alpha and
        // kappa = delta^2 / 2 - delta beta, so that e^{ku} n(w) = e^kappa n(w - delta).
        Real expTimesCumulativeIntegral(
            Real k, Real alpha, Real delta, Real kappa, const SoftIntegralEnd& a, const SoftIntegralEnd& b)
        {
            // The antiderivative (e^{ku} N(w) - e^kappa N(w - delta)) / k, in whichever of three equal forms is
            // accurate at this end.  The first two have no 1/k, and the first no power of a large k.
            auto plain = [&](const SoftIntegralEnd& e) -> Real
            {
                if (std::max(e.w, e.w - delta) <= softMillsLimit)
                    return std::exp(e.logP) * millsRatioDividedDifference(e.w, e.w - delta) / alpha;
                const Real x = delta * (e.w - 0.5 * delta);
                if (std::fabs(x) <= 1.0)
                    return std::exp(e.ku - x) / alpha *
                           ((e.w - 0.5 * delta) * relativeExpm1(x) * softCumulative(e.w) +
                            cumulativeDividedDifference(e.w, delta));
                return (expTimesCumulative(e.logP, e.ku, e.w) - expTimesCumulative(e.logP, kappa, e.w - delta)) / k;
            };
            const Real pa = plain(a), pb = plain(b);
            if (delta != 0.0 && a.w - delta > 0.0 && b.w - delta > 0.0)
            {
                // The same plus e^kappa / k, (e^{ku} N(w) + e^kappa (1 - N(w - delta))) / k, which doesn't carry
                // that constant through the difference where it dominates.
                auto shifted = [&](const SoftIntegralEnd& e)
                { return (expTimesCumulative(e.logP, e.ku, e.w) + std::exp(e.logP) * millsRatio(delta - e.w)) / k; };
                const Real sa = shifted(a), sb = shifted(b);
                if (std::max(std::fabs(sa), std::fabs(sb)) < std::max(std::fabs(pa), std::fabs(pb)))
                    return sb - sa;
            }
            return pb - pa;
        }


        // The knock-in value of a down-and-in call or an up-and-in put with a soft barrier on [L, U], for a spot
        // that hasn't passed the whole range.
        class SoftBarrierKnockIn
        {
          public:
            SoftBarrierKnockIn(Real spot,
                               Real strike,
                               DiscountFactor riskFreeDiscount,
                               DiscountFactor dividendDiscount,
                               Real variance,
                               Option::Type type);
            Real value(Real L, Real U) const;

          private:
            enum class Region
            {
                Vanilla,
                C,
                ABD
            };
            bool isNarrow(Real L, Real U) const;
            Real pointValue(Region region, Real H) const;
            Real cIntegral(Real from, Real to) const;
            Real abdIntegral(Real from, Real to) const;
            Real logE(Real u) const { return -2.0 * u * (u + lnSX_) / variance_; }

            Real S_, X_, phi_, F_, K_, discount_, bT_, variance_;
            // used where variance_ >= QL_EPSILON
            Real s_ = 0.0, lnSX_ = 0.0, m1_ = 0.0, x1_ = 0.0, k1_ = 0.0, k2_ = 0.0, vanilla_ = 0.0;
        };

        /* In the notation of Reiner and Rubinstein's single-barrier formulas, with phi = 1 for the call and -1 for
           the put, F = S e^{-qT}, K = X e^{-rT}, s = vol sqrt(T), and mu = (b + vol^2 / 2) / vol^2:
             m1 = mu s, x1 = ln(S/X) / s + m1, x2 = -u / s + m1, y1 = (2u + ln(S/X)) / s + m1, y2 = u / s + m1,
           and k1 = 2 mu + 1, k2 = 2 mu - 1 are the powers of H/S in the integrals over u = ln(H/S). */
        SoftBarrierKnockIn::SoftBarrierKnockIn(Real spot,
                                               Real strike,
                                               DiscountFactor riskFreeDiscount,
                                               DiscountFactor dividendDiscount,
                                               Real variance,
                                               Option::Type type)
        : S_(spot), X_(strike), phi_(type == Option::Call ? 1.0 : -1.0), F_(spot * dividendDiscount),
          K_(strike * riskFreeDiscount), discount_(riskFreeDiscount),
          bT_(std::log(dividendDiscount / riskFreeDiscount)), variance_(variance)
        {
            if (variance_ < QL_EPSILON)
                return;
            s_ = std::sqrt(variance_);
            lnSX_ = std::log(S_ / X_);
            m1_ = bT_ / s_ + 0.5 * s_;
            x1_ = lnSX_ / s_ + m1_;
            k1_ = 2.0 * (bT_ + variance_) / variance_;
            k2_ = 2.0 * bT_ / variance_;
            vanilla_ = phi_ * (F_ * softCumulative(phi_ * x1_) - K_ * softCumulative(phi_ * (x1_ - s_)));
        }

        Real SoftBarrierKnockIn::value(Real L, Real U) const
        {
            const bool down = phi_ > 0.0;
            if (variance_ < QL_EPSILON)
            {
                // The underlying follows its forward, and crosses the barriers above its lowest point, for a down
                // barrier, or below its highest, for an up one.
                const Real forward = F_ / discount_;
                const Real crossed =
                    down ? (U - std::min(S_, forward)) / (U - L) : (std::max(S_, forward) - L) / (U - L);
                const Real intrinsic = down ? forward - X_ : X_ - forward;
                return discount_ * std::max(intrinsic, 0.0) * std::clamp(crossed, 0.0, 1.0);
            }

            // Past the spot the option has knocked in, and is worth the vanilla.  Short of it, the single-barrier
            // knock-in is Reiner and Rubinstein's C up to the strike, and A - B + D beyond it.
            struct Piece
            {
                Region region;
                Real from, to;
            };
            const std::array<Piece, 3> pieces =
                down ? std::array<Piece, 3>{{{Region::Vanilla, std::max(L, S_), U},
                                             {Region::C, L, std::min({U, S_, X_})},
                                             {Region::ABD, std::max(L, X_), std::min(U, S_)}}} :
                       std::array<Piece, 3>{{{Region::Vanilla, L, std::min(S_, U)},
                                             {Region::C, std::max({L, S_, X_}), U},
                                             {Region::ABD, std::max(L, S_), std::min(U, X_)}}};

            const bool narrow = isNarrow(L, U);
            Real sum = 0.0;
            for (const auto& piece : pieces)
            {
                if (!(piece.from < piece.to))
                    continue;
                const Real length = piece.to - piece.from;
                if (narrow)
                    sum += length * gaussLegendre5Average([&](Real H) { return pointValue(piece.region, H); },
                                                          piece.from + 0.5 * length, 0.5 * length);
                else if (piece.region == Region::Vanilla)
                    sum += vanilla_ * length;
                else if (piece.region == Region::C)
                    sum += cIntegral(piece.from, piece.to);
                else
                    sum += abdIntegral(piece.from, piece.to);
            }
            return sum / (U - L);
        }

        // Whether [L, U] is so short that Gauss-Legendre on the knock-in is exact to machine precision, and the
        // closed form, a difference between its ends, would cancel.  The knock-in varies in u at a rate below
        // `rate`.
        bool SoftBarrierKnockIn::isNarrow(Real L, Real U) const
        {
            Real largest = 0.0;
            for (Real H : {L, U})
            {
                const Real u = std::log(H / S_);
                largest = std::max({largest, std::fabs((2.0 * u + lnSX_) / s_ + m1_), std::fabs(-u / s_ + m1_),
                                    std::fabs(u / s_ + m1_)});
            }
            const Real rate = 2.0 / s_ * (2.0 + s_ + std::fabs(m1_) + largest);
            return std::log(U / L) * rate < 0.1;
        }

        // The single-barrier knock-in at H.  (H/S)^{2 mu} n(y1) = n(x1) E and (H/S)^{2 mu - 2} n(y1 - s) =
        // n(x1 - s) E, with E = exp(logE(u)); (H/S)^{2 mu} n(y2) = n(x2) and (H/S)^{2 mu - 2} n(y2 - s) = n(x2 - s).
        Real SoftBarrierKnockIn::pointValue(Region region, Real H) const
        {
            if (region == Region::Vanilla)
                return vanilla_;
            const Real u = std::log(H / S_);
            if (region == Region::C)
            {
                const Real y1 = (2.0 * u + lnSX_) / s_ + m1_;
                const Real w1 = phi_ * y1, w2 = phi_ * (y1 - s_);
                // F n(x1) = K n(x1 - s), so C = s F n(x1) E R[w1, w2]
                if (std::max(w1, w2) <= softMillsLimit)
                    return s_ * F_ * std::exp(softLogDensity(x1_) + logE(u)) * millsRatioDividedDifference(w1, w2);
                return phi_ * (F_ * expTimesCumulative(softLogDensity(x1_) + logE(u), (k1_ - 1.0) * u, w1) -
                               K_ * expTimesCumulative(softLogDensity(x1_ - s_) + logE(u), (k2_ - 1.0) * u, w2));
            }
            const Real x2 = -u / s_ + m1_, y2 = u / s_ + m1_;
            const Real B = phi_ * (F_ * softCumulative(phi_ * x2) - K_ * softCumulative(phi_ * (x2 - s_)));
            const Real D = phi_ * (F_ * expTimesCumulative(softLogDensity(x2), (k1_ - 1.0) * u, phi_ * y2) -
                                   K_ * expTimesCumulative(softLogDensity(x2 - s_), (k2_ - 1.0) * u, phi_ * (y2 - s_)));
            return vanilla_ - B + D;
        }

        // The integral of C over [from, to], as phi S [F J1 - K J2], with J1 and J2 integrals of
        // e^{k1 u} N(phi y1) and e^{k2 u} N(phi (y1 - s)).
        Real SoftBarrierKnockIn::cIntegral(Real from, Real to) const
        {
            std::array<SoftIntegralEnd, 2> c1, c2;
            for (Size i = 0; i < 2; ++i)
            {
                const Real u = std::log((i == 0 ? from : to) / S_);
                const Real y1 = (2.0 * u + lnSX_) / s_ + m1_;
                c1[i] = {phi_ * y1, u + softLogDensity(x1_) + logE(u), k1_ * u};
                c2[i] = {phi_ * (y1 - s_), u + softLogDensity(x1_ - s_) + logE(u), k2_ * u};
            }
            const Real alpha = 2.0 * phi_ / s_;
            const Real j1 =
                expTimesCumulativeIntegral(k1_, alpha, phi_ * (m1_ + 0.5 * s_),
                                           -(m1_ + 0.5 * s_) * (0.5 * (m1_ - 0.5 * s_) + lnSX_ / s_), c1[0], c1[1]);
            const Real j2 =
                expTimesCumulativeIntegral(k2_, alpha, phi_ * (m1_ - 0.5 * s_),
                                           -(m1_ - 0.5 * s_) * (0.5 * (m1_ - 1.5 * s_) + lnSX_ / s_), c2[0], c2[1]);
            return phi_ * S_ * (F_ * j1 - K_ * j2);
        }

        // The integral of A - B + D over [from, to].  A is the vanilla, B = phi [F N(phi x2) - K N(phi (x2 - s))],
        // and D is C with y2 in place of y1.
        Real SoftBarrierKnockIn::abdIntegral(Real from, Real to) const
        {
            std::array<SoftIntegralEnd, 2> b1, b2, d1, d2;
            for (Size i = 0; i < 2; ++i)
            {
                const Real u = std::log((i == 0 ? from : to) / S_);
                const Real x2 = -u / s_ + m1_, y2 = u / s_ + m1_;
                const Real logP1 = u + softLogDensity(x2), logP2 = u + softLogDensity(x2 - s_);
                b1[i] = {phi_ * x2, logP1, u};
                b2[i] = {phi_ * (x2 - s_), logP2, u};
                d1[i] = {phi_ * y2, logP1, k1_ * u};
                d2[i] = {phi_ * (y2 - s_), logP2, k2_ * u};
            }
            const Real B =
                phi_ * S_ *
                (F_ * expTimesCumulativeIntegral(1.0, -phi_ / s_, -phi_ * s_, bT_ + variance_, b1[0], b1[1]) -
                 K_ * expTimesCumulativeIntegral(1.0, -phi_ / s_, -phi_ * s_, bT_, b2[0], b2[1]));
            const Real D =
                phi_ * S_ *
                (F_ * expTimesCumulativeIntegral(k1_, phi_ / s_, phi_ * (2.0 * m1_ + s_), bT_ + variance_, d1[0],
                                                 d1[1]) -
                 K_ * expTimesCumulativeIntegral(k2_, phi_ / s_, phi_ * (2.0 * m1_ - s_), bT_, d2[0], d2[1]));
            return vanilla_ * (to - from) - B + D;
        }

    }


    AnalyticSoftBarrierEngine::AnalyticSoftBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process))
    {
        registerWith(process_);
    }


    void AnalyticSoftBarrierEngine::calculate() const
    {

        // Market data
        Real S = underlying();
        Real X = strike();
        Rate r = riskFreeRate();
        Rate q = dividendYield();
        Volatility sigma = volatility();

        // Barrier parameters
        Real U = barrierHi();
        Real L = barrierLo();
        Barrier::Type barrierType = arguments_.barrierType;

        // Option parameters
        Time T = residualTime();
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        Option::Type optionType = payoff->optionType();

        validateInputs(S, X, r, q, T, U, L, optionType, barrierType, sigma);

        bool isKnockedIn = (barrierType == Barrier::DownIn && S <= L) || (barrierType == Barrier::UpIn && S >= U);
        bool isKnockedOut = (barrierType == Barrier::DownOut && S <= L) || (barrierType == Barrier::UpOut && S >= U);


        // edge case 1: fully knocked in options should be priced as vanilla (there are no more barrier features to
        // consider)
        if (isKnockedIn)
        {
            results_.value = vanillaEquivalent();
            return;
        }

        // edge case 2: knocked out options are worthless
        else if (isKnockedOut)
        {
            results_.value = 0.0;
            return;
        }

        // edge case 3: with U = L the option is a single-barrier option, priced as one on the same process
        if (U == L)
        {
            BarrierOption option(barrierType, L, 0.0, payoff, arguments_.exercise);
            option.setPricingEngine(ext::make_shared<AnalyticBarrierEngine>(process_));
            results_.value = option.NPV();
            return;
        }

        // The knock-in below covers down barriers for calls and up barriers for puts.
        const bool isDown = barrierType == Barrier::DownIn || barrierType == Barrier::DownOut;
        QL_REQUIRE((optionType == Option::Call) == isDown,
                   "soft-barrier formula not available for " << barrierType << " " << optionType << " options");

        // soft barrier pricing logic.  The knock-in's pieces cancel where it is negligible, or all but the vanilla,
        // so neither value is let go negative.
        Real w = std::max(
            SoftBarrierKnockIn(S, X, riskFreeDiscount(), dividendDiscount(), sigma * sigma * T, optionType).value(L, U),
            0.0);
        results_.value = (barrierType == Barrier::DownIn || barrierType == Barrier::UpIn) ?
                             w // knock in price
                             :
                             std::max(vanillaEquivalent() - w, 0.0); // knock out price
    }


    // helper function to check inputs are reasonable
    void AnalyticSoftBarrierEngine::validateInputs(Real S,
                                                   Real X,
                                                   Rate r,
                                                   Rate q,
                                                   Time T,
                                                   Real U,
                                                   Real L,
                                                   Option::Type optionType,
                                                   Barrier::Type barrierType,
                                                   Real sigma) const
    {
        // Core Parameter checks
        QL_REQUIRE(S > 0.0, "Spot price must be > 0");
        QL_REQUIRE(X > 0.0, "Strike price must be > 0");
        QL_REQUIRE(T > 0.0, "Option must have time to maturity > 0");
        QL_REQUIRE(sigma >= 0.0, "Volatility must be >= 0");
        QL_REQUIRE(optionType == Option::Call || optionType == Option::Put, "Invalid option type");
        QL_REQUIRE(r <= 1.0 && r >= -0.05, "Interest rate must be between -5% and 100%");
        QL_REQUIRE(q <= 1.0 && q >= -0.1, "Dividend yield must be between -10% and 100%");


        // Barrier type checks
        QL_REQUIRE(barrierType == Barrier::DownIn || barrierType == Barrier::DownOut || barrierType == Barrier::UpIn ||
                       barrierType == Barrier::UpOut,
                   "Invalid barrier type");
        QL_REQUIRE(L != Null<Real>(), "no low barrier given");
        QL_REQUIRE(U != Null<Real>(), "no high barrier given");
        QL_REQUIRE(U > 0.0 && L > 0.0, "Barrier levels must be positive");
        QL_REQUIRE(U >= L, "Upper barrier must be greater than or equal to lower barrier");
    }


    // helper functions
    Real AnalyticSoftBarrierEngine::underlying() const
    {
        return process_->x0();
    }

    Real AnalyticSoftBarrierEngine::strike() const
    {
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        return payoff->strike();
    }

    Time AnalyticSoftBarrierEngine::residualTime() const
    {
        return process_->time(arguments_.exercise->lastDate());
    }

    Volatility AnalyticSoftBarrierEngine::volatility() const
    {
        return process_->blackVolatility()->blackVol(residualTime(), strike());
    }

    Real AnalyticSoftBarrierEngine::stdDeviation() const
    {
        return volatility() * std::sqrt(residualTime());
    }

    Real AnalyticSoftBarrierEngine::barrierLo() const
    {
        return arguments_.barrier_lo;
    }

    Real AnalyticSoftBarrierEngine::barrierHi() const
    {
        return arguments_.barrier_hi;
    }

    Rate AnalyticSoftBarrierEngine::riskFreeRate() const
    {
        return process_->riskFreeRate()->zeroRate(residualTime(), Continuous, NoFrequency);
    }

    DiscountFactor AnalyticSoftBarrierEngine::riskFreeDiscount() const
    {
        return process_->riskFreeRate()->discount(residualTime());
    }

    Rate AnalyticSoftBarrierEngine::dividendYield() const
    {
        return process_->dividendYield()->zeroRate(residualTime(), Continuous, NoFrequency);
    }

    DiscountFactor AnalyticSoftBarrierEngine::dividendDiscount() const
    {
        return process_->dividendYield()->discount(residualTime());
    }


    Real AnalyticSoftBarrierEngine::vanillaEquivalent() const
    {
        ext::shared_ptr<StrikedTypePayoff> payoff = ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        Real forwardPrice = underlying() * dividendDiscount() / riskFreeDiscount();
        BlackCalculator black(payoff, forwardPrice, stdDeviation(), riskFreeDiscount());
        Real vanilla = black.value();
        return std::max(vanilla, 0.0);
    }

}
