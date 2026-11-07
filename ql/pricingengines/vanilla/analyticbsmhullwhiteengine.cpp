/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
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

/*! \file analyticbsmhullwhiteengine.hpp
    \brief analytic Black-Scholes engines including stochastic interest rates
*/

#include <ql/math/expm1.hpp>
#include <ql/pricingengines/vanilla/analyticbsmhullwhiteengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <utility>

namespace QuantLib
{

    namespace
    {

        class ShiftedBlackVolTermStructure : public BlackVolTermStructure
        {
          public:
            ShiftedBlackVolTermStructure(Real varianceOffset, const Handle<BlackVolTermStructure>& volTS)
            : BlackVolTermStructure(volTS->referenceDate(), volTS->calendar(), Following, volTS->dayCounter()),
              varianceOffset_(varianceOffset), volTS_(volTS)
            {
            }

            Real minStrike() const override { return volTS_->minStrike(); }
            Real maxStrike() const override { return volTS_->maxStrike(); }
            Date maxDate() const override { return volTS_->maxDate(); }

          protected:
            Real blackVarianceImpl(Time t, Real strike) const override
            {
                return volTS_->blackVariance(t, strike, true) + varianceOffset_;
            }
            Volatility blackVolImpl(Time t, Real strike) const override
            {
                Time nonZeroMaturity = (t == 0.0 ? 0.00001 : t);
                Real var = blackVarianceImpl(nonZeroMaturity, strike);
                return std::sqrt(var / nonZeroMaturity);
            }

          private:
            const Real varianceOffset_;
            const Handle<BlackVolTermStructure> volTS_;
        };
    }

    AnalyticBSMHullWhiteEngine::AnalyticBSMHullWhiteEngine(Real equityShortRateCorrelation,
                                                           ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                                           const ext::shared_ptr<HullWhite>& model)
    : GenericModelEngine<HullWhite, VanillaOption::arguments, VanillaOption::results>(model),
      rho_(equityShortRateCorrelation), process_(std::move(process))
    {
        QL_REQUIRE(process_, "no Black-Scholes process specified");
        QL_REQUIRE(!model_.empty(), "no Hull-White model specified");
        registerWith(process_);
    }

    void AnalyticBSMHullWhiteEngine::calculate() const
    {

        QL_REQUIRE(process_->x0() > 0.0, "negative or null underlying given");

        const ext::shared_ptr<StrikedTypePayoff> payoff =
            ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-striked payoff given");

        const ext::shared_ptr<Exercise> exercise = arguments_.exercise;

        Time t = process_->riskFreeRate()->dayCounter().yearFraction(process_->riskFreeRate()->referenceDate(),
                                                                     exercise->lastDate());

        const Real a = model_->params()[0];
        const Real sigma = model_->params()[1];
        const Real eta = process_->blackVolatility()->blackVol(exercise->lastDate(), payoff->strike());

        // sigma^2 / a^2 (t + 2 / a exp(-a t) - 1 / (2 a) exp(-2 a t) - 3 / (2 a)) and
        // 2 rho sigma eta / a (t - (1 - exp(-a t)) / a) subtract terms of order one and divide what's left by up
        // to a^3.  With phi and h as in the Vasicek model, they're sigma^2 t^3 h(a t) / 2 and
        // 2 rho sigma eta t^2 phi(a t), which keep their digits, and their limits, at small a t.
        Real phi, h;
        detail::decayPhiAndH(a * t, phi, h);
        const Real varianceOffset = 0.5 * sigma * sigma * t * t * t * h + 2.0 * rho_ * sigma * eta * t * t * phi;

        Handle<BlackVolTermStructure> volTS(ext::shared_ptr<BlackVolTermStructure>(
            new ShiftedBlackVolTermStructure(varianceOffset, process_->blackVolatility())));

        ext::shared_ptr<GeneralizedBlackScholesProcess> adjProcess(new GeneralizedBlackScholesProcess(
            process_->stateVariable(), process_->dividendYield(), process_->riskFreeRate(), volTS));

        ext::shared_ptr<AnalyticEuropeanEngine> bsmEngine(new AnalyticEuropeanEngine(adjProcess));

        VanillaOption(payoff, exercise).setupArguments(bsmEngine->getArguments());
        bsmEngine->calculate();

        results_ = *dynamic_cast<const OneAssetOption::results*>(bsmEngine->getResults());
    }
}
