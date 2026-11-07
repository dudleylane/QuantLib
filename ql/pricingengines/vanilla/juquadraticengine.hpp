/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004 Neil Firth
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

/*! \file juquadraticengine.hpp
    \brief Ju quadratic (1999) approximation engine
*/

#ifndef quantlib_ju_quadratic_engine_hpp
#define quantlib_ju_quadratic_engine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantLib
{

    //! Pricing engine for American options with Ju quadratic approximation
    /*! Reference:
        An Approximate Formula for Pricing American Options,
        Journal of Derivatives Winter 1999,
        Ju, N.

        \warning Barone-Adesi-Whaley critical commodity price
                 calculation is used, it has not been modified to see
                 whether the method of Ju is faster. Ju does not say
                 how he solves the equation for the critical stock
                 price, e.g. Newton method. He just gives the
                 solution.  The method of BAW gives answers to the
                 same accuracy as in Ju (1999).

        \warning This engine departs from the published formula in two
                 ways. The formula's correction divides by 1 - chi, and
                 chi can reach 1, mostly for puts with r near 0 and q < 0,
                 where the formula returns any number at all. So above
                 chi = 0.4, chi saturates smoothly at 0.6. That leaves the
                 value and delta continuous, and the gamma jumps where it
                 starts. And the value is floored at the best fixed
                 exercise, the largest of the European value, the
                 intrinsic value and the value of exercising at any fixed
                 time, which the correction could land below. Where the
                 floor binds, the delta and gamma are its own.

        \warning The value isn't reliable for low volatilities with long
                 expiries, where it can sit above the exact value and no
                 simple bound catches it. For a call with S = 500,
                 K = 100, r = 7%, q = 1%, 10 years to expiry and any
                 volatility up to 1%, it is about 407.0, and the exact
                 value about 405.2. At zero volatility the engine returns
                 the exact value, so it jumps there.
                 QdFpAmericanEngine is closer in that region, and over a
                 wide grid of inputs this engine was within 1.63 above and
                 0.56 below it, at S = 100.

        \ingroup vanillaengines

        \test the correctness of the returned value is tested by
              reproducing results available in literature.
    */
    class JuQuadraticApproximationEngine : public VanillaOption::engine
    {
      public:
        JuQuadraticApproximationEngine(ext::shared_ptr<GeneralizedBlackScholesProcess>);
        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    };

}


#endif
