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
    /*! The engine values both legs from today (#228). Coupons not paid by
        the evaluation date count less the premium accrued before it, with
        protection from the later of their accrual start and today, and the
        premium accrued on a default counts from today too. On a discount
        curve dated after today, coupons paid on or before its reference
        date don't count, and protection starts on it. The upfront is taken
        as paid on the first coupon's accrual start, on the contract's
        notional. It counts only if that's on or after today (#227), and not
        at all if it falls before the discount curve's reference date
        (#206).

        The protection pays the claim on the name that is the nth to default,
        at the loss model's expected recovery for that name. Where the names
        left have different recoveries, that takes the model's probability of
        each name being the nth to default in each step,
        DefaultLossModel::probsBeingNthEventBetween. ConstantLossModel gives
        it where its integration integrates vectors, as GaussianQuadrature
        does and Trapezoid doesn't (#208), and RandomDefaultLM from its paths
        (#231). The other models don't, so the engine throws on them there.
        RandomLossLM gives no expected recovery, since it draws one with each
        default, so the engine throws on it whatever the recoveries (#234).
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
