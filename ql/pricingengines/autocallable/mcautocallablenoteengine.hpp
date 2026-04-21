/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 dudleylane

 This file is new in the dudleylane fork of QuantLib and is licensed
 under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
 See the LICENSE file in the repository root for the full license text.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file mcautocallablenoteengine.hpp
    \brief Monte Carlo engine for AutocallableNote under Black-Scholes
*/

#ifndef quantlib_mc_autocallable_note_engine_hpp
#define quantlib_mc_autocallable_note_engine_hpp

#include <ql/instruments/autocallablenote.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/math/statistics/statistics.hpp>
#include <ql/timegrid.hpp>
#include <utility>

namespace QuantLib {

    //! Plain Monte-Carlo engine for an AutocallableNote on a BS process.
    /*! Single-underlying, one path through the observation schedule per
        draw. Payoff per path:
          - First observation index i* where S(t_i) >= B_i * S_0:
                PV = notional * (1 + coupon * (i+1)) * df(t_i)
          - If never autocalled:
                at maturity T = t_N:
                  if S(T) >= protectionBarrier * S_0:
                      PV = notional * (1 + coupon * N) * df(T)
                  else:
                      PV = notional * (S(T) / S_0) * df(T)
        Discount factors are taken from the process's risk-free curve.
        No variance reduction; antithetic / control variates can be
        layered on as follow-ups.
    */
    template <class RNG = PseudoRandom, class S = Statistics>
    class MCAutocallableNoteEngine : public AutocallableNote::engine {
      public:
        MCAutocallableNoteEngine(
            ext::shared_ptr<GeneralizedBlackScholesProcess> process,
            Size requiredSamples,
            BigNatural seed = 0)
        : process_(std::move(process)),
          requiredSamples_(requiredSamples), seed_(seed) {
            QL_REQUIRE(process_, "MCAutocallableNoteEngine: null process");
            QL_REQUIRE(requiredSamples_ > 0,
                       "MCAutocallableNoteEngine: samples must be positive");
            registerWith(process_);
        }

        void calculate() const override {
            const auto& args = this->arguments_;
            auto& results = this->results_;

            // Year fractions from today to each observation date and
            // to maturity, under the BS process's day counter.
            const auto& dc = process_->riskFreeRate()->dayCounter();
            Date today = process_->riskFreeRate()->referenceDate();

            std::vector<Time> obsTimes;
            obsTimes.reserve(args.observationDates.size());
            for (const Date& d : args.observationDates) {
                Time t = dc.yearFraction(today, d);
                QL_REQUIRE(t > 0.0,
                           "MCAutocallableNoteEngine: observation date "
                           << d << " is not strictly after today ("
                           << today << ")");
                obsTimes.push_back(t);
            }
            Time maturityTime = dc.yearFraction(today, args.maturityDate);

            TimeGrid grid(obsTimes.begin(), obsTimes.end());

            std::vector<DiscountFactor> obsDfs;
            obsDfs.reserve(obsTimes.size());
            for (Time t : obsTimes)
                obsDfs.push_back(process_->riskFreeRate()->discount(t));
            DiscountFactor maturityDf =
                process_->riskFreeRate()->discount(maturityTime);

            // Path generator
            using generator_type =
                PathGenerator<typename RNG::rsg_type>;
            typename RNG::rsg_type rsg =
                RNG::make_sequence_generator(grid.size() - 1, seed_);
            generator_type pathGen(process_, grid, rsg, /*brownianBridge=*/false);

            S stats;
            for (Size n = 0; n < requiredSamples_; ++n) {
                const auto& sample = pathGen.next();
                const Path& path = sample.value;
                Real pv = pricePath(path, obsTimes, obsDfs, maturityDf, args);
                stats.add(pv);
            }
            results.value = stats.mean();
            results.errorEstimate = stats.errorEstimate();
            results.additionalResults["samples"] =
                static_cast<Real>(requiredSamples_);
        }

      private:
        static Real pricePath(
            const Path& path,
            const std::vector<Time>& obsTimes,
            const std::vector<DiscountFactor>& obsDfs,
            DiscountFactor maturityDf,
            const AutocallableNote::arguments& args) {
            const Real S0 = args.initialSpot;
            const Size nObs = obsTimes.size();

            // The grid was built from obsTimes, so index i+1 of the
            // path (path[0] is S0) corresponds to observation i.
            for (Size i = 0; i < nObs; ++i) {
                Real S_i = path[i + 1];
                if (S_i >= args.autocallBarriers[i] * S0) {
                    // Autocall: pay notional * (1 + coupon * (i+1))
                    // discounted to t_i.
                    Real periods = static_cast<Real>(i + 1);
                    return args.notional *
                           (1.0 + args.couponRate * periods) * obsDfs[i];
                }
            }
            // Never autocalled: maturity payoff.
            Real S_T = path[nObs];  // final observation == maturity
            Real ratio = S_T / S0;
            if (ratio >= args.protectionBarrier) {
                Real periods = static_cast<Real>(nObs);
                return args.notional *
                       (1.0 + args.couponRate * periods) * maturityDf;
            }
            return args.notional * ratio * maturityDf;
        }

        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Size requiredSamples_;
        BigNatural seed_;
    };

}

#endif
