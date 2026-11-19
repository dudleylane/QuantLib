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

#include <ql/experimental/credit/integralcdoengine.hpp>

#ifndef QL_PATCH_SOLARIS

#    include <ql/cashflows/fixedratecoupon.hpp>
#    include <ql/termstructures/yieldtermstructure.hpp>
#    include <algorithm>

namespace QuantLib
{

    void IntegralCDOEngine::calculate() const
    {
        Date today = Settings::instance().evaluationDate();

        results_.protectionValue = 0.0;
        results_.premiumValue = 0.0;
        results_.upfrontPremiumValue = 0.0;
        results_.error = 0;
        results_.expectedTrancheLoss.clear();
        results_.xMin = arguments_.basket->attachmentAmount();
        results_.xMax = arguments_.basket->detachmentAmount();
        results_.remainingNotional = arguments_.basket->remainingTrancheNotional();
        const Real inceptionTrancheNotional = arguments_.basket->trancheNotional();

        // The default curves of the names left don't reach back before their
        //   reference dates, so each expected tranche loss is taken on the
        //   later of its date and the latest of those (#275): defaults before
        //   it, of names whose curves start earlier, count as before the
        //   protection starts (#331). The integration steps stay where they were.
        const Date curvesDate = detail::latestDefaultCurveDate(*arguments_.basket);

        // Both legs run from today (#228). The protection starts from the
        //   expected tranche loss on the latest of today, the curves' date and
        //   the accrual start of the first coupon not yet paid or still
        //   accruing: what's lost already is in every later expected loss,
        //   and is no flow to come. A coupon paid before its accrual end, as
        //   under Preceding payments, has no premium left but still owes
        //   protection to that end (#329).
        Real e1 = inceptionTrancheNotional - results_.remainingNotional;
        // todo add includeSettlement date flows variable to engine.
        const auto firstLeft = std::find_if(
            arguments_.normalizedLeg.begin(), arguments_.normalizedLeg.end(), [&](const ext::shared_ptr<CashFlow>& cf)
            { return !cf->hasOccurred(today) || ext::dynamic_pointer_cast<Coupon>(cf)->accrualEndDate() > today; });
        if (firstLeft != arguments_.normalizedLeg.end())
            e1 = arguments_.basket->expectedTrancheLoss(
                std::max({ext::dynamic_pointer_cast<Coupon>(*firstLeft)->accrualStartDate(), today, curvesDate}));
        results_.expectedTrancheLoss.push_back(e1);

        for (auto& i : arguments_.normalizedLeg)
        {
            const ext::shared_ptr<Coupon> coupon = ext::dynamic_pointer_cast<Coupon>(i);
            // a coupon paid by today has no premium left, and nothing at all
            //   once its accrual has ended too (#329)
            const bool paid = i->hasOccurred(today);
            if (paid && coupon->accrualEndDate() <= today)
            {
                // add includeSettlement date flows variable to engine.
                results_.expectedTrancheLoss.push_back(0.);
                continue;
            }

            // what's left of the coupon from today: the premium accrued
            //   before today is out of the premium leg, as the losses before
            //   it are out of the protection. The coupon accrues to its
            //   accrual end and is paid on its payment date (#276), so its
            //   steps end at the accrual end, and each one's premium is
            //   discounted from the payment date. A coupon whose accrual
            //   ended before today, paid after it, has none left: its one
            //   step is empty.
            Date d1 = std::max(coupon->accrualStartDate(), today);
            Date d2 = std::max(coupon->accrualEndDate(), d1);

            Date d, d0 = d1;
            Real e2;
            do
            {
                d = NullCalendar().advance(d0, stepSize_);
                if (d > d2)
                    d = d2;

                e2 = arguments_.basket->expectedTrancheLoss(std::max(d, curvesDate));

                // a premium paid before the discount curve's reference date
                //   counts as paid on it, as the protection does (#328)
                if (!paid)
                    results_.premiumValue +=
                        (inceptionTrancheNotional - e2) * arguments_.runningRate *
                        arguments_.dayCounter.yearFraction(d0, d) *
                        discountCurve_->discount(std::max(coupon->date(), discountCurve_->referenceDate()));

                // TO DO: Addd default coupon accrual value here-----

                if (e2 < e1)
                    results_.error++;

                // a loss before the discount curve's reference date is paid
                //   as at that date, as in MidPointCDOEngine
                results_.protectionValue +=
                    (e2 - e1) * discountCurve_->discount(std::max(d, discountCurve_->referenceDate()));

                d0 = d;
                e1 = e2;
            } while (d < d2);
            results_.expectedTrancheLoss.push_back(e2);
        }

        // add includeSettlement date flows variable to engine.
        // The upfront is taken as paid on the first coupon's accrual start. It
        //   counts only if that's on or after today, as the legs run from
        //   today (#227), and not at all if it falls before the discount
        //   curve's reference date, where CDO doesn't count it either (#206).
        const Date upfrontDate = ext::dynamic_pointer_cast<Coupon>(arguments_.normalizedLeg[0])->accrualStartDate();
        if (upfrontDate >= today && upfrontDate >= discountCurve_->referenceDate())
            results_.upfrontPremiumValue =
                results_.remainingNotional * arguments_.upfrontRate * discountCurve_->discount(upfrontDate);

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
