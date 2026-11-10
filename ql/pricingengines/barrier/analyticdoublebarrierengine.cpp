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
#include <ql/pricingengines/barrier/analyticdoublebarrierengine.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <cmath>
#include <utility>

namespace QuantLib
{

    namespace
    {

        // exp(logWeight) * (N(a) - N(b)).  At low volatility the weight can be huge while the
        // difference cancels or underflows, and their product is still finite.
        Real weightedDifference(const CumulativeNormalDistribution& f, Real logWeight, Real a, Real b)
        {
            // Above the median, N(a) - N(b) = N(-b) - N(-a) keeps its digits.
            if (a > 0.0 && b > 0.0)
            {
                const Real upper = a;
                a = -b;
                b = -upper;
            }
            const Real weight = std::exp(logWeight);
            // Unlike f, erfc keeps relative accuracy below the median, for the weight to multiply.
            if (std::isfinite(weight))
                return weight * 0.5 * (std::erfc(-a * M_SQRT_2) - std::erfc(-b * M_SQRT_2));
            return std::exp(logWeight + f.logValue(a)) - std::exp(logWeight + f.logValue(b));
        }

        /* A double knock-out's value before discounting, by the eigenfunction expansion of the log of the underlying
           between the barriers.  With x = ln(S / L), Z = ln(U / L), y = ln(S_T / L), V = vol^2 T and the drift
           mu = r - q - vol^2 / 2, the density of y at T, killed on leaving (0, Z), is the sum over i of
           2 / Z sin(w x) sin(w y) exp(-w^2 V / 2) exp(k (y - x) - k^2 V / 2), with w = i pi / Z and k = mu / vol^2.
           Against the payoff, each term is an integral of exp(a y) sin(w y) over the range that pays, in closed form.
           The terms fall like exp(-w^2 V / 2), so where sqrt(V) > Z / 2 a few of them suffice.  None of them is of
           the order of the spot where the value is small, as the images are there. */
        Real knockOutByEigenfunctions(
            Option::Type type, Real spot, Real strike, Real lower, Real upper, Real muT, Real variance)
        {
            const Real x = std::log(spot / lower), Z = std::log(upper / lower), k = muT / variance;
            // The range that pays, [yLo, yHi], ends at a barrier or at the strike.
            const bool strikeBelow = type == Option::Call && strike > lower;
            const bool strikeAbove = type == Option::Put && strike < upper;
            const Real yLo = strikeBelow ? std::log(strike / lower) : 0.0;
            const Real yHi = strikeAbove ? std::log(strike / lower) : Z;
            // exp(k (y - x) - k^2 V / 2) at the ends.  It is at most exp((y - x)^2 / (2V)), below e^2 here.
            const Real eLo = std::exp(-muT * (muT + 2.0 * (x - yLo)) / (2.0 * variance));
            const Real eHi = std::exp(-muT * (muT + 2.0 * (x - yHi)) / (2.0 * variance));
            // Terms after the J-th are at most j^2 exp(-(j^2 - 1) pi^2 V / (2 Z^2)) of the first, where j = J + 1.
            // With this J that is below 1e-18; J is 6 at sqrt(V) = Z / 2, and 1 from sqrt(V) = 2Z.
            const Real c = 0.5 * M_PI * M_PI * variance / (Z * Z);
            int J = 1;
            while ((J + 1.0) * (J + 1.0) * std::exp(-((J + 1.0) * (J + 1.0) - 1.0) * c) >= 1.0e-18)
                ++J;
            const Real twoKPlusOne = 2.0 * k + 1.0;
            Real sum = 0.0;
            for (int i = 1; i <= J; ++i)
            {
                // lo and hi are the antiderivative of exp(k y) sin(w y) (L e^y - K) at the two ends, over exp(k y)
                // and times (k^2 + w^2) ((k + 1)^2 + w^2), with L e^y = K at the strike and sin(w y) = 0 at a barrier.
                // So arranged, its parts of the order of the strike over k don't cancel where k is large.
                const Real w = i * M_PI / Z, dK = k * k + w * w, dA = dK + twoKPlusOne;
                const auto atStrike = [&](Real y)
                { return strike * (std::sin(w * y) * (w * w - k * k - k) + w * std::cos(w * y) * twoKPlusOne); };
                const Real lo = strikeBelow ? atStrike(yLo) : w * ((strike - lower) * dK + strike * twoKPlusOne);
                const Real hi = strikeAbove ? atStrike(yHi) :
                                              (i % 2 == 0 ? w : -w) * ((strike - upper) * dK + strike * twoKPlusOne);
                sum += std::exp(-0.5 * w * w * variance) * std::sin(w * x) * (eHi * hi - eLo * lo) / (dA * dK);
            }
            return (type == Option::Call ? 2.0 : -2.0) / Z * sum;
        }

    }

    AnalyticDoubleBarrierEngine::AnalyticDoubleBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                                             int series)
    : process_(std::move(process)), series_(series)
    {
        registerWith(process_);
    }

    void AnalyticDoubleBarrierEngine::calculate() const
    {

        QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "this engine handles only european options");

        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");

        Real strike = payoff->strike();
        QL_REQUIRE(strike > 0.0, "strike must be positive");

        Real spot = underlying();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");
        QL_REQUIRE(!triggered(spot), "barrier(s) already touched");

        DoubleBarrier::Type barrierType = arguments_.barrierType;

        if (barrierType == DoubleBarrier::KnockIn || barrierType == DoubleBarrier::KnockOut)
        {
            Real variance = process_->blackVolatility()->blackVariance(arguments_.exercise->lastDate(), strike);
            if (variance < QL_EPSILON)
            {
                // The underlying follows its forward, and leaves the corridor if the forward does.
                Real forward = spot * dividendDiscount() / riskFreeDiscount();
                bool hit = forward <= barrierLo() || forward >= barrierHi();
                results_.value =
                    hit == (barrierType == DoubleBarrier::KnockIn) ? riskFreeDiscount() * (*payoff)(forward) : 0.0;
                return;
            }
        }

        if (triggered(spot))
        {
            if (barrierType == DoubleBarrier::KnockIn)
                results_.value = vanillaEquivalent(); // knocked in
            else
                results_.value = 0.0; // knocked out
        }
        else
        {
            switch (payoff->optionType())
            {
                case Option::Call:
                    switch (barrierType)
                    {
                        case DoubleBarrier::KnockIn:
                            results_.value = callKI();
                            break;
                        case DoubleBarrier::KnockOut:
                            results_.value = callKO();
                            break;
                        case DoubleBarrier::KIKO:
                        case DoubleBarrier::KOKI:
                            QL_FAIL("unsupported double-barrier type: " << barrierType);
                        default:
                            QL_FAIL("unknown double-barrier type: " << barrierType);
                    }
                    break;
                case Option::Put:
                    switch (barrierType)
                    {
                        case DoubleBarrier::KnockIn:
                            results_.value = putKI();
                            break;
                        case DoubleBarrier::KnockOut:
                            results_.value = putKO();
                            break;
                        case DoubleBarrier::KIKO:
                        case DoubleBarrier::KOKI:
                            QL_FAIL("unsupported double-barrier type: " << barrierType);
                        default:
                            QL_FAIL("unknown double-barrier type: " << barrierType);
                    }
                    break;
                default:
                    QL_FAIL("unknown type");
            }
        }
    }


    Real AnalyticDoubleBarrierEngine::underlying() const
    {
        return process_->x0();
    }

    Real AnalyticDoubleBarrierEngine::strike() const
    {
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        return payoff->strike();
    }

    Time AnalyticDoubleBarrierEngine::residualTime() const
    {
        return process_->time(arguments_.exercise->lastDate());
    }

    Volatility AnalyticDoubleBarrierEngine::volatility() const
    {
        return process_->blackVolatility()->blackVol(residualTime(), strike());
    }

    Real AnalyticDoubleBarrierEngine::volatilitySquared() const
    {
        return volatility() * volatility();
    }

    Real AnalyticDoubleBarrierEngine::stdDeviation() const
    {
        return volatility() * std::sqrt(residualTime());
    }

    Real AnalyticDoubleBarrierEngine::barrierLo() const
    {
        return arguments_.barrier_lo;
    }

    Real AnalyticDoubleBarrierEngine::barrierHi() const
    {
        return arguments_.barrier_hi;
    }

    Rate AnalyticDoubleBarrierEngine::riskFreeRate() const
    {
        return process_->riskFreeRate()->zeroRate(residualTime(), Continuous, NoFrequency);
    }

    DiscountFactor AnalyticDoubleBarrierEngine::riskFreeDiscount() const
    {
        return process_->riskFreeRate()->discount(residualTime());
    }

    Rate AnalyticDoubleBarrierEngine::dividendYield() const
    {
        return process_->dividendYield()->zeroRate(residualTime(), Continuous, NoFrequency);
    }

    DiscountFactor AnalyticDoubleBarrierEngine::dividendDiscount() const
    {
        return process_->dividendYield()->discount(residualTime());
    }

    Rate AnalyticDoubleBarrierEngine::costOfCarry() const
    {
        return riskFreeRate() - dividendYield();
    }

    Real AnalyticDoubleBarrierEngine::vanillaEquivalent() const
    {
        // Call KI equates to vanilla - callKO
        ext::shared_ptr<StrikedTypePayoff> payoff = ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        Real forwardPrice = underlying() * dividendDiscount() / riskFreeDiscount();
        BlackCalculator black(payoff, forwardPrice, stdDeviation(), riskFreeDiscount());
        Real vanilla = black.value();
        if (vanilla < 0.0)
            vanilla = 0.0;
        return vanilla;
    }

    Real AnalyticDoubleBarrierEngine::callKO() const
    {
        if (strike() >= barrierHi())
            return 0.0;
        const Real stdDev = stdDeviation();
        // Where the volatility is wide against the corridor, the images below need more terms the wider it is, of the
        // order of the spot, which cancel as the value falls.  The eigenfunction expansion needs a few there.
        if (stdDev > 0.5 * std::log(barrierHi() / barrierLo()))
        {
            const Real variance = stdDev * stdDev, muT = costOfCarry() * residualTime() - 0.5 * variance;
            const Real value =
                knockOutByEigenfunctions(Option::Call, underlying(), strike(), barrierLo(), barrierHi(), muT, variance);
            return std::max(0.0, riskFreeDiscount() * value);
        }
        // The integral runs over the terminal spots inside the barriers that pay.
        const Real bound = std::max(strike(), barrierLo());
        // N.B. for flat barriers mu3=mu1 and mu2=0
        Real mu1 = 2 * costOfCarry() / volatilitySquared() + 1;
        Real bsigma = (costOfCarry() + volatilitySquared() / 2.0) * residualTime() / stdDev;

        const Real logLo = std::log(barrierLo()), logHi = std::log(barrierHi());
        const Real logSpot = std::log(underlying()), logBound = std::log(bound);

        Real acc1 = 0;
        Real acc2 = 0;
        for (int n = -series_; n <= series_; ++n)
        {
            const Real logRatio = n * (logHi - logLo);
            const Real logImage = (n + 1) * logLo - n * logHi - logSpot;
            const Real d1 = (logSpot + 2.0 * logRatio - logBound) / stdDev + bsigma;
            const Real d2 = (logSpot + 2.0 * logRatio - logHi) / stdDev + bsigma;
            const Real d3 = (logSpot + 2.0 * logImage - logBound) / stdDev + bsigma;
            const Real d4 = (logSpot + 2.0 * logImage - logHi) / stdDev + bsigma;

            acc1 += weightedDifference(f_, mu1 * logRatio, d1, d2) - weightedDifference(f_, mu1 * logImage, d3, d4);

            acc2 += weightedDifference(f_, (mu1 - 2) * logRatio, d1 - stdDev, d2 - stdDev) -
                    weightedDifference(f_, (mu1 - 2) * logImage, d3 - stdDev, d4 - stdDev);
        }

        Real rend = std::exp(-dividendYield() * residualTime());
        Real kov = underlying() * rend * acc1 - strike() * riskFreeDiscount() * acc2;
        return std::max(0.0, kov);
    }

    Real AnalyticDoubleBarrierEngine::callKI() const
    {
        // Call KI equates to vanilla - callKO
        return std::max(0.0, vanillaEquivalent() - callKO());
    }

    Real AnalyticDoubleBarrierEngine::putKO() const
    {
        if (strike() <= barrierLo())
            return 0.0;
        const Real stdDev = stdDeviation();
        // As in callKO.
        if (stdDev > 0.5 * std::log(barrierHi() / barrierLo()))
        {
            const Real variance = stdDev * stdDev, muT = costOfCarry() * residualTime() - 0.5 * variance;
            const Real value =
                knockOutByEigenfunctions(Option::Put, underlying(), strike(), barrierLo(), barrierHi(), muT, variance);
            return std::max(0.0, riskFreeDiscount() * value);
        }
        // The integral runs over the terminal spots inside the barriers that pay.
        const Real bound = std::min(strike(), barrierHi());
        Real mu1 = 2 * costOfCarry() / volatilitySquared() + 1;
        Real bsigma = (costOfCarry() + volatilitySquared() / 2.0) * residualTime() / stdDev;

        const Real logLo = std::log(barrierLo()), logHi = std::log(barrierHi());
        const Real logSpot = std::log(underlying()), logBound = std::log(bound);

        Real acc1 = 0;
        Real acc2 = 0;
        for (int n = -series_; n <= series_; ++n)
        {
            const Real logRatio = n * (logHi - logLo);
            const Real logImage = (n + 1) * logLo - n * logHi - logSpot;
            const Real y1 = (logSpot + 2.0 * logRatio - logLo) / stdDev + bsigma;
            const Real y2 = (logSpot + 2.0 * logRatio - logBound) / stdDev + bsigma;
            const Real y3 = (logSpot + 2.0 * logImage - logLo) / stdDev + bsigma;
            const Real y4 = (logSpot + 2.0 * logImage - logBound) / stdDev + bsigma;

            acc1 += weightedDifference(f_, (mu1 - 2) * logRatio, y1 - stdDev, y2 - stdDev) -
                    weightedDifference(f_, (mu1 - 2) * logImage, y3 - stdDev, y4 - stdDev);

            acc2 += weightedDifference(f_, mu1 * logRatio, y1, y2) - weightedDifference(f_, mu1 * logImage, y3, y4);
        }

        Real rend = std::exp(-dividendYield() * residualTime());
        Real kov = strike() * riskFreeDiscount() * acc1 - underlying() * rend * acc2;
        return std::max(0.0, kov);
    }

    Real AnalyticDoubleBarrierEngine::putKI() const
    {
        // Put KI equates to vanilla - putKO
        return std::max(0.0, vanillaEquivalent() - putKO());
    }


}
