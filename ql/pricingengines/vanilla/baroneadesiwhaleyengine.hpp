/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003, 2004 Ferdinando Ametrano
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

/*! \file baroneadesiwhaleyengine.hpp
    \brief Barone-Adesi and Whaley approximation engine
*/

#ifndef quantlib_barone_adesi_whaley_engine_hpp
#define quantlib_barone_adesi_whaley_engine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantLib
{

    class BlackCalculator;

    namespace detail
    {

        // The value of exercising at the best fixed time, and its delta and
        // gamma, under the flat rates the engine below,
        // JuQuadraticApproximationEngine and
        // BjerksundStenslandApproximationEngine assume. It is the American
        // value at zero variance and a lower bound on it at any other. All
        // three use it, so it is declared here; file-local copies would clash
        // in a unity build, which compiles the three files as one. The
        // derivation is at its definition, in baroneadesiwhaleyengine.cpp.
        struct FixedExerciseResults
        {
            Real value, delta, gamma;
            // when to exercise, as a fraction of the time to expiry
            Real exerciseFraction;
        };

        FixedExerciseResults bestFixedExercise(const StrikedTypePayoff& payoff,
                                               Real spot,
                                               DiscountFactor dividendDiscount,
                                               DiscountFactor riskFreeDiscount,
                                               const BlackCalculator& black);

        // The critical price that
        // BaroneAdesiWhaleyApproximationEngine::criticalPrice() returned
        // last, with the inputs it returned it for. The engine below and
        // JuQuadraticApproximationEngine each hold one. None of the inputs is
        // the spot, so pricing again after the spot moves reuses it instead
        // of running the Newton solve again, and any other inputs solve
        // afresh. Inputs match only when their bits do, so the result is what
        // criticalPrice() returns for the same inputs, whatever was solved
        // before. The entry changes only once a solve has returned, so a
        // solve that throws leaves the previous one. It is declared here for
        // the same reason as bestFixedExercise().
        class CriticalPriceCache
        {
          public:
            Real operator()(const ext::shared_ptr<StrikedTypePayoff>& payoff,
                            DiscountFactor riskFreeDiscount,
                            DiscountFactor dividendDiscount,
                            Real variance,
                            Real tolerance);

          private:
            bool valid_ = false;
            Option::Type type_ = Option::Call;
            Real strike_ = 0.0, riskFreeDiscount_ = 0.0, dividendDiscount_ = 0.0, variance_ = 0.0, tolerance_ = 0.0;
            Real criticalPrice_ = 0.0;
        };

    }

    //! Barone-Adesi and Whaley pricing engine for American options (1987)
    /*! \ingroup vanillaengines

        The engine keeps the last critical price it solved for, with the
        option type, strike, discount factors to expiry, variance and
        tolerance it solved it for. None of them is the spot, so pricing
        again after the spot moves reuses it, and any other change solves
        afresh; either way the result is the one an engine that had never
        priced would return, to the bit. One engine shared by options of
        different strikes or types solves on every price.

        \warning The value isn't reliable for low volatilities with long
                 expiries, where it can sit above the exact value and no
                 simple bound catches it. For a call with S = 500,
                 K = 100, r = 7%, q = 1%, 10 years to expiry and any
                 volatility up to 1%, it is about 411.8, and the exact
                 value about 405.2. At zero volatility the engine returns
                 the exact value, so it jumps there.
                 QdFpAmericanEngine is closer in that region, and over a
                 wide grid of inputs this engine was within 5.25 above and
                 1.90 below it, at S = 100.

        \test the correctness of the returned value is tested by
              reproducing results available in literature.
    */
    class BaroneAdesiWhaleyApproximationEngine : public VanillaOption::engine
    {
      public:
        BaroneAdesiWhaleyApproximationEngine(ext::shared_ptr<GeneralizedBlackScholesProcess>);
        static Real criticalPrice(const ext::shared_ptr<StrikedTypePayoff>& payoff,
                                  DiscountFactor riskFreeDiscount,
                                  DiscountFactor dividendDiscount,
                                  Real variance,
                                  Real tolerance = 1e-6);
        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        mutable detail::CriticalPriceCache criticalPriceCache_;
    };

}


#endif
