/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2011 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/exotic/analyticwriterextensibleoptionengine.hpp>
#include <utility>

using namespace std;

namespace QuantLib
{

    AnalyticWriterExtensibleOptionEngine::AnalyticWriterExtensibleOptionEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process))
    {
        registerWith(process_);
    }

    void AnalyticWriterExtensibleOptionEngine::calculate() const
    {
        // We take all the arguments:

        ext::shared_ptr<PlainVanillaPayoff> payoff1 = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff1, "not a plain vanilla payoff");

        ext::shared_ptr<PlainVanillaPayoff> payoff2 = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff2);
        QL_REQUIRE(payoff2, "not a plain vanilla payoff");

        ext::shared_ptr<Exercise> exercise1 = arguments_.exercise;

        ext::shared_ptr<Exercise> exercise2 = arguments_.exercise2;


        // We create and apply the calculate process:

        Option::Type type = payoff1->optionType();

        // STEP 1:

        // S = spot
        Real spot = process_->stateVariable()->value();

        // For the B&S formulae:
        const Date date1 = exercise1->lastDate();
        const Date date2 = exercise2->lastDate();
        const DiscountFactor discount1 = process_->riskFreeRate()->discount(date1);
        const DiscountFactor discount2 = process_->riskFreeRate()->discount(date2);
        const Real growth2 = process_->dividendYield()->discount(date2);
        const Real forward1 = spot * process_->dividendYield()->discount(date1) / discount1;
        const Real forward2 = spot * growth2 / discount2;
        const Real variance1 = process_->blackVolatility()->blackVariance(date1, payoff1->strike());
        const Real variance2 = process_->blackVolatility()->blackVariance(date2, payoff1->strike());

        // With no variance to speak of before the first expiry, the spot there is
        // known: it's either exercised then, or extended into an option with its forward.
        if (variance1 < QL_EPSILON * QL_EPSILON)
        {
            results_.value = (*payoff1)(forward1) > 0.0 ? Real(discount1 * (*payoff1)(forward1)) :
                                                          blackFormula(type, payoff2->strike(), forward2,
                                                                       std::sqrt(variance2 - variance1), discount2);
            return;
        }

        const Real stdDev1 = std::sqrt(variance1);
        const Real stdDev2 = std::sqrt(variance2);

        // Call the B&S method:
        Real black = blackFormula(type, payoff1->strike(), forward1, stdDev1, discount1);

        // STEP 2:

        // Standard bivariate normal distribution:
        Real ro = std::sqrt(variance1 / variance2);
        Real z1 = std::log(forward2 / payoff2->strike()) / stdDev2 + stdDev2 / 2;
        Real z2 = std::log(forward1 / payoff1->strike()) / stdDev1 + stdDev1 / 2;

        // Call the bivariate method:
        BivariateCumulativeNormalDistributionWe04DP biv(-ro);


        // STEP 3:

        Real bivariate1, bivariate2, result;

        // Final computing:
        if (type == Option::Call)
        {
            // Call case:
            bivariate1 = biv(z1, -z2);
            bivariate2 = biv(z1 - stdDev2, -z2 + stdDev1);
            result = black + spot * growth2 * bivariate1 - payoff2->strike() * discount2 * bivariate2;
        }
        else
        {
            // Put case:
            bivariate1 = biv(-z1, z2);
            bivariate2 = biv(-z1 + stdDev2, z2 - stdDev1);
            result = black - spot * growth2 * bivariate1 + payoff2->strike() * discount2 * bivariate2;
        }

        // Save the result:
        results_.value = result;
    }

}
