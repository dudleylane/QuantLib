/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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

#include <ql/exercise.hpp>
#include <ql/math/distributions/bivariatenormaldistribution.hpp>
#include <ql/pricingengines/barrier/analyticpartialtimebarrieroptionengine.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <cmath>
#include <utility>

namespace QuantLib
{

    AnalyticPartialTimeBarrierOptionEngine::AnalyticPartialTimeBarrierOptionEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process))
    {
        registerWith(process_);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::calculate(
        PartialTimeBarrierOption::arguments& arguments,
        const ext::shared_ptr<PlainVanillaPayoff>& payoff,
        const ext::shared_ptr<GeneralizedBlackScholesProcess>& process) const
    {
        Barrier::Type barrierType = arguments.barrierType;
        PartialBarrier::Range barrierRange = arguments.barrierRange;
        Rate r = process->riskFreeRate()->zeroRate(residualTime(), Continuous, NoFrequency);
        Rate q = process->dividendYield()->zeroRate(residualTime(), Continuous, NoFrequency);
        Real barrier = arguments.barrier;
        Real strike = payoff->strike();

        switch (barrierType)
        {
            case Barrier::DownOut:
                switch (barrierRange)
                {
                    case PartialBarrier::Start:
                        return CA(1, barrier, strike, r, q);
                    case PartialBarrier::EndB1:
                        return CoB1(barrier, strike, r, q);
                    case PartialBarrier::EndB2:
                        return CoB2(Barrier::DownOut, barrier, strike, r, q);
                    default:
                        QL_FAIL("invalid barrier range");
                }
                break;

            case Barrier::DownIn:
                switch (barrierRange)
                {
                    case PartialBarrier::Start:
                        return CIA(1, barrier, strike, r, q, payoff, process);
                    case PartialBarrier::EndB1:
                    case PartialBarrier::EndB2:
                        QL_FAIL("Down-and-in partial-time end barrier is not implemented");
                    default:
                        QL_FAIL("invalid barrier range");
                }
                break;

            case Barrier::UpOut:
                switch (barrierRange)
                {
                    case PartialBarrier::Start:
                        return CA(-1, barrier, strike, r, q);
                    case PartialBarrier::EndB1:
                        return CoB1(barrier, strike, r, q);
                    case PartialBarrier::EndB2:
                        return CoB2(Barrier::UpOut, barrier, strike, r, q);
                    default:
                        QL_FAIL("invalid barrier range");
                }
                break;

            case Barrier::UpIn:
                switch (barrierRange)
                {
                    case PartialBarrier::Start:
                        return CIA(-1, barrier, strike, r, q, payoff, process);
                    case PartialBarrier::EndB1:
                    case PartialBarrier::EndB2:
                        QL_FAIL("Up-and-in partial-time end barrier is not implemented");
                    default:
                        QL_FAIL("invalid barrier range");
                }
                break;
            default:
                QL_FAIL("unknown barrier type");
        }
    }

    void AnalyticPartialTimeBarrierOptionEngine::calculate() const
    {
        ext::shared_ptr<PlainVanillaPayoff> payoff = ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        QL_REQUIRE(payoff->strike() > 0.0, "strike must be positive");

        Real spot = process_->x0();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");
        // Part of a window would be in the past, where the engine can't know the path.
        QL_REQUIRE(coverEventTime() >= 0.0, "cover event date has passed");
        // A window that starts today watches today's spot, so a spot past the barrier has hit it already: the Start
        // window, and EndB2 with the cover date today. EndB1 is hit from either side, so only by a spot on the barrier.
        // As in AnalyticBarrierEngine, a spot on the barrier passes, and a knock-out there is worth 0.
        if (arguments_.barrierRange == PartialBarrier::Start ||
            (arguments_.barrierRange == PartialBarrier::EndB2 && coverEventTime() == 0.0))
        {
            bool down = arguments_.barrierType == Barrier::DownIn || arguments_.barrierType == Barrier::DownOut;
            QL_REQUIRE(down ? spot >= arguments_.barrier : spot <= arguments_.barrier, "barrier touched");
        }

        auto getSymmetricBarrierType = [](Barrier::Type barrierType) -> Barrier::Type
        {
            if (barrierType == Barrier::UpIn)
                return Barrier::DownIn;
            if (barrierType == Barrier::DownIn)
                return Barrier::UpIn;
            if (barrierType == Barrier::UpOut)
                return Barrier::DownOut;
            return Barrier::UpOut;
        };

        auto tmp_arguments_ = arguments_;
        if (payoff->optionType() == Option::Put)
        {
            // S (S / H) rather than S^2 / H, which can round away from S: a spot on the barrier must stay on it. The
            // strike goes through the same expression, so that rounding can't move it past the barrier or off it.
            Real callStrike = spot * (spot / payoff->strike());
            ext::shared_ptr<PlainVanillaPayoff> callPayoff =
                ext::make_shared<PlainVanillaPayoff>(Option::Call, callStrike);
            tmp_arguments_.barrierType = getSymmetricBarrierType(arguments_.barrierType);
            tmp_arguments_.barrier = spot * (spot / arguments_.barrier);
            tmp_arguments_.payoff = callPayoff;
            auto callProcess = ext::make_shared<GeneralizedBlackScholesProcess>(
                process_->stateVariable(), process_->riskFreeRate(), process_->dividendYield(),
                process_->blackVolatility());

            results_.value = payoff->strike() / spot * calculate(tmp_arguments_, callPayoff, callProcess);
        }
        else
            results_.value = calculate(tmp_arguments_, payoff, process_);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::CoB2(
        Barrier::Type barrierType, Real barrier, Real strike, Rate r, Rate q) const
    {
        QL_REQUIRE(strike < barrier, "case of strike>barrier is not implemented for OutEnd B2 type");

        Real b = r - q;
        if (zeroVariance(strike))
        {
            auto [low, high] = forwardRange(coverEventTime(), residualTime(), b);
            bool hit = barrierType == Barrier::DownOut ? low <= barrier : high >= barrier;
            return hit ? 0.0 : vanillaAtZeroVariance(strike, r, q);
        }

        // With the cover date today, a spot on the barrier hits it now. The e terms would be 0 / 0.
        if (coverEventTime() == 0.0 && underlying() == barrier)
            return 0.0;

        Real result = 0.0;
        Real T = residualTime();
        Real S = underlying();
        Real mu_ = mu(strike, b);
        Real g1_ = g1(barrier, strike, b);
        Real g2_ = g2(barrier, strike, b);
        Real g3_ = g3(barrier, strike, b);
        Real g4_ = g4(barrier, strike, b);
        Real e1_ = e1(barrier, strike, b);
        Real e2_ = e2(barrier, strike, b);
        Real e3_ = e3(barrier, strike, b);
        Real e4_ = e4(barrier, strike, b);
        Real rho_ = rho();
        Real HSMu = HS(S, barrier, 2 * mu_);
        Real HSMu1 = HS(S, barrier, 2 * (mu_ + 1));
        Real logHS = std::log(barrier / S);
        auto imageMu = [&](Real x, Real y, Real corr) { return powerTimesM(HSMu, 2 * mu_ * logHS, x, y, corr); };
        auto imageMu1 = [&](Real x, Real y, Real corr)
        { return powerTimesM(HSMu1, 2 * (mu_ + 1) * logHS, x, y, corr); };
        Real X1 = strike * std::exp(-r * T);

        switch (barrierType)
        {
            case Barrier::DownOut:
                result = S * std::exp((b - r) * T);
                result *= (M(g1_, e1_, rho_) - imageMu1(g3_, -e3_, -rho_));
                result -= X1 * (M(g2_, e2_, rho_) - imageMu(g4_, -e4_, -rho_));
                return result;

            case Barrier::UpOut:
                result = S * std::exp((b - r) * T);
                result *= (M(-g1_, -e1_, rho_) - imageMu1(-g3_, e3_, -rho_));
                result -= X1 * (M(-g2_, -e2_, rho_) - imageMu(-g4_, e4_, -rho_));
                result -= S * std::exp((b - r) * T) *
                          (M(-d1(strike, b), -e1_, rho_) - imageMu1(e3_, -f1(barrier, strike, b), -rho_));
                result += X1 * (M(-d2(strike, b), -e2_, rho_) - imageMu(e4_, -f2(barrier, strike, b), -rho_));
                return result;

            default:
                QL_FAIL("invalid barrier type");
        }
    }

    Real AnalyticPartialTimeBarrierOptionEngine::CoB1(Real barrier, Real strike, Rate r, Rate q) const
    {
        Rate b = r - q;
        if (zeroVariance(strike))
        {
            // B1 is knocked out by a hit from either side.
            auto [low, high] = forwardRange(coverEventTime(), residualTime(), b);
            return low <= barrier && barrier <= high ? 0.0 : vanillaAtZeroVariance(strike, r, q);
        }

        // With the cover date today, a spot on the barrier hits it now. The e terms would be 0 / 0.
        if (coverEventTime() == 0.0 && underlying() == barrier)
            return 0.0;

        Real result = 0.0;
        Real T = residualTime();
        Real S = underlying();
        Real mu_ = mu(strike, b);
        Real g1_ = g1(barrier, strike, b);
        Real g2_ = g2(barrier, strike, b);
        Real g3_ = g3(barrier, strike, b);
        Real g4_ = g4(barrier, strike, b);
        Real e1_ = e1(barrier, strike, b);
        Real e2_ = e2(barrier, strike, b);
        Real e3_ = e3(barrier, strike, b);
        Real e4_ = e4(barrier, strike, b);
        Real rho_ = rho();
        Real HSMu = HS(S, barrier, 2 * mu_);
        Real HSMu1 = HS(S, barrier, 2 * (mu_ + 1));
        Real logHS = std::log(barrier / S);
        auto imageMu = [&](Real x, Real y, Real corr) { return powerTimesM(HSMu, 2 * mu_ * logHS, x, y, corr); };
        auto imageMu1 = [&](Real x, Real y, Real corr)
        { return powerTimesM(HSMu1, 2 * (mu_ + 1) * logHS, x, y, corr); };
        Real X1 = strike * std::exp(-r * T);

        if (strike > barrier)
        {
            result = S * std::exp((b - r) * T);
            result *= (M(d1(strike, b), e1_, rho_) - imageMu1(f1(barrier, strike, b), -e3_, -rho_));
            result -= X1 * (M(d2(strike, b), e2_, rho_) - imageMu(f2(barrier, strike, b), -e4_, -rho_));
            return result;
        }
        else
        {
            Real S1 = S * std::exp((b - r) * T);
            result = S1;
            result *= (M(-g1_, -e1_, rho_) - imageMu1(-g3_, e3_, -rho_));
            result -= X1 * (M(-g2_, -e2_, rho_) - imageMu(-g4_, e4_, -rho_));
            result -= S1 * (M(-d1(strike, b), -e1_, rho_) - imageMu1(-f1(barrier, strike, b), e3_, -rho_));
            result += X1 * (M(-d2(strike, b), -e2_, rho_) - imageMu(-f2(barrier, strike, b), e4_, -rho_));
            result += S1 * (M(g1_, e1_, rho_) - imageMu1(g3_, -e3_, -rho_));
            result -= X1 * (M(g2_, e2_, rho_) - imageMu(g4_, -e4_, -rho_));
            return result;
        }
    }

    // eta = -1: Up-and-In Call
    // eta =  1: Down-and-In Call
    // A put comes here as the equivalent call, whose payoff and process these are.
    Real
    AnalyticPartialTimeBarrierOptionEngine::CIA(Integer eta,
                                                Real barrier,
                                                Real strike,
                                                Rate r,
                                                Rate q,
                                                const ext::shared_ptr<PlainVanillaPayoff>& payoff,
                                                const ext::shared_ptr<GeneralizedBlackScholesProcess>& process) const
    {
        // The same vanilla as CA's, so that a knock-in that isn't hit is worth 0 exactly.
        if (zeroVariance(strike))
            return vanillaAtZeroVariance(strike, r, q) - CA(eta, barrier, strike, r, q);

        ext::shared_ptr<EuropeanExercise> exercise = ext::dynamic_pointer_cast<EuropeanExercise>(arguments_.exercise);

        VanillaOption europeanOption(payoff, exercise);

        europeanOption.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(process));

        return europeanOption.NPV() - CA(eta, barrier, strike, r, q);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::CA(Integer eta, Real barrier, Real strike, Rate r, Rate q) const
    {
        // Partial-Time-Start- OUT  Call Option calculation
        Real b = r - q;
        if (zeroVariance(strike))
        {
            auto [low, high] = forwardRange(0.0, coverEventTime(), b);
            bool hit = eta == 1 ? low <= barrier : high >= barrier;
            return hit ? 0.0 : vanillaAtZeroVariance(strike, r, q);
        }

        // With the cover date today, a spot on the barrier hits it now. The e terms would be 0 / 0.
        if (coverEventTime() == 0.0 && underlying() == barrier)
            return 0.0;

        Real rho_ = rho();
        Real T = residualTime();
        Real S = underlying();
        Real mu_ = mu(strike, b);
        Real e1_ = e1(barrier, strike, b);
        Real e2_ = e2(barrier, strike, b);
        Real e3_ = e3(barrier, strike, b);
        Real e4_ = e4(barrier, strike, b);
        Real HSMu = HS(S, barrier, 2 * mu_);
        Real HSMu1 = HS(S, barrier, 2 * (mu_ + 1));
        Real logHS = std::log(barrier / S);
        auto imageMu = [&](Real x, Real y, Real corr) { return powerTimesM(HSMu, 2 * mu_ * logHS, x, y, corr); };
        auto imageMu1 = [&](Real x, Real y, Real corr)
        { return powerTimesM(HSMu1, 2 * (mu_ + 1) * logHS, x, y, corr); };

        Real result;
        result = S * std::exp((b - r) * T);
        result *= (M(d1(strike, b), eta * e1_, eta * rho_) - imageMu1(f1(barrier, strike, b), eta * e3_, eta * rho_));
        result -= (strike * std::exp(-r * T) *
                   (M(d2(strike, b), eta * e2_, eta * rho_) - imageMu(f2(barrier, strike, b), eta * e4_, eta * rho_)));
        return result;
    }

    Real AnalyticPartialTimeBarrierOptionEngine::underlying() const
    {
        return process_->x0();
    }

    Time AnalyticPartialTimeBarrierOptionEngine::residualTime() const
    {
        return process_->time(arguments_.exercise->lastDate());
    }

    Time AnalyticPartialTimeBarrierOptionEngine::coverEventTime() const
    {
        return process_->time(arguments_.coverEventDate);
    }

    Volatility AnalyticPartialTimeBarrierOptionEngine::volatility(Time t, Real strike) const
    {
        return process_->blackVolatility()->blackVol(t, strike);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::f1(Real barrier, Real strike, Rate b) const
    {
        Real S = underlying();
        Real T = residualTime();
        Real sigma = volatility(T, strike);
        return (std::log(S / strike) + 2 * std::log(barrier / S) + (b + (std::pow(sigma, 2) / 2)) * T) /
               (sigma * std::sqrt(T));
    }

    Real AnalyticPartialTimeBarrierOptionEngine::f2(Real barrier, Real strike, Rate b) const
    {
        Time T = residualTime();
        return f1(barrier, strike, b) - volatility(T, strike) * std::sqrt(T);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::M(Real a, Real b, Real rho) const
    {
        BivariateCumulativeNormalDistribution CmlNormDist(rho);
        return CmlNormDist(a, b);
    }

    // power * M(a, b; rho), where power = exp(logPower). logValue is within about 1e-14 of M however small M is, at
    // about 30 times its cost. Where M is 0.1 or more, M itself is that close. Below 0.1 its absolute error is at most
    // about 1e-16, and the product's error is the power times it, so up to a power of 100 the product is within about
    // 1e-14 of the term's prefactor. So where M is below 0.1 and the power above 100, which takes a low volatility or a
    // distant barrier, the product goes through logs, and it stays finite where the power overflows and M underflows.
    Real AnalyticPartialTimeBarrierOptionEngine::powerTimesM(Real power, Real logPower, Real a, Real b, Real rho) const
    {
        BivariateCumulativeNormalDistribution f(rho);
        Real m = f(a, b);
        return power <= 100.0 || m >= 0.1 ? Real(power * m) : Real(std::exp(logPower + f.logValue(a, b)));
    }

    Real AnalyticPartialTimeBarrierOptionEngine::rho() const
    {
        return std::sqrt(coverEventTime() / residualTime());
    }

    Rate AnalyticPartialTimeBarrierOptionEngine::mu(Real strike, Rate b) const
    {
        Volatility vol = volatility(coverEventTime(), strike);
        return (b - (vol * vol) / 2) / (vol * vol);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::d1(Real strike, Rate b) const
    {
        Time T2 = residualTime();
        Volatility vol = volatility(T2, strike);
        return (std::log(underlying() / strike) + (b + vol * vol / 2) * T2) / (std::sqrt(T2) * vol);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::d2(Real strike, Rate b) const
    {
        Time T2 = residualTime();
        Volatility vol = volatility(T2, strike);
        return d1(strike, b) - vol * std::sqrt(T2);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::e1(Real barrier, Real strike, Rate b) const
    {
        Time T1 = coverEventTime();
        Volatility vol = volatility(T1, strike);
        return (std::log(underlying() / barrier) + (b + vol * vol / 2) * T1) / (std::sqrt(T1) * vol);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::e2(Real barrier, Real strike, Rate b) const
    {
        Time T1 = coverEventTime();
        Volatility vol = volatility(T1, strike);
        return e1(barrier, strike, b) - vol * std::sqrt(T1);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::e3(Real barrier, Real strike, Rate b) const
    {
        Time T1 = coverEventTime();
        Real vol = volatility(T1, strike);
        // e1 + 2 ln(H/S) / (vol sqrt(T1)), with the logarithms added first: at T1 = 0 both terms are infinite.
        return (std::log(barrier / underlying()) + (b + vol * vol / 2) * T1) / (std::sqrt(T1) * vol);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::e4(Real barrier, Real strike, Rate b) const
    {
        Time t = coverEventTime();
        return e3(barrier, strike, b) - volatility(t, strike) * std::sqrt(t);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::g1(Real barrier, Real strike, Rate b) const
    {
        Time T2 = residualTime();
        Volatility vol = volatility(T2, strike);
        return (std::log(underlying() / barrier) + (b + vol * vol / 2) * T2) / (std::sqrt(T2) * vol);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::g2(Real barrier, Real strike, Rate b) const
    {
        Time T2 = residualTime();
        Volatility vol = volatility(T2, strike);
        return g1(barrier, strike, b) - vol * std::sqrt(T2);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::g3(Real barrier, Real strike, Rate b) const
    {
        Time T2 = residualTime();
        Real vol = volatility(T2, strike);
        return g1(barrier, strike, b) + (2 * std::log(barrier / underlying()) / (vol * std::sqrt(T2)));
    }

    Real AnalyticPartialTimeBarrierOptionEngine::g4(Real barrier, Real strike, Rate b) const
    {
        Time T2 = residualTime();
        Real vol = volatility(T2, strike);
        return g3(barrier, strike, b) - vol * std::sqrt(T2);
    }

    Real AnalyticPartialTimeBarrierOptionEngine::HS(Real S, Real H, Real power) const
    {
        return std::pow((H / S), power);
    }

    // The formulas divide by the volatility. Without any, the underlying follows its forward.
    bool AnalyticPartialTimeBarrierOptionEngine::zeroVariance(Real strike) const
    {
        return process_->blackVolatility()->blackVariance(residualTime(), strike) < QL_EPSILON;
    }

    // The forward is monotonic, so over [from, to] it is lowest and highest at the ends.
    std::pair<Real, Real> AnalyticPartialTimeBarrierOptionEngine::forwardRange(Time from, Time to, Rate b) const
    {
        Real first = underlying() * std::exp(b * from), last = underlying() * std::exp(b * to);
        return {std::min(first, last), std::max(first, last)};
    }

    Real AnalyticPartialTimeBarrierOptionEngine::vanillaAtZeroVariance(Real strike, Rate r, Rate q) const
    {
        Time T = residualTime();
        return std::exp(-r * T) * std::max(underlying() * std::exp((r - q) * T) - strike, Real(0.0));
    }

}
