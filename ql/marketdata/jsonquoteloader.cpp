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

#include <ql/marketdata/jsonquoteloader.hpp>
#include <ql/errors.hpp>
#include <cmath>
#include <fstream>

#ifdef QL_ENABLE_JSON_MARKETDATA
#include <nlohmann/json.hpp>
#endif

namespace QuantLib {

#ifdef QL_ENABLE_JSON_MARKETDATA

    JsonQuoteLoader::JsonQuoteLoader(const std::string& filepath) {
        std::ifstream in(filepath);
        QL_REQUIRE(in.is_open(),
                   "JsonQuoteLoader: cannot open file " << filepath);

        nlohmann::json doc;
        try {
            in >> doc;
        } catch (const std::exception& e) {
            QL_FAIL("JsonQuoteLoader: parse error in " << filepath
                    << ": " << e.what());
        }

        // Schema detection: Schema B has {"quotes": [...]}; Schema A
        // is a top-level object of id -> value pairs.
        QL_REQUIRE(doc.is_object(),
                   "JsonQuoteLoader: top-level JSON must be an object in "
                   << filepath);

        const bool hasQuotesArray = doc.contains("quotes")
            && doc["quotes"].is_array();

        if (hasQuotesArray) {
            // Schema B must not be mixed with Schema-A-style siblings;
            // a hybrid document is ambiguous and almost certainly a
            // construction error.
            for (auto it = doc.begin(); it != doc.end(); ++it) {
                QL_REQUIRE(it.key() == "quotes",
                           "JsonQuoteLoader: ambiguous schema in "
                           << filepath << ": 'quotes' array is present "
                           "alongside top-level key '" << it.key()
                           << "'. Supply one or the other, not both.");
            }
            for (const auto& entry : doc["quotes"]) {
                QL_REQUIRE(entry.is_object(),
                           "JsonQuoteLoader: 'quotes' array must contain objects");
                QL_REQUIRE(entry.contains("id") && entry.contains("value"),
                           "JsonQuoteLoader: quote entry missing 'id' or 'value'");
                const auto& idNode = entry["id"];
                const auto& valNode = entry["value"];
                QL_REQUIRE(idNode.is_string(),
                           "JsonQuoteLoader: 'id' must be a string");
                QL_REQUIRE(valNode.is_number(),
                           "JsonQuoteLoader: 'value' must be a number for id '"
                           << idNode.get<std::string>() << "'");
                std::string id = idNode.get<std::string>();
                QL_REQUIRE(!id.empty(),
                           "JsonQuoteLoader: empty 'id' in " << filepath);
                Real value = valNode.get<Real>();
                QL_REQUIRE(std::isfinite(value),
                           "JsonQuoteLoader: value for id '" << id
                           << "' is not finite (got " << value << ") in "
                           << filepath);
                auto res = quotes_.emplace(
                    id, ext::make_shared<SimpleQuote>(value));
                QL_REQUIRE(res.second,
                           "JsonQuoteLoader: duplicate id '" << id << "'");
            }
        } else {
            // Schema A.
            for (auto it = doc.begin(); it != doc.end(); ++it) {
                QL_REQUIRE(!it.key().empty(),
                           "JsonQuoteLoader: empty key in " << filepath);
                QL_REQUIRE(it.value().is_number(),
                           "JsonQuoteLoader: value for id '" << it.key()
                           << "' is not a number");
                Real value = it.value().get<Real>();
                QL_REQUIRE(std::isfinite(value),
                           "JsonQuoteLoader: value for id '" << it.key()
                           << "' is not finite (got " << value << ") in "
                           << filepath);
                quotes_.emplace(
                    it.key(), ext::make_shared<SimpleQuote>(value));
                // Top-level object duplicate keys are handled by
                // nlohmann/json at parse time per its convention.
            }
        }

        QL_REQUIRE(!quotes_.empty(),
                   "JsonQuoteLoader: no quote rows loaded from " << filepath);
    }

    const ext::shared_ptr<SimpleQuote>&
    JsonQuoteLoader::operator[](const std::string& id) const {
        auto it = quotes_.find(id);
        QL_REQUIRE(it != quotes_.end(),
                   "JsonQuoteLoader: no quote with id '" << id << "'");
        return it->second;
    }

    bool JsonQuoteLoader::has(const std::string& id) const {
        return quotes_.find(id) != quotes_.end();
    }

#else // QL_ENABLE_JSON_MARKETDATA not defined

    JsonQuoteLoader::JsonQuoteLoader(const std::string& /*filepath*/) {
        QL_FAIL("JsonQuoteLoader: JSON market-data support is disabled. "
                "Reconfigure with -DQL_ENABLE_JSON_MARKETDATA=ON (default ON) "
                "to link nlohmann/json and compile the loader.");
    }

    const ext::shared_ptr<SimpleQuote>&
    JsonQuoteLoader::operator[](const std::string& /*id*/) const {
        QL_FAIL("JsonQuoteLoader is disabled: rebuild with "
                "-DQL_ENABLE_JSON_MARKETDATA=ON.");
    }

    bool JsonQuoteLoader::has(const std::string& /*id*/) const {
        return false;
    }

#endif // QL_ENABLE_JSON_MARKETDATA

}
