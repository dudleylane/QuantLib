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

#include <ql/risk/frtbsagirr.hpp>
#include <ql/errors.hpp>
#include <algorithm>
#include <cmath>
#include <utility>

namespace QuantLib {

    FrtbSaGirrDelta::FrtbSaGirrDelta(std::vector<Real> tenors,
                                     std::vector<Real> sensitivities,
                                     std::vector<Real> riskWeights,
                                     Real theta,
                                     Real phiFloor)
    : tenors_(std::move(tenors)),
      theta_(theta), phiFloor_(phiFloor) {

        QL_REQUIRE(!tenors_.empty(),
                   "FrtbSaGirrDelta: need at least one tenor");
        QL_REQUIRE(sensitivities.size() == tenors_.size(),
                   "FrtbSaGirrDelta: sensitivities size "
                   << sensitivities.size() << " != tenors size "
                   << tenors_.size());
        QL_REQUIRE(riskWeights.size() == tenors_.size(),
                   "FrtbSaGirrDelta: risk-weights size "
                   << riskWeights.size() << " != tenors size "
                   << tenors_.size());
        QL_REQUIRE(theta_ > 0.0,
                   "FrtbSaGirrDelta: theta must be positive, got " << theta_);
        QL_REQUIRE(phiFloor_ >= 0.0 && phiFloor_ <= 1.0,
                   "FrtbSaGirrDelta: phiFloor outside [0, 1]: "
                   << phiFloor_);
        for (Size i = 0; i < tenors_.size(); ++i) {
            QL_REQUIRE(tenors_[i] > 0.0,
                       "FrtbSaGirrDelta: non-positive tenor at index "
                       << i << ": " << tenors_[i]);
        }

        ws_.resize(tenors_.size());
        for (Size i = 0; i < tenors_.size(); ++i)
            ws_[i] = riskWeights[i] * sensitivities[i];
    }

    Real FrtbSaGirrDelta::correlation(Size i, Size j) const {
        if (i == j) return 1.0;
        const Real minT = std::min(tenors_[i], tenors_[j]);
        const Real dT = std::fabs(tenors_[i] - tenors_[j]);
        Real rho = std::exp(-theta_ * dT / minT);
        return std::max(rho, phiFloor_);
    }

    Real FrtbSaGirrDelta::bucketCharge() const {
        // K_b = sqrt( sum_i WS_i^2 + 2 * sum_{i<j} rho_{ij} * WS_i * WS_j )
        Real sumSq = 0.0;
        for (Real w : ws_) sumSq += w * w;

        Real cross = 0.0;
        const Size n = ws_.size();
        for (Size i = 0; i < n; ++i) {
            for (Size j = i + 1; j < n; ++j) {
                cross += correlation(i, j) * ws_[i] * ws_[j];
            }
        }
        // Defensive check: the (theta, phiFloor, tenors) correlation
        // matrix is not guaranteed positive-semidefinite across all
        // regimes. A negative aggregated inner value is almost always
        // inconsistent inputs (ill-posed correlation), not a valid
        // capital charge — reject with a pointed message rather than
        // silently clamp, so the caller learns of the misconfiguration.
        Real inner = sumSq + 2.0 * cross;
        QL_REQUIRE(inner >= 0.0,
                   "FrtbSaGirrDelta: aggregated inner value " << inner
                   << " is negative; correlation matrix likely not "
                   "positive-semidefinite for the supplied theta / "
                   "phiFloor / tenor combination");
        return std::sqrt(inner);
    }

}
