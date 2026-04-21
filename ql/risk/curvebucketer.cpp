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

#include <ql/risk/curvebucketer.hpp>
#include <ql/errors.hpp>
#include <utility>

namespace QuantLib {

    namespace {
        // RAII guard: restore a SimpleQuote to its original value on
        // scope exit, even when an exception is thrown. Keeps the
        // observer graph clean for subsequent callers regardless of
        // what the pricing engine does under shock.
        class ScopedQuoteRestore {
          public:
            ScopedQuoteRestore(const ext::shared_ptr<SimpleQuote>& q)
            : quote_(q), original_(q->value()) {}
            ~ScopedQuoteRestore() { quote_->setValue(original_); }
            Real original() const { return original_; }
            // Non-copyable, non-movable
            ScopedQuoteRestore(const ScopedQuoteRestore&) = delete;
            ScopedQuoteRestore& operator=(const ScopedQuoteRestore&) = delete;
          private:
            const ext::shared_ptr<SimpleQuote>& quote_;
            Real original_;
        };
    }

    CurveBucketer::CurveBucketer(
        std::vector<ext::shared_ptr<SimpleQuote> > quotes, Real bump)
    : quotes_(std::move(quotes)), bump_(bump) {
        QL_REQUIRE(bump_ > 0.0,
                   "CurveBucketer bump size must be positive; got " << bump_);
        for (Size i = 0; i < quotes_.size(); ++i) {
            QL_REQUIRE(quotes_[i], "CurveBucketer: quote at index " << i
                                   << " is a null shared_ptr");
        }
    }

    std::vector<Real>
    CurveBucketer::bucketedDelta(const Instrument& instrument) const {
        std::vector<Real> result;
        result.reserve(quotes_.size());
        for (const auto& q : quotes_) {
            ScopedQuoteRestore guard(q);
            q->setValue(guard.original() + bump_);
            Real up = instrument.NPV();
            q->setValue(guard.original() - bump_);
            Real down = instrument.NPV();
            result.push_back((up - down) / (2.0 * bump_));
            // guard's destructor restores q -> original value
        }
        return result;
    }

    std::vector<Real>
    CurveBucketer::bucketedGamma(const Instrument& instrument) const {
        // Evaluate baseline once; the quote state is the original
        // during this call since no bucket has been mutated yet.
        Real mid = instrument.NPV();
        std::vector<Real> result;
        result.reserve(quotes_.size());
        const Real bump2 = bump_ * bump_;
        for (const auto& q : quotes_) {
            ScopedQuoteRestore guard(q);
            q->setValue(guard.original() + bump_);
            Real up = instrument.NPV();
            q->setValue(guard.original() - bump_);
            Real down = instrument.NPV();
            result.push_back((up - 2.0 * mid + down) / bump2);
        }
        return result;
    }

    Real CurveBucketer::parallelDelta(const Instrument& instrument) const {
        // Centered parallel shift: bump all quotes up, then all down,
        // then restore. Gives the sum of the bucketed deltas when the
        // instrument's NPV is locally linear in each quote, which is
        // the regime where bucketing is meaningful in the first place.
        std::vector<Real> originals;
        originals.reserve(quotes_.size());
        for (const auto& q : quotes_) originals.push_back(q->value());

        Real up = 0.0, down = 0.0;
        try {
            for (Size i = 0; i < quotes_.size(); ++i)
                quotes_[i]->setValue(originals[i] + bump_);
            up = instrument.NPV();
            for (Size i = 0; i < quotes_.size(); ++i)
                quotes_[i]->setValue(originals[i] - bump_);
            down = instrument.NPV();
        } catch (...) {
            for (Size i = 0; i < quotes_.size(); ++i)
                quotes_[i]->setValue(originals[i]);
            throw;
        }
        for (Size i = 0; i < quotes_.size(); ++i)
            quotes_[i]->setValue(originals[i]);
        return (up - down) / (2.0 * bump_);
    }

}
