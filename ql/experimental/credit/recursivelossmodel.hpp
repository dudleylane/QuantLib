/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
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

#ifndef quantlib_recursive_loss_model_hpp
#define quantlib_recursive_loss_model_hpp

#include <ql/experimental/credit/constantlosslatentmodel.hpp>
#include <ql/experimental/credit/defaultlossmodel.hpp>
#include <algorithm>
#include <map>

namespace QuantLib
{

    /*! Recursive STCDO default loss model for a heterogeneous pool of names.
    The pool names are heterogeneous in their default probabilities, notionals
    and recovery rates. Correlations are given by the latent model.
    The recursive pricing algorithm used here is described in Andersen, Sidenius
    and Basu; "All your hedges in one basket", Risk, November 2003, pages 67-72

        Notice that using copulas other than Gaussian it is only an
        approximation (see remark on p.68).

        \todo Make the loss unit equal to some small fraction depending on the
        portfolio loss weights (notionals and recoveries). As it is now this
        is ok for pricing but not for risk metrics. See the discussion in O'Kane
        18.3.2
    */
    template <class copulaPolicy>
    class RecursiveLossModel : public DefaultLossModel, public Observer
    {
      public:
        explicit RecursiveLossModel(const ext::shared_ptr<ConstantLossLatentmodel<copulaPolicy>>& m,
                                    // nope! use max common divisor. See O'Kane. Or give both options at least.
                                    Size nbuckets = 1)
        : copula_(m), nBuckets_(nbuckets)
        {
            registerWith(copula_);
        }

      private:
        // what the latent model says goes on to the baskets
        void update() override { notifyObservers(); }
        /* The loss distribution given the factors, in loss units, from each live
        name's default probability given them (eq. 10 p.68).
        */
        std::map<Real, Probability> conditionalLossDistrib(const std::vector<Probability>& pDefCond) const;
        /*!
        @param pDefDate Vector of unconditional default probabilities for each
        live name (at the current evaluation date). This is passed instead of
        the date for performance reasons (if in the future other magnitudes
        -e.g. lgd- are contingent on the date they shouldd be passed too).
        @param invPDefDate Their inverses through the copula. A name whose
        probability is below 1e-10 doesn't default, as in
        conditionalDefaultProbability, and its entry isn't read.
        */
        std::vector<Real> conditionalLossProb(const std::vector<Probability>& pDefDate,
                                              const std::vector<Real>& invPDefDate,
                                              const std::vector<Real>& mktFactor) const;
        Real expectedConditionalLossInvP(const std::vector<Real>& pDefDate,
                                         // const Date& date,
                                         const std::vector<Real>& mktFactor) const;

      protected:
        void resetModel() override;

      public:
        /*  Expected tranche Loss calculation.
            This is computed from the first equation on page 70 (not numbered)
            Notice that while we want to compute:
            \f[
            EL(t) = \sum_{l_k}l_k P(l;t) =
              \sum_{l_k}l_k \int P(l_k;t|\omega) d\omega q(\omega)
            \f]
            One can invert the sumation and the integral order to:
            \f[
            EL(t) = \int\,q(\omega)\,d\omega\,\sum_{l_k}\,l_k\,P(l_k;t|\omega) =
              \int\,q(\omega)\,d\omega\,EL(t|\omega)
            \f]
            and this is the way it is integrated here. The recursion formula
            makes it easier this way.
        */
        Real expectedTrancheLoss(const Date& date) const override;
        std::vector<Real> lossProbability(const Date& date) const;
        // REMEBER THIS HAS TO BE MOVED TO A DISTRIBUTION OBJECT.............
        std::map<Real, Probability> lossDistribution(const Date& d) const override;
        // INTEGRATE THEN SEARCH RATHER THAN SEARCH AND THEN INTEGRATE:
        // Here I am not using a search because the point might not be attainable
        //  (loss distrib is not continuous)
        Real percentile(const Date& d, Real percentile) const override;
        Real expectedShortfall(const Date& d, Real perctl) const override;

      protected:
        const ext::shared_ptr<ConstantLossLatentmodel<copulaPolicy>> copula_;

      private:
        // loss model descriptor members
        const Size nBuckets_;
        mutable std::vector<Real> wk_;
        mutable Real lossUnit_;
        //! name to name factor. In the single factor copula:
        //    correl = beta * beta
        // When constructing through a single correlation number the factor is
        //   taken to be the positive swuare root of this number in the copula.
        ////////in the latent model now: mutable std::vector<Real> oneFactorCorrels_;
        // cached remaining basket magnitudes:
        mutable Real attachAmount_, detachAmount_, notional_;
        mutable Size remainingBsktSize_;
        mutable std::vector<Real> notionals_;
    };


    typedef RecursiveLossModel<GaussianCopulaPolicy> RecursiveGaussLossModel;

    // Inlines ------------------------------------------------

    template <class CP>
    inline Real RecursiveLossModel<CP>::expectedTrancheLoss(const Date& date) const
    {
        /*
                std::map<Real, Probability> dist = lossDistribution(date);

                Real expLoss = 0.;
                std::map<Real, Probability>::iterator distIt = dist.begin();

                while(distIt != dist.end()) {
                    Real loss = distIt->first * lossUnit_;
                    loss = std::max(std::min(loss, detachAmount_)-attachAmount_, 0.);
                    // MIN MAX BUGS ....??
                    expLoss += loss * distIt->second;
                    distIt++;
                }
                return expLoss ;




            ///////////////////////////////////////////////////////////////////////

                // calculate inverted unconditional Ps first so we save the inversion:
                // TO DO : turn to STL algorithm code
                std::vector<Probability> uncDefProb =
                    basket_->remainingProbabilities(date);

                return copula_->integratedExpectedValue(
                    [&](const std::vector<Real>& v1) {
                        return expectedConditionalLoss(uncDefProb, v1);
                    });
                    */

        std::vector<Probability> uncDefProb = basket_->remainingProbabilities(date);
        const std::vector<Size>& live = basket_->liveList();
        std::vector<Real> invProb;
        for (Size i = 0; i < uncDefProb.size(); ++i)
            invProb.push_back(copula_->inverseCumulativeY(uncDefProb[i], live[i]));
        ///  invProb.push_back(CP::inverseCumulativeY(uncDefProb[i], i));//<-static call
        return copula_->integratedExpectedValue([&](const std::vector<Real>& v1)
                                                { return expectedConditionalLossInvP(invProb, v1); });
    }

    template <class CP>
    inline std::vector<Real> RecursiveLossModel<CP>::lossProbability(const Date& date) const
    {

        std::vector<Probability> uncDefProb = basket_->remainingProbabilities(date);
        // inverted once here, not at every node
        const std::vector<Size>& live = basket_->liveList();
        std::vector<Real> invProb(uncDefProb.size(), 0.);
        for (Size i = 0; i < uncDefProb.size(); ++i)
            if (!(uncDefProb[i] < 1.e-10))
                invProb[i] = copula_->inverseCumulativeY(uncDefProb[i], live[i]);
        return copula_->integratedExpectedValueV([&](const std::vector<Real>& v1)
                                                 { return conditionalLossProb(uncDefProb, invProb, v1); });
    }

    // -------------------------------------------------------------------

    template <class CP>
    void RecursiveLossModel<CP>::resetModel()
    {
        // basket update:
        notionals_ = basket_->remainingNotionals();
        notional_ = basket_->remainingNotional();
        attachAmount_ = basket_->remainingAttachmentAmount();
        detachAmount_ = basket_->remainingDetachmentAmount();
        // model parameters:
        remainingBsktSize_ = notionals_.size();

        copula_->resetBasket(basket_.currentLink());

        const std::vector<Size>& live = basket_->liveList();
        std::vector<Real> lgdsTmp, lgds;
        for (Size i = 0; i < remainingBsktSize_; ++i)
            lgds.push_back(notionals_[i] * (1. - copula_->recoveries()[live[i]]));
        lgdsTmp = lgds;
        ///////////////std::remove(lgds.begin(), lgds.end(), 0.);
        lgds.erase(std::remove(lgds.begin(), lgds.end(), 0.), lgds.end());
        // no name left that can lose anything means nothing to measure losses in
        lossUnit_ = lgds.empty() ? 1. : *(std::min_element(lgds.begin(), lgds.end())) / nBuckets_;
        wk_.clear();
        for (Size i = 0; i < remainingBsktSize_; ++i)
            wk_.push_back(std::floor(lgdsTmp[i] / lossUnit_ + .5));
    }

    // make it return a distribution object?
    template <class CP>
    std::map<Real, Probability> RecursiveLossModel<CP>::lossDistribution(const Date& d) const
    {
        std::vector<Real> values = lossProbability(d);
        /* Those are the probabilities of the attainable losses, in order. A
        loss distribution given the factors has one key for each, whatever its
        probability, so they're its keys where no name defaults. The i-th isn't
        i loss units unless every count of units up to the largest can be lost,
        which more than one bucket, or names of other losses, rule out (#261).
        */
        const std::map<Real, Probability> attainable =
            conditionalLossDistrib(std::vector<Probability>(remainingBsktSize_, 0.));
        QL_REQUIRE(attainable.size() == values.size(),
                   attainable.size() << " attainable losses for " << values.size() << " probabilities");
        std::map<Real, Probability> distrib;
        Real sum = 0.;
        auto loss = attainable.begin();
        for (Size i = 0; i < values.size(); ++i, ++loss)
        {
            distrib.insert(std::make_pair(loss->first * lossUnit_, sum + values[i]));
            sum += values[i];
        }
        return distrib;
    }

    // Integrate then search rather than search and then integrate?
    // Here I am not using a search because the point might be not attainable
    //   (loss distrib is not continuous)
    template <class CP>
    Real RecursiveLossModel<CP>::percentile(const Date& d, Real percentile) const
    {
        return detail::trancheLossPercentile(lossDistribution(d), attachAmount_, detachAmount_, percentile);
    }

    template <class CP>
    Real RecursiveLossModel<CP>::expectedShortfall(const Date& d, Real perctl) const
    {
        if (d == Settings::instance().evaluationDate())
            return 0.;
        return detail::trancheLossExpectedShortfall(lossDistribution(d), attachAmount_, detachAmount_, perctl);
    }

    template <class CP>
    std::map<Real, Probability>
    RecursiveLossModel<CP>::conditionalLossDistrib(const std::vector<Probability>& pDefCond) const
    {
        // eq. 10 p.68
        // attainable losses distribution, recursive algorithm
        std::map<Real, Probability> pIndepDistrib;
        ////////  K=0
        pIndepDistrib.insert(std::make_pair(0., 1.));
        for (Size iName = 0; iName < remainingBsktSize_; ++iName)
        {
            Probability pDef = pDefCond[iName];
            ////// iterate on all possible losses in the distribution:
            std::map<Real, Probability> pDistTemp;
            auto distIt = pIndepDistrib.begin();
            while (distIt != pIndepDistrib.end())
            {
                ///   update prob if this name does not default
                auto matchIt = pDistTemp.find(distIt->first);
                if (matchIt != pDistTemp.end())
                {
                    matchIt->second += distIt->second * (1. - pDef);
                }
                else
                {
                    pDistTemp.insert(std::make_pair(distIt->first, distIt->second * (1. - pDef)));
                }
                ////   and if it does
                matchIt = pDistTemp.find(distIt->first + wk_[iName]);
                if (matchIt != pDistTemp.end())
                {
                    matchIt->second += distIt->second * pDef;
                }
                else
                {
                    pDistTemp.insert(std::make_pair(distIt->first + wk_[iName], distIt->second * pDef));
                }
                ++distIt;
            }
            /////  swap back
            pIndepDistrib.swap(pDistTemp);
        }
        /* Apply tranche limits now .... mind you this could be done outside*/
        ////  to be done....
        return pIndepDistrib;
    }


    /*
    Bugs here???. The max min on the tranche looks
    wrong. It is better to have a tranche function since that way we can avoid
    adding up losses over all the possible losses rather than just over the
    tranche limits.
    */
    //! Portfolio loss conditional to the market factor value
    template <class CP>
    Real RecursiveLossModel<CP>::expectedConditionalLossInvP(const std::vector<Real>& invPDefDate,
                                                             // const Date& date,
                                                             const std::vector<Real>& mktFactor) const
    {
        const std::vector<Size>& live = basket_->liveList();
        std::vector<Probability> pDefCond(remainingBsktSize_);
        for (Size iName = 0; iName < remainingBsktSize_; ++iName)
            pDefCond[iName] = copula_->conditionalDefaultProbabilityInvP(invPDefDate[iName], live[iName], mktFactor);
        std::map<Real, Probability> pIndepDistrib = conditionalLossDistrib(pDefCond);

        // get the expected value subject to the value of the market
        //   factor.
        Real expLoss = 0.;
        //---------------------------------------------------------------
        /* This is the original (easy to read) loop which I have partially
             unroll below to take profit of the fact that once we go over
             the tranche top the loss amount is fixed:
        */
        auto distIt = pIndepDistrib.begin();

        while (distIt != pIndepDistrib.end())
        {
            Real loss = distIt->first * lossUnit_;
            //         loss = std::max(std::min(loss, detachAmount_)-attachAmount_, 0.);
            loss = std::min(std::max(loss - attachAmount_, 0.), detachAmount_ - attachAmount_);
            // MIN MAX BUGS ....???
            expLoss += loss * distIt->second;
            ++distIt;
        }
        return expLoss;
    }

    template <class CP>
    std::vector<Real> RecursiveLossModel<CP>::conditionalLossProb(const std::vector<Probability>& pDefDate,
                                                                  const std::vector<Real>& invPDefDate,
                                                                  const std::vector<Real>& mktFactor) const
    {
        const std::vector<Size>& live = basket_->liveList();
        std::vector<Probability> pDefCond(remainingBsktSize_);
        for (Size iName = 0; iName < remainingBsktSize_; ++iName)
            pDefCond[iName] =
                pDefDate[iName] < 1.e-10 ?
                    0. :
                    copula_->conditionalDefaultProbabilityInvP(invPDefDate[iName], live[iName], mktFactor);
        std::map<Real, Probability> pIndepDistrib = conditionalLossDistrib(pDefCond);

        std::vector<Real> results;
        auto distIt = pIndepDistrib.begin();
        while (distIt != pIndepDistrib.end())
        {
            // Real loss = distIt->first * loss_unit_
            //                     ;
            // loss = std::max(std::min(loss,
            //     results_.xMax)-results_.xMin, 0.);
            // expLoss += loss * distIt->second;

            results.push_back(distIt->second);
            ++distIt;
        }
        return results;
    }

}

#endif
