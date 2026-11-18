/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Jose Aparicio

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

#ifndef quantlib_saddle_point_lossmodel_hpp
#define quantlib_saddle_point_lossmodel_hpp

#include <ql/experimental/credit/basket.hpp>
#include <ql/experimental/credit/constantlosslatentmodel.hpp>
#include <ql/experimental/credit/defaultlossmodel.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/math/solvers1d/newton.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

namespace QuantLib
{

    namespace detail
    {
        /*! A name that loses l on default with probability p, under the measure that tilts the loss by s: given
            x = l s, its term in the cumulant generating function, log(1 - p + p e^x), and its probabilities of
            default and survival under that measure, q = p e^x / (1 - p + p e^x) and t = 1 - q. All three are
            taken from e^{-|x|}, which neither overflows nor, for p close to 1, loses 1 - p next to e^x.
        */
        inline void tiltedName(Probability p, Real x, Real& logTerm, Real& q, Real& t)
        {
            if (x > 0.)
            {
                Real e = std::exp(-x), denominator = p + (1. - p) * e;
                q = p / denominator;
                t = (1. - p) * e / denominator;
                logTerm = x + std::log(denominator);
            }
            else
            {
                Real e = std::exp(x), denominator = 1. - p + p * e;
                q = p * e / denominator;
                t = (1. - p) / denominator;
                logTerm = std::log(denominator);
            }
        }

        /*! The integrals of the standard normal density against e^{-u z} above 0 that the first-order saddle-point
            forms take, for u >= 0: tail = e^{u^2/2} Phi(-u) and excess = phi(0) - u tail. Given the saddle point s
            of a level y, and u = |s| sqrt(K''(s)), the tail probability beyond y is e^{K(s) - s y} tail and the
            expected excess beyond it e^{K(s) - s y} sqrt(K''(s)) excess. Below u = 3 they are taken directly; from
            there, where the direct forms lose their digits to cancellation and overflow, from Laplace's continued
            fraction for Mills' ratio R(u) = sqrt(2 pi) tail: 1 / R(u) = u + 1 / T(u), with T(u) = u + 2 / (u + 3 /
            (u + ...)), so that excess = tail / T(u). Within 4e-16 of the exact values, relative, from u = 3, and
            2.3e-14 below it.
        */
        inline void saddlePointTailIntegrals(Real u, Real& tail, Real& excess)
        {
            if (u < 3.0)
            {
                tail = std::exp(0.5 * u * u) * 0.5 * std::erfc(u * M_SQRT1_2);
                excess = 1.0 / std::sqrt(2.0 * M_PI) - u * tail;
                return;
            }
            Real t = u;
            for (int k = 60; k >= 2; --k)
                t = u + k / t;
            tail = 1.0 / ((u + 1.0 / t) * std::sqrt(2.0 * M_PI));
            excess = tail / t;
        }
    }

    /*! \brief Saddle point portfolio credit default loss model.\par
      Default Loss model implementing the Saddle point expansion
      integrations on several default risk metrics. Codepence is dealt
      through a latent model making the integrals conditional to the latent
      model factor. Latent variables are integrated indirectly.\par
    See:\par
    <b>Taking to the saddle</b> by R.Martin, K.Thompson and C.Browne; RISK JUNE
        2001; p.91\par
    <b>The saddlepoint method and portfolio optionalities</b> R.Martin in Risk
        December 2006\par
    <b>VAR: who contributes and how much?</b> R.Martin, K.Thompson and
        C.Browne RISK AUGUST 2001\par
    <b>Shortfall: Who contributes and how much?</b> R. J. Martin, Credit Suisse
        January 3, 2007 \par
    <b>Don't Fall from the Saddle: the Importance of Higher Moments of Credit
        Loss Distributions</b> J.Annaert, C.Garcia Joao Batista, J.Lamoot,
        G.Lanine February 2006, Gent University\par
    <b>Analytical techniques for synthetic CDOs and credit default risk
        measures</b> A. Antonov, S. Mechkovy, and T. Misirpashaevz;
        NumeriX May 23, 2005 \par
    <b>Computation of VaR and VaR contribution in the Vasicek portfolio credit
        loss model: a comparative study</b> X.Huang, C.W.Oosterlee, M.Mesters
        Journal of Credit Risk (75-96) Volume 3/ Number 3, Fall 2007 \par
    <b>Higher-order saddlepoint approximations in the Vasicek portfolio credit
        loss model</b> X.Huang, C.W.Oosterlee, M.Mesters  Journal of
        Computational Finance (93-113) Volume 11/Number 1, Fall 2007 \par
    The tail probability and the expected excess of the loss over a level
    take the first-order forms, both from one tilted normal approximation,
    and the percentile and the expected shortfall come from them (#204). The
    density keeps the second-order expansion, see the paper by Antonov et al
    for the terms retained, where its correction is no larger than its
    leading term, and is that term alone elsewhere (#308); so given the
    factor it steps where the correction reaches the leading term, to that
    term from 0 or from twice it.\par
    For a discussion of an alternative to fix the error at low loss levels
    (more relevant to pricing than risk metrics) see: \par
    <b>The hybrid saddlepoint method for credit portfolios</b> by A.Owen,
    A.McLeod and K.Thompson; in Risk, August 2009. This is not implemented here
    though (yet?...)\par
    For the more general context mathematical theory see: <b>Saddlepoint
    approximations with applications</b> by R.W. Butler, Cambridge series in
    statistical and probabilistic mathematics. 2007 \par
    \warning Next to an atom of the distribution given the factor that,
        under the saddle point's tilt, names all but certain to default or
        to survive make, the density rises like the inverse square root of
        the distance to it, and is large at the atom itself (#324): on five
        names that each lose 60, two of them on a loading of sqrt(0.999), it
        is 747 one double below three defaults a year out, and 2.1e7 at
        them, but within 0.1 below them it holds 6.5e-6 of probability,
        against the atom's 0.0126.
    \todo Methods here are calling and integrating using the unconditional
        probabilities without inverting them first; quite a lot of calls to
        the copula inversion can be avoided; this should improve performance.
    \warning The expected shortfall is the percentile plus the expected
        excess of the tranche's loss over it, and the expected tranche loss
        is the shortfall at 0: given the factor, the expected excess of the
        loss over the attachment less that over the detachment (#262).
        Given the factor, these come from a smooth approximation, and where
        names lose alike the distribution is a lattice: far in its tail, and
        just above its atoms, the approximation is off (#309), as it is
        beside more than four names that might default and lose more than
        the two smallest together, whose atoms it smears (#325). On 20 names
        that each lose 60 of 100, with a correlation of 0.3, the shortfall
        on 50% to 100% of the basket is 9% below the exact value at five
        years and 12% at one; until #204 it was 42% and 63% below. On the
        same names the expected losses of the tranches from 6% to 9%, 9% to
        12% and 12% to 22% are each off by up to 10% at one year and 5.9% at
        five, under a Gaussian copula or a T copula of order 3; until #262
        they were the expected loss given the factor, tranched, and were off
        by up to 61% and 40%.
    \warning Given the factor, the loss distribution is exact up to the two
        smallest losses one default causes, taken together, where a loss
        short of the level has at most one default, and above the total loss
        less those two, where a loss needs every name but one to default,
        both on top of the loss of the names the factor makes certain to
        default; it comes from the approximation in between. A name that
        might default and loses more than those two together has its own
        loss and the total less it in between, so where at most four do,
        the model takes the others' distribution given each outcome of those
        names, all defaulting or not, each with its own exact parts (#308);
        where more do, the approximation, which smears their atoms (#325).
        In between, the expected excess over a level is held between bounds
        that the exact parts give, so that it meets them; the probability
        isn't, and steps where the parts meet (#309). On 19 names that lose 80
        to 99 each, under a Gaussian copula and a T copula of order 3, the
        error in the expected shortfall at levels up to 0.99 is at most
        0.0002% of a tranche of 3% of the basket, and 0.6% and 0.9% of a
        tranche of 10%; until #205 the exact part below ended at the smallest
        loss, the parts didn't meet, and those errors were 1.4% and 2.1%, and
        0.8% and 1.2%. On 20 names that each lose 60, the shortfall on a
        tranche of 10% is within 0.03% of the exact value from the 55th
        percentile to the 81st, and from there between 0.2% below it and 2.0%
        above, where the percentile comes from the approximation, which has no
        atoms (#309); until #205 it was up to 2.9% above.
    \warning The percentile is the level at which the tail probability
        crosses 1 less the probability asked for, solved to 1e-4 of the
        tranche. Where the tail jumps across it at a loss of the exact
        parts, it is that loss: at a node of the factor, what the names
        certain to default there lose, alone and plus each loss of the
        names that might default there up to their two smallest together,
        plus those two together, plus their total less each of these, and
        plus their total (#307, #317), and the same for the others given
        each outcome of the names that lose more than the two smallest
        together, where the model takes them so (#308). On the Trapezoid
        integration type, whose nodes depend on what it integrates, it can
        still be solved to 1e-4 of the tranche at such a loss (#320). Neither
        it nor the shortfall is above the most the tranche can lose, what the
        names that can default by the date lose less its attachment.
    */

    /* The treatment of recovery wont work with random recoveries, they should
    be passed to the conditional methods in the same way as the probabilities.
    */

    /*
    TO DO:
    - With 15 quadrature points things are OK but 25 gives me -1#IND000 errors
            (over region around the EL I think)
    - Silly bug when calling some methods on todays date (zero time).
            ProbDef = 0 there
    - VaR <- tranched?????!
    - VaR split
    - ESF split?

    When introducing defaults; somewhere, (after an update?) there should be
    a check that: copula_->basketSize() EQUALS remainingBasket_.size()
    */
    template <class CP>
    class SaddlePointLossModel : public DefaultLossModel, public Observer
    {
      public:
        explicit SaddlePointLossModel(const ext::shared_ptr<ConstantLossLatentmodel<CP>>& m) : copula_(m)
        {
            registerWith(copula_);
        }

      private:
        // what the latent model says goes on to the baskets
        void update() override { notifyObservers(); }

      protected:
        struct NamesCond;

        // ----------- Cumulants and derivatives auxiliary functions ---------

        /*! Returns the cumulant generating function (zero-th order
        expansion term) conditional to the mkt factor:
            \f$ K = \sum_j ln(1-p_j + p_j e^{N_j \times lgd_j \times s}) \f$
        */
        Real CumulantGeneratingCond(const std::vector<Real>& invUncondProbs,
                                    Real lossFraction, // saddle pt
                                    const std::vector<Real>& mktFactor) const;
        /*! Returns the first derivative of the cumulant generating function
        (first order expansion term) conditional to the mkt factor:
           \f$ K1 = \sum_j \frac{p_j \times N_j \times LGD_j \times
                e^{N_j \times LGD_j \times s}} \
                             {1-p_j + p_j e^{N_j \times LGD_j \times s}} \f$
           One of its properties is that its value at zero is the portfolio
           expected loss (in fractional units). Its value at infinity is the
           max attainable portfolio loss. To be understood conditional to the
           market factor.
        */
        Real CumGen1stDerivativeCond(const std::vector<Real>& invUncondProbs,
                                     Real saddle, // in fract loss units... humm not really
                                     const std::vector<Real>& mktFactor) const;
        /*! Returns the second derivative of the cumulant generating function
        (first order expansion term) conditional to the mkt factor:
            \f$ K2 = \sum_j \frac{p_j \times (N_j \times LGD_j)^2 \times
                e^{N_j \times LGD_j \times s}}
                             {1-p_j + p_j e^{N_j \times LGD_j \times s}}
                      - (\frac{p_j \times N_j \times LGD_j \times e^{N_j \times
                      LGD_j \times s}}
                             {1-p_j + p_j e^{N_j \times LGD_j \times s}})^2 \f$
        */
        Real CumGen2ndDerivativeCond(const std::vector<Real>& invUncondProbs,
                                     Real saddle,
                                     const std::vector<Real>& mktFactor) const;
        Real CumGen3rdDerivativeCond(const std::vector<Real>& invUncondProbs,
                                     Real saddle,
                                     const std::vector<Real>& mktFactor) const;
        Real CumGen4thDerivativeCond(const std::vector<Real>& invUncondProbs,
                                     Real saddle,
                                     const std::vector<Real>& mktFactor) const;
        /*! Returns the cumulant and second to fourth derivatives together.
          Included for optimization, most methods work on expansion of these
          terms.
          Alternatively use a local private buffer member? */
        std::tuple<Real, Real, Real, Real> CumGen0234DerivCond(const std::vector<Real>& invUncondProbs,
                                                               Real saddle,
                                                               const std::vector<Real>& mktFactor) const;
        std::tuple<Real, Real> CumGen02DerivCond(const std::vector<Real>& invUncondProbs,
                                                 Real saddle,
                                                 const std::vector<Real>& mktFactor) const;
        std::tuple<Real, Real, Real, Real> CumGen0234DerivCond(const NamesCond& names, Real saddle) const;
        std::tuple<Real, Real> CumGen02DerivCond(const NamesCond& names, Real saddle) const;

        /* Unconditional cumulants. Because this class integrates the various
          statistics it provides in indirect mode they are never used.
          Provided for completeness/extendability */
        /*! Returns the cumulant generating function (zero-th order expansion
          term) weighting the conditional value by the prob density of the
          market factor, called by integrations */
        Real CumulantGenerating(const Date& date, Real s) const;
        Real CumGen1stDerivative(const Date& date, Real s) const;
        Real CumGen2ndDerivative(const Date& date, Real s) const;
        Real CumGen3rdDerivative(const Date& date, Real s) const;
        Real CumGen4thDerivative(const Date& date, Real s) const;

        // -------- Saddle point search functions ---------------------------
        /* K'(s) less the target, as CumGen1stDerivativeCond gives it. Each
        name's default probability given the factors, and its loss on default,
        don't depend on s, so they're taken once, from the node's names, rather
        than at each step of the search.
        */
        class SaddleObjectiveFunction
        {
            Real targetValue_;
            std::vector<Probability> pDef_;
            std::vector<Real> lossInDef_;

          public:
            //! The passed target is in fractional loss units
            SaddleObjectiveFunction(const SaddlePointLossModel& me,
                                    const Real target,
                                    const std::vector<Real>& invUncondProbs,
                                    const std::vector<Real>& mktFactor)
            : SaddleObjectiveFunction(me, target, me.namesCond(invUncondProbs, mktFactor))
            {
            }
            SaddleObjectiveFunction(const SaddlePointLossModel& me, const Real target, const NamesCond& names)
            : targetValue_(target), pDef_(names.pDef)
            {
                const Size nNames = me.remainingNotionals_.size();
                lossInDef_.reserve(nNames);
                // loss in fractional units
                for (Size iName = 0; iName < nNames; iName++)
                    lossInDef_.push_back(me.remainingNotionals_[iName] * names.lgd[iName] / me.remainingNotional_);
            }
            Real operator()(const Real x) const
            {
                Real sum = 0.;
                for (Size iName = 0; iName < pDef_.size(); iName++)
                {
                    Real midFactor = pDef_[iName] * std::exp(lossInDef_[iName] * x);
                    sum += lossInDef_[iName] * midFactor / (1. - pDef_[iName] + midFactor);
                }
                return sum - targetValue_;
            }
        };

        /*! Where the loss distribution is known without a saddle point. Given the factor, the names that default with
            probability 1 lose a fixed amount, and the loss is that amount plus what the other names, the uncertain
            ones, lose. A name that loses nothing is never an uncertain one: none of its defaults changes the loss. Nor
            is a name whose default probability given the factor is 0: none of its defaults happens there (#318).
        */
        struct LossLimits
        {
            Real certain;     //!< the loss of the names certain to default, an amount
            Real smallest;    //!< the least loss one default of an uncertain name causes, an amount
            Real second;      //!< the next least, an amount; the same as the least if two names cause it
            Real total;       //!< the loss if every uncertain name defaults, an amount
            Probability none; //!< the probability that no uncertain name defaults
            Probability any;  //!< the probability that some uncertain name defaults
            Probability all;  //!< the probability that every uncertain name defaults
            Real mean;        //!< what the uncertain names lose on average, E[U], an amount
            Real beyond;      //!< what they lose beyond the smallest loss of one on average, E[(U - smallest)^+]
            Real largest;     //!< the greatest loss one default of an uncertain name causes, an amount
        };
        //! Whether a name is an uncertain one, given its default probability given the factor and its loss
        static bool uncertainName(Probability p, Real loss) { return p != 1. && p != 0. && loss > 0.; }
        /*! The tail of the loss at a level, given the market factor: the probability that the loss reaches it, and
            the expected excess of the loss over it, E[(L - y)^+], an amount. Exact in the regions the loss limits
            give, and from the first-order saddle-point forms elsewhere, which take both from the same tilted
            normal approximation; there the excess is held between bounds that the exact regions on either side
            give.
        */
        struct Tail
        {
            Probability over; //!< P(L >= y)
            Real excess;      //!< E[(L - y)^+], an amount
        };
        /*! Each name's default probability given the market factor, and its loss given default as a fraction of its
            notional, 1 less its recovery; the limits on the loss they make; and the exact tails where the exact
            regions end. None of them depends on the loss level, so the statistics at a node take them once and share
            them (#267): the names through the copula, the rest the first time a level needs them.
        */
        struct NamesCond
        {
            std::vector<Probability> pDef; //!< the default probabilities given the factor
            std::vector<Real> lgd;         //!< 1 less the recoveries given the factor
            mutable bool limited = false, joined = false;
            mutable LossLimits limits;
            mutable Tail low, high;
        };
        NamesCond namesCond(const std::vector<Real>& invUncondProbs, const std::vector<Real>& mktFactor) const
        {
            const Size nNames = remainingNotionals_.size();
            NamesCond names;
            names.pDef.reserve(nNames);
            names.lgd.reserve(nNames);
            for (Size iName = 0; iName < nNames; iName++)
            {
                names.pDef.push_back(
                    copula_->conditionalDefaultProbabilityInvP(invUncondProbs[iName], liveNames_[iName], mktFactor));
                names.lgd.push_back(
                    1. - copula_->conditionalRecoveryInvP(invUncondProbs[iName], liveNames_[iName], mktFactor));
            }
            return names;
        }
        const LossLimits& limitsOf(const NamesCond& names) const
        {
            if (!names.limited)
            {
                names.limits = lossLimitsCond(names);
                names.limited = true;
            }
            return names.limits;
        }
        //! the most big names a node is conditioned on, 2^maxBig tails a level
        static constexpr Size maxBig = 4;
        /*! Calls f(rest, w) for each outcome of the big names, the uncertain names that lose more than the two
            smallest losses together, with rest the names with theirs set to 1 or 0 and w its probability; returns
            false, calling nothing, where there are none or more than maxBig (#308). With more, the node keeps the
            approximation (#325): conditioning on some of them leaves the others' atoms in the rests' bands, and
            puts steps in the tail where the approximation has none.
        */
        template <class F>
        bool forBigOutcomes(const NamesCond& names, F&& f) const
        {
            const LossLimits& limits = limitsOf(names);
            if (limits.second == QL_MAX_REAL || !(limits.largest > limits.smallest + limits.second))
                return false;
            const Real e2 = limits.smallest + limits.second;
            std::array<Size, maxBig> big;
            Size nBig = 0;
            for (Size i = 0; i < names.pDef.size(); i++)
            {
                Real loss = remainingNotionals_[i] * names.lgd[i];
                if (uncertainName(names.pDef[i], loss) && loss > e2)
                {
                    if (nBig == maxBig)
                        return false;
                    big[nBig++] = i;
                }
            }
            NamesCond rest;
            rest.lgd = names.lgd;
            for (Size mask = 0; mask < (Size(1) << nBig); mask++)
            {
                rest.pDef = names.pDef;
                rest.limited = rest.joined = false;
                Real w = 1.;
                for (Size j = 0; j < nBig; j++)
                {
                    const Size i = big[j];
                    const bool defaults = ((mask >> j) & 1) != 0;
                    w *= defaults ? names.pDef[i] : 1. - names.pDef[i];
                    rest.pDef[i] = defaults ? 1. : 0.;
                }
                if (w > 0.)
                    f(static_cast<const NamesCond&>(rest), w);
            }
            return true;
        }
        //! whether a level lies between the exact regions of the names given the factor
        bool inBand(const NamesCond& names, Real loss) const
        {
            const LossLimits& limits = limitsOf(names);
            const Real rest = loss - limits.certain;
            return loss > QL_EPSILON && limits.second < QL_MAX_REAL && rest > limits.smallest + limits.second &&
                   rest <= limits.total - limits.smallest - limits.second;
        }
        Tail restTailCond(const NamesCond& names, Real loss, bool withExcess) const;
        Probability restDensityCond(const NamesCond& names, Real loss) const;
        /*! With withExcess false, only the probability is wanted: the excess between the exact regions is then
            the approximation's own, not held to their bounds, which saves computing them.
        */
        Tail tailCond(const std::vector<Real>& invUncondProbs,
                      Real loss,
                      const std::vector<Real>& mktFactor,
                      bool withExcess = true) const;
        Tail tailCond(const NamesCond& names, Real loss, bool withExcess = true) const;
        /*! The tail where a loss short of the level has at most one uncertain default, up to the two smallest
            losses of one together: there any two defaults reach the level, and so does one name that loses
            enough on its own, and every term of the distribution is known.
        */
        Tail oneDefaultTailCond(const NamesCond& names, Real loss, const LossLimits& limits) const;
        /*! The tail where a loss reaches the level only if at most one uncertain name survives, above their total
            less the two smallest losses of one: there the loss is the total less what the survivor would have
            lost, and every term of the distribution is known.
        */
        Tail oneSurvivorTailCond(const NamesCond& names, Real loss, const LossLimits& limits) const;

        //! Conditional on the market factor
        LossLimits lossLimitsCond(const std::vector<Real>& invUncondProbs, const std::vector<Real>& mktFactor) const
        {
            return lossLimitsCond(namesCond(invUncondProbs, mktFactor));
        }
        LossLimits lossLimitsCond(const NamesCond& names) const
        {
            LossLimits limits = {0., QL_MAX_REAL, QL_MAX_REAL, 0., 1., 0., 1., 0., 0., 0.};
            // The sums are compensated (Neumaier's): a plain sum of losses that aren't exact in double can come out
            //   doubles away from the real one, and move the limits by as much.
            Real certainCompensation = 0., totalCompensation = 0.;
            // 1 - none loses its digits where the names' probabilities are small, below 1e-16 all of them (#306), so
            //   the probability of some default is taken from the logarithm of none's.
            Real logNone = 0.;
            // the probability that some uncertain name among those so far defaults
            Probability some = 0.;
            auto add = [](Real& sum, Real& compensation, Real loss)
            {
                const Real next = sum + loss;
                compensation += std::abs(sum) >= std::abs(loss) ? (sum - next) + loss : (loss - next) + sum;
                sum = next;
            };
            for (Size iName = 0; iName < remainingNotionals_.size(); iName++)
            {
                Real loss = remainingNotionals_[iName] * names.lgd[iName];
                Probability p = names.pDef[iName];
                if (p == 1.)
                {
                    add(limits.certain, certainCompensation, loss);
                }
                else if (uncertainName(p, loss))
                {
                    // What the names so far lose beyond the smallest loss among them: the first of them to default
                    //   loses its loss less the smallest, and every later one its whole loss. Where a smaller loss
                    //   comes, every set of the names before with a default goes beyond it by that much more. No
                    //   term can be negative (#313).
                    if (loss < limits.smallest)
                    {
                        limits.beyond += (limits.smallest - loss) * some;
                        limits.second = limits.smallest;
                        limits.smallest = loss;
                    }
                    else if (loss < limits.second)
                        limits.second = loss;
                    limits.largest = std::max(limits.largest, loss);
                    limits.beyond += p * (loss * some + (loss - limits.smallest) * limits.none);
                    limits.mean += p * loss;
                    some += p * limits.none;
                    add(limits.total, totalCompensation, loss);
                    limits.none *= 1. - p;
                    logNone += std::log1p(-p);
                    limits.all *= p;
                }
            }
            limits.certain += certainCompensation;
            limits.total += totalCompensation;
            limits.any = -std::expm1(logNone);
            return limits;
        }

        /*! Calculates the mkt-fct-conditional saddle point for the loss level
            given and the probability passed.
            The date is implicitly given through the probability. Performance
            requires to pass the probabilities for that date. Otherwise once we
            integrate this over the market factor we would be computing the same
            probabilities over and over. While this works fine here some models
            of the recovery rate might require the date.

            The passed lossLevel is in total portfolio loss fractional units.

            \todo Improve convergence speed (which is bad at the moment).See
            discussion in several places; references above and The Oxford
            Handbook of CD, sect 2.9
        */
        Real findSaddle(const std::vector<Real>& invUncondProbs,
                        Real lossLevel,
                        const std::vector<Real>& mktFactor,
                        Real accuracy = 1.0e-3, // 1.e-4
                        Natural maxEvaluations = 50) const;
        Real
        findSaddle(const NamesCond& names, Real lossLevel, Real accuracy = 1.0e-3, Natural maxEvaluations = 50) const;

        class SaddlePercObjFunction
        {
            const SaddlePointLossModel& me_;
            Real targetValue_;
            Date date_;

          public:
            SaddlePercObjFunction(const SaddlePointLossModel& me, const Real target, const Date& date)
            : me_(me), targetValue_(1. - target), date_(date)
            {
            }
            /*! The passed x is the _tranche_ loss fraction */
            Real operator()(const Real x) const { return me_.probOverLoss(date_, x) - targetValue_; }
        };
        // Functionality, Provides various portfolio statistics---------------
      public:
        /*! Returns the loss amount at the requested date for which the
        probability of lossing that amount or less is equal to the value passed.
        */
        Real percentile(const Date& d, Probability percentile) const override;

      protected:
        /*! Conditional (on the mkt factor) prob of a loss fraction of
            the the tranched portfolio.

            The trancheLossFract parameter is the fraction over the
            tranche notional and must be in [0,1].
        */
        Probability probOverLossCond(const std::vector<Real>& invUncondProbs,
                                     Real trancheLossFract,
                                     const std::vector<Real>& mktFactor) const;
        Probability probOverLossPortfCond1stOrder(const std::vector<Real>& invUncondProbs,
                                                  Real loss,
                                                  const std::vector<Real>& mktFactor) const;

      public:
        Probability probOverLoss(const Date& d, Real trancheLossFract) const override;

        std::map<Real, Probability> lossDistribution(const Date& d) const override;

      protected:
        /*!
            Probability of having losses in the portfolio due to default
            events equal or larger than a given absolute loss value on a
            given date conditional to the latent model factor.
            The integral expression on the expansion is the first order
            integration as presented in several references, see for instance;
            equation 8 in R.Martin, K.Thompson, and C. Browne 's
            'Taking to the Saddle', Risk Magazine, June 2001, page 91

            The passed loss is in absolute value.
        */
        Probability probOverLossPortfCond(const std::vector<Real>& invUncondProbs,
                                          Real loss,
                                          const std::vector<Real>& mktFactor) const;

      public:
        Probability probOverPortfLoss(const Date& d, Real loss) const;
        Real expectedTrancheLoss(const Date& d) const override;

      protected:
        /*!
        Probability density of having losses in the total portfolio (untranched)
        due to default events equal to a given value on a given date conditional
        to the latent model factor.
        Based on the integrals of the expected shortfall.
        */
        Probability
        probDensityCond(const std::vector<Real>& invUncondProbs, Real loss, const std::vector<Real>& mktFactor) const;

      public:
        Probability probDensity(const Date& d, Real loss) const;

      protected:
        std::vector<Real>
        splitLossCond(const std::vector<Real>& invUncondProbs, Real loss, std::vector<Real> mktFactor) const;
        Real expectedShortfallFullPortfolioCond(const std::vector<Real>& invUncondProbs,
                                                Real lossPerc,
                                                const std::vector<Real>& mktFactor) const;
        /*! What the tranche loses beyond the given portfolio loss on
            average, conditional on the market factor: the conditional
            expected excess of the tranche's loss over the level's, with the
            level in absolute value.
        */
        Real expectedShortfallTrancheCond(const std::vector<Real>& invUncondProbs,
                                          Real lossLevel,
                                          const std::vector<Real>& mktFactor) const;
        std::vector<Real> expectedShortfallSplitCond(const std::vector<Real>& invUncondProbs,
                                                     Real lossPerc,
                                                     const std::vector<Real>& mktFactor) const;

      public:
        /*! Sensitivities of the individual names to a given portfolio loss
            value due to defaults. It returns ratios to the total structure
            notional, which aggregated add up to the requested loss value.
            Notice then that it refers to the total portfolio, not the tranched
            basket.
            \todo  Fix this.
            \par
            see equation 8 in <b>VAR: who contributes and how much?</b> by
            R.Martin, K.Thompson, and C. Browne in Risk Magazine, August 2001

            The passed loss is the loss amount level at which we want
            to request the sensitivity. Equivalent to a percentile.
        */
        std::vector<Real> splitVaRLevel(const Date& date, Real loss) const override;
        Real expectedShortfall(const Date& d, Probability percentile) const override;

      protected:
        // The most the tranche can lose by the date, given each live name's
        //   probability of defaulting by then: what the names that can default
        //   lose, less its attachment (#307).
        Real mostTrancheLoss(const std::vector<Probability>& uncondProbs) const;
        Real conditionalExpectedLoss(const std::vector<Real>& invUncondProbs, const std::vector<Real>& mktFactor) const;
        Real conditionalExpectedLoss(const NamesCond& names) const;
        Real conditionalExpectedTrancheLoss(const std::vector<Real>& invUncondProbs,
                                            const std::vector<Real>& mktFactor) const;

        void resetModel() override
        {
            remainingNotionals_ = basket_->remainingNotionals();
            liveNames_ = basket_->liveList();
            remainingNotional_ = basket_->remainingNotional();
            attachRatio_ = std::min(basket_->remainingAttachmentAmount() / basket_->remainingNotional(), 1.);
            detachRatio_ = std::min(basket_->remainingDetachmentAmount() / basket_->remainingNotional(), 1.);
            copula_->resetBasket(basket_.currentLink());
        }

        const ext::shared_ptr<ConstantLossLatentmodel<CP>> copula_;
        // cached todays arguments values
        mutable Size remainingSize_;
        mutable std::vector<Real> remainingNotionals_;
        // where each of those names is in the latent model, which has them all
        mutable std::vector<Size> liveNames_;
        mutable Real remainingNotional_;
        // remaining basket levels:
        mutable Real attachRatio_, detachRatio_;
        /*
        // Just for testing the ESF direct integration, not for release,
        //   this is very inneficient:
        class ESFIntegrator {
        public:
            ESFIntegrator(const SaddlePointLossModel& me,
                const Date& date,
                Real lossPercentileFract//,
                //const std::vector<Real>& mktFactor
                )
                : me_(me), date_(date),lossPercentileFract_(lossPercentileFract)
            {}


            Real operator()(Real x) const {
                return me_.densityTrancheLoss(date_, x + lossPercentileFract_)
                    * (x + lossPercentileFract_);
            }

            Real lossPercentileFract_;
            Date date_;
            //  const std::vector<Real>& mktFactor_;
            const SaddlePointLossModel& me_;
        };
        */
    };


    // -- Inlined integrations------------------------------------------------

    // Unconditional Moments and derivatives. --------------------------------
    template <class CP>
    inline Real SaddlePointLossModel<CP>::CumulantGenerating(const Date& date, Real s) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(date);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return CumulantGeneratingCond(invUncondProbs, s, v1); });
    }

    template <class CP>
    inline Real SaddlePointLossModel<CP>::CumGen1stDerivative(const Date& date, Real s) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(date);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return CumGen1stDerivativeCond(invUncondProbs, s, v1); });
    }

    template <class CP>
    inline Real SaddlePointLossModel<CP>::CumGen2ndDerivative(const Date& date, Real s) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(date);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return CumGen2ndDerivativeCond(invUncondProbs, s, v1); });
    }

    template <class CP>
    inline Real SaddlePointLossModel<CP>::CumGen3rdDerivative(const Date& date, Real s) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(date);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return CumGen3rdDerivativeCond(invUncondProbs, s, v1); });
    }

    template <class CP>
    inline Real SaddlePointLossModel<CP>::CumGen4thDerivative(const Date& date, Real s) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(date);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return CumGen4thDerivativeCond(invUncondProbs, s, v1); });
    }

    template <class CP>
    inline Probability SaddlePointLossModel<CP>::probOverLoss(const Date& d, Real trancheLossFract) const
    {
        // the fraction is of the tranche, which can't lose more than all of
        //   it; at 1 this is the probability that it loses all of it
        if (trancheLossFract > 1.)
            return 0.;

        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(d);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return probOverLossCond(invUncondProbs, trancheLossFract, v1); });
    }

    template <class CP>
    inline Probability SaddlePointLossModel<CP>::probOverPortfLoss(const Date& d, Real loss) const
    {
        std::vector<Probability> invUncondProbs = basket_->remainingProbabilities(d);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return probOverLossPortfCond(invUncondProbs, loss, v1); });
    }

    template <class CP>
    inline Real SaddlePointLossModel<CP>::expectedTrancheLoss(const Date& d) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(d);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return conditionalExpectedTrancheLoss(invUncondProbs, v1); });
    }

    template <class CP>
    inline Probability SaddlePointLossModel<CP>::probDensity(const Date& d, Real loss) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(d);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return probDensityCond(invUncondProbs, loss, v1); });
    }

    template <class CP>
    inline std::vector<Real> SaddlePointLossModel<CP>::splitVaRLevel(const Date& date, Real s) const
    {
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(date);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        return copula_->integratedExpectedValueV([&](const std::vector<Real>& v1)
                                                 { return splitLossCond(invUncondProbs, s, v1); });
    }


    /* ------------------------------------------------------------------------
                    Conditional Moments and derivatives.

        Notice that in all this methods the date dependence is implicitly
        present in the unconditional probabilities. But, as in other LMs, it
        is redundant and expensive to perform the call to the probabilities in
        these methods since they are integrands.
       ---------------------------------------------------------------------- */

    template <class CP>
    Real SaddlePointLossModel<CP>::CumulantGeneratingCond(const std::vector<Real>& invUncondProbs,
                                                          Real lossFraction,
                                                          const std::vector<Real>& mktFactor) const
    {
        const Size nNames = remainingNotionals_.size();
        Real sum = 0.;

        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer =
                copula_->conditionalDefaultProbabilityInvP(invUncondProbs[iName], liveNames_[iName], mktFactor);
            sum += std::log(1. - pBuffer +
                            pBuffer * std::exp(remainingNotionals_[iName] *
                                               (1. - copula_->conditionalRecoveryInvP(invUncondProbs[iName],
                                                                                      liveNames_[iName], mktFactor)) *
                                               lossFraction / remainingNotional_));
        }
        return sum;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::CumGen1stDerivativeCond(const std::vector<Real>& invUncondProbs,
                                                           Real saddle,
                                                           const std::vector<Real>& mktFactor) const
    {
        const Size nNames = remainingNotionals_.size();
        Real sum = 0.;

        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer =
                copula_->conditionalDefaultProbabilityInvP(invUncondProbs[iName], liveNames_[iName], mktFactor);
            // loss in fractional units
            Real lossInDef =
                remainingNotionals_[iName] *
                (1. - copula_->conditionalRecoveryInvP(invUncondProbs[iName], liveNames_[iName], mktFactor)) /
                remainingNotional_;
            Real midFactor = pBuffer * std::exp(lossInDef * saddle);
            sum += lossInDef * midFactor / (1. - pBuffer + midFactor);
        }
        return sum;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::CumGen2ndDerivativeCond(const std::vector<Real>& invUncondProbs,
                                                           Real saddle,
                                                           const std::vector<Real>& mktFactor) const
    {
        const Size nNames = remainingNotionals_.size();
        Real sum = 0.;

        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer =
                copula_->conditionalDefaultProbabilityInvP(invUncondProbs[iName], liveNames_[iName], mktFactor);
            // loss in fractional units
            Real lossInDef =
                remainingNotionals_[iName] *
                (1. - copula_->conditionalRecoveryInvP(invUncondProbs[iName], liveNames_[iName], mktFactor)) /
                remainingNotional_;
            Real midFactor = pBuffer * std::exp(lossInDef * saddle);
            Real denominator = 1. - pBuffer + midFactor;
            sum += lossInDef * lossInDef * midFactor / denominator - std::pow(lossInDef * midFactor / denominator, 2.);
        }
        return sum;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::CumGen3rdDerivativeCond(const std::vector<Real>& invUncondProbs,
                                                           Real saddle,
                                                           const std::vector<Real>& mktFactor) const
    {
        const Size nNames = remainingNotionals_.size();
        Real sum = 0.;

        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer =
                copula_->conditionalDefaultProbabilityInvP(invUncondProbs[iName], liveNames_[iName], mktFactor);
            Real lossInDef =
                remainingNotionals_[iName] *
                (1. - copula_->conditionalRecoveryInvP(invUncondProbs[iName], liveNames_[iName], mktFactor)) /
                remainingNotional_;

            const Real midFactor = pBuffer * std::exp(lossInDef * saddle);
            const Real denominator = 1. - pBuffer + midFactor;

            const Real& suma0 = denominator;
            const Real suma1 = lossInDef * midFactor;
            const Real suma2 = lossInDef * suma1;
            const Real suma3 = lossInDef * suma2;

            sum += (suma3 + (2. * std::pow(suma1, 3.) / suma0 - 3. * suma1 * suma2) / suma0) / suma0;
        }
        return sum;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::CumGen4thDerivativeCond(const std::vector<Real>& invUncondProbs,
                                                           Real saddle,
                                                           const std::vector<Real>& mktFactor) const
    {
        const Size nNames = remainingNotionals_.size();
        Real sum = 0.;

        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer =
                copula_->conditionalDefaultProbabilityInvP(invUncondProbs[iName], liveNames_[iName], mktFactor);
            Real lossInDef =
                remainingNotionals_[iName] *
                (1. - copula_->conditionalRecoveryInvP(invUncondProbs[iName], liveNames_[iName], mktFactor)) /
                remainingNotional_;

            Real midFactor = pBuffer * std::exp(lossInDef * saddle);
            Real denominator = 1. - pBuffer + midFactor;

            const Real& suma0 = denominator;
            const Real suma1 = lossInDef * midFactor;
            const Real suma2 = lossInDef * suma1;
            const Real suma3 = lossInDef * suma2;
            const Real suma4 = lossInDef * suma3;

            sum += (suma4 + (-4. * suma1 * suma3 - 3. * suma2 * suma2 +
                             (12. * suma1 * suma1 * suma2 - 6. * std::pow(suma1, 4.) / suma0) / suma0) /
                                suma0) /
                   suma0;
        }
        return sum;
    }

    template <class CP>
    std::tuple<Real, Real, Real, Real> SaddlePointLossModel<CP>::CumGen0234DerivCond(
        const std::vector<Real>& invUncondProbs, Real saddle, const std::vector<Real>& mktFactor) const
    {
        return CumGen0234DerivCond(namesCond(invUncondProbs, mktFactor), saddle);
    }

    template <class CP>
    std::tuple<Real, Real, Real, Real> SaddlePointLossModel<CP>::CumGen0234DerivCond(const NamesCond& names,
                                                                                     Real saddle) const
    {
        // Each name's terms are a Bernoulli variable's under the tilted measure (detail::tiltedName), as for the
        //   tail: l^2 q t, l^3 q t (t - q) and l^4 q t (1 - 6 q t). Taken from powers of p e^{l s} over
        //   1 - p + p e^{l s} they overflowed far out in the tilt, and lost their digits where a name's tilted
        //   probability is near 1 (#308).
        const Size nNames = remainingNotionals_.size();
        Real deriv0 = 0., deriv2 = 0., deriv3 = 0., deriv4 = 0.;
        for (Size iName = 0; iName < nNames; iName++)
        {
            Real lossInDef = remainingNotionals_[iName] * names.lgd[iName] / remainingNotional_;
            Real logTerm, tiltedDefault, tiltedSurvival;
            detail::tiltedName(names.pDef[iName], lossInDef * saddle, logTerm, tiltedDefault, tiltedSurvival);
            const Real qt = tiltedDefault * tiltedSurvival, l2 = lossInDef * lossInDef;
            deriv0 += logTerm;
            deriv2 += l2 * qt;
            deriv3 += l2 * lossInDef * qt * (tiltedSurvival - tiltedDefault);
            deriv4 += l2 * l2 * qt * (1. - 6. * qt);
        }
        return {deriv0, deriv2, deriv3, deriv4};
    }

    template <class CP>
    std::tuple<Real, Real> SaddlePointLossModel<CP>::CumGen02DerivCond(const std::vector<Real>& invUncondProbs,
                                                                       Real saddle,
                                                                       const std::vector<Real>& mktFactor) const
    {
        return CumGen02DerivCond(namesCond(invUncondProbs, mktFactor), saddle);
    }

    template <class CP>
    std::tuple<Real, Real> SaddlePointLossModel<CP>::CumGen02DerivCond(const NamesCond& names, Real saddle) const
    {
        // Each name's terms are a Bernoulli variable's under the tilted measure (detail::tiltedName): l^2 q t
        //   in K''. The sums of powers of p e^{l s} they replace grow beyond any double far above the expected
        //   loss, and l^2 q less (l q)^2 loses q t to cancellation where q is close to 1, to below 0 at times.
        const Size nNames = remainingNotionals_.size();
        Real deriv0 = 0., deriv2 = 0.;
        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer = names.pDef[iName];
            Real lossInDef = remainingNotionals_[iName] * names.lgd[iName] / remainingNotional_;
            Real logTerm, tiltedDefault, tiltedSurvival;
            detail::tiltedName(pBuffer, lossInDef * saddle, logTerm, tiltedDefault, tiltedSurvival);
            deriv0 += logTerm;
            deriv2 += lossInDef * lossInDef * tiltedDefault * tiltedSurvival;
        }
        return {deriv0, deriv2};
    }

    // ----- Saddle point search ----------------------------------------------

    template <class CP>
    Real SaddlePointLossModel<CP>::findSaddle(const std::vector<Real>& invUncondPs,
                                              Real lossLevel, // in total portfolio loss fractional unit
                                              const std::vector<Real>& mktFactor,
                                              Real accuracy,
                                              Natural maxEvaluations) const
    {
        return findSaddle(namesCond(invUncondPs, mktFactor), lossLevel, accuracy, maxEvaluations);
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::findSaddle(const NamesCond& names,
                                              Real lossLevel, // in total portfolio loss fractional unit
                                              Real accuracy,
                                              Natural maxEvaluations) const
    {
        // K'(s) rises with s, and K'(0) is the expected loss, so a bracket
        //   widened from s = 0 until K' crosses the level holds the saddle
        //   point. Past |s| = 700 over the largest loss of a name the
        //   exponentials in K overflow. A level K' hasn't crossed by then is
        //   above every loss the factor makes possible, and QL_MAX_REAL is
        //   returned, or below the losses it makes certain, and -QL_MAX_REAL.
        Real largestLoss = 0.;
        for (Size iName = 0; iName < remainingNotionals_.size(); iName++)
            largestLoss = std::max(largestLoss, remainingNotionals_[iName] * names.lgd[iName] / remainingNotional_);
        if (largestLoss == 0.)
            return QL_MAX_REAL;
        SaddleObjectiveFunction f(*this, lossLevel, names);
        const Real widest = 700. / largestLoss;
        Real lower = 0., upper = 0., step = 1.;
        if (f(0.) < 0.)
        {
            do
            {
                if (upper == widest)
                    return QL_MAX_REAL;
                lower = upper;
                upper = std::min(upper + step, widest);
                step *= 2.;
            } while (f(upper) < 0.);
        }
        else
        {
            do
            {
                if (lower == -widest)
                    return -QL_MAX_REAL;
                upper = lower;
                lower = std::max(lower - step, -widest);
                step *= 2.;
            } while (f(lower) > 0.);
        }

        Brent solverBrent;
        solverBrent.setMaxEvaluations(maxEvaluations);
        return solverBrent.solve(f, accuracy, (lower + upper) / 2., lower, upper);
    }


    // ----- Statistics -------------------------------------------------------


    template <class CP>
    Real SaddlePointLossModel<CP>::mostTrancheLoss(const std::vector<Probability>& uncondProbs) const
    {
        // the losses added and the attachment subtracted as percentile's
        //   atoms are, each a rounded product first, so that at the top this
        //   is the atom less the attachment to the bit
        const std::vector<Real>& recoveries = copula_->recoveries();
        Real total = 0.;
        for (Size i = 0; i < uncondProbs.size(); i++)
        {
            Real loss = remainingNotionals_[i] * (1. - recoveries[liveNames_[i]]);
            if (loss > 0. && uncondProbs[i] > 0.)
                total += loss;
        }
        Real attachAmount = attachRatio_ * remainingNotional_;
        if (total <= attachAmount)
            return 0.;
        return std::min(total - attachAmount, (detachRatio_ - attachRatio_) * remainingNotional_);
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::percentile(const Date& d, Probability percentile) const
    {
        // this test should be in the calling basket...?
        QL_REQUIRE(percentile >= 0. && percentile <= 1., "Incorrect percentile value.");

        // still this does not tackle the situation where we have cumulated
        //   losses from previous defaults:
        if (d <= Settings::instance().evaluationDate())
            return 0.;

        // Trivial cases when the percentile is outside the prob range
        //   associated to the tranche limits. The lower one is the
        //   probability of any loss, taken at the least fraction the solver
        //   below takes; at 0 it would be 1.
        if (percentile <= 1. - probOverLoss(d, QL_EPSILON))
            return 0.;
        // the fraction solved for is of the tranche between the remaining
        //   ratios, whose detachment is no more than the remaining notional
        Real trancheAmount = (detachRatio_ - attachRatio_) * remainingNotional_;
        Real attachAmount = attachRatio_ * remainingNotional_;
        // What the names that can default by the date lose, in all and the
        //   least of them; a name that can't adds nothing to the most the
        //   pool can lose
        Real total = 0., smallest = QL_MAX_REAL;
        const std::vector<Real>& recoveries = copula_->recoveries();
        const std::vector<Probability> uncondProbs = basket_->remainingProbabilities(d);
        for (Size i = 0; i < remainingNotionals_.size(); i++)
        {
            Real loss = remainingNotionals_[i] * (1. - recoveries[liveNames_[i]]);
            if (loss > 0. && uncondProbs[i] > 0.)
            {
                total += loss;
                smallest = std::min(smallest, loss);
            }
        }
        // Where the pool can't lose the tranche's detachment, the most the
        //   tranche loses is the total less its attachment, reached only if
        //   every name that can default does: at a probability of 1 that is
        //   the percentile, and below it P(L >= y) for any y above the total
        //   less the smallest loss is the probability of reaching it, where
        //   y lies in the exact region at every node. No percentile is above
        //   it.
        Real most = mostTrancheLoss(uncondProbs);
        if (most < trancheAmount)
        {
            if (percentile >= 1.)
                return most;
            if (smallest < QL_MAX_REAL)
            {
                Real top = total - smallest / 2.;
                if (percentile >= 1. - probOverPortfLoss(d, top))
                    return most;
            }
        }
        else if (percentile >= 1. || percentile >= 1. - probOverLoss(d, 1.))
            return trancheAmount;

        SaddlePercObjFunction f(*this, percentile, d);
        Brent solver;
        solver.setMaxEvaluations(100);
        Real minVal = QL_EPSILON;

        Real maxVal = 1. - QL_EPSILON;
        Real guess = 0.5;

        Real solut = solver.solve(f, 1.e-4, guess, minVal, maxVal);

        // Where P jumps across 1 - p, at an atom, the solver stops on either
        //   side of it within its accuracy. The atom is the percentile if the
        //   probability of reaching just below it is above 1 - p, and of
        //   reaching just above it no more (#307). At a node of the factor
        //   the exact regions put atoms at what the names certain to default
        //   there lose, c, at c plus the total T of the uncertain names, and
        //   with two of those or more at c plus each of their losses up to
        //   their two smallest together, plus those two, and plus T less each
        //   of these (#317). One pass over the nodes takes the atoms within
        //   the solver's reach, an atom within `step` of one taken counting
        //   as it, but not one to which every node that puts it there gives a
        //   probability below QL_EPSILON (1 - p) over the number of names:
        //   its jump in P is then within P's rounding at 1 - p, so the check
        //   could pass at it only where P's continuous part crosses 1 - p
        //   within `step` of it, and there the solver's level is as good as
        //   at any other. On many names nodes far out can put tens of such
        //   atoms within reach, each costing P up to twice to check. The
        //   probabilities and that floor are taken as logarithms, the former
        //   from the names'; the two smallest losses together, and the total
        //   less them, which pairs of names that lose alike can share, count
        //   at every node. The pass integrates 0, so it visits every node of
        //   the fixed rules, on which P is integrated too, but of the
        //   Trapezoid type's only the 65 a factor where it first stops
        //   (#320). Where every name that can default by the date might or
        //   must at the node, c + T is the total, which isn't tried: where
        //   1 - p lies in its jump, the checks above have returned. Where the
        //   tail at a node is taken given the outcomes of the names that lose
        //   more than the two smallest together, so are its atoms, each
        //   outcome's probability with its own (#308).
        Real level = attachAmount + trancheAmount * solut;
        Real reach = 2.e-4 * trancheAmount;
        Real step = 1.e-10 * remainingNotional_;
        std::vector<Real> atoms;
        // the least probability, as a logarithm, at which an atom is tried
        Real logFloor = std::log(QL_EPSILON * (1. - percentile)) - std::log(Real(remainingNotionals_.size()));
        std::vector<Real> invUncondProbs(uncondProbs);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);
        copula_->integratedExpectedValue(
            [&](const std::vector<Real>& v1)
            {
                const NamesCond node = namesCond(invUncondProbs, v1);
                auto collect = [&](const NamesCond& names, Real logWeight)
                {
                    const LossLimits& limits = limitsOf(names);
                    // the logarithms of the probabilities that no uncertain name
                    //   defaults and that every one does, taken once an atom
                    //   within reach needs them
                    Real logNone = 0., logAll = 0.;
                    bool logged = false;
                    auto logs = [&]()
                    {
                        if (!logged)
                        {
                            for (Size i = 0; i < names.pDef.size(); i++)
                                if (uncertainName(names.pDef[i], remainingNotionals_[i] * names.lgd[i]))
                                {
                                    logNone += std::log1p(-names.pDef[i]);
                                    logAll += std::log(names.pDef[i]);
                                }
                            logNone += logWeight;
                            logAll += logWeight;
                            logged = true;
                        }
                    };
                    auto take = [&](Real z, const auto& logMass)
                    {
                        if (std::fabs(z - level) <= reach && z >= attachAmount && z <= attachAmount + trancheAmount &&
                            std::fabs(z - total) > step &&
                            std::none_of(atoms.begin(), atoms.end(),
                                         [&](Real a) { return std::fabs(a - z) <= step; }) &&
                            logMass() >= logFloor)
                            atoms.push_back(z);
                    };
                    take(limits.certain,
                         [&]()
                         {
                             logs();
                             return logNone;
                         });
                    take(limits.certain + limits.total,
                         [&]()
                         {
                             logs();
                             return logAll;
                         });
                    if (limits.second < QL_MAX_REAL)
                    {
                        Real e2 = limits.smallest + limits.second;
                        for (Size i = 0; i < names.pDef.size(); i++)
                        {
                            Real loss = remainingNotionals_[i] * names.lgd[i];
                            Probability p = names.pDef[i];
                            if (uncertainName(p, loss) && loss <= e2)
                            {
                                // only this name defaults, and all but this one
                                take(limits.certain + loss,
                                     [&]()
                                     {
                                         logs();
                                         return std::log(p) + logNone - std::log1p(-p);
                                     });
                                take(limits.certain + limits.total - loss,
                                     [&]()
                                     {
                                         logs();
                                         return std::log1p(-p) + logAll - std::log(p);
                                     });
                            }
                        }
                        take(limits.certain + e2, [&]() { return logWeight; });
                        take(limits.certain + limits.total - e2, [&]() { return logWeight; });
                    }
                };
                if (!forBigOutcomes(node, [&](const NamesCond& rest, Real w) { collect(rest, std::log(w)); }))
                    collect(node, 0.);
                return 0.;
            });
        std::sort(atoms.begin(), atoms.end(),
                  [level](Real x, Real y) { return std::fabs(x - level) < std::fabs(y - level); });
        for (Real z : atoms)
            if (probOverPortfLoss(d, z - step) > 1. - percentile && probOverPortfLoss(d, z + step) <= 1. - percentile)
                return std::min(z - attachAmount, most);
        return std::min(trancheAmount * solut, most);
    }

    template <class CP>
    Probability SaddlePointLossModel<CP>::probOverLossCond(const std::vector<Real>& invUncondPs,
                                                           Real trancheLossFract,
                                                           const std::vector<Real>& mktFactor) const
    {
        Real portfFract = attachRatio_ + trancheLossFract * (detachRatio_ - attachRatio_); // these are remaining ratios

        // for non-equity losses add the probability jump at zero tranche
        //   losses (since this method returns prob of losing more or
        //   equal to)
        ////////////////---       if(trancheLossFract <= QL_EPSILON) return 1.;
        return probOverLossPortfCond(invUncondPs, portfFract * remainingNotional_, mktFactor);
    }

    template <class CP>
    std::map<Real, Probability> SaddlePointLossModel<CP>::lossDistribution(const Date& d) const
    {
        std::map<Real, Probability> distrib;
        static constexpr double numPts = 500.;
        for (Real lossFraction = 1. / numPts; lossFraction < 0.45; lossFraction += 1. / numPts)
            distrib.insert(std::make_pair<Real, Probability>(
                lossFraction * remainingNotional_, 1. - probOverPortfLoss(d, lossFraction * remainingNotional_)));
        return distrib;
    }

    /*  NOTICE THIS IS ON THE TOTAL PORTFOLIO ---- UNTRANCHED..............
        Probability of having losses in the portfolio due to default
        events equal or larger than a given absolute loss value on a
        given date conditional to the latent model factor.
        The integral expression on the expansion is the first order
        integration as presented in several references, see for instance;
        equation 8 in R.Martin, K.Thompson, and C. Browne 's
        'Taking to the Saddle', Risk Magazine, June 2001, page 91

        The loss is passed in absolute value.
    */
    template <class CP>
    Probability SaddlePointLossModel<CP>::probOverLossPortfCond(const std::vector<Real>& invUncondProbs,
                                                                Real loss,
                                                                const std::vector<Real>& mktFactor) const
    {
        return tailCond(invUncondProbs, loss, mktFactor, false).over;
    }

    template <class CP>
    typename SaddlePointLossModel<CP>::Tail SaddlePointLossModel<CP>::tailCond(const std::vector<Real>& invUncondProbs,
                                                                               Real loss,
                                                                               const std::vector<Real>& mktFactor,
                                                                               bool withExcess) const
    {
        return tailCond(namesCond(invUncondProbs, mktFactor), loss, withExcess);
    }

    template <class CP>
    typename SaddlePointLossModel<CP>::Tail
    SaddlePointLossModel<CP>::tailCond(const NamesCond& names, Real loss, bool withExcess) const
    {
        // Between the exact regions a name that loses more than the two
        //   smallest together has atoms, its loss and the total less it;
        //   given each outcome of such names the others' distribution has
        //   exact regions of its own (#308).
        Tail sum = {0., 0.};
        if (inBand(names, loss) && forBigOutcomes(names,
                                                  [&](const NamesCond& rest, Real w)
                                                  {
                                                      const Tail t = restTailCond(rest, loss, withExcess);
                                                      sum.over += w * t.over;
                                                      sum.excess += w * t.excess;
                                                  }))
            return sum;
        return restTailCond(names, loss, withExcess);
    }

    template <class CP>
    typename SaddlePointLossModel<CP>::Tail
    SaddlePointLossModel<CP>::restTailCond(const NamesCond& names, Real loss, bool withExcess) const
    {
        // The expected loss given the factor is a pass over the names, and
        //   only this return and the approximation's two below it use it, so
        //   it is computed there alone (#314).
        if (loss <= QL_EPSILON)
            return {1., conditionalExpectedLoss(names) - loss};

        // The names certain to default reach their loss for sure, and the
        //   uncertain names must make up the rest. Every set of defaults of
        //   theirs short of all of them loses at most their total less the
        //   smallest loss of one, and any default at least that smallest
        //   loss. So above the first the rest is reached only if every
        //   uncertain name defaults, and up to the second if any does. A
        //   level within rounding of the certain loss is then never searched
        //   for: K' only tends to it, and the saddle point is at -infinity.
        const LossLimits& limits = limitsOf(names);
        Real rest = loss - limits.certain;
        // at or below the certain loss every loss reaches the level, by what
        //   the uncertain names lose and the certain loss less the level
        if (rest <= 0.)
            return {1., limits.mean - rest};
        if (rest > limits.total)
            return {0., 0.};
        if (rest > limits.total - limits.smallest)
            return {limits.all, (limits.certain + limits.total - loss) * limits.all};
        // Below the first uncertain default, every loss but the certain one
        //   alone reaches the level, by what the defaults lose beyond the
        //   smallest loss and the smallest less the rest: parts that can't be
        //   negative, where E[U] - rest P(any default) kept no digits of its
        //   own near the smallest loss (#313).
        if (rest <= limits.smallest)
            return {limits.any, limits.beyond + (limits.smallest - rest) * limits.any};
        // Every set of defaults that leaves out two names or more loses at
        //   most the total less the two smallest losses, so above that the
        //   rest is reached only if at most one name survives (#282).
        if (rest > limits.total - limits.smallest - limits.second)
            return oneSurvivorTailCond(names, loss, limits);
        // Up to the two smallest losses together, a loss short of the level
        //   has at most one uncertain default (#205).
        if (rest <= limits.smallest + limits.second)
            return oneDefaultTailCond(names, loss, limits);

        /* In between, the tail comes from the approximation. From the level
           where the lower exact region ends to the one where the upper starts,
           the exact excess falls at P(L > t), which lies between the
           probabilities the two regions give there; so it lies between the
           lines through their excesses with those two slopes. The
           approximation's excess is held between those lines, so that it
           meets both regions: until #205 it stepped where it met them, and
           the expected shortfall stepped with it. The upper region counts at
           most one survivor, so its probability there is at most
           P(L >= level), which still bounds P(L > t) below it. A call for the
           probability alone skips the bounds. */
        Real lowest = -QL_MAX_REAL, highest = QL_MAX_REAL;
        if (withExcess)
        {
            Real lowJoin = limits.certain + limits.smallest + limits.second;
            Real highJoin = limits.certain + limits.total - limits.smallest - limits.second;
            if (!names.joined)
            {
                names.low = oneDefaultTailCond(names, lowJoin, limits);
                names.high = oneSurvivorTailCond(names, highJoin, limits);
                names.joined = true;
            }
            const Tail& low = names.low;
            const Tail& high = names.high;
            lowest = std::max(low.excess - (loss - lowJoin) * low.over, high.excess + (highJoin - loss) * high.over);
            highest = std::min(low.excess - (loss - lowJoin) * high.over, high.excess + (highJoin - loss) * low.over);
        }
        auto held = [lowest, highest](Tail t)
        {
            t.excess = std::min(std::max(t.excess, lowest), highest);
            return t;
        };

        Real saddlePt = findSaddle(names, loss / remainingNotional_);
        if (saddlePt == QL_MAX_REAL)
            return held({0., 0.});
        if (saddlePt == -QL_MAX_REAL)
            return held({1., conditionalExpectedLoss(names) - loss});

        /* Above the expected loss, the probability is the first-order form
           the documentation of probOverLossPortfCond cites, and the excess
           comes from the same tilted normal approximation
           (detail::saddlePointTailIntegrals); below it, their mirror images:
           1 less the tail below, and the expected loss less the level plus
           the excess below. Until #204 the probability was multiplied by
           second-order terms, a series in the saddle point that grows
           without bound with it: on 20 names that each lose 60, at a loss of
           1,000 and a factor of 0, by about 1,140, where the first-order form
           is within 10% of the exact value. The excess, from Martin's formula
           on that and on the density, came out negative there. Here the
           probability stays in [0, 1], the excess at least
           (expected - level)^+, both are continuous through the expected
           loss, where they are 1/2 and sqrt(K''/(2 pi)), and e^{K - s y} is
           at most 1. */
        std::tuple<Real, Real> cumulants = CumGen02DerivCond(names, saddlePt);
        Real scale = std::exp(std::get<0>(cumulants) - saddlePt * loss / remainingNotional_);
        Real deviation = std::sqrt(std::get<1>(cumulants));
        Real tail, excess;
        detail::saddlePointTailIntegrals(std::abs(saddlePt) * deviation, tail, excess);
        Real beyond = scale * deviation * excess * remainingNotional_;
        if (saddlePt >= 0.)
            return held({scale * tail, beyond});
        return held({1. - scale * tail, conditionalExpectedLoss(names) - loss + beyond});
    }

    template <class CP>
    typename SaddlePointLossModel<CP>::Tail
    SaddlePointLossModel<CP>::oneDefaultTailCond(const NamesCond& names, Real loss, const LossLimits& limits) const
    {
        // the uncertain names, as lossLimitsCond takes them
        std::vector<Probability> pDef;
        std::vector<Real> lossInDef;
        for (Size iName = 0; iName < remainingNotionals_.size(); iName++)
        {
            Real nameLoss = remainingNotionals_[iName] * names.lgd[iName];
            Probability p = names.pDef[iName];
            if (uncertainName(p, nameLoss))
            {
                pDef.push_back(p);
                lossInDef.push_back(nameLoss);
            }
        }
        /* With N the number of uncertain defaults and rest the level less the
           certain loss, any two defaults reach the rest, and one alone if its
           loss does:
             P(L >= y) = P(N >= 2) + sum over l_i >= rest of P(only i),
             E[(L - y)^+] = E[(U - rest) 1{N >= 2}]
                            + sum over l_i > rest of (l_i - rest) P(only i),
           with U what the uncertain names lose. Each "some other name
           defaults" comes from expm1 of the sums of log-survivals before and
           after the name, so that it keeps its digits where the names'
           probabilities are small (#306). Until #311 the first term of the
           excess was E[U 1{N >= 2}] - rest P(N >= 2), a difference that kept no
           digits of its own where three defaults are far rarer than two and
           most pairs lose about the rest, and could come out below 0. With s1
           and s2 the two smallest losses, it is now
             E[(U - s1 - s2) 1{N >= 2}] + (s1 + s2 - rest) P(N >= 2),
           and neither term can be negative. The first comes from one pass
           over the names, which takes s1 and s2 off the losses of the first
           two defaults, one each, and adds every later default's whole loss:
           the name that loses s1 takes s1 where it is one of the two, so that
           neither difference is below 0. In the second, s1 + s2 - rest is
           formed as (s2 - rest) + s1, exact wherever it is at most s1 / 2.
           Where rounding puts the rest above s1 + s2 it is held at 0, which
           is exact for the pair that loses s1 and s2, as it then falls short
           of the rest, and off by no more than that rounding for any other
           set of defaults. */
        const Size nNames = pDef.size();
        std::vector<Real> logAfter(nNames + 1, 0.);
        for (Size i = nNames; i-- > 0;)
            logAfter[i] = logAfter[i + 1] + std::log1p(-pDef[i]);
        Size cheapest = 0;
        Real s1 = QL_MAX_REAL, s2 = QL_MAX_REAL;
        for (Size i = 0; i < nNames; i++)
        {
            if (lossInDef[i] < s1)
            {
                s2 = s1;
                s1 = lossInDef[i];
                cheapest = i;
            }
            else if (lossInDef[i] < s2)
                s2 = lossInDef[i];
        }
        Real rest = loss - limits.certain;
        Real logBefore = 0., twoOrMore = 0., single = 0., singleExcess = 0.;
        for (Size i = 0; i < nNames; i++)
        {
            Probability onlyThis = pDef[i] * std::exp(logBefore + logAfter[i + 1]);
            twoOrMore += std::exp(logBefore) * pDef[i] * -std::expm1(logAfter[i + 1]);
            if (lossInDef[i] >= rest)
                single += onlyThis;
            if (lossInDef[i] > rest)
                singleExcess += (lossInDef[i] - rest) * onlyThis;
            logBefore += std::log1p(-pDef[i]);
        }
        // among the names before i: the probabilities that none, one, two,
        //   and three or more default, E[(U - s1 - s2) 1{N = 2}] and
        //   E[(U - s1 - s2) 1{N >= 3}], and to build them E[(l - s1) 1{N = 1}]
        //   and, before the name that loses s1, E[(l - s2) 1{N = 1}]
        Real none = 1., one = 0., two = 0., three = 0.;
        Real oneBeyond1 = 0., oneBeyond2 = 0., twoBeyond = 0., threeBeyond = 0.;
        for (Size i = 0; i < nNames; i++)
        {
            const Probability p = pDef[i], q = 1. - p;
            const Real l = lossInDef[i];
            threeBeyond += p * (twoBeyond + l * (two + three));
            three += p * two;
            twoBeyond = q * twoBeyond + p * (i == cheapest ? oneBeyond2 : oneBeyond1 + (l - s2) * one);
            two = q * two + p * one;
            oneBeyond1 = q * oneBeyond1 + p * (l - s1) * none;
            if (i < cheapest)
                oneBeyond2 = q * oneBeyond2 + p * (l - s2) * none;
            one = q * one + p * none;
            none *= q;
        }
        Real twoOrMoreExcess = twoBeyond + threeBeyond;
        // skipped where no two names default: with no uncertain name s1 and
        //   s2 would still be QL_MAX_REAL, and the gap infinite
        if (two + three > 0.)
            twoOrMoreExcess += std::max((s2 - rest) + s1, 0.) * (two + three);
        return {twoOrMore + single, twoOrMoreExcess + singleExcess};
    }

    template <class CP>
    typename SaddlePointLossModel<CP>::Tail
    SaddlePointLossModel<CP>::oneSurvivorTailCond(const NamesCond& names, Real loss, const LossLimits& limits) const
    {
        // the uncertain names, as lossLimitsCond takes them
        std::vector<Probability> pDef;
        std::vector<Real> lossInDef;
        for (Size iName = 0; iName < remainingNotionals_.size(); iName++)
        {
            Real nameLoss = remainingNotionals_[iName] * names.lgd[iName];
            Probability p = names.pDef[iName];
            if (uncertainName(p, nameLoss))
            {
                pDef.push_back(p);
                lossInDef.push_back(nameLoss);
            }
        }
        // Only name i surviving loses the total less its loss, which reaches
        //   the level if that loss is at most the total less the rest. Its
        //   probability takes the product of the other names' before and
        //   after it, rather than the whole product over p_i, so that names
        //   whose probabilities are 0 don't take the others' terms with them.
        const Size nNames = pDef.size();
        std::vector<Probability> after(nNames + 1, 1.);
        for (Size i = nNames; i-- > 0;)
            after[i] = after[i + 1] * pDef[i];
        Real top = limits.certain + limits.total;
        Real room = limits.total - (loss - limits.certain);
        Tail tail = {limits.all, (top - loss) * limits.all};
        Probability before = 1.;
        for (Size i = 0; i < nNames; i++)
        {
            if (lossInDef[i] <= room)
            {
                Probability onlyThisSurvives = (1. - pDef[i]) * before * after[i + 1];
                tail.over += onlyThisSurvives;
                tail.excess += (top - lossInDef[i] - loss) * onlyThisSurvives;
            }
            before *= pDef[i];
        }
        return tail;
    }

    template <class CP>
    Probability SaddlePointLossModel<CP>::probOverLossPortfCond1stOrder(const std::vector<Real>& invUncondPs,
                                                                        Real loss,
                                                                        const std::vector<Real>& mktFactor) const
    {
        // The first-order form is the one probOverLossPortfCond takes. This kept a copy of its own, which cut the
        //   probability to 0 wherever its exponent passed 700 (#281).
        return tailCond(invUncondPs, loss, mktFactor, false).over;
    }


    /*!   NOTICE THIS IS ON THE TOTAL PORTFOLIO ---- UNTRANCHED
    Probability density of having losses in the portfolio due to default
        events equal to a given value on a given date conditional to the w
        latent model factor.
        Based on the integrals of the expected shortfall. See......refernce.
    */
    template <class CP>
    Probability SaddlePointLossModel<CP>::probDensityCond(const std::vector<Real>& invUncondPs,
                                                          Real loss,
                                                          const std::vector<Real>& mktFactor) const
    {
        if (loss <= QL_EPSILON)
            return 0.;

        NamesCond names = namesCond(invUncondPs, mktFactor);
        Probability sum = 0.;
        if (inBand(names, loss) &&
            forBigOutcomes(names, [&](const NamesCond& rest, Real w) { sum += w * restDensityCond(rest, loss); }))
            return sum;
        return restDensityCond(names, loss);
    }

    template <class CP>
    Probability SaddlePointLossModel<CP>::restDensityCond(const NamesCond& names, Real loss) const
    {
        Real relativeLoss = loss / remainingNotional_;
        // No set of defaults of the uncertain names loses more than nothing
        //   but less than the smallest loss of one, nor more than their total
        //   less that loss but less than their total; the names certain to
        //   default add their loss to all of them.
        const LossLimits& limits = limitsOf(names);
        Real rest = loss - limits.certain;
        if (rest < limits.smallest || rest > limits.total - limits.smallest)
            return 0.;
        // Above their total less the two smallest losses, the loss is their
        //   total, or the total less the loss of the one name that survives:
        //   atoms, with nothing between them.
        if (rest > limits.total - limits.smallest - limits.second)
            return 0.;
        // Up to the two smallest losses together, the loss is nothing, the
        //   loss of one name, or those two together (#205): atoms again,
        //   the smallest loss itself among them.
        if (rest <= limits.smallest + limits.second)
            return 0.;
        Real saddlePt = findSaddle(names, relativeLoss);
        if (std::abs(saddlePt) == QL_MAX_REAL)
            return 0.;

        std::tuple<Real, Real, Real, Real> cumulants = CumGen0234DerivCond(names, saddlePt);
        /// access them directly rather than through this copy
        Real K0Saddle = std::get<0>(cumulants);
        Real K2Saddle = std::get<1>(cumulants);
        Real K3Saddle = std::get<2>(cumulants);
        Real K4Saddle = std::get<3>(cumulants);
        /* see, for instance R.Martin "he saddle point method and portfolio
        optionalities." in Risk December 2006 p.93 */
        Real leading = std::exp(K0Saddle - saddlePt * relativeLoss) / (std::sqrt(2. * M_PI * K2Saddle));
        Real factor = 1. + K4Saddle / (8. * K2Saddle * K2Saddle) -
                      5. * K3Saddle * K3Saddle / (24. * K2Saddle * K2Saddle * K2Saddle);
        // The second-order factor only where its correction is no larger
        //   than the leading term, else the leading term alone (#308): where
        //   one name decides the level the factor is negative, and where the
        //   tilted distribution closes on an atom it grows like 1/K2.
        return std::abs(factor - 1.) <= 1. ? factor * leading : leading;
    }

    /*    NOTICE THIS IS ON THE TOTAL PORTFOLIO ---- UNTRANCHED..
        Sensitivities of the individual names to a given portfolio loss value
        due to defaults. It returns ratios to the total structure notional,
        which aggregated add up to the requested loss value.

    see equation 8 in 'VAR: who contributes and how much?' by R.Martin,
    K.Thompson, and C. Browne in Risk Magazine, August 2001

    The passed loss is the loss amount level at which we want to
    request the sensitivity.  Equivalent to a percentile.
    */
    template <class CP>
    std::vector<Real> SaddlePointLossModel<CP>::splitLossCond(const std::vector<Real>& invUncondProbs,
                                                              Real loss,
                                                              std::vector<Real> mktFactor) const
    {
        const Size nNames = remainingNotionals_.size();
        std::vector<Real> condContrib(nNames, 0.);
        if (loss <= QL_EPSILON)
            return condContrib;

        NamesCond names = namesCond(invUncondProbs, mktFactor);
        Real saddlePt = findSaddle(names, loss / remainingNotional_);

        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer = names.pDef[iName];
            Real lossInDef = remainingNotionals_[iName] * names.lgd[iName];
            // At a level above every loss the factor makes possible, each
            //   name that can default does; below the losses it makes
            //   certain, only the names certain to.
            if (std::abs(saddlePt) == QL_MAX_REAL)
            {
                condContrib[iName] = (saddlePt > 0. ? pBuffer > 0. : pBuffer == 1.) ? lossInDef : 0.;
                continue;
            }
            Real midFactor = pBuffer * std::exp(lossInDef * saddlePt / remainingNotional_);
            Real denominator = 1. - pBuffer + midFactor;

            condContrib[iName] = lossInDef * midFactor / denominator;
        }
        return condContrib;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::conditionalExpectedLoss(const std::vector<Real>& invUncondProbs,
                                                           const std::vector<Real>& mktFactor) const
    {
        return conditionalExpectedLoss(namesCond(invUncondProbs, mktFactor));
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::conditionalExpectedLoss(const NamesCond& names) const
    {
        const Size nNames = remainingNotionals_.size();
        Real eloss = 0.;
        for (Size iName = 0; iName < nNames; iName++)
            eloss += names.pDef[iName] * remainingNotionals_[iName] * names.lgd[iName];
        return eloss;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::conditionalExpectedTrancheLoss(const std::vector<Real>& invUncondProbs,
                                                                  const std::vector<Real>& mktFactor) const
    {
        // What the tranche loses given the factor, E[min((L - a)^+, d - a)], is E[(L - a)^+] - E[(L - d)^+], each
        //   from the tail, where it was the expected loss tranched, min((E[L] - a)^+, d - a) (#262).
        return expectedShortfallTrancheCond(invUncondProbs, attachRatio_ * remainingNotional_, mktFactor);
    }

    template <class CP>
    std::vector<Real> SaddlePointLossModel<CP>::expectedShortfallSplitCond(const std::vector<Real>& invUncondProbs,
                                                                           Real lossPerc,
                                                                           const std::vector<Real>& mktFactor) const
    {
        const Size nNames = remainingNotionals_.size();
        std::vector<Real> lgds;
        for (Size iName = 0; iName < nNames; iName++)
            lgds.push_back(
                remainingNotionals_[iName] *
                (1. - copula_->conditionalRecoveryInvP(invUncondProbs[iName], liveNames_[iName], mktFactor)));
        std::vector<Real> vola(nNames, 0.), mu(nNames, 0.);
        Real volaTot = 0., muTot = 0.;
        for (Size iName = 0; iName < nNames; iName++)
        {
            Probability pBuffer =
                copula_->conditionalDefaultProbabilityInvP(invUncondProbs[iName], liveNames_[iName], mktFactor);
            mu[iName] = lgds[iName] * pBuffer / remainingNotionals_[iName];
            muTot += lgds[iName] * pBuffer;
            vola[iName] = lgds[iName] * lgds[iName] * pBuffer * (1. - pBuffer) / remainingNotionals_[iName];
            volaTot += lgds[iName] * lgds[iName] * pBuffer * (1. - pBuffer);
        }
        for (Size iName = 0; iName < nNames; iName++)
            vola[iName] = vola[iName] / volaTot;

        std::vector<Real> esfPartition(nNames, 0.);
        for (Size iName = 0; iName < nNames; iName++)
        {
            Real uEdisp = (lossPerc - muTot) / std::sqrt(volaTot);
            esfPartition[iName] = mu[iName] * CumulativeNormalDistribution()(uEdisp) // static call?
                                  + vola[iName] * NormalDistribution()(uEdisp);
        }
        return esfPartition;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::expectedShortfallTrancheCond(const std::vector<Real>& invUncondProbs,
                                                                Real lossLevel,
                                                                const std::vector<Real>& mktFactor) const
    {
        // E[(L - x)^+] at the level less the same at the detachment is what
        //   the tranche loses beyond the level, between 0 and the tranche's
        //   room above the level. With both ends in one region the difference
        //   keeps to that but for rounding. With the level in an exact region
        //   and the detachment in the approximation, the approximation's
        //   error at the detachment took it outside, by up to 2.1 on a
        //   plateau of the distribution given the factor (#308), until its
        //   excess was held between bounds the exact regions give (#205);
        //   since then by at most 5.2e-13 where measured, and it is still kept
        //   to that range.
        Real detachAmount = detachRatio_ * remainingNotional_;
        NamesCond names = namesCond(invUncondProbs, mktFactor);
        Real excess = tailCond(names, lossLevel).excess - tailCond(names, detachAmount).excess;
        return std::min(std::max(excess, 0.), std::max(detachAmount - lossLevel, 0.));
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::expectedShortfallFullPortfolioCond(const std::vector<Real>& invUncondProbs,
                                                                      Real lossPerc, // value
                                                                      const std::vector<Real>& mktFactor) const
    {
        /* E[L 1{L >= y}] = E[(L - y)^+] + y P(L >= y), both from the same
           approximation. Until #204 this was Martin's (2006)
           mu P + (y - mu) f / s on the tail probability and the density,
           each from its own expansion: where the level was far above the
           losses the factor made likely they disagreed, and the excess it
           gave came out negative, and next to the expected loss, where the
           search returns a saddle point of 0, it divided by 0. */
        Tail tail = tailCond(invUncondProbs, lossPerc, mktFactor);
        return tail.excess + lossPerc * tail.over;
    }

    template <class CP>
    Real SaddlePointLossModel<CP>::expectedShortfall(const Date& d, Probability percProb) const
    {
        // as for the percentile, nothing more is lost by today
        if (d <= Settings::instance().evaluationDate())
            return 0.;
        Real lossPerc = percentile(d, percProb);

        // check the trivial case when the loss is over the detachment limit
        //   to avoid computation:
        Real trancheAmount = (detachRatio_ - attachRatio_) * remainingNotional_;
        if (lossPerc >= trancheAmount)
            return trancheAmount;
        // at a probability of 1 the percentile is the most the tranche can
        //   lose, and nothing is lost beyond it
        if (percProb >= 1.)
            return lossPerc;
        std::vector<Real> invUncondProbs = basket_->remainingProbabilities(d);
        // the most the tranche can lose, as the percentile takes it
        Real most = mostTrancheLoss(invUncondProbs);
        for (Size i = 0; i < invUncondProbs.size(); i++)
            invUncondProbs[i] = copula_->inverseCumulativeY(invUncondProbs[i], liveNames_[i]);

        // The percentile, and what the tranche loses beyond it on average
        //   over the probability of getting there (Rockafellar and Uryasev).
        Real lossLevel = attachRatio_ * remainingNotional_ + lossPerc;
        Real excess = copula_->integratedExpectedValue(
            [&](const std::vector<Real>& v1) { return expectedShortfallTrancheCond(invUncondProbs, lossLevel, v1); });
        return std::min(lossPerc + excess / (1. - percProb), most);

        /* test:?
            return std::inner_product(integrESFPartition.begin(),
            integrESFPartition.end(), remainingNotionals_.begin(), 0.);
        */
    }


}

#endif
