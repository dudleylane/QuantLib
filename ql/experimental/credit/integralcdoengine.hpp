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


#ifndef quantlib_integral_cdo_engine_hpp
#define quantlib_integral_cdo_engine_hpp

#include <ql/qldefines.hpp>

#ifndef QL_PATCH_SOLARIS

#    include <ql/experimental/credit/syntheticcdo.hpp>
#    include <utility>

namespace QuantLib
{

    class YieldTermStructure;

    //! CDO engine integrating the expected tranche loss over each coupon
    /*! The engine values both legs from today (#228). Coupons not paid by the
        evaluation date count less the premium accrued before it, and the
        protection starts from the expected tranche loss on the latest of
        today, the accrual start of the first coupon not yet paid or still
        accruing, and the reference dates of the default curves of the names
        left. Those curves don't reach back before their reference dates, so
        every expected tranche loss is taken on the later of its date and the
        latest of them (#275). Where they all start on the same date, that's
        the value on curves from today with no hazard until then. Where they
        start on different dates, no protection counts before the latest of
        them, and the defaults until then of the names whose curves start
        earlier count as before the protection starts (#331). A coupon paid
        before its accrual end, as under Preceding payments, has no premium
        left but still counts its protection to that end (#329). A premium
        paid after today but before the discount curve's reference date counts
        as paid on that date, as the protection over those days does (#328).
        The upfront is taken as paid on the first coupon's accrual start, on
        the tranche's remaining notional. It counts only if that's on or after
        today (#227), and not at all if it falls before the discount curve's
        reference date (#206): it's the one flow in that window left out.

        Each coupon accrues to its accrual end and is paid on its payment date
        (#276). So a coupon's last integration step ends at its accrual end,
        and each step's premium is discounted from the coupon's payment date,
        or from the discount curve's reference date if that's later (#328).
        Each step's losses are discounted from the step's end, or from the
        discount curve's reference date if that's later, as in
        MidPointCDOEngine.

        \warning Each step's defaults count as at its end: its losses are
            discounted from there, and its premium is paid on the notional
            left there. So the protection is low by about half a step's
            discounting, and the premium leaves out what the notional lost in
            the step accrues before its default. Both shrink with the step, so
            the value moves with it (#335).
    */
    class IntegralCDOEngine : public SyntheticCDO::engine
    {
      public:
        explicit IntegralCDOEngine(Handle<YieldTermStructure> discountCurve, Period stepSize = 3 * Months)
        : stepSize_(stepSize), discountCurve_(std::move(discountCurve))
        {
        }
        void calculate() const override;

      protected:
        Period stepSize_;
        Handle<YieldTermStructure> discountCurve_;
    };

}

#endif

#endif
