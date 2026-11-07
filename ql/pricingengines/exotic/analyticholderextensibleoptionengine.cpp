/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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

#include <ql/exercise.hpp>
#include <ql/math/distributions/bivariatenormaldistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/exotic/analyticholderextensibleoptionengine.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

using std::pow;
using std::log;
using std::exp;
using std::sqrt;

namespace QuantLib
{

    AnalyticHolderExtensibleOptionEngine::AnalyticHolderExtensibleOptionEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process))
    {
        registerWith(process_);
    }

    void AnalyticHolderExtensibleOptionEngine::calculate() const
    {
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        const Option::Type type = payoff->optionType();
        const Real eta = Real(type);
        const Real infinity = std::numeric_limits<Real>::infinity();

        Real S = process_->x0();
        Real X1 = strike();
        Real X2 = arguments_.secondStrike;
        Time T2 = secondExpiryTime();
        Time t1 = firstExpiryTime();
        Real A = arguments_.premium;
        Real variance1 = variance(t1);
        Real variance2 = variance(T2);

        // calculate dividend discount factor assuming continuous compounding (e^-rt)
        DiscountFactor growth = dividendDiscount(t1);
        // calculate payoff discount factor assuming continuous compounding
        DiscountFactor discount = riskFreeDiscount(t1);
        DiscountFactor growth2 = dividendDiscount(T2);
        DiscountFactor discount2 = riskFreeDiscount(T2);

        // With no variance to speak of before the first expiry, the spot there is known, and so is the choice.
        if (variance1 < QL_EPSILON * QL_EPSILON)
        {
            Real forward = S * growth / discount;
            results_.value = discount * std::max({eta * (forward - X1), bsCalculator(forward, type).value() - A, 0.0});
            return;
        }

        Real rho = sqrt(variance1 / variance2);
        Real s1 = sqrt(variance1);
        Real s2 = sqrt(variance2);
        Real z1 = log(S * growth2 / (discount2 * X2)) / s2 + s2 / 2;

        const auto y = [&](Real x) { return log(S * growth / (discount * x)) / s1 + s1 / 2; };

        // What extending adds to the option for first expiry spots between two levels: the extended
        // option less the premium, and less what exercising the option would have paid there.
        const auto gain = [&](Real lower, Real upper)
        {
            Real yl = y(lower), yu = y(upper);
            Real extended = eta * (S * growth2 * M2(yu, yl, eta * z1, eta * rho) -
                                   X2 * discount2 * M2(yu - s1, yl - s1, eta * (z1 - s2), eta * rho));
            Real ye = y(X1);
            Real ya = type == Option::Call ? std::min(yl, ye) : yl;
            Real yb = type == Option::Call ? yu : std::max(yu, ye);
            Real exercised = eta * (S * growth * N2(yb, ya) - X1 * discount * N2(yb - s1, ya - s1));
            return extended - A * discount * N2(yu - s1, yl - s1) - exercised;
        };

        Real result = BlackScholesCalculator(payoff, S, growth, s1, discount).value();

        // Beyond the first strike, in the option's money, extending competes with exercising.  Their
        // difference is convex in the spot, and this is its limit at the far end.
        DiscountFactor dq = growth2 / growth, dr = discount2 / discount;
        Real farEnd =
            type == Option::Call ? (dq < 1.0 ? -infinity : (dq > 1.0 ? infinity : X1 - X2 * dr - A)) : X2 * dr - A - X1;
        Real atStrike = bsCalculator(X1, type).value() - A;

        // The difference falls all the way to the far end, unless a negative dividend yield turns it back up.
        Real lowest = farEnd;
        if (dq > 1.0)
        {
            Real d1 = eta * InverseCumulativeNormal()(1.0 / dq);
            Real s = sqrt(variance2 - variance1);
            Real turningPoint = X2 * dr / dq * exp(d1 * s - s * s / 2);
            lowest = eta * (turningPoint - X1) > 0.0 ?
                         bsCalculator(turningPoint, type).value() - A - eta * (turningPoint - X1) :
                         atStrike;
        }

        // Extending beats lapsing up to the first strike only if it's worth the premium at the strike.
        if (atStrike > 0.0)
        {
            Real againstLapsing = criticalPrice(X1, type, 0.0);
            Real againstExercising =
                lowest >= 0.0 ? (type == Option::Call ? infinity : 0.0) : criticalPrice(X1, type, eta);
            result += type == Option::Call ? gain(againstLapsing, againstExercising) :
                                             gain(againstExercising, againstLapsing);
        }

        // Once the difference has turned back up, extending beats exercising again.  The value of
        // the option has a floor in its forward, which is linear, and that gives a starting point.
        if (farEnd > 0.0 && lowest < 0.0)
        {
            result += type == Option::Call ? gain(criticalPrice((X2 * dr + A - X1) / (dq - 1.0), type, eta), infinity) :
                                             gain(0.0, criticalPrice(farEnd / (dq - 1.0), type, eta));
        }

        results_.value = result;
    }

    Real AnalyticHolderExtensibleOptionEngine::criticalPrice(Real start, Option::Type type, Real exerciseWeight) const
    {
        Real X1 = strike();
        Real X2 = arguments_.secondStrike;
        Real A = arguments_.premium;

        // Newton-Raphson process: the function is convex and the start is where it's positive
        // and on the root's side of any minimum, so that it never overshoots.
        Real Sv = start;
        for (Size iteration = 0; iteration < 1000; ++iteration)
        {
            BlackScholesCalculator bs = bsCalculator(Sv, type);
            Real yi = bs.value() - A - exerciseWeight * (Sv - X1);
            Real di = bs.delta() - exerciseWeight;
            // Relative to the sizes involved, since the rounding of values that size can exceed anything less.
            if (std::fabs(yi) <= 1.0e-12 * (Sv + X1 + X2) || di == 0.0)
                return Sv;
            Sv -= yi / di;
        }
        QL_FAIL("no critical price found for the holder-extensible option");
    }

    BlackScholesCalculator AnalyticHolderExtensibleOptionEngine::bsCalculator(Real spot, Option::Type optionType) const
    {
        Time T2 = secondExpiryTime();
        Time t1 = firstExpiryTime();

        // What's left after the first expiry, forward from there.
        return {ext::make_shared<PlainVanillaPayoff>(optionType, arguments_.secondStrike), spot,
                dividendDiscount(T2) / dividendDiscount(t1), std::sqrt(variance(T2) - variance(t1)),
                riskFreeDiscount(T2) / riskFreeDiscount(t1)};
    }

    Real AnalyticHolderExtensibleOptionEngine::M2(Real a, Real b, Real d, Real rho) const
    {
        // a is -inf with no upper level and b is +inf with no lower one, and the
        // bivariate distribution can't take an infinite argument.
        BivariateCumulativeNormalDistributionWe04DP M(rho);
        Real upper = std::isinf(b) ? CumulativeNormalDistribution()(d) : M(b, d);
        Real lower = std::isinf(a) ? 0.0 : M(a, d);
        return upper - lower;
    }

    Real AnalyticHolderExtensibleOptionEngine::N2(Real a, Real b) const
    {
        CumulativeNormalDistribution NormDist;
        return NormDist(b) - NormDist(a);
    }

    Real AnalyticHolderExtensibleOptionEngine::strike() const
    {
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        return payoff->strike();
    }

    Time AnalyticHolderExtensibleOptionEngine::firstExpiryTime() const
    {
        return process_->time(arguments_.exercise->lastDate());
    }

    Time AnalyticHolderExtensibleOptionEngine::secondExpiryTime() const
    {
        return process_->time(arguments_.secondExpiryDate);
    }

    Real AnalyticHolderExtensibleOptionEngine::variance(Time t) const
    {
        return process_->blackVolatility()->blackVariance(t, strike());
    }

    DiscountFactor AnalyticHolderExtensibleOptionEngine::dividendDiscount(Time t) const
    {
        return process_->dividendYield()->discount(t);
    }

    DiscountFactor AnalyticHolderExtensibleOptionEngine::riskFreeDiscount(Time t) const
    {
        return process_->riskFreeRate()->discount(t);
    }

}
