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

#include <ql/experimental/forward/analytichestonforwardeuropeanengine.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <complex>
#include <utility>

namespace QuantLib
{

    namespace
    {

        // The inner rule's nodes, per variance at the reset and per panel, and the outer rule's.
        constexpr Size forwardHestonInnerOrder = 256;
        constexpr Size forwardHestonOuterOrder = 128;

        // Below this R, and where the outer range starts at zero, the outer nodes are Gauss-Jacobi's.
        constexpr Real forwardHestonJacobiMaxR = 8.0;

        // The inner range is split into panels, the first this many multiples of 1 / s wide, s^2 being the
        // integrated variance expected over the tenor, or this many multiples of 1 / (sigma tau) where that is
        // narrower, each twice as wide as the one before but at most this many turns of the integrand, and at most
        // this many panels.
        constexpr Real forwardHestonFirstPanel = 16.0;
        constexpr Real forwardHestonFirstPanelVolOfVol = 512.0;
        constexpr Real forwardHestonPanelTurns = 64.0;
        constexpr Size forwardHestonMaxPanels = 192;

        // The inner range ends where the integrand, and what it has left beyond, have decayed to this.
        constexpr Real forwardHestonInnerEps = 1e-17;

        // The outer range ends where the variance density has less than this of its mass beyond it.
        constexpr Real forwardHestonTailMass = 1e-14;

        /* The x beyond which a noncentral chi-squared variable with R degrees of freedom and noncentrality Lambda
           has probability below eps: the Chernoff bound P(X > x) <= e^{-sx} E[e^{sX}], 0 < s < 1/2, with
           E[e^{sX}] = (1 - 2s)^{-R/2} e^{Lambda s / (1 - 2s)}, solved for x and minimized over s.  The bound holds at
           every s, and is unimodal in s, so a golden-section search finds its least value. */
        Real forwardHestonChiSquaredTail(Real R, Real Lambda, Real eps)
        {
            const Real L = -std::log(eps);
            const auto bound = [&](Real s)
            { return (L - 0.5 * R * std::log1p(-2.0 * s) + Lambda * s / (1.0 - 2.0 * s)) / s; };
            const Real g = 0.5 * (std::sqrt(5.0) - 1.0);
            Real a = 0.0, b = 0.5;
            Real c = b - g * (b - a), d = a + g * (b - a);
            Real fc = bound(c), fd = bound(d);
            for (Size i = 0; i < 60; ++i)
            {
                if (fc < fd)
                {
                    b = d;
                    d = c;
                    fd = fc;
                    c = b - g * (b - a);
                    fc = bound(c);
                }
                else
                {
                    a = c;
                    c = d;
                    fc = fd;
                    d = a + g * (b - a);
                    fd = bound(d);
                }
            }
            return std::min(fc, fd);
        }

        /* For a decay at the rate c > 0, the u beyond which both e^{-cu} / u and its integral from u on, at most
           e^{-cu} / (cu), are below forwardHestonInnerEps, or uMax where that lies beyond uMax: the root of
           c u + log(u) = L with L = -log(forwardHestonInnerEps) + max(0, -log(c)).  Where c is near 1e-10, as at a
           variance near zero over a tenor of minutes, the integrand's tail beyond where e^{-cu} / u reaches eps is
           near eps / c, 1e-7.  The root lies above L / (1 + c), where c u + log(u) - L = log(x) - x < 0 with
           x = L / (1 + c), and at most at max(L / c, 1), so it's bracketed however small c is.  (AnalyticHestonEngine's
           integration limit, which stops at e^{-cu} / u = eps, searches for its bracket down from L / c in growing
           steps, which land below zero where c is below about 4e-11.) */
        Real forwardHestonDecayLimit(Real c, Real uMax)
        {
            const Real L = -std::log(forwardHestonInnerEps) + std::max(0.0, -std::log(c));
            const auto f = [&](Real u) { return c * u + std::log(u) - L; };
            uMax = std::min(uMax, std::max(L / c, 1.0));
            if (f(uMax) <= 0.0)
                return uMax;
            const Real lo = L / (1.0 + c);
            return Brent().solve(f, QL_EPSILON * uMax, 0.5 * (lo + uMax), lo, uMax);
        }

        // The u at which e^{-a u^2} / u has fallen to forwardHestonInnerEps, for a > 0: the root of
        // a u^2 + log(u) = L, which lies between sqrt(L / (1 + a)), where the left side is below L, and
        // max(sqrt(L / a), 1).
        Real forwardHestonGaussLimit(Real a)
        {
            const Real L = -std::log(forwardHestonInnerEps);
            const auto f = [&](Real u) { return a * u * u + std::log(u) - L; };
            const Real lo = std::sqrt(L / (1.0 + a)), hi = std::max(std::sqrt(L / a), 1.0);
            return Brent().solve(f, QL_EPSILON * hi, 0.5 * (lo + hi), lo, hi);
        }

        /* P1 and P2 of the Heston model over the tenor tau from the variance v, for k = log(K / F) with F the
           forward at expiry: P2 is the probability that the spot ends above K, and P1 that under the share
           measure.  Gil-Pelaez on the real line needs the characteristic function psi(u) out to where it has
           decayed, past u = 1000 at a day's tenor, where it oscillates as e^{-iuk} away from the money.  So, as
           AnalyticHestonEngine's AngledContour control variate prices a call, the integrals are taken on the
           line z = u (1 + i tan(phi)) - i/2, less the characteristic function of Black-Scholes with the expected
           integrated variance s^2, psiBS(z) = exp(-s^2 (z^2 + iz) / 2), whose probabilities are N(d2) and N(d1):
               P2 = N(d2) + 1/pi \int_0^U Re[(1 + i tan(phi)) e^{-izk} (psi - psiBS)(z) / (iz)] du,
               P1 = N(d1) + e^k/pi \int_0^U Re[(1 + i tan(phi)) e^{-izk} (psi - psiBS)(z) / (i (z + i))] du.
           Moving the real line there crosses no pole, since psi - psiBS vanishes at z = 0 and z = -i.  The angle
           is AnalyticHestonEngine's: it tilts the line where the integrand then decays faster.  On a tilted line
           psi can overflow where psi e^{-izk} doesn't, so the two are taken as one exponential.
           - U is where the integrand has decayed to forwardHestonInnerEps, as Andersen and Piterbarg take it: from
             its decay rate along the line, c (sqrt(1 - rho^2) - tan(phi) r) with c = (v + kappa theta tau) /
             sigma, as e^{-cu} / u, and from the control variate's term, which decays as a Gaussian and, on a tilted
             line, also as e^{-|k tan(phi)| u}.  Where a rate is below 1, it's what is left beyond U that's held to
             forwardHestonInnerEps (forwardHestonDecayLimit).
           - Where that rate is small, U lies far beyond the scale 1/s on which the integrand varies near zero,
             which one rule over [0, U] misses.  At a variance near zero where R is small, psi stays near 1 out to
             u near 1 / c, and the integrand falls as 1/u over decades; near rho = -1 or 1, it turns many times on
             the way out: as e^{-iuk}, and as psi, whose phase grows towards c |rho + tan(phi) sqrt(1 - rho^2)| a
             unit far out, and slowly at short tenors.  So [0, U] is split into panels, the first
             forwardHestonFirstPanel / s wide and each twice as wide as the one before, but none of more than
             forwardHestonPanelTurns turns.  The turns are counted as |k| U plus psi's phase at U, psi being real at
             u = 0, and spread evenly over [0, U]; where that phase isn't monotone along the line, it turned back by
             at most 1.4 turns on the rows measured.
           - Where sigma^2 tau is large against the variance, psi first changes on the scale 1 / (sigma tau), up to
             sqrt(8 / R) times finer than 1/s, so the first panel is at most forwardHestonFirstPanelVolOfVol /
             (sigma tau) wide.
           One evaluation of psi serves both integrals. */
        std::pair<Real, Real> forwardHestonP1P2(const AnalyticHestonEngine& engine,
                                                Real v,
                                                Real k,
                                                Time tau,
                                                Real kappa,
                                                Real theta,
                                                Real sigma,
                                                Real rho,
                                                const Array& x,
                                                const Array& w)
        {
            constexpr std::complex<Real> i(0.0, 1.0);

            // the expected average variance over the tenor, and the control variate's integrated variance
            const Real kt = kappa * tau;
            const Real g = kt == 0.0 ? 1.0 : Real(-std::expm1(-kt) / kt);
            const Real vAvg = theta + (v - theta) * g;
            const Real s2 = vAvg * tau;
            QL_REQUIRE(s2 > 0.0, "no expected variance (" << s2 << ") over the tenor from the variance " << v);
            const Real s = std::sqrt(s2);

            const Real freq = -k;
            const Real c = (v + kappa * theta * tau) / sigma;
            const Real r = rho - freq / c;
            const Real tanPhi = std::tan(r * freq < 0.0 ? M_PI / 12 * (freq > 0.0 ? 1.0 : -1.0) : 0.0);
            const Real cEff = c * (std::sqrt(1.0 - rho * rho) - tanPhi * r);
            QL_REQUIRE(cEff > 0.0, "the characteristic function doesn't decay along the integration line (rho "
                                       << rho << ", log(K / F) " << k
                                       << "), as at rho = -1 or 1 where the line isn't tilted: at the forward, and"
                                          " above it at rho = -1, below it at rho = 1");
            Real U = std::max(forwardHestonDecayLimit(cEff, QL_MAX_REAL), forwardHestonGaussLimit(0.5 * s2));
            if (tanPhi != 0.0)
                U = std::max(forwardHestonDecayLimit(cEff, U), forwardHestonDecayLimit(std::fabs(k * tanPhi), U));

            const Real phase = std::fabs(std::imag(engine.lnChF(std::complex<Real>(U, U * tanPhi - 0.5), tau)));
            const Real turns = (std::fabs(k) * U + phase) / (2.0 * M_PI);
            const Real firstWidth =
                std::min(forwardHestonFirstPanel / s, forwardHestonFirstPanelVolOfVol / (sigma * tau));
            const Real maxWidth = turns > forwardHestonPanelTurns ? U * forwardHestonPanelTurns / turns : U;
            Size panels = 0;
            for (Real a = 0.0, width = firstWidth; a < U && panels <= forwardHestonMaxPanels; width *= 2.0)
            {
                a += std::min(width, maxWidth);
                ++panels;
            }
            QL_REQUIRE(panels <= forwardHestonMaxPanels,
                       "the inner integral at the variance "
                           << v << " needs more panels than this engine takes (" << forwardHestonMaxPanels
                           << "): it turns " << turns << " times over [0, " << U << "] at a tenor of " << tau
                           << ", log(K / F) " << k << " and rho " << rho << " (1 - |rho| = " << 1.0 - std::fabs(rho)
                           << "), as at rho very near -1 or 1, or away from the forward at tenors under a second");

            const std::complex<Real> dz(1.0, tanPhi);
            Real i1 = 0.0, i2 = 0.0;
            for (Real a = 0.0, width = firstWidth; a < U; width *= 2.0)
            {
                const Real b = std::min(U, a + std::min(width, maxWidth)), half = 0.5 * (b - a);
                Real p1 = 0.0, p2 = 0.0;
                for (Size j = 0; j < x.size(); ++j)
                {
                    const Real u = a + half * (1.0 + x[j]);
                    const std::complex<Real> z(u, u * tanPhi - 0.5);
                    const std::complex<Real> e = -i * z * k;
                    const std::complex<Real> h =
                        dz * (std::exp(engine.lnChF(z, tau) + e) - std::exp(e - 0.5 * s2 * (z * z + i * z)));
                    p1 += w[j] * std::real(h / (i * (z + i)));
                    p2 += w[j] * std::real(h / (i * z));
                }
                i1 += half * p1;
                i2 += half * p2;
                a = b;
            }

            const CumulativeNormalDistribution N;
            const Real d2 = (-k - 0.5 * s2) / s;
            return {N(d2 + s) + std::exp(k) * i1 / M_PI, N(d2) + i2 / M_PI};
        }

    }


    AnalyticHestonForwardEuropeanEngine::AnalyticHestonForwardEuropeanEngine(ext::shared_ptr<HestonProcess> process,
                                                                             Size integrationOrder)
    : process_(std::move(process)), integrationOrder_(integrationOrder)
    {

        v0_ = process_->v0();
        rho_ = process_->rho();
        kappa_ = process_->kappa();
        theta_ = process_->theta();
        sigma_ = process_->sigma();
        s0_ = process_->s0();

        QL_REQUIRE(sigma_ > 0.1, "Very low values (<~10%) for Heston Vol-of-Vol cause numerical issues"
                                 "in this implementation of the propagator function, try using"
                                 "MCForwardEuropeanHestonEngine Monte-Carlo engine instead");

        riskFreeRate_ = process_->riskFreeRate();
        dividendYield_ = process_->dividendYield();

        // Some of the required constant intermediate variables can be calculated now.  R = 4 kappaHat thetaHat /
        // sigma^2 with thetaHat = kappa theta / kappaHat, taken without the division so that kappaHat = 0 works.
        kappaHat_ = kappa_ - rho_ * sigma_;
        R_ = 4 * kappa_ * theta_ / (sigma_ * sigma_);
        QL_REQUIRE(R_ > 0.0, "4 kappa theta / sigma^2 (" << R_
                                                         << ") must be positive: at zero, the variance at the reset"
                                                            " has an atom at zero, which this engine can't integrate");

        GaussLegendreIntegration inner(forwardHestonInnerOrder);
        innerX_ = inner.x();
        innerW_ = inner.weights();
        GaussLegendreIntegration outer(forwardHestonOuterOrder);
        outerX_ = outer.x();
        outerW_ = outer.weights();
        if (R_ < forwardHestonJacobiMaxR)
        {
            GaussJacobiIntegration jacobi(forwardHestonOuterOrder, 0.0, R_ - 1.0);
            jacobiX_ = jacobi.x();
            jacobiW_ = jacobi.weights();
        }
    }


    void AnalyticHestonForwardEuropeanEngine::calculate() const
    {
        // This is a european option pricer
        QL_REQUIRE(this->arguments_.exercise->type() == Exercise::European, "not an European option");

        // We only price plain vanillas
        ext::shared_ptr<PlainVanillaPayoff> payoff =
            ext::dynamic_pointer_cast<PlainVanillaPayoff>(this->arguments_.payoff);
        QL_REQUIRE(payoff, "non plain vanilla payoff given");

        Time resetTime = this->process_->time(this->arguments_.resetDate);
        Time expiryTime = this->process_->time(this->arguments_.exercise->lastDate());
        Time tenor = expiryTime - resetTime;
        Real moneyness = this->arguments_.moneyness;

        // K needs to be scaled to forward AT RESET TIME, not spot...
        Real expiryDcf = riskFreeRate_->discount(expiryTime);
        Real resetDcf = riskFreeRate_->discount(resetTime);
        Real expiryDividendDiscount = dividendYield_->discount(expiryTime);
        Real resetDividendDiscount = dividendYield_->discount(resetTime);
        Real expiryRatio = expiryDcf / expiryDividendDiscount;
        Real resetRatio = resetDcf / resetDividendDiscount;

        QL_REQUIRE(resetTime >= 0.0, "Reset Date cannot be in the past");
        QL_REQUIRE(expiryTime >= 0.0, "Expiry Date cannot be in the past");
        QL_REQUIRE(tenor > 0.0, "no time (" << tenor << ") between the reset and the expiry");

        // Use some heuristics to decide upon nuRightLimit
        Real nuRightLimit =
            std::max(2.0, 10.0 * (1 + std::max(0.0, rho_)) * sigma_ * std::sqrt(resetTime * std::max(v0_, theta_)));

        // The variance at the reset is then integrated where its density is, from its mean and standard deviation,
        // both in closed form for the square-root process that the propagator describes.  The range above is
        // orders of magnitude wider than the density at short resets, which then falls between the nodes, and
        // above a variance of 2 it can leave the density out.  So the range starts at the mean less 20 standard
        // deviations, floored at zero, and ends at the mean plus 20.  Where the range above ends between 10 and 20
        // standard deviations above the mean it keeps that end, since widening it would only make the rule coarser
        // where the density is, and where it ends below 10 the range ends at 10.  With f = (1 - e^{-kappaHat t}) /
        // kappaHat, which is t at kappaHat = 0, the mean is v0 e^{-kappaHat t} + kappa theta f and the variance
        // sigma^2 f (v0 e^{-kappaHat t} + kappa theta f / 2).
        Real e = std::exp(-kappaHat_ * resetTime);
        Real f = kappaHat_ == 0.0 ? resetTime : Real(-std::expm1(-kappaHat_ * resetTime) / kappaHat_);
        Real meanVariance = v0_ * e + kappa_ * theta_ * f;
        Real stdDevVariance = std::sqrt(sigma_ * sigma_ * f * (v0_ * e + 0.5 * kappa_ * theta_ * f));
        Real nuLeftLimit = std::max(0.0, meanVariance - 20.0 * stdDevVariance);
        nuRightLimit = std::min(meanVariance + 20.0 * stdDevVariance,
                                std::max(nuRightLimit, meanVariance + 10.0 * stdDevVariance));
        if (resetTime > 0.0)
        {
            Real B = 4.0 / (sigma_ * sigma_ * f);
            nuRightLimit =
                std::max(nuRightLimit, forwardHestonChiSquaredTail(R_, B * e * v0_, forwardHestonTailMass) / B);
        }

        // do the 2D integral calculation. For very short times, we just fall back on the standard
        // calculation, both for accuracy and because tStar==0 causes some numerical issues...
        std::pair<Real, Real> P1HatP2Hat;
        if (resetTime <= 1e-3)
        {
            Handle<Quote> tempQuote(ext::shared_ptr<Quote>(new SimpleQuote(s0_->value())));
            P1HatP2Hat = calculateP1P2(tenor, tempQuote, moneyness * s0_->value(), expiryRatio);
        }
        else
        {
            P1HatP2Hat =
                calculateP1P2Hat(tenor, resetTime, moneyness, expiryRatio / resetRatio, nuLeftLimit, nuRightLimit);
        }

        // Apply the payoff functions
        Real value = 0.0;
        Real F = s0_->value() / expiryRatio;
        switch (payoff->optionType())
        {
            case Option::Call:
                value = expiryDcf * (F * P1HatP2Hat.first - moneyness * s0_->value() * P1HatP2Hat.second / resetRatio);
                break;
            case Option::Put:
                value = expiryDcf *
                        (moneyness * s0_->value() * (1 - P1HatP2Hat.second) / resetRatio - F * (1 - P1HatP2Hat.first));
                break;
            default:
                QL_FAIL("unknown option type");
        }

        results_.value = value;

        results_.additionalResults["dcf"] = expiryDcf;
        results_.additionalResults["qf"] = expiryDividendDiscount;
        results_.additionalResults["expiryRatio"] = expiryRatio;
        results_.additionalResults["resetRatio"] = resetRatio;
        results_.additionalResults["moneyness"] = moneyness;
        results_.additionalResults["s0"] = s0_->value();
        results_.additionalResults["fwd"] = F;
        results_.additionalResults["resetTime"] = resetTime;
        results_.additionalResults["expiryTime"] = expiryTime;
        results_.additionalResults["P1Hat"] = P1HatP2Hat.first;
        results_.additionalResults["P2Hat"] = P1HatP2Hat.second;
        results_.additionalResults["nuLeftLimit"] = nuLeftLimit;
        results_.additionalResults["nuRightLimit"] = nuRightLimit;
    }


    std::pair<Real, Real> AnalyticHestonForwardEuropeanEngine::calculateP1P2Hat(
        Time tenor, Time resetTime, Real moneyness, Real ratio, Real nuLeftLimit, Real nuRightLimit) const
    {

        // Re-expressing moneyness in terms of the forward here (strike fixes to spot, but in
        // our pricing calculation we need to compare it to the future at expiry)
        Real logMoneyness = std::log(moneyness * ratio);

        // The density of the variance at the reset goes as v^(R/2 - 1) at zero, which Gauss-Legendre nodes can't
        // integrate where R is small.  Where the range starts at zero, it's integrated in y = sqrt(v), where the
        // density goes as y^(R - 2) and dv = 2y dy, by Gauss-Jacobi nodes for the weight (1 + x)^(R - 1): they
        // integrate a polynomial times that power, and so follow both the density and the inner integrals, which
        // go as sqrt(v) near zero at short tenors.
        const bool sqrtV = nuLeftLimit == 0.0 && !jacobiX_.empty();
        const Array& x = sqrtV ? jacobiX_ : outerX_;
        const Array& w = sqrtV ? jacobiW_ : outerW_;
        const Real scale = sqrtV ? 0.5 * std::sqrt(nuRightLimit) : 0.5 * (nuRightLimit - nuLeftLimit);

        Array v(x.size()), weight(x.size());
        for (Size j = 0; j < x.size(); ++j)
        {
            if (sqrtV)
            {
                const Real y = scale * (1.0 + x[j]);
                v[j] = y * y;
                weight[j] = w[j] * 2.0 * y * propagator(resetTime, v[j]);
            }
            else
            {
                v[j] = nuLeftLimit + scale * (1.0 + x[j]);
                weight[j] = w[j] * propagator(resetTime, v[j]);
            }
        }

        Real p1Hat = 0.0, p2Hat = 0.0;
        for (Size j = 0; j < x.size(); ++j)
        {
            const std::pair<Real, Real> p = forwardHestonP1P2(*nodeEngine(v[j]), v[j], logMoneyness, tenor, kappa_,
                                                              theta_, sigma_, rho_, innerX_, innerW_);
            p1Hat += weight[j] * p.first;
            p2Hat += weight[j] * p.second;
        }
        return {scale * p1Hat, scale * p2Hat};
    }


    Real AnalyticHestonForwardEuropeanEngine::propagator(Time resetTime, Real varReset) const
    {
        // B = 4 kappaHat / (sigma^2 (1 - e^{-kappaHat t})) = 4 / (sigma^2 f), with f = (1 - e^{-kappaHat t}) /
        // kappaHat, which is t at kappaHat = 0.
        Real f = kappaHat_ == 0.0 ? resetTime : Real(-std::expm1(-kappaHat_ * resetTime) / kappaHat_);
        Real B = 4 / (sigma_ * sigma_ * f);
        Real Lambda = B * std::exp(-kappaHat_ * resetTime) * v0_;
        Real x = B * varReset;
        Real nu = R_ / 2 - 1;

        // Equation (18) from the paper: B times the density of x = B v, noncentral chi-squared with R degrees of
        // freedom and noncentrality Lambda,
        //     exp(-(x + Lambda) / 2) B / 2 * (x / Lambda)^(nu / 2) * I_nu(sqrt(Lambda x)).
        // The exponential underflows where the Bessel function overflows, at short resets near the density's peak
        // too, so it's taken in logs and with the exponentially weighted Bessel function, as
        //     exp(-(x + Lambda) / 2) I_nu(z) = exp(-(sqrt(x) - sqrt(Lambda))^2 / 2) e^-z I_nu(z),  z = sqrt(Lambda x).
        // A power of zero is kept at zero, so that the density at v = 0 stays finite at R = 2.
        Real d = std::sqrt(x) - std::sqrt(Lambda);
        Real power = nu == 0.0 ? 0.0 : 0.5 * nu * std::log(x / Lambda);
        return std::exp(std::log(0.5 * B) - 0.5 * d * d + power +
                        std::log(modifiedBesselFunction_i_exponentiallyWeighted(nu, std::sqrt(Lambda * x))));
    }

    ext::shared_ptr<AnalyticHestonEngine> AnalyticHestonForwardEuropeanEngine::nodeEngine(Real varReset) const
    {
        // Only the characteristic function of the engine is used, which depends on the model alone; Gauss-Lobatto
        // is the integration cheapest to construct.
        auto hestonProcess = ext::make_shared<HestonProcess>(riskFreeRate_, dividendYield_,
                                                             Handle<Quote>(ext::make_shared<SimpleQuote>(1.0)),
                                                             varReset, kappa_, theta_, sigma_, rho_);
        return ext::make_shared<AnalyticHestonEngine>(ext::make_shared<HestonModel>(hestonProcess), 1e-8, 1000);
    }

    ext::shared_ptr<AnalyticHestonEngine> AnalyticHestonForwardEuropeanEngine::forwardChF(Handle<Quote>& spotReset,
                                                                                          Real varReset) const
    {

        // Probably a wasteful implementation here, could be improved by importing
        // only the CF-generating parts of the AnalyticHestonEngine (currently private)
        ext::shared_ptr<HestonProcess> hestonProcess(
            new HestonProcess(riskFreeRate_, dividendYield_, spotReset, varReset, kappa_, theta_, sigma_, rho_));

        ext::shared_ptr<HestonModel> hestonModel(new HestonModel(hestonProcess));

        ext::shared_ptr<AnalyticHestonEngine> analyticHestonEngine(
            new AnalyticHestonEngine(hestonModel, integrationOrder_));

        // Not sure how to pass only the chF, so just pass the whole thing for now!
        return analyticHestonEngine;
    }


    std::pair<Real, Real>
    AnalyticHestonForwardEuropeanEngine::calculateP1P2(Time tenor, Handle<Quote>& St, Real K, Real ratio) const
    {

        Real logK = std::log(K * ratio / St->value());

        return forwardHestonP1P2(*nodeEngine(v0_), v0_, logK, tenor, kappa_, theta_, sigma_, rho_, innerX_, innerW_);
    }
}
