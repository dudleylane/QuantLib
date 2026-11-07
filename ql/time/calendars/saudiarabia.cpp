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

        // From 2008 on the holidays go by the last session before them and the first one after.
        // From 2016 on those are in the exchange's notices, and from 2027 on in its current
        // calendar, which follows the Umm al-Qura calendar, so they can still move. From 2008 to
        // 2015 they come from daily data: the trades of every share and the TASI index, which
        // agree with each other and with the two notices found, and give the trading days the
        // exchange reports for 2012 to 2015.

        // Before 2008 there's only one such series, so a holiday goes by its Eid date, from Eid-1
        // to Eid+4. That can be some days off (#171).

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
                Date(7, April, 1998),     Date(27, March, 1999),    Date(16, March, 2000),   Date(5, March, 2001),
                Date(23, February, 2002), Date(12, February, 2003), Date(1, February, 2004), Date(21, January, 2005),
                Date(10, January, 2006),  Date(31, December, 2006), Date(20, December, 2007)};

            static std::vector<std::pair<Date, Date>> sessions = {
                {Date(3, December, 2008), Date(13, December, 2008)},
                {Date(25, November, 2009), Date(5, December, 2009)},
                {Date(10, November, 2010), Date(20, November, 2010)},
                {Date(2, November, 2011), Date(12, November, 2011)},
                {Date(24, October, 2012), Date(3, November, 2012)},
                {Date(10, October, 2013), Date(20, October, 2013)},
                {Date(2, October, 2014), Date(12, October, 2014)},
                {Date(21, September, 2015), Date(29, September, 2015)},
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
            static std::vector<Date> EidAlFitr = {Date(16, Dec, 2001), Date(5, Dec, 2002), Date(25, Nov, 2003),
                                                  Date(13, Nov, 2004), Date(3, Nov, 2005), Date(23, Oct, 2006),
                                                  Date(12, Oct, 2007)};

            static std::vector<std::pair<Date, Date>> sessions = {
                {Date(28, September, 2008), Date(6, October, 2008)},
                {Date(16, September, 2009), Date(26, September, 2009)},
                {Date(6, September, 2010), Date(14, September, 2010)},
                {Date(24, August, 2011), Date(3, September, 2011)},
                {Date(15, August, 2012), Date(25, August, 2012)},
                {Date(5, August, 2013), Date(13, August, 2013)},
                {Date(24, July, 2014), Date(3, August, 2014)},
                {Date(15, July, 2015), Date(22, July, 2015)},
                {Date(30, June, 2016), Date(10, July, 2016)},
                {Date(22, June, 2017), Date(2, July, 2017)},
                {Date(12, June, 2018), Date(20, June, 2018)},
                {Date(30, May, 2019), Date(9, June, 2019)},
                {Date(21, May, 2020), Date(31, May, 2020)},
                {Date(10, May, 2021), Date(17, May, 2021)},
                {Date(28, April, 2022), Date(8, May, 2022)},
                {Date(17, April, 2023), Date(25, April, 2023)},
                {Date(4, April, 2024), Date(14, April, 2024)},
                {Date(27, March, 2025), Date(3, April, 2025)},
                {Date(16, March, 2026), Date(24, March, 2026)},
                {Date(4, March, 2027), Date(14, March, 2027)},
                {Date(24, February, 2028), Date(5, March, 2028)},
                {Date(11, February, 2029), Date(19, February, 2029)},
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
            // and off its Friday, going by the daily data, as no notice was found
            || (d == 24 && m == September && y == 2011)
            // no sessions in the daily data and none in the trading days reported, though no notice
            // was found: the day before National Day, and the Sunday after King Abdullah's death
            || (d == 22 && m == September && y == 2013) ||
            (d == 25 && m == January && y == 2015)
            // National Day, extended by royal order
            || (d == 24 && m == September && y == 2018)
            // declared by royal order after the World Cup match against Argentina
            || (d == 23 && m == November && y == 2022))
            return false; // NOLINT(readability-simplify-boolean-expr)
        return true;
    }

}
