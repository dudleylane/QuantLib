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

#include <ql/instruments/autocallablenote.hpp>
#include <ql/errors.hpp>
#include <ql/settings.hpp>
#include <utility>

namespace QuantLib {

    AutocallableNote::AutocallableNote(Real notional,
                                       Real initialSpot,
                                       std::vector<Date> observationDates,
                                       std::vector<Real> autocallBarriers,
                                       Rate couponRate,
                                       Real protectionBarrier,
                                       Date maturityDate)
    : notional_(notional), initialSpot_(initialSpot),
      observationDates_(std::move(observationDates)),
      autocallBarriers_(std::move(autocallBarriers)),
      couponRate_(couponRate), protectionBarrier_(protectionBarrier),
      maturityDate_(maturityDate) {

        QL_REQUIRE(notional_ > 0.0,
                   "AutocallableNote: notional must be positive, got "
                   << notional_);
        QL_REQUIRE(initialSpot_ > 0.0,
                   "AutocallableNote: initial spot must be positive, got "
                   << initialSpot_);
        QL_REQUIRE(!observationDates_.empty(),
                   "AutocallableNote: need at least one observation date");
        QL_REQUIRE(observationDates_.size() == autocallBarriers_.size(),
                   "AutocallableNote: observation dates ("
                   << observationDates_.size()
                   << ") and autocall barriers ("
                   << autocallBarriers_.size() << ") sizes differ");
        for (Size i = 0; i < autocallBarriers_.size(); ++i) {
            QL_REQUIRE(autocallBarriers_[i] > 0.0,
                       "AutocallableNote: autocall barrier #" << i
                       << " must be positive, got " << autocallBarriers_[i]);
        }
        for (Size i = 1; i < observationDates_.size(); ++i) {
            QL_REQUIRE(observationDates_[i] > observationDates_[i-1],
                       "AutocallableNote: observation dates must be "
                       "strictly increasing");
        }
        QL_REQUIRE(protectionBarrier_ > 0.0,
                   "AutocallableNote: protection barrier must be positive");
        QL_REQUIRE(maturityDate_ >= observationDates_.back(),
                   "AutocallableNote: maturity ("  << maturityDate_
                   << ") must be on or after the last observation date ("
                   << observationDates_.back() << ")");
    }

    bool AutocallableNote::isExpired() const {
        return maturityDate_ < Settings::instance().evaluationDate();
    }

    void AutocallableNote::setupArguments(
        PricingEngine::arguments* args) const {
        auto* a = dynamic_cast<arguments*>(args);
        QL_REQUIRE(a != nullptr,
                   "AutocallableNote: wrong arguments type");
        a->notional = notional_;
        a->initialSpot = initialSpot_;
        a->observationDates = observationDates_;
        a->autocallBarriers = autocallBarriers_;
        a->couponRate = couponRate_;
        a->protectionBarrier = protectionBarrier_;
        a->maturityDate = maturityDate_;
    }

    void AutocallableNote::arguments::validate() const {
        QL_REQUIRE(notional > 0.0, "non-positive notional");
        QL_REQUIRE(initialSpot > 0.0, "non-positive initial spot");
        QL_REQUIRE(!observationDates.empty(),
                   "empty observation-date vector");
        QL_REQUIRE(observationDates.size() == autocallBarriers.size(),
                   "observation dates / autocall barriers size mismatch");
        QL_REQUIRE(protectionBarrier > 0.0,
                   "non-positive protection barrier");
    }

}
