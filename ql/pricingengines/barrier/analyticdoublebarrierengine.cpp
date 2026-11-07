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
        // The integral runs over the terminal spots inside the barriers that pay.
        const Real bound = std::max(strike(), barrierLo());
        // N.B. for flat barriers mu3=mu1 and mu2=0
        Real mu1 = 2 * costOfCarry() / volatilitySquared() + 1;
        Real bsigma = (costOfCarry() + volatilitySquared() / 2.0) * residualTime() / stdDeviation();

        const Real stdDev = stdDeviation();
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
        // The integral runs over the terminal spots inside the barriers that pay.
        const Real bound = std::min(strike(), barrierHi());
        Real mu1 = 2 * costOfCarry() / volatilitySquared() + 1;
        Real bsigma = (costOfCarry() + volatilitySquared() / 2.0) * residualTime() / stdDeviation();

        const Real stdDev = stdDeviation();
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
