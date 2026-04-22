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

/*! \file jsonquoteloader.hpp
    \brief JSON market-data snapshot loader

    Companion to `CsvQuoteLoader` that accepts JSON instead of CSV.
    Two schemas are supported (the loader auto-detects):

    Schema A — flat object form, one key per quote:
    \code
      {
        "USD3M": 0.0452,
        "USD6M": 0.0471,
        "EUR3M": 0.0325
      }
    \endcode

    Schema B — array form, with richer structure room for follow-ups
    (bid / ask / mid columns, timestamps, etc. — the MVP still only
    reads `id` and `value`):
    \code
      { "quotes": [
          {"id": "USD3M", "value": 0.0452},
          {"id": "USD6M", "value": 0.0471}
      ] }
    \endcode

    Gated on CMake option `QL_ENABLE_JSON_MARKETDATA` (default ON).
    Uses nlohmann/json v3 for parsing. When the flag is off the
    header still compiles; the constructor throws at runtime with a
    pointer to the CMake option.

    Out of scope: streaming updates, Arrow / Parquet, vendor-specific
    formats (Bloomberg BVAL, Refinitiv Eikon, ICE market-data),
    multi-column quotes (bid/ask/mid), timestamp-aware snapshots.
    Each is a follow-up that sits on top of this primitive.
*/

#ifndef quantlib_json_quote_loader_hpp
#define quantlib_json_quote_loader_hpp

#include <ql/quotes/simplequote.hpp>
#include <map>
#include <string>

namespace QuantLib {

    //! JSON-backed market quote snapshot.
    class JsonQuoteLoader {
      public:
        //! Construct from a path to a JSON file. See class doc for the
        //  two supported schemas. Throws on parse error, malformed
        //  entries, duplicate ids, or file-open failure.
        explicit JsonQuoteLoader(const std::string& filepath);

        //! Access by id; throws if unknown.
        const ext::shared_ptr<SimpleQuote>&
        operator[](const std::string& id) const;

        bool has(const std::string& id) const;

        const std::map<std::string, ext::shared_ptr<SimpleQuote> >&
        quotes() const { return quotes_; }

        Size size() const { return quotes_.size(); }

      private:
        std::map<std::string, ext::shared_ptr<SimpleQuote> > quotes_;
    };

}

#endif
