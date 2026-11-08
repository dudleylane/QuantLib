/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2025 William Day

 This file is part of QuantLib, a free-software/open-source librarys
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

/*! \file analyticsoftbarrierengine.hpp
    \brief Analytic soft barrier european option pricing engine
*/

#ifndef quantlib_analytic_soft_barrier_engine_hpp
#define quantlib_analytic_soft_barrier_engine_hpp

#include <ql/instruments/softbarrieroption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantLib
{

    //! Pricing engine for soft barrier european options using an analytical formula
    /*! A soft barrier option, introduced by Hart and Ross (1994), knocks in or out in proportion to how far into the
        range [L, U] the underlying goes: its knock-in value is the average, over barriers H from L to U, of
        single-barrier knock-ins ("The complete guide to option pricing formulas 2nd Ed", E.G. Haug, p.165).  This
        engine integrates Reiner and Rubinstein's single-barrier formulas over that range in closed form, taking the
        vanilla where H is past the spot and their C or A - B + D on either side of the strike.  Haug's formula
        integrates C alone, so it agrees with this engine only where the whole range is at or short of both the spot
        and the strike.

        The formulas cover down-and-in and down-and-out calls, and up-and-in and up-and-out puts.  Other
        combinations of barrier and option type raise an error, except with U = L, where the option is a
        single-barrier option and is priced as one.

        Below a variance of QL_EPSILON the underlying is taken to follow its forward, as in AnalyticBarrierEngine.
        At the money that moves the value by up to about 0.4 F sqrt(QL_EPSILON), with F = S exp(-qT).
    */

    class AnalyticSoftBarrierEngine : public SoftBarrierOption::engine
    {
      public:
        explicit AnalyticSoftBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process);

        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;

        void validateInputs(Real S,
                            Real X,
                            Rate r,
                            Rate q,
                            Time T,
                            Real U,
                            Real L,
                            Option::Type optionType,
                            Barrier::Type barrierType,
                            Real sigma) const;


        // Helper methods to extract parameters
        Real underlying() const;
        Real strike() const;
        Time residualTime() const;
        Volatility volatility() const;
        Real volatilitySquared() const;
        Real barrierLo() const;
        Real barrierHi() const;
        Real stdDeviation() const;
        Rate riskFreeRate() const;
        DiscountFactor riskFreeDiscount() const;
        Rate dividendYield() const;
        Rate costOfCarry() const;
        DiscountFactor dividendDiscount() const;

        // Pricing helpers
        Real vanillaEquivalent() const; // Computes the equivalent vanilla  option price (with no barriers)
    };

}

#endif