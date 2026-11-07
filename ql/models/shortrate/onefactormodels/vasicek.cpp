/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2007 StatPro Italia srl

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

#include <ql/math/expm1.hpp>
#include <ql/models/shortrate/onefactormodels/vasicek.hpp>
#include <ql/pricingengines/blackformula.hpp>

namespace QuantLib
{

    Vasicek::Vasicek(Rate r0, Real a, Real b, Real sigma, Real lambda)
    : OneFactorAffineModel(4), r0_(r0), a_(arguments_[0]), b_(arguments_[1]), sigma_(arguments_[2]),
      lambda_(arguments_[3])
    {
        a_ = ConstantParameter(a, PositiveConstraint());
        b_ = ConstantParameter(b, NoConstraint());
        sigma_ = ConstantParameter(sigma, PositiveConstraint());
        lambda_ = ConstantParameter(lambda, NoConstraint());
    }

    Real Vasicek::A(Time t, Time T) const
    {
        // A(t,T) = exp((b + lambda sigma / a - sigma^2 / (2 a^2)) (B - tau) - sigma^2 B^2 / (4 a))
        // written as
        //     exp(-(a b + lambda sigma) tau^2 phi(a tau) + sigma^2 tau^3 h(a tau) / 4)
        // which doesn't cancel, and has the right limit for a -> 0.
        const Time tau = T - t;
        const Real sigma2 = sigma() * sigma();
        Real phi, h;
        detail::decayPhiAndH(a() * tau, phi, h);
        return std::exp(-(a() * b() + lambda() * sigma()) * tau * tau * phi + 0.25 * sigma2 * tau * tau * tau * h);
    }

    Real Vasicek::B(Time t, Time T) const
    {
        const Time tau = T - t;
        return tau * detail::decayRatio(a() * tau);
    }

    Real Vasicek::discountBondOption(Option::Type type, Real strike, Time maturity, Time bondMaturity) const
    {

        Real v;
        if (std::fabs(maturity) < QL_EPSILON)
        {
            v = 0.0;
        }
        else
        {
            // 0.5 * (1 - exp(-2 a maturity)) / a
            const Real variance = maturity * detail::decayRatio(2.0 * a() * maturity);
            v = sigma() * B(maturity, bondMaturity) * std::sqrt(variance);
        }
        Real f = discountBond(0.0, bondMaturity, r0_);
        Real k = discountBond(0.0, maturity, r0_) * strike;

        return blackFormula(type, k, f, v);
    }

}
