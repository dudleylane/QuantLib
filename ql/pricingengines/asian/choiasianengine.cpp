/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 Klaus Spanderen

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
#include <ql/instruments/basketoption.hpp>
#include <ql/math/comparison.hpp>
#include <ql/pricingengines/asian/choiasianengine.hpp>
#include <ql/pricingengines/basket/choibasketengine.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

namespace QuantLib
{

    ChoiAsianEngine::ChoiAsianEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                     Real lambda,
                                     Size maxNrIntegrationSteps)
    : process_(std::move(process)), lambda_(lambda), maxNrIntegrationSteps_(maxNrIntegrationSteps)
    {
        registerWith(process_);
    }

    void ChoiAsianEngine::calculate() const
    {
        QL_REQUIRE(arguments_.averageType == Average::Type::Arithmetic, "must be Average::Type Arithmetic ");

        QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not a European Option");

        const ext::shared_ptr<PlainVanillaPayoff> payoff =
            ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non plain vanilla payoff given");

        std::vector<Date> fixingDates = arguments_.fixingDates;
        std::sort(fixingDates.begin(), fixingDates.end());

        Size futureFixings = fixingDates.size();
        Size pastFixings = arguments_.pastFixings;
        Real runningAccumulator = arguments_.runningAccumulator;

        const Date exerciseDate = arguments_.exercise->lastDate();
        const Handle<YieldTermStructure> rTS = process_->riskFreeRate();
        const DiscountFactor discount = rTS->discount(exerciseDate);

        // A fixing today is known, but it's the spot itself: it still moves with it.
        Real spotInAccrued = 0.0;

        if (futureFixings > 0 && process_->time(fixingDates.front()) == Time(0))
        {
            // push today fixing to past fixings
            fixingDates.erase(fixingDates.begin());
            futureFixings--;
            pastFixings++;
            runningAccumulator += process_->x0();
            spotInAccrued = process_->x0() / (pastFixings + futureFixings);
        }

        // With the average certain, or certain to pay off, only the discounting is left, and
        // delta comes from the part of the average that moves with the spot.  At the strike
        // it takes the half that BlackCalculator gives there.
        const auto setResultsForKnownAverage = [&](Real average, Real movingWithSpot)
        {
            const Real sign = Real(payoff->optionType());
            const Real k = payoff->strike();
            const Real side = close(average, k) ? 0.5 * sign : (sign * (average - k) > 0.0 ? sign : 0.0);
            results_.value = discount * (*payoff)(average);
            results_.delta = side * discount * movingWithSpot / process_->x0();
            results_.gamma = 0.0;
        };

        if (futureFixings == 0)
        {
            QL_REQUIRE(pastFixings > 0, "no past fixings given");
            setResultsForKnownAverage(runningAccumulator / pastFixings, spotInAccrued);

            return;
        }

        QL_REQUIRE(fixingDates.back() <= exerciseDate, "last fixing date must be before exercise date");
        QL_REQUIRE(process_->time(fixingDates.front()) >= 0.0, "first fixing date is in the past");

        QL_REQUIRE(std::adjacent_find(fixingDates.begin(), fixingDates.end()) == fixingDates.end(),
                   "two fixing dates are the same");

        const Real accruedAverage = (pastFixings != 0) ? Real(runningAccumulator / (pastFixings + futureFixings)) : 0.0;

        const Real strike = payoff->strike() - accruedAverage;

        const Handle<YieldTermStructure> qTS = process_->dividendYield();
        const Handle<BlackVolTermStructure> volTS = process_->blackVolatility();
        const Date volRefDate = volTS->referenceDate();
        const DayCounter volDc = volTS->dayCounter();
        const Real weight = 1.0 / (pastFixings + futureFixings);

        // A fixing with no variance is its forward, which comes off the strike: in the basket,
        // its correlations would be 0/0.  Below QL_EPSILON, a standard deviation is worth less
        // than the rounding of the forward.  With the strike already covered nothing else
        // matters, since no fixing can go negative.
        Real deterministicForwards = 0.0;
        Real stochasticForwards = 0.0;
        std::vector<Date> stochasticDates;
        std::vector<Time> fixingTimes;
        std::vector<Real> variances;
        for (const Date& fixingDate : fixingDates)
        {
            const Real forward = weight * process_->x0() * qTS->discount(fixingDate) / rTS->discount(fixingDate);
            const Real variance = strike > 0.0 ? Real(volTS->blackVariance(fixingDate, strike)) : 0.0;
            if (variance < QL_EPSILON * QL_EPSILON)
            {
                deterministicForwards += forward;
            }
            else
            {
                stochasticForwards += forward;
                stochasticDates.push_back(fixingDate);
                fixingTimes.push_back(volDc.yearFraction(volRefDate, fixingDate));
                variances.push_back(variance);
            }
        }

        const Real residualStrike = strike - deterministicForwards;
        if (variances.empty() || residualStrike <= 0.0)
        {
            const Real forwards = deterministicForwards + stochasticForwards;
            setResultsForKnownAverage(accruedAverage + forwards, forwards + spotInAccrued);

            return;
        }

        if (variances.size() > 1)
        {
            Matrix rho(variances.size(), variances.size());
            for (Size i = 0; i < rho.rows(); ++i)
                for (Size j = i; j < rho.columns(); ++j)
                    rho[i][j] = rho[j][i] = variances[std::min(i, j)] / std::sqrt(variances[i] * variances[j]);

            const Handle<YieldTermStructure> zeroTS(
                ext::make_shared<FlatForward>(rTS->referenceDate(), 0.0, rTS->dayCounter()));

            std::vector<ext::shared_ptr<GeneralizedBlackScholesProcess>> processes;
            processes.reserve(variances.size());
            for (Size i = 0; i < variances.size(); ++i)
            {
                const Date& fixingDate = stochasticDates[i];
                const Volatility sig =
                    volTS->blackVol(fixingDate, payoff->strike()) * std::sqrt(fixingTimes[i] / fixingTimes.back());

                processes.emplace_back(ext::make_shared<GeneralizedBlackScholesProcess>(
                    Handle<Quote>(ext::make_shared<SimpleQuote>(process_->x0() * qTS->discount(fixingDate) /
                                                                rTS->discount(fixingDate))),
                    zeroTS, zeroTS,
                    Handle<BlackVolTermStructure>(ext::make_shared<BlackConstantVol>(
                        volRefDate, volTS->calendar(), Handle<Quote>(ext::make_shared<SimpleQuote>(sig)), volDc))));
            }

            BasketOption basketOption(ext::make_shared<AverageBasketPayoff>(
                                          ext::make_shared<PlainVanillaPayoff>(payoff->optionType(), residualStrike),
                                          Array(variances.size(), weight)),
                                      ext::make_shared<EuropeanExercise>(stochasticDates.back()));
            basketOption.setPricingEngine(
                ext::make_shared<ChoiBasketEngine>(processes, rho, lambda_, maxNrIntegrationSteps_));

            results_.value = basketOption.NPV() * discount;
        }
        else
        {
            results_.value = blackFormula(payoff->optionType(), residualStrike, stochasticForwards,
                                          std::sqrt(variances.back()), discount);
        }
    }
}
