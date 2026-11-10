/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Thema Consulting SA

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

/*! \file analyticdoublebarrierengine.hpp
    \brief Analytic double barrier european option engines
*/

#ifndef quantlib_analytic_double_barrier_engine_hpp
#define quantlib_analytic_double_barrier_engine_hpp

#include <ql/instruments/doublebarrieroption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantLib
{

    //! Pricing engine for double barrier european options using analytical formulae
    /*! The formulas are taken from "The complete guide to option pricing formulas 2nd Ed",
         E.G. Haug, McGraw-Hill, p.156 and following.
         Implements the Ikeda and Kunitomo series (see "Pricing Options with
         Curved Boundaries" Mathematical Finance 2/1992").
         This code handles only flat barriers

         The series sums images of the spot in the barriers, \p series of them on each side.  Where
         \f$ \sigma \sqrt{T} \f$ is above half of \f$ \ln(U/L) \f$, the engine sums the eigenfunction
         expansion of the density between the barriers instead, to convergence.  There the images would
         need more terms the wider the volatility is against the corridor, and they are of the order of
         the spot while the value falls far below it.  Below that, the default \p series leaves out
         terms below \f$ e^{-240} \f$ of the leading ones.

         A rebate is paid at the first touch of either barrier for a knock-out, and at expiry if
         neither is touched for a knock-in, as BinomialDoubleBarrierEngine and MCDoubleBarrierEngine
         pay it.  Its value comes from AnalyticDoubleBarrierBinaryEngine's sums.

        \ingroup barrierengines

        \test the correctness of the returned value is tested by
              reproducing results available in literature.
    */
    class AnalyticDoubleBarrierEngine : public DoubleBarrierOption::engine
    {
      public:
        explicit AnalyticDoubleBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process, int series = 5);
        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        CumulativeNormalDistribution f_;
        int series_;
        // helper methods
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
        Real vanillaEquivalent() const;
        Real callKO() const;
        Real putKO() const;
        Real callKI() const;
        Real putKI() const;
    };

    namespace detail
    {

        // The engine above and SuoWangDoubleBarrierEngine both use these, so they are declared here; file-local
        // copies would leave two to keep in step.  Their derivations are at their definitions, in
        // analyticdoublebarrierengine.cpp.

        // exp(logWeight) * (N(a) - N(b)), with N the cumulative normal f, keeping its digits where the weight
        // overflows and the difference underflows or cancels, as the images do at low volatility.
        Real doubleBarrierWeightedDifference(const CumulativeNormalDistribution& f, Real logWeight, Real a, Real b);

        // A double knock-out's value before discounting, by the eigenfunction expansion of the density between the
        // barriers, with muT = (r - q - vol^2 / 2) T and variance = vol^2 T.  It takes a few terms where
        // vol sqrt(T) is above half of ln(U/L), where the images would need more and cancel.  Some of the corridor
        // must pay: a call struck below the upper barrier, a put struck above the lower.
        Real doubleKnockOutByEigenfunctions(
            Option::Type type, Real spot, Real strike, Real lower, Real upper, Real muT, Real variance);

    }

}


#endif
