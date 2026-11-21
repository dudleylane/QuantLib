/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010, 2017 Klaus Spanderen

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

/*! \file analyticptdhestonengine.cpp
    \brief analytic piecewise time dependent Heston-model engine
*/

#include <ql/instruments/payoffs.hpp>
#include <ql/math/expm1.hpp>
#include <ql/math/functional.hpp>
#include <ql/pricingengines/blackcalculator.hpp>
#include <ql/pricingengines/vanilla/analyticptdhestonengine.hpp>

namespace QuantLib
{

    // helper class for integration
    class AnalyticPTDHestonEngine::Fj_Helper
    {
      public:
        Fj_Helper(const Handle<PiecewiseTimeDependentHestonModel>& model, Time term, Real strike, Size j);

        Real operator()(Real phi) const;

      private:
        const Size j_;
        const Time term_;
        const Real v0_, x_, sx_;

        std::vector<Rate> r_, q_;
        const ext::shared_ptr<YieldTermStructure> qTS_;
        const Handle<PiecewiseTimeDependentHestonModel> model_;

        const TimeGrid timeGrid_;
    };

    AnalyticPTDHestonEngine::Fj_Helper::Fj_Helper(const Handle<PiecewiseTimeDependentHestonModel>& model,
                                                  Time term,
                                                  Real strike,
                                                  Size j)
    : j_(j), term_(term),

      v0_(model->v0()), x_(std::log(model->s0())), sx_(std::log(strike)), r_(model->timeGrid().size() - 1),
      q_(model->timeGrid().size() - 1), model_(model), timeGrid_(model->timeGrid())
    {

        for (Size i = 0; i < timeGrid_.size() - 1; ++i)
        {
            const Time begin = std::min(term_, timeGrid_[i]);
            const Time end = std::min(term_, timeGrid_[i + 1]);
            r_[i] = model->riskFreeRate()->forwardRate(begin, end, Continuous, NoFrequency).rate();
            q_[i] = model->dividendYield()->forwardRate(begin, end, Continuous, NoFrequency).rate();
        }
    }

    Real AnalyticPTDHestonEngine::Fj_Helper::operator()(Real phi) const
    {

        // avoid numeric overflow for phi->0.
        // todo: use l'Hospital's rule use to get lim_{phi->0}
        phi = std::max(Real(std::numeric_limits<float>::epsilon()), phi);

        std::complex<Real> D = 0.0;
        std::complex<Real> C = 0.0;

        for (Size i = timeGrid_.size() - 1; i > 0; --i)
        {
            const Time begin = timeGrid_[i - 1];
            if (begin < term_)
            {
                const Time end = std::min(term_, timeGrid_[i]);
                const Time tau = end - begin;
                const Time t = 0.5 * (end + begin);

                const Real rho = model_->rho(t);
                const Real sigma = model_->sigma(t);
                const Real kappa = model_->kappa(t);
                const Real theta = model_->theta(t);

                const Real sigma2 = sigma * sigma;
                const Real t0 = kappa - ((j_ == 1) ? Real(rho * sigma) : 0);
                const Real rpsig = rho * sigma * phi;

                const std::complex<Real> t1 = t0 + std::complex<Real>(0, -rpsig);
                const std::complex<Real> d =
                    std::sqrt(t1 * t1 - sigma2 * phi * std::complex<Real>(-phi, (j_ == 1) ? 1 : -1));
                /* Over the interval D solves D' = nu / 2 - t1 D + sigma^2 D^2 / 2 from the later interval's D. A root
                   of the right-hand side, D_- = (t1 - d) / sigma^2, is also nu / (t1 + d), which doesn't cancel where
                   Re(t1 conj(d)) > 0, as in AnalyticHestonEngine's lnChF. With y = (1 - e^{-d tau}) / (2 d) and
                   x = -sigma^2 (D - D_-) y, D becomes (D (1 - (t1 + d) y) + nu y) / (1 + x), and C gains
                   kappa theta (D_- tau + 2 (D - D_-) y log1p(x) / x). Only D_-'s first form divides by sigma^2, and
                   only where t1 - d doesn't cancel; where sigma^2 is 0, below about 2.2e-162, that leaves NaN wherever
                   Re t1 isn't positive, which needs kappa below |rho| sigma too. Gatheral's form divided t1 - d by
                   sigma^2: at sigma 1e-7 calls were up to 0.12 off, and where sigma^2 is 0 NaN (#360). 1 + x is the
                   argument of its logarithm, (1 - gt e^{-d tau}) / (1 - gt), so the branch taken is the same. */
                const std::complex<Real> nu = phi * std::complex<Real>(-phi, (j_ == 1) ? 1 : -1);
                const std::complex<Real> Dm =
                    (t1.real() * d.real() + t1.imag() * d.imag() > 0.0) ? nu / (t1 + d) : (t1 - d) / sigma2;
                const std::complex<Real> y = (d.real() != 0.0 || d.imag() != 0.0) ?
                                                 std::complex<Real>(-expm1(-d * tau) / (2.0 * d)) :
                                                 std::complex<Real>(0.5 * tau, 0.0);
                const std::complex<Real> u0 = D - Dm;
                const std::complex<Real> x = -sigma2 * u0 * y;
                const std::complex<Real> L =
                    (x == 0.0) ? std::complex<Real>(1.0, 0.0) : std::complex<Real>(log1p(x) / x);
                C += kappa * theta * (Dm * tau + 2.0 * u0 * y * L) +
                     std::complex<Real>(0.0, phi * (r_[i - 1] - q_[i - 1]) * tau);
                D = (D * (1.0 - (t1 + d) * y) + nu * y) / (1.0 + x);
            }
        }
        return std::exp(v0_ * D + C + std::complex<Real>(0.0, phi * (x_ - sx_))).imag() / phi;
    }

    class AnalyticPTDHestonEngine::AP_Helper
    {
      public:
        AP_Helper(Time term,
                  Real s0,
                  Real strike,
                  Real ratio,
                  Volatility sigmaBS,
                  const AnalyticPTDHestonEngine* const enginePtr)
        : term_(term), sigmaBS_(sigmaBS), x_(std::log(s0)), sx_(std::log(strike)), dd_(x_ - std::log(ratio)),
          enginePtr_(enginePtr)
        {
            QL_REQUIRE(enginePtr != nullptr, "pricing engine required");
        }

        Real operator()(Real u) const
        {
            const std::complex<Real> z(u, -0.5);

            const std::complex<Real> phiBS =
                std::exp(-0.5 * sigmaBS_ * sigmaBS_ * term_ * (z * z + std::complex<Real>(-z.imag(), z.real())));

            return (std::exp(std::complex<Real>(0.0, u * (dd_ - sx_))) * (phiBS - enginePtr_->chF(z, term_)) /
                    (u * u + 0.25))
                .real();
        }

      private:
        const Time term_;
        const Volatility sigmaBS_;
        const Real x_, sx_, dd_;
        const AnalyticPTDHestonEngine* const enginePtr_;
    };


    std::complex<Real> AnalyticPTDHestonEngine::lnChF(const std::complex<Real>& z, Time T) const
    {

        const Real v0 = model_->v0();

        std::complex<Real> D = 0.0;
        std::complex<Real> C = 0.0;

        const TimeGrid& timeGrid = model_->timeGrid();
        const Time lastModelTime = timeGrid.back();

        QL_REQUIRE(T <= lastModelTime, "maturity (" << T
                                                    << ") is too large, "
                                                       "time grid is bounded by "
                                                    << lastModelTime);

        const Size lastI = std::distance(timeGrid.begin(), std::lower_bound(timeGrid.begin(), timeGrid.end(), T));

        for (Integer i = lastI - 1; i >= 0; --i)
        {
            const Time begin = timeGrid[i];
            const Time end = std::min(T, timeGrid[i + 1]);
            const Time tau = end - begin;

            const Time t = 0.5 * (end + begin);
            const Real kappa = model_->kappa(t);
            const Real sigma = model_->sigma(t);
            const Real theta = model_->theta(t);
            const Real rho = model_->rho(t);

            const Real sigma2 = sigma * sigma;

            const std::complex<Real> k = kappa + rho * sigma * std::complex<Real>(z.imag(), -z.real());

            const std::complex<Real> d = std::sqrt(k * k + (z * z + std::complex<Real>(-z.imag(), z.real())) * sigma2);

            // as in Fj_Helper (#360), with nu = -z (z + i)
            const std::complex<Real> nu = -z * std::complex<Real>(z.real(), z.imag() + 1);
            const std::complex<Real> Dm =
                (k.real() * d.real() + k.imag() * d.imag() > 0.0) ? nu / (k + d) : (k - d) / sigma2;
            const std::complex<Real> y = (d.real() != 0.0 || d.imag() != 0.0) ?
                                             std::complex<Real>(-expm1(-d * tau) / (2.0 * d)) :
                                             std::complex<Real>(0.5 * tau, 0.0);
            const std::complex<Real> u0 = D - Dm;
            const std::complex<Real> x = -sigma2 * u0 * y;
            const std::complex<Real> L = (x == 0.0) ? std::complex<Real>(1.0, 0.0) : std::complex<Real>(log1p(x) / x);
            C += kappa * theta * (Dm * tau + 2.0 * u0 * y * L);
            D = (D * (1.0 - (k + d) * y) + nu * y) / (1.0 + x);
        }

        return D * v0 + C;
    }

    std::complex<Real> AnalyticPTDHestonEngine::chF(const std::complex<Real>& z, Time T) const
    {
        return std::exp(lnChF(z, T));
    }

    AnalyticPTDHestonEngine::AnalyticPTDHestonEngine(const ext::shared_ptr<PiecewiseTimeDependentHestonModel>& model,
                                                     Size integrationOrder)
    : GenericModelEngine<PiecewiseTimeDependentHestonModel, VanillaOption::arguments, VanillaOption::results>(model),
      evaluations_(0), cpxLog_(Gatheral), integration_(new Integration(Integration::gaussLaguerre(integrationOrder))),
      andersenPiterbargEpsilon_(Null<Real>())
    {
    }

    AnalyticPTDHestonEngine::AnalyticPTDHestonEngine(const ext::shared_ptr<PiecewiseTimeDependentHestonModel>& model,
                                                     Real relTolerance,
                                                     Size maxEvaluations)
    : GenericModelEngine<PiecewiseTimeDependentHestonModel, VanillaOption::arguments, VanillaOption::results>(model),
      evaluations_(0), cpxLog_(Gatheral),
      integration_(new Integration(Integration::gaussLobatto(relTolerance, Null<Real>(), maxEvaluations))),
      andersenPiterbargEpsilon_(Null<Real>())
    {
    }

    AnalyticPTDHestonEngine::AnalyticPTDHestonEngine(const ext::shared_ptr<PiecewiseTimeDependentHestonModel>& model,
                                                     ComplexLogFormula cpxLog,
                                                     const Integration& itg,
                                                     Real andersenPiterbargEpsilon)
    : GenericModelEngine<PiecewiseTimeDependentHestonModel, VanillaOption::arguments, VanillaOption::results>(model),
      evaluations_(0), cpxLog_(cpxLog), integration_(new Integration(itg)),
      andersenPiterbargEpsilon_(andersenPiterbargEpsilon)
    {
    }


    void AnalyticPTDHestonEngine::calculate() const
    {
        // this is an european option pricer
        QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "not an European option");

        // plain vanilla
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-striked payoff given");

        const Real v0 = model_->v0();
        const Real spotPrice = model_->s0();
        QL_REQUIRE(spotPrice > 0.0, "negative or null underlying given");

        const Real strike = payoff->strike();
        const Real term = model_->riskFreeRate()->dayCounter().yearFraction(model_->riskFreeRate()->referenceDate(),
                                                                            arguments_.exercise->lastDate());

        QL_REQUIRE(term < model_->timeGrid().back() || close_enough(term, model_->timeGrid().back()),
                   "maturity (" << term << ") is too large, time grid is bounded by " << model_->timeGrid().back());

        const Real riskFreeDiscount = model_->riskFreeRate()->discount(arguments_.exercise->lastDate());
        const Real dividendDiscount = model_->dividendYield()->discount(arguments_.exercise->lastDate());

        // average values
        const TimeGrid& timeGrid = model_->timeGrid();
        QL_REQUIRE(timeGrid.size() > 1, "at least two model points needed");

        const Size n = timeGrid.size() - 1;
        Real kappaAvg = 0.0, thetaAvg = 0.0, sigmaAvg = 0.0, rhoAvg = 0.0;

        for (Size i = 1; i <= n; ++i)
        {
            const Time t = 0.5 * (timeGrid[i - 1] + timeGrid[i]);
            kappaAvg += model_->kappa(t);
            thetaAvg += model_->theta(t);
            sigmaAvg += model_->sigma(t);
            rhoAvg += model_->rho(t);
        }
        kappaAvg /= n;
        thetaAvg /= n;
        sigmaAvg /= n;
        rhoAvg /= n;

        evaluations_ = 0;

        switch (cpxLog_)
        {
            case Gatheral:
            {
                const Real c_inf = std::min(0.2, std::max(0.0001, std::sqrt(1.0 - squared(rhoAvg)) / sigmaAvg)) *
                                   (v0 + kappaAvg * thetaAvg * term);

                const Real p1 = integration_->calculate(c_inf, Fj_Helper(model_, term, strike, 1)) / M_PI;
                evaluations_ += integration_->numberOfEvaluations();

                const Real p2 = integration_->calculate(c_inf, Fj_Helper(model_, term, strike, 2)) / M_PI;
                evaluations_ += integration_->numberOfEvaluations();

                switch (payoff->optionType())
                {
                    case Option::Call:
                        results_.value =
                            spotPrice * dividendDiscount * (p1 + 0.5) - strike * riskFreeDiscount * (p2 + 0.5);
                        break;
                    case Option::Put:
                        results_.value =
                            spotPrice * dividendDiscount * (p1 - 0.5) - strike * riskFreeDiscount * (p2 - 0.5);
                        break;
                    default:
                        QL_FAIL("unknown option type");
                }
            }
            break;
            case AndersenPiterbarg:
            {
                QL_REQUIRE(term <= timeGrid.back(), "maturity (" << term
                                                                 << ") is too large, "
                                                                    "time grid is bounded by "
                                                                 << timeGrid.back());

                const Time t05 = 0.5 * timeGrid.at(1);

                const std::complex<Real> D_u_inf =
                    -std::complex<Real>(std::sqrt(1 - squared(model_->rho(t05))), model_->rho(t05)) /
                    model_->sigma(t05);

                const Size lastI =
                    std::distance(timeGrid.begin(), std::lower_bound(timeGrid.begin(), timeGrid.end(), term));

                std::complex<Real> C_u_inf(0.0, 0.0);
                for (Size i = 0; i < lastI; ++i)
                {
                    const Time begin = timeGrid[i];
                    const Time end = std::min(term, timeGrid[i + 1]);
                    const Time tau = end - begin;
                    const Time t = 0.5 * (end + begin);

                    const Real kappa = model_->kappa(t);
                    const Real theta = model_->theta(t);
                    const Real sigma = model_->sigma(t);
                    const Real rho = model_->rho(t);

                    C_u_inf += -kappa * theta * tau / sigma * std::complex<Real>(std::sqrt(1 - rho * rho), rho);
                }

                const Real ratio = riskFreeDiscount / dividendDiscount;

                const Real fwdPrice = spotPrice / ratio;

                const Real epsilon =
                    andersenPiterbargEpsilon_ * M_PI / (std::sqrt(strike * fwdPrice) * riskFreeDiscount);

                const Real c_inf = -(C_u_inf + D_u_inf * v0).real();

                // through expm1, which keeps its precision where kappaAvg term is small, and is 1 where it is 0
                const Real vAvg = thetaAvg + (v0 - thetaAvg) * detail::decayRatio(kappaAvg * term);

                /* The integrand is (phiBS - phi) / (u^2 + 1/4), phiBS the control variate's Gaussian at vAvg. So the
                   closed form's Gaussian part takes vAvg, where phiBS falls to epsilon, and the limit is then read off
                   phi(u - i / 2), as AnalyticHestonEngine's is (#340), where the criterion there is still above
                   epsilon: on piecewise parameters c_inf and any one variance can misstate how far phi needs (#344). */
                const std::function<Real()> uM = [=, this]()
                {
                    return detail::andersenPiterbargLimitFromChF(
                        Integration::andersenPiterbargIntegrationLimit(c_inf, epsilon, vAvg, term), epsilon,
                        [&](Real u) { return lnChF(std::complex<Real>(u, -0.5), term).real(); });
                };

                const Real bsPrice =
                    BlackCalculator(Option::Call, strike, fwdPrice, std::sqrt(vAvg * term), riskFreeDiscount).value();

                // exp-sinh's scaling, s = 0.25 / sqrt(vAvg t / 2) held between 0.25 and 1,000, as AnalyticHestonEngine
                // takes it for its Andersen-Piterbarg forms; until #382 this engine took s = 1
                const Real scalingFactor = std::max(0.25, std::min(1000.0, 0.25 / std::sqrt(0.5 * vAvg * term)));
                const Real h_cv =
                    integration_->calculate(c_inf, AP_Helper(term, spotPrice, strike, ratio, std::sqrt(vAvg), this), uM,
                                            scalingFactor) *
                    std::sqrt(strike * fwdPrice) * riskFreeDiscount / M_PI;
                evaluations_ += integration_->numberOfEvaluations();

                switch (payoff->optionType())
                {
                    case Option::Call:
                        results_.value = bsPrice + h_cv;
                        break;
                    case Option::Put:
                        results_.value = bsPrice + h_cv - riskFreeDiscount * (fwdPrice - strike);
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

    Size AnalyticPTDHestonEngine::numberOfEvaluations() const
    {
        return evaluations_;
    }
}
