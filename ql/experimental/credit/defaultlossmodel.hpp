/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Roland Lichters
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

#ifndef quantlib_defaultlossmodel_hpp
#define quantlib_defaultlossmodel_hpp

#include <ql/experimental/credit/basket.hpp>
#include <ql/experimental/credit/defaultprobabilitykey.hpp>
#include <ql/instruments/claim.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <algorithm>
#include <map>

/* Intended to replace LossDistribution in
    ql/experimental/credit/lossdistribution, not sure its covering all the
    functionality (see mthod below)
*/

namespace QuantLib
{

    /*! Default loss model interface definition.
    Allows communication between the basket and specific algorithms. Intended to
    hold any kind of portfolio joint loss, latent models, top-down,....

    An inconvenience of this design as opposed to the full arguments/results
    is that when pricing several derivatives instruments on the same basket
    not all the pricing engines would point to the same loss model; thus when
    pricing a set of such instruments there might be some switching on the
    basket loss models, which might require recalculations (of the basket) or
    not depending on the pricing order.
    */
    class DefaultLossModel : public virtual Observable
    { // joint-? basket?-defaultLoss
        /* Protection together with frienship to avoid the need of checking the
        basket-argument pointer integrity. It is the responsibility of the basket
        now; our only caller.
        */
        friend class Basket;

      protected:
        // argument basket:
        mutable RelinkableHandle<Basket> basket_;

        DefaultLossModel() = default;
        //! \name Statistics
        //@{
        /* Non mandatory implementations, fails if client is not providing what
        requested. */

        /* Default implementation using the expectedLoss(Date) method.
          Typically this method is called repeatedly with the same
          date parameter which makes it innefficient. */
        virtual Real expectedTrancheLoss(const Date& d) const
        {
            QL_FAIL("expectedTrancheLoss Not implemented for this model.");
        }
        /*! Probability of the tranche losing the same or more than the
            fractional amount given.

            The passed lossFraction is a fraction of losses over the
            tranche notional (not the portfolio).
        */
        virtual Probability probOverLoss(const Date& d, Real lossFraction) const
        {
            QL_FAIL("probOverLoss Not implemented for this model.");
        }
        //! Value at Risk given a default loss percentile.
        virtual Real percentile(const Date& d, Real percentile) const
        {
            QL_FAIL("percentile Not implemented for this model.");
        }
        //! Expected shortfall given a default loss percentile.
        virtual Real expectedShortfall(const Date& d, Real percentile) const
        {
            QL_FAIL("eSF Not implemented for this model.");
        }
        //! Associated VaR fraction to each counterparty.
        virtual std::vector<Real> splitVaRLevel(const Date& d, Real loss) const
        {
            QL_FAIL("splitVaRLevel Not implemented for this model.");
        }
        //! Associated ESF fraction to each counterparty.
        virtual std::vector<Real> splitESFLevel(const Date& d, Real loss) const
        {
            QL_FAIL("splitESFLevel Not implemented for this model.");
        }

        // \todo Add splits by instrument position.

        //! Full loss distribution.
        virtual std::map<Real, Probability> lossDistribution(const Date&) const
        {
            QL_FAIL("lossDistribution Not implemented for this model.");
        }
        //! Probability density of a given loss fraction of the basket notional.
        virtual Real densityTrancheLoss(const Date& d, Real lossFraction) const
        {
            QL_FAIL("densityTrancheLoss Not implemented for this model.");
        }
        /*! The probability of each name being the nth default among the
        names left at the evaluation date, and defaulting by d. One per name,
        in the pool's order, and 0 for the names that defaulted before the
        evaluation date. Their sum is probAtLeastNEvents(n, d). For pricing
        that depends on the order of the defaults (e.g. NTDs).
        */
        virtual std::vector<Probability> probsBeingNthEvent(Size n, const Date& d) const
        {
            QL_FAIL("probsBeingNthEvent Not implemented for this model.");
        }
        /*! The probability of each name being the nth default among the
        names left at the evaluation date, and defaulting after d0 and by d.
        One per name, in the pool's order, and 0 for the names that defaulted
        before the evaluation date. Their sum is the probability of the nth
        default falling in the period. For pricing where the loss depends on
        which name defaults (e.g. NTDs on names whose recoveries differ).
        */
        virtual std::vector<Probability> probsBeingNthEventBetween(Size n, const Date& d0, const Date& d) const
        {
            QL_FAIL("probsBeingNthEventBetween, the probability of each name being the nth default in a "
                    "period, is not implemented for this model.");
        }
        /*! The mean recovery of each name given that it's the nth default
        among the names left at the evaluation date, after d0 and by d, as
        probsBeingNthEventBetween counts it. One per name, in the pool's
        order, and 0 for the names that defaulted before the evaluation date.
        It means something only where that name's probability of being the
        nth default in the period is positive. With those probabilities it
        prices a payment on the nth default that is linear in the recovery,
        as the library's claims are (#250).

        By default it's each name's expectedRecovery at d, which is the mean
        given any default only where the recovery is fixed by name. A model
        that draws recoveries has to override it, as RandomLossLM does.
        */
        virtual std::vector<Real> recoveriesBeingNthEventBetween(Size n, const Date& d0, const Date& d) const
        {
            std::vector<Real> recoveries(basket_->size(), 0.);
            for (Size iName : basket_->liveList())
                recoveries[iName] = expectedRecovery(d, iName, basket_->defaultKeys()[iName]);
            return recoveries;
        }
        //! Pearsons' default probability correlation.
        virtual Real defaultCorrelation(const Date& d, Size iName, Size jName) const
        {
            QL_FAIL("defaultCorrelation Not implemented for this model.");
        }
        /*! Returns the probaility of having a given or larger number of
        defaults in the basket portfolio at a given time.

        The defaults are counted among the names left at the evaluation
        date; Basket::probAtLeastNEvents adds those before it.
        */
        virtual Probability probAtLeastNEvents(Size n, const Date& d) const
        {
            QL_FAIL("probAtLeastNEvents Not implemented for this model.");
        }
        /*! Expected RR for name conditinal to default by that date.
         */
        virtual Real expectedRecovery(const Date&, Size iName, const DefaultProbKey&) const
        {
            QL_FAIL("expected recovery Not implemented for this model.");
        }
        //@}

        /*! Send a reference to the basket to allow the model to read the
        problem arguments (contained in the basket)
        */
      private: // can only be called from Basket
        void setBasket(Basket* bskt)
        {
            /* The model reads its arguments from one basket at a time, so its
            other baskets aren't calculated any more, and have to take it back.
            They hear before the reset, which can refuse the new basket with the
            model already moved. The basket taking the model ignores this.
            */
            basket_.linkTo(ext::shared_ptr<Basket>(bskt, null_deleter()), false);
            notifyObservers();
            resetModel(); // or rename to setBasketImpl(...)
        }
        // the call order matters, which is the reason for the parent to be the
        //   sole caller.
        //! Concrete models do now any updates/inits they need on basket reset
        virtual void resetModel() = 0;
    };

    namespace detail
    {
        /* The percentile and the expected shortfall of what a tranche loses, from a portfolio
        loss distribution given as losses and the probability of losing each or less. Between
        two losses the portfolio's loss is linear in the probability; below the first loss's
        probability it is the first loss, and above the last's the last loss. Probabilities
        above 1 are taken as 1.
        */
        inline Real trancheLossPercentile(const std::map<Real, Probability>& distribution,
                                          Real attachment,
                                          Real detachment,
                                          Probability level)
        {
            auto trancheLoss = [&](Real loss)
            { return std::min(std::max(loss - attachment, 0.), detachment - attachment); };
            auto point = distribution.begin();
            if (std::min(point->second, 1.) >= level)
                return trancheLoss(point->first);
            while (point != distribution.end() && std::min(point->second, 1.) <= level)
                ++point;
            if (point == distribution.end())
                return trancheLoss(distribution.rbegin()->first);
            Real valPlus = std::min(point->second, 1.);
            Real xPlus = point->first;
            --point; // not the first, which is at or below the level
            Real valMin = point->second;
            Real xMin = point->first;
            return trancheLoss(xPlus - (xPlus - xMin) * (valPlus - level) / (valPlus - valMin));
        }

        /* The mean of trancheLossPercentile over the levels above the given one. Between two
        losses it's linear in the probability except where the portfolio's loss crosses the
        attachment or the detachment, and the integral is split there.
        */
        inline Real trancheLossExpectedShortfall(const std::map<Real, Probability>& distribution,
                                                 Real attachment,
                                                 Real detachment,
                                                 Probability level)
        {
            if (level >= 1.)
                return trancheLossPercentile(distribution, attachment, detachment, 1.);
            auto trancheLoss = [&](Real loss)
            { return std::min(std::max(loss - attachment, 0.), detachment - attachment); };
            auto point = distribution.begin();
            Probability valMin = std::min(point->second, 1.);
            Real xMin = point->first;
            Real sum = valMin > level ? (valMin - level) * trancheLoss(xMin) : 0.;
            for (++point; point != distribution.end(); ++point)
            {
                Probability valPlus = std::min(point->second, 1.);
                Real xPlus = point->first;
                if (valPlus > level && valPlus > valMin)
                {
                    Probability from = std::max(valMin, level);
                    Real xFrom = from > valMin ? xPlus - (xPlus - xMin) * (valPlus - from) / (valPlus - valMin) : xMin;
                    for (Real limit : {attachment, detachment})
                    {
                        if (xFrom < limit && limit < xPlus)
                        {
                            Probability to = from + (limit - xFrom) / (xPlus - xFrom) * (valPlus - from);
                            sum += .5 * (trancheLoss(xFrom) + trancheLoss(limit)) * (to - from);
                            from = to;
                            xFrom = limit;
                        }
                    }
                    sum += .5 * (trancheLoss(xFrom) + trancheLoss(xPlus)) * (valPlus - from);
                }
                valMin = valPlus;
                xMin = xPlus;
            }
            if (valMin < 1.)
                sum += (1. - std::max(valMin, level)) * trancheLoss(xMin);
            return sum / (1. - level);
        }
    }

}

#endif
