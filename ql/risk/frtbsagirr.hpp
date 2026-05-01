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

/*! \file frtbsagirr.hpp
    \brief FRTB Standardised Approach — GIRR delta bucket charge (MVP)

    Implements the aggregation formula for a single General Interest-Rate
    Risk (GIRR) delta bucket under BCBS d457 (FRTB-SA):

      K_b = sqrt( sum_i WS_i^2 + 2 * sum_{i<j} rho_{ij} * WS_i * WS_j )

    where:
      WS_i = w_i * s_i         (weighted sensitivity, with risk weight w_i)
      s_i                       (net delta sensitivity at tenor T_i)
      rho_{ij} = max(exp(-theta * |T_i - T_j| / min(T_i, T_j)), phi_floor)

    Typical theta = 3% (0.03) and phi_floor = 40% (0.40) per the
    published GIRR correlation formula in d457. Both are exposed as
    constructor parameters because BCBS periodically revises them.

    ### What this MVP is

    - The **algorithmic shell**: tenor × sensitivity aggregation with
      the standard intra-bucket correlation formula.
    - Accepts the **risk weights and the (theta, phi_floor) parameters
      from the caller**. The canonical values from d457 are documented
      in the regression test, not hardcoded, because BCBS updates the
      numbers and users must match their jurisdiction's latest CRR /
      PRA / FCA implementation.
    - Composes naturally with `CurveBucketer` from the same
      subdirectory: feed its `bucketedDelta` output in as `s`.

    ### What this MVP explicitly is not

    - Other GIRR components: vega, curvature, cross-currency basis,
      inflation curves.
    - Other risk classes: credit-spread (CSR non-sec / sec / corr),
      equity, commodity, FX.
    - Cross-bucket aggregation with gamma_bc correlations.
    - Diversified vs. undiversified charge selection; default
      liquidity-horizon scaling.

    Those are all follow-ups that extend this primitive without
    invalidating it.
*/

#ifndef quantlib_frtb_sa_girr_hpp
#define quantlib_frtb_sa_girr_hpp

#include <ql/types.hpp>
#include <vector>

namespace QuantLib
{

    //! FRTB-SA GIRR delta bucket charge calculator (single currency).
    /*! \ingroup riskanalysis */
    class FrtbSaGirrDelta
    {
      public:
        /*! \param tenors      strictly-positive vertex tenors in years,
                                one per sensitivity.
            \param sensitivities one sensitivity per tenor (same size),
                                typically produced by CurveBucketer.
            \param riskWeights  one risk weight per tenor. Interpreted as
                                absolute decimals (e.g. 0.017 for 1.7%),
                                not basis points.
            \param theta        the exponent coefficient for the intra-
                                bucket correlation; the BCBS d457
                                default is 0.03.
            \param phiFloor     lower floor on the correlation; the
                                d457 default is 0.40.
        */
        FrtbSaGirrDelta(std::vector<Real> tenors,
                        std::vector<Real> sensitivities,
                        std::vector<Real> riskWeights,
                        Real theta = 0.03,
                        Real phiFloor = 0.40);

        //! Bucket charge K_b per the d457 delta formula.
        Real bucketCharge() const;

        //! Weighted-sensitivity vector WS_i = w_i * s_i.
        const std::vector<Real>& weightedSensitivities() const { return ws_; }

        //! Correlation matrix element rho_{ij}.
        /*! \param i  tenor index; must be less than `size()`.
            \param j  tenor index; must be less than `size()`.
            Returns 1.0 on the diagonal; otherwise the d457 formula
            `max(exp(-theta * |T_i - T_j| / min(T_i, T_j)), phiFloor)`. */
        Real correlation(Size i, Size j) const;

        Size size() const { return tenors_.size(); }

      private:
        std::vector<Real> tenors_;
        std::vector<Real> ws_; // precomputed w_i * s_i
        Real theta_;
        Real phiFloor_;
    };

}

#endif
