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

#include <ql/errors.hpp>
#include <ql/time/calendars/saudiarabia.hpp>
#include <algorithm>
#include <utility>
#include <vector>

namespace QuantLib
{

    namespace
    {

        bool isTrueWeekend(Date d)
        {
            // The Saudi weekend was changed from 29th June 2013
            Weekday w = d.weekday();
            return (d < Date(29, June, 2013)) ? (w == Thursday || w == Friday) : (w == Friday || w == Saturday);
        }

        // In 2015 and 2014, the Eid holidays of the Tadawul Exchange
        // have been from Eid-1 to Eid+4
        // Sometimes, slightly longer holidays are observed
        // But conservatively, we take Eid-1 to Eid+4 as the holiday

        // Eid Date    Holiday     Offset    Remarks
        // 2015-09-23    22-27     -1,+4     later extended to 22-28 or -1+5
        // 2015-07-17    18-21     -1,+4
        // 2014-10-05    03-11     -1,+4     because of weekend 03-11 is same as 04-09
        // 2014-07-28    25-03     -1,+4     because of weekend 25-03 is same as 27-01
        // 2013-10-15    11-19     -2,+4     because of weekend 11-19 is same as 13-19
        // 2013-08-08    06-12     -2,+4
        // 2012-10-26    25-03     -1,+5     because of weekend 25-03 is same as 25-31
        // 2012-08-19    16-24     -1,+4     because of weekend 16-24 is same as 18-23

        // From 2016 on the holidays follow the exchange's notices, which name the last session
        // before a holiday and the first one after it. Those from 2027 on are in its current
        // calendar, which follows the Umm al-Qura calendar, so they can still move.

        bool isBetweenSessions(Date d, const std::vector<std::pair<Date, Date>>& sessions)
        {
            return std::any_of(sessions.begin(), sessions.end(),
                               [=](const std::pair<Date, Date>& p) { return d > p.first && d < p.second; });
        }

        bool isEidAlAdha(Date d)
        {
            // Eid al Adha dates taken from:
            // https://en.wikipedia.org/wiki/Eid_al-Adha#Eid_al-Adha_in_the_Gregorian_calendar
            static std::vector<Date> EidAlAdha = {
                Date(7, April, 1998),     Date(27, March, 1999),    Date(16, March, 2000),    Date(5, March, 2001),
                Date(23, February, 2002), Date(12, February, 2003), Date(1, February, 2004),  Date(21, January, 2005),
                Date(10, January, 2006),  Date(31, December, 2006), Date(20, December, 2007), Date(8, December, 2008),
                Date(27, November, 2009), Date(16, November, 2010), Date(6, November, 2011),  Date(26, October, 2012),
                Date(15, October, 2013),  Date(4, October, 2014),   Date(24, September, 2015)};

            static std::vector<std::pair<Date, Date>> sessions = {
                {Date(8, September, 2016), Date(18, September, 2016)},
                {Date(29, August, 2017), Date(6, September, 2017)},
                {Date(16, August, 2018), Date(26, August, 2018)},
                {Date(8, August, 2019), Date(18, August, 2019)},
                {Date(28, July, 2020), Date(5, August, 2020)},
                {Date(15, July, 2021), Date(25, July, 2021)},
                {Date(6, July, 2022), Date(13, July, 2022)},
                {Date(22, June, 2023), Date(2, July, 2023)},
                {Date(13, June, 2024), Date(23, June, 2024)},
                {Date(4, June, 2025), Date(11, June, 2025)},
                {Date(21, May, 2026), Date(31, May, 2026)},
                {Date(13, May, 2027), Date(23, May, 2027)},
                {Date(2, May, 2028), Date(10, May, 2028)},
                {Date(19, April, 2029), Date(29, April, 2029)},
            };

            return std::any_of(EidAlAdha.begin(), EidAlAdha.end(), [=](Date p) { return d >= p - 1 && d <= p + 4; }) ||
                   isBetweenSessions(d, sessions);
        }

        bool isEidAlFitr(Date d)
        {
            // Eid al Fitr dates taken from:
            // https://en.wikipedia.org/wiki/Eid_al-Fitr#In_the_Gregorian_calendar
            static std::vector<Date> EidAlFitr = {Date(16, Dec, 2001), Date(5, Dec, 2002),  Date(25, Nov, 2003),
                                                  Date(13, Nov, 2004), Date(3, Nov, 2005),  Date(23, Oct, 2006),
                                                  Date(12, Oct, 2007), Date(30, Sep, 2008), Date(20, Sep, 2009),
                                                  Date(10, Sep, 2010), Date(30, Aug, 2011), Date(19, Aug, 2012),
                                                  Date(8, Aug, 2013),  Date(28, Jul, 2014), Date(17, Jul, 2015)};

            static std::vector<std::pair<Date, Date>> sessions = {
                {Date(30, June, 2016), Date(10, July, 2016)},     {Date(22, June, 2017), Date(2, July, 2017)},
                {Date(12, June, 2018), Date(20, June, 2018)},     {Date(30, May, 2019), Date(9, June, 2019)},
                {Date(21, May, 2020), Date(31, May, 2020)},       {Date(10, May, 2021), Date(17, May, 2021)},
                {Date(28, April, 2022), Date(8, May, 2022)},      {Date(17, April, 2023), Date(25, April, 2023)},
                {Date(4, April, 2024), Date(14, April, 2024)},    {Date(27, March, 2025), Date(3, April, 2025)},
                {Date(16, March, 2026), Date(24, March, 2026)},   {Date(4, March, 2027), Date(14, March, 2027)},
                {Date(24, February, 2028), Date(5, March, 2028)}, {Date(11, February, 2029), Date(19, February, 2029)},
            };

            return std::any_of(EidAlFitr.begin(), EidAlFitr.end(), [=](Date p) { return d >= p - 1 && d <= p + 4; }) ||
                   isBetweenSessions(d, sessions);
        }

    }

    SaudiArabia::SaudiArabia(Market market)
    {
        // all calendar instances share the same implementation instance
        static ext::shared_ptr<Calendar::Impl> tadawulImpl(new SaudiArabia::TadawulImpl);
        switch (market)
        {
            case Tadawul:
                impl_ = tadawulImpl;
                break;
            default:
                QL_FAIL("unknown market");
        }
    }

    bool SaudiArabia::TadawulImpl::isWeekend(Weekday w) const
    {
        return w == Friday || w == Saturday;
    }

    bool SaudiArabia::TadawulImpl::isBusinessDay(const Date& date) const
    {
        Day d = date.dayOfMonth();
        Month m = date.month();
        Year y = date.year();
        Weekday w = date.weekday();

        if (isTrueWeekend(date) || isEidAlAdha(date) ||
            isEidAlFitr(date)
            // National Day
            || (d == 23 && m == September)
            // moved to the Thursday before a Friday or the Sunday after a Saturday, since 2016
            || (((d == 22 && w == Thursday) || (d == 24 && w == Sunday)) && m == September && y >= 2016)
            // Founding Day (since 2022, moved to the Thursday before a Friday or the Sunday after a Saturday)
            || ((d == 22 || (d == 21 && w == Thursday) || (d == 23 && w == Sunday)) && m == February && y >= 2022)
            // other one-shot holidays
            || (d == 26 && m == February && y == 2011) ||
            (d == 19 && m == March && y == 2011)
            // National Day, moved off the Thursday of the old weekend
            || (d == 22 && m == September && y == 2010)
            // National Day, extended by royal order
            || (d == 24 && m == September && y == 2018)
            // declared by royal order after the World Cup match against Argentina
            || (d == 23 && m == November && y == 2022))
            return false; // NOLINT(readability-simplify-boolean-expr)
        return true;
    }

}
