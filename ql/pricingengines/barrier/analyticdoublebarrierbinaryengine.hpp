/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Thema Consulting SA

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

/*! \file analyticdoublebarrierbinaryengine.hpp
    \brief analytic binary double barrier (one-touch double barrier) option engine
*/

#ifndef quantlib_binary_double_barrier_engine_hpp
#define quantlib_binary_double_barrier_engine_hpp

#include <ql/instruments/doublebarrieroption.hpp>
#include <ql/processes/blackscholesprocess.hpp>

namespace QuantLib
{

    //! Analytic pricing engine for double barrier binary options
    /*! This engine implements C.H.Hui series ("One-Touch Double Barrier
        Binary Option Values", Applied Financial Economics 6/1996), as
        described in "The complete guide to option pricing formulas 2nd Ed",
        E.G. Haug, McGraw-Hill, p.180

        The Knock In part of KI+KO and KO+KI options pays at hit, while the
        Double Knock In pays at end.
        This engine thus requires European esercise for Double Knock options,
        and American exercise for KIKO/KOKI.

        A rebate is paid where BinomialDoubleBarrierEngine pays it: a Double
        Knock Out's at the first touch of either barrier, a Double Knock In's
        at expiry if neither barrier is touched, a KIKO's at a touch of the
        upper barrier before the lower, and a KOKI's at a touch of the lower
        barrier before the upper, or either's at expiry if neither barrier is
        touched.  With the spot already at or past a barrier, an option
        knocked out there is worth its rebate, paid at once, and one knocked
        in has none.

        Hui's series converge ever more slowly as the volatility falls, and
        their terms grow while the value doesn't. So where the volatility to
        expiry, vol sqrt(T), is at most half of ln(U/L) for Double Knock
        options, and at most ln(U/L) for KIKO/KOKI, the engine sums images of
        the spot in the barriers instead. Above that, it sums the part of the
        KIKO/KOKI series that doesn't depend on expiry in closed form, since
        that part's terms fall only like 1/i^3. At zero volatility the
        underlying follows its forward. The Knock In is worth the discounted
        cash less the Knock Out, so its error is absolute, and a small
        Knock In loses its relative accuracy.

        \ingroup barrierengines

        greeks are calculated by simple numeric derivation

        \test
        - the correctness of the returned value is tested by reproducing
          results available in literature.
    */
    class AnalyticDoubleBarrierBinaryEngine : public DoubleBarrierOption::engine
    {
      public:
        explicit AnalyticDoubleBarrierBinaryEngine(ext::shared_ptr<GeneralizedBlackScholesProcess>);
        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
    };

    namespace detail
    {

        // The probability that the underlying stays inside (lower, upper) up to T, from spot, with its log moving by
        // (carry - vol^2 / 2) t + vol W_t and variance = vol^2 T; 0 at or outside the barriers.  The engine above
        // prices Double Knock options with it, and VannaVolgaDoubleBarrierEngine weights its smile adjustment by it.
        // How it's summed is at its definition, in analyticdoublebarrierbinaryengine.cpp.
        Real doubleBarrierNoTouchProbability(Real spot, Real lower, Real upper, Real carry, Time T, Real variance);

        // A double-barrier option's rebate, paid when DiscretizedDoubleBarrierOption and MCDoubleBarrierEngine pay it:
        // for a knock-out at the first touch of either barrier, if one comes by T, and for a knock-in at T, if neither
        // barrier is touched.  The first is the sum of what the engine above pays at a touch for KIKO and KOKI options,
        // with r the risk-free rate, flat to T, that discounts from the touch; the second is rebate * discount times
        // the probability above.  The other arguments are the probability's.  AnalyticDoubleBarrierEngine and
        // SuoWangDoubleBarrierEngine add it to their values, and VannaVolgaDoubleBarrierEngine to its own.
        Real doubleBarrierRebate(DoubleBarrier::Type type,
                                 Real rebate,
                                 Real spot,
                                 Real lower,
                                 Real upper,
                                 Rate r,
                                 Real carry,
                                 Time T,
                                 Real variance,
                                 DiscountFactor discount);

    }

}


#endif
