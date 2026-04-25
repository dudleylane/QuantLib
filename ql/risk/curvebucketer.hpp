/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2026 dudleylane

 This file is new in the dudleylane fork of QuantLib and is licensed
 under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
 See the LICENSE file in the repository root for the full license text.

 Unlike upstream QuantLib files (which remain under BSD-3-Clause — see
 LICENSE.TXT), this file and any derivative works of it are subject to
 the copyleft and network-use clauses of AGPL-3.0.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file curvebucketer.hpp
    \brief Tenor-bucketed sensitivities for quote-driven curves.

    MVP scope: given a vector of `SimpleQuote` handles used to build a
    curve the instrument depends on, return the sensitivity of the
    instrument's NPV to a bump of each quote in isolation. Works for any
    `Instrument` that is registered (via the standard observer chain)
    with the curve built from those quotes.

    Out of scope for this first-cut: vol-surface (2D) bucketing,
    cross-gamma, multi-curve segregation, scenario generation,
    regulatory-grade FRTB / SIMM sensitivities. Those build on top of
    this primitive.
*/

#ifndef quantlib_curve_bucketer_hpp
#define quantlib_curve_bucketer_hpp

#include <ql/instrument.hpp>
#include <ql/quotes/simplequote.hpp>
#include <vector>

namespace QuantLib {

    //! Tenor-bucketed delta/gamma for an instrument priced off a curve.
    /*! \ingroup riskanalysis */
    /*! The caller supplies the `SimpleQuote` objects that seed the curve
        (deposit/future/swap quotes, vol quotes, etc.). CurveBucketer
        centered-differences each quote, one at a time, repricing the
        instrument through its pricing engine for each shock.

        Observer-graph correctness: setting a SimpleQuote's value calls
        `notifyObservers()`, which cascades to the term structure and
        transitively to the instrument (a `LazyObject`). The instrument's
        `update()` invalidates its cached NPV. The next `NPV()` call
        triggers a full recalculation through the engine, which reads the
        bumped curve. Quotes are restored to their original values after
        each bucket (and after any thrown exception), leaving the
        instrument's observer state clean for subsequent callers.

        Not thread-safe: quote mutation is shared state.
    */
    class CurveBucketer {
      public:
        //! Construct from the vector of input quotes and a bump size.
        /*! Default bump is 1 basis point (1e-4). Regulatory sensitivity
            conventions (FRTB, SIMM) typically use 1bp for IR, 1% for
            equity, 1 vol point (0.01) for vols — the caller knows the
            right size for the underlying market. */
        CurveBucketer(std::vector<ext::shared_ptr<SimpleQuote> > quotes,
                      Real bump = 1.0e-4);

        //! Bucketed delta: one entry per quote.
        /*! \param instrument  the instrument to shock; its pricing
                engine must be attached to the same curve the input
                quotes feed.
            \return vector of centered-difference deltas, one per
                input quote: `(NPV(q_i + bump) - NPV(q_i - bump)) /
                (2 * bump)`.
            Quote state is restored on return (even on exception). */
        std::vector<Real> bucketedDelta(const Instrument& instrument) const;

        //! Bucketed gamma (diagonal only): one entry per quote.
        /*! \param instrument  see `bucketedDelta` above.
            \return vector of diagonal gammas, one per input quote:
                `(NPV(q_i + bump) - 2·NPV(q_i) + NPV(q_i - bump)) /
                bump^2`.
            Cross-gamma (off-diagonal) is not covered by this MVP; it
            would require O(n^2) repricings and a 2D output. */
        std::vector<Real> bucketedGamma(const Instrument& instrument) const;

        //! Parallel-shift delta: every input quote shocked in lock-step.
        /*! \param instrument  see `bucketedDelta` above.
            \return `(NPV(all quotes + bump) - NPV(all quotes - bump))
                / (2 * bump)`.
            For a locally linear instrument this equals `sum(bucketed\
            Delta)`; offered as a sanity check for bucketing
            consistency. */
        Real parallelDelta(const Instrument& instrument) const;

        //! Number of buckets (== number of input quotes).
        Size size() const { return quotes_.size(); }

        //! Bump size used by this bucketer.
        Real bump() const { return bump_; }

      private:
        std::vector<ext::shared_ptr<SimpleQuote> > quotes_;
        Real bump_;
    };

}

#endif
