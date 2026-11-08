/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*

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

#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/pricingengines/bacheliercalculator.hpp>

namespace QuantLib
{

    class BachelierCalculator::Calculator : public AcyclicVisitor,
                                            public Visitor<Payoff>,
                                            public Visitor<PlainVanillaPayoff>,
                                            public Visitor<CashOrNothingPayoff>,
                                            public Visitor<AssetOrNothingPayoff>,
                                            public Visitor<GapPayoff>
    {
      private:
        BachelierCalculator& bachelier_;

      public:
        explicit Calculator(BachelierCalculator& bachelier) : bachelier_(bachelier) {}
        void visit(Payoff&) override;
        void visit(PlainVanillaPayoff&) override;
        void visit(CashOrNothingPayoff&) override;
        void visit(AssetOrNothingPayoff&) override;
        void visit(GapPayoff&) override;
    };


    BachelierCalculator::BachelierCalculator(const ext::shared_ptr<StrikedTypePayoff>& p,
                                             Real forward,
                                             Real stdDev,
                                             Real discount)
    : strike_(p->strike()), forward_(forward), stdDev_(stdDev), discount_(discount), variance_(stdDev * stdDev)
    {
        initialize(p);
    }

    BachelierCalculator::BachelierCalculator(
        Option::Type optionType, Real strike, Real forward, Real stdDev, Real discount)
    : strike_(strike), forward_(forward), stdDev_(stdDev), discount_(discount), variance_(stdDev * stdDev)
    {
        initialize(ext::shared_ptr<StrikedTypePayoff>(new PlainVanillaPayoff(optionType, strike)));
    }

    void BachelierCalculator::initialize(const ext::shared_ptr<StrikedTypePayoff>& p)
    {
        QL_REQUIRE(stdDev_ >= 0.0, "stdDev (" << stdDev_ << ") must be non-negative");
        QL_REQUIRE(discount_ > 0.0, "discount (" << discount_ << ") must be positive");

        // alpha_ can't tell them apart: an out-of-the-money put's is 0 at zero volatility.
        type_ = p->optionType();

        // For Bachelier model, we use d = (F - K) / σ instead of the Black-Scholes d1, d2
        if (stdDev_ >= QL_EPSILON)
        {
            // Bachelier d parameter: d = (F - K) / σ
            d_ = (forward_ - strike_) / stdDev_;

            CumulativeNormalDistribution f;
            cum_d_ = f(d_);
            n_d_ = f.derivative(d_);
        }
        else
        {
            // When volatility is zero
            if (close(forward_, strike_))
            {
                d_ = 0;
                cum_d_ = 0.5;
                n_d_ = M_SQRT_2 * M_1_SQRTPI;
            }
            else if (forward_ > strike_)
            {
                d_ = QL_MAX_REAL;
                cum_d_ = 1.0;
                n_d_ = 0.0;
            }
            else
            {
                d_ = QL_MIN_REAL;
                cum_d_ = 0.0;
                n_d_ = 0.0;
            }
        }

        x_ = strike_;
        DxDstrike_ = 1.0;
        DxDs_ = 0.0;

        // alpha_ and beta_ are the coefficients of the forward and of x_ in the value less its time value, as
        // BlackCalculator's are: for a call, (F - K) N(d) = F alpha + K beta.  The value and the Greeks don't
        // read them.
        switch (p->optionType())
        {
            case Option::Call:
                alpha_ = cum_d_;  // N(d)
                DalphaDd_ = n_d_; // n(d)
                beta_ = -cum_d_;  // -N(d) - base part
                DbetaDd_ = -n_d_; // -n(d)
                break;
            case Option::Put:
                alpha_ = cum_d_ - 1.0; // N(d) - 1 = -N(-d)
                DalphaDd_ = n_d_;      // n(d)
                beta_ = 1.0 - cum_d_;  // 1 - N(d) = N(-d)
                DbetaDd_ = -n_d_;      // -n(d)
                break;
            default:
                QL_FAIL("invalid option type");
        }

        // In the money, where w F_T > w K with w = 1 for a call and -1 for a put, the payoff is
        // slope_ F_T + intercept_, so that with F_T normal with mean F and standard deviation s,
        //   V = D [(slope F + intercept) N(w d) + slope w s n(d)].
        // The Greeks follow from dn(d)/dd = -d n(d) and dd/dF = -dd/dK = 1/s.  A vanilla option pays
        // w F_T - w K; the visitors below set the other payoffs.
        slope_ = (type_ == Option::Call) ? 1.0 : -1.0;
        intercept_ = -slope_ * strike_;
        DinterceptDstrike_ = -slope_;
        vanilla_ = true;

        // now dispatch on type.
        Calculator calc(*this);
        p->accept(calc);

        // Zero for a vanilla option, so that its Greeks have no terms in it.
        jump_ = slope_ * strike_ + intercept_;
    }

    void BachelierCalculator::Calculator::visit(Payoff& p)
    {
        QL_FAIL("unsupported payoff type: " << p.name());
    }

    void BachelierCalculator::Calculator::visit(PlainVanillaPayoff&) {}

    void BachelierCalculator::Calculator::visit(CashOrNothingPayoff& payoff)
    {
        bachelier_.alpha_ = bachelier_.DalphaDd_ = 0.0;
        bachelier_.x_ = payoff.cashPayoff();
        bachelier_.DxDstrike_ = 0.0;
        switch (payoff.optionType())
        {
            case Option::Call:
                bachelier_.beta_ = bachelier_.cum_d_;
                bachelier_.DbetaDd_ = bachelier_.n_d_;
                break;
            case Option::Put:
                bachelier_.beta_ = 1.0 - bachelier_.cum_d_;
                bachelier_.DbetaDd_ = -bachelier_.n_d_;
                break;
            default:
                QL_FAIL("invalid option type");
        }
        bachelier_.slope_ = bachelier_.DinterceptDstrike_ = 0.0;
        bachelier_.intercept_ = payoff.cashPayoff();
        bachelier_.vanilla_ = false;
    }

    void BachelierCalculator::Calculator::visit(AssetOrNothingPayoff& payoff)
    {
        bachelier_.beta_ = bachelier_.DbetaDd_ = 0.0;
        switch (payoff.optionType())
        {
            case Option::Call:
                bachelier_.alpha_ = bachelier_.cum_d_;
                bachelier_.DalphaDd_ = bachelier_.n_d_;
                break;
            case Option::Put:
                bachelier_.alpha_ = 1.0 - bachelier_.cum_d_;
                bachelier_.DalphaDd_ = -bachelier_.n_d_;
                break;
            default:
                QL_FAIL("invalid option type");
        }
        bachelier_.slope_ = 1.0;
        bachelier_.intercept_ = bachelier_.DinterceptDstrike_ = 0.0;
        bachelier_.vanilla_ = false;
    }

    void BachelierCalculator::Calculator::visit(GapPayoff& payoff)
    {
        bachelier_.x_ = payoff.secondStrike();
        bachelier_.DxDstrike_ = 0.0;
        // A vanilla option struck at the second strike, paid where the first is in the money.
        bachelier_.intercept_ = -bachelier_.slope_ * payoff.secondStrike();
        bachelier_.DinterceptDstrike_ = 0.0;
        bachelier_.vanilla_ = false;
    }

    Real BachelierCalculator::value() const
    {
        // V = D [(slope F + intercept) N(w d) + slope w s n(d)], as in initialize().
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        const Real timeValue = (stdDev_ > QL_EPSILON) ? stdDev_ * n_d_ : 0.0;
        const Real result = (slope_ * forward_ + intercept_) * itmCashProbability() + slope_ * w * timeValue;

        // A vanilla option can't be worth less than zero, though its terms cancel far out of the money.  A gap
        // option can, and so can an asset-or-nothing option, as the forward can be negative.
        return discount_ * (vanilla_ ? std::max(result, 0.0) : result);
    }

    Real BachelierCalculator::delta(Real spot) const
    {

        // For Bachelier model:
        // Delta = dV/dS = (dV/dF) * (dF/dS)
        // where dF/dS = F/S (assuming forward = spot * exp(r*T))

        Real DforwardDs = forward_ / spot;
        Real deltaFwd = deltaForward();

        return deltaFwd * DforwardDs;
    }

    Real BachelierCalculator::deltaForward() const
    {
        if (stdDev_ <= QL_EPSILON)
        {
            // Away from the strike N(w d) doesn't move with the forward.
            return discount_ * slope_ * itmCashProbability();
        }

        // dV/dF = D [slope N(w d) + w jump n(d) / s]
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        return discount_ * (slope_ * itmCashProbability() + w * jump_ * n_d_ / stdDev_);
    }

    Real BachelierCalculator::elasticity(Real spot) const
    {
        Real val = value();
        Real del = delta(spot);
        if (val > QL_EPSILON)
            return del / val * spot;
        else if (std::fabs(del) < QL_EPSILON)
            return 0.0;
        else if (del > 0.0)
            return QL_MAX_REAL;
        else
            return QL_MIN_REAL;
    }

    Real BachelierCalculator::elasticityForward() const
    {
        Real val = value();
        Real del = deltaForward();
        if (val > QL_EPSILON)
            return del / val * forward_;
        else if (std::fabs(del) < QL_EPSILON)
            return 0.0;
        else if (del > 0.0)
            return QL_MAX_REAL;
        else
            return QL_MIN_REAL;
    }

    Real BachelierCalculator::gamma(Real spot) const
    {

        // For Bachelier model:
        // Gamma = d²V/dS² = d²V/dF² * (dF/dS)², as dF/dS = F/S doesn't move with the spot

        Real DforwardDs = forward_ / spot;

        return gammaForward() * DforwardDs * DforwardDs;
    }

    Real BachelierCalculator::gammaForward() const
    {
        if (stdDev_ <= QL_EPSILON)
        {
            return 0.0;
        }

        // d2V/dF2 = D w n(d) / s [slope - jump d / s]
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        return discount_ * w * n_d_ / stdDev_ * (slope_ - jump_ * d_ / stdDev_);
    }

    Real BachelierCalculator::theta(Real spot, Time maturity) const
    {

        QL_REQUIRE(maturity >= 0.0, "maturity (" << maturity << ") must be non-negative");
        if (close(maturity, 0.0))
            return 0.0;

        // Theta = dV/dt at a fixed spot = r V - (r - q) S Delta - 1/2 sigma^2 Gamma_F, with r T = -ln D,
        // (r - q) T = ln(F/S) and sigma^2 T = s^2.  The forward, not the spot, has the normal volatility sigma,
        // so the last term takes the forward's gamma; Black-Scholes' S^2 Gamma would add (F/S)^2.

        return -(std::log(discount_) * value() + std::log(forward_ / spot) * spot * delta(spot) +
                 0.5 * variance_ * gammaForward()) /
               maturity;
    }

    Real BachelierCalculator::vega(Time maturity) const
    {
        QL_REQUIRE(maturity >= 0.0, "negative maturity not allowed");

        if (maturity <= QL_EPSILON || stdDev_ <= QL_EPSILON)
        {
            return 0.0;
        }

        // dV/ds = s d2V/dF2 = D w n(d) [slope - jump d / s], and s = sigma sqrt(T)
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        return discount_ * std::sqrt(maturity) * w * n_d_ * (slope_ - jump_ * d_ / stdDev_);
    }

    Real BachelierCalculator::rho(Time maturity) const
    {
        QL_REQUIRE(maturity >= 0.0, "negative maturity not allowed");

        // For Bachelier model:
        // Rho = dV/dr = T * (delta_forward * forward - value), as the forward is S exp((r - q) T)
        // and the discount factor exp(-r T)

        Real deltaFwd = deltaForward();
        Real rho_value = maturity * (deltaFwd * forward_ - value());

        return rho_value;
    }

    Real BachelierCalculator::dividendRho(Time maturity) const
    {
        QL_REQUIRE(maturity >= 0.0, "negative maturity not allowed");

        // For Bachelier model:
        // Dividend rho = dV/dq = -T * forward * delta_forward, as the forward is S exp((r - q) T)

        return -maturity * deltaForward() * forward_;
    }

    Real BachelierCalculator::strikeSensitivity() const
    {
        if (stdDev_ <= QL_EPSILON)
        {
            // Away from the strike N(w d) doesn't move with it.
            return discount_ * DinterceptDstrike_ * itmCashProbability();
        }

        // dV/dK = D [dintercept/dK N(w d) - w jump n(d) / s]
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        return discount_ * (DinterceptDstrike_ * itmCashProbability() - w * jump_ * n_d_ / stdDev_);
    }


    Real BachelierCalculator::strikeGamma() const
    {
        if (stdDev_ <= QL_EPSILON)
        {
            return 0.0;
        }

        // d2V/dK2 = D w n(d) / s [-2 dintercept/dK - jump d / s - slope]
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        return discount_ * w * n_d_ / stdDev_ * (-2.0 * DinterceptDstrike_ - jump_ * d_ / stdDev_ - slope_);
    }

    // Sensitivity of Vega to forward (Vanna)
    Real BachelierCalculator::vanna(Time maturity) const
    {
        if (maturity <= QL_EPSILON || stdDev_ <= QL_EPSILON)
        {
            return 0.0;
        }

        // d vega / dF = sqrt(T) D w n(d) / s [jump (d^2 - 1) / s - slope d]
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        return discount_ * std::sqrt(maturity) * w * n_d_ / stdDev_ * (jump_ * (d_ * d_ - 1.0) / stdDev_ - slope_ * d_);
    }

    // Sensitivity of Vega to volatility (Volga)
    Real BachelierCalculator::volga(Time maturity) const
    {
        if (maturity <= QL_EPSILON || stdDev_ <= QL_EPSILON)
        {
            return 0.0;
        }

        // d vega / d sigma = T D w n(d) / s [slope d^2 + jump d (2 - d^2) / s], as s = sigma sqrt(T)
        const Real w = (type_ == Option::Call) ? 1.0 : -1.0;
        return discount_ * maturity * w * n_d_ / stdDev_ * (slope_ * d_ * d_ + jump_ * d_ * (2.0 - d_ * d_) / stdDev_);
    }
}
