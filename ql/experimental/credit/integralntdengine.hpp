/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Roland Lichters

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

#ifndef quantlib_integral_ntd_engine_hpp
#define quantlib_integral_ntd_engine_hpp

#include <ql/experimental/credit/nthtodefault.hpp>
#include <utility>

namespace QuantLib
{

    class YieldTermStructure;

    // Varying recoveries allowed, allow now for heterogeneous notionals
    //! Nth to default engine integrating the trigger probability over each coupon
    /*! Coupons paid after the discount curve's reference date count in
        full, with protection from the later of their accrual start and that
        date. The upfront is taken as paid on the first coupon's accrual
        start, on the contract's notional. It counts until the first coupon
        is paid, and not at all if it falls before the curve's reference
        date (#206).

        \warning Inside a coupon, on a discount curve as of today, the
            premium for the part of the coupon that has passed is in the
            premium leg, but its protection isn't, so the fair premium dips
            until the coupon is paid (#228).
        \warning On a discount curve dated on or before the first coupon's
            accrual start, the upfront still counts after that date, until
            the first coupon is paid, and then drops out with nothing paid
            (#227).
    */
    class IntegralNtdEngine : public NthToDefault::engine
    {
      public:
        IntegralNtdEngine(const Period& integrationStep, Handle<YieldTermStructure> discountCurve)
        : discountCurve_(std::move(discountCurve)), integrationStepSize_(integrationStep)
        {
        }
        void calculate() const override;

      protected:
        Handle<YieldTermStructure> discountCurve_;
        Period integrationStepSize_;
    };

}

#endif
