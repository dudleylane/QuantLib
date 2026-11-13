/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2009, 2014 Jose Aparicio

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

#ifndef quantlib_inhomogenous_pool_default_model_hpp
#define quantlib_inhomogenous_pool_default_model_hpp

#include <ql/experimental/credit/basket.hpp>
#include <ql/experimental/credit/constantlosslatentmodel.hpp>
#include <ql/experimental/credit/defaultlossmodel.hpp>
#include <ql/experimental/credit/lossdistribution.hpp>
#include <ql/experimental/math/tcopulapolicy.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <cmath>
#include <type_traits>

// Intended to replace InhomogeneousPoolCDOEngine in syntheticcdoengines.hpp

namespace QuantLib
{

    //-------------------------------------------------------------------------
    //! Default loss distribution convolution for finite non homogeneous pool
    /* A note on the number of buckets: As it is now the code goes splitting
    losses into buckets from loses equal to zero to losses up to the value of
    the underlying basket. This is in view of a stochastic loss given default
    but in a constant LGD situation this is a waste and it is more efficient to
    go up to the attainable losses.

    The factor is integrated by the midpoint rule on [min, max] in nSteps
    steps of its normal quantile u (#259): the node at u is the factor's
    value F^{-1}(Phi(u)), and its weight the step times phi(u). On a Gaussian
    factor that is u itself, weighted by its density, as before. On a t
    factor the rule was in the factor itself, and [-5, 5] left out 3.2e-3 of
    a t3 factor's mass and 1.3e-3 of a t5 one's, which the loss distribution
    then counted as a loss beyond every bucket; in u it leaves out 5.7e-7 of
    any factor's. Nodes past |u| of about 37.5, where Phi(-|u|) underflows,
    are skipped: their weights are below 1e-305. A step in u spans more of a
    t factor in its tails than in its middle, and a thin tranche at a short
    horizon takes its losses from the tails. So on t factors such tranches
    need more steps than on Gaussian ones, and few steps resolve them worse
    than the rule in the factor itself did.

    The expected tranche loss integrates over the same nodes the tranche's
    expected loss given the factor (#260), taking each bucket's losses at
    their average. That is exact where each bucket holds losses of one
    value, as on names of equal losses with buckets narrower than one loss.
    Where a bucket holds several, the bucketing moves them together, so a
    tranche whose attachment falls inside such a bucket, or where the
    bucketing carries some of them past the detachment, is off: on 12 names
    of unequal losses by up to 3.6e-3 at 50 buckets and 2.8e-5 at 200. It
    came from the integrated distribution's excess probabilities before, a
    sum that counts each loss as reaching the top of its bucket: at 200
    buckets, Hull-White's senior tranche's came out 16 % high by five years,
    and on those 12 names the tranches' up to 10 %.

    On t factors of 3 degrees of freedom at correlation 0.3, 100 names at
    hazard 1 %, the 6-10 % tranche's expected loss by a year at the default
    50 steps is 1.8 % above its value at 170 steps, where the rule in the
    factor and the sum used before gave 34 %. At 15 steps it is 54 % above,
    where they gave -3.3 %, by cancelling the rule's truncation against its
    coarseness. By five years 50 steps are within 1.2e-4. The default
    bounds leave out a Gaussian factor's tails beyond 5, where a senior
    tranche at low correlation takes its losses at short horizons: at
    correlation 0.1 the 10-100 % tranche's expected loss by a year comes out
    4 % low.
    \todo Extend to the multifactor case for a generic LM
    \todo Many common code with the homogeneous version, both classes perform
    the same work on different loss distribution types, merge and send the
    distribution object?
    */
    template <class copulaPolicy>
    class InhomogeneousPoolLossModel : public DefaultLossModel, public Observer
    {
      private:
        void resetModel() override;
        // what the latent model says goes on to the baskets
        void update() override { notifyObservers(); }

      public:
        // allow base correlations:
        typedef copulaPolicy copulaType;

        InhomogeneousPoolLossModel(
            // restricted to non random recoveries, but it could be possible.
            const ext::shared_ptr<ConstantLossLatentmodel<copulaPolicy>>& copula,
            Size nBuckets,
            Real max = 5.,
            Real min = -5.,
            Size nSteps = 50)
        : copula_(copula), nBuckets_(nBuckets), max_(max), min_(min), nSteps_(nSteps), delta_((max - min) / nSteps)
        {
            QL_REQUIRE(copula->numFactors() == 1, "Inhomogeneous model not implemented for multifactor");
            registerWith(copula_);
        }
        // Write another constructor sending the LM factors and recoveries.
      protected:
        Distribution lossDistrib(const Date& d) const;
        /* The node at u of the midpoint rule in the factor's normal quantile
           (#259): the factor's value F^{-1}(Phi(u)) and the weight delta
           phi(u); on a Gaussian factor u itself and its density. False where
           Phi(-|u|) underflows, beyond |u| of about 37.5: the quantile
           overflows there, and the weight is below 1e-305. */
        bool factorNode(Real u, std::vector<Real>& m, Real& weight) const;
        Real expectedTrancheLossByNodes(const Date& d) const;

      public:
        Real expectedTrancheLoss(const Date& d) const override
        {
            return expectedTrancheLossByNodes(d);
            // This one if the distribution is over the whole loss structure:
            // but it becomes very expensive
            /*
            return lossDistrib(d).trancheExpectedValue(
                attachAmount_, detachAmount_);
            */
        }
        Real percentile(const Date& d, Real percentile) const override
        {
            Real portfLoss = lossDistrib(d).confidenceLevel(percentile);
            return std::min(std::max(portfLoss - attachAmount_, 0.), detachAmount_ - attachAmount_);
        }
        Real expectedShortfall(const Date& d, Probability percentile) const override
        {
            return lossDistrib(d).trancheExpectedShortfall(percentile, attachAmount_, detachAmount_);
        }

      protected:
        const ext::shared_ptr<ConstantLossLatentmodel<copulaPolicy>> copula_;
        Size nBuckets_;
        mutable Real attach_, detach_, notional_, attachAmount_, detachAmount_;
        mutable std::vector<Real> notionals_;

      private:
        // integration:
        //  \todo move integration to latent model types when moving to a
        //  multifactor version
        const Real max_; // redundant?
        const Real min_;
        const Size nSteps_;
        const Real delta_;
    };
    // \todo Add other loss distribution statistics
    typedef InhomogeneousPoolLossModel<GaussianCopulaPolicy> IHGaussPoolLossModel;
    typedef InhomogeneousPoolLossModel<TCopulaPolicy> IHStudentPoolLossModel;

    //-----------------------------------------------------------------------

    template <class CP>
    void InhomogeneousPoolLossModel<CP>::resetModel()
    {
        // need to be capped now since the limit amounts might be over the
        //  remaining notional (think amortizing)
        attach_ = std::min(basket_->remainingAttachmentAmount() / basket_->remainingNotional(), 1.);
        detach_ = std::min(basket_->remainingDetachmentAmount() / basket_->remainingNotional(), 1.);
        notional_ = basket_->remainingNotional();
        notionals_ = basket_->remainingNotionals();
        attachAmount_ = basket_->remainingAttachmentAmount();
        detachAmount_ = basket_->remainingDetachmentAmount();

        copula_->resetBasket(basket_.currentLink());
    }

    template <class CP>
    Distribution InhomogeneousPoolLossModel<CP>::lossDistrib(const Date& d) const
    {
        LossDistBucketing bucktLDistBuff(nBuckets_, detachAmount_);

        // these go by the names left in the basket, the latent model by all of them
        const std::vector<Size>& live = basket_->liveList();
        std::vector<Real> lgd; // switch to a mutable cache member
        lgd.reserve(live.size());
        for (Size iName = 0; iName < live.size(); iName++)
            lgd.push_back(notionals_[iName] * (1.0 - copula_->recoveries()[live[iName]]));
        std::vector<Real> prob = basket_->remainingProbabilities(d);
        for (Size iName = 0; iName < prob.size(); iName++)
            prob[iName] = copula_->inverseCumulativeY(prob[iName], live[iName]);

        // integrate locally (1 factor).
        // use explicitly a 1D latent model object?
        // \todo Use a library integrator here and in the homogeneous case.
        Distribution dist(nBuckets_, 0.0, detachAmount_);
        // notional_);
        std::vector<Real> mkft(1);
        Real u = min_ + delta_ / 2.;
        for (Size i = 0; i < nSteps_; i++, u += delta_)
        {
            Real densitydm;
            if (!factorNode(u, mkft, densitydm))
                continue;
            std::vector<Real> conditionalProbs;
            conditionalProbs.reserve(notionals_.size());
            for (Size iName = 0; iName < notionals_.size(); iName++)
                conditionalProbs.push_back(copula_->conditionalDefaultProbabilityInvP(prob[iName], live[iName], mkft));
            Distribution bld = bucktLDistBuff(lgd, conditionalProbs);
            // also, instead of calling the static method it could be wrapped
            // through an inlined call in the latent model
            for (Size j = 0; j < nBuckets_; j++)
                dist.addDensity(j, bld.density(j) * densitydm);
        }
        return dist;
    }

    template <class CP>
    Real InhomogeneousPoolLossModel<CP>::expectedTrancheLossByNodes(const Date& d) const
    {
        LossDistBucketing bucktLDistBuff(nBuckets_, detachAmount_);
        const std::vector<Size>& live = basket_->liveList();
        std::vector<Real> lgd;
        lgd.reserve(live.size());
        for (Size iName = 0; iName < live.size(); iName++)
            lgd.push_back(notionals_[iName] * (1.0 - copula_->recoveries()[live[iName]]));
        std::vector<Real> prob = basket_->remainingProbabilities(d);
        for (Size iName = 0; iName < prob.size(); iName++)
            prob[iName] = copula_->inverseCumulativeY(prob[iName], live[iName]);
        Real result = 0.0;
        std::vector<Real> mkft(1);
        Real u = min_ + delta_ / 2.;
        for (Size i = 0; i < nSteps_; i++, u += delta_)
        {
            Real densitydm;
            if (!factorNode(u, mkft, densitydm))
                continue;
            std::vector<Real> conditionalProbs;
            conditionalProbs.reserve(notionals_.size());
            for (Size iName = 0; iName < notionals_.size(); iName++)
                conditionalProbs.push_back(copula_->conditionalDefaultProbabilityInvP(prob[iName], live[iName], mkft));
            Distribution bld = bucktLDistBuff(lgd, conditionalProbs);
            Real e = 0.0, inGrid = 0.0;
            for (Size j = 0; j < nBuckets_; j++)
            {
                const Real pj = bld.density(j) * bld.dx(j);
                inGrid += pj;
                e += pj * std::min(std::max(bld.average(j) - attachAmount_, Real(0.)), detachAmount_ - attachAmount_);
            }
            e += std::max(1.0 - inGrid, 0.0) * (detachAmount_ - attachAmount_);
            result += densitydm * e;
        }
        return result;
    }

    template <class CP>
    bool InhomogeneousPoolLossModel<CP>::factorNode(Real u, std::vector<Real>& m, Real& weight) const
    {
        if constexpr (std::is_same_v<CP, TCopulaPolicy>)
        {
            // from the lower tail, where it is accurate; t factors are symmetric
            const Real tail = CumulativeNormalDistribution()(-std::fabs(u));
            if (tail < QL_MIN_POSITIVE_REAL)
                return false;
            const Real lower = copula_->inverseCumulativeDensity(tail, 0);
            m[0] = u <= 0.0 ? lower : Real(-lower);
            weight = delta_ * NormalDistribution()(u);
        }
        else
        {
            m[0] = u;
            weight = delta_ * copula_->density(m);
        }
        return true;
    }

}

#endif
