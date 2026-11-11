/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Ferdinando Ametrano

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

/*! \file incompletegamma.hpp
    \brief Incomplete Gamma function
*/

#ifndef quantlib_math_incompletegamma_h
#define quantlib_math_incompletegamma_h

#include <ql/errors.hpp>
#include <ql/types.hpp>
#include <functional>

namespace QuantLib
{

    //! Incomplete Gamma function
    /*! The regularized lower incomplete gamma function
        \f[
            P(a, x) = \frac{1}{\Gamma(a)} \int_0^x t^{a-1} e^{-t} dt
        \f]
        for \f$ a > 0 \f$ and \f$ x \ge 0 \f$, from Boost's \c gamma_p: within a few parts in \f$ 10^{16} \f$ up to
        \f$ a = 10^4 \f$, and \f$ 2 \cdot 10^{-14} \f$ at \f$ 10^6 \f$. With Boost 1.83, near \f$ x = a \f$ from a
        shape of about \f$ 2 \cdot 10^{10} \f$, it gives up and throws its own \c evaluation_error, not a
        QuantLib::Error.

        \c accuracy and \c maxIteration are ignored, and kept so that calls passing them still compile. A caller who
        passed a loose accuracy or a small limit to save time gets, and pays for, full precision.
    */
    Real incompleteGammaFunction(Real a, Real x, Real accuracy = 1.0e-13, Integer maxIteration = 100);

    //! Complementary incomplete Gamma function
    /*! \f$ Q(a, x) = 1 - P(a, x) \f$, from Boost's \c gamma_q, which keeps its relative accuracy where \f$ Q \f$ is
        small and \f$ 1 - P \f$ would lose it. Its domain and limits are those of incompleteGammaFunction.
    */
    Real incompleteGammaFunctionComplement(Real a, Real x);

    //! Series for the incomplete Gamma function
    /*! The series for \f$ P(a, x) \f$ in "Numerical Recipes in C", 2nd edition, Press, Teukolsky, Vetterling,
        Flannery, chapter 6. It stops once a term is below \c accuracy of the sum, and throws after \c maxIteration
        terms; near \f$ x = a \f$ it needs of the order of \f$ \sqrt{a} \f$ of them. incompleteGammaFunction doesn't
        use it.
    */
    Real incompleteGammaFunctionSeriesRepr(Real a, Real x, Real accuracy = 1.0e-13, Integer maxIteration = 100);
    //! Continued fraction for the complementary incomplete Gamma function
    /*! The continued fraction for \f$ Q(a, x) \f$ from the same chapter. It stops once a step changes the value by
        less than \c accuracy, and throws after \c maxIteration steps. incompleteGammaFunction doesn't use it.
    */
    Real
    incompleteGammaFunctionContinuedFractionRepr(Real a, Real x, Real accuracy = 1.0e-13, Integer maxIteration = 100);

}


#endif
