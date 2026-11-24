/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl

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

/*! \file actualactual.hpp
    \brief act/act day counters
*/

#ifndef quantlib_actualactual_day_counter_h
#define quantlib_actualactual_day_counter_h

#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>
#include <utility>
#include <vector>

namespace QuantLib
{

    //! Actual/Actual day count
    /*! The day count can be calculated according to:

        - the ISDA convention, also known as "Actual/Actual (Historical)",
          "Actual/Actual", "Act/Act", and according to ISDA also "Actual/365",
          "Act/365", and "A/365";
        - the ISMA and US Treasury convention, also known as
          "Actual/Actual (Bond)";
        - the AFB convention, also known as "Actual/Actual (Euro)".

        With the ISMA or Bond convention and a schedule, the day counter
        computes the schedule's period dates once, when it is built, and
        uses them while no calendar has changed since: a holiday added or
        removed, or a weekend changed, on any calendar.  After such a
        change it computes them again on every call, which costs several
        times as much, so build it once the holidays are set (#410).

        For more details, refer to
        https://www.isda.org/a/pIJEE/The-Actual-Actual-Day-Count-Fraction-1999.pdf

        \ingroup daycounters

        \test the correctness of the results is checked against known
              good values.
    */
    class ActualActual : public DayCounter
    {
      public:
        enum Convention
        {
            ISMA,
            Bond,
            ISDA,
            Historical,
            Actual365,
            AFB,
            Euro
        };

      private:
        class ISMA_Impl final : public DayCounter::Impl
        {
          public:
            explicit ISMA_Impl(Schedule schedule);

            std::string name() const override { return std::string("Actual/Actual (ISMA)"); }
            Time yearFraction(const Date& d1,
                              const Date& d2,
                              const Date& refPeriodStart,
                              const Date& refPeriodEnd) const override;

          private:
            Schedule schedule_;
            // The schedule's dates, with the notional ones of its irregular
            // periods, as computed when the day counter was built; used while
            // no calendar's business days have changed since, which could move
            // the notional ones (#102).
            std::vector<Date> periodDates_;
            unsigned long long calendarChanges_ = 0;
            bool periodDatesKept_ = false, periodDatesSorted_ = false;
        };
        class Old_ISMA_Impl final : public DayCounter::Impl
        {
          public:
            std::string name() const override { return std::string("Actual/Actual (ISMA)"); }
            Time yearFraction(const Date& d1,
                              const Date& d2,
                              const Date& refPeriodStart,
                              const Date& refPeriodEnd) const override;
        };
        class ISDA_Impl final : public DayCounter::Impl
        {
          public:
            std::string name() const override { return std::string("Actual/Actual (ISDA)"); }
            Time yearFraction(const Date& d1, const Date& d2, const Date&, const Date&) const override;
        };
        class AFB_Impl final : public DayCounter::Impl
        {
          public:
            std::string name() const override { return std::string("Actual/Actual (AFB)"); }
            Time yearFraction(const Date& d1, const Date& d2, const Date&, const Date&) const override;
        };
        static ext::shared_ptr<DayCounter::Impl> implementation(Convention c, Schedule schedule);

      public:
        explicit ActualActual(Convention c, Schedule schedule = Schedule())
        : DayCounter(implementation(c, std::move(schedule)))
        {
        }
    };

}

#endif
