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

#ifndef quantlib_default_latent_model_hpp
#define quantlib_default_latent_model_hpp

#include <ql/experimental/credit/basket.hpp>
#include <ql/experimental/math/gaussiancopulapolicy.hpp>
#include <ql/experimental/math/latentmodel.hpp>

namespace QuantLib
{

    /*! \brief Default event Latent Model.

     This is a model for joint default events based on a generic Latent
      Model. It models solely the default events in a portfolio, not making any
      reference to severities, exposures, etc...
     An implicit correspondence is stablished between the variables modelled and
     the names in the basket given by the basket and model variable access
     indices.
     The class is parametric on the Latent Model copula.

     \todo Consider QL_REQUIRE(basket_, "No portfolio basket set.") test in
     debug model only for performance reasons.
    */
    template <class copulaPolicy>
    class DefaultLatentModel : public LatentModel<copulaPolicy>
    {
        // import template members
      protected:
        using LatentModel<copulaPolicy>::factorWeights_;
        using LatentModel<copulaPolicy>::idiosyncFctrs_;
        using LatentModel<copulaPolicy>::copula_;

      public:
        using LatentModel<copulaPolicy>::inverseCumulativeY;
        using LatentModel<copulaPolicy>::cumulativeZ;
        using LatentModel<copulaPolicy>::integratedExpectedValue; // which one?
      protected:
        // not a handle, the model doesnt keep any cached magnitudes, no need
        //  for notifications, still...
        mutable ext::shared_ptr<Basket> basket_;
        ext::shared_ptr<LMIntegration> integration_;

      private:
        typedef typename copulaPolicy::initTraits initTraits;

      public:
        /*!
        @param factorWeights Latent model independent factors weights for each
            variable.
        @param integralType Integration type.
        @param ini Copula initialization if any.

        \warning On a basket with defaults before the evaluation date, the
            statistics of one name or a pair, such as probOfDefault and
            defaultCorrelation, take a defaulted name's curve as if it
            hadn't defaulted. probAtLeastNEvents counts only the names left.
        */
        DefaultLatentModel(const std::vector<std::vector<Real>>& factorWeights,
                           LatentModelIntegrationType::LatentModelIntegrationType integralType,
                           const initTraits& ini = initTraits())
        : LatentModel<copulaPolicy>(factorWeights, ini),
          integration_(
              LatentModel<copulaPolicy>::IntegrationFactory::createLMIntegration(factorWeights[0].size(), integralType))
        {
        }
        DefaultLatentModel(const Handle<Quote>& mktCorrel,
                           Size nVariables,
                           LatentModelIntegrationType::LatentModelIntegrationType integralType,
                           const initTraits& ini = initTraits())
        : LatentModel<copulaPolicy>(mktCorrel, nVariables, ini),
          integration_(LatentModel<copulaPolicy>::IntegrationFactory::createLMIntegration(1, integralType))
        {
        }
        /* \todo
            Add other constructors as in LatentModel for ease of use. (less
            dimensions, factors, etcc...)
        */

        /* To interface with loss models. It is possible to change the basket
        since there are no cached magnitudes.
        */
        void resetBasket(const ext::shared_ptr<Basket>& basket) const
        {
            basket_ = basket;
            // in the future change 'size' to 'liveSize'
            QL_REQUIRE(basket_->size() == factorWeights_.size(), "Incompatible new basket and model sizes.");
        }

        /*! Returns the probability of default of a given name conditional on
        the realization of a given set of values of the model independent
        factors. The date at which the probability is given is implicit in the
        probability since theres not other time dependence in this model.
        @param prob Unconditional probability of default.
        @param iName desired name.
        @param mktFactors Value of LM independent factors.
        \warning Most often it is preferred to use the method below avoiding the
        cumulative inversion.
        */
        Probability
        conditionalDefaultProbability(Probability prob, Size iName, const std::vector<Real>& mktFactors) const
        {
            // we can be called from the outside (from an integrable loss model)
            //   but we are called often at integration points. This or
            //   consider a list of friends.
#if defined(QL_EXTRA_SAFETY_CHECKS)
            QL_REQUIRE(basket_, "No portfolio basket set.");
#endif
            /*Avoid redundant call to minimum value inversion (might be \infty),
            and this independently of the copula function.
            */
            if (prob < 1.e-10)
                return 0.; // use library macro...
            return conditionalDefaultProbabilityInvP(inverseCumulativeY(prob, iName), iName, mktFactors);
        }

      public: // open since users access it for performance on joint integrations.
        /*! Returns the probability of default of a given name conditional on
        the realization of a given set of values of the model independent
        factors. The date at which the probability is given is implicit in the
        probability since theres not other time dependent in this model.
        Same intention as above but provides a performance opportunity, if the
        integration is along the market factors (as usually is) avoids computing
        the inverse of the probability on each call.
        @param invCumYProb Inverse cumul of the unconditional probability of
          default, has to follow the same copula law for results to be coherent
        @param iName desired name.
        @param m Value of LM independent factors.
        */
        Probability conditionalDefaultProbabilityInvP(Real invCumYProb, Size iName, const std::vector<Real>& m) const
        {
            Real sumMs =
                std::inner_product(factorWeights_[iName].begin(), factorWeights_[iName].end(), m.begin(), Real(0.));
            Real res = cumulativeZ((invCumYProb - sumMs) / idiosyncFctrs_[iName]);
#if defined(QL_EXTRA_SAFETY_CHECKS)
            QL_REQUIRE(res >= 0. && res <= 1., "conditional probability " << res << "out of range");
#endif

            return res;
        }

      protected:
        /*! Returns the probability of default of a given name conditional on
        the realization of a given set of values of the model independent
        factors.
        @param date The date for the probability of default.
        @param iName desired name.
        @param mktFactors Value of LM independent factors.

        Same intention as the above methods. Usage of this one is typically more
        expensive because most often the date we call this method with
        repeats itself and with this one the probability can not be cached
        outside the call.
        */
        Probability
        conditionalDefaultProbability(const Date& date, Size iName, const std::vector<Real>& mktFactors) const
        {
            const ext::shared_ptr<Pool>& pool = basket_->pool();
            Probability pDefUncond = pool->get(pool->names()[iName])
                                         .defaultProbability(basket_->defaultKeys()[iName])
                                         ->defaultProbability(date);
            return conditionalDefaultProbability(pDefUncond, iName, mktFactors);
        }
        /*! Conditional default probability product, intermediate step in the
            correlation calculation.*/
        Probability condProbProduct(
            Real invCumYProb1, Real invCumYProb2, Size iName1, Size iName2, const std::vector<Real>& mktFactors) const
        {
            return conditionalDefaultProbabilityInvP(invCumYProb1, iName1, mktFactors) *
                   conditionalDefaultProbabilityInvP(invCumYProb2, iName2, mktFactors);
        }
        //! Conditional probability of n default events or more among the names left.
        Real conditionalProbAtLeastNEvents(Size n, const Date& date, const std::vector<Real>& mktFactors) const;
        //! access to integration:
        const ext::shared_ptr<LMIntegration>& integration() const override { return integration_; }

      public:
        /*! Computes the unconditional probability of default of a given name.
        Trivial method for testing
        */
        Probability probOfDefault(Size iName, const Date& d) const
        {
            QL_REQUIRE(basket_, "No portfolio basket set.");
            const ext::shared_ptr<Pool>& pool = basket_->pool();
            // avoid repeating this, or its inverse below, in the integration:
            Probability pUncond = pool->get(pool->names()[iName])
                                      .defaultProbability(basket_->defaultKeys()[iName])
                                      ->defaultProbability(d);
            if (pUncond < 1.e-10)
                return 0.;
            const Real inverse = inverseCumulativeY(pUncond, iName);

            return integratedExpectedValue([&](const std::vector<Real>& v1)
                                           { return conditionalDefaultProbabilityInvP(inverse, iName, v1); });
        }
        /*! Pearsons' default probability correlation.
            Users should consider specialization on the copula type for specific
            distributions since that might simplify the integrations, most
            importantly if this is to be used in calibration of observations for
            factor coefficients as it is expensive to integrate directly.
        */
        Real defaultCorrelation(const Date& d, Size iNamei, Size iNamej) const;

        /*! Returns the probaility of having a given or larger number of
        defaults in the basket portfolio at a given time.

        The defaults are counted among the names left at the evaluation
        date; Basket::probAtLeastNEvents adds those before it.
        */
        Probability probAtLeastNEvents(Size n, const Date& date) const;
        /*! The probability of each name being the nth default among the names
        left at the evaluation date, and defaulting after d0 and by d. One per
        name, in the pool's order, and 0 for the names that defaulted before
        the evaluation date. Their sum is probAtLeastNEvents(n, d) less
        probAtLeastNEvents(n, d0).

        Given the factors the names default independently. The probability of
        the nth default in the period is exact, and its split among the names
        is approximate, as in detail::conditionalProbsBeingNthEventBetween.

        \warning The integration has to integrate vectors, as
            GaussianQuadrature does and Trapezoid doesn't.
        */
        std::vector<Probability> probsBeingNthEventBetween(Size n, const Date& d0, const Date& d) const;
    };


    //---- Defines -----------------------------------------------------------

    template <class CP>
    Real DefaultLatentModel<CP>::defaultCorrelation(const Date& d, Size iNamei, Size iNamej) const
    {
        QL_REQUIRE(basket_, "No portfolio basket set.");

        const ext::shared_ptr<Pool>& pool = basket_->pool();
        // unconditionals:
        Probability pi =
            pool->get(pool->names()[iNamei]).defaultProbability(basket_->defaultKeys()[iNamei])->defaultProbability(d);
        Probability pj =
            pool->get(pool->names()[iNamej]).defaultProbability(basket_->defaultKeys()[iNamej])->defaultProbability(d);
        Real pipj = pi * pj;
        Real invPi = inverseCumulativeY(pi, iNamei);
        Real invPj = inverseCumulativeY(pj, iNamej);
        // avoid repetitive calls when i=j?
        Real E1i1j; // joint default covariance term
        if (iNamei != iNamej)
        {
            E1i1j = integratedExpectedValue([&](const std::vector<Real>& v1)
                                            { return condProbProduct(invPi, invPj, iNamei, iNamej, v1); });
        }
        else
        {
            E1i1j = pi;
        }
        return (E1i1j - pipj) / std::sqrt(pipj * (1. - pi) * (1. - pj));
    }


    namespace detail
    {
        /* Adds a name, defaulting with probability p independently of the
        others, to the number of defaults among the names before it: at [j]
        the probability of j defaults for j up to n - 1, and of n or more at
        [n]. last and next may be the same. n has to be at least 1.
        */
        inline void addNameToDefaultCounts(const Probability* last, Probability* next, Size n, Probability p)
        {
            next[n] = last[n] + last[n - 1] * p;
            for (Size j = n - 1; j > 0; --j)
                next[j] = last[j] * (1. - p) + last[j - 1] * p;
            next[0] = last[0] * (1. - p);
        }

        /* For names defaulting independently, each with the given probability:
        the probability of n defaults or more among them. The count is built
        one name at a time, in L n steps for L names.
        */
        inline Probability conditionalProbAtLeastNEvents(Size n, const std::vector<Probability>& pDefCond)
        {
            if (n == 0)
                return 1.;
            if (n > pDefCond.size())
                return 0.;
            std::vector<Probability> counts(n + 1, 0.);
            counts[0] = 1.;
            for (Probability p : pDefCond)
                addNameToDefaultCounts(counts.data(), counts.data(), n, p);
            return counts[n];
        }
    }


    template <class CP>
    Real DefaultLatentModel<CP>::conditionalProbAtLeastNEvents(Size n,
                                                               const Date& date,
                                                               const std::vector<Real>& mktFactors) const
    {
        QL_REQUIRE(basket_, "No portfolio basket set.");

        const std::vector<Size>& alive = basket_->liveList();
        const ext::shared_ptr<Pool>& pool = basket_->pool();

        // Precalc conditional probabilities, each name's by its place in the pool
        std::vector<Probability> pDefCond;
        pDefCond.reserve(alive.size());
        for (Size i : alive)
            pDefCond.push_back(conditionalDefaultProbability(
                pool->get(pool->names()[i]).defaultProbability(basket_->defaultKeys()[i])->defaultProbability(date), i,
                mktFactors));

        return detail::conditionalProbAtLeastNEvents(n, pDefCond);
    }


    template <class CP>
    Probability DefaultLatentModel<CP>::probAtLeastNEvents(Size n, const Date& date) const
    {
        QL_REQUIRE(basket_, "No portfolio basket set.");
        const std::vector<Size>& alive = basket_->liveList();
        const Size liveSize = alive.size();
        const ext::shared_ptr<Pool>& pool = basket_->pool();
        /* Each name's probability, inverted here once rather than at each
        point of the integration. Below 1.e-10 the conditional probability
        is 0, as in conditionalDefaultProbability.
        */
        std::vector<Probability> p(liveSize);
        std::vector<Real> inverse(liveSize, 0.);
        for (Size k = 0; k < liveSize; ++k)
        {
            const Size i = alive[k];
            p[k] = pool->get(pool->names()[i]).defaultProbability(basket_->defaultKeys()[i])->defaultProbability(date);
            if (p[k] >= 1.e-10)
                inverse[k] = inverseCumulativeY(p[k], i);
        }
        return integratedExpectedValue(
            [&](const std::vector<Real>& m)
            {
                std::vector<Probability> pDefCond(liveSize);
                for (Size k = 0; k < liveSize; ++k)
                    pDefCond[k] = p[k] < 1.e-10 ? 0. : conditionalDefaultProbabilityInvP(inverse[k], alive[k], m);
                return detail::conditionalProbAtLeastNEvents(n, pDefCond);
            });
    }


    namespace detail
    {
        /* For names defaulting independently, each with the given probability
        of defaulting by a period's start and by its end: the probability of
        each being the nth to default and defaulting in the period. Their sum
        is the probability of the nth default falling in the period, exactly.
        It is split among the names in proportion to each one's probability of
        defaulting in the period times the mean, at the period's two ends, of
        the probability of exactly n - 1 defaults among the others, which is
        exact to second order in the period's length. Where that leaves
        nothing to split by, as where every name is certain to default in the
        period, the split goes by the probability of defaulting in it.
        */
        inline std::vector<Probability> conditionalProbsBeingNthEventBetween(Size n,
                                                                             const std::vector<Probability>& atStart,
                                                                             const std::vector<Probability>& atEnd)
        {
            QL_REQUIRE(n >= 1, "the rank of a default has to be at least 1");
            const Size size = atStart.size();
            const Size width = n + 1;
            /* The number of defaults among the names before the kth, at
            [k * width + j]: the probability of j defaults for j up to n - 1,
            and of n or more at j = n.
            */
            auto countsBefore = [&](const std::vector<Probability>& p)
            {
                std::vector<Probability> counts((size + 1) * width, 0.);
                counts[0] = 1.;
                for (Size k = 0; k < size; ++k)
                    addNameToDefaultCounts(&counts[k * width], &counts[(k + 1) * width], n, p[k]);
                return counts;
            };
            // The number among the kth and the names after it, up to n - 1.
            auto countsFrom = [&](const std::vector<Probability>& p)
            {
                std::vector<Probability> counts((size + 1) * width, 0.);
                counts[size * width] = 1.;
                for (Size k = size; k > 0; --k)
                {
                    const Probability* last = &counts[k * width];
                    Probability* next = &counts[(k - 1) * width];
                    for (Size j = n - 1; j > 0; --j)
                        next[j] = last[j] * (1. - p[k - 1]) + last[j - 1] * p[k - 1];
                    next[0] = last[0] * (1. - p[k - 1]);
                }
                return counts;
            };
            const std::vector<Probability> beforeStart = countsBefore(atStart), beforeEnd = countsBefore(atEnd);
            const std::vector<Probability> fromStart = countsFrom(atStart), fromEnd = countsFrom(atEnd);
            // exactly n - 1 defaults among the names other than the kth
            auto othersAtRank =
                [n, width](const std::vector<Probability>& before, const std::vector<Probability>& after, Size k)
            {
                Probability sum = 0.;
                for (Size j = 0; j < n; ++j)
                    sum += before[k * width + j] * after[(k + 1) * width + n - 1 - j];
                return sum;
            };
            std::vector<Real> shares(size), inPeriod(size);
            Real sumShares = 0., sumInPeriod = 0.;
            for (Size k = 0; k < size; ++k)
            {
                inPeriod[k] = std::max(atEnd[k] - atStart[k], Real(0.));
                shares[k] =
                    inPeriod[k] * 0.5 * (othersAtRank(beforeStart, fromStart, k) + othersAtRank(beforeEnd, fromEnd, k));
                sumShares += shares[k];
                sumInPeriod += inPeriod[k];
            }
            const Probability total = std::max(beforeEnd[size * width + n] - beforeStart[size * width + n], Real(0.));
            const std::vector<Real>& by = sumShares > 0. ? shares : inPeriod;
            const Real sumBy = sumShares > 0. ? sumShares : sumInPeriod;
            std::vector<Probability> result(size, 0.);
            if (sumBy > 0.)
                for (Size k = 0; k < size; ++k)
                    result[k] = total * (by[k] / sumBy);
            return result;
        }
    }


    template <class CP>
    std::vector<Probability>
    DefaultLatentModel<CP>::probsBeingNthEventBetween(Size n, const Date& d0, const Date& d) const
    {
        QL_REQUIRE(basket_, "No portfolio basket set.");
        QL_REQUIRE(n >= 1, "the rank of a default has to be at least 1");
        QL_REQUIRE(d0 <= d, "the period's start, " << d0 << ", is after its end, " << d);
        const std::vector<Size>& alive = basket_->liveList();
        const Size liveSize = alive.size();
        std::vector<Probability> probs(basket_->size(), 0.);
        if (n > liveSize)
            return probs;
        const ext::shared_ptr<Pool>& pool = basket_->pool();
        /* Each name's probability at both dates, inverted here once rather
        than at each point of the integration. Below 1.e-10 the conditional
        probability is 0, as in conditionalDefaultProbability.
        */
        std::vector<Probability> p0(liveSize), p1(liveSize);
        std::vector<Real> inverse0(liveSize, 0.), inverse1(liveSize, 0.);
        for (Size k = 0; k < liveSize; ++k)
        {
            const Size i = alive[k];
            const Handle<DefaultProbabilityTermStructure>& curve =
                pool->get(pool->names()[i]).defaultProbability(basket_->defaultKeys()[i]);
            p0[k] = curve->defaultProbability(d0);
            p1[k] = curve->defaultProbability(d);
            if (p0[k] >= 1.e-10)
                inverse0[k] = inverseCumulativeY(p0[k], i);
            if (p1[k] >= 1.e-10)
                inverse1[k] = inverseCumulativeY(p1[k], i);
        }
        const std::vector<Real> integrated = this->integratedExpectedValueV(
            [&](const std::vector<Real>& m)
            {
                std::vector<Probability> q0(liveSize), q1(liveSize);
                for (Size k = 0; k < liveSize; ++k)
                {
                    q0[k] = p0[k] < 1.e-10 ? 0. : conditionalDefaultProbabilityInvP(inverse0[k], alive[k], m);
                    q1[k] = p1[k] < 1.e-10 ? 0. : conditionalDefaultProbabilityInvP(inverse1[k], alive[k], m);
                }
                return detail::conditionalProbsBeingNthEventBetween(n, q0, q1);
            });
        for (Size k = 0; k < liveSize; ++k)
            probs[alive[k]] = integrated[k];
        return probs;
    }


    // often used:
    typedef DefaultLatentModel<GaussianCopulaPolicy> GaussianDefProbLM;
    typedef DefaultLatentModel<TCopulaPolicy> TDefProbLM;
}

#endif
