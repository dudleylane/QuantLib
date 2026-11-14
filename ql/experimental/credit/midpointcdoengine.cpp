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

#include <ql/experimental/credit/midpointcdoengine.hpp>

#ifndef QL_PATCH_SOLARIS

#    include <ql/cashflows/fixedratecoupon.hpp>
#    include <ql/termstructures/yieldtermstructure.hpp>
#    include <algorithm>

namespace QuantLib
{

    void MidPointCDOEngine::calculate() const
    {
        Date today = Settings::instance().evaluationDate();

        results_.premiumValue = 0.0;
        results_.protectionValue = 0.0;
        results_.upfrontPremiumValue = 0.0;
        results_.error = 0;
        results_.expectedTrancheLoss.clear();
        results_.xMin = arguments_.basket->attachmentAmount();
        results_.xMax = arguments_.basket->detachmentAmount();
        results_.remainingNotional = arguments_.basket->remainingTrancheNotional();
        const Real inceptionTrancheNotional = arguments_.basket->trancheNotional();

        // Both legs run from today (#228). The protection starts from the
        //   expected tranche loss on the latest of today, the first unpaid
        //   coupon's accrual start and the reference dates of the default
        //   curves of the names left: what's lost already is in every later
        //   expected loss, and is no flow to come, and the curves don't
        //   reach back before their reference dates.
        Real e1 = inceptionTrancheNotional - results_.remainingNotional;
        // todo add includeSettlement date flows variable to engine.
        const auto firstUnpaid =
            std::find_if(arguments_.normalizedLeg.begin(), arguments_.normalizedLeg.end(),
                         [&](const ext::shared_ptr<CashFlow>& cf) { return !cf->hasOccurred(today); });
        if (firstUnpaid != arguments_.normalizedLeg.end())
        {
            Date start = std::max(ext::dynamic_pointer_cast<Coupon>(*firstUnpaid)->accrualStartDate(), today);
            const ext::shared_ptr<Pool>& pool = arguments_.basket->pool();
            for (Size i : arguments_.basket->liveList())
                start = std::max(
                    start, pool->get(pool->names()[i]).defaultProbability(pool->defaultKeys()[i])->referenceDate());
            e1 = arguments_.basket->expectedTrancheLoss(start);
        }
        results_.expectedTrancheLoss.push_back(e1);
        for (auto& i : arguments_.normalizedLeg)
        {
            if (i->hasOccurred(today))
            {
                results_.expectedTrancheLoss.push_back(0.);
                continue;
            }
            ext::shared_ptr<Coupon> coupon = ext::dynamic_pointer_cast<Coupon>(i);
            Date paymentDate = coupon->date();
            // what's left of the coupon from today: its losses from then on,
            //   and its premium less what has accrued before today. A coupon
            //   whose accrual ended before today, paid after it, has none.
            Date startDate = std::max({coupon->accrualStartDate(), today, discountCurve_->referenceDate()});
            Date endDate = std::max(coupon->accrualEndDate(), startDate);
            // we assume the loss within the period took place on this date:
            Date defaultDate = startDate + (endDate - startDate) / 2;

            Real e2 = arguments_.basket->expectedTrancheLoss(endDate);
            results_.expectedTrancheLoss.push_back(e2);
            results_.premiumValue += ((inceptionTrancheNotional - e2) / inceptionTrancheNotional) *
                                     (coupon->amount() - coupon->accruedAmount(today)) *
                                     discountCurve_->discount(paymentDate);
            // default flows:
            const Real discount = discountCurve_->discount(defaultDate);

            /* Accrual removed till the argument flag is implemented
            // pays accrued on defaults' date
            results_.premiumValue += coupon->accruedAmount(defaultDate)
                * discount * (e2 - e1) / inceptionTrancheNotional;
            */
            results_.protectionValue += discount * (e2 - e1);
            /* use it in a future version for coherence with the integral engine
             * arguments_.leverageFactor;
             */
            e1 = e2;
        }

        //\todo treat upfron tnow as in the new CDS (see March 2014)
        // add includeSettlement date flows variable to engine ?
        // The upfront is taken as paid on the first coupon's accrual start. It
        //   counts only if that's on or after today, as the legs run from
        //   today (#227), and not at all if it falls before the discount
        //   curve's reference date, where CDO doesn't count it either (#206).
        const Date upfrontDate = ext::dynamic_pointer_cast<Coupon>(arguments_.normalizedLeg[0])->accrualStartDate();
        if (upfrontDate >= today && upfrontDate >= discountCurve_->referenceDate())
            results_.upfrontPremiumValue =
                results_.remainingNotional * arguments_.upfrontRate * discountCurve_->discount(upfrontDate);
        /* use it in a future version for coherence with the integral engine
            arguments_.leverageFactor * ;
        */
        if (arguments_.side == Protection::Buyer)
        {
            results_.protectionValue *= -1;
            results_.premiumValue *= -1;
            results_.upfrontPremiumValue *= -1;
        }
        results_.value = results_.premiumValue - results_.protectionValue + results_.upfrontPremiumValue;
        results_.errorEstimate = Null<Real>();
        // Fair spread GIVEN the upfront
        Real fairSpread = 0.;
        if (results_.premiumValue != 0.0)
        {
            fairSpread = -(results_.protectionValue + results_.upfrontPremiumValue) * arguments_.runningRate /
                         results_.premiumValue;
        }

        results_.additionalResults["fairPremium"] = fairSpread;
        results_.additionalResults["premiumLegNPV"] = Real(results_.premiumValue + results_.upfrontPremiumValue);
        results_.additionalResults["protectionLegNPV"] = results_.protectionValue;
    }

}

#endif
