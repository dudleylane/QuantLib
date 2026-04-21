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

/*! \file xvacalculator.hpp
    \brief Exposure-driven XVA calculator (CVA/DVA/FVA/KVA + MVA)

    Given a pre-computed expected-exposure profile on a time grid, plus
    default-probability curves, funding / capital / margin spreads, and
    a discount curve, returns the adjustments as discrete-integral
    approximations. The caller is responsible for exposure generation:
    deterministic profiles, bucketed bump-and-reprice with `CurveBucketer`,
    or Monte-Carlo forward valuation all plug in uniformly.

    This is the adjustment layer — not a full exposure-simulation
    framework. Upstream `cvaswapengine` remains the analytic single-
    trade CVA for vanilla swaps; this calculator generalizes to any
    instrument for which the caller can produce an EPE/ENE curve.

    \warning MVP scope. No wrong-way risk, no collateral thresholds /
    MTAs, no rating-dependent spreads, no SA-CCR initial margin
    formula (MVA takes a user-supplied IM profile). Those are
    follow-ups that sit on top of this primitive.
*/

#ifndef quantlib_xva_calculator_hpp
#define quantlib_xva_calculator_hpp

#include <ql/handle.hpp>
#include <ql/time/date.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <vector>

namespace QuantLib {

    //! Exposure-driven XVA adjustments (CVA/DVA/FVA/KVA/MVA).
    /*! All adjustments are returned as positive numbers from the
        bank's perspective: CVA is the cost of counterparty default
        (subtracted from trade PV), DVA is the bank's own-default
        benefit, FVA/KVA/MVA are funding/capital/margin-funding costs.

        Discretization: the exposure grid is interpreted as piecewise-
        constant on each segment [t_i, t_{i+1}]. Default probability
        increments over each segment are taken from the supplied
        default curves directly. The integrals are summed:

          CVA = (1 - R_cpty) * sum_i EPE[i] * (P_cpty(t_{i+1}) - P_cpty(t_i)) * df(t_{i+1})
          DVA = (1 - R_own) * sum_i ENE[i] * (P_own(t_{i+1}) - P_own(t_i)) * df(t_{i+1})
          FVA = sum_i EPE[i] * fundingSpread * tau_i * df(t_{i+1})
          KVA = sum_i EPE[i] * capitalSpread * tau_i * df(t_{i+1})
          MVA = sum_i IM[i]  * marginSpread * tau_i * df(t_{i+1})

        where tau_i is the year fraction of the segment under the
        discount curve's day counter, P_cpty/P_own are the cumulative
        default probabilities of the counterparty and the bank, and R
        are the recovery rates. When no IM profile is passed, MVA is
        reported as zero.
    */
    class XvaCalculator {
      public:
        XvaCalculator(
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
            std::vector<Real> initialMargin = {});

        //! \name Adjustments — all reported as positive magnitudes.
        //@{
        Real cva() const;
        Real dva() const;
        Real fva() const;
        Real kva() const;
        Real mva() const;
        //! CVA + FVA + KVA + MVA minus DVA.
        /*! Sign convention: positive means the bank's view of trade PV
            is reduced by this amount. DVA subtracts because it is an
            own-credit benefit. */
        Real totalXvaAdjustment() const;
        //@}

      private:
        std::vector<Date> exposureDates_;
        std::vector<Real> epe_;
        std::vector<Real> ene_;
        Handle<DefaultProbabilityTermStructure> counterpartyPd_;
        Handle<DefaultProbabilityTermStructure> ownPd_;
        Real counterpartyRecovery_;
        Real ownRecovery_;
        Spread fundingSpread_;
        Spread capitalSpread_;
        Spread marginSpread_;
        Handle<YieldTermStructure> discountCurve_;
        std::vector<Real> initialMargin_;
    };

}

#endif
