/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2018 Roy Zywina
 Copyright (C) 2019 Eisuke Tani

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

#include <ql/event.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/instruments/overnightindexfuture.hpp>
#include <algorithm>
#include <utility>

namespace QuantLib
{

    namespace
    {

        // The index's forwarding curve, checked once for forecasts from d, since later ones start later; and d's time
        // on it.
        std::pair<ext::shared_ptr<YieldTermStructure>, Time> overnightFutureForecastCurve(const OvernightIndex& index,
                                                                                          const Date& d)
        {
            const Handle<YieldTermStructure> curve = index.forwardingTermStructure();
            QL_REQUIRE(!curve.empty(),
                       "can't forecast " << index.name() << " from " << d << ": it has no forwarding curve");
            const Time t = curve->timeFromReference(d);
            // the curve's own condition on a time it is asked for
            QL_REQUIRE(t >= 0.0, "can't forecast " << index.name() << " from " << d
                                                   << ": its forwarding curve starts on " << curve->referenceDate());
            return {curve.currentLink(), t};
        }

    }

    OvernightIndexFuture::OvernightIndexFuture(ext::shared_ptr<OvernightIndex> overnightIndex,
                                               const Date& valueDate,
                                               const Date& maturityDate,
                                               Handle<Quote> convexityAdjustment,
                                               RateAveraging::Type averagingMethod)
    : overnightIndex_(std::move(overnightIndex)), valueDate_(valueDate), maturityDate_(maturityDate),
      convexityAdjustment_(std::move(convexityAdjustment)), averagingMethod_(averagingMethod)
    {
        QL_REQUIRE(overnightIndex_, "null overnight index");
        registerWith(overnightIndex_);
        registerWith(convexityAdjustment_);
        registerWith(Settings::instance().evaluationDate());
    }

    Real OvernightIndexFuture::averagedRate() const
    {
        Date today = Settings::instance().evaluationDate();
        Calendar calendar = overnightIndex_->fixingCalendar();
        DayCounter dayCounter = overnightIndex_->dayCounter();
        Real avg = 0;
        Date d1 = valueDate_;
        // d1 could be a holiday
        Date fixingDate = calendar.adjust(d1, Preceding);
        // The fixing dates only increase, so the history is read only if the first is due by today.
        if (fixingDate <= today)
        {
            const auto& history = overnightIndex_->timeSeries();
            while (d1 < maturityDate_ && fixingDate <= today)
            {
                Real fwd = history[fixingDate];
                if (fwd == Null<Real>())
                {
                    QL_REQUIRE(fixingDate == today,
                               "missing rate on " << fixingDate << " for index " << overnightIndex_->name());
                    // today's fixing is forecast below, as InterestRateIndex::fixing does, unless it has to be stored
                    QL_REQUIRE(!Settings::instance().enforcesTodaysHistoricFixings(),
                               "Missing " << overnightIndex_->name() << " fixing for " << fixingDate);
                    break;
                }
                Date d2 = calendar.advance(d1, 1, Days);
                // The rate is accrued starting from d1 even when the fixing date is earlier.
                // d2 might be beyond the maturity date if the latter is a holiday.
                avg += fwd * dayCounter.yearFraction(d1, std::min(d2, maturityDate_));
                fixingDate = d1 = d2;
            }
        }
        if (d1 < maturityDate_)
        {
            const ext::shared_ptr<YieldTermStructure> forwardCurve =
                overnightFutureForecastCurve(*overnightIndex_, fixingDate).first;
            do
            {
                Date d2 = calendar.advance(d1, 1, Days);
                Real fwd = forwardCurve->forwardRate(fixingDate, d2, dayCounter, Simple).rate();
                avg += fwd * dayCounter.yearFraction(d1, std::min(d2, maturityDate_));
                fixingDate = d1 = d2;
            } while (d1 < maturityDate_);
        }

        return avg / dayCounter.yearFraction(valueDate_, maturityDate_);
    }

    Real OvernightIndexFuture::compoundedRate() const
    {
        const Date evaluationDate = Settings::instance().evaluationDate();
        Calendar calendar = overnightIndex_->fixingCalendar();
        DayCounter dayCounter = overnightIndex_->dayCounter();
        Real prod = 1;
        Date d1 = valueDate_;
        // d1 could be a holiday
        Date fixingDate = calendar.adjust(d1, Preceding);
        // The fixing dates only increase, so the history is read only if the first is due by today.
        if (fixingDate <= evaluationDate)
        {
            // can't value on a weekend inside reference period because we
            // won't know the reset rate until start of next business day.
            // user can supply an estimate if they really want to do this
            const Date today = calendar.adjust(evaluationDate);
            // for valuations inside the reference period, index quotes
            // must have been populated in the history
            const auto& history = overnightIndex_->timeSeries();
            while (d1 < maturityDate_ && fixingDate <= today)
            {
                Real r = history[fixingDate];
                if (r == Null<Real>())
                {
                    QL_REQUIRE(fixingDate == today,
                               "missing rate on " << fixingDate << " for index " << overnightIndex_->name());
                    // today's fixing is forecast below, as InterestRateIndex::fixing does, unless it has to be
                    // stored; where today was moved to the next business day, that day's fixing isn't due yet
                    QL_REQUIRE(fixingDate != evaluationDate || !Settings::instance().enforcesTodaysHistoricFixings(),
                               "Missing " << overnightIndex_->name() << " fixing for " << fixingDate);
                    break;
                }
                // The rate is accrued starting from d1 even when the fixing date is earlier,
                // and only up to the maturity, which today can be past.
                Date d2 = std::min(calendar.advance(d1, 1, Days), maturityDate_);
                prod *= 1 + r * dayCounter.yearFraction(d1, d2);
                fixingDate = d1 = d2;
            }
        }
        // the telescopic part goes from the end of the last known fixing to the maturity
        if (d1 < maturityDate_)
        {
            const auto [forwardCurve, startTime] = overnightFutureForecastCurve(*overnightIndex_, d1);
            DiscountFactor forwardDiscount = forwardCurve->discount(maturityDate_) / forwardCurve->discount(startTime);
            prod /= forwardDiscount;
        }

        return (prod - 1) / dayCounter.yearFraction(valueDate_, maturityDate_);
    }

    Real OvernightIndexFuture::rate() const
    {
        switch (averagingMethod_)
        {
            case RateAveraging::Simple:
                return averagedRate();
            case RateAveraging::Compound:
                return compoundedRate();
            default:
                QL_FAIL("unknown compounding convention (" << Integer(averagingMethod_) << ")");
        }
    }

    bool OvernightIndexFuture::isExpired() const
    {
        return detail::hasOccurred(maturityDate_);
    }

    Real OvernightIndexFuture::convexityAdjustment() const
    {
        return convexityAdjustment_.empty() ? 0.0 : convexityAdjustment_->value();
    }

    void OvernightIndexFuture::performCalculations() const
    {
        Rate R = convexityAdjustment() + rate();
        NPV_ = 100.0 * (1.0 - R);
    }

}
