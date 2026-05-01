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
#include <ql/marketdata/csvquoteloader.hpp>
#include <cstdio>
#include <fstream>
#include <string>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CsvQuoteLoaderTests)

namespace
{

    // Write the given body to a temp file and return its path.
    // Caller is responsible for std::remove()-ing it.
    std::string writeTempCsv(const std::string& body)
    {
        char tmpl[] = "/tmp/ql_csv_test_XXXXXX";
        int fd = mkstemp(tmpl);
        QL_REQUIRE(fd != -1, "mkstemp failed");
        std::string path(tmpl);
        close(fd);
        std::ofstream out(path);
        out << body;
        out.close();
        return path;
    }

}

BOOST_AUTO_TEST_CASE(testHappyPathRoundTrip)
{
    BOOST_TEST_MESSAGE("CsvQuoteLoader: simple id,value file loads into "
                       "SimpleQuote map...");

    std::string path = writeTempCsv("# sample market snapshot\n"
                                    "id,value\n"
                                    "USD3M,0.0452\n"
                                    "USD6M,0.0471\n"
                                    "EUR3M,0.0325\n"
                                    "\n" // blank line is ignored
                                    "GBP3M,0.0507\n");

    CsvQuoteLoader loader(path);
    BOOST_CHECK_EQUAL(loader.size(), Size(4));
    BOOST_CHECK(loader.has("USD3M"));
    BOOST_CHECK(!loader.has("USD1Y"));
    BOOST_CHECK_SMALL(std::fabs(loader["USD3M"]->value() - 0.0452), 1e-15);
    BOOST_CHECK_SMALL(std::fabs(loader["GBP3M"]->value() - 0.0507), 1e-15);
    std::remove(path.c_str());
}

BOOST_AUTO_TEST_CASE(testSimpleQuoteIsObservable)
{
    BOOST_TEST_MESSAGE("CsvQuoteLoader: returned SimpleQuote can be "
                       "re-valued, observer chain still works...");

    std::string path = writeTempCsv("USD3M,0.05\n");
    CsvQuoteLoader loader(path);
    auto q = loader["USD3M"];
    BOOST_REQUIRE(q);
    BOOST_CHECK_SMALL(std::fabs(q->value() - 0.05), 1e-15);
    // The loader returns the same SimpleQuote shared_ptr the map owns;
    // downstream setValue() notifies observers as usual.
    q->setValue(0.07);
    BOOST_CHECK_SMALL(std::fabs(loader["USD3M"]->value() - 0.07), 1e-15);
    std::remove(path.c_str());
}

BOOST_AUTO_TEST_CASE(testMalformedInputsAreRejected)
{
    BOOST_TEST_MESSAGE("CsvQuoteLoader: malformed rows throw with "
                       "file:line context...");

    // Missing comma
    std::string p1 = writeTempCsv("USD3M 0.05\n");
    auto call1 = [&]
    {
        CsvQuoteLoader l(p1);
        (void)l;
    };
    BOOST_CHECK_EXCEPTION(call1(), Error, ExpectedErrorMessage("missing comma separator"));
    std::remove(p1.c_str());

    // Non-numeric value
    std::string p2 = writeTempCsv("USD3M,not_a_number\n");
    auto call2 = [&]
    {
        CsvQuoteLoader l(p2);
        (void)l;
    };
    BOOST_CHECK_EXCEPTION(call2(), Error, ExpectedErrorMessage("could not parse"));
    std::remove(p2.c_str());

    // Duplicate id
    std::string p3 = writeTempCsv("USD3M,0.05\nUSD3M,0.06\n");
    auto call3 = [&]
    {
        CsvQuoteLoader l(p3);
        (void)l;
    };
    BOOST_CHECK_EXCEPTION(call3(), Error, ExpectedErrorMessage("duplicate id"));
    std::remove(p3.c_str());

    // Empty file
    std::string p4 = writeTempCsv("# only comments\n# and more\n");
    auto call4 = [&]
    {
        CsvQuoteLoader l(p4);
        (void)l;
    };
    BOOST_CHECK_EXCEPTION(call4(), Error, ExpectedErrorMessage("no quote rows"));
    std::remove(p4.c_str());
}

BOOST_AUTO_TEST_CASE(testUtf8BomStripped)
{
    BOOST_TEST_MESSAGE("CsvQuoteLoader: leading UTF-8 BOM (0xEF 0xBB 0xBF) "
                       "is stripped, first id is not corrupted...");

    // Files exported from Excel on Windows have a leading UTF-8 BOM.
    std::string bom = "\xEF\xBB\xBF";
    std::string path = writeTempCsv(bom + "USD3M,0.05\nEUR3M,0.03\n");
    CsvQuoteLoader loader(path);
    BOOST_CHECK(loader.has("USD3M"));
    BOOST_CHECK(!loader.has("\xEF\xBB\xBFUSD3M"));
    BOOST_CHECK_SMALL(std::fabs(loader["USD3M"]->value() - 0.05), 1e-15);
    std::remove(path.c_str());
}

BOOST_AUTO_TEST_CASE(testNonFiniteValueRejected)
{
    BOOST_TEST_MESSAGE("CsvQuoteLoader: Inf/NaN values are rejected with "
                       "a clear error naming the id...");

    std::string p1 = writeTempCsv("USD3M,inf\n");
    auto c1 = [&]
    {
        CsvQuoteLoader l(p1);
        (void)l;
    };
    BOOST_CHECK_EXCEPTION(c1(), Error, ExpectedErrorMessage("not finite"));
    std::remove(p1.c_str());

    std::string p2 = writeTempCsv("EUR3M,NaN\n");
    auto c2 = [&]
    {
        CsvQuoteLoader l(p2);
        (void)l;
    };
    BOOST_CHECK_EXCEPTION(c2(), Error, ExpectedErrorMessage("not finite"));
    std::remove(p2.c_str());
}

BOOST_AUTO_TEST_CASE(testLocaleIndependentParse)
{
    BOOST_TEST_MESSAGE("CsvQuoteLoader: '.' is always the decimal "
                       "separator regardless of LC_NUMERIC...");

    // from_chars ignores the locale; stod would fail here under
    // LC_NUMERIC=de_DE.UTF-8 for "0.05".
    std::string path = writeTempCsv("USD3M,0.12345\n");
    CsvQuoteLoader loader(path);
    BOOST_CHECK_SMALL(std::fabs(loader["USD3M"]->value() - 0.12345), 1e-15);

    // Comma-as-decimal must NOT be accepted.
    std::string bad = writeTempCsv("EUR3M,0,12345\n");
    auto call = [&]
    {
        CsvQuoteLoader l(bad);
        (void)l;
    };
    BOOST_CHECK_EXCEPTION(call(), Error, ExpectedErrorMessage("trailing non-numeric"));
    std::remove(path.c_str());
    std::remove(bad.c_str());
}

BOOST_AUTO_TEST_CASE(testMissingIdThrowsOnAccess)
{
    BOOST_TEST_MESSAGE("CsvQuoteLoader: operator[] throws on unknown id...");

    std::string path = writeTempCsv("USD3M,0.05\n");
    CsvQuoteLoader loader(path);
    auto call = [&]
    {
        auto q = loader["EUR3M"];
        (void)q;
    };
    BOOST_CHECK_EXCEPTION(call(), Error, ExpectedErrorMessage("no quote with id"));
    std::remove(path.c_str());
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
