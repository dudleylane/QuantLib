/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Piter Dias
 Copyright (C) 2011 StatPro Italia srl

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

/*! \file business252.hpp
    \brief business/252 day counter
*/

#ifndef quantlib_business252_day_counter_hpp
#define quantlib_business252_day_counter_hpp

#include <ql/time/calendar.hpp>
#include <ql/time/calendars/brazil.hpp>
#include <ql/time/daycounter.hpp>
#include <utility>

namespace QuantLib
{

    //! Business/252 day count convention
    /*! A count across months adds up figures for the whole months and
        years in it, kept per thread for each calendar, and for at most
        eight calendars at a time.  Copies of a calendar share its
        figures; another calendar has its own, even under the same name.
        So a calendar built for each instrument, such as a JointCalendar,
        fills its own on first use, and more than eight of them in turn
        fill theirs again each time, at about the cost of counting the
        days one by one.  The figures are dropped whenever any calendar's
        holidays or weekend change through addHoliday, removeHoliday,
        resetAddedAndRemovedHolidays or BespokeCalendar::addWeekend.

        \warning A calendar implementation whose business days change in
                 any other way leaves the figures stale, unless it calls
                 detail::recordCalendarChange().

        \ingroup daycounters
    */
    class Business252 : public DayCounter
    {
      private:
        class Impl final : public DayCounter::Impl
        {
          private:
            Calendar calendar_;

          public:
            std::string name() const override;
            Date::serial_type dayCount(const Date& d1, const Date& d2) const override;
            Time yearFraction(const Date& d1, const Date& d2, const Date&, const Date&) const override;
            explicit Impl(Calendar c) : calendar_(std::move(c)) {}
        };

      public:
        Business252(const Calendar& c = Brazil())
        : DayCounter(ext::shared_ptr<DayCounter::Impl>(new Business252::Impl(c)))
        {
        }
    };

}

#endif
