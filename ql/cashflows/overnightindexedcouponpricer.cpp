/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2016 Stefano Fondi
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer

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

#include <ql/cashflows/overnightindexedcouponpricer.hpp>
#include <ql/math/expm1.hpp>
#include <utility>

namespace QuantLib
{

    namespace
    {

        // The number of fixings due by a date: those whose periods start before it, and so all of them at the accrual
        // end of a coupon whose rate is computed over an earlier period.
        Size determineNumberOfFixings(const std::vector<Date>& interestDates, const Date& date)
        {
            return std::lower_bound(interestDates.begin(), interestDates.end() - 1, date) - interestDates.begin();
        }

        // The time a coupon's rate is worked out over by a date, in its index's day counter: from its rate computation
        // start, its first interest date, to the date or, after it, to its rate computation end, its last. The rate is
        // on the index's basis, as the index's fixings are, and the coupon's own day counter scales its amount.
        Time ratePeriodOnIndexBasis(const OvernightIndexedCoupon& coupon, const Date& date)
        {
            const std::vector<Date>& interestDates = coupon.interestDates();
            return coupon.overnightIndex()->dayCounter().yearFraction(interestDates.front(),
                                                                      std::min(date, interestDates.back()));
        }
    }

    OvernightIndexedCouponPricer::OvernightIndexedCouponPricer(Handle<OptionletVolatilityStructure> v,
                                                               const bool effectiveVolatilityInput)
    : capletVol_(std::move(v)), effectiveVolatilityInput_(effectiveVolatilityInput)
    {
        registerWith(capletVol_);
    }

    void OvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon)
    {
        const OvernightIndexedCoupon* onCoupon = coupon.overnightIndexedCoupon();
        if (onCoupon == nullptr)
        {
            // a capped or floored overnight coupon gives null only when it has no underlying coupon
            QL_REQUIRE(
                dynamic_cast<const CappedFlooredOvernightIndexedCoupon*>(&coupon) == nullptr,
                "OvernightIndexedCouponPricer: CappedFlooredOvernightIndexedCoupon underlying coupon not defined");
            QL_FAIL("OvernightIndexedCouponPricer: unsupported coupon type");
        }
        coupon_ = onCoupon;
    }

    bool OvernightIndexedCouponPricer::effectiveVolatilityInput() const
    {
        return effectiveVolatilityInput_;
    }

    Real OvernightIndexedCouponPricer::effectiveCapletVolatility() const
    {
        return effectiveCapletVolatility_;
    }

    Real OvernightIndexedCouponPricer::effectiveFloorletVolatility() const
    {
        return effectiveFloorletVolatility_;
    }

    CompoundingOvernightIndexedCouponPricer::CompoundingOvernightIndexedCouponPricer(
        Handle<OptionletVolatilityStructure> v, const bool effectiveVolatilityInput)
    : OvernightIndexedCouponPricer(std::move(v), effectiveVolatilityInput)
    {
    }

    void CompoundingOvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon)
    {
        const OvernightIndexedCoupon* onCoupon = coupon.overnightIndexedCoupon();
        QL_REQUIRE(onCoupon == nullptr || onCoupon->averagingMethod() == RateAveraging::Compound,
                   "Averaging method required to be compound for CompoundingOvernightIndexedCouponPricer");
        OvernightIndexedCouponPricer::initialize(coupon);
    }

    Rate CompoundingOvernightIndexedCouponPricer::swapletRate() const
    {
        auto [swapletRate, effectiveSpread, effectiveIndexFixing] = compute(coupon_->accrualEndDate());
        swapletRate_ = swapletRate;
        effectiveSpread_ = effectiveSpread;
        effectiveIndexFixing_ = effectiveIndexFixing;
        return swapletRate;
    }

    Rate CompoundingOvernightIndexedCouponPricer::averageRate(const Date& date) const
    {
        auto [rate, effectiveSpread, effectiveIndexFixing] = compute(date);
        return rate;
    }

    Rate CompoundingOvernightIndexedCouponPricer::effectiveSpread() const
    {
        auto [r, effectiveSpread, effectiveIndexFixing] = compute(coupon_->accrualEndDate());
        effectiveSpread_ = effectiveSpread;
        return effectiveSpread_;
    }

    Rate CompoundingOvernightIndexedCouponPricer::effectiveIndexFixing() const
    {
        auto [r, effectiveSpread, effectiveIndexFixing] = compute(coupon_->accrualEndDate());
        effectiveIndexFixing_ = effectiveIndexFixing;
        return effectiveIndexFixing_;
    }

    std::tuple<Rate, Spread, Rate> CompoundingOvernightIndexedCouponPricer::compute(const Date& date) const
    {
        const Date today = Settings::instance().evaluationDate();

        const ext::shared_ptr<OvernightIndex>& index = coupon_->overnightIndex();

        const auto& fixingDates = coupon_->fixingDates();
        const auto& valueDates = coupon_->valueDates();
        const auto& interestDates = coupon_->interestDates();
        const auto& dt = coupon_->dt();
        const bool applyObservationShift = coupon_->applyObservationShift();
        // with observation shift and a lookback, each fixing counted is applied for its whole period, over the
        // observation period, and the rate is annualised over the span of the periods counted
        const bool shiftsObservation = applyObservationShift && coupon_->fixingDays() > 0;
        Real couponSpread = coupon_->spread();

        Size i = 0;
        const Size n = determineNumberOfFixings(interestDates, date);

        // The loops below read the index's history only at fixings due by today, starting from
        // the first, so a coupon whose first fixing is after today doesn't look it up.
        const TimeSeries<Real>* pastFixings = nullptr;
        if (n > 0 && fixingDates[0] <= today)
            pastFixings = &index->timeSeries();

        Real compoundFactor = 1.0, compoundFactorWithoutSpread = 1.0;

        // already fixed part
        while (i < n && fixingDates[i] < today)
        {
            // rate must have been fixed
            Rate fixing = (*pastFixings)[fixingDates[i]];
            QL_REQUIRE(fixing != Null<Real>(), "Missing " << index->name() << " fixing for " << fixingDates[i]);
            Time span = (shiftsObservation || date >= interestDates[i + 1] ?
                             dt[i] :
                             index->dayCounter().yearFraction(interestDates[i], date));
            if (coupon_->compoundSpreadDaily())
            {
                compoundFactorWithoutSpread *= (1.0 + fixing * span);
                fixing += coupon_->spread();
            }
            compoundFactor *= (1.0 + fixing * span);
            ++i;
        }

        // today is a border case: a fixing stored for today is taken at each fixing date that is today, as a
        // lockout repeats it, and a missing one is forecast below unless Settings::enforcesTodaysHistoricFixings()
        // requires it, as InterestRateIndex::fixing does
        while (i < n && fixingDates[i] == today)
        {
            Rate fixing = (*pastFixings)[fixingDates[i]];
            if (fixing != Null<Real>())
            {
                Time span = (shiftsObservation || date >= interestDates[i + 1] ?
                                 dt[i] :
                                 index->dayCounter().yearFraction(interestDates[i], date));
                if (coupon_->compoundSpreadDaily())
                {
                    compoundFactorWithoutSpread *= (1.0 + fixing * span);
                    fixing += coupon_->spread();
                }
                compoundFactor *= (1.0 + fixing * span);
                ++i;
            }
            else
            {
                QL_REQUIRE(!Settings::instance().enforcesTodaysHistoricFixings(),
                           "Missing " << index->name() << " fixing for " << fixingDates[i]);
                break;
            }
        }

        // forward part using telescopic property in order
        // to avoid the evaluation of multiple forward fixings
        // where possible.
        if (i < n)
        {
            const Handle<YieldTermStructure> curve = index->forwardingTermStructure();
            QL_REQUIRE(!curve.empty(), "null term structure set to this instance of " << index->name());

            const auto effectiveRate = [&index, &fixingDates, &date, &interestDates, &dt, &couponSpread,
                                        shiftsObservation](Size position, bool compoundSpreadDaily)
            {
                Rate fixing = index->fixing(fixingDates[position]);
                Time span = (shiftsObservation || date >= interestDates[position + 1] ?
                                 dt[position] :
                                 index->dayCounter().yearFraction(interestDates[position], date));
                Spread spreadToAdd = compoundSpreadDaily ? couponSpread : 0.0;
                return span * (fixing + spreadToAdd);
            };

            if (!coupon_->canApplyTelescopicFormula() || (coupon_->compoundSpreadDaily() && couponSpread != 0.0))
            {
                // With lookback applied, the telescopic formula cannot be used,
                // we need to project each fixing in the coupon.
                // Nor can it with a nonzero spread compounded daily: the spread
                // compounds with each day's fixing, and the ratio of discount
                // factors gives the product of the fixings' factors alone.
                // Only in one particular case when observation shift is used and
                // no intrinsic index fixing delay is applied, the telescopic formula
                // holds, because regardless of the fixing delay in the coupon,
                // in such configuration the fixings compound over the periods
                // between value dates, which are their observation dates.
                // A potential lockout, which may occur in tandem with a lookback
                // setting, will be handled automatically based on fixing dates.
                // Same applies to a case when accrual calculation date does or
                // does not occur on an interest date.
                while (i < n)
                {
                    compoundFactorWithoutSpread *= (1.0 + effectiveRate(i, false));
                    compoundFactor *= (1.0 + effectiveRate(i, coupon_->compoundSpreadDaily()));
                    ++i;
                }
            }
            else
            {
                // No lookback, we can partially apply the telescopic formula.
                // But we need to make a correction for a potential lockout.
                // Nor does it apply to a first period whose value date is before its interest date, a rate computation
                // start that isn't a fixing day: its fixing applies from the start, while the ratio of discount factors
                // would take it from its value date. Such a period is compounded on its own.
                if (i == 0 && !shiftsObservation && valueDates[0] < interestDates[0])
                {
                    compoundFactorWithoutSpread *= (1.0 + effectiveRate(0, false));
                    compoundFactor *= (1.0 + effectiveRate(0, coupon_->compoundSpreadDaily()));
                    ++i;
                }
                if (i < n)
                {
                    const Size nLockout = n - coupon_->lockoutDays();
                    const bool isLockoutApplied = coupon_->lockoutDays() > 0;

                    // Lockout could already start at or before i.
                    // In such case the ratio of discount factors will be equal to 1.
                    const DiscountFactor startDiscount = curve->discount(valueDates[std::min<Size>(nLockout, i)]);
                    // The formula runs to the end of the last fixing's period where the date is at or after it, as
                    // long as the period ends at its value date: with observation shift, the end of its observation
                    // period; otherwise, unless it ends at a rate computation end that isn't a fixing day.
                    if ((interestDates[n] <= date && (shiftsObservation || valueDates[n] == interestDates[n])) ||
                        isLockoutApplied)
                    {
                        // telescopic formula up to potential lockout dates.
                        const DiscountFactor endDiscount = curve->discount(valueDates[std::min<Size>(nLockout, n)]);
                        compoundFactor *= startDiscount / endDiscount;
                        compoundFactorWithoutSpread *= startDiscount / endDiscount;
                        // For the lockout periods the telescopic formula does not apply.
                        // The value dates (at which the projection is calculated) correspond
                        // to the locked-out fixing, while the interest dates (at which the
                        // interest over that fixing is accrued) are not fixed at lockout,
                        // hence they do not cancel out.
                        i = std::max(nLockout, i);

                        // With no lockout, the loop is skipped because i = n.
                        while (i < n)
                        {
                            compoundFactorWithoutSpread *= (1.0 + effectiveRate(i, false));
                            compoundFactor *= (1.0 + effectiveRate(i, coupon_->compoundSpreadDaily()));
                            ++i;
                        }
                    }
                    else
                    {
                        // No lockout, and the last fixing is not used up to its value date: the date is inside its
                        // period, or the period ends at a rate computation end that isn't a fixing day. We can use the
                        // telescopic formula until the previous date, then we'll add the missing bit.
                        const DiscountFactor endDiscount = curve->discount(valueDates[n - 1]);
                        compoundFactor *= startDiscount / endDiscount;
                        compoundFactorWithoutSpread *= startDiscount / endDiscount;
                        compoundFactor *= (1.0 + effectiveRate(n - 1, coupon_->compoundSpreadDaily()));
                        compoundFactorWithoutSpread *= (1.0 + effectiveRate(n - 1, false));
                    }
                }
            }
        }

        const Rate tau = index->dayCounter().yearFraction(valueDates.front(), valueDates.back());
        // the period the rate is over: with observation shift and a lookback, the part of the observation period the
        // fixings counted span; else, or where none is counted yet, the rate computation period up to the date. No
        // fixing is due by a date at or before the rate computation start, as early in the accrual period of a coupon
        // whose rate is taken over a last recent period, and the index has added nothing by it.
        const Time ratePeriod = shiftsObservation && n > 0 ?
                                    index->dayCounter().yearFraction(valueDates.front(), valueDates[n]) :
                                    ratePeriodOnIndexBasis(*coupon_, date);
        const Rate rate = n > 0 ? (compoundFactor - 1.0) / ratePeriod : 0.0;
        Rate swapletRate = coupon_->gearing() * rate;
        Spread effectiveSpread;
        Rate effectiveIndexFixing;

        if (!coupon_->compoundSpreadDaily())
        {
            swapletRate += coupon_->spread();
            effectiveSpread = coupon_->spread();
            effectiveIndexFixing = rate;
        }
        else
        {
            effectiveSpread = rate - (compoundFactorWithoutSpread - 1.0) / tau;
            effectiveIndexFixing = rate - effectiveSpread;
        }

        return std::make_tuple(swapletRate, effectiveSpread, effectiveIndexFixing);
    }

    void ArithmeticAveragedOvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon)
    {
        const OvernightIndexedCoupon* onCoupon = coupon.overnightIndexedCoupon();
        QL_REQUIRE(onCoupon == nullptr || onCoupon->averagingMethod() == RateAveraging::Simple,
                   "Averaging method required to be simple for ArithmeticAveragedOvernightIndexedCouponPricer");
        OvernightIndexedCouponPricer::initialize(coupon);
    }

    Rate ArithmeticAveragedOvernightIndexedCouponPricer::swapletRate() const
    {
        return averageRate(coupon_->accrualEndDate());
    }

    Rate ArithmeticAveragedOvernightIndexedCouponPricer::averageRate(const Date& date) const
    {

        // a raw pointer, so that the forecast loop below doesn't reload it from the coupon for each fixing
        const OvernightIndex* index = coupon_->overnightIndex().get();

        const auto& fixingDates = coupon_->fixingDates();
        const auto& interestDates = coupon_->interestDates();
        const auto& dt = coupon_->dt();

        Size i = 0;
        const Size n = determineNumberOfFixings(interestDates, date);

        Real accumulatedRate = 0.0;

        // already fixed part
        Date today = Settings::instance().evaluationDate();
        // As in compute(), the history is read only if the first fixing is due by today.
        const TimeSeries<Real>* pastFixings = nullptr;
        if (n > 0 && fixingDates[0] <= today)
            pastFixings = &index->timeSeries();
        while (i < n && fixingDates[i] < today)
        {
            // rate must have been fixed
            Rate pastFixing = (*pastFixings)[fixingDates[i]];
            QL_REQUIRE(pastFixing != Null<Real>(), "Missing " << index->name() << " fixing for " << fixingDates[i]);
            Time span =
                (date >= interestDates[i + 1] ? dt[i] : index->dayCounter().yearFraction(interestDates[i], date));
            accumulatedRate += pastFixing * span;
            ++i;
        }

        // today is a border case: a fixing stored for today is taken, and a missing one is forecast below unless
        // Settings::enforcesTodaysHistoricFixings() requires it, as InterestRateIndex::fixing does
        if (i < n && fixingDates[i] == today)
        {
            Rate pastFixing = (*pastFixings)[fixingDates[i]];
            if (pastFixing != Null<Real>())
            {
                Time span =
                    (date >= interestDates[i + 1] ? dt[i] : index->dayCounter().yearFraction(interestDates[i], date));
                accumulatedRate += pastFixing * span;
                ++i;
            }
            else
            {
                QL_REQUIRE(!Settings::instance().enforcesTodaysHistoricFixings(),
                           "Missing " << index->name() << " fixing for " << fixingDates[i]);
            }
        }

        // forward part
        if (i < n)
        {
            Handle<YieldTermStructure> curve = index->forwardingTermStructure();
            QL_REQUIRE(!curve.empty(), "null term structure set to this instance of " << index->name());

            const auto& dates = coupon_->valueDates();
            const Time te = curve->timeFromReference(dates[n]);
            // a fixing's forecast, accrued over its period, or up to the date in the period that contains it, as a
            // known fixing is
            const auto forecast = [&](Size k)
            {
                // forcast fixing
                Rate forecastFixing = index->fixing(fixingDates[k]);
                Time ti1 = curve->timeFromReference(dates[k]);
                Time ti2 = curve->timeFromReference(dates[k + 1]);
                /*convexity adjustment due to payment dalay of each
                overnight fixing, supposing an Hull-White short rate model*/
                // exp(vol^2 / (2 a^3) (exp(2 a ti1) - 1) (exp(-a ti2) - exp(-a te)) (exp(-a ti2) - exp(-a ti1))),
                // whose differences cancel at small a, written without them
                Real convAdj = std::exp(-vol_ * vol_ * detail::decayIntegral(2.0 * mrs_, ti1) *
                                        detail::decayIntegral(mrs_, te - ti2) * detail::decayIntegral(mrs_, ti2 - ti1) *
                                        std::exp(-mrs_ * (ti2 - ti1)));
                Real accrued = convAdj * (1 + forecastFixing * dt[k]) - 1;
                // the period that contains the date accrues only up to it, as a known fixing does
                if (date < interestDates[k + 1])
                    accrued *= index->dayCounter().yearFraction(interestDates[k], date) / dt[k];
                return accrued;
            };

            if (byApprox_)
            {
                /* forward part using telescopic property in order
                to avoid the evaluation of multiple forward fixings
                (approximation proposed by Katsumi Takada)*/
                // A first period starting at a rate computation start that isn't a fixing day is forecast as without
                // the approximation: its fixing applies from the start, while the discount factors would take it from
                // its value date, before it.
                if (i == 0 && !index->fixingCalendar().isBusinessDay(interestDates[0]))
                {
                    accumulatedRate += forecast(0);
                    ++i;
                }
            }
            if (byApprox_ && i < n)
            {
                // The forecast runs up to the date, or the rate computation end before it, as the known fixings do:
                // it's telescoped up to the last fixing day on or before that end, and where the end isn't a fixing
                // day, that day's forecast is accrued from it up to the end. At an interest date that's a fixing day,
                // that's up to dates[n].
                const Date end = std::min(date, interestDates[n]);
                const Date last = date < interestDates[n] || !index->fixingCalendar().isBusinessDay(interestDates[n]) ?
                                      index->fixingCalendar().adjust(end, Preceding) :
                                      dates[n];
                DiscountFactor startDiscount = curve->discount(dates[i]);
                DiscountFactor endDiscount = curve->discount(last);

                accumulatedRate += log(startDiscount / endDiscount) -
                                   convAdj1(curve->timeFromReference(dates[i]), curve->timeFromReference(last)) -
                                   convAdj2(curve->timeFromReference(dates[i]), curve->timeFromReference(last));
                if (last < end)
                    accumulatedRate +=
                        index->fixing(index->fixingDate(last)) * index->dayCounter().yearFraction(last, end);
            }
            // otherwise
            else
            {
                while (i < n)
                {
                    accumulatedRate += forecast(i);
                    ++i;
                }
            }
        }

        // as in compute(), no fixing is due by a date at or before the rate computation start
        Rate rate = n > 0 ? accumulatedRate / ratePeriodOnIndexBasis(*coupon_, date) : 0.0;
        return coupon_->gearing() * rate + coupon_->spread();
    }

    Real ArithmeticAveragedOvernightIndexedCouponPricer::convAdj1(Time ts, Time te) const
    {
        // vol^2 / (4 a^3) (1 - exp(-2 a ts)) (1 - exp(-a (te - ts)))^2, whose differences cancel at small a
        const Real B = detail::decayIntegral(mrs_, te - ts);
        return 0.5 * vol_ * vol_ * detail::decayIntegral(2.0 * mrs_, ts) * B * B;
    }

    Real ArithmeticAveragedOvernightIndexedCouponPricer::convAdj2(Time ts, Time te) const
    {
        // vol^2 / (2 a^2) (d - (1 - exp(-a d))^2 / a - (1 - exp(-2 a d)) / (2 a)) with d = te - ts, which
        // subtracts terms of order one and divides what's left by a^3, is vol^2 d^3 h(a d) / 4 with h as in
        // the Vasicek model
        const Time d = te - ts;
        Real phi, h;
        detail::decayPhiAndH(mrs_ * d, phi, h);
        return 0.25 * vol_ * vol_ * d * d * d * h;
    }
}
