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

#include <ql/indexes/fallbackiborindex.hpp>
#include <ql/errors.hpp>

namespace QuantLib {

    namespace {
        // Check the legacy index is non-null and return it, so the
        // member-initializer list can dereference it without needing
        // fallback ternaries for every IborIndex base-class argument.
        const ext::shared_ptr<IborIndex>& checkLegacy(
            const ext::shared_ptr<IborIndex>& legacy) {
            QL_REQUIRE(legacy,
                       "FallbackIborIndex requires a non-null legacy IBOR index");
            return legacy;
        }
    }

    FallbackIborIndex::FallbackIborIndex(
        const ext::shared_ptr<IborIndex>& legacyIndex,
        const ext::shared_ptr<OvernightIndex>& rfrIndex,
        const Date& cessationDate,
        Spread spreadAdjustment)
    : IborIndex(checkLegacy(legacyIndex)->familyName() + "-Fallback",
                legacyIndex->tenor(),
                legacyIndex->fixingDays(),
                legacyIndex->currency(),
                legacyIndex->fixingCalendar(),
                legacyIndex->businessDayConvention(),
                legacyIndex->endOfMonth(),
                legacyIndex->dayCounter(),
                legacyIndex->forwardingTermStructure()),
      legacyIndex_(legacyIndex), rfrIndex_(rfrIndex),
      cessationDate_(cessationDate), spreadAdjustment_(spreadAdjustment) {

        QL_REQUIRE(rfrIndex_,
                   "FallbackIborIndex requires a non-null replacement RFR index");
        QL_REQUIRE(legacyIndex_->currency() == rfrIndex_->currency(),
                   "FallbackIborIndex: legacy index currency ("
                   << legacyIndex_->currency().code()
                   << ") must match RFR currency ("
                   << rfrIndex_->currency().code() << ")");
        // Day-counter consistency: compoundedRfrRate() takes dfs from the
        // RFR curve but scales tau by dayCounter() (inherited from the
        // legacy index). If they differ, the period-yield implied by
        // the RFR discount factors is misread. Most real pairs coincide
        // (SOFR/USD-LIBOR Act/360; SONIA/GBP-LIBOR Act/365; €STR/EURIBOR
        // Act/360; SARON/CHF-LIBOR Act/360); enforce strictly.
        QL_REQUIRE(legacyIndex_->dayCounter() == rfrIndex_->dayCounter(),
                   "FallbackIborIndex: legacy day counter ("
                   << legacyIndex_->dayCounter().name()
                   << ") differs from RFR day counter ("
                   << rfrIndex_->dayCounter().name()
                   << "). Coerce the RFR index's day counter to match "
                   "or the post-cessation tau/discount pairing is "
                   "inconsistent.");
        registerWith(legacyIndex_);
        registerWith(rfrIndex_);
    }

    Rate FallbackIborIndex::forecastFixing(const Date& fixingDate) const {
        if (fixingDate < cessationDate_) {
            // Legacy regime: delegate to the IBOR index.
            return legacyIndex_->forecastFixing(fixingDate);
        }
        // Post-cessation: compound the RFR over the legacy tenor and add
        // the ISDA spread adjustment.
        Date valueDt = valueDate(fixingDate);
        Date maturityDt = maturityDate(valueDt);
        return compoundedRfrRate(valueDt, maturityDt) + spreadAdjustment_;
    }

    Rate FallbackIborIndex::compoundedRfrRate(const Date& startDate,
                                              const Date& endDate) const {
        QL_REQUIRE(endDate > startDate,
                   "FallbackIborIndex::compoundedRfrRate: end date ("
                   << endDate << ") must be after start date ("
                   << startDate << ")");

        Handle<YieldTermStructure> rfrTS = rfrIndex_->forwardingTermStructure();
        QL_REQUIRE(!rfrTS.empty(),
                   "FallbackIborIndex: RFR index "
                   << rfrIndex_->name() << " has no forwarding term structure; "
                   "cannot forecast post-cessation fixings");

        // The RFR's discount curve reflects compounded overnight rates, so
        // the simply-compounded rate implied by df(start)/df(end) over the
        // period is mathematically the limit of the ISDA daily-compounded
        // rate when fixings and curve agree. For historical periods with
        // published daily fixings, a future enhancement can walk the
        // fixing series; for now the curve-based computation covers both
        // past and future periods uniformly.
        DiscountFactor dfStart = rfrTS->discount(startDate);
        DiscountFactor dfEnd   = rfrTS->discount(endDate);
        Time tau = dayCounter().yearFraction(startDate, endDate);
        QL_REQUIRE(tau > 0.0,
                   "FallbackIborIndex::compoundedRfrRate: non-positive "
                   "year fraction (" << tau << ")");
        return (dfStart / dfEnd - 1.0) / tau;
    }

    ext::shared_ptr<IborIndex> FallbackIborIndex::clone(
        const Handle<YieldTermStructure>& forwarding) const {
        // The forwarding handle is relevant only to the legacy index
        // (pre-cessation). Clone the legacy with the new handle and keep
        // the same RFR pointer, cessation date, and spread: the RFR's
        // own forwarding handle is independent of the legacy's, and
        // the cessation/spread are fixed protocol parameters. Callers
        // who want an independent RFR handle can clone the RFR
        // separately before constructing a new FallbackIborIndex.
        return ext::make_shared<FallbackIborIndex>(
            legacyIndex_->clone(forwarding),
            rfrIndex_,
            cessationDate_,
            spreadAdjustment_);
    }

}
