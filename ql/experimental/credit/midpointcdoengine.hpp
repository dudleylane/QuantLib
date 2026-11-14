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
    /*! The engine values both legs from today (#228). Coupons not paid by
        the evaluation date count less the premium accrued before it, and
        each coupon's losses are discounted from the middle of what's left
        of it after today, or after the discount curve's reference date if
        that's later. The protection starts from the expected tranche
        loss on the latest of today, the first unpaid coupon's accrual
        start and the reference dates of the default curves of the names
        left. So where those curves start on different dates, no protection
        counts before the latest of them. The upfront is taken as paid on
        the first coupon's accrual start, on the tranche's remaining
        notional. It counts only if that's on or after today (#227), and not
        at all if it falls before the discount curve's reference date
        (#206).

        \warning On default curves dated after today, the engine asks for
            the expected tranche loss at each coupon's accrual end, and so
            throws where a coupon still to be paid ends before the curves'
            reference date (#275).
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
