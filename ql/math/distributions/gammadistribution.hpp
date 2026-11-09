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

/*! \file gammadistribution.hpp
    \brief Gamma distribution
*/

#ifndef quantlib_math_gamma_distribution_h
#define quantlib_math_gamma_distribution_h

#include <ql/errors.hpp>
#include <ql/types.hpp>
#include <functional>

namespace QuantLib
{

    class CumulativeGammaDistribution
    {
      public:
        CumulativeGammaDistribution(Real a) : a_(a) { QL_REQUIRE(a > 0.0, "invalid parameter for gamma distribution"); }
        Real operator()(Real x) const;

      private:
        Real a_;
    };

    //! Gamma function class
    /*! This is a function defined by
        \f[
            \Gamma(z) = \int_0^{\infty}t^{z-1}e^{-t}dt
        \f]

        logValue() is the C library's \c lgamma_r, within a few units in the last place of \f$ \ln\Gamma(x) \f$.
        value() is its exponential from \f$ x = 1 \f$, so its relative error grows with \f$ |\ln\Gamma(x)| \f$, to
        about 1.2e-13 below 171.6, past which \f$ \Gamma(x) \f$ overflows.

        \test the correctness of the returned value is tested by
              checking it against known good results.
    */
    class GammaFunction
    {
      public:
        Real value(Real x) const;
        Real logValue(Real x) const;
    };

}


#endif
