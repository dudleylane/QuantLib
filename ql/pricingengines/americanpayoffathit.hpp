/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004 Ferdinando Ametrano

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

/*! \file americanpayoffathit.hpp
    \brief Analytical formulae for american exercise with payoff at hit
*/

#ifndef quantlib_americanpayoffathit_h
#define quantlib_americanpayoffathit_h

#include <ql/instruments/payoffs.hpp>

namespace QuantLib
{

    namespace detail
    {

        // What 1 paid when the underlying first reaches a barrier is worth, if that happens by
        // expiry, and its derivatives.  Time is measured by the variance s that has elapsed: the
        // log of the underlying moves by mu s plus a standard Brownian motion in s, the payment
        // is discounted at rho per unit of s, and the barrier is at a log distance b from the
        // spot, to be reached by s = variance.  AmericanPayoffAtHit and AnalyticBarrierEngine's
        // rebate have this in closed form in lambda = sqrt(mu^2 + 2 rho), which takes N at
        // complex arguments where a negative rate makes mu^2 + 2 rho negative.  This integrates
        // the first-passage density instead, which holds at any sign, and both use it there.
        struct UnitPayoffAtHitResults
        {
            Real value;
            // with respect to b, mu and rho
            Real dValueDb, d2ValueDb2, dValueDMu, dValueDRho;
        };

        // Without derivatives, they are left null, and the value takes half the time.
        UnitPayoffAtHitResults unitPayoffAtHit(Real b, Real mu, Real rho, Real variance, bool derivatives = true);

    }

    //! Analytic formula for American exercise payoff at-hit options
    /*! \todo calculate greeks */
    class AmericanPayoffAtHit
    {
      public:
        AmericanPayoffAtHit(Real spot,
                            DiscountFactor discount,
                            DiscountFactor dividendDiscount,
                            Real variance,
                            const ext::shared_ptr<StrikedTypePayoff>& payoff);
        Real value() const;
        Real delta() const;
        Real gamma() const;
        Real rho(Time maturity) const;

      private:
        Real spot_;
        DiscountFactor discount_, dividendDiscount_;
        Real variance_;
        Volatility stdDev_;

        Real strike_, K_, DKDstrike_;

        Real mu_, lambda_, muPlusLambda_, muMinusLambda_, log_H_S_;

        Real D1_, D2_, cum_d1_, cum_d2_;

        Real alpha_, beta_, DalphaDd1_, DbetaDd2_;

        bool inTheMoney_;
        Real forward_, X_, DXDstrike_;

        // where the value and its derivatives come from detail::unitPayoffAtHit
        bool integrated_ = false;
        detail::UnitPayoffAtHitResults atHit_ = {};
    };


    // inline definitions

    inline Real AmericanPayoffAtHit::value() const
    {
        return K_ * (forward_ * alpha_ + X_ * beta_);
    }

}


#endif
