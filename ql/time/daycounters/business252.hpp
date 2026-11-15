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
        years in it, kept per thread for at most eight calendars at a
        time.  Calendars known to have the same business days share
        figures: copies of a calendar, a market's calendars built alike,
        which for most markets share an implementation, and joint, null
        and bespoke calendars built alike (the same calendars joined by
        the same rule, or the same weekend) with the same added and
        removed holidays.  Other calendars have their own, even under the
        same name, so more than eight of them in turn fill theirs again
        each time, at about the cost of counting the days one by one.
        When a calendar's holidays or weekend change through addHoliday,
        removeHoliday, resetAddedAndRemovedHolidays or
        BespokeCalendar::addWeekend, it uses figures for its new business
        days from then on, and so does any joint calendar it is part of;
        other calendars keep theirs.

        \warning A calendar implementation whose business days change in
                 any other way leaves the figures stale, unless it calls
                 detail::recordCalendarChange(), which drops every
                 thread's figures.  One whose business days depend on
                 another calendar leaves them stale when that calendar's
                 holidays change, unless it appends that calendar's key
                 in Calendar::Impl::appendBusinessDaysKey, as
                 JointCalendar's implementation does.

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
