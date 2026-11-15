/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005, 2006, 2007 StatPro Italia srl
 Copyright (C) 2006 Piter Dias
 Copyright (C) 2020 Leonardo Arcari
 Copyright (C) 2020 Kline s.r.l.

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

/*! \file calendar.hpp
    \brief %calendar class
*/

#ifndef quantlib_calendar_hpp
#define quantlib_calendar_hpp

#include <ql/errors.hpp>
#include <ql/shared_ptr.hpp>
#include <ql/time/businessdayconvention.hpp>
#include <ql/time/date.hpp>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace QuantLib
{

    class Period;
    class Calendar;

    namespace detail
    {
        void appendBusinessDaysKey(const Calendar&, std::vector<std::uintptr_t>&);
    }

    //! %calendar class
    /*! This class provides methods for determining whether a date is a
        business day or a holiday for a given market, and for
        incrementing/decrementing a date of a given number of business days.

        The Bridge pattern is used to provide the base behavior of the
        calendar, namely, to determine whether a date is a business day.

        A calendar should be defined for specific exchange holiday schedule
        or for general country holiday schedule. Legacy city holiday schedule
        calendars will be moved to the exchange/country convention.

        \ingroup datetime

        \test the methods for adding and removing holidays are tested
              by inspecting the calendar before and after their
              invocation.
    */
    class Calendar
    {
      protected:
        //! abstract base class for calendar implementations
        class Impl
        {
          public:
            virtual ~Impl() = default;
            virtual std::string name() const = 0;
            virtual bool isBusinessDay(const Date&) const = 0;
            virtual bool isWeekend(Weekday) const = 0;
            //! appends what this implementation's business days depend on to a key
            /*! Caches of business days, such as Business252's, give two
                calendars the same figures when their keys are equal (see
                detail::appendBusinessDaysKey), and after a change to any
                calendar's holidays or weekend they keep a calendar's
                figures only if its key is still the one they were made
                for.  So equal keys must mean the same business days,
                before added and removed holidays, whatever changes are
                made to calendars' holidays and weekends.

                The default appends 0 and the implementation's address, so a
                calendar shares figures only with those on the same
                implementation: its copies, and for most markets the
                calendars built alike, which share one.  That holds for an
                implementation whose business days depend only on the date
                and on its own added and removed holidays.  One whose
                business days also depend on another calendar must append
                that calendar's key with detail::appendBusinessDaysKey, as
                JointCalendar does, or its figures stay those of the other
                calendar's earlier holidays.  One whose business days change
                in a way its key doesn't show must call
                detail::recordCalendarChange().

                JointCalendar, NullCalendar and BespokeCalendar, which build
                an implementation for each calendar, append 1, 2 and 3, and
                then what their business days depend on, so that calendars
                built alike share figures.  An override elsewhere should
                start with a tag no other implementation uses, such as the
                address of a static object of its own, and what follows
                should show where it ends, as a count before a list does,
                since a joint calendar's key runs its calendars' keys
                together.
            */
            virtual void appendBusinessDaysKey(std::vector<std::uintptr_t>& key) const;
            std::set<Date> addedHolidays, removedHolidays;
        };
        ext::shared_ptr<Impl> impl_;
        friend void detail::appendBusinessDaysKey(const Calendar&, std::vector<std::uintptr_t>&);

      public:
        /*! The default constructor returns a calendar with a null
            implementation, which is therefore unusable except as a
            placeholder.
        */
        Calendar() = default;
        //! \name Calendar interface
        //@{
        //!  Returns whether or not the calendar is initialized
        bool empty() const;
        //! Returns the name of the calendar.
        /*! \warning This method is used for output and comparison between
                calendars. It is <b>not</b> meant to be used for writing
                switch-on-type code.
        */
        std::string name() const;
        /*! Returns <tt>true</tt> iff the date is a business day for the
            given market.
        */

        /*! Returns the set of added holidays for the given calendar */
        const std::set<Date>& addedHolidays() const;

        /*! Returns the set of removed holidays for the given calendar */
        const std::set<Date>& removedHolidays() const;

        /*! Clear the set of added and removed holidays */
        void resetAddedAndRemovedHolidays();

        bool isBusinessDay(const Date& d) const;
        /*! Returns <tt>true</tt> iff the date is a holiday for the given
            market.
        */
        bool isHoliday(const Date& d) const;
        /*! Returns <tt>true</tt> iff the weekday is part of the
            weekend for the given market.
        */
        bool isWeekend(Weekday w) const;
        /*! Returns <tt>true</tt> iff in the given market, the date is on
            or before the first business day for that month.
        */
        bool isStartOfMonth(const Date& d) const;
        //! first business day of the month to which the given date belongs
        Date startOfMonth(const Date& d) const;
        /*! Returns <tt>true</tt> iff in the given market, the date is on
            or after the last business day for that month.
        */
        bool isEndOfMonth(const Date& d) const;
        //! last business day of the month to which the given date belongs
        Date endOfMonth(const Date& d) const;

        /*! Adds a date to the set of holidays for the given calendar. */
        void addHoliday(const Date&);
        /*! Removes a date from the set of holidays for the given calendar. */
        void removeHoliday(const Date&);

        /*! Returns the holidays between two dates. */
        std::vector<Date> holidayList(const Date& from, const Date& to, bool includeWeekEnds = false) const;
        /*! Returns the business days between two dates. */
        std::vector<Date> businessDayList(const Date& from, const Date& to) const;

        /*! Adjusts a non-business day to the appropriate near business day
            with respect to the given convention.
        */
        Date adjust(const Date&, BusinessDayConvention convention = Following) const;
        /*! Advances the given date of the given number of business days and
            returns the result.
            \note The input date is not modified.
        */
        Date advance(const Date&,
                     Integer n,
                     TimeUnit unit,
                     BusinessDayConvention convention = Following,
                     bool endOfMonth = false) const;
        /*! Advances the given date as specified by the given period and
            returns the result.
            \note The input date is not modified.
        */
        Date advance(const Date& date,
                     const Period& period,
                     BusinessDayConvention convention = Following,
                     bool endOfMonth = false) const;
        /*! Calculates the number of business days between two given
            dates and returns the result.
        */
        Date::serial_type
        businessDaysBetween(const Date& from, const Date& to, bool includeFirst = true, bool includeLast = false) const;
        //@}

      protected:
        //! partial calendar implementation
        /*! This class provides the means of determining the Easter
            Monday for a given year, as well as specifying Saturdays
            and Sundays as weekend days.
        */
        class WesternImpl : public Impl
        {
          public:
            bool isWeekend(Weekday) const override;
            //! expressed relative to first day of year
            static Day easterMonday(Year);
        };
        //! partial calendar implementation
        /*! This class provides the means of determining the Orthodox
            Easter Monday for a given year, as well as specifying
            Saturdays and Sundays as weekend days.
        */
        class OrthodoxImpl : public Impl
        {
          public:
            bool isWeekend(Weekday) const override;
            //! expressed relative to first day of year
            static Day easterMonday(Year);
        };
    };

    /*! Returns <tt>true</tt> iff the two calendars belong to the same
        derived class.
        \relates Calendar
    */
    bool operator==(const Calendar&, const Calendar&);

    /*! \relates Calendar */
    bool operator!=(const Calendar&, const Calendar&);

    /*! \relates Calendar */
    std::ostream& operator<<(std::ostream&, const Calendar&);

    namespace detail
    {

        //! the number of changes made so far to any calendar's holidays or weekend
        /*! Calendar::addHoliday, Calendar::removeHoliday,
            Calendar::resetAddedAndRemovedHolidays and
            BespokeCalendar::addWeekend record one each, through
            recordKeyedCalendarChange, and recordCalendarChange records one
            too.  Caches of business days, such as Business252's, check a
            calendar's key again (see appendBusinessDaysKey) when it differs
            from the number under which they last did.
        */
        unsigned long long calendarChangeCount();

        //! the number of changes recorded so far by recordCalendarChange
        /*! Caches of business days drop all their figures when it differs
            from the number they were made under.
        */
        unsigned long long unkeyedCalendarChangeCount();

        //! records a change to a calendar's business days that its key shows
        /*! such as to its added or removed holidays, or to a
            BespokeCalendar's weekend.  Caches of business days check the
            keys of the calendars they count on again, and keep the figures
            of those whose keys are unchanged.
        */
        void recordKeyedCalendarChange();

        //! records a change to a calendar's business days that keys don't show
        /*! A calendar implementation whose business days change other than
            through the methods above, and other than as its key shows (see
            Calendar::Impl::appendBusinessDaysKey), should call it, so that
            such caches are dropped.
        */
        void recordCalendarChange();

        //! appends what the calendar's business days depend on to a key
        /*! The implementation's part (see Calendar::Impl::appendBusinessDaysKey),
            then the number of added holidays and their serial numbers, and
            the number of removed holidays and theirs.  Two calendars with
            equal keys have the same business days.
        */
        void appendBusinessDaysKey(const Calendar& calendar, std::vector<std::uintptr_t>& key);

    }


    // inline definitions

    inline bool Calendar::empty() const
    {
        return !impl_;
    }

    inline std::string Calendar::name() const
    {
        QL_REQUIRE(impl_, "no calendar implementation provided");
        return impl_->name();
    }

    inline const std::set<Date>& Calendar::addedHolidays() const
    {
        QL_REQUIRE(impl_, "no calendar implementation provided");

        return impl_->addedHolidays;
    }

    inline const std::set<Date>& Calendar::removedHolidays() const
    {
        QL_REQUIRE(impl_, "no calendar implementation provided");

        return impl_->removedHolidays;
    }

    inline bool Calendar::isBusinessDay(const Date& d) const
    {
        QL_REQUIRE(impl_, "no calendar implementation provided");

#ifdef QL_HIGH_RESOLUTION_DATE
        const Date _d(d.dayOfMonth(), d.month(), d.year());
#else
        const Date& _d = d;
#endif

        if (!impl_->addedHolidays.empty() && impl_->addedHolidays.find(_d) != impl_->addedHolidays.end())
            return false;

        if (!impl_->removedHolidays.empty() && impl_->removedHolidays.find(_d) != impl_->removedHolidays.end())
            return true;

        return impl_->isBusinessDay(_d);
    }

    inline bool Calendar::isStartOfMonth(const Date& d) const
    {
        return d <= startOfMonth(d);
    }

    inline Date Calendar::startOfMonth(const Date& d) const
    {
        return adjust(Date::startOfMonth(d), Following);
    }

    inline bool Calendar::isEndOfMonth(const Date& d) const
    {
        return d >= endOfMonth(d);
    }

    inline Date Calendar::endOfMonth(const Date& d) const
    {
        return adjust(Date::endOfMonth(d), Preceding);
    }

    inline bool Calendar::isHoliday(const Date& d) const
    {
        return !isBusinessDay(d);
    }

    inline bool Calendar::isWeekend(Weekday w) const
    {
        QL_REQUIRE(impl_, "no calendar implementation provided");
        return impl_->isWeekend(w);
    }

    inline bool operator==(const Calendar& c1, const Calendar& c2)
    {
        return (c1.empty() && c2.empty()) || (!c1.empty() && !c2.empty() && c1.name() == c2.name());
    }

    inline bool operator!=(const Calendar& c1, const Calendar& c2)
    {
        return !(c1 == c2);
    }

    inline std::ostream& operator<<(std::ostream& out, const Calendar& c)
    {
        return out << c.name();
    }

}

#endif
