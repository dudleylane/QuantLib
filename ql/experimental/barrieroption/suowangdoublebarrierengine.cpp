/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2013 Yue Tian

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
#include <ql/experimental/barrieroption/suowangdoublebarrierengine.hpp>
#include <ql/instruments/europeanoption.hpp>
#include <ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.hpp>
#include <ql/pricingengines/barrier/analyticdoublebarrierengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <cmath>
#include <utility>

namespace QuantLib
{

    SuoWangDoubleBarrierEngine::SuoWangDoubleBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                                           int series)
    : process_(std::move(process)), series_(series)
    {
        registerWith(process_);
    }

    void SuoWangDoubleBarrierEngine::calculate() const
    {

        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        QL_REQUIRE(payoff->strike() > 0.0, "strike must be positive");

        Real K = payoff->strike();
        Real S = process_->x0();
        QL_REQUIRE(S > 0.0, "negative or null underlying given");
        QL_REQUIRE(!triggered(S), "barrier touched");

        DoubleBarrier::Type barrierType = arguments_.barrierType;
        QL_REQUIRE(barrierType == DoubleBarrier::KnockOut || barrierType == DoubleBarrier::KnockIn,
                   "only KnockIn and KnockOut options supported");

        Real L = arguments_.barrier_lo;
        Real H = arguments_.barrier_hi;
        Real K_up = std::min(H, K);
        Real K_down = std::max(L, K);
        Time T = residualTime();
        Real rd = riskFreeRate();
        Real dd = riskFreeDiscount();
        Real rf = dividendYield();
        Real df = dividendDiscount();
        Real vol = volatility();
        Real mu = rd - rf - vol * vol / 2.0;

        // european option
        EuropeanOption europeanOption(payoff, arguments_.exercise);
        ext::shared_ptr<PricingEngine> analyticEuropeanEngine = ext::make_shared<AnalyticEuropeanEngine>(process_);
        europeanOption.setPricingEngine(analyticEuropeanEngine);
        Real european = europeanOption.NPV();

        const Real variance = vol * vol * T, stdDev = vol * std::sqrt(T);
        const Option::Type type = payoff->optionType();
        QL_REQUIRE(type == Option::Call || type == Option::Put, "option type not recognized");
        Real barrierOut = 0;
        if (variance < QL_EPSILON)
        {
            // The underlying follows its forward, and leaves the corridor if the forward does.
            const Real forward = S * df / dd;
            if (forward > L && forward < H)
                barrierOut = dd * (*payoff)(forward);
        }
        else if ((type == Option::Call && K >= H) || (type == Option::Put && K <= L))
        {
            // Nothing that survives pays.
        }
        else if (stdDev > 0.5 * std::log(H / L))
        {
            // Where the volatility is wide against the corridor, the images below need more terms the wider it is, of
            // the order of the spot, which cancel as the value falls.  The eigenfunction expansion needs a few there.
            barrierOut = dd * detail::doubleKnockOutByEigenfunctions(type, S, K, L, H, mu * T, variance);
        }
        else
        {
            // The images, with their weights as logs: at low volatility the weights overflow while the differences
            // of f they multiply underflow or cancel, and their products are still finite.
            const Real logRatio = std::log(L / H), logHS = std::log(H / S), k = 2.0 * mu / (vol * vol);
            const Real logHSWeight = k * logHS, HHoverS = H * H / S;
            const auto d = [&](Real logX) { return (logX + (vol * vol + mu) * T) / stdDev; };
            for (int n = -series_; n < series_; n++)
            {
                // the logs of (L/H)^(2n mu/vol^2) and (L/H)^(2n)
                const Real logW = n * k * logRatio, logR = 2.0 * n * logRatio;
                if (type == Option::Call)
                {
                    const Real d1 = d(-logHS + logR), d1_down = d(std::log(S / K_down) + logR);
                    const Real k1 = d(std::log(L / S) + logR - logRatio);
                    const Real k1_down = d(std::log(H / K_down) + logHS + logR);
                    barrierOut +=
                        df * S * detail::doubleBarrierWeightedDifference(f_, logW + logR, d1_down, d1) -
                        dd * K * detail::doubleBarrierWeightedDifference(f_, logW, d1_down - stdDev, d1 - stdDev) -
                        df * HHoverS *
                            detail::doubleBarrierWeightedDifference(f_, logW + logR + logHSWeight, k1_down, k1) +
                        dd * K *
                            detail::doubleBarrierWeightedDifference(f_, logW + logHSWeight, k1_down - stdDev,
                                                                    k1 - stdDev);
                }
                else
                {
                    const Real h1 = d(-logHS + logR - logRatio), d1_up = d(std::log(S / K_up) + logR);
                    const Real g1 = d(logHS + logR - logRatio);
                    const Real k1_up = d(std::log(H / K_up) + logHS + logR);
                    barrierOut +=
                        dd * K * detail::doubleBarrierWeightedDifference(f_, logW, h1 - stdDev, d1_up - stdDev) -
                        df * S * detail::doubleBarrierWeightedDifference(f_, logW + logR, h1, d1_up) -
                        dd * K *
                            detail::doubleBarrierWeightedDifference(f_, logW + logHSWeight, g1 - stdDev,
                                                                    k1_up - stdDev) +
                        df * HHoverS *
                            detail::doubleBarrierWeightedDifference(f_, logW + logR + logHSWeight, g1, k1_up);
                }
            }
        }
        barrierOut = std::max(0.0, barrierOut);
        const Real barrierIn = std::max(0.0, european - barrierOut);

        // The rebate, which the images leave out: paid at the first touch for a knock-out, and at expiry if neither
        // barrier is touched for a knock-in, as the lattice and Monte Carlo engines pay it.
        const Real rebate = arguments_.rebate == 0.0 ? 0.0 :
                                                       detail::doubleBarrierRebate(barrierType, arguments_.rebate, S, L,
                                                                                   H, rd, rd - rf, T, variance, dd);

        if (barrierType == DoubleBarrier::KnockOut)
            results_.value = barrierOut;
        else
            results_.value = barrierIn;
        if (rebate != 0.0)
            results_.value += rebate;
        results_.additionalResults["vanilla"] = european;
        results_.additionalResults["barrierOut"] = barrierOut;
        results_.additionalResults["barrierIn"] = barrierIn;
        results_.additionalResults["rebate"] = rebate;
    }


    Real SuoWangDoubleBarrierEngine::strike() const
    {
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        return payoff->strike();
    }

    Time SuoWangDoubleBarrierEngine::residualTime() const
    {
        return process_->time(arguments_.exercise->lastDate());
    }

    Volatility SuoWangDoubleBarrierEngine::volatility() const
    {
        return process_->blackVolatility()->blackVol(residualTime(), strike());
    }

    Rate SuoWangDoubleBarrierEngine::riskFreeRate() const
    {
        return process_->riskFreeRate()->zeroRate(residualTime(), Continuous, NoFrequency);
    }

    DiscountFactor SuoWangDoubleBarrierEngine::riskFreeDiscount() const
    {
        return process_->riskFreeRate()->discount(residualTime());
    }

    Rate SuoWangDoubleBarrierEngine::dividendYield() const
    {
        return process_->dividendYield()->zeroRate(residualTime(), Continuous, NoFrequency);
    }

    DiscountFactor SuoWangDoubleBarrierEngine::dividendDiscount() const
    {
        return process_->dividendYield()->discount(residualTime());
    }

}
