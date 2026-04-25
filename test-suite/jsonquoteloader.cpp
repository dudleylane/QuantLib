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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/marketdata/jsonquoteloader.hpp>
#include <cstdio>
#include <fstream>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(JsonQuoteLoaderTests)

namespace {
    std::string writeTemp(const std::string& body) {
        char tmpl[] = "/tmp/ql_json_test_XXXXXX";
        int fd = mkstemp(tmpl);
        QL_REQUIRE(fd != -1, "mkstemp failed");
        std::string path(tmpl);
        close(fd);
        std::ofstream out(path);
        out << body;
        return path;
    }
}

#ifdef QL_ENABLE_JSON_MARKETDATA

BOOST_AUTO_TEST_CASE(testFlatObjectSchema) {
    BOOST_TEST_MESSAGE("JsonQuoteLoader: schema A (flat id -> value)...");
    std::string p = writeTemp(R"({
        "USD3M": 0.0452,
        "USD6M": 0.0471,
        "EUR3M": 0.0325
    })");
    JsonQuoteLoader loader(p);
    BOOST_CHECK_EQUAL(loader.size(), Size(3));
    BOOST_CHECK(loader.has("USD3M"));
    BOOST_CHECK_SMALL(std::fabs(loader["USD6M"]->value() - 0.0471), 1e-15);
    std::remove(p.c_str());
}

BOOST_AUTO_TEST_CASE(testArraySchema) {
    BOOST_TEST_MESSAGE("JsonQuoteLoader: schema B (quotes array)...");
    std::string p = writeTemp(R"({"quotes": [
        {"id": "USD3M", "value": 0.05},
        {"id": "GBP1Y", "value": 0.06}
    ]})");
    JsonQuoteLoader loader(p);
    BOOST_CHECK_EQUAL(loader.size(), Size(2));
    BOOST_CHECK_SMALL(std::fabs(loader["USD3M"]->value() - 0.05), 1e-15);
    BOOST_CHECK_SMALL(std::fabs(loader["GBP1Y"]->value() - 0.06), 1e-15);
    std::remove(p.c_str());
}

BOOST_AUTO_TEST_CASE(testMalformedJsonRejected) {
    std::string p1 = writeTemp(R"({"USD3M": 0.05 "USD6M": 0.06})"); // no comma
    auto open1 = [&]{ JsonQuoteLoader l(p1); (void)l; };
    BOOST_CHECK_EXCEPTION(open1(), Error,
        ExpectedErrorMessage("parse error"));
    std::remove(p1.c_str());

    std::string p2 = writeTemp(R"({"USD3M": "not a number"})");
    auto open2 = [&]{ JsonQuoteLoader l(p2); (void)l; };
    BOOST_CHECK_EXCEPTION(open2(), Error,
        ExpectedErrorMessage("not a number"));
    std::remove(p2.c_str());

    std::string p3 = writeTemp("[]");  // top-level array rejected
    auto open3 = [&]{ JsonQuoteLoader l(p3); (void)l; };
    BOOST_CHECK_EXCEPTION(open3(), Error,
        ExpectedErrorMessage("top-level JSON must be an object"));
    std::remove(p3.c_str());
}

BOOST_AUTO_TEST_CASE(testAmbiguousSchemaRejected) {
    BOOST_TEST_MESSAGE("JsonQuoteLoader: a document mixing 'quotes' "
                       "array with top-level id:value siblings is "
                       "rejected (not silently partial)...");

    std::string p = writeTemp(R"({
        "quotes": [{"id": "USD3M", "value": 0.05}],
        "EUR3M": 0.03
    })");
    auto open = [&]{ JsonQuoteLoader l(p); (void)l; };
    BOOST_CHECK_EXCEPTION(open(), Error,
        ExpectedErrorMessage("ambiguous schema"));
    std::remove(p.c_str());
}

BOOST_AUTO_TEST_CASE(testEmptyIdRejected) {
    BOOST_TEST_MESSAGE("JsonQuoteLoader: empty id in either schema is "
                       "rejected...");

    // Schema B with empty id
    std::string pB = writeTemp(R"({"quotes": [{"id": "", "value": 0.05}]})");
    auto openB = [&]{ JsonQuoteLoader l(pB); (void)l; };
    BOOST_CHECK_EXCEPTION(openB(), Error, ExpectedErrorMessage("empty"));
    std::remove(pB.c_str());

    // Schema A with empty key
    std::string pA = writeTemp(R"({"": 0.05})");
    auto openA = [&]{ JsonQuoteLoader l(pA); (void)l; };
    BOOST_CHECK_EXCEPTION(openA(), Error, ExpectedErrorMessage("empty"));
    std::remove(pA.c_str());
}

BOOST_AUTO_TEST_CASE(testNonFiniteValueRejected) {
    BOOST_TEST_MESSAGE("JsonQuoteLoader: non-finite numbers rejected...");

    // nlohmann/json accepts non-standard literals like this only with
    // tolerant parsing; Inf/NaN round-trip through its number type on
    // some configurations. The isfinite guard catches them regardless.
    std::string p = writeTemp(R"({"quotes": [{"id": "X", "value": 1e400}]})");
    auto open = [&]{ JsonQuoteLoader l(p); (void)l; };
    // 1e400 overflows to inf; isfinite fires. If the parser rejects
    // before we get there, the error still thrown — accept either.
    BOOST_CHECK_THROW(open(), Error);
    std::remove(p.c_str());
}

BOOST_AUTO_TEST_CASE(testEmptyObjectRejected) {
    std::string p = writeTemp("{}");
    auto open = [&]{ JsonQuoteLoader l(p); (void)l; };
    BOOST_CHECK_EXCEPTION(open(), Error,
        ExpectedErrorMessage("no quote rows loaded"));
    std::remove(p.c_str());
}

#else // QL_ENABLE_JSON_MARKETDATA disabled

BOOST_AUTO_TEST_CASE(testDisabledBuildThrows) {
    std::string p = writeTemp(R"({"USD3M": 0.05})");
    auto open = [&]{ JsonQuoteLoader l(p); (void)l; };
    BOOST_CHECK_EXCEPTION(open(), Error,
        ExpectedErrorMessage("JSON market-data support is disabled"));
    std::remove(p.c_str());
}

#endif

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
