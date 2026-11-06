/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2007 StatPro Italia srl
 Copyright (C) 2023 Klaus Spanderen

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

#include <ql/any.hpp>
#include <ql/exercise.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/functional.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/pricingengines/vanilla/baroneadesiwhaleyengine.hpp>
#include <ql/pricingengines/vanilla/bjerksundstenslandengine.hpp>
#include <cmath>
#include <utility>

namespace QuantLib
{

    namespace
    {

        CumulativeNormalDistribution cumNormalDist;

        // At low variance the powers of I / S below overflow where the normal
        // tail they multiply, or the exponential they divide by, is out of
        // range too.  The result is small, but inf * 0 and inf / inf are NaN,
        // so those two cases go through logarithms.  Elsewhere these are the
        // plain expressions.
        Real logErfc(Real x)
        {
            if (x < 26.0)
                return std::log(std::erfc(x));
            // erfc(x) has underflowed, or nearly: use its asymptotic series
            const Real t = 0.5 / (x * x);
            return -x * x - std::log(x * M_SQRTPI) + std::log1p(t * (-1.0 + t * (3.0 - 15.0 * t)));
        }

        // pow(base, p) * erfc(x)
        Real powTimesErfc(Real base, Real p, Real x)
        {
            const Real power = std::pow(base, p);
            const Real tail = std::erfc(x);
            if (std::isfinite(power) && tail > 1.0e-300)
                return power * tail;
            return std::exp(p * std::log(base) + logErfc(x));
        }

        // pow(base, p) / exp(z)
        Real powOverExp(Real base, Real p, Real z)
        {
            const Real power = std::pow(base, p);
            const Real divisor = std::exp(z);
            if (std::isfinite(power) && std::isfinite(divisor))
                return power / divisor;
            return std::exp(p * std::log(base) - z);
        }

        // expm1(x) / x, with its limits
        Real expm1Ratio(Real x)
        {
            if (x == 0.0)
                return 1.0;
            if (std::isinf(x))
                return x > 0.0 ? x : 0.0;
            return std::expm1(x) / x;
        }

        // (x * exp(x) - expm1(x)) / x^2, with its limit at 0
        Real boundaryWeight(Real x)
        {
            if (std::fabs(x) < 1.0e-3)
                return 0.5 + x * (1.0 / 3.0 + x * (0.125 + x / 30.0));
            return (x * std::exp(x) - std::expm1(x)) / (x * x);
        }

        Real phi(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real variance)
        {

            Real lambda = (-rT + gamma * bT + 0.5 * gamma * (gamma - 1.0) * variance);
            Real d = -(std::log(S / H) + (bT + (gamma - 0.5) * variance)) / std::sqrt(variance);
            Real kappa = 2.0 * bT / variance + (2.0 * gamma - 1.0);
            Real d2 = d - 2.0 * std::log(I / S) / std::sqrt(variance);
            Real power = std::pow(I / S, kappa);
            Real tail = cumNormalDist(d2);
            Real reflected =
                (std::isfinite(power) && tail > 0.0) ? power * tail : 0.5 * powTimesErfc(I / S, kappa, -d2 / M_SQRT2);
            return std::exp(lambda) * (cumNormalDist(d) - reflected);
        }

        Real phi_S(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);
            const Real lis = std::log(I / S);
            const Real sv = std::sqrt(v);

            return std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2.) *
                   ((-(powOverExp(I / S, 2 * (gamma + bT / v),
                                  squared(2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (8. * v)) /
                       I) -
                     1 / (std::exp(squared(2 * bT - v + 2 * gamma * v + 2 * lsh) / (8. * v)) * S)) /
                        (M_SQRT2 * M_SQRTPI * sv) +
                    (powTimesErfc(I / S, 2 * (gamma + bT / v),
                                  (2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (2. * M_SQRT2 * sv)) *
                     (2 * bT + (-1 + 2 * gamma) * v)) /
                        (2. * I * v));
        }

        Real phi_SS(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);
            const Real lis = std::log(I / S);
            const Real sv = std::sqrt(v);
            const Real powerOverEx = powOverExp(I / S, 2 * (gamma + bT / v),
                                                squared(2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (8. * v));
            const Real ey = std::exp(squared(2 * bT + (-1 + 2 * gamma) * v + 2 * lsh) / (8. * v));

            return (std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2.) *
                    ((M_SQRT2 * I * v * sv) / ey +
                     2 * M_SQRT2 * powerOverEx * S * sv * (2 * bT + (-1 + 2 * gamma) * v) -
                     2 * std::sqrt(M_PI) *
                         powTimesErfc(I / S, 2 * (gamma + bT / v),
                                      (2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (2. * M_SQRT2 * sv)) *
                         S * (bT + gamma * v) * (2 * bT + (-1 + 2 * gamma) * v) +
                     (M_SQRT2 * I * sv * (bT + (-0.5 + gamma) * v + lsh)) / ey -
                     powerOverEx * S * sv * (2 * bT - 3 * v + 2 * gamma * v + 4 * lis + 2 * lsh) / M_SQRT2)) /
                   (2. * I * M_SQRTPI * squared(S * v));
        }

        Real phi_gamma(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);
            const Real lis = std::log(I / S);
            const Real sv = std::sqrt(v);

            return std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2) *
                   (((-std::exp(-squared(2 * bT - v + 2 * gamma * v + 2 * lsh) / (8 * v)) +
                      powOverExp(I / S, -1 + 2 * gamma + (2 * bT) / v,
                                 squared(2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (8 * v))) *
                     sv) /
                        (M_SQRT2 * M_SQRTPI) +
                    ((2 * bT + (-1 + 2 * gamma) * v) *
                     std::erfc((2 * bT + (-1 + 2 * gamma) * v + 2 * lsh) / (2. * M_SQRT2 * sv))) /
                        4. -
                    (powTimesErfc(I / S, -1 + 2 * gamma + (2 * bT) / v,
                                  (2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (2. * M_SQRT2 * sv)) *
                     (2 * bT + (-1 + 2 * gamma) * v + 4 * lis)) /
                        4.);
        }

        Real phi_H(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);

            return (std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2.) *
                    (I / std::exp(squared(2 * bT - v + 2 * gamma * v + 2 * lsh) / (8. * v)) -
                     powOverExp(I / S, 2 * (gamma + bT / v),
                                squared(2 * bT - v + 2 * gamma * v + 4 * std::log(I / S) + 2 * lsh) / (8. * v)) *
                         S)) /
                   (H * I * std::sqrt(2 * M_PI) * std::sqrt(v));
        }

        Real phi_I(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);
            const Real lis = std::log(I / S);
            const Real sv = std::sqrt(v);

            return (std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2.) * S *
                    ((2 * std::sqrt(2 / M_PI)) *
                         powOverExp(I / S, 2 * (gamma + bT / v),
                                    squared(2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (8. * v)) /
                         sv +
                     (1 - 2 * gamma - (2 * bT) / v) *
                         powTimesErfc(I / S, 2 * (gamma + bT / v),
                                      (2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (2. * M_SQRT2 * sv)))) /
                   (2. * I * I);
        }

        Real phi_rt(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);
            return (std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2.) *
                    (-(I * std::erfc((2 * bT - v + 2 * gamma * v + 2 * lsh) / (2. * std::sqrt(2 * v)))) +
                     powTimesErfc(I / S, 2 * (gamma + bT / v),
                                  (2 * bT - v + 2 * gamma * v + 4 * std::log(I / S) + 2 * lsh) /
                                      (2. * std::sqrt(2 * v))) *
                         S)) /
                   (2. * I);
        }

        Real phi_bt(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);
            const Real lis = std::log(I / S);
            const Real sv = std::sqrt(v);

            return (std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2.) *
                    (M_SQRT2 *
                         (-(I / std::exp(squared(2 * bT - v + 2 * gamma * v + 2 * lsh) / (8. * v))) +
                          powOverExp(I / S, 2 * (gamma + bT / v),
                                     squared(2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (8. * v)) *
                              S) *
                         sv +
                     gamma * I * std::sqrt(M_PI) * v *
                         std::erfc((2 * bT - v + 2 * gamma * v + 2 * lsh) / (2. * M_SQRT2 * sv)) -
                     M_SQRTPI *
                         powTimesErfc(I / S, 2 * (gamma + bT / v),
                                      (2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (2. * M_SQRT2 * sv)) *
                         S * (gamma * v + 2 * lis))) /
                   (2. * I * std::sqrt(M_PI) * v);
        }

        Real phi_v(Real S, Real gamma, Real H, Real I, Real rT, Real bT, Real v)
        {
            const Real lsh = std::log(S / H);
            const Real lis = std::log(I / S);
            const Real sv = std::sqrt(v);
            const Real x = (2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (2. * M_SQRT2 * sv);

            return (std::exp(bT * gamma - rT + ((-1 + gamma) * gamma * v) / 2.) *
                    (((-1 + gamma) * gamma *
                      (I * std::erfc((2 * bT - v + 2 * gamma * v + 2 * lsh) / (2. * M_SQRT2 * sv)) -
                       powTimesErfc(I / S, 2 * (gamma + bT / v), x) * S)) /
                         (2. * I) +
                     (2 * bT * powTimesErfc(I / S, -1 + 2 * gamma + (2 * bT) / v, x) * lis) / (v * v) +
                     (2 * bT + v - 2 * gamma * v + 2 * lsh) /
                         (2. * std::exp(std::pow(2 * bT + (-1 + 2 * gamma) * v + 2 * lsh, 2) / (8. * v)) * M_SQRT2 *
                          M_SQRTPI * v * sv) -
                     (powOverExp(I / S, -1 + 2 * gamma + (2 * bT) / v,
                                 squared(2 * bT - v + 2 * gamma * v + 4 * lis + 2 * lsh) / (8. * v)) *
                      (2 * bT + v - 2 * gamma * v + 4 * lis + 2 * lsh)) /
                         (2. * M_SQRT2 * M_SQRTPI * v * sv))) /
                   2.;
        }
    }

    BjerksundStenslandApproximationEngine::BjerksundStenslandApproximationEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process))
    {
        registerWith(process_);
    }

    OneAssetOption::results
    BjerksundStenslandApproximationEngine::europeanCallResults(Real S, Real X, Real rfD, Real dD, Real variance) const
    {

        OneAssetOption::results results;

        const Real forwardPrice = S * dD / rfD;
        const BlackCalculator black(Option::Call, X, forwardPrice, std::sqrt(variance), rfD);

        results.value = black.value();
        results.delta = black.delta(S);
        results.gamma = black.gamma(S);

        const DayCounter rfdc = process_->riskFreeRate()->dayCounter();
        const DayCounter divdc = process_->dividendYield()->dayCounter();
        const DayCounter voldc = process_->blackVolatility()->dayCounter();
        Time t = rfdc.yearFraction(process_->riskFreeRate()->referenceDate(), arguments_.exercise->lastDate());
        results.rho = black.rho(t);

        t = divdc.yearFraction(process_->dividendYield()->referenceDate(), arguments_.exercise->lastDate());
        results.dividendRho = black.dividendRho(t);

        t = voldc.yearFraction(process_->blackVolatility()->referenceDate(), arguments_.exercise->lastDate());
        results.vega = black.vega(t);
        results.theta = black.theta(S, t);
        results.thetaPerDay = black.thetaPerDay(S, t);

        results.strikeSensitivity = black.strikeSensitivity();
        results.additionalResults["strikeGamma"] = Real(results.gamma * squared(S / X));
        results.additionalResults["exerciseType"] = std::string("European");

        return results;
    }

    OneAssetOption::results BjerksundStenslandApproximationEngine::immediateExercise(Real S, Real X) const
    {
        OneAssetOption::results results;
        results.value = std::max(0.0, S - X);
        results.delta = (S >= X) ? 1.0 : 0.0;
        results.gamma = 0.0;
        results.rho = 0.0;
        results.dividendRho = 0.0;
        results.vega = 0.0;
        results.theta = 0.0;
        results.thetaPerDay = 0.0;

        results.strikeSensitivity = -results.delta;
        results.additionalResults["strikeGamma"] = Real(0.0);
        results.additionalResults["exerciseType"] = std::string("Immediate");

        return results;
    }

    OneAssetOption::results BjerksundStenslandApproximationEngine::zeroVarianceCallResults(
        Real S, Real X, Real rfD, Real dD, Real variance) const
    {
        const PlainVanillaPayoff payoff(Option::Call, X);
        const BlackCalculator black(Option::Call, X, S * dD / rfD, std::sqrt(variance), rfD);
        const detail::FixedExerciseResults best = detail::bestFixedExercise(payoff, S, dD, rfD, black);

        if (best.exerciseFraction == 1.0)
            return europeanCallResults(S, X, rfD, dD, variance);
        if (best.exerciseFraction == 0.0)
            return immediateExercise(S, X);

        // Exercise at a known time before expiry, which moves with the spot
        // and the strike but not with the volatility or the expiry.
        const Date refDate = process_->riskFreeRate()->referenceDate();
        const Date exerciseDate = arguments_.exercise->lastDate();
        const Time tr = process_->riskFreeRate()->dayCounter().yearFraction(refDate, exerciseDate);
        const Time tq = process_->dividendYield()->dayCounter().yearFraction(refDate, exerciseDate);
        const Real s = best.exerciseFraction;

        OneAssetOption::results results;
        results.value = best.value;
        results.delta = best.delta;
        results.gamma = best.gamma;
        results.rho = X * s * tr * std::pow(rfD, s);
        results.dividendRho = -S * s * tq * std::pow(dD, s);
        results.vega = 0.0;
        results.theta = 0.0;
        results.thetaPerDay = 0.0;
        results.strikeSensitivity = -std::pow(rfD, s);
        results.additionalResults["strikeGamma"] = Real(results.gamma * squared(S / X));
        results.additionalResults["exerciseType"] = std::string("American");

        return results;
    }

    OneAssetOption::results BjerksundStenslandApproximationEngine::americanCallApproximation(
        Real S, Real X, Real rfD, Real dD, Real variance) const
    {

        const OneAssetOption::results europeanResults = europeanCallResults(S, X, rfD, dD, variance);

        OneAssetOption::results results;

        const Real bT = std::log(dD / rfD);
        const Real rT = std::log(1.0 / rfD);

        // This is (0.5 - bT / v) + sqrt((bT / v - 0.5)^2 + 2 rT / v), arranged so
        // that nothing cancels.  dD <= 1 here, so the radicand is a sum of two
        // terms that aren't negative, and beta is never below 1.  With q = 0 the
        // second term is zero and beta is exactly 1 for r >= -variance / 2.  At
        // low variance it is lost beside the first, so beta - 1 divides by the
        // conjugate root + shift instead of subtracting.
        const Real shift = bT / variance + 0.5;
        const Real radicandTerm = -2.0 * std::log(dD) / variance;
        const Real root = std::sqrt(squared(shift) + radicandTerm);
        const Real betaMinusOne = shift > 0.0 ? radicandTerm / (root + shift) : root - shift;
        const Real beta = 1.0 + betaMinusOne;

        const Real BInfinity = beta / betaMinusOne * X;
        const Real B0 = (bT == rT) ? X : std::max(X, rT / (rT - bT) * X);
        const Real a = (bT + 2.0 * std::sqrt(variance)) * B0;
        const Real ht = -a / (BInfinity - B0);

        // This is B0 + (BInfinity - B0) * (1 - exp(ht)), in a form that holds
        // as beta tends to 1: with q = 0 and -variance / 2 <= r < 0 beta is 1,
        // BInfinity is infinite, and I is B0 + a.
        const Real I = B0 + a * expm1Ratio(ht);

        const Real fwd = S * dD / rfD;
        const Real q = std::log(I / fwd) / std::sqrt(variance);

        if (S >= I)
        {
            results = immediateExercise(S, X);
        }
        else if (q > 12.5)
        {
            // We have a run away exercise boundary. It is numerically
            // more accurate to use the Greeks of the European engine.
            results = europeanResults;
        }
        else
        {
            const Real phi_S_beta_I_I_rT_bT_v = phi(S, beta, I, I, rT, bT, variance);
            const Real phi_S_1_I_I_rT_bT_v = phi(S, 1.0, I, I, rT, bT, variance);
            const Real phi_S_1_X_I_rT_bT_V = phi(S, 1.0, X, I, rT, bT, variance);
            results.value = (I - X) * std::pow(S / I, beta) * (1 - phi_S_beta_I_I_rT_bT_v) + S * phi_S_1_I_I_rT_bT_v -
                            S * phi_S_1_X_I_rT_bT_V - X * phi(S, 0.0, I, I, rT, bT, variance) +
                            X * phi(S, 0.0, X, I, rT, bT, variance);

            const Real phi_S_S_beta_I_I_rT_bT_v = phi_S(S, beta, I, I, rT, bT, variance);
            const Real phi_S_S_1_I_I_rT_bT_v = phi_S(S, 1.0, I, I, rT, bT, variance);
            const Real phi_S_S_1_X_I_rT_bT_v = phi_S(S, 1.0, X, I, rT, bT, variance);
            results.delta = (I - X) * std::pow(S / I, beta - 1) * beta / I * (1 - phi_S_beta_I_I_rT_bT_v) -
                            (I - X) * std::pow(S / I, beta) * phi_S_S_beta_I_I_rT_bT_v + phi_S_1_I_I_rT_bT_v +
                            S * phi_S_S_1_I_I_rT_bT_v - phi_S_1_X_I_rT_bT_V - S * phi_S_S_1_X_I_rT_bT_v -
                            X * phi_S(S, 0.0, I, I, rT, bT, variance) + X * phi_S(S, 0.0, X, I, rT, bT, variance);

            const Date refDate = process_->riskFreeRate()->referenceDate();
            const Date exerciseDate = arguments_.exercise->lastDate();
            const DayCounter qdc = process_->dividendYield()->dayCounter();
            const Time tq = qdc.yearFraction(refDate, exerciseDate);

            // beta's derivatives divide by the root, which vanishes only at q = 0
            // and r = -variance / 2.  There beta has a kink in r and in the
            // volatility, where the ratio below takes the side with beta = 1, and
            // a vertical tangent in q, where betaDq is merely very large.
            const Real betaRoot = std::max(root, QL_EPSILON);
            const Real betaRatio = root > 0.0 ? Real(betaMinusOne / root) : Real(0.0);

            // The derivative of I with respect to a parameter, from those of
            // B0, A = bT + 2 sqrt(v) and beta:
            //     dB0 + exp(ht) * (dA * B0 + A * dB0) + (dBInfinity - dB0) * w,
            // with w = ht * exp(ht) - expm1(ht).  As beta tends to 1, dBInfinity
            // = -X * dBeta / (beta - 1)^2 diverges and w vanishes, so their
            // product uses ht / (beta - 1) = -a / (beta * X - B0 * (beta - 1)),
            // which stays finite.
            const bool smallHt = std::fabs(ht) < 1.0;
            const Real w = smallHt ? Real(squared(ht) * boundaryWeight(ht)) :
                                     (ht < -700.0 ? Real(1.0) : Real(ht * std::exp(ht) - std::expm1(ht)));
            const Real wOverBeta = smallHt ? Real(squared(a / (beta * X - B0 * betaMinusOne)) * boundaryWeight(ht)) :
                                             Real(w / squared(betaMinusOne));
            const auto dI = [&](Real dB0, Real dA, Real dBeta)
            {
                return dB0 + std::exp(ht) * (dA * B0 + (bT + 2.0 * std::sqrt(variance)) * dB0) - X * dBeta * wOverBeta -
                       dB0 * w;
            };

            const Real betaDq = tq * beta / (variance * betaRoot);
            const Real B0Dq = (dD <= rfD) ? Real(0.0) : Real(X * std::log(rfD) / squared(std::log(dD)) * tq);

            const Real IDq = dI(B0Dq, -tq, betaDq);

            const Real phi_H_S_beta_I_I_rT_bT_v = phi_H(S, beta, I, I, rT, bT, variance);
            const Real phi_I_S_beta_I_I_rT_bT_v = phi_I(S, beta, I, I, rT, bT, variance);
            const Real phi_gamma_S_beta_I_I_rT_bT_v = phi_gamma(S, beta, I, I, rT, bT, variance);
            const Real phi_bt_S_beta_I_I_rT_bT_v = phi_bt(S, beta, I, I, rT, bT, variance);
            const Real phi_H_S_1_I_I_rT_bT_v = phi_H(S, 1.0, I, I, rT, bT, variance);
            const Real phi_I_S_1_I_I_rT_bT_v = phi_I(S, 1.0, I, I, rT, bT, variance);
            const Real phi_bt_S_1_I_I_rT_bT_v = phi_bt(S, 1.0, I, I, rT, bT, variance);
            const Real phi_I_S_1_X_I_rT_bT_v = phi_I(S, 1.0, X, I, rT, bT, variance);
            const Real phi_bt_S_1_X_I_rT_bT_v = phi_bt(S, 1.0, X, I, rT, bT, variance);
            const Real phi_H_S_0_I_I_rT_bT_v = phi_H(S, 0.0, I, I, rT, bT, variance);
            const Real phi_I_S_0_I_I_rT_bT_v = phi_I(S, 0.0, I, I, rT, bT, variance);
            const Real phi_bt_S_0_I_I_rT_bT_v = phi_bt(S, 0.0, I, I, rT, bT, variance);
            const Real phi_I_S_0_X_I_rT_bT_v = phi_I(S, 0.0, X, I, rT, bT, variance);
            const Real phi_bt_S_0_X_I_rT_bT_v = phi_bt(S, 0.0, X, I, rT, bT, variance);

            results.dividendRho =
                (IDq * std::pow(S / I, beta) +
                 (I - X) * std::pow(S / I, beta) * (betaDq * std::log(S / I) - beta * 1 / I * IDq)) *
                    (1 - phi_S_beta_I_I_rT_bT_v) -
                (I - X) * std::pow(S / I, beta) *
                    (phi_H_S_beta_I_I_rT_bT_v * IDq + phi_I_S_beta_I_I_rT_bT_v * IDq +
                     phi_gamma_S_beta_I_I_rT_bT_v * betaDq - phi_bt_S_beta_I_I_rT_bT_v * tq) +
                S * (phi_H_S_1_I_I_rT_bT_v * IDq + phi_I_S_1_I_I_rT_bT_v * IDq - phi_bt_S_1_I_I_rT_bT_v * tq) -
                S * (phi_I_S_1_X_I_rT_bT_v * IDq - phi_bt_S_1_X_I_rT_bT_v * tq) -
                X * (phi_H_S_0_I_I_rT_bT_v * IDq + phi_I_S_0_I_I_rT_bT_v * IDq - phi_bt_S_0_I_I_rT_bT_v * tq) +
                X * (phi_I_S_0_X_I_rT_bT_v * IDq - phi_bt_S_0_X_I_rT_bT_v * tq);

            const DayCounter rdc = process_->riskFreeRate()->dayCounter();
            const Time tr = rdc.yearFraction(refDate, exerciseDate);

            const Real betaDr = -tr * betaRatio / variance;
            const Real B0Dr = (dD <= rfD) ? Real(0) : Real(-X * tr / std::log(dD));

            const Real IDr = dI(B0Dr, tr, betaDr);

            results.rho = (IDr * std::pow(S / I, beta) +
                           (I - X) * std::pow(S / I, beta) * (betaDr * std::log(S / I) - beta / I * IDr)) *
                              (1 - phi_S_beta_I_I_rT_bT_v) -
                          (I - X) * std::pow(S / I, beta) *
                              (phi_H_S_beta_I_I_rT_bT_v * IDr + phi_I_S_beta_I_I_rT_bT_v * IDr +
                               phi_gamma_S_beta_I_I_rT_bT_v * betaDr + phi_rt(S, beta, I, I, rT, bT, variance) * tr +
                               phi_bt_S_beta_I_I_rT_bT_v * tr) +
                          S * (phi_H_S_1_I_I_rT_bT_v * IDr + phi_I_S_1_I_I_rT_bT_v * IDr +
                               phi_rt(S, 1.0, I, I, rT, bT, variance) * tr + phi_bt_S_1_I_I_rT_bT_v * tr) -
                          S * (phi_I_S_1_X_I_rT_bT_v * IDr + phi_rt(S, 1.0, X, I, rT, bT, variance) * tr +
                               phi_bt_S_1_X_I_rT_bT_v * tr) -
                          X * (phi_H_S_0_I_I_rT_bT_v * IDr + phi_I_S_0_I_I_rT_bT_v * IDr +
                               phi_rt(S, 0.0, I, I, rT, bT, variance) * tr + phi_bt_S_0_I_I_rT_bT_v * tr) +
                          X * (phi_I_S_0_X_I_rT_bT_v * IDr + phi_rt(S, 0.0, X, I, rT, bT, variance) * tr +
                               phi_bt_S_0_X_I_rT_bT_v * tr);

            const DayCounter vdc = process_->blackVolatility()->dayCounter();
            const Time tv = vdc.yearFraction(refDate, exerciseDate);
            const Real varianceDv = 2 * std::sqrt(variance * tv);

            const Real betaDv =
                varianceDv / variance * (bT / variance * betaRatio + std::log(dD) / (variance * betaRoot));

            const Real IDv = dI(0.0, varianceDv / std::sqrt(variance), betaDv);

            results.vega =
                (IDv * std::pow(S / I, beta) +
                 (I - X) * std::pow(S / I, beta) * (betaDv * std::log(S / I) - beta / I * IDv)) *
                    (1 - phi_S_beta_I_I_rT_bT_v) -
                (I - X) * std::pow(S / I, beta) *
                    (phi_H_S_beta_I_I_rT_bT_v * IDv + phi_I_S_beta_I_I_rT_bT_v * IDv +
                     phi_gamma_S_beta_I_I_rT_bT_v * betaDv + phi_v(S, beta, I, I, rT, bT, variance) * varianceDv) +
                S * (phi_H_S_1_I_I_rT_bT_v * IDv + phi_I_S_1_I_I_rT_bT_v * IDv +
                     phi_v(S, 1.0, I, I, rT, bT, variance) * varianceDv) -
                S * (phi_I_S_1_X_I_rT_bT_v * IDv + phi_v(S, 1.0, X, I, rT, bT, variance) * varianceDv) -
                X * (phi_H_S_0_I_I_rT_bT_v * IDv + phi_I_S_0_I_I_rT_bT_v * IDv +
                     phi_v(S, 0.0, I, I, rT, bT, variance) * varianceDv) +
                X * (phi_I_S_0_X_I_rT_bT_v * IDv + phi_v(S, 0.0, X, I, rT, bT, variance) * varianceDv);

            results.gamma =
                (I - X) * std::pow(S / I, beta - 2) * beta * (beta - 1) / squared(I) * (1 - phi_S_beta_I_I_rT_bT_v) -
                2 * (I - X) * std::pow(S / I, beta - 1) * beta / I * phi_S_S_beta_I_I_rT_bT_v -
                (I - X) * std::pow(S / I, beta) * phi_SS(S, beta, I, I, rT, bT, variance)

                + 2 * phi_S_S_1_I_I_rT_bT_v + S * phi_SS(S, 1.0, I, I, rT, bT, variance)

                - 2 * phi_S_S_1_X_I_rT_bT_v - S * phi_SS(S, 1.0, X, I, rT, bT, variance)

                - X * phi_SS(S, 0.0, I, I, rT, bT, variance) + X * phi_SS(S, 0.0, X, I, rT, bT, variance);

            const Volatility vol = std::sqrt(variance / tv);

            const Date tomorrow = refDate + Period(1, Days);
            const Time dtq = qdc.yearFraction(refDate, exerciseDate) - qdc.yearFraction(tomorrow, exerciseDate);
            const Time dtr = rdc.yearFraction(refDate, exerciseDate) - rdc.yearFraction(tomorrow, exerciseDate);
            const Time dtv = vdc.yearFraction(refDate, exerciseDate) - vdc.yearFraction(tomorrow, exerciseDate);

            results.thetaPerDay = -(0.5 * results.vega * vol / tv * dtv + results.rho * rT / (tr * tr) * dtr +
                                    results.dividendRho * (rT - bT) / (tq * tq) * dtq);
            results.theta = 365 * results.thetaPerDay;

            results.strikeSensitivity = results.value / X - S / X * results.delta;
            results.additionalResults["strikeGamma"] = Real(results.gamma * squared(S / X));

            results.additionalResults["exerciseType"] = std::string("American");
        }

        // check if European engine gives higher NPV
        if (results.value < europeanResults.value)
        {
            results = europeanResults;
        }

        return results;
    }


    void BjerksundStenslandApproximationEngine::calculate() const
    {

        QL_REQUIRE(arguments_.exercise->type() == Exercise::American, "not an American Option");

        ext::shared_ptr<AmericanExercise> ex = ext::dynamic_pointer_cast<AmericanExercise>(arguments_.exercise);
        QL_REQUIRE(ex, "non-American exercise given");
        QL_REQUIRE(!ex->payoffAtExpiry(), "payoff at expiry not handled");

        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");

        Real variance = process_->blackVolatility()->blackVariance(ex->lastDate(), payoff->strike());
        DiscountFactor dividendDiscount = process_->dividendYield()->discount(ex->lastDate());
        DiscountFactor riskFreeDiscount = process_->riskFreeRate()->discount(ex->lastDate());
        Real spot = process_->stateVariable()->value();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");
        Real strike = payoff->strike();

        if (payoff->optionType() == Option::Put)
        {
            // use put-call symmetry
            std::swap(spot, strike);
            std::swap(riskFreeDiscount, dividendDiscount);
            payoff = ext::make_shared<PlainVanillaPayoff>(Option::Call, strike);
        }

        if (variance < QL_EPSILON)
        {
            results_ = zeroVarianceCallResults(spot, strike, riskFreeDiscount, dividendDiscount, variance);
        }
        else if (dividendDiscount > 1.0 && riskFreeDiscount > dividendDiscount)
        {
            QL_FAIL("double-boundary case r<q<0 for a call given");
        }
        else if (dividendDiscount >= 1.0 && dividendDiscount >= riskFreeDiscount)
        {
            results_ = europeanCallResults(spot, strike, riskFreeDiscount, dividendDiscount, variance);
        }
        else
        {
            // early exercise can be optimal - use approximation
            results_ = americanCallApproximation(spot, strike, riskFreeDiscount, dividendDiscount, variance);
        }

        // check if immediate exercise gives higher NPV
        if (results_.value < (spot - strike) * (1 + 10 * QL_EPSILON))
        {
            results_ = immediateExercise(spot, strike);
        }

        if (ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff)->optionType() == Option::Put)
        {

            std::swap(results_.delta, results_.strikeSensitivity);

            Real tmp = results_.gamma;
            results_.gamma = ext::any_cast<Real>(results_.additionalResults["strikeGamma"]);
            results_.additionalResults["strikeGamma"] = tmp;

            std::swap(results_.rho, results_.dividendRho);

            Time tr = process_->riskFreeRate()->dayCounter().yearFraction(process_->riskFreeRate()->referenceDate(),
                                                                          arguments_.exercise->lastDate());
            Time tq = process_->dividendYield()->dayCounter().yearFraction(process_->dividendYield()->referenceDate(),
                                                                           arguments_.exercise->lastDate());

            results_.rho *= tr / tq;
            results_.dividendRho *= tq / tr;
        }
    }
}
