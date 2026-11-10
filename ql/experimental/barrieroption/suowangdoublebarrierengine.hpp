/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2013 Yue Tian

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

/*! \file suowangdoublebarrierengine.hpp
    \brief Wulin Suo, Yong Wang double-barrier option engine
*/

#ifndef wulin_yong_double_barrier_engine_hpp
#define wulin_yong_double_barrier_engine_hpp

#include <ql/instruments/doublebarrieroption.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantLib
{

    //! Pricing engine for barrier options using analytical formulae
    /*! The formulas are taken from "Barrier Option Pricing",
         Wulin Suo, Yong Wang.

         They sum images of the spot in the barriers, from -\p series to \p series - 1.  Where
         \f$ \sigma \sqrt{T} \f$ is above half of \f$ \ln(U/L) \f$, the engine sums the eigenfunction
         expansion of the density between the barriers instead, as AnalyticDoubleBarrierEngine does, so
         the paper's formulas are used only below that.  At zero volatility the underlying follows its
         forward.

         A rebate is paid at the first touch of either barrier for a knock-out, and at expiry if
         neither is touched for a knock-in, as BinomialDoubleBarrierEngine and MCDoubleBarrierEngine
         pay it.  Its value comes from AnalyticDoubleBarrierBinaryEngine's sums, and the additional
         result "rebate" is its part of the value.

        \ingroup barrierengines

        \test the correctness of the returned value is tested by
              reproducing results available in literature.
    */
    class SuoWangDoubleBarrierEngine : public DoubleBarrierOption::engine
    {
      public:
        explicit SuoWangDoubleBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process, int series = 5);
        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        const int series_;
        CumulativeNormalDistribution f_;
        // helper methods
        Real strike() const;
        Time residualTime() const;
        Volatility volatility() const;
        Rate riskFreeRate() const;
        DiscountFactor riskFreeDiscount() const;
        Rate dividendYield() const;
        DiscountFactor dividendDiscount() const;
    };

}


#endif
