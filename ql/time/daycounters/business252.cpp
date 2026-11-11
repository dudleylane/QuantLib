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

#include <ql/time/daycounters/business252.hpp>
#include <algorithm>
#include <map>
#include <vector>

namespace QuantLib
{

    namespace
    {

        typedef std::map<Year, std::map<Month, Date::serial_type>> Cache;
        typedef std::map<Year, Date::serial_type> OuterCache;

        // One calendar's business days in whole months and in whole years.
        // Calendars are told apart by their implementation, since two can
        // share a name (#122).  The copy of the calendar keeps that
        // implementation alive, so that no other calendar can be given its
        // address while the figures are kept.
        struct CalendarFigures
        {
            const void* implementation;
            Calendar calendar;
            unsigned long long changes; // detail::calendarChangeCount() when filled
            Cache monthly;
            OuterCache yearly;
        };

        // A calendar's added holidays are kept in its implementation, and
        // Calendar has no other public handle on it.
        const void* implementationOf(const Calendar& calendar)
        {
            return &calendar.addedHolidays();
        }

        // Per thread, because a day count across months writes to these even
        // when each thread has its own day counter (#87).  Most recently used
        // first, for at most this many calendars.
        constexpr std::size_t maxCalendarFigures = 8;
        thread_local std::vector<CalendarFigures> calendarFigures_;

        CalendarFigures& figuresFor(const Calendar& calendar)
        {
            const void* implementation = implementationOf(calendar);
            const unsigned long long changes = detail::calendarChangeCount();
            auto i =
                std::find_if(calendarFigures_.begin(), calendarFigures_.end(),
                             [implementation](const CalendarFigures& f) { return f.implementation == implementation; });
            if (i == calendarFigures_.end())
            {
                if (calendarFigures_.size() == maxCalendarFigures)
                    calendarFigures_.pop_back();
                calendarFigures_.insert(calendarFigures_.begin(),
                                        CalendarFigures{implementation, calendar, changes, {}, {}});
                return calendarFigures_.front();
            }
            std::rotate(calendarFigures_.begin(), i, i + 1);
            CalendarFigures& figures = calendarFigures_.front();
            if (figures.changes != changes)
            {
                // some calendar's holidays or weekend changed since these were
                // filled, possibly a calendar this one is joined with.
                figures.monthly.clear();
                figures.yearly.clear();
                figures.changes = changes;
            }
            return figures;
        }

        bool sameYear(const Date& d1, const Date& d2)
        {
            return d1.year() == d2.year();
        }

        bool sameMonth(const Date& d1, const Date& d2)
        {
            return d1.year() == d2.year() && d1.month() == d2.month();
        }

        Date::serial_type businessDays(Cache& cache, const Calendar& calendar, Month month, Year year)
        {
            if (cache[year][month] == 0)
            {
                // calculate and store.
                Date d1 = Date(1, month, year);
                Date d2 = d1 + 1 * Months;
                cache[year][month] = calendar.businessDaysBetween(d1, d2);
            }
            return cache[year][month];
        }

        Date::serial_type businessDays(OuterCache& outerCache, Cache& cache, const Calendar& calendar, Year year)
        {
            if (outerCache[year] == 0)
            {
                // calculate and store.
                Date::serial_type total = 0;
                for (Integer i = 1; i <= 12; ++i)
                {
                    total += businessDays(cache, calendar, Month(i), year);
                }
                outerCache[year] = total;
            }
            return outerCache[year];
        }

    }

    std::string Business252::Impl::name() const
    {
        std::ostringstream out;
        out << "Business/252(" << calendar_.name() << ")";
        return out.str();
    }

    Date::serial_type Business252::Impl::dayCount(const Date& d1, const Date& d2) const
    {
        if (sameMonth(d1, d2) || d1 >= d2)
        {
            // we treat the case of d1 > d2 here, since we'd need a
            // second cache to get it right (our cached figures are
            // for first included, last excluded and might have to be
            // changed going the other way.)
            return calendar_.businessDaysBetween(d1, d2);
        }
        else if (sameYear(d1, d2))
        {
            Cache& cache = figuresFor(calendar_).monthly;
            Date::serial_type total = 0;
            Date d;
            // first, we get to the beginning of next month.
            d = Date(1, d1.month(), d1.year()) + 1 * Months;
            total += calendar_.businessDaysBetween(d1, d);
            // then, we add any whole months (whose figures might be
            // cached already) in the middle of our period.
            while (!sameMonth(d, d2))
            {
                total += businessDays(cache, calendar_, d.month(), d.year());
                d += 1 * Months;
            }
            // finally, we get to the end of the period.
            total += calendar_.businessDaysBetween(d, d2);
            return total;
        }
        else
        {
            CalendarFigures& figures = figuresFor(calendar_);
            Cache& cache = figures.monthly;
            OuterCache& outerCache = figures.yearly;
            Date::serial_type total = 0;
            Date d;
            // first, we get to the beginning of next year.
            // The first bit gets us to the end of this month...
            d = Date(1, d1.month(), d1.year()) + 1 * Months;
            total += calendar_.businessDaysBetween(d1, d);
            // ...then we add any remaining months, possibly cached
            for (Integer m = Integer(d1.month()) + 1; m <= 12; ++m)
            {
                total += businessDays(cache, calendar_, Month(m), d.year());
            }
            // then, we add any whole year in the middle of our period.
            d = Date(1, January, d1.year() + 1);
            while (!sameYear(d, d2))
            {
                total += businessDays(outerCache, cache, calendar_, d.year());
                d += 1 * Years;
            }
            // finally, we get to the end of the period.
            // First, we add whole months...
            for (Integer m = 1; m < Integer(d2.month()); ++m)
            {
                total += businessDays(cache, calendar_, Month(m), d2.year());
            }
            // ...then the last bit.
            d = Date(1, d2.month(), d2.year());
            total += calendar_.businessDaysBetween(d, d2);
            return total;
        }
    }

    Time Business252::Impl::yearFraction(const Date& d1, const Date& d2, const Date&, const Date&) const
    {
        return dayCount(d1, d2) / 252.0;
    }

}
