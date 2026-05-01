/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 dudleylane

 This file is new in the dudleylane fork of QuantLib and is licensed
 under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
 See the LICENSE file in the repository root for the full license text.

 Unlike upstream QuantLib files (which remain under BSD-3-Clause — see
 LICENSE.TXT), this file and any derivative works of it are subject to
 the copyleft and network-use clauses of AGPL-3.0.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file autocallablenote.hpp
    \brief Single-underlying step-down autocallable note (Phase 2 MVP)

    Payoff on each observation date:
      - If spot(t_i) >= autocallBarrier_i * initialSpot, redeem early:
        pay notional * (1 + couponRate * accumulatedPeriods) and stop.
    At maturity (if never autocalled):
      - If spot(T) >= protectionBarrier * initialSpot, pay notional *
        (1 + couponRate * totalPeriods).
      - Else pay notional * spot(T) / initialSpot (full participation
        in downside — European-style knock-in, no soft/hard protection
        distinction in this MVP).

    \warning MVP scope. Not covered: multiple underlyings (worst-of /
    best-of), memory / snowball coupons, knock-in barriers during the
    life, callable-at-issuer-discretion, stochastic-volatility models
    (Heston), skew-aware pricing. Those build on top of this class.
*/

#ifndef quantlib_autocallable_note_hpp
#define quantlib_autocallable_note_hpp

#include <ql/instrument.hpp>
#include <ql/pricingengine.hpp>
#include <ql/time/date.hpp>
#include <vector>

namespace QuantLib
{

    //! Single-underlying step-down autocallable note.
    /*! \ingroup instruments */
    class AutocallableNote : public Instrument
    {
      public:
        class arguments;
        class results;
        class engine;

        AutocallableNote(Real notional,
                         Real initialSpot,
                         std::vector<Date> observationDates,
                         std::vector<Real> autocallBarriers,
                         Rate couponRate,
                         Real protectionBarrier,
                         Date maturityDate);

        //! \name Instrument interface
        //@{
        bool isExpired() const override;
        void setupArguments(PricingEngine::arguments*) const override;
        //@}

        //! \name Inspectors
        //@{
        Real notional() const { return notional_; }
        Real initialSpot() const { return initialSpot_; }
        const std::vector<Date>& observationDates() const { return observationDates_; }
        const std::vector<Real>& autocallBarriers() const { return autocallBarriers_; }
        Rate couponRate() const { return couponRate_; }
        Real protectionBarrier() const { return protectionBarrier_; }
        Date maturityDate() const { return maturityDate_; }
        //@}

      private:
        Real notional_;
        Real initialSpot_;
        std::vector<Date> observationDates_;
        std::vector<Real> autocallBarriers_;
        Rate couponRate_;
        Real protectionBarrier_;
        Date maturityDate_;
    };

    class AutocallableNote::arguments : public PricingEngine::arguments
    {
      public:
        //! Principal / face value of the note.
        Real notional = 0.0;
        //! Reference spot S_0 at inception; barriers are expressed as
        //! multiples of this value.
        Real initialSpot = 0.0;
        //! Observation dates at which autocall is checked, in strictly
        //! increasing order.
        std::vector<Date> observationDates;
        //! Autocall barriers, one per observation date. Redemption is
        //! triggered if `spot(t_i) >= autocallBarriers[i] * initialSpot`.
        std::vector<Real> autocallBarriers;
        //! Fixed coupon rate applied per elapsed observation period on
        //! autocall or at maturity when above the protection barrier.
        Rate couponRate = 0.0;
        //! Maturity protection barrier, as a multiple of `initialSpot`.
        //! If `spot(T) / initialSpot >= protectionBarrier` the note
        //! pays `notional * (1 + couponRate * N)`; otherwise the
        //! holder participates one-for-one in the downside ratio.
        Real protectionBarrier = 0.0;
        //! Final maturity; must be >= the last observation date.
        Date maturityDate;
        void validate() const override;
    };

    //! Pricing results for an AutocallableNote.
    /*! Uses the default `Instrument::results` surface. Engines are
        expected to set `results_.value` plus any additional
        per-engine diagnostics via `additionalResults`. */
    class AutocallableNote::results : public Instrument::results
    {
    };

    class AutocallableNote::engine : public GenericEngine<AutocallableNote::arguments, AutocallableNote::results>
    {
    };

}

#endif
