/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2004 Walter Penschke

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

/*! \file poissondistribution.hpp
    \brief Poisson distribution
*/

#ifndef quantlib_poisson_distribution_hpp
#define quantlib_poisson_distribution_hpp

#include <ql/math/factorial.hpp>
#include <ql/math/incompletegamma.hpp>
#include <limits>

namespace QuantLib
{

    //! Poisson distribution function
    /*! Given an integer \f$ k \f$, it returns its probability
        in a Poisson distribution.

        \test the correctness of the returned value is tested by
              checking it against known good results.
    */
    class PoissonDistribution
    {
      public:
        PoissonDistribution(Real mu);
        // function
        Real operator()(BigNatural k) const;

      private:
        Real mu_, logMu_;
    };


    //! Cumulative Poisson distribution function
    /*! The probability of at most \f$ k \f$ events, which is \f$ Q(k + 1, \mu) \f$, the complementary incomplete
        gamma function (incompleteGammaFunctionComplement). So it keeps its relative accuracy where it is small.

        \test the correctness of the returned value is tested by
              checking it against known good results.
    */
    class CumulativePoissonDistribution
    {
      public:
        CumulativePoissonDistribution(Real mu) : mu_(mu) {}
        Real operator()(BigNatural k) const { return incompleteGammaFunctionComplement(k + 1, mu_); }

      private:
        Real mu_;
    };


    //! Inverse cumulative Poisson distribution function
    /*! \test the correctness of the returned value is tested by
              checking it against known good results.
    */
    class InverseCumulativePoisson
    {
      public:
        InverseCumulativePoisson(Real lambda = 1.0);
        Real operator()(Real x) const;

      private:
        Real lambda_;
        Real calcSummand(BigNatural index) const;
    };


    // inline definitions

    inline PoissonDistribution::PoissonDistribution(Real mu) : mu_(mu)
    {

        QL_REQUIRE(mu_ >= 0.0, "mu must be non negative (" << mu_ << " not allowed)");

        if (mu_ != 0.0)
            logMu_ = std::log(mu_);
    }

    inline Real PoissonDistribution::operator()(BigNatural k) const
    {
        if (mu_ == 0.0)
        {
            if (k == 0)
                return 1.0;
            else
                return 0.0;
        }
        Real logFactorial = Factorial::ln(k);
        return std::exp(k * std::log(mu_) - logFactorial - mu_);
    }


    inline InverseCumulativePoisson::InverseCumulativePoisson(Real lambda) : lambda_(lambda)
    {
        QL_REQUIRE(lambda_ > 0.0, "lambda must be positive");
    }

    inline Real InverseCumulativePoisson::operator()(Real x) const
    {
        QL_REQUIRE(x >= 0.0 && x <= 1.0, "Inverse cumulative Poisson distribution is "
                                         "only defined on the interval [0,1]");

        if (x == 1.0)
            return QL_MAX_REAL;

        if (x <= std::exp(-lambda_))
            return 0.0;

        QL_REQUIRE(lambda_ < Real((std::numeric_limits<Natural>::max)()),
                   "lambda (" << lambda_ << ") exceeds the supported range");

        // Starting from the mode, since the terms at zero underflow for large means, and each
        // term follows from the one beside it, since their powers and factorials overflow.
        BigNatural index = static_cast<BigNatural>(lambda_);
        Real term = calcSummand(index);

        if (x > 0.5)
        {
            // Summing what's to the right, since 1 - x is exact where a sum up to x wouldn't be.
            // Above the mode, a geometric series bounds what's further right still.
            const Real tail = 1.0 - x;
            while (term / (1.0 - lambda_ / (index + 1.0)) > tail * QL_EPSILON)
                term *= lambda_ / ++index;

            Real sum = 0.0;
            while (index > 0)
            {
                sum += term;
                if (sum > tail)
                    return Real(index);
                term *= index / lambda_;
                --index;
            }
            // x is above exp(-lambda), so the terms from one up fell short of 1 - x only through their rounding.
            return 1.0;
        }

        // Likewise below the mode, for what's further left.  Terms this small would lose their
        // precision to the multiplications, and could stay stuck at the smallest subnormal.
        const Real smallTerm = 1e-280;
        while (index > 0)
        {
            const Real previous = term > smallTerm ? Real(term * index / lambda_) : calcSummand(index - 1);
            if (previous / (1.0 - (index - 1.0) / lambda_) <= x * QL_EPSILON)
                break;
            term = previous;
            --index;
        }
        // At zero, the term the multiplications reach can be a few ulps above exp(-lambda), the probability x was
        // checked against above, and x just above it would then stop the sum at zero.
        if (index == 0)
            term = std::exp(-lambda_);

        Real sum = term;
        while (x > sum)
        {
            ++index;
            term = term > smallTerm ? Real(term * lambda_ / index) : calcSummand(index);
            sum += term;
        }

        return Real(index);
    }

    inline Real InverseCumulativePoisson::calcSummand(BigNatural index) const
    {
        return std::exp(index * std::log(lambda_) - Factorial::ln(Natural(index)) - lambda_);
    }

}


#endif
