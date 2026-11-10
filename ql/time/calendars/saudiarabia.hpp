/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004 FIMAT Group
 Copyright (C) 2007, 2011 StatPro Italia srl

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

/*! \file saudiarabia.hpp
    \brief Saudi Arabian calendar
*/

#ifndef quantlib_saudi_arabian_calendar_hpp
#define quantlib_saudi_arabian_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantLib
{

    //! Saudi Arabian calendar
    /*! Holidays for the Tadawul financial market
        (data from <https://www.saudiexchange.sa>):
        <ul>
        <li>Thursdays and Fridays, until May 27th, 1994</li>
        <li>Fridays, from May 28th, 1994 to June 14th, 2006, when Thursdays
        had a session</li>
        <li>Thursdays and Fridays, from June 15th, 2006 to June 28th, 2013</li>
        <li>Fridays and Saturdays, from June 29th, 2013</li>
        <li>Founding Day, February 22nd, since 2022 (moved to the Thursday
        before a Friday, or to the Sunday after a Saturday)</li>
        <li>National Day of Saudi Arabia, September 23rd, since 2005 (moved
        likewise since 2016)</li>
        </ul>

        Other holidays for which no rule is given (data available for
        2001-2029 only; from 2027 on these are the exchange's current calendar,
        which follows the Umm al-Qura calendar, so they can still move:)
        <ul>
        <li>Eid Al-Adha</li>
        <li>Eid Al-Fitr</li>
        </ul>

        From 2016 on these follow the exchange's notices, and from 2002 to
        2015 the sessions in daily trading data, a single series before 2008.
        Before 2002 they go by a rule, so treat those years as an
        approximation.

        \note Calendar::isWeekend takes only a weekday, so the calendar answers
               for its week since June 29th, 2013. Whether a date is a business
               day doesn't depend on it, but Calendar::holidayList does, unless
               asked to include weekends: for a date before then it lists the
               Thursdays of the old weekend and leaves out a holiday on a
               Saturday.

        \ingroup calendars
    */
    class SaudiArabia : public Calendar
    {
      private:
        class TadawulImpl final : public Calendar::Impl
        {
          public:
            std::string name() const override { return "Tadawul"; }
            bool isWeekend(Weekday) const override;
            bool isBusinessDay(const Date&) const override;
        };

      public:
        enum Market
        {
            Tadawul //!< Tadawul financial market
        };
        SaudiArabia(Market m = Tadawul);
    };

}


#endif
