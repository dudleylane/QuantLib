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

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/experimental/credit/basket.hpp>
#include <ql/experimental/credit/integralntdengine.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <algorithm>

namespace QuantLib
{

    void IntegralNtdEngine::calculate() const
    {
        Date today = Settings::instance().evaluationDate();

        results_.errorEstimate = Null<Real>();
        results_.value = 0.0;
        results_.premiumValue = 0.0;
        results_.upfrontPremiumValue = 0.;
        Real accrualValue = 0.0;
        Real claimValue = 0.0;
        Date d0;
        const ext::shared_ptr<Basket>& basket = arguments_.basket;
        const Size n = arguments_.ntdOrder;

        for (auto& i : arguments_.premiumLeg)
        {
            ext::shared_ptr<FixedRateCoupon> coupon = ext::dynamic_pointer_cast<FixedRateCoupon>(i);
            Date d = i->date();
            /* Both legs run from today (#228): each coupon not paid by today
               counts less the premium accrued before today, and so does the
               premium accrued on a default, with protection from the latest
               of the coupon's accrual start, today and the discount curve's
               reference date. As before, a coupon paid on or before that
               reference date doesn't count, which leaves out only those paid
               before a discount curve dated after today. */
            if (!i->hasOccurred(today) && d > discountCurve_->referenceDate())
            {
                const Real accruedBeforeToday = coupon->accruedAmount(today);
                /*
                std::vector<Probability> probsTriggering =
                    arguments_.basket->probsBeingNthEvent(arguments_.ntdOrder,
                        d);
                Probability defaultProb =
                    std::accumulate(probsTriggering.begin(),
                    probsTriggering.end(), Real(0.));
                // OVERKILL???? 1-probAtLeastNEvents is enough

*/
                // prob of contract not having been triggered by date of payment
                Probability probNonTriggered = 1. - basket->probAtLeastNEvents(n, d);

                results_.premiumValue +=
                    (i->amount() - accruedBeforeToday) * discountCurve_->discount(d) * probNonTriggered;
                ////   * (1.0 - defaultProb);

                d = std::max({coupon->accrualStartDate(), today, discountCurve_->referenceDate()});

                // do steps of specified size
                d0 = d;
                Period stepSize = integrationStepSize_;
                /*
                                probsTriggering =
                                    arguments_.basket->probsBeingNthEvent(arguments_.ntdOrder,
                                    ///////REDUNDANT?
                                        d0);
                                Probability defProb0 = std::accumulate(probsTriggering.begin(),
                                ///OVERKILL????
                                    probsTriggering.end(), Real(0.));
                */
                Probability defProb0 = basket->probAtLeastNEvents(n, d0);
                // the names left at the evaluation date, by their place in the pool
                const std::vector<Size> alive = basket->liveList();
                do
                {
                    DiscountFactor disc = discountCurve_->discount(d);

                    Probability defProb1 = basket->probAtLeastNEvents(n, d);
                    Probability dcfdd = defProb1 - defProb0;

                    /* The protection pays the claim on whichever name is the
                    nth to default (#208), at the model's mean recovery for
                    that name given that it's the nth default in the step:
                    its expected recovery where the recoveries are fixed by
                    name, and the mean of the ones its triggers drew where
                    they're drawn (#250). Where the names left share it, it's
                    the claim at that recovery on the probability of the nth
                    default in the step; otherwise it takes each name's
                    probability of being the one. For a claim linear in the
                    recovery, as the library's are, that's its mean.
                    */
                    if (!alive.empty())
                    {
                        const std::vector<Real> recoveries = basket->recoveriesBeingNthEventBetween(n, d0, d);
                        const Real recovery = recoveries[alive[0]];
                        if (std::all_of(alive.begin() + 1, alive.end(),
                                        [&](Size iName) { return recoveries[iName] == recovery; }))
                        {
                            claimValue -= dcfdd * basket->claim()->amount(d, arguments_.notional, recovery) * disc;
                        }
                        else
                        {
                            const std::vector<Probability> probsTriggering =
                                basket->probsBeingNthEventBetween(n, d0, d);
                            for (Size iName : alive)
                                claimValue -= probsTriggering[iName] *
                                              basket->claim()->amount(d, arguments_.notional, recoveries[iName]) * disc;
                        }
                    }

                    defProb0 = defProb1;

                    if (arguments_.settlePremiumAccrual)
                        accrualValue += (coupon->accruedAmount(d) - accruedBeforeToday) * disc * dcfdd;

                    d0 = d;
                    d = d0 + stepSize;
                    // reduce step size ?
                    if (stepSize != 1 * Days && d > coupon->accrualEndDate())
                    {
                        stepSize = 1 * Days;
                        d = d0 + stepSize;
                    }
                } while (d <= coupon->accrualEndDate());
            }
        }

        // The upfront is taken as paid on the first coupon's accrual start, on
        //   the contract's notional, as the premium and the protection are. It
        //   counts only if that's on or after today, as the legs run from
        //   today (#227), and not at all if it falls before the discount
        //   curve's reference date, where CDO doesn't count it either (#206).
        const Date upfrontDate =
            ext::dynamic_pointer_cast<FixedRateCoupon>(arguments_.premiumLeg[0])->accrualStartDate();
        if (upfrontDate >= today && upfrontDate >= discountCurve_->referenceDate())
            results_.upfrontPremiumValue =
                arguments_.notional * arguments_.upfrontRate * discountCurve_->discount(upfrontDate);
        if (arguments_.side == Protection::Buyer)
        {
            results_.premiumValue *= -1;
            accrualValue *= -1;
            claimValue *= -1;
            results_.upfrontPremiumValue *= -1;
        }

        results_.value = results_.premiumValue + accrualValue + claimValue + results_.upfrontPremiumValue;

        // Where no premium is left to pay, as once the defaults so far reach
        //   the contract's rank, there's no fair premium.
        if (results_.premiumValue + accrualValue != 0.)
        {
            results_.fairPremium = -arguments_.premiumRate * claimValue / (results_.premiumValue + accrualValue);
            results_.additionalResults["fairPremium"] = results_.fairPremium;
        }
        // alternatively use results buffers and omit locals.
        results_.protectionValue = claimValue;

        results_.additionalResults["premiumLegNPV"] = Real(results_.premiumValue + results_.upfrontPremiumValue);
        results_.additionalResults["protectionLegNPV"] = results_.protectionValue;
    }

}
