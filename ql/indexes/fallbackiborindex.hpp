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

/*! \file fallbackiborindex.hpp
    \brief IBOR index with an ISDA 2020 Protocol fallback to an RFR
*/

#ifndef quantlib_fallback_ibor_index_hpp
#define quantlib_fallback_ibor_index_hpp

#include <ql/indexes/iborindex.hpp>

namespace QuantLib {

    //! IBOR index with an ISDA 2020 Protocol fallback.
    /*! Wraps a legacy IBOR index (e.g. USD LIBOR 3M) and an overnight
        replacement RFR (e.g. SOFR). Before the cessation date, forecasts
        delegate to the legacy index. On or after the cessation date,
        forecasts use the compounded RFR over the period implied by the
        legacy index's tenor, plus a fixed spread adjustment per the ISDA
        2020 IBOR Fallbacks Protocol.

        The compounding is computed from the RFR's discount curve:

            compoundedRate = (df(startDate) / df(endDate) - 1) / tau

        This is the simply-compounded rate implied by the overnight-rate
        curve over the period, which is mathematically the limit of the
        ISDA daily-compounded rate when fixings and curve agree. A future
        enhancement can add historical daily-fixing compounding for
        partially-observed periods.

        Typical construction (USD LIBOR 3M → SOFR + 26.161 bps):

        \code
        auto usdLibor3M = ext::make_shared<USDLibor>(3 * Months);
        auto sofr       = ext::make_shared<Sofr>(sofrCurveHandle);
        Date cessation(30, June, 2023);
        Spread isdaSpread = 0.0026161;
        auto fallback = ext::make_shared<FallbackIborIndex>(
            usdLibor3M, sofr, cessation, isdaSpread);
        \endcode

        \ingroup indexes
    */
    class FallbackIborIndex : public IborIndex {
      public:
        FallbackIborIndex(const ext::shared_ptr<IborIndex>& legacyIndex,
                          const ext::shared_ptr<OvernightIndex>& rfrIndex,
                          const Date& cessationDate,
                          Spread spreadAdjustment);

        //! \name Inspectors
        //@{
        const ext::shared_ptr<IborIndex>& legacyIndex() const {
            return legacyIndex_;
        }
        const ext::shared_ptr<OvernightIndex>& rfrIndex() const {
            return rfrIndex_;
        }
        const Date& cessationDate() const { return cessationDate_; }
        Spread spreadAdjustment() const { return spreadAdjustment_; }
        //@}

        //! \name IborIndex interface
        //@{
        Rate forecastFixing(const Date& fixingDate) const override;
        ext::shared_ptr<IborIndex> clone(
            const Handle<YieldTermStructure>& forwarding) const override;
        //@}

      private:
        ext::shared_ptr<IborIndex> legacyIndex_;
        ext::shared_ptr<OvernightIndex> rfrIndex_;
        Date cessationDate_;
        Spread spreadAdjustment_;

        Rate compoundedRfrRate(const Date& startDate,
                               const Date& endDate) const;
    };

}

#endif
