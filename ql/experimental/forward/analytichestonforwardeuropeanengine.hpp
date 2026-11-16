/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 Jack Gillett

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

/*! \file analytichestonforwardeuropeanengine.hpp
    \brief analytic heston engine for forward-starting european options
*/

#ifndef quantlib_analytic_heston_forward_european_engine_hpp
#define quantlib_analytic_heston_forward_european_engine_hpp

#include <ql/exercise.hpp>
#include <ql/instruments/forwardvanillaoption.hpp>
#include <ql/math/integrals/gaussianquadratures.hpp>
#include <ql/math/modifiedbessel.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantLib
{

    //! Analytic Heston engine for forward-starting european options
    /*! This class is pricing a european option under the following process

        \f[
        \begin{array}{rcl}
        dS(t, S)  &=& (r-d) S dt +\sqrt{v} S dW_1 \\
        dv(t, S)  &=& \kappa (\theta - v) dt + \sigma \sqrt{v} dW_2 \\
        dW_1 dW_2 &=& \rho dt \\
        \end{array}
        \f]

        References:

        Implements the analytical solution for forward-starting
        strike-reset options descriped in "On the Pricing of Forward
        Starting Options under Stochastic Volatility", S. Kruse (2003)

        The price integrates, over the variance at the reset, that
        variance's density times the probabilities P1 and P2 of a
        vanilla option from the reset to the expiry.
        - P1 and P2 are taken at each variance by Gil-Pelaez, along
          the line on which AnalyticHestonEngine's AngledContour
          control variate prices a call, less a Black-Scholes control
          variate, with 256 Gauss-Legendre nodes out to where the
          integrand, and where it decays slowly what it leaves
          beyond, have decayed to 1e-17.  The range is split into
          panels, the first 16 / s wide, with s^2 the variance
          expected over the tenor, or 512 / (sigma tau) where that
          is narrower, and each twice as wide as the one before, up
          to 64 turns of the integrand.  Where that would take more
          than 192 panels, as at rho very near -1 or 1, or away from
          the forward at tenors under a second, the engine throws.
        - The variance at the reset is integrated with 128
          Gauss-Jacobi nodes in its square root where its range
          starts at zero and R = 4 kappa theta / sigma^2 is below 8,
          and with 128 Gauss-Legendre nodes otherwise.  The range
          runs from 20 standard deviations below the mean, floored at
          zero, to at least 10 above it, and on to where at most
          1e-14 of the density's mass lies beyond it.
        - The density of the variance at the reset is noncentral
          chi-squared.  It's summed from its power series, which has
          no Bessel function to underflow, except where the series'
          terms grow at first; there it's taken from its Bessel
          function, and where that underflows, as it does over a band
          of resets where R is in the hundreds or more, from its
          Poisson-weighted sum of central chi-squared densities,
          summed from the largest term.  A density that misses 1 by
          more than 1e-8 over the range is refused.  Accuracy falls
          with R: against independent references, prices were within
          1e-12 for R up to 12,800, 8.4e-10 up to R 1.28 million and
          3.4e-9 up to R 32 million, where the shortest resets tried
          were refused.
        - A reset within 1e-11 years of today is priced as a vanilla
          option from v0, which must then be positive.
        - At rho = -1 or 1 the characteristic function doesn't decay
          along an untilted line.  There the line takes the angle
          AnalyticHestonEngine takes there, from
          r = rho - sigma log(F / K) / (v + kappa theta tau) alone,
          on either side of the forward, and a range past 5e8 is
          refused.  Where r is near 0, the integrand's turns from the
          strike and from the characteristic function cancel; where
          the count above would take more than 192 panels, the turns
          are counted again, net of that.
        - At rho = -1 the spot can't end above the strike from a
          variance at the reset below
          v* = -rho sigma log(K / F) - kappa theta tau, nor, at
          rho = 1 where kappa >= sigma / 2, below it, so P1 and P2
          are exactly 0 or 1 there, and not analytic at v*.  Where v*
          lies inside the range, the range up to it takes that value
          and the rest is integrated apart, with 128 Gauss-Legendre
          nodes, in the variance's square root where the range's
          nodes are Gauss-Jacobi.  At rho = 1 where
          kappa < sigma / 2, P1 and P2 are exact nowhere but still
          not analytic at v*, and the two sides of it are integrated
          apart, the side above v* in v^(R/2) where R < 1.  Against
          independent references, prices were within 6e-13 for R 8
          and 0.83, and within 1.7e-8 of the closed form at
          kappa = sigma / 2 with R = 0.4; for R 0.0036 there was none
          with a reset after today.  A variance node next to v*,
          where r is near 0, can take the range past 5e8, and the
          engine throws: where v* is near the range's low end and the
          tenor is short, and at rho = 1 where kappa < sigma / 2 for
          strikes within about 1e-3 below
          F e^{-kappa theta tau / sigma} in log terms (#304).
        - Just inside -1 and 1 the engine keeps its line for
          abs(rho) < 1, and throws within about 1e-7 of them where
          that line isn't tilted: at the forward, above it near -1
          and below it near 1 (#303).

        The integration order is used only by forwardChF(); the
        engine's own integrations don't depend on it.

        \ingroup forwardengines

        \test
        - For tReset > 0, price from the analytic pricer is compared
          to the MC priver for calls/puts at various moneynesses
        - For tReset ~ 0, price from the analytic pricer is compared
          to the Heston analytic vanilla pricer for various options
    */

    class AnalyticHestonForwardEuropeanEngine
    : public GenericEngine<ForwardOptionArguments<VanillaOption::arguments>, VanillaOption::results>
    {
      public:
        explicit AnalyticHestonForwardEuropeanEngine(ext::shared_ptr<HestonProcess> process,
                                                     Size integrationOrder = 144);

        void calculate() const override;

        // The evolution probability function from t0 to tReset
        Real propagator(Time resetTime, Real varReset) const;

        // Forward characteristic function for given (t, vt, St)
        ext::shared_ptr<AnalyticHestonEngine> forwardChF(Handle<Quote>& spotReset, Real varReset) const;

      private:
        // Parameters for the internal chF generators
        ext::shared_ptr<HestonProcess> process_;
        Size integrationOrder_;

        // Initial process params
        Real v0_, rho_, kappa_, theta_, sigma_;
        Handle<YieldTermStructure> dividendYield_;
        Handle<YieldTermStructure> riskFreeRate_;
        Handle<Quote> s0_;

        // Some intermediate calculation constant parameters
        Real kappaHat_, R_;

        // The nodes and weights of the fixed Gauss rules on [-1, 1]: the inner rule's, over the characteristic
        // function, and the outer rules', over the variance at the reset, Gauss-Legendre's and, where R is small,
        // Gauss-Jacobi's in its square root.  This pricer needs to perform nested integrations, and an adaptive
        // outer integrator can cause very long runtimes.
        Array innerX_, innerW_, outerX_, outerW_, jacobiX_, jacobiW_;

        // Integrate over characteristic function to generate P1, P2 (fall back on this
        // when reset time is very close, to avoid numerical issues with tReset=0s)
        std::pair<Real, Real> calculateP1P2(Time t, Handle<Quote>& St, Real K, Real ratio) const;

        // Integrate P1, P2 over te propagator function to calculate forward-start price
        std::pair<Real, Real>
        calculateP1P2Hat(Time tenor, Time resetTime, Real K, Real ratio, Real nuLeftLimit, Real nuRightLimit) const;

        // An engine for the characteristic function at a variance at the reset
        ext::shared_ptr<AnalyticHestonEngine> nodeEngine(Real varReset) const;
    };
}

#endif
