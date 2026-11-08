/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 Lew Wei Hao

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
#include <ql/math/expm1.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanvasicekengine.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <algorithm>
#include <cmath>
#include <utility>

namespace QuantLib
{

    AnalyticBlackVasicekEngine::AnalyticBlackVasicekEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> blackProcess,
                                                           ext::shared_ptr<Vasicek> vasicekProcess,
                                                           Real correlation)
    : blackProcess_(std::move(blackProcess)), vasicekProcess_(std::move(vasicekProcess)), correlation_(correlation)
    {
        registerWith(blackProcess_);
        registerWith(vasicekProcess_);
    }

    void AnalyticBlackVasicekEngine::calculate() const
    {
        QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not an European option");

        ext::shared_ptr<StrikedTypePayoff> payoff = ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);

        QL_REQUIRE(payoff, "non-striked payoff given");

        const Handle<YieldTermStructure>& riskFreeRate = blackProcess_->riskFreeRate();
        const Handle<BlackVolTermStructure>& volatility = blackProcess_->blackVolatility();
        DayCounter dc = riskFreeRate->dayCounter();
        Date today = riskFreeRate->referenceDate();
        Date maturity = arguments_.exercise->lastDate();
        Real T = dc.yearFraction(today, maturity);
        Real kappa = vasicekProcess_->a();
        Real sigma_r = vasicekProcess_->sigma();
        Real r_t = vasicekProcess_->r0();
        // Net of the dividends paid until maturity.
        Real S_t = blackProcess_->x0() * blackProcess_->dividendYield()->discount(maturity);
        Real K = payoff->strike();

        // The forward's variance is the integral over [0, T] of
        //     sigma_s(u)^2 + 2 rho sigma_s(u) sigma_r g(T - u) + sigma_r^2 g(T - u)^2,
        // with g(x) = (1 - exp(-a x)) / a. The stock's part is the term structure's variance, so that it's the term
        // structure's on any day counter, and the rate's is sigma_r^2 T^3 h(a T) / 2, with phi and h as in the
        // Vasicek model.
        Real stockVariance = volatility->blackVariance(maturity, K);
        Real phi, h;
        detail::decayPhiAndH(kappa * T, phi, h);
        Real variance = stockVariance + 0.5 * sigma_r * sigma_r * T * T * T * h;

        if (correlation_ != 0.0 && sigma_r != 0.0)
        {
            Real cross;
            auto flat = ext::dynamic_pointer_cast<BlackConstantVol>(volatility.currentLink());
            if (flat && flat->dayCounter() == dc && flat->referenceDate() == today)
            {
                // A constant volatility on the rate's clock, and the integral of g up to T is T^2 phi(a T).
                cross = std::sqrt(stockVariance * T) * T * phi;
            }
            else
            {
                // Otherwise the volatility over each day is the square root of the day's forward variance per unit
                // of the rate's time, which is exact where the variance is linear between dates. Over a day ending
                // at x = T - t1 before the maturity, the integral of g(T - u) is dt (g(x) + exp(-a x) dt phi(a dt)),
                // whose terms don't cancel.
                DayCounter volatilityDayCounter = volatility->dayCounter();
                Date volatilityReferenceDate = volatility->referenceDate();
                // The maturity's variance checked the range and the strike, so the dates before it skip the checks.
                auto varianceTo = [&](const Date& d)
                {
                    return d <= volatilityReferenceDate ?
                               Real(0.0) :
                               volatility->blackVariance(volatilityDayCounter.yearFraction(volatilityReferenceDate, d),
                                                         K, true);
                };
                cross = 0.0;
                Date d0 = today;
                Real t0 = 0.0, v0 = varianceTo(today);
                while (d0 < maturity)
                {
                    Date d1 = std::min(d0 + 1, maturity);
                    Real t1 = dc.yearFraction(today, d1), v1 = varianceTo(d1);
                    Real dt = t1 - t0, dv = v1 - v0;
                    // A variance that falls has no instantaneous volatility.
                    QL_REQUIRE(dv >= 0.0, "decreasing variance at strike " << K << " between " << d0 << " and " << d1);
                    Real dayPhi, dayH;
                    detail::decayPhiAndH(kappa * dt, dayPhi, dayH);
                    Real e = std::expm1(-kappa * (T - t1));
                    cross += std::sqrt(dv * dt) * (-e / kappa + (1.0 + e) * dt * dayPhi);
                    d0 = d1;
                    t0 = t1;
                    v0 = v1;
                }
            }
            variance += 2.0 * correlation_ * sigma_r * cross;
        }

        Real zcb = vasicekProcess_->discountBond(0.0, T, r_t);

        // Under the measure with the bond as numeraire, the forward is lognormal with that variance.
        results_.value = BlackCalculator(payoff, S_t / zcb, std::sqrt(variance), zcb).value();
    }

}
