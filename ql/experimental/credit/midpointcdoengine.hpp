/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2009, 2014 Jose Aparicio

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

#ifndef quantlib_midpoint_cdo_engine_hpp
#define quantlib_midpoint_cdo_engine_hpp

#include <ql/qldefines.hpp>

#ifndef QL_PATCH_SOLARIS

#    include <ql/experimental/credit/syntheticcdo.hpp>
#    include <utility>

namespace QuantLib
{

    class YieldTermStructure;

    //! CDO base engine taking schedule steps
    /*! Coupons not paid by the evaluation date count in full, and each
        coupon's losses are discounted from the middle of what's left of it
        after the discount curve's reference date. The protection starts
        from the expected tranche loss on the first coupon's accrual start
        while that coupon is unpaid, and from the losses realised so far
        once it's paid. The upfront is taken as paid on the first coupon's
        accrual start, on the tranche's remaining notional. It counts until
        the first coupon is paid, and not at all if it falls before the
        discount curve's reference date (#206).

        \warning Inside the first coupon, the protection over the part of it
            that has passed still counts, and on default curves dated after
            the coupon's accrual start the engine throws (#228).
        \warning On a discount curve dated on or before the first coupon's
            accrual start, the upfront still counts after that date, until
            the first coupon is paid, and then drops out with nothing paid
            (#227).
    */

    /* The engine obtains the cdo reference basket from its arguments and it
    is expecting it to have a default model assigned.
    */
    /* FIX ME: ASSUMES basket->expectedTrancheLoss(endDate) includes past
    realized losses (between cdo inception and calculation time) .... what if
    basket inception is not the same as CDO's ?????

    \todo JTD metrics might be invalid
    */
    class MidPointCDOEngine : public SyntheticCDO::engine
    {
      public:
        explicit MidPointCDOEngine(Handle<YieldTermStructure> discountCurve) : discountCurve_(std::move(discountCurve))
        {
        }
        void calculate() const override;

      protected:
        Handle<YieldTermStructure> discountCurve_;
    };

}

#endif

#endif
