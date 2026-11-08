/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 Lew Wei Hao

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

#ifndef quantlib_analytic_black_vasicek_engine_hpp
#define quantlib_analytic_black_vasicek_engine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/models/shortrate/onefactormodels/vasicek.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantLib
{

    /**
     *
     * Pricing of Vanilla European options under stochastic Vasicek interest rate model
     * Analytical solution is based on following research paper:
     *
     * http://hsrm-mathematik.de/WS201516/master/option-pricing/Black-Scholes-Vasicek-Model.pdf
     *
     * The paper's volatility is constant. A term structure enters through its variance to
     * the maturity and, with a correlation, through its forward variance over each day up
     * to the maturity, as the volatility's instantaneous value over that day. That's exact
     * for a term structure linear in variance between dates, as BlackVarianceCurve and
     * BlackVarianceSurface are by default, and its cost grows with the days to maturity
     * unless the volatility is a BlackConstantVol on the rate curve's day counter and
     * reference date. Where the variance is smooth between dates instead, the day's average
     * leaves an error, up to 8.5e-7 of the value on a cubic variance curve with a steep
     * short end. A variance that falls between two dates has no instantaneous volatility,
     * and with a correlation the engine refuses it.
     */

    class AnalyticBlackVasicekEngine : public VanillaOption::engine
    {
      public:
        AnalyticBlackVasicekEngine(ext::shared_ptr<GeneralizedBlackScholesProcess>,
                                   ext::shared_ptr<Vasicek>,
                                   Real correlation);
        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> blackProcess_;
        ext::shared_ptr<Vasicek> vasicekProcess_;
        Real correlation_;
    };

}

#endif
