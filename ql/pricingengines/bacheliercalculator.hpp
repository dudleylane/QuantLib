/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003, 2004, 2005, 2006 Ferdinando Ametrano
 Copyright (C) 2006 StatPro Italia srl

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

/*! \file bacheliercalculator.hpp
    \brief Bachelier-formula calculator class
*/

#ifndef quantlib_bacheliercalculator_hpp
#define quantlib_bacheliercalculator_hpp

#include <ql/instruments/payoffs.hpp>

namespace QuantLib
{

    //! Bachelier calculator class
    /*! Values plain-vanilla, cash-or-nothing, asset-or-nothing and gap payoffs on a forward that is normal at
        expiry, with standard deviation \p stdDev.

        At zero standard deviation, the Greeks of a payoff that jumps at the strike leave the jump out.  With
        the forward at the strike, its term in the delta would be infinite.
    */
    class BachelierCalculator
    {
      private:
        class Calculator;

      public:
        BachelierCalculator(const ext::shared_ptr<StrikedTypePayoff>& payoff,
                            Real forward,
                            Real stdDev,
                            Real discount = 1.0);
        BachelierCalculator(Option::Type optionType, Real strike, Real forward, Real stdDev, Real discount = 1.0);
        ~BachelierCalculator() = default;

        Real value() const;

        /*! Sensitivity to change in the underlying forward price. */
        Real deltaForward() const;
        /*! Sensitivity to change in the underlying spot price. */
        Real delta(Real spot) const;

        /*! Sensitivity in percent to a percent change in the
            underlying forward price. */
        Real elasticityForward() const;
        /*! Sensitivity in percent to a percent change in the
            underlying spot price. */
        Real elasticity(Real spot) const;

        /*! Second order derivative with respect to change in the
            underlying forward price. */
        Real gammaForward() const;
        /*! Second order derivative with respect to change in the
            underlying spot price. */
        Real gamma(Real spot) const;

        /*! Sensitivity to time to maturity. */
        Real theta(Real spot, Time maturity) const;
        /*! Sensitivity to time to maturity per day,
            assuming 365 day per year. */
        Real thetaPerDay(Real spot, Time maturity) const;

        /*! Sensitivity to volatility. */
        Real vega(Time maturity) const;

        /*! Sensitivity to discounting rate. */
        Real rho(Time maturity) const;

        /*! Sensitivity to dividend/growth rate. */
        Real dividendRho(Time maturity) const;

        /*! Probability of being in the money in the bond martingale
            measure, i.e. N(d).
            It is a risk-neutral probability, not the real world one.
        */
        Real itmCashProbability() const;

        /*! The asset-or-nothing option's value over the discounted
            forward, E[F_T 1{in the money}] / F: N(d) + s n(d) / F for a
            call and N(-d) - s n(d) / F for a put, s being the standard
            deviation.  For a call, that is what BlackCalculator's gives,
            where it is the probability of being in the money in the asset
            martingale measure; BlackCalculator's gives a put the call's
            (#235).  A normal forward can be negative, so there is no such
            measure here, and this isn't a probability: it can lie outside
            [0, 1].  The forward can't be 0 (#195).
        */
        Real itmAssetProbability() const;

        /*! Sensitivity to strike. */
        Real strikeSensitivity() const;

        /*! gamma w.r.t. strike. */
        Real strikeGamma() const;

        /*! Sensitivity of vega to the forward (Vanna) */
        Real vanna(Time maturity) const;

        /*! Sensitivity of vega to volatility (Volga) */
        Real volga(Time maturity) const;

        Real alpha() const;
        Real beta() const;

      protected:
        void initialize(const ext::shared_ptr<StrikedTypePayoff>& p);

        //! Member variables
        Option::Type type_;
        Real strike_, forward_, stdDev_, discount_, variance_;
        Real d_;                                 // Single d parameter for Bachelier model
        Real alpha_, beta_, DalphaDd_, DbetaDd_; // Simplified derivative names
        Real n_d_, cum_d_;                       // Single normal distribution values
        Real x_, DxDs_, DxDstrike_;
        // In the money the payoff is slope_ * F_T + intercept_, and jump_ is what it pays at the strike.
        Real slope_, intercept_, DinterceptDstrike_, jump_;
        bool vanilla_;
    };

    // inline
    inline Real BachelierCalculator::thetaPerDay(Real spot, Time maturity) const
    {
        return theta(spot, maturity) / 365.0;
    }

    inline Real BachelierCalculator::itmCashProbability() const
    {
        // For Bachelier model:
        // Call ITM probability: P(F > K) = N(d) where d = (F-K)/σ
        // Put ITM probability:  P(F < K) = N(-d) = 1 - N(d) where d = (F-K)/σ

        if (type_ == Option::Call)
        {
            return cum_d_; // N(d)
        }
        else
        {
            return 1.0 - cum_d_; // N(-d) = 1 - N(d)
        }
    }

    inline Real BachelierCalculator::itmAssetProbability() const
    {
        // E[F_T 1{w F_T > w K}] / F, with w = 1 for a call and -1 for a put: an asset-or-nothing
        // option's value, D [F N(w d) + w s n(d)] as value() takes it, over D F (#195).
        QL_REQUIRE(forward_ != 0.0, "the asset probability isn't defined at a forward of 0");
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        const Real timeValue = (stdDev_ > QL_EPSILON) ? stdDev_ * n_d_ : 0.0;
        return itmCashProbability() + w * timeValue / forward_;
    }

    inline Real BachelierCalculator::alpha() const
    {
        return alpha_;
    }

    inline Real BachelierCalculator::beta() const
    {
        return beta_;
    }

}

#endif
