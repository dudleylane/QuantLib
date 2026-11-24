/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2005 Joseph Wang

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

#include <ql/event.hpp>
#include <ql/optional.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/settings.hpp>

namespace QuantLib
{

    namespace
    {

        // Event::hasOccurred's rule.  It is file-local so that both callers below inline it: in a shared build the
        // library is compiled with -fPIC, and there GCC doesn't inline a global function, which another library
        // could interpose (#103).  It takes the optional by reference, and asks eventDate for the date only once it
        // knows the reference date and the rule, as Event::hasOccurred always did: otherwise GCC keeps a register
        // more across the calls, and Event::hasOccurred runs more instructions than it did.
        template <class EventDate>
        bool eventDateHasOccurred(const EventDate& eventDate,
                                  const Date& d, // refDate
                                  const ext::optional<bool>& includeRefDate)
        {
            Date refDate = d != Date() ? d : Settings::instance().evaluationDate();
            bool includeRefDateEvent = includeRefDate ? // NOLINT(readability-implicit-bool-conversion)
                                           *includeRefDate :
                                           Settings::instance().includeReferenceDateEvents();
            if (includeRefDateEvent)
                return eventDate() < refDate;
            else
                return eventDate() <= refDate;
        }

    }

    bool Event::hasOccurred(const Date& d, // refDate
                            ext::optional<bool> includeRefDate) const
    {
        return eventDateHasOccurred([this]() { return date(); }, d, includeRefDate);
    }

    void Event::accept(AcyclicVisitor& v)
    {
        auto* v1 = dynamic_cast<Visitor<Event>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            QL_FAIL("not an event visitor");
    }

    namespace detail
    {

        bool hasOccurred(const Date& eventDate, const Date& refDate, ext::optional<bool> includeRefDate)
        {
            return eventDateHasOccurred([&eventDate]() -> const Date& { return eventDate; }, refDate, includeRefDate);
        }

    }

}
