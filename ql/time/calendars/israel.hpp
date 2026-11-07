/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Paolo Mazzocchi
 Copyright (C) 2015 Riccardo Barone

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

/*! \file israel.hpp
    \brief Israelian calendar
*/

#ifndef quantlib_israel_calendar_hpp
#define quantlib_israel_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantLib
{

    //! Israel calendar
    /*! Due to the lack of reliable sources, the settlement calendar keeps
        the rules this class had for the exchange until 2026: Friday and Saturday,
        the holidays below, and their eves with those of Passover and Shavuot only
        until 2020 and that of the new year only until 2019.

        Holidays for the Tel-Aviv Stock Exchange
        (data from <http://www.tase.co.il>):
        <ul>
        <li>Friday and Saturday until January 3rd, 2026</li>
        <li>Saturday and Sunday from January 4th, 2026</li>
        <li>From then on, a Friday before a Sunday on which it's closed</li>
        <li>The eve of each holiday below that isn't on a weekend</li>
        <li>Election days, from 2019</li>
        </ul>
        Other holidays for wich no rule is given
        (data available for 2013-2044 only:)
        <ul>
        <li>Purim, Adar 14th (between Feb 24th & Mar 26th)</li>
        <li>Passover I, Nisan 15th (between Mar 26th & Apr 25th)</li>
        <li>Passover VII, Nisan 21st (between Apr 1st & May 1st)</li>
        <li>Memorial Day, Nisan 27th (between Apr 7th & May 7th)</li>
        <li>Indipendence Day, Iyar 5th (between Apr 15th & May 15th)</li>
        <li>Pentecost (Shavuot), Sivan 6th (between May 15th & June 14th)</li>
        <li>Fast Day</li>
        <li>Jewish New Year, Tishrei 1st & 2nd (between Sep 5th & Oct 5th)</li>
        <li>Yom Kippur, Tishrei 10th (between Sep 14th & Oct 14th)</li>
        <li>Sukkoth, Tishrei 15th (between Sep 19th & Oct 19th)</li>
        <li>Simchat Tora, Tishrei 22nd (between Sep 26th & Oct 26th)</li>
        </ul>

        \note Calendar::isWeekend takes only a weekday, so the exchange's answers
               for its week since 2026. Whether a date is a business day doesn't
               depend on it, but Calendar::holidayList does, unless asked to include
               weekends: for a date before 2026 it lists each Friday and leaves out
               a holiday on a Sunday.

        \ingroup calendars
    */
    class Israel : public Calendar
    {
      private:
        class SettlementImpl;
        class TelAvivImpl;
        class ShirImpl;

      public:
        enum Market
        {
            Settlement, //!< generic settlement calendar
            TASE,       //!< Tel-Aviv stock exchange calendar
            SHIR        //!< SHIR fixing calendar
        };
        Israel(Market market = Settlement);
    };

}


#endif
