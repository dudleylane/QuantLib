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

/*! \file csvquoteloader.hpp
    \brief Minimal CSV loader for named market quotes

    MVP scope: read a CSV file in which each row is an
    instrument-id / rate pair, produce a name -> SimpleQuote map for
    downstream curve bootstrapping. File format:

      # optional comment lines start with '#'
      id,value
      USD3MLIB,0.0452
      USD6MLIB,0.0471
      USD1Y,0.0488
      ...

    A loader instance can also be asked for a specific quote by name,
    which returns the SimpleQuote shared_ptr suitable for handing to
    a Handle<Quote> / RateHelper pipeline.

    Out of scope (follow-ups): multi-column quotes (bid/ask/mid),
    tenor-encoded parsing, vendor formats (Bloomberg BVAL, Refinitiv
    Eikon, CME), arrow/parquet, snapshot chaining across dates,
    timestamped streaming loaders.
*/

#ifndef quantlib_csv_quote_loader_hpp
#define quantlib_csv_quote_loader_hpp

#include <ql/quotes/simplequote.hpp>
#include <map>
#include <string>

namespace QuantLib {

    //! CSV-backed market quote snapshot.
    /*! \ingroup marketdata */
    class CsvQuoteLoader {
      public:
        //! Construct from a path to a CSV file.
        /*! The file is read once at construction. Lines whose first
            non-whitespace character is '#' are treated as comments
            and skipped; a header line `id,value` is optional. Blank
            lines are ignored. All other non-comment lines must have
            exactly two comma-separated fields; the second must parse
            as a `Real`. Throws on malformed data. */
        explicit CsvQuoteLoader(const std::string& filepath);

        //! Access a quote by its instrument id.
        /*! Throws if the id is not present in the snapshot. */
        const ext::shared_ptr<SimpleQuote>&
        operator[](const std::string& id) const;

        //! Test whether an id is present.
        bool has(const std::string& id) const;

        //! Full quote map. The caller can iterate / filter as needed.
        const std::map<std::string, ext::shared_ptr<SimpleQuote> >&
        quotes() const { return quotes_; }

        //! Number of quotes loaded.
        Size size() const { return quotes_.size(); }

      private:
        std::map<std::string, ext::shared_ptr<SimpleQuote> > quotes_;
    };

}

#endif
