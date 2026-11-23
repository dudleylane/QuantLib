/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2007 StatPro Italia srl

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
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <iterator>
#include <utility>

namespace QuantLib
{

    AnalyticEuropeanEngine::AnalyticEuropeanEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process))
    {
        registerWith(process_);
    }

    AnalyticEuropeanEngine::AnalyticEuropeanEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                                   Handle<YieldTermStructure> discountCurve)
    : process_(std::move(process)), discountCurve_(std::move(discountCurve))
    {
        registerWith(process_);
        registerWith(discountCurve_);
    }

    void AnalyticEuropeanEngine::reset()
    {
        // Keep the last calculation's additional results for calculate() to refill. The base class then empties the
        // results, additional ones included, as before (#88).
        if (!results_.additionalResults.empty())
            results_.additionalResults.swap(previousAdditionalResults_);
        VanillaOption::engine::reset();
    }

    void AnalyticEuropeanEngine::calculate() const
    {

        // if the discount curve is not specified, we default to the
        // risk free rate curve embedded within the GBM process
        ext::shared_ptr<YieldTermStructure> discountPtr =
            discountCurve_.empty() ? process_->riskFreeRate().currentLink() : discountCurve_.currentLink();

        QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not an European option");

        ext::shared_ptr<StrikedTypePayoff> payoff = ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-striked payoff given");

        Real variance = process_->blackVolatility()->blackVariance(arguments_.exercise->lastDate(), payoff->strike());
        DiscountFactor dividendDiscount = process_->dividendYield()->discount(arguments_.exercise->lastDate());
        DiscountFactor df = discountPtr->discount(arguments_.exercise->lastDate());
        DiscountFactor riskFreeDiscountForFwdEstimation =
            process_->riskFreeRate()->discount(arguments_.exercise->lastDate());
        Real spot = process_->stateVariable()->value();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");
        Real forwardPrice = spot * dividendDiscount / riskFreeDiscountForFwdEstimation;

        BlackCalculator black(payoff, forwardPrice, std::sqrt(variance), df);


        results_.value = black.value();
        results_.delta = black.delta(spot);
        results_.deltaForward = black.deltaForward();
        results_.elasticity = black.elasticity(spot);
        results_.gamma = black.gamma(spot);

        DayCounter rfdc = discountPtr->dayCounter();
        DayCounter divdc = process_->dividendYield()->dayCounter();
        DayCounter voldc = process_->blackVolatility()->dayCounter();
        Time t = rfdc.yearFraction(process_->riskFreeRate()->referenceDate(), arguments_.exercise->lastDate());
        results_.rho = black.rho(t);

        t = divdc.yearFraction(process_->dividendYield()->referenceDate(), arguments_.exercise->lastDate());
        results_.dividendRho = black.dividendRho(t);

        t = voldc.yearFraction(process_->blackVolatility()->referenceDate(), arguments_.exercise->lastDate());
        results_.vega = black.vega(t);
        try
        {
            results_.theta = black.theta(spot, t);
            results_.thetaPerDay = black.thetaPerDay(spot, t);
        }
        catch (Error&)
        {
            results_.theta = Null<Real>();
            results_.thetaPerDay = Null<Real>();
        }

        results_.strikeSensitivity = black.strikeSensitivity();
        results_.itmCashProbability = black.itmCashProbability();

        Real tte = process_->blackVolatility()->timeFromReference(arguments_.exercise->lastDate());
        // The additional results, in the order of the map's keys. Where reset() kept the last calculation's entries
        // and they have these keys, which the walk below checks in this order, write the values into them and hand
        // them back, which allocates nothing; otherwise insert them, as before (#88). Out of order, the walk would
        // never match: the values would still be right, but every calculation would insert them again.
        const std::pair<const char*, Real> additionalResults[] = {
            {"dividendDiscount", dividendDiscount},
            {"forward", forwardPrice},
            {"riskFreeDiscount", riskFreeDiscountForFwdEstimation},
            {"spot", spot},
            {"strike", payoff->strike()},
            {"timeToExpiry", tte},
            {"volatility", Real(std::sqrt(variance / tte))}};
        bool sameKeys =
            results_.additionalResults.empty() && previousAdditionalResults_.size() == std::size(additionalResults);
        auto entry = previousAdditionalResults_.begin();
        for (auto result = std::begin(additionalResults); sameKeys && result != std::end(additionalResults);
             ++result, ++entry)
        {
            if (entry->first != result->first)
                sameKeys = false;
            else if (auto* value = ext::any_cast<Real>(&entry->second))
                *value = result->second;
            else
                entry->second = result->second;
        }
        if (sameKeys)
        {
            results_.additionalResults.swap(previousAdditionalResults_);
        }
        else
        {
            for (const auto& [key, value] : additionalResults)
                results_.additionalResults[key] = value;
        }
    }

}
