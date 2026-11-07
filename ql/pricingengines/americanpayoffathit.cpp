/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2004 Ferdinando Ametrano

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

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/integrals/tanhsinhintegral.hpp>
#include <ql/mathconstants.hpp>
#include <ql/pricingengines/americanpayoffathit.hpp>

namespace QuantLib
{

    namespace detail
    {

        UnitPayoffAtHitResults unitPayoffAtHit(Real b, Real mu, Real rho, Real variance, bool derivatives)
        {
            const Real lambdaSquared = mu * mu + 2.0 * rho;
            if (b == 0.0)
            {
                // The barrier is hit now.  These derivatives are the ones the closed form gives at
                // b = 0, the means of their limits from either side, so that they don't jump where
                // lambda^2 changes sign.
                return {1.0, mu, mu * mu + lambdaSquared, 0.0, 0.0};
            }

            // The variance s at which the barrier is first reached has the density
            //     |b| / sqrt(2 pi s^3) exp(-(b - mu s)^2 / (2 s)),
            // and the value is its integral, discounted by exp(-rho s), over (0, variance].  With
            // s = b^2 / w^2, w0 = |b| / sqrt(variance) and c = -lambda^2 variance / 2, that's P J, with
            //     P = sqrt(2 / pi) exp(-(b - mu variance)^2 / (2 variance) - rho variance),
            //     J = integral from w0 to infinity of exp(-(w^2 - w0^2) / 2 - c (1 - w0^2 / w^2)) dw,
            // whose integrand is at most 1 where lambda^2 <= 0.  Differentiating under the integral in
            // b, mu and rho takes one more integral, G, of the same integrand over w^2.  Without its
            // drift, exp(-b mu) P J is an expected discount at the time a Brownian motion reaches b,
            // so half its second derivative in b is its derivative in the variance plus lambda^2 / 2
            // times itself; and that derivative in the variance is the discounted density above at
            // s = variance, times exp(-b mu).  That gives the second derivative in b.
            const Real stdDev = std::sqrt(variance);
            const Real driftedDistance = b - mu * variance;
            const Real P =
                M_SQRT2 * M_1_SQRTPI * std::exp(-0.5 * driftedDistance * driftedDistance / variance - rho * variance);
            if (P == 0.0)
                return {0.0, 0.0, 0.0, 0.0, 0.0};

            // In t = log(w / w0), both integrands change on the scale of their range: for w0 near 0,
            // with the spot almost on the barrier, and for large w0, where the range shrinks as
            // 1 / w0^2.  Past tMax, exp(-(w^2 - w0^2) / 2) is below exp(-50).  With u = w^2 / w0^2 - 1,
            // the integrand above is exp(-w0^2 u / 2 - c u / (1 + u)), and dw = w0 sqrt(1 + u) dt.
            const Real w0 = std::fabs(b) / stdDev;
            const Real c = -0.5 * lambdaSquared * variance;
            const auto shape = [w0, c](Real u) { return std::exp(-0.5 * w0 * w0 * u - c * u / (1.0 + u)); };
            const auto forJ = [&shape](Real t)
            {
                const Real u = std::expm1(2.0 * t);
                return shape(u) * std::sqrt(1.0 + u);
            };
            const auto forG = [&shape](Real t)
            {
                const Real u = std::expm1(2.0 * t);
                return shape(u) / std::sqrt(1.0 + u);
            };
            const Real tMax = 0.5 * std::log1p(100.0 / (w0 * w0));
            const TanhSinhIntegral integral(1.0e-12);
            const Real J = w0 * integral(forJ, 0.0, tMax);
            UnitPayoffAtHitResults results = {P * J, Null<Real>(), Null<Real>(), Null<Real>(), Null<Real>()};
            if (!derivatives)
                return results;

            const Real G = integral(forG, 0.0, tMax) / w0;
            const Real sign = b > 0.0 ? 1.0 : -1.0;
            results.dValueDb = P * (mu * J - sign / stdDev - lambdaSquared * b * G);
            results.d2ValueDb2 =
                P * ((mu * mu + lambdaSquared) * J - 2.0 * mu * (sign / stdDev + lambdaSquared * b * G) +
                     std::fabs(b) / (variance * stdDev));
            results.dValueDMu = P * (b * J - mu * b * b * G);
            results.dValueDRho = -P * b * b * G;
            return results;
        }

    }

    AmericanPayoffAtHit::AmericanPayoffAtHit(Real spot,
                                             DiscountFactor discount,
                                             DiscountFactor dividendDiscount,
                                             Real variance,
                                             const ext::shared_ptr<StrikedTypePayoff>& payoff)
    : spot_(spot), discount_(discount), dividendDiscount_(dividendDiscount), variance_(variance)
    {

        QL_REQUIRE(spot_ > 0.0, "positive spot value required");

        QL_REQUIRE(discount_ > 0.0, "positive discount required");

        QL_REQUIRE(dividendDiscount_ > 0.0, "positive dividend discount required");

        QL_REQUIRE(variance_ >= 0.0, "negative variance not allowed");

        stdDev_ = std::sqrt(variance_);

        Option::Type type = payoff->optionType();
        strike_ = payoff->strike();


        log_H_S_ = std::log(strike_ / spot_);

        Real n_d1, n_d2, cum_minus_d1, cum_minus_d2;
        Real lambdaSquared = 0.0;
        mu_ = 0.0;
        if (variance_ >= QL_EPSILON)
        {
            if (discount_ == 0.0 && dividendDiscount_ == 0.0)
            {
                mu_ = -0.5;
                lambdaSquared = 0.25;
            }
            else if (discount_ == 0.0)
            {
                QL_FAIL("null discount not handled yet");
            }
            else
            {
                mu_ = std::log(dividendDiscount_ / discount_) / variance_ - 0.5;
                lambdaSquared = mu_ * mu_ - 2.0 * std::log(discount_) / variance_;
            }
            // A negative rate can make lambda^2 negative, and the closed form below would then take
            // N at complex arguments; at lambda = 0, rho() would divide by it.  The value and its
            // derivatives come from detail::unitPayoffAtHit there.
            integrated_ = lambdaSquared <= 0.0;
        }
        if (variance_ >= QL_EPSILON && !integrated_)
        {
            lambda_ = std::sqrt(lambdaSquared);
            D1_ = log_H_S_ / stdDev_ + lambda_ * stdDev_;
            D2_ = D1_ - 2.0 * lambda_ * stdDev_;
            CumulativeNormalDistribution f;
            cum_d1_ = f(D1_);
            cum_d2_ = f(D2_);
            // N(-d) directly, since 1 - N(d) loses the tail to cancellation.
            cum_minus_d1 = f(-D1_);
            cum_minus_d2 = f(-D2_);
            n_d1 = f.derivative(D1_);
            n_d2 = f.derivative(D2_);
        }
        else
        {
            // The value below doesn't need any of these.
            lambda_ = D1_ = D2_ = 0.0;
            cum_d1_ = cum_d2_ = cum_minus_d1 = cum_minus_d2 = 0.0;
            n_d1 = n_d2 = 0.0;
        }


        switch (type)
        {
            // up-and-in cash-(at-hit)-or-nothing option
            // a.k.a. american call with cash-or-nothing payoff
            case Option::Call:
                if (strike_ > spot_)
                {
                    alpha_ = cum_minus_d1; //  N(-d1)
                    DalphaDd1_ = -n_d1;    // -n( d1)
                    beta_ = cum_minus_d2;  //  N(-d2)
                    DbetaDd2_ = -n_d2;     // -n( d2)
                }
                else
                {
                    alpha_ = 0.5;
                    DalphaDd1_ = 0.0;
                    beta_ = 0.5;
                    DbetaDd2_ = 0.0;
                }
                break;
            // down-and-in cash-(at-hit)-or-nothing option
            // a.k.a. american put with cash-or-nothing payoff
            case Option::Put:
                if (strike_ < spot_)
                {
                    alpha_ = cum_d1_;  //  N(d1)
                    DalphaDd1_ = n_d1; //  n(d1)
                    beta_ = cum_d2_;   //  N(d2)
                    DbetaDd2_ = n_d2;  //  n(d2)
                }
                else
                {
                    alpha_ = 0.5;
                    DalphaDd1_ = 0.0;
                    beta_ = 0.5;
                    DbetaDd2_ = 0.0;
                }
                break;
            default:
                QL_FAIL("invalid option type");
        }


        muPlusLambda_ = mu_ + lambda_;
        muMinusLambda_ = mu_ - lambda_;
        if (variance_ >= QL_EPSILON && !integrated_)
        {
            // These multiply to 2 log(discount) / variance.  Getting the smaller one from
            // that keeps it from cancelling at low variance.
            const Real product = 2.0 * std::log(discount_) / variance_;
            if (mu_ < 0.0)
                muPlusLambda_ = product / muMinusLambda_;
            else if (muPlusLambda_ != 0.0)
                muMinusLambda_ = product / muPlusLambda_;
        }
        inTheMoney_ = (type == Option::Call && strike_ < spot_) || (type == Option::Put && strike_ > spot_);

        if (inTheMoney_)
        {
            forward_ = 1.0;
            X_ = 1.0;
            DXDstrike_ = 0.0;
        }
        else if (integrated_)
        {
            // The discount rate per unit of variance is -log(discount) / variance.
            atHit_ = detail::unitPayoffAtHit(log_H_S_, mu_, -std::log(discount_) / variance_, variance_);
            forward_ = 1.0;
            alpha_ = atHit_.value;
            X_ = 0.0;
            beta_ = 0.0;
        }
        else if (variance_ >= QL_EPSILON)
        {
            forward_ = std::pow(strike_ / spot_, muPlusLambda_);
            X_ = std::pow(strike_ / spot_, muMinusLambda_);
            //            DXDstrike_ = ......;

            // At small variance a power can overflow while the probability it multiplies
            // underflows, and their product is still finite.
            CumulativeNormalDistribution f;
            const Real sign = type == Option::Call ? -1.0 : 1.0;
            if (!std::isfinite(forward_) || alpha_ == 0.0)
            {
                const Real logPower = muPlusLambda_ * log_H_S_;
                alpha_ = std::exp(logPower + f.logValue(sign * D1_));
                DalphaDd1_ = sign * std::exp(logPower - 0.5 * D1_ * D1_ - 0.5 * std::log(M_TWOPI));
                forward_ = 1.0;
            }
            if (!std::isfinite(X_) || beta_ == 0.0)
            {
                const Real logPower = muMinusLambda_ * log_H_S_;
                beta_ = std::exp(logPower + f.logValue(sign * D2_));
                DbetaDd2_ = sign * std::exp(logPower - 0.5 * D2_ * D2_ - 0.5 * std::log(M_TWOPI));
                X_ = 1.0;
            }
        }
        else
        {
            // The underlying follows its forward, and the payoff comes when the forward
            // reaches the strike, if it does before maturity.
            const Real timeToStrike = log_H_S_ == 0.0 ? 0.0 : log_H_S_ / std::log(dividendDiscount_ / discount_);
            alpha_ = 1.0;
            beta_ = 0.0;
            X_ = 0.0;
            forward_ =
                (timeToStrike >= 0.0 && timeToStrike <= 1.0) ? std::exp(timeToStrike * std::log(discount_)) : 0.0;
        }


        // Binary Cash-Or-Nothing payoff?
        ext::shared_ptr<CashOrNothingPayoff> coo = ext::dynamic_pointer_cast<CashOrNothingPayoff>(payoff);
        if (coo != nullptr)
        {
            K_ = coo->cashPayoff();
            DKDstrike_ = 0.0;
        }

        // Binary Asset-Or-Nothing payoff?
        ext::shared_ptr<AssetOrNothingPayoff> aoo = ext::dynamic_pointer_cast<AssetOrNothingPayoff>(payoff);
        if (aoo != nullptr)
        {
            if (inTheMoney_)
            {
                K_ = spot_;
                DKDstrike_ = 0.0;
            }
            else
            {
                K_ = aoo->strike();
                DKDstrike_ = 1.0;
            }
        }
    }


    Real AmericanPayoffAtHit::delta() const
    {
        // b = log(strike / spot) moves by -1 / spot per unit of spot.  In the money the value
        // doesn't move, and atHit_ holds zeros.
        if (integrated_)
            return -K_ * atHit_.dValueDb / spot_;

        Real tempDelta = -spot_ * stdDev_;
        Real DalphaDs = DalphaDd1_ / tempDelta;
        Real DbetaDs = DbetaDd2_ / tempDelta;

        Real DforwardDs, DXDs;
        if (inTheMoney_)
        {
            DforwardDs = 0.0;
            DXDs = 0.0;
        }
        else
        {
            DforwardDs = -muPlusLambda_ * forward_ / spot_;
            DXDs = -muMinusLambda_ * X_ / spot_;
        }

        return K_ * (DalphaDs * forward_ + alpha_ * DforwardDs + DbetaDs * X_ + beta_ * DXDs);
    }


    Real AmericanPayoffAtHit::gamma() const
    {
        if (integrated_)
            return K_ * (atHit_.dValueDb + atHit_.d2ValueDb2) / (spot_ * spot_);

        Real tempDelta = -spot_ * stdDev_;
        Real DalphaDs = DalphaDd1_ / tempDelta;
        Real DbetaDs = DbetaDd2_ / tempDelta;
        Real D2alphaDs2 = -DalphaDs / spot_ * (1 - D1_ / stdDev_);
        Real D2betaDs2 = -DbetaDs / spot_ * (1 - D2_ / stdDev_);

        Real DforwardDs, DXDs, D2forwardDs2, D2XDs2;
        if (inTheMoney_)
        {
            DforwardDs = 0.0;
            DXDs = 0.0;
            D2forwardDs2 = 0.0;
            D2XDs2 = 0.0;
        }
        else
        {
            DforwardDs = -muPlusLambda_ * forward_ / spot_;
            DXDs = -muMinusLambda_ * X_ / spot_;
            D2forwardDs2 = muPlusLambda_ * forward_ / (spot_ * spot_) * (1 + muPlusLambda_);
            D2XDs2 = muMinusLambda_ * X_ / (spot_ * spot_) * (1 + muMinusLambda_);
        }

        return K_ * (D2alphaDs2 * forward_ + DalphaDs * DforwardDs + DalphaDs * DforwardDs + alpha_ * D2forwardDs2 +
                     D2betaDs2 * X_ + DbetaDs * DXDs + DbetaDs * DXDs + beta_ * D2XDs2);
    }


    Real AmericanPayoffAtHit::rho(Time maturity) const
    {
        QL_REQUIRE(maturity >= 0.0, "negative maturity not allowed");

        // mu and the discount rate per unit of variance both move by maturity / variance per unit
        // of rate.
        if (integrated_)
            return maturity * K_ * (atHit_.dValueDMu + atHit_.dValueDRho) / variance_;

        // actually D.Dr / T
        Real DalphaDr = -DalphaDd1_ / (lambda_ * stdDev_) * (1.0 + mu_);
        Real DbetaDr = DbetaDd2_ / (lambda_ * stdDev_) * (1.0 + mu_);
        Real DforwardDr, DXDr;
        if (inTheMoney_)
        {
            DforwardDr = 0.0;
            DXDr = 0.0;
        }
        else
        {
            DforwardDr = forward_ * (1.0 + (1.0 + mu_) / lambda_) * log_H_S_ / variance_;
            DXDr = X_ * (1.0 - (1.0 + mu_) / lambda_) * log_H_S_ / variance_;
        }

        return maturity * K_ * (DalphaDr * forward_ + alpha_ * DforwardDr + DbetaDr * X_ + beta_ * DXDr);
    }

}
