/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
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

#include <ql/cashflows/blackovernightindexedcouponpricer.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/overnightindexedcouponpricer.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/utilities/vectors.hpp>
#include <algorithm>
#include <type_traits>
#include <typeinfo>
#include <utility>

using std::vector;

namespace QuantLib
{

    namespace
    {
        Date applyLookbackPeriod(const ext::shared_ptr<InterestRateIndex>& index,
                                 const Date& valueDate,
                                 Natural lookbackDays)
        {
            return index->fixingCalendar().advance(valueDate, -static_cast<Integer>(lookbackDays), Days);
        }

        // The overnight pricer a capped or floored coupon prices with: a Black pricer, the pricers that price caps
        // and floors, is told by its exact type, and any other pricer by a cast. black says which it was.
        OvernightIndexedCouponPricer* overnightPricerOf(FloatingRateCouponPricer* pricer, bool& black)
        {
            black = false;
            if (pricer == nullptr)
                return nullptr;
            const std::type_info& type = typeid(*pricer);
            if (type == typeid(BlackCompoundingOvernightIndexedCouponPricer) ||
                type == typeid(BlackAveragingOvernightIndexedCouponPricer))
            {
                black = true;
                return static_cast<OvernightIndexedCouponPricer*>(pricer);
            }
            return dynamic_cast<OvernightIndexedCouponPricer*>(pricer);
        }
    }

    OvernightIndexedCoupon::OvernightIndexedCoupon(const Date& paymentDate,
                                                   Real nominal,
                                                   const Date& startDate,
                                                   const Date& endDate,
                                                   const ext::shared_ptr<OvernightIndex>& overnightIndex,
                                                   Real gearing,
                                                   Spread spread,
                                                   const Date& refPeriodStart,
                                                   const Date& refPeriodEnd,
                                                   const DayCounter& dayCounter,
                                                   bool telescopicValueDates,
                                                   RateAveraging::Type averagingMethod,
                                                   Natural lookbackDays,
                                                   Natural lockoutDays,
                                                   bool applyObservationShift,
                                                   bool compoundSpreadDaily,
                                                   const Date& rateComputationStartDate,
                                                   const Date& rateComputationEndDate)
    : FloatingRateCoupon(paymentDate,
                         nominal,
                         startDate,
                         endDate,
                         lookbackDays,
                         overnightIndex,
                         gearing,
                         spread,
                         refPeriodStart,
                         refPeriodEnd,
                         dayCounter,
                         false),
      overnightIndex_(overnightIndex), averagingMethod_(averagingMethod), lockoutDays_(lockoutDays),
      applyObservationShift_(applyObservationShift), compoundSpreadDaily_(compoundSpreadDaily),
      rateComputationStartDate_(rateComputationStartDate), rateComputationEndDate_(rateComputationEndDate)
    {

        // ctor guard prevents construction of an object with illogically ordered dates.
        QL_REQUIRE(paymentDate >= endDate, "Payment date cannot be earlier than accrual end date");

        // The rate is worked out over the rate computation period: the accrual period, unless the coupon is given
        // another, as OvernightLeg gives a coupon fixed in advance (the previous period) or one whose rate is taken
        // over a last recent period.
        const Date rateStart = rateComputationStartDate_ == Null<Date>() ? startDate : rateComputationStartDate_;
        const Date rateEnd = rateComputationEndDate_ == Null<Date>() ? endDate : rateComputationEndDate_;
        QL_REQUIRE(rateStart < rateEnd, "rate computation start date ("
                                            << rateStart << ") must be earlier than its end date (" << rateEnd << ")");
        QL_REQUIRE(rateEnd <= endDate, "rate computation end date ("
                                           << rateEnd << ") must not be later than the accrual end date (" << endDate
                                           << ")");

        // value dates
        Date tmpEndDate = rateEnd;

        /* For the coupon's valuation only the first and last future valuation
           dates matter, therefore we can avoid to construct the whole series
           of valuation dates, a front and back stub will do. However notice
           that if the global evaluation date moves forward it might run past
           the front stub of valuation dates we build here (which incorporates
           a grace period of 7 business after the evaluation date). This will
           lead to false coupon projections (see the warning the class header). */

        QL_REQUIRE(canApplyTelescopicFormula() || !telescopicValueDates,
                   "Telescopic formula cannot be applied for a coupon with lookback.");

        // A nonzero spread compounded daily compounds with each day's fixing, so
        // such a compounded coupon is priced day by day and needs all its value dates.
        const bool telescopic = telescopicValueDates &&
                                !(averagingMethod == RateAveraging::Compound && compoundSpreadDaily && spread != 0.0);

        if (telescopic)
        {
            // build optimised value dates schedule: front stub goes
            // from rateStart to max(evalDate, rateStart) + 7bd
            Date evalDate = Settings::instance().evaluationDate();
            tmpEndDate = overnightIndex->fixingCalendar().advance(std::max(rateStart, evalDate), 7, Days, Following);
            tmpEndDate = std::min(tmpEndDate, rateEnd);
        }
        // A start that isn't a fixing day takes the fixing of the business day before it, which applies from the
        // start to the next business day; with observation shift and a lookback, the observation period starts the
        // lookback's business days before the first business day on or after the start.
        const Date firstValueDate = overnightIndex->fixingCalendar().adjust(
            rateStart, applyObservationShift_ && fixingDays_ > 0 ? Following : Preceding);
        Schedule sch = MakeSchedule()
                           .from(firstValueDate)
                           // .to(endDate)
                           .to(tmpEndDate)
                           .withTenor(1 * Days)
                           .withCalendar(overnightIndex->fixingCalendar())
                           .withConvention(overnightIndex->businessDayConvention())
                           .backwards();
        valueDates_ = sch.dates();

        if (telescopic)
        {
            // the back stub covers the lockout period and the business day
            // before it, whose fixing the lockout repeats
            tmpEndDate = overnightIndex->fixingCalendar().adjust(rateEnd, overnightIndex->businessDayConvention());
            Date tmpLockoutDate = overnightIndex->fixingCalendar().advance(
                rateEnd, -static_cast<Integer>(lockoutDays_) - 1, Days, Preceding);
            while (tmpLockoutDate <= tmpEndDate)
            {
                if (tmpLockoutDate > valueDates_.back())
                    valueDates_.push_back(tmpLockoutDate);
                tmpLockoutDate = overnightIndex->fixingCalendar().advance(tmpLockoutDate, 1, Days, Following);
            }
        }

        QL_ENSURE(valueDates_.size() >= 2, "degenerate schedule");

        n_ = valueDates_.size() - 1;

        // the interest dates are the value dates but the last, the rate computation end, up to which the last fixing
        // applies, and the first, the rate computation start, from which the first fixing applies; with observation
        // shift and a lookback, the first is the first business day on or after the start, the day the first fixing is
        // observed for
        interestDates_ = valueDates_;
        if (!(applyObservationShift_ && fixingDays_ > 0))
            interestDates_.front() = rateStart;
        interestDates_.back() = rateEnd;

        if (fixingDays_ == overnightIndex->fixingDays() && fixingDays_ == 0)
        {
            fixingDates_ = vector<Date>(valueDates_.begin(), valueDates_.end() - 1);
        }
        else
        {
            // Lookback (fixing days):
            // The date that the fixing rate is pulled  from (the observation date) is k
            // business days before the date that interest is applied (the interest date).
            // Without observation shift it is applied for the number of calendar days
            // until the next business day following the interest date; with it, for the
            // days from its value date to the next observation date's (see the accrual
            // periods below). The interest dates are the rate computation period's either way.
            fixingDates_.resize(n_);
            for (Size i = 0; i <= n_; ++i)
            {
                Date tmp = applyLookbackPeriod(overnightIndex, valueDates_[i], fixingDays_);
                if (i < n_)
                    fixingDates_[i] = tmp;
                if (fixingDays_ != overnightIndex->fixingDays())
                    // If fixing dates of the coupon deviate from fixing days in the index
                    // we need to correct the value dates such that they reflect dates
                    // corresponding to a deposit instrument linked to the index.
                    // This is to ensure that future projections (which are computed
                    // based on the value dates) of the index do not
                    // yield any convexity corrections.
                    valueDates_[i] = overnightIndex->valueDate(tmp);
            }
        }
        // When lockout is used the fixing rate applied for the last k days of the
        // interest period is frozen at the rate observed k days before the period ends.
        if (lockoutDays_ != 0)
        {
            QL_REQUIRE(lockoutDays_ > 0 && lockoutDays_ < n_,
                       "Lockout period cannot be negative or exceed the number of fixing days.");
            Date lockoutDate = fixingDates_[n_ - 1 - lockoutDays_];
            for (Size i = n_ - 1; i > n_ - 1 - lockoutDays_; --i)
                fixingDates_[i] = lockoutDate;
        }

        // accrual (compounding) periods: between interest dates or, with observation shift
        // and a lookback, between value dates, the days the observed fixings are for, so
        // that the fixings compound over the observation period
        const vector<Date>& periodDates = applyObservationShift_ && fixingDays_ > 0 ? valueDates_ : interestDates_;
        dt_.resize(n_);
        const DayCounter& dc = overnightIndex->dayCounter();
        for (Size i = 0; i < n_; ++i)
            dt_[i] = dc.yearFraction(periodDates[i], periodDates[i + 1]);

        switch (averagingMethod)
        {
            case RateAveraging::Simple:
                QL_REQUIRE(fixingDays_ == overnightIndex->fixingDays() && !applyObservationShift_ && lockoutDays_ == 0,
                           "Cannot price an overnight coupon with simple averaging with lookback or lockout.");
                setPricer(ext::make_shared<ArithmeticAveragedOvernightIndexedCouponPricer>(telescopicValueDates));
                break;
            case RateAveraging::Compound:
                setPricer(ext::make_shared<CompoundingOvernightIndexedCouponPricer>());
                break;
            default:
                QL_FAIL("unknown compounding convention (" << Integer(averagingMethod) << ")");
        }
    }

    Real OvernightIndexedCoupon::accruedAmount(const Date& d) const
    {
        if (d <= accrualStartDate_ || d > paymentDate_)
        {
            // out of coupon range
            return 0.0;
        }
        else if (tradingExCoupon(d))
        {
            return nominal() * averageRate(d) * accruedPeriod(d);
        }
        else
        {
            // usual case
            return nominal() * averageRate(std::min(d, accrualEndDate_)) * accruedPeriod(d);
        }
    }

    Rate OvernightIndexedCoupon::averageRate(const Date& d) const
    {
        QL_REQUIRE(pricer_, "pricer not set");
        pricer_->initialize(*this);
        if (const auto overnightIndexedPricer = ext::dynamic_pointer_cast<OvernightIndexedCouponPricer>(pricer_))
        {
            return overnightIndexedPricer->averageRate(d);
        }
        return pricer_->swapletRate();
    }

    const vector<Rate>& OvernightIndexedCoupon::indexFixings() const
    {
        fixings_.resize(n_);
        for (Size i = 0; i < n_; ++i)
            fixings_[i] = index_->fixing(fixingDates_[i]);
        return fixings_;
    }

    void OvernightIndexedCoupon::accept(AcyclicVisitor& v)
    {
        auto* v1 = dynamic_cast<Visitor<OvernightIndexedCoupon>*>(&v);
        if (v1 != nullptr)
        {
            v1->visit(*this);
        }
        else
        {
            FloatingRateCoupon::accept(v);
        }
    }

    Real OvernightIndexedCoupon::effectiveSpread() const
    {
        if (!compoundSpreadDaily_)
            return spread();

        if (averagingMethod_ == RateAveraging::Simple)
            return spread();

        auto p = ext::dynamic_pointer_cast<CompoundingOvernightIndexedCouponPricer>(pricer());
        QL_REQUIRE(p, "OvernightIndexedCoupon::effectiveSpread() needs a CompoundingOvernightIndexedCouponPricer");
        p->initialize(*this);
        return p->effectiveSpread();
    }

    Real OvernightIndexedCoupon::effectiveIndexFixing() const
    {
        auto p = ext::dynamic_pointer_cast<CompoundingOvernightIndexedCouponPricer>(pricer());

        if (averagingMethod_ == RateAveraging::Simple)
            QL_FAIL("OvernightIndexedCoupon::effectiveIndexFixing(): only a compounded coupon has an effective index "
                    "fixing");

        QL_REQUIRE(p, "OvernightIndexedCoupon::effectiveIndexFixing() needs a CompoundingOvernightIndexedCouponPricer");
        p->initialize(*this);
        return p->effectiveIndexFixing();
    }

    // CappedFlooredOvernightIndexedCoupon implementation

    CappedFlooredOvernightIndexedCoupon::CappedFlooredOvernightIndexedCoupon(
        const ext::shared_ptr<OvernightIndexedCoupon>& underlying,
        Real cap,
        Real floor,
        bool nakedOption,
        bool dailyCapFloor)
    : FloatingRateCoupon(underlying->date(),
                         underlying->nominal(),
                         underlying->accrualStartDate(),
                         underlying->accrualEndDate(),
                         underlying->fixingDays(),
                         underlying->index(),
                         underlying->gearing(),
                         underlying->spread(),
                         underlying->referencePeriodStart(),
                         underlying->referencePeriodEnd(),
                         underlying->dayCounter(),
                         false),
      underlying_(underlying), nakedOption_(nakedOption), dailyCapFloor_(dailyCapFloor)
    {

        QL_REQUIRE(
            !underlying_->compoundSpreadDaily() || close_enough(underlying_->gearing(), 1.0),
            "CappedFlooredOvernightIndexedCoupon: if include spread = true, only a gearing 1.0 is allowed - scale "
            "the notional in this case instead.");

        if (!dailyCapFloor)
        {
            if (gearing_ > 0.0)
            {
                cap_ = cap;
                floor_ = floor;
            }
            else
            {
                cap_ = floor;
                floor_ = cap;
            }
        }
        else
        {
            cap_ = cap;
            floor_ = floor;
        }
        if (cap_ != Null<Real>() && floor_ != Null<Real>())
        {
            QL_REQUIRE(cap_ >= floor, "cap level (" << cap_ << ") less than floor level (" << floor_ << ")");
        }
        registerWith(underlying_);
        if (nakedOption_)
            underlying_->alwaysForwardNotifications();
    }

    void CappedFlooredOvernightIndexedCoupon::alwaysForwardNotifications()
    {
        LazyObject::alwaysForwardNotifications();
        underlying_->alwaysForwardNotifications();
    }

    void CappedFlooredOvernightIndexedCoupon::deepUpdate()
    {
        update();
        underlying_->deepUpdate();
    }

    void CappedFlooredOvernightIndexedCoupon::performCalculations() const
    {
        const ext::shared_ptr<FloatingRateCouponPricer> underlyingPricer = underlying_->pricer();
        QL_REQUIRE(underlyingPricer, "underlying coupon pricer not set");
        const bool underlyingWasCalculated = underlying_->isCalculated();
        Rate swapletRate = nakedOption_ ? 0.0 : underlying_->rate();
        bool blackPricer = false;
        OvernightIndexedCouponPricer* cfONPricer = overnightPricerOf(pricer_.get(), blackPricer);
        QL_REQUIRE(cfONPricer, "coupon pricer not an instance of OvernightIndexedCouponPricer");

        if (floor_ != Null<Real>() || cap_ != Null<Real>())
        {
            // The pricer holds what initializing it on this coupon would give where it is a Black pricer, the
            // underlying shares it and was calculated just now, the pricer's coupon is the underlying, and neither
            // coupon is of a derived class.
            const OvernightIndexedCoupon& underlyingCoupon = *underlying_;
            const bool initialized = blackPricer && !underlyingWasCalculated && underlyingCoupon.isCalculated() &&
                                     underlyingPricer.get() == pricer_.get() &&
                                     cfONPricer->coupon_ == &underlyingCoupon &&
                                     typeid(underlyingCoupon) == typeid(OvernightIndexedCoupon) &&
                                     typeid(*this) == typeid(CappedFlooredOvernightIndexedCoupon);
            if (!initialized)
                cfONPricer->initialize(*this);
        }
        Rate floorletRate = 0.;
        if (floor_ != Null<Real>())
            floorletRate = cfONPricer->floorletRate(effectiveFloor(), dailyCapFloor());
        Rate capletRate = 0.;
        if (cap_ != Null<Real>())
            capletRate = (nakedOption_ && floor_ == Null<Real>() ? -1.0 : 1.0) *
                         cfONPricer->capletRate(effectiveCap(), dailyCapFloor());
        rate_ = swapletRate + floorletRate - capletRate;

        effectiveCapletVolatility_ = cfONPricer->effectiveCapletVolatility();
        effectiveFloorletVolatility_ = cfONPricer->effectiveFloorletVolatility();
    }

    Rate CappedFlooredOvernightIndexedCoupon::cap() const
    {
        return gearing_ > 0.0 ? cap_ : floor_;
    }

    Rate CappedFlooredOvernightIndexedCoupon::floor() const
    {
        return gearing_ > 0.0 ? floor_ : cap_;
    }

    Rate CappedFlooredOvernightIndexedCoupon::rate() const
    {
        calculate();
        return rate_;
    }

    Rate CappedFlooredOvernightIndexedCoupon::convexityAdjustment() const
    {
        return underlying_->convexityAdjustment();
    }

    Rate CappedFlooredOvernightIndexedCoupon::effectiveCap() const
    {
        if (cap_ == Null<Real>())
            return Null<Real>();
        /* We have four cases dependent on dailyCapFloor_ and compoundSpreadDaily. Notation in the formulas:
        g         gearing,
        s         spread,
        A         coupon amount,
        f_i       daily fixings,
        \tau_i    daily accrual fractions,
        \tau      coupon accrual fraction,
        C         cap rate
        F         floor rate
        */
        if (dailyCapFloor_)
        {
            if (underlying_->compoundSpreadDaily())
            {
                // A = g \cdot \frac{\prod (1 + \tau_i \min ( \max ( f_i + s , F), C)) - 1}{\tau}
                return cap_ - underlying_->spread();
            }
            else
            {
                // A = g \cdot \frac{\prod (1 + \tau_i \min ( \max ( f_i , F), C)) - 1}{\tau} + s
                return cap_;
            }
        }
        else
        {
            if (underlying_->compoundSpreadDaily())
            {
                // A = \min \left( \max \left( g \cdot \frac{\prod (1 + \tau_i(f_i + s)) - 1}{\tau}, F \right), C
                // \right)
                return (cap_ / gearing() - underlying_->effectiveSpread());
            }
            else
            {
                // A = \min \left( \max \left( g \cdot \frac{\prod (1 + \tau_i f_i) - 1}{\tau} + s, F \right), C \right)
                return (cap_ - underlying_->effectiveSpread()) / gearing();
            }
        }
    }

    Rate CappedFlooredOvernightIndexedCoupon::effectiveFloor() const
    {
        if (floor_ == Null<Real>())
            return Null<Real>();
        if (dailyCapFloor_)
        {
            if (underlying_->compoundSpreadDaily())
            {
                return floor_ - underlying_->spread();
            }
            else
            {
                return floor_;
            }
        }
        else
        {
            if (underlying_->compoundSpreadDaily())
            {
                return (floor_ - underlying_->effectiveSpread());
            }
            else
            {
                return (floor_ - underlying_->effectiveSpread()) / gearing();
            }
        }
    }

    Real CappedFlooredOvernightIndexedCoupon::effectiveCapletVolatility() const
    {
        calculate();
        return effectiveCapletVolatility_;
    }

    Real CappedFlooredOvernightIndexedCoupon::effectiveFloorletVolatility() const
    {
        calculate();
        return effectiveFloorletVolatility_;
    }

    void CappedFlooredOvernightIndexedCoupon::accept(AcyclicVisitor& v)
    {
        auto* v1 = dynamic_cast<Visitor<CappedFlooredOvernightIndexedCoupon>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            FloatingRateCoupon::accept(v);
    }

    void CappedFlooredOvernightIndexedCoupon::setPricer(const ext::shared_ptr<FloatingRateCouponPricer>& pricer)
    {
        auto p = ext::dynamic_pointer_cast<OvernightIndexedCouponPricer>(pricer);
        QL_REQUIRE(p, "The pricer is required to be an instance of OvernightIndexedCouponPricer");
        FloatingRateCoupon::setPricer(p);
    }

    // OvernightLeg implementation

    OvernightLeg::OvernightLeg(Schedule schedule, const ext::shared_ptr<OvernightIndex>& i)
    : schedule_(std::move(schedule)), overnightIndex_(i), paymentCalendar_(schedule_.calendar())
    {
        QL_REQUIRE(overnightIndex_, "no index provided");
    }

    OvernightLeg& OvernightLeg::withNotionals(Real notional)
    {
        notionals_ = vector<Real>(1, notional);
        return *this;
    }

    OvernightLeg& OvernightLeg::withNotionals(const vector<Real>& notionals)
    {
        notionals_ = notionals;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentDayCounter(const DayCounter& dc)
    {
        paymentDayCounter_ = dc;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentAdjustment(BusinessDayConvention convention)
    {
        paymentAdjustment_ = convention;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentCalendar(const Calendar& cal)
    {
        paymentCalendar_ = cal;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentLag(Integer lag)
    {
        paymentLag_ = lag;
        return *this;
    }

    OvernightLeg& OvernightLeg::withGearings(Real gearing)
    {
        gearings_ = vector<Real>(1, gearing);
        return *this;
    }

    OvernightLeg& OvernightLeg::withGearings(const vector<Real>& gearings)
    {
        gearings_ = gearings;
        return *this;
    }

    OvernightLeg& OvernightLeg::withSpreads(Spread spread)
    {
        spreads_ = vector<Spread>(1, spread);
        return *this;
    }

    OvernightLeg& OvernightLeg::withSpreads(const vector<Spread>& spreads)
    {
        spreads_ = spreads;
        return *this;
    }

    OvernightLeg& OvernightLeg::withTelescopicValueDates(bool telescopicValueDates)
    {
        telescopicValueDates_ = telescopicValueDates;
        return *this;
    }

    OvernightLeg& OvernightLeg::withAveragingMethod(RateAveraging::Type averagingMethod)
    {
        averagingMethod_ = averagingMethod;
        return *this;
    }

    OvernightLeg& OvernightLeg::withLookbackDays(Natural lookbackDays)
    {
        lookbackDays_ = lookbackDays;
        return *this;
    }
    OvernightLeg& OvernightLeg::withLockoutDays(Natural lockoutDays)
    {
        lockoutDays_ = lockoutDays;
        return *this;
    }
    OvernightLeg& OvernightLeg::withObservationShift(bool applyObservationShift)
    {
        applyObservationShift_ = applyObservationShift;
        return *this;
    }

    OvernightLeg& OvernightLeg::compoundingSpreadDaily(bool compoundSpreadDaily)
    {
        compoundSpreadDaily_ = compoundSpreadDaily;
        return *this;
    }

    OvernightLeg& OvernightLeg::withCaps(Rate cap)
    {
        caps_ = std::vector<Rate>(1, cap);
        return *this;
    }

    OvernightLeg& OvernightLeg::withCaps(const std::vector<Rate>& caps)
    {
        caps_ = caps;
        return *this;
    }

    OvernightLeg& OvernightLeg::withFloors(Rate floor)
    {
        floors_ = std::vector<Rate>(1, floor);
        return *this;
    }

    OvernightLeg& OvernightLeg::withFloors(const std::vector<Rate>& floors)
    {
        floors_ = floors;
        return *this;
    }

    OvernightLeg& OvernightLeg::withNakedOption(const bool nakedOption)
    {
        nakedOption_ = nakedOption;
        return *this;
    }

    OvernightLeg& OvernightLeg::withDailyCapFloor(const bool dailyCapFloor)
    {
        dailyCapFloor_ = dailyCapFloor;
        return *this;
    }

    OvernightLeg& OvernightLeg::inArrears(const bool inArrears)
    {
        inArrears_ = inArrears;
        return *this;
    }

    OvernightLeg& OvernightLeg::withLastRecentPeriod(const ext::optional<Period>& lastRecentPeriod)
    {
        lastRecentPeriod_ = lastRecentPeriod;
        return *this;
    }

    OvernightLeg& OvernightLeg::withLastRecentPeriodCalendar(const Calendar& lastRecentPeriodCalendar)
    {
        lastRecentPeriodCalendar_ = lastRecentPeriodCalendar;
        return *this;
    }

    OvernightLeg& OvernightLeg::withPaymentDates(const std::vector<Date>& paymentDates)
    {
        paymentDates_ = paymentDates;
        return *this;
    }

    OvernightLeg& OvernightLeg::withCouponPricer(const ext::shared_ptr<OvernightIndexedCouponPricer>& couponPricer)
    {
        couponPricer_ = couponPricer;
        return *this;
    }

    OvernightLeg::operator Leg() const
    {

        QL_REQUIRE(!notionals_.empty(), "no notional given");

        if (couponPricer_ != nullptr)
        {
            if (averagingMethod_ == RateAveraging::Compound)
                QL_REQUIRE(ext::dynamic_pointer_cast<CompoundingOvernightIndexedCouponPricer>(couponPricer_),
                           "Wrong coupon pricer provided, provide a CompoundingOvernightIndexedCouponPricer");
            else
                QL_REQUIRE(ext::dynamic_pointer_cast<ArithmeticAveragedOvernightIndexedCouponPricer>(couponPricer_),
                           "Wrong coupon pricer provided, provide a ArithmeticAveragedOvernightIndexedCouponPricer");
        }

        Leg cashflows;

        // the following is not always correct
        Calendar calendar = schedule_.calendar();
        Calendar paymentCalendar = paymentCalendar_;

        if (calendar.empty())
            calendar = paymentCalendar;
        if (calendar.empty())
            calendar = WeekendsOnly();
        if (paymentCalendar.empty())
            paymentCalendar = calendar;

        Date refStart, start, refEnd, end;
        Date paymentDate;

        Size n = schedule_.size() - 1;

        // Initial consistency checks
        if (!paymentDates_.empty())
        {
            QL_REQUIRE(paymentDates_.size() == n, "Expected the number of explicit payment dates ("
                                                      << paymentDates_.size()
                                                      << ") to equal the number of calculation periods (" << n << ")");
        }

        for (Size i = 0; i < n; ++i)
        {
            refStart = start = schedule_.date(i);
            refEnd = end = schedule_.date(i + 1);

            // If explicit payment dates provided, use them.
            if (!paymentDates_.empty())
            {
                paymentDate = paymentDates_[i];
            }
            else
            {
                paymentDate = paymentCalendar.advance(end, paymentLag_, Days, paymentAdjustment_);
            }

            // determine refStart and refEnd
            if (i == 0 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1))
                refStart = calendar.adjust(end - schedule_.tenor(), paymentAdjustment_);
            if (i == n - 1 && schedule_.hasIsRegular() && !schedule_.isRegular(i + 1))
                refEnd = calendar.adjust(start + schedule_.tenor(), paymentAdjustment_);

            // Determine the rate computation start and end date as
            // - the coupon start and end date, if in arrears, and
            // - the previous coupon start and end date, if in advance.
            // In addition, adjust the start date, if a last recent period is given.

            Date rateComputationStartDate, rateComputationEndDate;
            if (inArrears_)
            {
                // in arrears fixing (i.e. the "classic" case)
                rateComputationStartDate = start;
                rateComputationEndDate = end;
            }
            else
            {
                // handle in advance fixing
                if (i > 0)
                {
                    // if there is a previous period, we take that
                    rateComputationStartDate = schedule_.date(i - 1);
                    rateComputationEndDate = schedule_.date(i);
                }
                else
                {
                    // otherwise we construct the previous period
                    rateComputationEndDate = start;
                    if (schedule_.hasTenor() && schedule_.tenor() != 0 * Days)
                        rateComputationStartDate = calendar.adjust(start - schedule_.tenor(), Preceding);
                    else
                        rateComputationStartDate = calendar.adjust(start - (end - start), Preceding);
                }
            }

            if (lastRecentPeriod_)
            {
                rateComputationStartDate = (lastRecentPeriodCalendar_.empty() ? calendar : lastRecentPeriodCalendar_)
                                               .advance(rateComputationEndDate, -*lastRecentPeriod_);
            }

            // build coupon

            if (close_enough(detail::get(gearings_, i, 1.0), 0.0))
            {
                // fixed coupon
                cashflows.push_back(
                    QuantLib::ext::make_shared<FixedRateCoupon>(paymentDate, detail::get(notionals_, i, 1.0),
                                                                detail::effectiveFixedRate(spreads_, caps_, floors_, i),
                                                                paymentDayCounter_, start, end, refStart, refEnd));
            }
            else
            {
                // floating coupon
                auto cpn = ext::make_shared<OvernightIndexedCoupon>(
                    paymentDate, detail::get(notionals_, i, 1.0), start, end, overnightIndex_,
                    detail::get(gearings_, i, 1.0), detail::get(spreads_, i, 0.0), refStart, refEnd, paymentDayCounter_,
                    telescopicValueDates_, averagingMethod_, lookbackDays_, lockoutDays_, applyObservationShift_,
                    compoundSpreadDaily_, rateComputationStartDate, rateComputationEndDate);
                if (couponPricer_)
                {
                    cpn->setPricer(couponPricer_);
                }
                Real cap = detail::get(caps_, i, Null<Real>());
                Real floor = detail::get(floors_, i, Null<Real>());
                if (cap == Null<Real>() && floor == Null<Real>())
                {
                    cashflows.push_back(cpn);
                }
                else
                {
                    auto cfCpn = ext::make_shared<CappedFlooredOvernightIndexedCoupon>(cpn, cap, floor, nakedOption_,
                                                                                       dailyCapFloor_);
                    if (couponPricer_)
                    {
                        cfCpn->setPricer(couponPricer_);
                    }
                    cashflows.push_back(cfCpn);
                }
            }
        }
        return cashflows;
    }

}
