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

        // Schema detection: array form has {"quotes": [...]}, flat
        // form is a top-level object of id -> value pairs.
        if (doc.is_object() && doc.contains("quotes")
                && doc["quotes"].is_array()) {
            // Schema B.
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
                Real value = valNode.get<Real>();
                auto res = quotes_.emplace(
                    id, ext::make_shared<SimpleQuote>(value));
                QL_REQUIRE(res.second,
                           "JsonQuoteLoader: duplicate id '" << id << "'");
            }
        } else if (doc.is_object()) {
            // Schema A.
            for (auto it = doc.begin(); it != doc.end(); ++it) {
                QL_REQUIRE(it.value().is_number(),
                           "JsonQuoteLoader: value for id '" << it.key()
                           << "' is not a number");
                Real value = it.value().get<Real>();
                quotes_.emplace(
                    it.key(), ext::make_shared<SimpleQuote>(value));
                // Top-level object can't contain duplicate keys by
                // JSON spec, so no explicit duplicate check is needed
                // here — the parser already rejects or dedup's them
                // per the nlohmann/json convention.
            }
        } else {
            QL_FAIL("JsonQuoteLoader: top-level JSON must be an object; "
                    "got a different type in " << filepath);
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
