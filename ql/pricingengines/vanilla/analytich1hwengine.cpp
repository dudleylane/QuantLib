/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 Klaus Spanderen

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

/*! \file analytichestonengine.cpp
    \brief analytic Heston-Hull-White engine based on the H1-HW approximation
*/

#include <ql/math/distributions/gammadistribution.hpp>
#include <ql/math/expm1.hpp>
#include <ql/pricingengines/vanilla/analytich1hwengine.hpp>

namespace QuantLib
{
    // integration helper class
    class AnalyticH1HWEngine::Fj_Helper
    {

      public:
        Fj_Helper(const Handle<HestonModel>& hestonModel,
                  const ext::shared_ptr<HullWhite>& hullWhiteModel,
                  Real rho_xr,
                  Time term,
                  Real strike,
                  Size j);

        std::complex<Real> operator()(Real u) const;

      private:
        Real c(Time t) const;
        Real lambda(Time t) const;
        Real Lambda(Time t) const;
        Real LambdaApprox(Time t) const;

        const Size j_;
        const Real lambda_, eta_;
        const Real v0_, kappa_, theta_, gamma_;
        const Real d_;
        const Real rhoSr_;
        const Time term_;
    };

    AnalyticH1HWEngine::Fj_Helper::Fj_Helper(const Handle<HestonModel>& hestonModel,
                                             const ext::shared_ptr<HullWhite>& hullWhiteModel,
                                             Real rhoSr,
                                             Time term,
                                             Real,
                                             Size j)
    : j_(j), lambda_(hullWhiteModel->a()), eta_(hullWhiteModel->sigma()), v0_(hestonModel->v0()),
      kappa_(hestonModel->kappa()), theta_(hestonModel->theta()), gamma_(hestonModel->sigma()),
      d_(4.0 * kappa_ * theta_ / (gamma_ * gamma_)), rhoSr_(rhoSr), term_(term)
    {
    }

    Real AnalyticH1HWEngine::Fj_Helper::c(Time t) const
    {
        return gamma_ * gamma_ / (4 * kappa_) * (1.0 - std::exp(-kappa_ * t));
    }

    Real AnalyticH1HWEngine::Fj_Helper::lambda(Time t) const
    {
        return 4.0 * kappa_ * v0_ * std::exp(-kappa_ * t) / (gamma_ * gamma_ * (1.0 - std::exp(-kappa_ * t)));
    }

    Real AnalyticH1HWEngine::Fj_Helper::LambdaApprox(Time t) const
    {
        return std::sqrt(c(t) * (lambda(t) - 1.0) + c(t) * d_ * (1.0 + 1.0 / (2.0 * (d_ + lambda(t)))));
    }

    Real AnalyticH1HWEngine::Fj_Helper::Lambda(Time t) const
    {
        const GammaFunction g = GammaFunction();
        const Size maxIter = 1000;
        const Real lambdaT = lambda(t);

        Size i = 0;
        Real retVal = 0.0, s;

        do
        {
            Real k = static_cast<Real>(i);
            s = std::exp(k * std::log(0.5 * lambdaT) + g.logValue(0.5 * (1 + d_) + k) - g.logValue(k + 1) -
                         g.logValue(0.5 * d_ + k));
            retVal += s;
            // The terms rise to their largest and then fall, so the sum is done once a term is below QL_EPSILON
            // times it. A stop at float epsilon ended it after its first term where d is below about 1.35e-7, and
            // the fit was NaN (#368).
        } while (s > QL_EPSILON * retVal && ++i < maxIter);

        // The sum grows like e^{lambda/2} sqrt(lambda/2): where lambda is above about 1413 it overflows, at first
        // while its terms are still finite, and the stop ends at once, so a sum that isn't finite is refused (#372).
        QL_REQUIRE(i < maxIter && std::isfinite(retVal), "can not calculate Lambda");

        retVal *= std::sqrt(2 * c(t)) * std::exp(-0.5 * lambdaT);
        return retVal;
    }

    std::complex<Real> AnalyticH1HWEngine::Fj_Helper::operator()(Real u) const
    {

        const Real gamma2 = gamma_ * gamma_;

        Real a, b, c;
        if (8.0 * kappa_ * theta_ / gamma2 > 1.0)
        {
            a = std::sqrt(theta_ - gamma2 / (8.0 * kappa_));
            b = std::sqrt(v0_) - a;
            c = -std::log((LambdaApprox(1.0) - a) / b);
        }
        else
        {
            a = std::sqrt(gamma2 / (2.0 * kappa_)) *
                std::exp(GammaFunction().logValue(0.5 * (d_ + 1.0)) - GammaFunction().logValue(0.5 * d_));

            const Time t1 = 0.0;
            const Time t2 = 1.0 / kappa_;

            const Real Lambda_t1 = std::sqrt(v0_);
            const Real Lambda_t2 = Lambda(t2);

            c = std::log((Lambda_t2 - a) / (Lambda_t1 - a)) / (t1 - t2);
            b = std::exp(c * t1) * (Lambda_t1 - a);
        }

        // I4 integrates the fit a + b e^{-ct} against 1 - e^{-lambda (T - t)} over [0, T], lambda being the
        // Hull-White a: that is lambda T^2 [a phi(lambda T) + b R(cT, (lambda - c) T)], with
        // phi(x) = (e^{-x} - 1 + x) / x^2, R(u, v) = (D(u) - D(u + v)) / v and D(x) = (1 - e^{-x}) / x. Taken so,
        // it divides by neither lambda, c nor c - lambda, which the differences of exponentials did (#369).
        Real phi, h;
        detail::decayPhiAndH(lambda_ * term_, phi, h);
        const std::complex<Real> I4 = -std::complex<Real>(u * u, ((j_ == 1U) ? -u : u)) * term_ * term_ *
                                      (a * phi + b * detail::decayRatioDifference(c * term_, (lambda_ - c) * term_));

        return eta_ * rhoSr_ * I4;
    }


    AnalyticH1HWEngine::AnalyticH1HWEngine(const ext::shared_ptr<HestonModel>& model,
                                           const ext::shared_ptr<HullWhite>& hullWhiteModel,
                                           Real rhoSr,
                                           Size integrationOrder)
    : AnalyticHestonHullWhiteEngine(model, hullWhiteModel, integrationOrder), rhoSr_(rhoSr)
    {
        QL_REQUIRE(rhoSr_ >= 0.0, "Fourier integration is not stable if "
                                  "the equity interest rate correlation is negative");
    }

    AnalyticH1HWEngine::AnalyticH1HWEngine(const ext::shared_ptr<HestonModel>& model,
                                           const ext::shared_ptr<HullWhite>& hullWhiteModel,
                                           Real rhoSr,
                                           Real relTolerance,
                                           Size maxEvaluations)
    : AnalyticHestonHullWhiteEngine(model, hullWhiteModel, relTolerance, maxEvaluations), rhoSr_(rhoSr)
    {
    }

    std::complex<Real> AnalyticH1HWEngine::addOnTerm(Real u, Time t, Size j) const
    {
        return AnalyticHestonHullWhiteEngine::addOnTerm(u, t, j) +
               Fj_Helper(model_, hullWhiteModel_, rhoSr_, t, 0.0, j)(u);
    }
}
