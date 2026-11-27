/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 Quaternion Risk Management Ltd

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.


 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/
#include <ql/cashflows/blackovernightindexedcouponpricer.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <utility>

namespace QuantLib
{

    BlackCompoundingOvernightIndexedCouponPricer::BlackCompoundingOvernightIndexedCouponPricer(
        Handle<OptionletVolatilityStructure> v, const bool effectiveVolatilityInput)
    : CompoundingOvernightIndexedCouponPricer(std::move(v), effectiveVolatilityInput)
    {
    }

    void BlackCompoundingOvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon)
    {
        // What this sets depends on the coupon only through overnightIndexedCoupon() and gearing(), which a
        // CappedFlooredOvernightIndexedCoupon takes from its underlying coupon; the capped coupon's performCalculations
        // relies on that when it takes the pricer to hold what initializing it on the capped coupon would give.
        const OvernightIndexedCoupon* onCoupon = coupon.overnightIndexedCoupon();
        QL_REQUIRE(onCoupon == nullptr || onCoupon->averagingMethod() == RateAveraging::Compound,
                   "Averaging method required to be compound for BlackCompoundingOvernightIndexedCouponPricer");
        OvernightIndexedCouponPricer::initialize(coupon);

        gearing_ = coupon.gearing();
        std::tie(swapletRate_, effectiveSpread_, effectiveIndexFixing_) =
            CompoundingOvernightIndexedCouponPricer::compute(coupon_->accrualEndDate());
        effectiveCapletVolatility_ = effectiveFloorletVolatility_ = Null<Real>();
    }

    Real BlackCompoundingOvernightIndexedCouponPricer::optionletRateGlobal(Option::Type optionType,
                                                                           Real effStrike) const
    {
        Date lastRelevantFixingDate = coupon_->fixingDate();
        if (lastRelevantFixingDate <= Settings::instance().evaluationDate())
        {
            // the amount is determined
            Real a, b;
            if (optionType == Option::Call)
            {
                a = effectiveIndexFixing_;
                b = effStrike;
            }
            else
            {
                a = effStrike;
                b = effectiveIndexFixing_;
            }
            return gearing_ * std::max(a - b, 0.0);
        }
        else
        {
            // not yet determined, use Black model
            QL_REQUIRE(!capletVolatility().empty(),
                       "BlackCompoundingOvernightIndexedCouponPricer: missing optionlet volatility");
            const std::vector<Date>& fixingDates = coupon_->fixingDates();
            QL_REQUIRE(!fixingDates.empty(), "BlackCompoundingOvernightIndexedCouponPricer: empty fixing dates");
            bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
            Real shift = capletVolatility()->displacement();
            Real stdDev;
            Real effectiveTime = capletVolatility()->timeFromReference(fixingDates.back());
            if (effectiveVolatilityInput())
            {
                // vol input is effective, i.e. we use a plain black model
                stdDev = capletVolatility()->volatility(fixingDates.back(), effStrike) * std::sqrt(effectiveTime);
            }
            else
            {
                // vol input is not effective:
                // for the standard deviation see Lyashenko, Mercurio, Looking forward to backward looking rates,
                // section 6.3. the idea is to dampen the average volatility sigma between the fixing start and fixing
                // end date by a linear function going from (fixing start, 1) to (fixing end, 0)
                Real fixingStartTime = capletVolatility()->timeFromReference(fixingDates.front());
                Real fixingEndTime = capletVolatility()->timeFromReference(fixingDates.back());
                Real sigma = capletVolatility()->volatility(
                    std::max(fixingDates.front(), capletVolatility()->referenceDate() + 1), effStrike);
                Real T = std::max(fixingStartTime, 0.0);
                if (!close_enough(fixingEndTime, T))
                    T += std::pow(fixingEndTime - T, 3.0) / std::pow(fixingEndTime - fixingStartTime, 2.0) / 3.0;
                stdDev = sigma * std::sqrt(T);
            }
            if (optionType == Option::Type::Call)
                effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
            else
                effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);
            Real fixing = shiftedLn ? blackFormula(optionType, effStrike, effectiveIndexFixing_, stdDev, 1.0, shift) :
                                      bachelierBlackFormula(optionType, effStrike, effectiveIndexFixing_, stdDev, 1.0);
            return gearing_ * fixing;
        }
    }

    namespace
    {
        Real cappedFlooredRate(Real r, Option::Type optionType, Real k)
        {
            if (optionType == Option::Call)
            {
                return std::min(r, k);
            }
            else
            {
                return std::max(r, k);
            }
        }
    } // namespace

    Real BlackCompoundingOvernightIndexedCouponPricer::optionletRateLocal(Option::Type optionType, Real effStrike) const
    {

        QL_REQUIRE(
            !effectiveVolatilityInput(),
            "BlackAverageONIndexedCouponPricer::optionletRateLocal() does not support effective volatility input.");

        // We compute a rate and a rawRate such that
        // rate * the coupon's accrual period * nominal is its amount with daily capped / floored rates
        // rawRate * the coupon's accrual period * nominal is its amount without capping / flooring the rate
        // We will then return the difference between rate and rawRate (with the correct sign, see below)
        // as the option component of the coupon.

        // See CappedFlooredOvernightIndexedCoupon::effectiveCap(), Floor() for what is passed in as effStrike.
        // From this we back out the absolute strike at which the
        // - daily rate + spread (spread included) or the
        // - daily rate (spread excluded)
        // is capped / floored.

        Real absStrike = coupon_->compoundSpreadDaily() ? effStrike + coupon_->spread() : effStrike;

        // This following code is inevitably quite similar to the plain ON coupon pricer code, possibly we can refactor
        // this, but as a first step it seems safer to add the full modified code explicitly here and leave the original
        // code alone.

        const ext::shared_ptr<OvernightIndex>& index = coupon_->overnightIndex();

        const std::vector<Date>& fixingDates = coupon_->fixingDates();
        const std::vector<Time>& dt = coupon_->dt();
        const std::vector<Date>& interestDates = coupon_->interestDates();
        const Date& accrualEnd = coupon_->accrualEndDate();
        // Each fixing is applied as the coupon's own rate applies it: with observation shift and a lookback, over its
        // period between value dates; otherwise over its interest days, the last only up to the accrual end, which its
        // period runs past where the end isn't a fixing day.
        const bool shiftsObservation = coupon_->applyObservationShift() && coupon_->fixingDays() > 0;
        const auto span = [&](Size j)
        {
            return shiftsObservation || accrualEnd >= interestDates[j + 1] ?
                       dt[j] :
                       index->dayCounter().yearFraction(interestDates[j], accrualEnd);
        };

        Size n = dt.size();
        Size i = 0;
        QL_REQUIRE(coupon_->lockoutDays() < n, "rate cutoff (" << coupon_->lockoutDays()
                                                               << ") must be less than number of fixings in period ("
                                                               << n << ")");
        Size nCutoff = n - coupon_->lockoutDays();

        Real compoundFactor = 1.0, compoundFactorRaw = 1.0;

        // already fixed part
        Date today = Settings::instance().evaluationDate();
        // As the plain pricers do for the coupon's swaplet, the loops below read the index's stored history, and only
        // at fixings due by today, starting from the first, so a coupon whose first fixing is after today doesn't
        // look it up.
        const TimeSeries<Real>* pastFixings = nullptr;
        if (n > 0 && fixingDates[0] <= today)
            pastFixings = &index->timeSeries();
        while (i < n && fixingDates[std::min(i, nCutoff)] < today)
        {
            // rate must have been fixed
            Rate pastFixing = (*pastFixings)[fixingDates[std::min(i, nCutoff)]];
            QL_REQUIRE(pastFixing != Null<Real>(),
                       "Missing " << index->name() << " fixing for " << fixingDates[std::min(i, nCutoff)]);
            if (coupon_->compoundSpreadDaily())
            {
                pastFixing += coupon_->spread();
            }
            const Time t = span(i);
            compoundFactor *= 1.0 + cappedFlooredRate(pastFixing, optionType, absStrike) * t;
            compoundFactorRaw *= 1.0 + pastFixing * t;
            ++i;
        }

        // today is a border case: a fixing stored for today is taken at each fixing date that is today, as a
        // lockout repeats it, and a missing one is forecast below unless Settings::enforcesTodaysHistoricFixings()
        // requires it, as InterestRateIndex::fixing does
        while (i < n && fixingDates[std::min(i, nCutoff)] == today)
        {
            Rate pastFixing = (*pastFixings)[today];
            if (pastFixing != Null<Real>())
            {
                if (coupon_->compoundSpreadDaily())
                {
                    pastFixing += coupon_->spread();
                }
                const Time t = span(i);
                compoundFactor *= 1.0 + cappedFlooredRate(pastFixing, optionType, absStrike) * t;
                compoundFactorRaw *= 1.0 + pastFixing * t;
                ++i;
            }
            else
            {
                QL_REQUIRE(!Settings::instance().enforcesTodaysHistoricFixings(),
                           "Missing " << index->name() << " fixing for " << today);
                break;
            }
        }

        // forward part, approximation by pricing a cap / floor in the middle of the future period
        const std::vector<Date>& dates = coupon_->valueDates();
        if (i < n)
        {
            Handle<YieldTermStructure> curve = index->forwardingTermStructure();
            QL_REQUIRE(!curve.empty(), "null term structure set to this instance of " << index->name());

            DiscountFactor startDiscount = curve->discount(dates[i]);
            DiscountFactor endDiscount = curve->discount(dates[std::max(nCutoff, i)]);

            // handle the rate cutoff period (if there is any, i.e. if nCutoff < n)
            if (nCutoff < n)
            {
                // forward discount factor for one calendar day on the cutoff date
                DiscountFactor discountCutoffDate =
                    curve->discount(dates[nCutoff] + 1) / curve->discount(dates[nCutoff]);
                // keep the above forward discount factor constant during the cutoff period
                endDiscount *= std::pow(discountCutoffDate, dates[n] - dates[nCutoff]);
            }

            // estimate the average daily rate over the future period (approximate the continuously compounded rate),
            // on the index's basis, as the fixings and the strike are
            Real tau = index->dayCounter().yearFraction(dates[i], dates.back());
            Real averageRate = -std::log(endDiscount / startDiscount) / tau;

            // compute the value of a cap or floor with fixing in the middle of the future period
            // (but accounting for the rate cutoff here)
            Time midPoint = (capletVolatility()->timeFromReference(dates[i]) +
                             capletVolatility()->timeFromReference(dates[nCutoff])) /
                            2.0;
            Real stdDev = capletVolatility()->volatility(midPoint, effStrike) * std::sqrt(midPoint);
            Real shift = capletVolatility()->displacement();
            bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
            Rate cfValue = shiftedLn ? blackFormula(optionType, effStrike, averageRate, stdDev, 1.0, shift) :
                                       bachelierBlackFormula(optionType, effStrike, averageRate, stdDev, 1.0);
            Real effectiveTime = capletVolatility()->timeFromReference(fixingDates.back());
            if (optionType == Option::Type::Call)
                effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
            else
                effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);

            // add spread to average rate
            if (coupon_->compoundSpreadDaily())
            {
                averageRate += coupon_->spread();
            }

            // incorporate cap/floor into average rate
            Real averageRateRaw = averageRate;
            averageRate += optionType == Option::Call ? (-cfValue) : cfValue;

            // now assume the averageRate is the effective rate over the future period and update the compoundFactor
            // over the days the coupon applies those fixings for, as above: with observation shift and a lookback, from
            // the first one's value date to the last value date; otherwise from its interest date to the accrual end
            // this is an approximation, see "Ester / Daily Spread Curve Setup in ORE": set tau to avg value
            const Date from = shiftsObservation ? dates[i] : interestDates[i];
            const Date to = shiftsObservation ? dates.back() : accrualEnd;
            Real dailyTau = index->dayCounter().yearFraction(from, to) / (to - from);
            // now use formula (4) from the paper
            compoundFactor *= std::pow(1.0 + dailyTau * averageRate, static_cast<int>(to - from));
            compoundFactorRaw *= std::pow(1.0 + dailyTau * averageRateRaw, static_cast<int>(to - from));
        }

        // the rates are on the index's basis; the coupon's day counter scales its amount. They annualise over the
        // period the coupon's own rate does: with observation shift and a lookback the fixings compound over the
        // observation period, between the value dates, and so annualise over it; otherwise over the accrual period
        Real tau = shiftsObservation ? index->dayCounter().yearFraction(dates.front(), dates.back()) :
                                       index->dayCounter().yearFraction(coupon_->accrualStartDate(), accrualEnd);
        Rate rate = (compoundFactor - 1.0) / tau;
        Rate rawRate = (compoundFactorRaw - 1.0) / tau;

        rate *= coupon_->gearing();
        rawRate *= coupon_->gearing();

        if (!coupon_->compoundSpreadDaily())
        {
            rate += coupon_->spread();
            rawRate += coupon_->spread();
        }

        // return optionletRate := r - rawRate, i.e. the option component only
        // (see CappedFlooredOvernightIndexedCoupon::rate() for the signs of the capletRate / flooredRate)

        return (optionType == Option::Call ? -1.0 : 1.0) * (rate - rawRate);
    }

    Rate BlackCompoundingOvernightIndexedCouponPricer::swapletRate() const
    {
        return swapletRate_;
    }

    Rate BlackCompoundingOvernightIndexedCouponPricer::capletRate(Rate effectiveCap) const
    {
        return capletRate(effectiveCap, false);
    }

    Rate BlackCompoundingOvernightIndexedCouponPricer::floorletRate(Rate effectiveFloor) const
    {
        return floorletRate(effectiveFloor, false);
    }

    Rate BlackCompoundingOvernightIndexedCouponPricer::capletRate(Rate effectiveCap, bool dailyCapFloor) const
    {
        return dailyCapFloor ? optionletRateLocal(Option::Call, effectiveCap) :
                               optionletRateGlobal(Option::Call, effectiveCap);
    }

    Rate BlackCompoundingOvernightIndexedCouponPricer::floorletRate(Rate effectiveFloor, bool dailyCapFloor) const
    {
        return dailyCapFloor ? optionletRateLocal(Option::Put, effectiveFloor) :
                               optionletRateGlobal(Option::Put, effectiveFloor);
    }

    Real BlackCompoundingOvernightIndexedCouponPricer::swapletPrice() const
    {
        QL_FAIL("BlackCompoundingOvernightIndexedCouponPricer::swapletPrice() not provided");
    }
    Real BlackCompoundingOvernightIndexedCouponPricer::capletPrice(Rate effectiveCap) const
    {
        QL_FAIL("BlackCompoundingOvernightIndexedCouponPricer::capletPrice() not provided");
    }
    Real BlackCompoundingOvernightIndexedCouponPricer::floorletPrice(Rate effectiveFloor) const
    {
        QL_FAIL("BlackCompoundingOvernightIndexedCouponPricer::floorletPrice() not provided");
    }

    BlackAveragingOvernightIndexedCouponPricer::BlackAveragingOvernightIndexedCouponPricer(
        Handle<OptionletVolatilityStructure> v, const bool effectiveVolatilityInput)
    : ArithmeticAveragedOvernightIndexedCouponPricer(0.03, 0.0, false, std::move(v), effectiveVolatilityInput)
    {
    }

    void BlackAveragingOvernightIndexedCouponPricer::initialize(const FloatingRateCoupon& coupon)
    {
        // What this sets depends on the coupon only through overnightIndexedCoupon() and gearing(), which a
        // CappedFlooredOvernightIndexedCoupon takes from its underlying coupon; the capped coupon's performCalculations
        // relies on that when it takes the pricer to hold what initializing it on the capped coupon would give.
        const OvernightIndexedCoupon* onCoupon = coupon.overnightIndexedCoupon();
        QL_REQUIRE(onCoupon == nullptr || onCoupon->averagingMethod() == RateAveraging::Simple,
                   "Averaging method required to be simple for BlackAveragingOvernightIndexedCouponPricer");
        OvernightIndexedCouponPricer::initialize(coupon);

        gearing_ = coupon.gearing();
        swapletRate_ = ArithmeticAveragedOvernightIndexedCouponPricer::swapletRate();
        forwardRate_ = (swapletRate_ - coupon_->spread()) / coupon_->gearing();
        effectiveCapletVolatility_ = effectiveFloorletVolatility_ = Null<Real>();
    }

    Real BlackAveragingOvernightIndexedCouponPricer::optionletRateGlobal(Option::Type optionType, Real effStrike) const
    {
        Date lastRelevantFixingDate = coupon_->fixingDate();
        if (lastRelevantFixingDate <= Settings::instance().evaluationDate())
        {
            // the amount is determined
            Real a, b;
            if (optionType == Option::Call)
            {
                a = forwardRate_;
                b = effStrike;
            }
            else
            {
                a = effStrike;
                b = forwardRate_;
            }
            return gearing_ * std::max(a - b, 0.0);
        }
        else
        {
            // not yet determined, use Black model
            QL_REQUIRE(!capletVolatility().empty(),
                       "BlackAveragingOvernightIndexedCouponPricer: missing optionlet volatility");
            const std::vector<Date>& fixingDates = coupon_->fixingDates();
            QL_REQUIRE(!fixingDates.empty(), "BlackAveragingOvernightIndexedCouponPricer: empty fixing dates");
            bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
            Real shift = capletVolatility()->displacement();
            Real stdDev;
            Real effectiveTime = capletVolatility()->timeFromReference(fixingDates.back());
            if (effectiveVolatilityInput())
            {
                // vol input is effective, i.e. we use a plain black model
                stdDev = capletVolatility()->volatility(fixingDates.back(), effStrike) * std::sqrt(effectiveTime);
            }
            else
            {
                // vol input is not effective:
                // for the standard deviation see Lyashenko, Mercurio, Looking forward to backward looking rates,
                // section 6.3. the idea is to dampen the average volatility sigma between the fixing start and fixing
                // end date by a linear function going from (fixing start, 1) to (fixing end, 0)
                Real fixingStartTime = capletVolatility()->timeFromReference(fixingDates.front());
                Real fixingEndTime = capletVolatility()->timeFromReference(fixingDates.back());
                Real sigma = capletVolatility()->volatility(
                    std::max(fixingDates.front(), capletVolatility()->referenceDate() + 1), effStrike);
                Real T = std::max(fixingStartTime, 0.0);
                if (!close_enough(fixingEndTime, T))
                    T += std::pow(fixingEndTime - T, 3.0) / std::pow(fixingEndTime - fixingStartTime, 2.0) / 3.0;
                stdDev = sigma * std::sqrt(T);
            }
            if (optionType == Option::Type::Call)
                effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
            else
                effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);
            Real fixing = shiftedLn ? blackFormula(optionType, effStrike, forwardRate_, stdDev, 1.0, shift) :
                                      bachelierBlackFormula(optionType, effStrike, forwardRate_, stdDev, 1.0);
            return gearing_ * fixing;
        }
    }

    Real BlackAveragingOvernightIndexedCouponPricer::optionletRateLocal(Option::Type optionType, Real effStrike) const
    {

        QL_REQUIRE(!effectiveVolatilityInput(), "BlackAveragingOvernightIndexedCouponPricer::optionletRateLocal() does "
                                                "not support effective volatility input.");

        // We compute a rate and a rawRate such that
        // rate * the coupon's accrual period * nominal is its amount with daily capped / floored rates
        // rawRate * the coupon's accrual period * nominal is its amount without capping / flooring the rate
        // We will then return the difference between rate and rawRate (with the correct sign, see below)
        // as the option component of the coupon.

        // See CappedFlooredOvernightIndexedCoupon::effectiveCap(), Floor() for what is passed in as effStrike.
        // From this we back out the absolute strike at which the
        // - daily rate + spread (spread included) or the
        // - daily rate (spread excluded)
        // is capped / floored.

        Real absStrike = coupon_->compoundSpreadDaily() ? effStrike + coupon_->spread() : effStrike;

        // This following code is inevitably quite similar to the plain ON coupon pricer code, possibly we can refactor
        // this, but as a first step it seems safer to add the full modified code explicitly here and leave the original
        // code alone.

        const ext::shared_ptr<OvernightIndex>& index = coupon_->overnightIndex();

        const std::vector<Date>& fixingDates = coupon_->fixingDates();
        const std::vector<Time>& dt = coupon_->dt();
        const std::vector<Date>& dates = coupon_->valueDates();
        const Date& accrualEnd = coupon_->accrualEndDate();
        // Each fixing is applied as the coupon's own rate applies it, over its period between value dates, which are an
        // averaged coupon's interest dates, the last only up to the accrual end, which its period runs past where the
        // end isn't a fixing day.
        const auto span = [&](Size j)
        { return accrualEnd >= dates[j + 1] ? dt[j] : index->dayCounter().yearFraction(dates[j], accrualEnd); };

        Size n = dt.size();
        Size i = 0;
        QL_REQUIRE(coupon_->lockoutDays() < n, "rate cutoff (" << coupon_->lockoutDays()
                                                               << ") must be less than number of fixings in period ("
                                                               << n << ")");
        Size nCutoff = n - coupon_->lockoutDays();

        Real accumulatedRate = 0.0, accumulatedRateRaw = 0.0;

        // already fixed part
        Date today = Settings::instance().evaluationDate();
        // As the plain pricers do for the coupon's swaplet, the loops below read the index's stored history, and only
        // at fixings due by today, starting from the first, so a coupon whose first fixing is after today doesn't
        // look it up.
        const TimeSeries<Real>* pastFixings = nullptr;
        if (n > 0 && fixingDates[0] <= today)
            pastFixings = &index->timeSeries();
        while (i < n && fixingDates[std::min(i, nCutoff)] < today)
        {
            // rate must have been fixed
            Rate pastFixing = (*pastFixings)[fixingDates[std::min(i, nCutoff)]];
            QL_REQUIRE(pastFixing != Null<Real>(),
                       "Missing " << index->name() << " fixing for " << fixingDates[std::min(i, nCutoff)]);
            if (coupon_->compoundSpreadDaily())
            {
                pastFixing += coupon_->spread();
            }
            const Time t = span(i);
            accumulatedRate += cappedFlooredRate(pastFixing, optionType, absStrike) * t;
            accumulatedRateRaw += pastFixing * t;
            ++i;
        }

        // today is a border case: a fixing stored for today is taken, and a missing one is forecast below unless
        // Settings::enforcesTodaysHistoricFixings() requires it, as InterestRateIndex::fixing does
        if (i < n && fixingDates[std::min(i, nCutoff)] == today)
        {
            Rate pastFixing = (*pastFixings)[today];
            if (pastFixing != Null<Real>())
            {
                if (coupon_->compoundSpreadDaily())
                {
                    pastFixing += coupon_->spread();
                }
                const Time t = span(i);
                accumulatedRate += cappedFlooredRate(pastFixing, optionType, absStrike) * t;
                accumulatedRateRaw += pastFixing * t;
                ++i;
            }
            else
            {
                QL_REQUIRE(!Settings::instance().enforcesTodaysHistoricFixings(),
                           "Missing " << index->name() << " fixing for " << today);
            }
        }

        // forward part, approximation by pricing a cap / floor in the middle of the future period
        if (i < n)
        {
            Handle<YieldTermStructure> curve = index->forwardingTermStructure();
            QL_REQUIRE(!curve.empty(), "null term structure set to this instance of " << index->name());

            DiscountFactor startDiscount = curve->discount(dates[i]);
            DiscountFactor endDiscount = curve->discount(dates[std::max(nCutoff, i)]);

            // handle the rate cutoff period (if there is any, i.e. if nCutoff < n)
            if (nCutoff < n)
            {
                // forward discount factor for one calendar day on the cutoff date
                DiscountFactor discountCutoffDate =
                    curve->discount(dates[nCutoff] + 1) / curve->discount(dates[nCutoff]);
                // keep the above forward discount factor constant during the cutoff period
                endDiscount *= std::pow(discountCutoffDate, dates[n] - dates[nCutoff]);
            }

            // estimate the average daily rate over the future period (approximate the continuously compounded rate),
            // on the index's basis, as the fixings and the strike are
            Real tau = index->dayCounter().yearFraction(dates[i], dates.back());
            Real averageRate = -std::log(endDiscount / startDiscount) / tau;

            // compute the value of a cap or floor with fixing in the middle of the future period
            // (but accounting for the rate cutoff here)
            Time midPoint = (capletVolatility()->timeFromReference(dates[i]) +
                             capletVolatility()->timeFromReference(dates[nCutoff])) /
                            2.0;
            Real stdDev = capletVolatility()->volatility(midPoint, effStrike) * std::sqrt(midPoint);
            Real shift = capletVolatility()->displacement();
            bool shiftedLn = capletVolatility()->volatilityType() == ShiftedLognormal;
            Rate cfValue = shiftedLn ? blackFormula(optionType, effStrike, averageRate, stdDev, 1.0, shift) :
                                       bachelierBlackFormula(optionType, effStrike, averageRate, stdDev, 1.0);

            Real effectiveTime = capletVolatility()->timeFromReference(fixingDates.back());
            if (optionType == Option::Type::Call)
                effectiveCapletVolatility_ = stdDev / std::sqrt(effectiveTime);
            else
                effectiveFloorletVolatility_ = stdDev / std::sqrt(effectiveTime);

            // add spread to average rate
            if (coupon_->compoundSpreadDaily())
            {
                averageRate += coupon_->spread();
            }

            // incorporate cap/floor into average rate
            Real averageRateRaw = averageRate;
            averageRate += optionType == Option::Call ? (-cfValue) : cfValue;

            // now assume the averageRate is the effective rate over the future period and update the average rate
            // over the days the coupon applies those fixings for, as above, from the first one's value date to the
            // accrual end
            // this is an approximation, see "Ester / Daily Spread Curve Setup in ORE": set tau to avg value
            Real dailyTau = index->dayCounter().yearFraction(dates[i], accrualEnd) / (accrualEnd - dates[i]);
            accumulatedRate += dailyTau * averageRate * static_cast<Real>(accrualEnd - dates[i]);
            accumulatedRateRaw += dailyTau * averageRateRaw * static_cast<Real>(accrualEnd - dates[i]);
        }

        // the rates are on the index's basis; the coupon's day counter scales its amount. They annualise over the
        // accrual period, as the coupon's own rate does
        Rate tau = index->dayCounter().yearFraction(coupon_->accrualStartDate(), accrualEnd);
        Rate rate = accumulatedRate / tau;
        Rate rawRate = accumulatedRateRaw / tau;

        rate *= coupon_->gearing();
        rawRate *= coupon_->gearing();

        if (!coupon_->compoundSpreadDaily())
        {
            rate += coupon_->spread();
            rawRate += coupon_->spread();
        }

        // return optionletRate := r - rawRate, i.e. the option component only
        // (see CappedFlooredAverageONIndexedCoupon::rate() for the signs of the capletRate / flooredRate)

        return (optionType == Option::Call ? -1.0 : 1.0) * (rate - rawRate);
    }

    Rate BlackAveragingOvernightIndexedCouponPricer::swapletRate() const
    {
        return swapletRate_;
    }

    Rate BlackAveragingOvernightIndexedCouponPricer::capletRate(Rate effectiveCap) const
    {
        return capletRate(effectiveCap, false);
    }

    Rate BlackAveragingOvernightIndexedCouponPricer::floorletRate(Rate effectiveFloor) const
    {
        return floorletRate(effectiveFloor, false);
    }

    Rate BlackAveragingOvernightIndexedCouponPricer::capletRate(Rate effectiveCap, bool dailyCapFloor) const
    {
        return dailyCapFloor ? optionletRateLocal(Option::Call, effectiveCap) :
                               optionletRateGlobal(Option::Call, effectiveCap);
    }

    Rate BlackAveragingOvernightIndexedCouponPricer::floorletRate(Rate effectiveFloor, bool dailyCapFloor) const
    {
        return dailyCapFloor ? optionletRateLocal(Option::Put, effectiveFloor) :
                               optionletRateGlobal(Option::Put, effectiveFloor);
    }

    Real BlackAveragingOvernightIndexedCouponPricer::swapletPrice() const
    {
        QL_FAIL("BlackAveragingOvernightIndexedCouponPricer::swapletPrice() not provided");
    }

    Real BlackAveragingOvernightIndexedCouponPricer::capletPrice(Rate effectiveCap) const
    {
        QL_FAIL("BlackAveragingOvernightIndexedCouponPricer::capletPrice() not provided");
    }

    Real BlackAveragingOvernightIndexedCouponPricer::floorletPrice(Rate effectiveFloor) const
    {
        QL_FAIL("BlackAveragingOvernightIndexedCouponPricer::floorletPrice() not provided");
    }

}
