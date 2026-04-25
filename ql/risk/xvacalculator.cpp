/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 dudleylane

 This file is new in the dudleylane fork of QuantLib and is licensed
 under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
 See the LICENSE file in the repository root for the full license text.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/risk/xvacalculator.hpp>
#include <ql/errors.hpp>
#include <utility>

namespace QuantLib {

    XvaCalculator::XvaCalculator(
        std::vector<Date> exposureDates,
        std::vector<Real> epe,
        std::vector<Real> ene,
        Handle<DefaultProbabilityTermStructure> counterpartyPd,
        Handle<DefaultProbabilityTermStructure> ownPd,
        Real counterpartyRecovery,
        Real ownRecovery,
        Spread fundingSpread,
        Spread capitalSpread,
        Spread marginSpread,
        Handle<YieldTermStructure> discountCurve,
        std::vector<Real> initialMargin)
    : exposureDates_(std::move(exposureDates)),
      epe_(std::move(epe)),
      ene_(std::move(ene)),
      counterpartyPd_(std::move(counterpartyPd)),
      ownPd_(std::move(ownPd)),
      counterpartyRecovery_(counterpartyRecovery),
      ownRecovery_(ownRecovery),
      fundingSpread_(fundingSpread),
      capitalSpread_(capitalSpread),
      marginSpread_(marginSpread),
      discountCurve_(std::move(discountCurve)),
      initialMargin_(std::move(initialMargin)) {

        QL_REQUIRE(exposureDates_.size() >= 2,
                   "XvaCalculator: need at least two exposure dates; got "
                   << exposureDates_.size());
        QL_REQUIRE(epe_.size() == exposureDates_.size(),
                   "XvaCalculator: EPE size " << epe_.size()
                   << " != exposure grid size " << exposureDates_.size());
        QL_REQUIRE(ene_.size() == exposureDates_.size(),
                   "XvaCalculator: ENE size " << ene_.size()
                   << " != exposure grid size " << exposureDates_.size());
        for (Real e : epe_) QL_REQUIRE(e >= 0.0,
            "XvaCalculator: expected positive exposure must be non-negative");
        for (Real e : ene_) QL_REQUIRE(e >= 0.0,
            "XvaCalculator: expected negative exposure (magnitude) must be non-negative");
        for (Size i = 1; i < exposureDates_.size(); ++i)
            QL_REQUIRE(exposureDates_[i] > exposureDates_[i-1],
                       "XvaCalculator: exposure dates must be strictly "
                       "increasing (violation at index " << i << ")");
        QL_REQUIRE(counterpartyRecovery_ >= 0.0 && counterpartyRecovery_ <= 1.0,
                   "counterparty recovery out of [0,1]: "
                   << counterpartyRecovery_);
        QL_REQUIRE(ownRecovery_ >= 0.0 && ownRecovery_ <= 1.0,
                   "own recovery out of [0,1]: " << ownRecovery_);
        QL_REQUIRE(!counterpartyPd_.empty(),
                   "XvaCalculator: counterparty default curve is empty");
        QL_REQUIRE(!ownPd_.empty(),
                   "XvaCalculator: own default curve is empty");
        QL_REQUIRE(!discountCurve_.empty(),
                   "XvaCalculator: discount curve is empty");
        if (!initialMargin_.empty())
            QL_REQUIRE(initialMargin_.size() == exposureDates_.size(),
                       "XvaCalculator: initial-margin grid size "
                       << initialMargin_.size()
                       << " != exposure grid size "
                       << exposureDates_.size());
    }

    Real XvaCalculator::cva() const {
        // Trapezoidal quadrature of LGD * integral(EPE(t) * dPD(t) * D(t))
        // over the exposure grid. For constant EPE this matches every
        // other convention; for monotone-rising or peak-and-fall profiles
        // the trapezoid is second-order accurate where left-endpoint is
        // first-order and biased low.
        const Real lgd = 1.0 - counterpartyRecovery_;
        Real total = 0.0;
        Probability p_prev = counterpartyPd_->defaultProbability(
            exposureDates_.front());
        for (Size i = 1; i < exposureDates_.size(); ++i) {
            Probability p_now = counterpartyPd_->defaultProbability(
                exposureDates_[i]);
            Real dPD = p_now - p_prev;
            DiscountFactor df = discountCurve_->discount(exposureDates_[i]);
            Real epeAvg = 0.5 * (epe_[i-1] + epe_[i]);
            total += epeAvg * dPD * df;
            p_prev = p_now;
        }
        return lgd * total;
    }

    Real XvaCalculator::dva() const {
        const Real lgd = 1.0 - ownRecovery_;
        Real total = 0.0;
        Probability p_prev = ownPd_->defaultProbability(
            exposureDates_.front());
        for (Size i = 1; i < exposureDates_.size(); ++i) {
            Probability p_now = ownPd_->defaultProbability(
                exposureDates_[i]);
            Real dPD = p_now - p_prev;
            DiscountFactor df = discountCurve_->discount(exposureDates_[i]);
            Real eneAvg = 0.5 * (ene_[i-1] + ene_[i]);
            total += eneAvg * dPD * df;
            p_prev = p_now;
        }
        return lgd * total;
    }

    namespace {
        // Trapezoidal quadrature of integral(exposure(t) * spread * D(t)) dt.
        Real spreadIntegral(const std::vector<Date>& dates,
                            const std::vector<Real>& exposures,
                            Spread spread,
                            const Handle<YieldTermStructure>& df) {
            Real total = 0.0;
            for (Size i = 1; i < dates.size(); ++i) {
                Time tau = df->dayCounter().yearFraction(
                    dates[i-1], dates[i]);
                Real expAvg = 0.5 * (exposures[i-1] + exposures[i]);
                total += expAvg * spread * tau *
                         df->discount(dates[i]);
            }
            return total;
        }
    }

    Real XvaCalculator::fva() const {
        return spreadIntegral(exposureDates_, epe_, fundingSpread_,
                              discountCurve_);
    }

    Real XvaCalculator::kva() const {
        return spreadIntegral(exposureDates_, epe_, capitalSpread_,
                              discountCurve_);
    }

    Real XvaCalculator::mva() const {
        if (initialMargin_.empty()) return 0.0;
        return spreadIntegral(exposureDates_, initialMargin_,
                              marginSpread_, discountCurve_);
    }

    Real XvaCalculator::totalXvaAdjustment() const {
        return cva() - dva() + fva() + kva() + mva();
    }

}
