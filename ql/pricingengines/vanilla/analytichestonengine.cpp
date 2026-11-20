/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004, 2005, 2008 Klaus Spanderen
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

/*! \file hestonmodel.hpp
  \brief analytic pricing engine for a heston option
  based on fourier transformation
*/

#include <ql/instruments/payoffs.hpp>
#include <ql/math/expm1.hpp>
#include <ql/math/functional.hpp>
#include <ql/math/integrals/discreteintegrals.hpp>
#include <ql/math/integrals/exponentialintegrals.hpp>
#include <ql/math/integrals/expsinhintegral.hpp>
#include <ql/math/integrals/gausslobattointegral.hpp>
#include <ql/math/integrals/kronrodintegral.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/integrals/trapezoidintegral.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <boost/math/special_functions/sign.hpp>
#include <boost/math/tools/minima.hpp>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

#if defined(QL_PATCH_MSVC)
#    pragma warning(disable : 4180)
#endif

namespace QuantLib
{

    namespace
    {

        class integrand1
        {
          private:
            const Real c_inf_;
            const std::function<Real(Real)> f_;

          public:
            integrand1(Real c_inf, std::function<Real(Real)> f) : c_inf_(c_inf), f_(std::move(f)) {}
            Real operator()(Real x) const
            {
                if ((1.0 - x) * c_inf_ > QL_EPSILON)
                    return f_(-std::log(0.5 - 0.5 * x) / c_inf_) / ((1.0 - x) * c_inf_);
                else
                    return 0.0;
            }
        };

        class integrand2
        {
          private:
            const Real c_inf_;
            const std::function<Real(Real)> f_;

          public:
            integrand2(Real c_inf, std::function<Real(Real)> f) : c_inf_(c_inf), f_(std::move(f)) {}
            Real operator()(Real x) const
            {
                if (x * c_inf_ > QL_EPSILON)
                {
                    return f_(-std::log(x) / c_inf_) / (x * c_inf_);
                }
                else
                {
                    return 0.0;
                }
            }
        };

        class integrand3
        {
          private:
            const integrand2 int_;

          public:
            integrand3(Real c_inf, const std::function<Real(Real)>& f) : int_(c_inf, f) {}

            Real operator()(Real x) const { return int_(1.0 - x); }
        };

        class u_Max
        {
          public:
            u_Max(Real c_inf, Real epsilon) : c_inf_(c_inf), logEpsilon_(std::log(epsilon)) {}

            Real operator()(Real u) const
            {
                ++evaluations_;
                return c_inf_ * u + std::log(u) + logEpsilon_;
            }

            Size evaluations() const { return evaluations_; }

          private:
            const Real c_inf_, logEpsilon_;
            mutable Size evaluations_ = 0;
        };


        class uHat_Max
        {
          public:
            uHat_Max(Real v0T2, Real epsilon) : v0T2_(v0T2), logEpsilon_(std::log(epsilon)) {}

            Real operator()(Real u) const
            {
                ++evaluations_;
                return v0T2_ * u * u + std::log(u) + logEpsilon_;
            }

            Size evaluations() const { return evaluations_; }

          private:
            const Real v0T2_, logEpsilon_;
            mutable Size evaluations_ = 0;
        };

        // The u at which the Gaussian control variate, e^{-v0 t u^2 / 2} / u, has fallen to epsilon, as the
        // Andersen-Piterbarg limit has always taken it, or Null<Real>() where that search fails.
        Real hestonGaussianLimit(Real v0, Real t, Real epsilon)
        {
            try
            {
                const Real uHatMaxGuess = std::sqrt(-std::log(epsilon) / (0.5 * v0 * t));
                return Brent().solve(uHat_Max(0.5 * v0 * t, epsilon), QL_EPSILON * uHatMaxGuess, uHatMaxGuess,
                                     0.001 * uHatMaxGuess);
            }
            catch (const Error&)
            {
                return Null<Real>();
            }
        }

        // past e^G the growth below cancels digits
        constexpr Real hestonUnitCorrelationGrowth = 2.0;
    }

    namespace detail
    {

        /* Andersen and Piterbarg's limit is where |phi(u - i (alpha + 1))| / u has fallen to epsilon, and
           Integration::andersenPiterbargIntegrationLimit solves that on two asymptotics of phi: e^{-c_inf u}, which
           leaves out the asymptotic's constant e^{Re psi} (#340), and the Gaussian at the average variance, which phi
           doesn't follow between the two regimes (#347). So u0, the larger of their roots, can stop short. Where
           |phi(u0 - i (alpha + 1))| / u0 is still above epsilon, and above QL_EPSILON, this reads the criterion off
           phi itself: it doubles past u0 until |phi| / u is below epsilon, and solves between the last two points.
           logModulus(u) is log |phi(u - i (alpha + 1))|. Where that isn't finite at a doubling point, or |phi| / u
           hasn't fallen below epsilon by 2^64 u0, the limit stays u0. AndersenPiterbarg and AndersenPiterbargOptCV
           take phi on that line. Where AngledContour, AngledContourNoCV and AsymptoticChF tilt it, and for
           AsymptoticChF, which integrates phi less its asymptotic, the read stands in for their integrand's decay, as
           the closed form did. */
        Real andersenPiterbargLimitFromChF(Real u0, Real epsilon, const std::function<Real(Real)>& logModulus)
        {
            const Real logEpsilon = std::log(epsilon);
            // log(|phi| / u) less log(epsilon), positive where |phi| / u is above epsilon
            const auto excess = [&](Real u) { return logModulus(u) - std::log(u) - logEpsilon; };
            const Real e0 = excess(u0);
            if (!(std::isfinite(e0) && e0 > 0.0 && e0 + logEpsilon > std::log(QL_EPSILON)))
                return u0;
            Real lo = u0, hi = 2.0 * u0;
            for (Size doublings = 0; doublings < 64; ++doublings)
            {
                const Real e = excess(hi);
                if (!std::isfinite(e))
                    return u0;
                if (e < 0.0)
                    return Brent().solve(excess, 1e-6 * hi, 0.5 * (lo + hi), lo, hi);
                lo = hi;
                hi *= 2.0;
            }
            return u0;
        }

        /* At rho = -1 or 1 the angled contour takes its angle from r = rho - sigma freq / w alone (#239):
           tan(phi) = -sign(r) tan(pi / 12), and 0 at r = 0, with freq = log(F / K) and w = v0 + kappa theta t. On
           the line u (1 + i tan(phi)) - i / 2 the integrand decays as e^{-(w / sigma) (sqrt(1 - rho^2) - r tan(phi)) u}
           (the AsymptoticChF expansion). Where |rho| < 1 the angle tan(pi / 12) sign(freq) where r freq < 0, and 0
           elsewhere, keeps that rate at least sqrt(1 - rho^2) w / sigma. At rho = -1 or 1 it would leave none where
           r freq >= 0, the money included: there the integrand decays only as e^{-a sqrt(u)} while it turns at the
           rate |r| w / sigma, and an adaptive rule converged falsely on it. With the angle by r alone the rate is
           (w / sigma) |r| tan(pi / 12) on either side of the money. That the line can be tilted that far, crossing
           no singularity, was measured: the integral along it agrees with the real line's within 2.2e-12 at 182
           strikes of 13 models, and with those along an eighth and a half of that slope and along tan(pi / 6)
           within 1.3e-12 at 2,999 strikes of 600 random models. */
        /* Where that line tilts against freq (r and freq of one sign, the strike between F and F e^{-rho w / sigma}),
           the factor e^{-u tan(phi) freq} in the integrand grows, and the characteristic function and its control
           variate with it, by up to e^{(tan(phi) freq)^2 / (2 vT (1 - tan(phi)^2))} before they decay, vT = vAvg t:
           by e^57 a day from expiry at v0 = theta = 0.04 and sigma 0.05, struck halfway to F e^{-rho w / sigma}, where
           the integral, a difference of the two, made the price 1.8e12 against 33.11. So there the slope is no
           steeper than keeps that growth to e^G: tan(phi)^2 = 2 G vT / (freq^2 + 2 G vT), which binds beyond
           about sqrt(26 G) standard deviations from the money. */
        Real hestonUnitCorrelationTanPhi(Real r, Real freq, Real vT)
        {
            if (r == 0.0)
                return 0.0;
            const Real sign = r > 0.0 ? -1.0 : 1.0, tanPhi = std::tan(M_PI / 12);
            if (sign * freq >= 0.0)
                return sign * tanPhi;
            const Real x = 2.0 * hestonUnitCorrelationGrowth * vT;
            return sign * std::min(tanPhi, std::sqrt(x / (freq * freq + x)));
        }

        // a and P as hestonUnitCorrelationLimit's comment, below, gives them
        HestonUnitCorrelationBound
        hestonUnitCorrelationBound(Real rho, Real kappa, Real theta, Real sigma, Real w, Real tanPhi)
        {
            const Real kappaPrime = kappa - 0.5 * rho * sigma;
            const Real a = w * std::sqrt(std::fabs(kappaPrime) / (sigma * sigma * sigma)) * M_SQRT2 *
                           std::pow(1.0 + tanPhi * tanPhi, 0.25) *
                           std::cos(M_PI_4 + 0.5 * std::fabs(std::atan(tanPhi)));
            const Real P = (w * std::max(kappaPrime, Real(0.0)) + 2.0 * kappa * theta * M_LN2) / (sigma * sigma);
            return {a, P};
        }

    }

    namespace
    {

        // past this the integration wasn't checked against a reference, and such limits are refused (#239)
        constexpr Real hestonMaxIntegrationLimit = 5.0e8;

        /* The integration limit where c_inf is 0, rho -1 or 1, for the angled contour with its control variate or
           none (#239). On the real line the characteristic function decays there at least as e^{P - a sqrt(u)},
           times a power of u that only speeds it up, with
               a = w sqrt(|kappa'| / sigma^3),   P = (w max(kappa', 0) + 2 kappa theta log 2) / sigma^2,
               w = v0 + kappa theta t,   kappa' = kappa - rho sigma / 2,
           and on the line of slope tan(phi), hestonUnitCorrelationTanPhi(r), at least as that with a times
           sqrt(2) (1 + tan(phi)^2)^{1/4} cos(pi / 4 + |phi| / 2), and as e^{-c u} with c = (w / sigma) |r tan(phi)|.
           The limit is the nearer of the two u where each bound, over u^2, falls to epsilon, with L = P - log(epsilon):
           x^2 for the root x of a x + 2 log(x) = L, and the root of c u + 2 log(u) = L; and it is at least where the
           Gaussian control variates have fallen to epsilon. Where a and c are both 0 (rho = 1, kappa = sigma / 2 and
           the strike at F e^{-w / sigma}) the decay is at most a power of u and no limit cuts it off, so u stays at
           QL_MAX_REAL; near there the limit is past hestonMaxIntegrationLimit. So it is where v0 t or vAvg t is tiny,
           minutes from expiry or with v0 near 0, from the Gaussian limits: there Gauss-Lobatto missed where the
           integrand lives and returned the control variate's value, 7.98e-5 for a call worth 5.47e-5. The cap refuses
           all three. */
        Real hestonUnitCorrelationLimit(
            Real rho, Real v0, Real kappa, Real theta, Real sigma, Real t, Real epsilon, Real freq)
        {
            QL_REQUIRE(epsilon > 0.0 && epsilon < 1.0, "epsilon (" << epsilon << ") must lie between 0 and 1");
            const Real w = v0 + kappa * theta * t, kappaPrime = kappa - 0.5 * rho * sigma;
            const Real r = rho - sigma * freq / w;
            const Real vT = (theta + (v0 - theta) * detail::decayRatio(kappa * t)) * t;
            const Real tanPhi = detail::hestonUnitCorrelationTanPhi(r, freq, vT);
            const detail::HestonUnitCorrelationBound bound =
                detail::hestonUnitCorrelationBound(rho, kappa, theta, sigma, w, tanPhi);
            const Real a = bound.a, P = bound.P;
            const Real c = w / sigma * std::fabs(r * tanPhi);
            const Real L = P - std::log(epsilon);
            Real u = QL_MAX_REAL;
            if (a > 0.0)
            {
                const auto f = [a, L](Real x) { return a * x + 2.0 * std::log(x) - L; };
                const Real lo = std::min(1.0, 0.5 * L / a), hi = std::max(L / a, 1.0);
                const Real x = Brent().solve(f, QL_EPSILON * hi, 0.5 * (lo + hi), lo, hi);
                u = x * x;
            }
            if (c > 0.0)
            {
                const auto f = [c, L](Real x) { return c * x + 2.0 * std::log(x) - L; };
                const Real lo = std::min(1.0, 0.5 * L / c), hi = std::max(L / c, 1.0);
                u = std::min(u, Brent().solve(f, QL_EPSILON * hi, 0.5 * (lo + hi), lo, hi));
            }
            // the control variate along the line, e^{-A u^2 / 2 + B u}, has fallen to epsilon at the root of
            // A u^2 / 2 - B u = -log(epsilon), with A = vT (1 - tan(phi)^2) and B = max(-tan(phi) freq, 0)
            const Real A = vT * (1.0 - tanPhi * tanPhi), B = std::max(-tanPhi * freq, Real(0.0));
            if (A > 0.0)
                u = std::max(u, (B + std::sqrt(B * B - 2.0 * A * std::log(epsilon))) / A);
            const Real uHat = hestonGaussianLimit(v0, t, epsilon);
            if (uHat != Null<Real>())
                u = std::max(u, uHat);
            QL_REQUIRE(u <= hestonMaxIntegrationLimit,
                       "with rho = " << rho << ", kappa - rho sigma / 2 = " << kappaPrime << ", log(F / K) = " << freq
                                     << ", v0 t = " << v0 * t << " and vAvg t = " << vT
                                     << " the integrand decays too slowly for an integration limit: it would be " << u
                                     << ", past the " << hestonMaxIntegrationLimit << " this engine takes");
            return u;
        }
    }

    // helper class for integration
    class AnalyticHestonEngine::Fj_Helper
    {
      public:
        Fj_Helper(Real kappa,
                  Real theta,
                  Real sigma,
                  Real v0,
                  Real s0,
                  Real rho,
                  const AnalyticHestonEngine* engine,
                  ComplexLogFormula cpxLog,
                  Time term,
                  Real strike,
                  Real ratio,
                  Size j);

        Real operator()(Real phi) const;

      private:
        const Size j_;
        //     const VanillaOption::arguments& arg_;
        const Real kappa_, theta_, sigma_, v0_, rho_;
        const ComplexLogFormula cpxLog_;

        // helper variables
        const Time term_;
        const Real x_, sx_, dd_;
        const Real rsigma_;

        const AnalyticHestonEngine* const engine_;
    };

    AnalyticHestonEngine::Fj_Helper::Fj_Helper(Real kappa,
                                               Real theta,
                                               Real sigma,
                                               Real v0,
                                               Real s0,
                                               Real rho,
                                               const AnalyticHestonEngine* const engine,
                                               ComplexLogFormula cpxLog,
                                               Time term,
                                               Real strike,
                                               Real ratio,
                                               Size j)
    : j_(j), kappa_(kappa), theta_(theta), sigma_(sigma), v0_(v0), rho_(rho), cpxLog_(cpxLog), term_(term),
      x_(std::log(s0)), sx_(std::log(strike)), dd_(x_ - std::log(ratio)), rsigma_(rho * sigma_),

      engine_(engine)
    {
    }

    Real AnalyticHestonEngine::Fj_Helper::operator()(Real phi) const
    {
        const std::complex<Real> addOnTerm = engine_ != nullptr ? engine_->addOnTerm(phi, term_, j_) : Real(0.0);

        if (cpxLog_ == Gatheral || cpxLog_ == BranchCorrection)
        {
            if (phi != 0.0)
            {
                // P_j's characteristic function is psi's at phi - i for P1 and at phi for P2, so both forms take
                // detail::hestonLnChF. Gatheral's form took (t1 - d) / sigma^2 as written, with a branch at sigma
                // <= 1e-5 that held only where sigma phi is small against kappa (#349), and Heston's original form,
                // which BranchCorrection took, cancels (t1 + d) t against twice its logarithm (#350). Taken without
                // cancelling, the original form's logarithm is d t plus the form's below, and that one doesn't cross
                // the principal branch's cut, so no branch counter is needed.
                const std::complex<Real> z(phi, (j_ == 1) ? -1.0 : 0.0);
                return std::exp(detail::hestonLnChF(kappa_, theta_, sigma_, rho_, v0_, z, term_) +
                                std::complex<Real>(0.0, phi * (dd_ - sx_)) + addOnTerm)
                           .imag() /
                       phi;
            }
            else
            {
                // use l'Hospital's rule to get lim_{phi->0}. Both forms take it: the discrete Simpson and trapezoid
                // rules take phi = 0 as a node, where BranchCorrection, with no case for it, gave NaN (#361). Its
                // closed forms difference exponentials near kappa = rho sigma and at small kappa (#363).
                //
                // The add-on term's part is the slope of its imaginary part at 0, which the limit left out (#362).
                // The term is the logarithm of a characteristic function's factor, so its imaginary part is odd in
                // phi, and the library's engines compute it near 0 without cancelling; so a central difference at
                // 1e-8 loses nothing to rounding, and agrees with their closed forms to 1e-10 of the term's size or
                // better. That calls addOnTerm with a negative phi.
                const Real h = 1e-8;
                const Real addOnSlope =
                    engine_ != nullptr ?
                        (engine_->addOnTerm(h, term_, j_) - engine_->addOnTerm(-h, term_, j_)).imag() / (2 * h) :
                        0.0;
                if (j_ == 1)
                {
                    const Real kmr = rsigma_ - kappa_;
                    if (std::fabs(kmr) > 1e-7)
                    {
                        return addOnSlope + dd_ - sx_ +
                               (std::exp(kmr * term_) * kappa_ * theta_ - kappa_ * theta_ * (kmr * term_ + 1.0)) /
                                   (2 * kmr * kmr) -
                               v0_ * (1.0 - std::exp(kmr * term_)) / (2.0 * kmr);
                    }
                    else
                        // \kappa = \rho * \sigma
                        return addOnSlope + dd_ - sx_ + 0.25 * kappa_ * theta_ * term_ * term_ + 0.5 * v0_ * term_;
                }
                else
                {
                    return addOnSlope + dd_ - sx_ -
                           (std::exp(-kappa_ * term_) * kappa_ * theta_ + kappa_ * theta_ * (kappa_ * term_ - 1.0)) /
                               (2 * kappa_ * kappa_) -
                           v0_ * (1.0 - std::exp(-kappa_ * term_)) / (2 * kappa_);
                }
            }
        }
        else
        {
            QL_FAIL("unknown complex logarithm formula");
        }
    }

    AnalyticHestonEngine::OptimalAlpha::OptimalAlpha(const Time t, const AnalyticHestonEngine* const enginePtr)
    : t_(t),
      fwd_(enginePtr->model_->process()->s0()->value() * enginePtr->model_->process()->dividendYield()->discount(t) /
           enginePtr->model_->process()->riskFreeRate()->discount(t)),
      kappa_(enginePtr->model_->kappa()), theta_(enginePtr->model_->theta()), sigma_(enginePtr->model_->sigma()),
      rho_(enginePtr->model_->rho()), eps_(std::pow(2, -int(0.5 * std::numeric_limits<Real>::digits))),
      enginePtr_(enginePtr)
    {
        km_ = k(0.0, -1);
        kp_ = k(0.0, 1);
    }

    Real AnalyticHestonEngine::OptimalAlpha::alphaMax(Real strike) const
    {
        const Real eps = 1e-8;
        const auto cm = [this](Real k) -> Real { return M(k) - t_; };

        Real alpha_max;
        const Real adx = kappa_ - sigma_ * rho_;
        if (adx > 0.0)
        {
            const Real kp_2pi = k(2 * M_PI, 1);

            alpha_max = Brent().solve(cm, eps_, 0.5 * (kp_ + kp_2pi), (1 + eps) * kp_, (1 - eps) * kp_2pi) - 1.0;
        }
        else if (adx < 0.0)
        {
            const Time tCut = -2 / (kappa_ - sigma_ * rho_ * kp_);
            if (t_ < tCut)
            {
                const Real kp_pi = k(M_PI, 1);
                alpha_max = Brent().solve(cm, eps_, 0.5 * (kp_ + kp_pi), (1 + eps) * kp_, (1 - eps) * kp_pi) - 1.0;
            }
            else
            {
                alpha_max = Brent().solve(cm, eps_, 0.5 * (1.0 + kp_), 1 + eps, (1 - eps) * kp_) - 1.0;
            }
        }
        else
        { // adx == 0.0
            const Real kp_pi = k(M_PI, 1);
            alpha_max = Brent().solve(cm, eps_, 0.5 * (kp_ + kp_pi), (1 + eps) * kp_, (1 - eps) * kp_pi) - 1.0;
        }

        QL_REQUIRE(alpha_max >= 0.0, "alpha max must be larger than zero");

        return alpha_max;
    }

    std::pair<Real, Real> AnalyticHestonEngine::OptimalAlpha::alphaGreaterZero(Real strike) const
    {
        const Real alpha_max = alphaMax(strike);

        return findMinima(eps_, std::max(2 * eps_, (1.0 - 1e-6) * alpha_max), strike);
    }

    Real AnalyticHestonEngine::OptimalAlpha::alphaMin(Real strike) const
    {
        const auto cm = [this](Real k) -> Real { return M(k) - t_; };

        const Real km_2pi = k(2 * M_PI, -1);

        const Real alpha_min =
            Brent().solve(cm, eps_, 0.5 * (km_2pi + km_), (1 - 1e-8) * km_2pi, (1 + 1e-8) * km_) - 1.0;

        QL_REQUIRE(alpha_min <= -1.0, "alpha min must be smaller than minus one");

        return alpha_min;
    }

    std::pair<Real, Real> AnalyticHestonEngine::OptimalAlpha::alphaSmallerMinusOne(Real strike) const
    {
        const Real alpha_min = alphaMin(strike);

        return findMinima(std::min(-1.0 - 1e-6, -1.0 + (1.0 - 1e-6) * (alpha_min + 1.0)), -1.0 - eps_, strike);
    }

    Real AnalyticHestonEngine::OptimalAlpha::operator()(Real strike) const
    {
        try
        {
            const std::pair<Real, Real> minusOne = alphaSmallerMinusOne(strike);
            const std::pair<Real, Real> greaterZero = alphaGreaterZero(strike);

            if (minusOne.second < greaterZero.second)
            {
                return minusOne.first;
            }
            else
            {
                return greaterZero.first;
            }
        }
        catch (const Error&)
        {
            return -0.5;
        }
    }

    Size AnalyticHestonEngine::OptimalAlpha::numberOfEvaluations() const
    {
        return evaluations_;
    }

    std::pair<Real, Real> AnalyticHestonEngine::OptimalAlpha::findMinima(Real lower, Real upper, Real strike) const
    {
        const Real freq = std::log(fwd_ / strike);

        return boost::math::tools::brent_find_minima(
            [freq, this](Real alpha) -> Real
            {
                ++evaluations_;
                const std::complex<Real> z(0, -(alpha + 1));
                return enginePtr_->lnChF(z, t_).real() - std::log(alpha * (alpha + 1)) + alpha * freq;
            },
            lower, upper, int(0.5 * std::numeric_limits<Real>::digits));
    }

    Real AnalyticHestonEngine::OptimalAlpha::M(Real k) const
    {
        const Real beta = kappa_ - sigma_ * rho_ * k;

        if (k >= km_ && k <= kp_)
        {
            const Real D = std::sqrt(beta * beta - sigma_ * sigma_ * k * (k - 1));
            return std::log((beta - D) / (beta + D)) / D;
        }
        else
        {
            const Real D_imag = std::sqrt(-(beta * beta - sigma_ * sigma_ * k * (k - 1)));

            return 2 / D_imag * (((beta > 0.0) ? M_PI : 0.0) - std::atan(D_imag / beta));
        }
    }

    Real AnalyticHestonEngine::OptimalAlpha::k(Real x, Integer sgn) const
    {
        return ((sigma_ - 2 * rho_ * kappa_) +
                sgn * std::sqrt(squared(sigma_ - 2 * rho_ * kappa_) +
                                4 * (kappa_ * kappa_ + x * x / (t_ * t_)) * (1 - rho_ * rho_))) /
               (2 * sigma_ * (1 - rho_ * rho_));
    }

    AnalyticHestonEngine::AP_Helper::AP_Helper(Time term,
                                               Real fwd,
                                               Real strike,
                                               ComplexLogFormula cpxLog,
                                               const AnalyticHestonEngine* const enginePtr,
                                               const Real alpha)
    : term_(term), fwd_(fwd), strike_(strike), freq_(std::log(fwd / strike)), cpxLog_(cpxLog), enginePtr_(enginePtr),
      alpha_(alpha), s_alpha_(std::exp(alpha * freq_))
    {
        QL_REQUIRE(enginePtr != nullptr, "pricing engine required");

        const Real v0 = enginePtr->model_->v0();
        const Real kappa = enginePtr->model_->kappa();
        const Real theta = enginePtr->model_->theta();
        const Real sigma = enginePtr->model_->sigma();
        const Real rho = enginePtr->model_->rho();

        /* Where the angled contour takes its angle from r alone, and the one exponential below (#239): at rho = -1
           or 1, where the engine's own rule is Gauss-Lobatto, the one rule that takes a limit there. Gauss-Laguerre
           and exp-sinh, which priced rho = -1 and 1 before, keep the contour and control variates they had. On this
           one, at 1 to 30 days, the default 144 nodes' prices moved by up to 6.8e-8, more of them more than 1e-6
           off; exp-sinh priced the rows where it had thrown, but with OptimalCV at 1e-8 it came out 3.6e-2 off a
           call worth 1e-15 that it had priced within 4.2e-5. The rule is enginePtr's, so a rule given separately,
           as to doCalculation, changes neither. */
        unitCorrelation_ = enginePtr->integration_->takesUnitCorrelationPath(rho);

        switch (cpxLog_)
        {
            case AndersenPiterbarg:
                // (1 - e^{-kappa term}) / (kappa term) through expm1, which keeps its precision where kappa term is
                // small, and is 1 where it is 0 (#239)
                vAvg_ = theta + (v0 - theta) * detail::decayRatio(kappa * term);
                break;
            case AndersenPiterbargOptCV:
                vAvg_ = -8.0 * std::log(enginePtr->chF(std::complex<Real>(0, alpha_), term).real()) / term;
                break;
            case AsymptoticChF:
                /* Its expansion takes log(1 - rho^2) and divides by sqrt(1 - rho^2), so at rho = -1 or 1 psi_ is
                   -inf and the control variate vanishes; Gauss-Laguerre and exp-sinh price with it as before.
                   Gauss-Lobatto takes its limit there from the angled contour's decay, worked out for AngledContour
                   and AngledContourNoCV (#239). */
                QL_REQUIRE(!unitCorrelation_, "with Gauss-Lobatto the AsymptoticChF control variate needs rho"
                                              " strictly between -1 and 1, given "
                                                  << rho << "; AngledContour and OptimalCV take rho = -1 and 1 there");
                phi_ = -(v0 + term * kappa * theta) / sigma * std::complex<Real>(std::sqrt(1 - rho * rho), rho);

                psi_ = std::complex<Real>((kappa - 0.5 * rho * sigma) * (v0 + term * kappa * theta) +
                                              kappa * theta * std::log(4 * (1 - rho * rho)),
                                          -((0.5 * rho * rho * sigma - kappa * rho) / std::sqrt(1 - rho * rho) *
                                                (v0 + kappa * theta * term) -
                                            2 * kappa * theta * std::atan(rho / std::sqrt(1 - rho * rho)))) /
                       (sigma * sigma);
                [[fallthrough]];
            case AngledContour:
                vAvg_ = theta + (v0 - theta) * detail::decayRatio(kappa * term);
                [[fallthrough]];
            case AngledContourNoCV:
            {
                const Real r = rho - sigma * freq_ / (v0 + kappa * theta * term);
                tanPhi_ = unitCorrelation_ ?
                              detail::hestonUnitCorrelationTanPhi(
                                  r, freq_, (theta + (v0 - theta) * detail::decayRatio(kappa * term)) * term) :
                              std::tan((r * freq_ < 0.0) ? M_PI / 12 * boost::math::sign(freq_) : 0.0);
            }
            break;
            default:
                QL_FAIL("unknown control variate");
        }
    }

    Real AnalyticHestonEngine::AP_Helper::operator()(Real u) const
    {
        QL_REQUIRE(enginePtr_->addOnTerm(u, term_, 1) == std::complex<Real>(0.0) &&
                       enginePtr_->addOnTerm(u, term_, 2) == std::complex<Real>(0.0),
                   "only Heston model is supported");

        constexpr std::complex<double> i(0, 1);

        if (cpxLog_ == AngledContour || cpxLog_ == AngledContourNoCV || cpxLog_ == AsymptoticChF)
        {
            const std::complex<Real> h_u(u, u * tanPhi_ - alpha_);
            const std::complex<Real> hPrime(h_u - i);

            if (unitCorrelation_)
            {
                /* The characteristic function and its control variate each times e^{-u tanPhi freq} e^{i u freq},
                   taken as one exponential: at rho = -1 or 1 the line can tilt against freq, and that factor then
                   grows, past the largest double before the integrand has decayed, while the characteristic
                   function underflows (#239). */
                const std::complex<Real> shift(-u * tanPhi_ * freq_, u * freq_);
                std::complex<Real> phiBS(0.0);
                if (cpxLog_ == AngledContour)
                    phiBS = std::exp(-0.5 * vAvg_ * term_ *
                                         (hPrime * hPrime + std::complex<Real>(-hPrime.imag(), hPrime.real())) +
                                     shift);
                const std::complex<Real> lnPhi = enginePtr_->lnChF(hPrime, term_);
                return (std::complex<Real>(1, tanPhi_) * (phiBS - std::exp(lnPhi + shift)) / (h_u * hPrime)).real() *
                       s_alpha_;
            }

            std::complex<Real> phiBS(0.0);
            if (cpxLog_ == AngledContour)
                phiBS = std::exp(-0.5 * vAvg_ * term_ *
                                 (hPrime * hPrime + std::complex<Real>(-hPrime.imag(), hPrime.real())));
            else if (cpxLog_ == AsymptoticChF)
                phiBS = std::exp(u * std::complex<Real>(1, tanPhi_) * phi_ + psi_);

            return std::exp(-u * tanPhi_ * freq_) *
                   (std::exp(std::complex<Real>(0.0, u * freq_)) * std::complex<Real>(1, tanPhi_) *
                    (phiBS - enginePtr_->chF(hPrime, term_)) / (h_u * hPrime))
                       .real() *
                   s_alpha_;
        }
        else if (cpxLog_ == AndersenPiterbarg || cpxLog_ == AndersenPiterbargOptCV)
        {
            const std::complex<Real> z(u, -alpha_);
            const std::complex<Real> zPrime(u, -alpha_ - 1);
            const std::complex<Real> phiBS =
                std::exp(-0.5 * vAvg_ * term_ * (zPrime * zPrime + std::complex<Real>(-zPrime.imag(), zPrime.real())));

            return (std::exp(std::complex<Real>(0.0, u * freq_)) * (phiBS - enginePtr_->chF(zPrime, term_)) /
                    (z * zPrime))
                       .real() *
                   s_alpha_;
        }
        else
            QL_FAIL("unknown control variate");
    }

    Real AnalyticHestonEngine::AP_Helper::controlVariateValue() const
    {
        if (cpxLog_ == AngledContour || cpxLog_ == AndersenPiterbarg || cpxLog_ == AndersenPiterbargOptCV)
        {
            return BlackCalculator(Option::Call, strike_, fwd_, std::sqrt(vAvg_ * term_)).value();
        }
        else if (cpxLog_ == AsymptoticChF)
        {
            QL_REQUIRE(alpha_ == -0.5, "alpha must be equal to -0.5");

            const std::complex<Real> phiFreq(phi_.real(), phi_.imag() + freq_);

            using namespace ExponentialIntegral;
            return fwd_ - std::sqrt(strike_ * fwd_) / M_PI *
                              (std::exp(psi_) * (-2.0 * Ci(-0.5 * phiFreq) * std::sin(0.5 * phiFreq) +
                                                 std::cos(0.5 * phiFreq) * (M_PI + 2.0 * Si(0.5 * phiFreq))))
                                  .real();
        }
        else if (cpxLog_ == AngledContourNoCV)
        {
            return ((alpha_ <= 0.0) ? fwd_ : 0.0) - ((alpha_ <= -1.0) ? strike_ : 0.0) -
                   0.5 * ((alpha_ == 0.0) ? fwd_ : 0.0) + 0.5 * ((alpha_ == -1.0) ? strike_ : 0.0);
        }
        else
            QL_FAIL("unknown control variate");
    }

    std::complex<Real> AnalyticHestonEngine::chF(const std::complex<Real>& z, Time t) const
    {
        /* From lnChF throughout. Where sigma <= 1e-6 and kappa >= 1e-8 this took an expansion to second order in
           sigma, whose coefficients cancel as kappa t falls and are then divided by kappa up to its fourth power. At
           sigma 1e-6, kappa 1e-7 and a month it was 4 % off psi at z = 50 - i/2, and the default engine's
           at-the-money call came out 1.6 % low (#348). */
        return std::exp(lnChF(z, t));
    }

    std::complex<Real> AnalyticHestonEngine::lnChF(const std::complex<Real>& z, Time t) const
    {
        return detail::hestonLnChF(model_->kappa(), model_->theta(), model_->sigma(), model_->rho(), model_->v0(), z,
                                   t);
    }

    std::complex<Real>
    detail::hestonLnChF(Real kappa, Real theta, Real sigma, Real rho, Real v0, const std::complex<Real>& z, Time t)
    {

        /* sigma = 0's limit, within O(sigma) of log psi: the variance follows its mean, and log psi is a normal's
           with the integrated variance. The form below multiplies kappa theta / sigma^2 by a factor of order
           sigma^2, so it needs sigma^2 normal, as it is above about 1.5e-154; where sigma^2 is 0, below about
           2.2e-162, it is NaN (#348, #355). */
        if (sigma <= 1e-100)
        {
            const Real vInt = (theta + (v0 - theta) * detail::decayRatio(kappa * t)) * t;
            return -0.5 * vInt * z * std::complex<Real>(z.real(), z.imag() + 1);
        }

        const Real sigma2 = sigma * sigma;

        const std::complex<Real> g = kappa + rho * sigma * std::complex<Real>(z.imag(), -z.real());

        const std::complex<Real> D = std::sqrt(g * g + (z * z + std::complex<Real>(-z.imag(), z.real())) * sigma2);

        // reduce cancelation errors, see. L. Andersen and M. Lake
        std::complex<Real> r(g - D);
        if (g.real() * D.real() + g.imag() * D.imag() > 0.0)
        {
            r = -sigma2 * z * std::complex<Real>(z.real(), z.imag() + 1) / (g + D);
        }

        std::complex<Real> y;
        if (D.real() != 0.0 || D.imag() != 0.0)
        {
            y = expm1(-D * t) / (2.0 * D);
        }
        else
            y = -0.5 * t;

        const std::complex<Real> A = kappa * theta / sigma2 * (r * t - 2.0 * log1p(-r * y));
        const std::complex<Real> B = z * std::complex<Real>(z.real(), z.imag() + 1) * y / (1.0 - r * y);

        return A + v0 * B;
    }

    AnalyticHestonEngine::AnalyticHestonEngine(const ext::shared_ptr<HestonModel>& model, Size integrationOrder)
    : GenericModelEngine<HestonModel, VanillaOption::arguments, VanillaOption::results>(model), evaluations_(0),
      cpxLog_(OptimalCV), integration_(new Integration(Integration::gaussLaguerre(integrationOrder))),
      andersenPiterbargEpsilon_(Null<Real>()), alpha_(-0.5)
    {
    }

    AnalyticHestonEngine::AnalyticHestonEngine(const ext::shared_ptr<HestonModel>& model,
                                               Real relTolerance,
                                               Size maxEvaluations)
    : GenericModelEngine<HestonModel, VanillaOption::arguments, VanillaOption::results>(model), evaluations_(0),
      cpxLog_(OptimalCV),
      integration_(new Integration(Integration::gaussLobatto(relTolerance, Null<Real>(), maxEvaluations))),
      andersenPiterbargEpsilon_(1e-40), alpha_(-0.5)
    {
    }

    AnalyticHestonEngine::AnalyticHestonEngine(const ext::shared_ptr<HestonModel>& model,
                                               ComplexLogFormula cpxLog,
                                               const Integration& integration,
                                               Real andersenPiterbargEpsilon,
                                               Real alpha)
    : GenericModelEngine<HestonModel, VanillaOption::arguments, VanillaOption::results>(model), evaluations_(0),
      cpxLog_(cpxLog), integration_(new Integration(integration)), andersenPiterbargEpsilon_(andersenPiterbargEpsilon),
      alpha_(alpha)
    {
        // The form tracked its branch from one phi to the next, in increasing order, so the adaptive rules were
        // refused with it. Since #350 it is the Gatheral form, which they take.
        QL_REQUIRE(cpxLog_ != BranchCorrection || !integration.isAdaptiveIntegration(),
                   "BranchCorrection is not accepted with adaptive integration methods; Gatheral, the same form, is");
    }

    AnalyticHestonEngine::ComplexLogFormula
    AnalyticHestonEngine::optimalControlVariate(Time t, Real v0, Real kappa, Real theta, Real sigma, Real rho)
    {

        if (t > 0.15 && (v0 + t * kappa * theta) / sigma * std::sqrt(1 - rho * rho) < 0.15 &&
            ((kappa - 0.5 * rho * sigma) * (v0 + t * kappa * theta) + kappa * theta * std::log(4 * (1 - rho * rho))) /
                    (sigma * sigma) <
                0.1)
        {
            return AsymptoticChF;
        }
        else
        {
            return AngledContour;
        }
    }

    Size AnalyticHestonEngine::numberOfEvaluations() const
    {
        return evaluations_;
    }

    Real AnalyticHestonEngine::priceVanillaPayoff(const ext::shared_ptr<PlainVanillaPayoff>& payoff,
                                                  const Date& maturity) const
    {

        const ext::shared_ptr<HestonProcess>& process = model_->process();
        const Real fwd = process->s0()->value() * process->dividendYield()->discount(maturity) /
                         process->riskFreeRate()->discount(maturity);

        return priceVanillaPayoff(payoff, process->time(maturity), fwd);
    }

    Real AnalyticHestonEngine::priceVanillaPayoff(const ext::shared_ptr<PlainVanillaPayoff>& payoff,
                                                  Time maturity) const
    {

        const ext::shared_ptr<HestonProcess>& process = model_->process();
        const Real fwd = process->s0()->value() * process->dividendYield()->discount(maturity) /
                         process->riskFreeRate()->discount(maturity);

        return priceVanillaPayoff(payoff, maturity, fwd);
    }

    Real AnalyticHestonEngine::priceVanillaPayoff(const ext::shared_ptr<PlainVanillaPayoff>& payoff,
                                                  Time maturity,
                                                  Real fwd) const
    {

        Real value;

        const ext::shared_ptr<HestonProcess>& process = model_->process();
        const DiscountFactor dr = process->riskFreeRate()->discount(maturity);

        const Real strike = payoff->strike();
        const Real spot = process->s0()->value();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");

        const DiscountFactor df = spot / fwd;
        const DiscountFactor dd = dr / df;

        const Real kappa = model_->kappa();
        const Real sigma = model_->sigma();
        const Real theta = model_->theta();
        const Real rho = model_->rho();
        const Real v0 = model_->v0();

        evaluations_ = 0;

        switch (cpxLog_)
        {
            case Gatheral:
            case BranchCorrection:
            {
                const Real c_inf = std::min(0.2, std::max(0.0001, std::sqrt(1.0 - rho * rho) / sigma)) *
                                   (v0 + kappa * theta * maturity);

                const Real p1 = integration_->calculate(c_inf, Fj_Helper(kappa, theta, sigma, v0, spot, rho, this,
                                                                         cpxLog_, maturity, strike, df, 1)) /
                                M_PI;
                evaluations_ += integration_->numberOfEvaluations();

                const Real p2 = integration_->calculate(c_inf, Fj_Helper(kappa, theta, sigma, v0, spot, rho, this,
                                                                         cpxLog_, maturity, strike, df, 2)) /
                                M_PI;
                evaluations_ += integration_->numberOfEvaluations();

                switch (payoff->optionType())
                {
                    case Option::Call:
                        value = spot * dd * (p1 + 0.5) - strike * dr * (p2 + 0.5);
                        break;
                    case Option::Put:
                        value = spot * dd * (p1 - 0.5) - strike * dr * (p2 - 0.5);
                        break;
                    default:
                        QL_FAIL("unknown option type");
                }
            }
            break;
            case AndersenPiterbarg:
            case AndersenPiterbargOptCV:
            case AsymptoticChF:
            case AngledContour:
            case AngledContourNoCV:
            case OptimalCV:
            {
                const Real c_inf = std::sqrt(1.0 - rho * rho) * (v0 + kappa * theta * maturity) / sigma;

                const Real epsilon = andersenPiterbargEpsilon_ * M_PI / (std::sqrt(strike * fwd) * dr);

                /* At rho = -1 or 1, past 0.15 years, OptimalCV takes AsymptoticChF, whose control variate vanishes
                   there. Gauss-Lobatto, which threw there before, takes AngledContour instead, the control variate
                   its limit is worked out for (#239); the other rules keep what they took. */
                ComplexLogFormula finalLog = cpxLog_;
                if (cpxLog_ == OptimalCV)
                    finalLog = integration_->takesUnitCorrelationPath(rho) ?
                                   AngledContour :
                                   optimalControlVariate(maturity, v0, kappa, theta, sigma, rho);

                /* Below u of about 1 / (sigma t) the characteristic function decays as a Gaussian at the average
                   variance, as the Andersen-Piterbarg control variates' Gaussian part does, so the limit's Gaussian
                   part takes it too (#339). Until then it took v0: short of where the integrand had decayed where v0
                   is above theta, and further out than it needed where v0 is below. */
                const Real vAvg = theta + (v0 - theta) * detail::decayRatio(kappa * maturity);

                /* Where rho is -1 or 1, c_inf is 0, and the limit comes from the decay that's left along the angled
                   contour (#239). The Andersen-Piterbarg control variates integrate along the real line, where at
                   rho = -1 or 1 the integrand decays only as e^{-a sqrt(u)} while it turns at the rate
                   |rho - sigma log(F / K) / w| w / sigma, and an adaptive rule converges falsely on it. */
                const std::function<Real()> uM = [&]()
                {
                    if (c_inf != 0.0)
                        return detail::andersenPiterbargLimitFromChF(
                            Integration::andersenPiterbargIntegrationLimit(c_inf, epsilon, vAvg, maturity), epsilon,
                            [&](Real u) { return lnChF(std::complex<Real>(u, -alpha_ - 1.0), maturity).real(); });
                    QL_REQUIRE(finalLog == AngledContour || finalLog == AngledContourNoCV,
                               "at rho = " << rho
                                           << " the Andersen-Piterbarg control variates' integrand doesn't decay"
                                              " fast enough for an integration limit; AngledContour and OptimalCV"
                                              " take one there");
                    return hestonUnitCorrelationLimit(rho, v0, kappa, theta, sigma, maturity, epsilon,
                                                      std::log(fwd / strike));
                };

                const AP_Helper cvHelper(maturity, fwd, strike, finalLog, this, alpha_);

                const Real cvValue = cvHelper.controlVariateValue();

                const Real scalingFactor =
                    (cpxLog_ != OptimalCV && cpxLog_ != AsymptoticChF) ?
                        std::max(0.25, std::min(1000.0, 0.25 / std::sqrt(0.5 * vAvg * maturity))) :
                        Real(1.0);

                Real h_cv;
                if (c_inf != 0.0)
                    h_cv = fwd / M_PI * integration_->calculate(c_inf, cvHelper, uM, scalingFactor);
                else
                {
                    try
                    {
                        h_cv = fwd / M_PI * integration_->calculate(c_inf, cvHelper, uM, scalingFactor);
                    }
                    catch (const Error& e)
                    {
                        QL_FAIL("at rho = " << rho
                                            << ", where the characteristic function decays more slowly: " << e.what());
                    }
                }

                evaluations_ += integration_->numberOfEvaluations();

                switch (payoff->optionType())
                {
                    case Option::Call:
                        value = (cvValue + h_cv) * dr;
                        break;
                    case Option::Put:
                        value = (cvValue + h_cv - (fwd - strike)) * dr;
                        break;
                    default:
                        QL_FAIL("unknown option type");
                }
            }
            break;
            default:
                QL_FAIL("unknown complex log formula");
        }

        return value;
    }

    void AnalyticHestonEngine::calculate() const
    {
        // this is a european option pricer
        QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not an European option");

        // plain vanilla
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non plain vanilla payoff given");

        const Date exerciseDate = arguments_.exercise->lastDate();

        results_.value = priceVanillaPayoff(payoff, exerciseDate);
    }


    AnalyticHestonEngine::Integration::Integration(Algorithm intAlgo, ext::shared_ptr<Integrator> integrator)
    : intAlgo_(intAlgo), integrator_(std::move(integrator))
    {
    }

    AnalyticHestonEngine::Integration::Integration(Algorithm intAlgo,
                                                   ext::shared_ptr<GaussianQuadrature> gaussianQuadrature)
    : intAlgo_(intAlgo), gaussianQuadrature_(std::move(gaussianQuadrature))
    {
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::gaussLobatto(
        Real relTolerance, Real absTolerance, Size maxEvaluations, bool useConvergenceEstimate, Size minimumDepth)
    {
        return Integration(GaussLobatto,
                           ext::shared_ptr<Integrator>(new GaussLobattoIntegral(
                               maxEvaluations, absTolerance, relTolerance, useConvergenceEstimate, minimumDepth)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::gaussKronrod(Real absTolerance,
                                                                                      Size maxEvaluations)
    {
        return Integration(GaussKronrod,
                           ext::shared_ptr<Integrator>(new GaussKronrodAdaptive(absTolerance, maxEvaluations)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::simpson(Real absTolerance, Size maxIterations)
    {
        return Integration(Simpson, ext::shared_ptr<Integrator>(new SimpsonIntegral(absTolerance, maxIterations)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::trapezoid(Real absTolerance,
                                                                                   Size maxIterations)
    {
        return Integration(Trapezoid,
                           ext::shared_ptr<Integrator>(new TrapezoidIntegral<Default>(absTolerance, maxIterations)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::gaussLaguerre(Size intOrder)
    {
        QL_REQUIRE(intOrder <= 192, "maximum integraton order (192) exceeded");
        return Integration(GaussLaguerre, ext::shared_ptr<GaussianQuadrature>(new GaussLaguerreIntegration(intOrder)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::gaussLegendre(Size intOrder)
    {
        return Integration(GaussLegendre, ext::shared_ptr<GaussianQuadrature>(new GaussLegendreIntegration(intOrder)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::gaussChebyshev(Size intOrder)
    {
        return Integration(GaussChebyshev,
                           ext::shared_ptr<GaussianQuadrature>(new GaussChebyshevIntegration(intOrder)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::gaussChebyshev2nd(Size intOrder)
    {
        return Integration(GaussChebyshev2nd,
                           ext::shared_ptr<GaussianQuadrature>(new GaussChebyshev2ndIntegration(intOrder)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::discreteSimpson(Size evaluations)
    {
        return Integration(DiscreteSimpson, ext::shared_ptr<Integrator>(new DiscreteSimpsonIntegrator(evaluations)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::discreteTrapezoid(Size evaluations)
    {
        return Integration(DiscreteTrapezoid,
                           ext::shared_ptr<Integrator>(new DiscreteTrapezoidIntegrator(evaluations)));
    }

    AnalyticHestonEngine::Integration AnalyticHestonEngine::Integration::expSinh(Real relTolerance)
    {
        return Integration(ExpSinh, ext::shared_ptr<Integrator>(new ExpSinhIntegral(relTolerance)));
    }

    Size AnalyticHestonEngine::Integration::numberOfEvaluations() const
    {
        if (integrator_ != nullptr)
        {
            return headEvaluations_ + integrator_->numberOfEvaluations();
        }
        else if (gaussianQuadrature_ != nullptr)
        {
            return gaussianQuadrature_->order();
        }
        else
        {
            QL_FAIL("neither Integrator nor GaussianQuadrature given");
        }
    }

    bool AnalyticHestonEngine::Integration::takesUnitCorrelationPath(Real rho) const
    {
        return rho * rho >= 1.0 && intAlgo_ == GaussLobatto;
    }

    bool AnalyticHestonEngine::Integration::isAdaptiveIntegration() const
    {
        return intAlgo_ == GaussLobatto || intAlgo_ == GaussKronrod || intAlgo_ == Simpson || intAlgo_ == Trapezoid ||
               intAlgo_ == ExpSinh;
    }

    Real AnalyticHestonEngine::Integration::calculate(Real c_inf,
                                                      const std::function<Real(Real)>& f,
                                                      const std::function<Real()>& maxBound,
                                                      const Real scaling) const
    {

        Real retVal;
        headEvaluations_ = 0;

        switch (intAlgo_)
        {
            case GaussLaguerre:
                retVal = (*gaussianQuadrature_)(f);
                break;
            case GaussLegendre:
            case GaussChebyshev:
            case GaussChebyshev2nd:
                /* The change of variable takes every node to infinity where c_inf is 0, and the integral came out 0:
                   the price was the control variate's alone, 2 % off with the PTD engine at the money (#239). */
                QL_REQUIRE(c_inf > 0.0, "this integration maps its nodes to u through the decay rate c_inf, given "
                                            << c_inf
                                            << ": where it is 0, as at rho = -1 or 1, every node lands at infinity"
                                               " and the price would be the control variate's alone");
                retVal = (*gaussianQuadrature_)(integrand1(c_inf, f));
                break;
            case ExpSinh:
                retVal = scaling * (*integrator_)([scaling, f](Real x) -> Real { return f(scaling * x); }, 0.0,
                                                  std::numeric_limits<Real>::max());
                break;
            case Simpson:
            case Trapezoid:
            case GaussLobatto:
            case GaussKronrod:
                if (maxBound && c_inf == 0.0)
                {
                    /* Where c_inf is 0, rho -1 or 1, only Gauss-Lobatto is taken (#239). Gauss-Kronrod's first 15
                       nodes land past where the integrand lives, and it stopped after 15 evaluations 0.2 to 0.87
                       off. Simpson and trapezoid, which threw there before, weren't measured against their
                       accuracy where c_inf is positive. */
                    QL_REQUIRE(intAlgo_ == GaussLobatto, "this integration needs a positive decay rate c_inf, given "
                                                             << c_inf << "; it is zero where rho is -1 or 1");
                    const Real U = maxBound();
                    QL_REQUIRE(U != Null<Real>(), "an integration limit is needed where c_inf is 0");
                    retVal = unitCorrelationIntegral(f, U);
                }
                else if (const Real U = maxBound ? maxBound() : Null<Real>(); U != Null<Real>())
                    retVal = (*integrator_)(f, 0.0, U);
                else
                {
                    QL_REQUIRE(c_inf > 0.0, "this integration needs a positive decay rate c_inf, given " << c_inf);
                    retVal = (*integrator_)(integrand2(c_inf, f), 0.0, 1.0);
                }
                break;
            case DiscreteTrapezoid:
            case DiscreteSimpson:
                // at c_inf = 0 their fixed nodes were 1e-6 to 1e4 off, on [0, U] and as unitCorrelationIntegral splits
                // it
                QL_REQUIRE(c_inf > 0.0, "the discrete Simpson and trapezoid rules need a positive decay rate c_inf,"
                                        " given "
                                            << c_inf << "; it is zero where rho is -1 or 1");
                if (const Real U = maxBound ? maxBound() : Null<Real>(); U != Null<Real>())
                    retVal = (*integrator_)(f, 0.0, U);
                else
                    retVal = (*integrator_)(integrand3(c_inf, f), 0.0, 1.0);
                break;
            default:
                QL_FAIL("unknwon integration algorithm");
        }

        return retVal;
    }

    /* Where c_inf is 0, rho -1 or 1, the integrand can decay as slowly as e^{-a sqrt(u)} and the limit U is far out
       (#239). Gauss-Lobatto sizes its tolerance from 13 points across [0, U], whose endpoint term grows with U, while
       the integrand lives near 0: on [0, U] its error was about U / 100 times its tolerance at the money. So [0, 1]
       is integrated in u, which keeps the integrand's value at 0, and [1, U] in x = sqrt(u), as 2 x f(x^2), where
       that decay is e^{-a x}. Each part sizes its tolerance from its own integral; where [0, 1]'s is small against
       the whole's, as near the money at a few days, that tolerance falls below the integrand's rounding and [0, 1]
       doesn't converge. There [0, U] is integrated whole instead. Each integration may take the integrator's
       maxEvaluations, so up to about twice that may be taken in all, and numberOfEvaluations() counts every one. */
    Real AnalyticHestonEngine::Integration::unitCorrelationIntegral(const std::function<Real(Real)>& f, Real U) const
    {
        if (U <= 1.0)
            return (*integrator_)(f, 0.0, U);
        Real head;
        try
        {
            head = (*integrator_)(f, 0.0, 1.0);
        }
        catch (const Error&)
        {
            headEvaluations_ = integrator_->numberOfEvaluations();
            return (*integrator_)(f, 0.0, U);
        }
        headEvaluations_ = integrator_->numberOfEvaluations();
        return head + (*integrator_)([&f](Real x) { return 2.0 * x * f(x * x); }, 1.0, std::sqrt(U));
    }

    Real
    AnalyticHestonEngine::Integration::calculate(Real c_inf, const std::function<Real(Real)>& f, Real maxBound) const
    {

        return AnalyticHestonEngine::Integration::calculate(c_inf, f, [=]() { return maxBound; });
    }

    Real AnalyticHestonEngine::Integration::andersenPiterbargIntegrationLimit(Real c_inf, Real epsilon, Real v, Real t)
    {
        QL_REQUIRE(c_inf > 0.0 && std::isfinite(c_inf),
                   "the Andersen-Piterbarg integration limit needs a positive, finite decay rate c_inf, given "
                       << c_inf << "; it is zero where rho is -1 or 1");
        QL_REQUIRE(epsilon > 0.0 && epsilon < 1.0, "epsilon (" << epsilon << ") must lie between 0 and 1");

        /* uMax solves c_inf u + log(u) = L, L = -log(epsilon). The left side increases with u; it's below L at
           L / (1 + c_inf), where it's L - x + log(x) with x = L / (1 + c_inf), and at least L at
           (L + max(0, log(c_inf / L))) / c_inf and at 1 / epsilon, where log(u) alone is L. Brent searches between
           them. Until #299 it stepped down from L / c_inf in growing steps, which went below 0 and threw where
           log(u) is more than about two thirds of L, as at epsilon 1e-17 from c_inf 4e-11 down. */
        const Real L = -std::log(epsilon);
        const Real uMaxLow = L / (1.0 + c_inf);
        const Real uMaxHigh = std::min((L + std::max(0.0, std::log(c_inf / L))) / c_inf, 1.0 / epsilon);
        QL_REQUIRE(uMaxHigh < QL_MAX_REAL,
                   "the Andersen-Piterbarg integration limit for c_inf = " << c_inf << " and epsilon = " << epsilon
                                                                           << " lies past the largest double");
        const u_Max f(c_inf, epsilon);
        // where rounding leaves f at most 0 at the top, the root is the top to rounding
        const Real uMax = f(uMaxHigh) <= 0.0 ?
                              uMaxHigh :
                              Brent().solve(f, QL_EPSILON * uMaxLow, 0.5 * (uMaxLow + uMaxHigh), uMaxLow, uMaxHigh);
        const Real uHatMax = hestonGaussianLimit(v, t, epsilon);

        return uHatMax != Null<Real>() ? std::max(uMax, uHatMax) : uMax;
    }


    void AnalyticHestonEngine::doCalculation(Real riskFreeDiscount,
                                             Real dividendDiscount,
                                             Real spotPrice,
                                             Real strikePrice,
                                             Real term,
                                             Real kappa,
                                             Real theta,
                                             Real sigma,
                                             Real v0,
                                             Real rho,
                                             const TypePayoff& type,
                                             const Integration& integration,
                                             const ComplexLogFormula cpxLog,
                                             const AnalyticHestonEngine* const enginePtr,
                                             Real& value,
                                             Size& evaluations)
    {

        const Real ratio = riskFreeDiscount / dividendDiscount;

        evaluations = 0;

        switch (cpxLog)
        {
            case Gatheral:
            case BranchCorrection:
            {
                const Real c_inf =
                    std::min(0.2, std::max(0.0001, std::sqrt(1.0 - rho * rho) / sigma)) * (v0 + kappa * theta * term);

                const Real p1 =
                    integration.calculate(c_inf, Fj_Helper(kappa, theta, sigma, v0, spotPrice, rho, enginePtr, cpxLog,
                                                           term, strikePrice, ratio, 1)) /
                    M_PI;
                evaluations += integration.numberOfEvaluations();

                const Real p2 =
                    integration.calculate(c_inf, Fj_Helper(kappa, theta, sigma, v0, spotPrice, rho, enginePtr, cpxLog,
                                                           term, strikePrice, ratio, 2)) /
                    M_PI;
                evaluations += integration.numberOfEvaluations();

                switch (type.optionType())
                {
                    case Option::Call:
                        value = spotPrice * dividendDiscount * (p1 + 0.5) - strikePrice * riskFreeDiscount * (p2 + 0.5);
                        break;
                    case Option::Put:
                        value = spotPrice * dividendDiscount * (p1 - 0.5) - strikePrice * riskFreeDiscount * (p2 - 0.5);
                        break;
                    default:
                        QL_FAIL("unknown option type");
                }
            }
            break;
            case AndersenPiterbarg:
            case AndersenPiterbargOptCV:
            case AsymptoticChF:
            case OptimalCV:
            {
                const Real c_inf = std::sqrt(1.0 - rho * rho) * (v0 + kappa * theta * term) / sigma;

                const Real fwdPrice = spotPrice / ratio;

                const Real epsilon = enginePtr->andersenPiterbargEpsilon_ * M_PI /
                                     (std::sqrt(strikePrice * fwdPrice) * riskFreeDiscount);

                // the limit's Gaussian part at the average variance, as in calculate() (#339)
                const Real vAvg = theta + (v0 - theta) * detail::decayRatio(kappa * term);

                // read off phi(u - i / 2), the point AP_Helper takes at its alpha of -0.5, as in calculate() (#340)
                const std::function<Real()> uM = [&]()
                {
                    return detail::andersenPiterbargLimitFromChF(
                        Integration::andersenPiterbargIntegrationLimit(c_inf, epsilon, vAvg, term), epsilon,
                        [&](Real u) { return enginePtr->lnChF(std::complex<Real>(u, -0.5), term).real(); });
                };

                AP_Helper cvHelper(term, fwdPrice, strikePrice,
                                   (cpxLog == OptimalCV) ? optimalControlVariate(term, v0, kappa, theta, sigma, rho) :
                                                           cpxLog,
                                   enginePtr);

                const Real cvValue = cvHelper.controlVariateValue();

                // F / pi, as calculate() takes, since AP_Helper's integrand carries (F / K)^alpha (#343)
                const Real h_cv = fwdPrice / M_PI * integration.calculate(c_inf, cvHelper, uM);
                evaluations += integration.numberOfEvaluations();

                switch (type.optionType())
                {
                    case Option::Call:
                        value = (cvValue + h_cv) * riskFreeDiscount;
                        break;
                    case Option::Put:
                        value = (cvValue + h_cv - (fwdPrice - strikePrice)) * riskFreeDiscount;
                        break;
                    default:
                        QL_FAIL("unknown option type");
                }
            }
            break;

            default:
                QL_FAIL("unknown complex log formula");
        }
    }
}
