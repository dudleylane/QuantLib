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

#include <ql/marketdata/csvquoteloader.hpp>
#include <ql/errors.hpp>
#include <fstream>
#include <sstream>

namespace QuantLib {

    namespace {
        // Trim leading/trailing whitespace in place.
        void trim(std::string& s) {
            const char* ws = " \t\r\n";
            auto first = s.find_first_not_of(ws);
            if (first == std::string::npos) { s.clear(); return; }
            auto last = s.find_last_not_of(ws);
            s = s.substr(first, last - first + 1);
        }
    }

    CsvQuoteLoader::CsvQuoteLoader(const std::string& filepath) {
        std::ifstream in(filepath);
        QL_REQUIRE(in.is_open(),
                   "CsvQuoteLoader: cannot open file " << filepath);

        std::string line;
        Size lineNo = 0;
        bool sawFirstData = false;
        while (std::getline(in, line)) {
            ++lineNo;

            std::string trimmed = line;
            trim(trimmed);
            if (trimmed.empty()) continue;
            if (trimmed.front() == '#') continue;

            std::string id, valueStr;
            auto comma = trimmed.find(',');
            QL_REQUIRE(comma != std::string::npos,
                       "CsvQuoteLoader: " << filepath << ":" << lineNo
                       << " missing comma separator in '" << line << "'");
            id = trimmed.substr(0, comma);
            valueStr = trimmed.substr(comma + 1);
            trim(id);
            trim(valueStr);

            // Allow a single header row of literal 'id,value'.
            if (!sawFirstData && id == "id" && valueStr == "value") {
                continue;
            }
            sawFirstData = true;

            QL_REQUIRE(!id.empty(),
                       "CsvQuoteLoader: " << filepath << ":" << lineNo
                       << " empty id field");
            QL_REQUIRE(!valueStr.empty(),
                       "CsvQuoteLoader: " << filepath << ":" << lineNo
                       << " empty value field for id '" << id << "'");

            Real value = 0.0;
            try {
                size_t pos = 0;
                value = std::stod(valueStr, &pos);
                // Anything after the number must be trailing whitespace.
                std::string rest = valueStr.substr(pos);
                trim(rest);
                QL_REQUIRE(rest.empty(),
                           "trailing non-numeric characters '" << rest << "'");
            } catch (const std::exception& e) {
                QL_FAIL("CsvQuoteLoader: " << filepath << ":" << lineNo
                        << " could not parse '" << valueStr
                        << "' as a number: " << e.what());
            }

            auto result = quotes_.emplace(
                id, ext::make_shared<SimpleQuote>(value));
            QL_REQUIRE(result.second,
                       "CsvQuoteLoader: " << filepath << ":" << lineNo
                       << " duplicate id '" << id << "'");
        }

        QL_REQUIRE(!quotes_.empty(),
                   "CsvQuoteLoader: no quote rows found in " << filepath);
    }

    const ext::shared_ptr<SimpleQuote>&
    CsvQuoteLoader::operator[](const std::string& id) const {
        auto it = quotes_.find(id);
        QL_REQUIRE(it != quotes_.end(),
                   "CsvQuoteLoader: no quote with id '" << id << "'");
        return it->second;
    }

    bool CsvQuoteLoader::has(const std::string& id) const {
        return quotes_.find(id) != quotes_.end();
    }

}
