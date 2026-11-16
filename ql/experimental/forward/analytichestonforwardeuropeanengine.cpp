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
#include <ql/math/distributions/gammadistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <algorithm>
#include <complex>
#include <utility>
#include <vector>

namespace QuantLib
{

    namespace
    {

        // The inner rule's nodes, per variance at the reset and per panel, and the outer rule's.
        constexpr Size forwardHestonInnerOrder = 256;
        constexpr Size forwardHestonOuterOrder = 128;

        // Below this R, and where the outer range starts at zero, the outer nodes are Gauss-Jacobi's.
        constexpr Real forwardHestonJacobiMaxR = 8.0;

        // Resets this close fall back on the vanilla price over the tenor from today's variance.
        constexpr Time forwardHestonFallbackTime = 1e-11;

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

        // At rho = -1 or 1, an inner range that would end beyond this is refused, as AnalyticHestonEngine's is there.
        constexpr Real forwardHestonMaxUnitCorrelationLimit = 5.0e8;

        // The outer range ends where the variance density has less than this of its mass beyond it.
        constexpr Real forwardHestonTailMass = 1e-14;

        // A variance density whose integral over the outer range is further than this from 1 is refused.
        constexpr Real forwardHestonMassTolerance = 1e-8;

        // Below this, e^-z I_nu(z) is near the subnormal numbers, where it loses precision before it underflows, and
        // the density is summed instead.
        constexpr Real forwardHestonBesselFloor = 1e-290;

        // Whether e^-z I_nu(z) is below forwardHestonBesselFloor by more than a factor e, by the leading term of its
        // uniform expansion (DLMF 10.41.3),
        //     log(e^-z I_nu(z)) ~ nu eta(t) - z - log(2 pi nu) / 2 - log(1 + t^2) / 4,   t = z / nu,
        //     eta(t) = sqrt(1 + t^2) + log(t / (1 + sqrt(1 + t^2))),
        // which was within 0.035 of it for nu from 1 to 30000 and z from 2 sqrt(nu + 1), where the density's series
        // ends, to 50 nu.  The Bessel function costs of order nu operations, most of a price's time where R is in the
        // millions, so it isn't evaluated where it would underflow.  A wrong estimate would only take the density
        // from its other exact form, below.
        bool forwardHestonBesselUnderflows(Real nu, Real z)
        {
            if (nu < 1.0)
                return false;
            const Real t = z / nu, s = std::sqrt(1.0 + t * t);
            const Real logWeighted =
                nu * (s + std::log(t / (1.0 + s))) - z - 0.5 * std::log(2.0 * M_PI * nu) - 0.25 * std::log1p(t * t);
            return logWeighted < std::log(forwardHestonBesselFloor) - 1.0;
        }

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

        /* The outer rule's integral of the variance density over [a, b], which holds all but 1e-14 of its mass, is
           1 to within 3.1e-12 wherever the density is right (measured on every row tried).  Where the density's
           Bessel function underflowed, over a band of resets once its order R / 2 - 1 was above about 350, the
           density was 0 at some nodes and the integral short by about as much as the price; the density is summed
           there since #237, and this stays as a guard. */
        void forwardHestonCheckMass(Real mass, Real a, Real b, Real R)
        {
            QL_REQUIRE(std::fabs(mass - 1.0) <= forwardHestonMassTolerance,
                       "the density of the variance at the reset doesn't integrate to 1 over ["
                           << a << ", " << b << "]: it misses by " << mass - 1.0
                           << " (R = 4 kappa theta / sigma^2 = " << R << ")");
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

        /* For psi e^{-izk} at most e^{P - a sqrt(u)}, a > 0, the u beyond which both the integrand's bound
           e^{P - a sqrt(u)} / u and what it leaves beyond, at most 2 e^{P - a x} / (a x) with x = sqrt(u), are below
           forwardHestonInnerEps: x = y / a for the root y of y + min(log(y / 2), 2 log(y / a)) = L with
           L = P - log(forwardHestonInnerEps), at least 39.  The left side is increasing, and below L at half of
           L - log(L / 2). */
        Real forwardHestonSqrtDecayLimit(Real a, Real P)
        {
            const Real L = P - std::log(forwardHestonInnerEps);
            const auto f = [&](Real y) { return y + std::min(std::log(0.5 * y), 2.0 * std::log(y / a)) - L; };
            const Real lo = 0.5 * (L - std::log(0.5 * L));
            Real hi = 2.0 * L;
            while (f(hi) <= 0.0)
                hi *= 2.0;
            const Real x = Brent().solve(f, QL_EPSILON * hi, 0.5 * (lo + hi), lo, hi) / a;
            return x * x;
        }

        /* The u at which e^{B u - A u^2 / 2} / u has fallen to forwardHestonInnerEps, for A > 0 and B >= 0: the root
           of A u^2 / 2 - B u + log(u) = L, L = -log(forwardHestonInnerEps).  The left side is below L up to 2 B / A,
           where B u - A u^2 / 2 is positive, and increasing beyond B / A, so the root is the only one. */
        Real forwardHestonTiltedGaussLimit(Real A, Real B)
        {
            const Real L = -std::log(forwardHestonInnerEps);
            const auto f = [&](Real u) { return 0.5 * A * u * u - B * u + std::log(u) - L; };
            Real lo = std::max(2.0 * B / A, std::sqrt(L / (0.5 * A + 1.0))),
                 hi = 2.0 * B / A + std::max(std::sqrt(2.0 * L / A), 1.0);
            while (f(hi) <= 0.0)
                hi *= 2.0;
            if (f(lo) >= 0.0)
                lo = std::min(lo, 1e-300);
            return Brent().solve(f, QL_EPSILON * hi, 0.5 * (lo + hi), lo, hi);
        }

        // log psi(z), from what AnalyticHestonEngine::chF takes psi from: lnChF, except where sigma is at most 1e-6
        // and kappa at least 1e-8, where chF takes an expansion in sigma instead.
        std::complex<Real> forwardHestonLogChF(
            const AnalyticHestonEngine& engine, const std::complex<Real>& z, Time tau, Real kappa, Real sigma)
        {
            return sigma > 1e-6 || kappa < 1e-8 ? engine.lnChF(z, tau) : std::log(engine.chF(z, tau));
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
           - At rho = -1 or 1, where sqrt(1 - rho^2) is 0, psi doesn't decay along an untilted line.  There the angle
             is AnalyticHestonEngine's at rho = -1 or 1 (detail::hestonUnitCorrelationTanPhi, #238): from r alone,
             on either side of the money, and held where the line tilts against freq.  Along that line psi e^{-izk}
             decays at the rate c |r tan(phi)|, which is 0 where r is, and at least as e^{P - a sqrt(u)}
             (detail::hestonUnitCorrelationBound).  Either bound holds alone, so its limit is the nearer of the two,
             and a limit beyond forwardHestonMaxUnitCorrelationLimit is refused, before psi is taken there: where a
             and r are both 0 (rho = 1, kappa = sigma / 2 and K at F e^{-w / sigma}) psi decays at most as a power of
             u.  Where the line tilts against freq the control variate's term grows before it decays, as
             e^{B u - A u^2 / 2} with A = s^2 (1 - tan(phi)^2) and B = -tan(phi) freq.
           - There, where r is near 0, |k| U and psi's phase at U count turns that cancel: psi e^{-izk} turns at the
             net rate c |r|, and only the control variate's term turns as e^{-iuk}, until its Gaussian has decayed
             at uCv.  So where that count takes more than forwardHestonMaxPanels panels, [0, U] is split at
             min(U, uCv) and the turns counted again: over the head as |k| u, the control variate's s^2 |tan(phi)| u^2
             and psi e^{-izk}'s phase, over the tail as that phase alone, each phase by its variation over 8 points a
             doubling from a panel's first width / 8, and each part's panels held to forwardHestonPanelTurns turns.
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
            Real tanPhi, U;
            if (rho * rho != 1.0)
            {
                tanPhi = std::tan(r * freq < 0.0 ? M_PI / 12 * (freq > 0.0 ? 1.0 : -1.0) : 0.0);
                const Real cEff = c * (std::sqrt(1.0 - rho * rho) - tanPhi * r);
                QL_REQUIRE(cEff > 0.0, "the characteristic function doesn't decay along the integration line (rho "
                                           << rho << ", log(K / F) " << k << ", the variance " << v << ")");
                U = std::max(forwardHestonDecayLimit(cEff, QL_MAX_REAL), forwardHestonGaussLimit(0.5 * s2));
                if (tanPhi != 0.0)
                    U = std::max(forwardHestonDecayLimit(cEff, U), forwardHestonDecayLimit(std::fabs(k * tanPhi), U));
            }
            else
            {
                tanPhi = detail::hestonUnitCorrelationTanPhi(r, freq, s2);
                const Real cEff = c * (std::sqrt(1.0 - rho * rho) - tanPhi * r);
                const detail::HestonUnitCorrelationBound bound =
                    detail::hestonUnitCorrelationBound(rho, kappa, theta, sigma, v + kappa * theta * tau, tanPhi);
                const Real uSqrt = bound.a > 0.0 ? forwardHestonSqrtDecayLimit(bound.a, bound.P) : QL_MAX_REAL;
                // psi e^{-izk}'s limit, or uMax where that lies beyond uMax
                const auto psiLimit = [&](Real uMax)
                { return std::min(cEff > 0.0 ? forwardHestonDecayLimit(cEff, uMax) : uMax, uSqrt); };
                const Real uPsi = psiLimit(QL_MAX_REAL);
                QL_REQUIRE(uPsi <= forwardHestonMaxUnitCorrelationLimit,
                           "at rho = " << rho << ", the variance " << v << ", log(K / F) " << k << " and a tenor of "
                                       << tau << " the integrand decays too slowly for an integration limit: it would"
                                       << " be " << uPsi << ", past the " << forwardHestonMaxUnitCorrelationLimit
                                       << " this engine takes");
                if (tanPhi * freq >= 0.0)
                {
                    U = std::max(uPsi, forwardHestonGaussLimit(0.5 * s2));
                    if (tanPhi != 0.0)
                        U = std::max(psiLimit(U), forwardHestonDecayLimit(std::fabs(k * tanPhi), U));
                }
                else
                    U = std::max(uPsi, forwardHestonTiltedGaussLimit(s2 * (1.0 - tanPhi * tanPhi), -tanPhi * freq));
            }

            const Real phase = std::fabs(
                std::imag(forwardHestonLogChF(engine, std::complex<Real>(U, U * tanPhi - 0.5), tau, kappa, sigma)));
            Real turns = (std::fabs(k) * U + phase) / (2.0 * M_PI);
            const Real firstWidth =
                std::min(forwardHestonFirstPanel / s, forwardHestonFirstPanelVolOfVol / (sigma * tau));
            Real maxWidth = turns > forwardHestonPanelTurns ? U * forwardHestonPanelTurns / turns : U;
            // one region, [0, U], unless the turns are counted again below: then a panel ends at uHead, beyond which
            // the panels are at most maxWidthTail wide
            Real uHead = U, maxWidthTail = maxWidth;
            const auto panelEnd = [&](Real a, Real width)
            {
                const Real b = std::min(U, a + std::min(width, a < uHead ? maxWidth : maxWidthTail));
                return a < uHead && b > uHead ? uHead : b;
            };
            const auto countPanels = [&]()
            {
                Size n = 0;
                for (Real a = 0.0, width = firstWidth; a < U && n <= forwardHestonMaxPanels; width *= 2.0)
                {
                    a = panelEnd(a, width);
                    ++n;
                }
                return n;
            };
            Size panels = countPanels();
            if (panels > forwardHestonMaxPanels && rho * rho == 1.0)
            {
                const Real t2 = 1.0 - tanPhi * tanPhi;
                const Real uCv = tanPhi * freq >= 0.0 ? forwardHestonGaussLimit(0.5 * s2 * t2) :
                                                        forwardHestonTiltedGaussLimit(s2 * t2, -tanPhi * freq);
                uHead = std::min(U, uCv);
                std::vector<Real> points{0.0, uHead, U};
                for (Real u = 0.125 * firstWidth; u < U; u *= std::pow(2.0, 0.125))
                    points.push_back(u);
                std::sort(points.begin(), points.end());
                points.erase(std::unique(points.begin(), points.end()), points.end());
                Real variationHead = 0.0, variationTail = 0.0, previous = 0.0;
                for (Size j = 0; j < points.size(); ++j)
                {
                    const Real u = points[j];
                    const Real angle = std::imag(forwardHestonLogChF(engine, std::complex<Real>(u, u * tanPhi - 0.5),
                                                                     tau, kappa, sigma)) -
                                       k * u;
                    if (j > 0)
                        (u <= uHead ? variationHead : variationTail) += std::fabs(angle - previous);
                    previous = angle;
                }
                const Real turnsHead =
                    (std::fabs(k) * uHead + s2 * std::fabs(tanPhi) * uHead * uHead + variationHead) / (2.0 * M_PI);
                const Real turnsTail = variationTail / (2.0 * M_PI);
                maxWidth = turnsHead > forwardHestonPanelTurns ? uHead * forwardHestonPanelTurns / turnsHead : uHead;
                maxWidthTail =
                    turnsTail > forwardHestonPanelTurns ? (U - uHead) * forwardHestonPanelTurns / turnsTail : U - uHead;
                turns = turnsHead + turnsTail;
                panels = countPanels();
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
                const Real b = panelEnd(a, width), half = 0.5 * (b - a);
                Real p1 = 0.0, p2 = 0.0;
                for (Size j = 0; j < x.size(); ++j)
                {
                    const Real u = a + half * (1.0 + x[j]);
                    const std::complex<Real> z(u, u * tanPhi - 0.5);
                    const std::complex<Real> e = -i * z * k;
                    const std::complex<Real> h = dz * (std::exp(forwardHestonLogChF(engine, z, tau, kappa, sigma) + e) -
                                                       std::exp(e - 0.5 * s2 * (z * z + i * z)));
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

        QL_REQUIRE(sigma_ > 0.0, "sigma (" << sigma_ << ") must be positive");

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
        if (resetTime <= forwardHestonFallbackTime)
        {
            QL_REQUIRE(v0_ > 0.0, "a reset within " << forwardHestonFallbackTime
                                                    << " years is priced as a vanilla option from v0, which must"
                                                       " then be positive (v0 = "
                                                    << v0_ << ")");
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

        // The nodes and their weights, the density included, first: a density that doesn't integrate to 1 is
        // refused before any inner integral is taken.
        Array v(x.size()), weight(x.size());
        Real mass = 0.0;
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
            mass += weight[j];
        }
        forwardHestonCheckMass(scale * mass, nuLeftLimit, nuRightLimit, R_);

        // At rho = -1, and at rho = 1 where kappa >= sigma / 2, the log spot at expiry is log F - rho w / sigma, with
        // w = v + kappa theta tau, plus terms in the variance at expiry and in its integral that have rho's sign.  So
        // at the variances up to vStar, where w = -rho sigma log(K / F), the spot can't end above the strike at
        // rho = -1, nor below it at rho = 1, and P1 = P2 = 0 or 1 exactly.  They aren't analytic at vStar, and a rule
        // across it can't follow them.  Where vStar lies inside the range, the whole range's mass, checked above, is
        // taken at that value, and above vStar the rule integrates what P1 and P2 differ from it by, which goes to 0
        // at vStar: with Gauss-Legendre nodes, in sqrt(v) where the density's nodes are Gauss-Jacobi, and left out
        // where the density has at most forwardHestonTailMass of its mass there.
        if (rho_ * rho_ == 1.0 && (rho_ < 0.0 || kappa_ >= 0.5 * sigma_))
        {
            const Real vStar = -rho_ * sigma_ * logMoneyness - kappa_ * theta_ * tenor;
            if (vStar > nuLeftLimit)
            {
                const Real pExact = rho_ < 0.0 ? 0.0 : 1.0;
                const Real totalMass = scale * mass;
                if (vStar >= nuRightLimit)
                    return {pExact * totalMass, pExact * totalMass};

                const Real leftScale = sqrtV ? 0.5 * std::sqrt(vStar) : 0.5 * (vStar - nuLeftLimit);
                Real leftMass = 0.0;
                for (Size j = 0; j < x.size(); ++j)
                {
                    if (sqrtV)
                    {
                        const Real y = leftScale * (1.0 + x[j]);
                        leftMass += w[j] * 2.0 * y * propagator(resetTime, y * y);
                    }
                    else
                        leftMass += w[j] * propagator(resetTime, nuLeftLimit + leftScale * (1.0 + x[j]));
                }
                if (totalMass - leftScale * leftMass <= forwardHestonTailMass)
                    return {pExact * totalMass, pExact * totalMass};

                const bool rightSqrtV = !jacobiX_.empty();
                const Real lo = rightSqrtV ? std::sqrt(vStar) : vStar;
                const Real hi = rightSqrtV ? std::sqrt(nuRightLimit) : nuRightLimit;
                const Real rightScale = 0.5 * (hi - lo);
                Real p1 = 0.0, p2 = 0.0;
                for (Size j = 0; j < outerX_.size(); ++j)
                {
                    const Real t = lo + rightScale * (1.0 + outerX_[j]);
                    const Real vj = rightSqrtV ? t * t : t;
                    const Real wj = outerW_[j] * (rightSqrtV ? 2.0 * t : 1.0) * propagator(resetTime, vj);
                    const std::pair<Real, Real> p = forwardHestonP1P2(*nodeEngine(vj), vj, logMoneyness, tenor, kappa_,
                                                                      theta_, sigma_, rho_, innerX_, innerW_);
                    p1 += wj * (p.first - pExact);
                    p2 += wj * (p.second - pExact);
                }
                return {pExact * totalMass + rightScale * p1, pExact * totalMass + rightScale * p2};
            }
        }

        // At rho = 1 where kappa < sigma / 2 the spot can end on either side of the strike from every variance, so P1
        // and P2 are exact nowhere, but they still aren't analytic at vStar, where r = rho - sigma log(F / K) / w is
        // 0, and a rule across it converges only as a power of its order.  Where vStar lies inside the range, the
        // range below it is integrated with the density's nodes and the range above it with the nodes taken above
        // vStar in the block above, apart, but for one thing.  Above vStar the rule integrates P1 and P2 themselves,
        // which don't go to 0 there as P1 - pExact does in the block above, and the density goes as v^(R/2 - 1) at 0,
        // so as t^(R - 1) in t = sqrt(v): for R < 1 that is unbounded a distance sqrt(vStar) below the range, and
        // where vStar is small, at strikes just below F e^{-kappa theta tau / sigma}, the rule in t converges slowly
        // (with 128 nodes it priced a call reset in 5 years at R 0.2 8.2e-3 low where vStar was 2e-9).  So where
        // R < 1 the range above vStar is taken in s = v^(R/2), in which the density times dv/ds is bounded and its
        // nearest singularity, at s = 0, lies vStar^(R/2) below the range rather than sqrt(vStar).  With
        // q = (nuRightLimit / vStar)^(R/2) - 1 and y = (1 + x) / 2, a node maps to v = vStar (1 + q y)^(2/R), through
        // expm1 and log1p so that it keeps its precision where R is small, with dv/dx = q v / (R (1 + q y)); at R = 1
        // that is t.  In the block above, the same nodes would crowd vStar, where the inner integral's limit grows
        // without bound, and refuse rows that the rule in t prices.
        if (rho_ == 1.0 && kappa_ < 0.5 * sigma_)
        {
            const Real vStar = -rho_ * sigma_ * logMoneyness - kappa_ * theta_ * tenor;
            if (vStar > nuLeftLimit && vStar < nuRightLimit)
            {
                const Real leftScale = sqrtV ? 0.5 * std::sqrt(vStar) : 0.5 * (vStar - nuLeftLimit);
                Real l1 = 0.0, l2 = 0.0;
                for (Size j = 0; j < x.size(); ++j)
                {
                    Real vj, wj;
                    if (sqrtV)
                    {
                        const Real y = leftScale * (1.0 + x[j]);
                        vj = y * y;
                        wj = w[j] * 2.0 * y * propagator(resetTime, vj);
                    }
                    else
                    {
                        vj = nuLeftLimit + leftScale * (1.0 + x[j]);
                        wj = w[j] * propagator(resetTime, vj);
                    }
                    const std::pair<Real, Real> p = forwardHestonP1P2(*nodeEngine(vj), vj, logMoneyness, tenor, kappa_,
                                                                      theta_, sigma_, rho_, innerX_, innerW_);
                    l1 += wj * p.first;
                    l2 += wj * p.second;
                }
                const bool rightSqrtV = !jacobiX_.empty();
                const bool rightMap = rightSqrtV && R_ < 1.0;
                const Real lo = rightSqrtV ? std::sqrt(vStar) : vStar;
                const Real hi = rightSqrtV ? std::sqrt(nuRightLimit) : nuRightLimit;
                const Real rMap = rightMap ? std::expm1(0.5 * R_ * std::log(nuRightLimit / vStar)) : 0.0;
                const Real rightScale = rightMap ? rMap / R_ : 0.5 * (hi - lo);
                Real r1 = 0.0, r2 = 0.0;
                for (Size j = 0; j < outerX_.size(); ++j)
                {
                    Real vj, dvdx;
                    if (rightMap)
                    {
                        const Real ry = 0.5 * rMap * (1.0 + outerX_[j]);
                        vj = vStar * std::exp(std::log1p(ry) / (0.5 * R_));
                        dvdx = vj / (1.0 + ry);
                    }
                    else
                    {
                        const Real t = lo + rightScale * (1.0 + outerX_[j]);
                        vj = rightSqrtV ? t * t : t;
                        dvdx = rightSqrtV ? 2.0 * t : 1.0;
                    }
                    const Real wj = outerW_[j] * dvdx * propagator(resetTime, vj);
                    const std::pair<Real, Real> p = forwardHestonP1P2(*nodeEngine(vj), vj, logMoneyness, tenor, kappa_,
                                                                      theta_, sigma_, rho_, innerX_, innerW_);
                    r1 += wj * p.first;
                    r2 += wj * p.second;
                }
                return {leftScale * l1 + rightScale * r1, leftScale * l2 + rightScale * r2};
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

        // From I_nu's power series, the density is also the central chi-squared density times a series in
        // y = Lambda x / 4,
        //     B (x / 2)^nu e^{-x / 2} / (2 Gamma(nu + 1)) e^{-Lambda / 2} S,   S = sum_k y^k / (k! (nu + 1)_k),
        // which has no Bessel function to underflow: I_nu(sqrt(Lambda x)) is below the smallest double where
        // Lambda x is small against nu, as at long resets, where Lambda is small.  Up to y = nu + 1 the terms of S
        // are positive and fall at least as 1 / k!, so a few dozen reach QL_EPSILON.  At v0 = 0, where Lambda is
        // zero, S is 1 and the density the central one.
        const Real y = 0.25 * Lambda * x;
        if (y <= nu + 1.0)
        {
            Real S = 1.0, term = 1.0;
            for (Size k = 1; term > QL_EPSILON * S; ++k)
            {
                term *= y / (k * (nu + k));
                S += term;
            }
            Real power = nu == 0.0 ? 0.0 : nu * std::log(0.5 * x);
            return std::exp(std::log(0.5 * B) + power - 0.5 * x - GammaFunction().logValue(nu + 1) - 0.5 * Lambda +
                            std::log(S));
        }

        // Equation (18) from the paper: B times the density of x = B v, noncentral chi-squared with R degrees of
        // freedom and noncentrality Lambda,
        //     exp(-(x + Lambda) / 2) B / 2 * (x / Lambda)^(nu / 2) * I_nu(sqrt(Lambda x)).
        // The exponential underflows where the Bessel function overflows, at short resets near the density's peak
        // too, so it's taken in logs and with the exponentially weighted Bessel function, as
        //     exp(-(x + Lambda) / 2) I_nu(z) = exp(-(sqrt(x) - sqrt(Lambda))^2 / 2) e^-z I_nu(z),  z = sqrt(Lambda x).
        // A power of zero is kept at zero, so that the density at v = 0 stays finite at R = 2.
        const Real z = std::sqrt(Lambda * x);
        if (!forwardHestonBesselUnderflows(nu, z))
        {
            Real d = std::sqrt(x) - std::sqrt(Lambda);
            Real power = nu == 0.0 ? 0.0 : 0.5 * nu * std::log(x / Lambda);
            const Real weighted = modifiedBesselFunction_i_exponentiallyWeighted(nu, z);
            if (weighted >= forwardHestonBesselFloor)
                return std::exp(std::log(0.5 * B) - 0.5 * d * d + power + std::log(weighted));
        }

        // Where nu is in the hundreds, e^-z I_nu(z) underflows while z is small against nu, though y is above
        // nu + 1, over a band of resets where Lambda is a few units (#237).  There the density is taken as the
        // Poisson-weighted sum of central chi-squared densities, B times
        //     sum_j e^{-Lambda / 2} (Lambda / 2)^j / j! (x / 2)^{nu + j} e^{-x / 2} / (2 Gamma(nu + 1 + j)),
        // the same series as S above, whose terms are positive and whose ratio y / ((j + 1)(nu + 1 + j)) falls
        // through 1 at the largest, j*: summed from there each way, in logs, until a term is below QL_EPSILON of the
        // sum.
        const Real js = std::max(0.0, std::floor(0.5 * (std::sqrt(nu * nu + 4.0 * y) - nu)));
        const GammaFunction gamma;
        const Real logLargest = -0.5 * Lambda + (js == 0.0 ? 0.0 : js * std::log(0.5 * Lambda)) -
                                gamma.logValue(js + 1.0) + (nu + js) * std::log(0.5 * x) - 0.5 * x -
                                gamma.logValue(nu + 1.0 + js);
        Real sum = 1.0, term = 1.0;
        for (Real j = js; term > QL_EPSILON * sum; j += 1.0)
        {
            term *= y / ((j + 1.0) * (nu + 1.0 + j));
            sum += term;
        }
        term = 1.0;
        for (Real j = js; j > 0.0 && term > QL_EPSILON * sum; j -= 1.0)
        {
            term *= j * (nu + j) / y;
            sum += term;
        }
        return std::exp(std::log(0.5 * B) + logLargest + std::log(sum));
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
