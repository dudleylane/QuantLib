/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2002, 2003 Sadruddin Rejeb

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

#include <ql/math/distributions/gammadistribution.hpp>
#include <ql/math/incompletegamma.hpp>
#include <cmath>

namespace QuantLib
{

    Real CumulativeGammaDistribution::operator()(Real x) const
    {
        if (x <= 0.0)
            return 0.0;

        return incompleteGammaFunction(a_, x);
    }

    Real GammaFunction::logValue(Real x) const
    {
        QL_REQUIRE(x > 0.0, "positive argument required");
        // The reentrant lgamma: std::lgamma also writes the global signgam, so two threads calling it race.
        int sign;
        return lgamma_r(x, &sign);
    }

    Real GammaFunction::value(Real x) const
    {
        if (x >= 1.0)
        {
            return std::exp(logValue(x));
        }
        else
        {
            if (x > -20.0)
            {
                // \Gamma(x) = \frac{\Gamma(x+1)}{x}
                return value(x + 1.0) / x;
            }
            else
            {
                // \Gamma(-x) = -\frac{\pi}{\Gamma(x)\sin(\pi x) x}
                return -M_PI / (value(-x) * x * std::sin(M_PI * x));
            }
        }
    }
}
