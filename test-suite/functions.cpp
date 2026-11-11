/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2014 Klaus Spanderen
 Copyright (C) 2015 Johannes Göttker-Schnetmann

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/math/abcdmathfunction.hpp>
#include <ql/math/comparison.hpp>
#include <ql/math/distributions/gammadistribution.hpp>
#include <ql/math/expm1.hpp>
#include <ql/math/factorial.hpp>
#include <ql/math/modifiedbessel.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

using std::exp;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(FunctionsTests)

BOOST_AUTO_TEST_CASE(testFactorial)
{

    BOOST_TEST_MESSAGE("Testing factorial numbers...");

    Real expected = 1.0;
    Real calculated = Factorial::get(0);
    if (calculated != expected)
        BOOST_FAIL("Factorial(0) = " << calculated);

    for (Natural i = 1; i < 171; ++i)
    {
        expected *= i;
        calculated = Factorial::get(i);
        // Above 27, get(i) is exp(logValue(i + 1)), whose relative error grows with ln i!, and the running product's
        // roundings add up to i eps / 2.
        if (std::fabs(calculated - expected) / expected > (4 * std::log(expected) + i) * QL_EPSILON)
            BOOST_FAIL("Factorial(" << i << ")" << std::setprecision(16) << std::scientific
                                    << "\n calculated: " << calculated << "\n   expected: " << expected
                                    << "\n rel. error: " << std::fabs(calculated - expected) / expected);
    }
}

BOOST_AUTO_TEST_CASE(testGammaFunction)
{

    BOOST_TEST_MESSAGE("Testing Gamma function...");

    Real expected = 0.0;
    Real calculated = GammaFunction().logValue(1);
    if (std::fabs(calculated) > 1.0e-15)
        BOOST_ERROR("GammaFunction(1)\n"
                    << std::setprecision(16) << std::scientific << "    calculated: " << calculated << "\n"
                    << "    expected:   " << expected);

    for (Size i = 2; i < 9000; i++)
    {
        expected += std::log(Real(i));
        calculated = GammaFunction().logValue(static_cast<Real>(i + 1));
        if (std::fabs(calculated - expected) / expected > 1.0e-9)
            BOOST_ERROR("GammaFunction(" << i << ")\n"
                                         << std::setprecision(16) << std::scientific << "    calculated: " << calculated
                                         << "\n"
                                         << "    expected:   " << expected << "\n"
                                         << "    rel. error: " << std::fabs(calculated - expected) / expected);
    }
}

BOOST_AUTO_TEST_CASE(testGammaValues)
{

    BOOST_TEST_MESSAGE("Testing Gamma values...");

    // reference results are calculated with R
    Real tasks[][3] = {{0.0001, 9999.422883231624, 1e3},         {1.2, 0.9181687423997607, 1e3},
                       {7.3, 1271.4236336639089586, 1e3},        {-1.1, 9.7148063829028946, 1e3},
                       {-4.001, -41.6040228304425312, 1e3},      {-4.999, -8.347576090315059, 1e3},
                       {-19.000001, 8.220610833201313e-12, 1e8}, {-19.5, 5.811045977502255e-18, 1e3},
                       {-21.000001, 1.957288098276488e-14, 1e8}, {-21.5, 1.318444918321553e-20, 1e6}};

    for (auto& task : tasks)
    {
        const Real x = task[0];
        const Real expected = task[1];
        const Real calculated = GammaFunction().value(x);
        const Real tol = task[2] * QL_EPSILON * std::fabs(expected);

        if (std::fabs(calculated - expected) > tol)
        {
            BOOST_ERROR("GammaFunction(" << x << ")\n"
                                         << std::setprecision(16) << std::scientific << "    calculated: " << calculated
                                         << "\n"
                                         << "    expected:   " << expected << "\n"
                                         << "    rel. error: " << std::fabs(calculated - expected) / expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testLogGammaReferenceValues)
{
    BOOST_TEST_MESSAGE("Testing ln Gamma against reference values...");

    // ln Gamma(x) at 50 digits (mpmath), rounded to the nearest double, on both sides of its zeros at 1 and 2 and on
    // the points of #150.  The six-term fit logValue() used before was off by 8.8e-13 at 11 and 1.9e-10 at 10001.
    const Real logGammas[][2] = {
        {1e-10, 23.025850929882736},     {0.001, 6.907178885383853},      {0.1, 2.252712651734206},
        {0.5, 0.5723649429247001},       {0.9, 0.06637623973474295},      {0.999, 0.0005780385328913802},
        {1.001, -0.0005763935982833062}, {1.1, -0.049872441259839764},    {1.5, -0.12078223763524522},
        {1.9, -0.03898427592308336},     {1.999, -0.0004224618006921073}, {2.001, 0.000423106734800117},
        {2.1, 0.04543773854448518},      {2.5, 0.2846828704729192},       {3.7, 1.428072326665388},
        {6.0, 4.787491742782046},        {8.5, 9.549267257300997},        {11.0, 15.104412573075516},
        {13.0, 19.987214495661885},      {16.0, 27.89927138384089},       {21.0, 42.335616460753485},
        {31.0, 74.65823634883016},       {51.0, 148.47776695177302},      {101.0, 363.73937555556347},
        {501.0, 2611.330458460156},      {10001.0, 82108.92783681436},    {100001.0, 1051299.2218991218},
        {1000001.0, 12815518.384658169}, {1e10, 220258509288.81058},      {1e100, 2.2925850929940456e+102},
        {1e300, 6.897755278982137e+302}};
    for (const auto& [x, expected] : logGammas)
    {
        BOOST_TEST_CONTEXT("ln Gamma(" << x << ")")
        {
            BOOST_CHECK_SMALL(GammaFunction().logValue(x) - expected,
                              4 * QL_EPSILON * std::max(1.0, std::fabs(expected)));
        }
    }

    // Gamma(x) at 50 digits.  value() is exp(logValue(x)) here, so its relative error grows with ln Gamma(x).
    const Real gammas[][2] = {
        {30.5, 4.822696933490909e+31}, {100.5, 9.320963104082716e+156}, {170.5, 5.56209241456e+305}};
    for (const auto& [x, expected] : gammas)
    {
        BOOST_TEST_CONTEXT("Gamma(" << x << ")")
        {
            BOOST_CHECK_SMALL(GammaFunction().value(x) / expected - 1.0, 4 * QL_EPSILON * std::log(expected));
        }
    }
}

BOOST_AUTO_TEST_CASE(testModifiedBesselFunctions)
{
    BOOST_TEST_MESSAGE("Testing modified Bessel function of first and second kind...");

    /* reference values are computed with R and the additional package Bessel
     * http://cran.r-project.org/web/packages/Bessel
     */

    Real r[][4] = {
        {-1.3, 2.0, 1.2079888436539505, 0.1608243636110430},      {1.3, 2.0, 1.2908192151358788, 0.1608243636110430},
        {0.001, 2.0, 2.2794705965773794, 0.1138938963603362},     {1.2, 0.5, 0.1768918783499572, 2.1086579232338192},
        {2.3, 0.1, 0.00037954958988425198, 572.096866928290183},  {-2.3, 1.1, 1.07222017902746969, 1.88152553684107371},
        {-10.0001, 1.1, 13857.7715614282552, 69288858.9474423379}};

    for (auto& i : r)
    {
        const Real nu = i[0];
        const Real x = i[1];
        const Real expected_i = i[2];
        const Real expected_k = i[3];
        const Real tol_i = 5e4 * QL_EPSILON * std::fabs(expected_i);
        const Real tol_k = 5e4 * QL_EPSILON * std::max(std::fabs(expected_k), std::fabs(expected_i));

        const Real calculated_i = modifiedBesselFunction_i(nu, x);
        const Real calculated_k = modifiedBesselFunction_k(nu, x);

        if (std::fabs(expected_i - calculated_i) > tol_i)
        {
            BOOST_ERROR("failed to reproduce modified Bessel "
                        << "function of first kind" << std::setprecision(16) << std::scientific
                        << "\n order     : " << nu << "\n argument  : " << x << "\n calculated: " << calculated_i
                        << "\n expected  : " << expected_i << "\n difference: " << std::fabs(expected_i - calculated_i)
                        << "\n tolerance : " << tol_i);
        }
        if (std::fabs(expected_k - calculated_k) > tol_k)
        {
            BOOST_ERROR("failed to reproduce modified Bessel "
                        << "function of second kind" << std::setprecision(16) << std::scientific
                        << "\n order     : " << nu << "\n argument  : " << x << "\n calculated: " << calculated_k
                        << "\n expected  : " << expected_k << "\n difference: " << std::fabs(expected_k - calculated_k)
                        << "\n tolerance : " << tol_k);
        }
    }

    Real c[][7] = {
        {-1.3, 2.0, 0.0, 1.2079888436539505, 0.0, 0.1608243636110430, 0.0},
        {1.2, 1.5, 0.3, 0.7891550871263575, 0.2721408731632123, 0.275126507673411, -0.1316314405663727},
        {1.2, -1.5, 0.0, -0.6650597524355781, -0.4831941938091643, -0.251112360556051, -2.400130904230102},
        {-11.2, 1.5, 0.3, 12780719.20252659, 16401053.26770633, -34155172.65672453, -43830147.36759921},
        {1.2, -1.5, 2.0, -0.3869803778520574, 0.9756701796853728, -3.111629716783005, 0.6307859871879062},
        {1.2, 0.0, 9.9999, -0.03507838078252647, 0.1079601550451466, -0.05979939995451453, 0.3929814473878203},
        {1.2, 0.0, 10.1, -0.02782046891519293, 0.08562259917678558, -0.02035685034691133, 0.3949834389686676},
        {1.2, 0.0, 12.1, 0.07092110620741207, -0.2182727210128104, 0.3368505862966958, -0.1299038064313366},
        {1.2, 0.0, 14.1, -0.03014378676768797, 0.09277303628303372, -0.237531022649052, -0.2351923034581644},
        {1.2, 0.0, 16.1, -0.03823210284792657, 0.1176663135266562, -0.1091239402448228, 0.2930535651966139},
        {1.2, 0.0, 18.1, 0.05626742394733754, -0.173173324361983, 0.2941636588154642, -0.02023355577954348},
        {1.2, 0.0, 180.1, -0.001230682086826484, 0.003787649998122361, 0.02284509628723454, 0.09055419580980778},
        {1.2, 0.0, 21.0, -0.04746415965014021, 0.1460796627610969, -0.2693825171336859, -0.04830804448126782},
        {1.2, 10.0, 0.0, 2609.784936867044, 0, 1.904394919838336e-05, 0},
        {1.2, 14.0, 0.0, 122690.4873454286, 0, 2.902060692576643e-07, 0},
        {1.2, 20.0, 10.0, -37452017.91168936, -13917587.22151363, -3.821534367487143e-10, 4.083211255351664e-10},
        {1.2, 9.0, 9.0, -621.7335051293694, 618.1455736670332, -4.480795479964915e-05, -3.489034389148745e-08}};

    for (auto& i : c)
    {
        const Real nu = i[0];
        const std::complex<Real> z = std::complex<Real>(i[1], i[2]);
        const std::complex<Real> expected_i = std::complex<Real>(i[3], i[4]);
        const std::complex<Real> expected_k = std::complex<Real>(i[5], i[6]);

        const Real tol_i = 5e4 * QL_EPSILON * std::abs(expected_i);
        const Real tol_k = 1e6 * QL_EPSILON * std::abs(expected_k);

        const std::complex<Real> calculated_i = modifiedBesselFunction_i(nu, z);
        const std::complex<Real> calculated_k = modifiedBesselFunction_k(nu, z);

        if (std::abs(expected_i - calculated_i) > tol_i)
        {
            BOOST_ERROR("failed to reproduce modified Bessel " << "function of first kind"
                                                               << "\n order     : " << nu << "\n argument  : " << z
                                                               << "\n calculated: " << calculated_i
                                                               << "\n expected  : " << expected_i);
        }
        if (std::abs(expected_k) > 1e-4 // do not check small values
            && std::abs(expected_k - calculated_k) > tol_k)
        {
            BOOST_ERROR("failed to reproduce modified Bessel " << "function of second kind"
                                                               << "\n order     : " << nu << "\n argument  : " << z
                                                               << "\n diff      : " << calculated_k - expected_k
                                                               << "\n calculated: " << calculated_k
                                                               << "\n expected  : " << expected_k);
        }
    }
}

BOOST_AUTO_TEST_CASE(testWeightedModifiedBesselFunctions)
{
    BOOST_TEST_MESSAGE("Testing weighted modified Bessel functions...");
    for (Real nu = -5.0; nu <= 5.0; nu += 0.5)
    {
        for (Real x = 0.1; x <= 15.0; x += 0.5)
        {
            Real calculated_i = modifiedBesselFunction_i_exponentiallyWeighted(nu, x);
            Real expected_i = modifiedBesselFunction_i(nu, x) * exp(-x);
            Real calculated_k = modifiedBesselFunction_k_exponentiallyWeighted(nu, x);
            Real expected_k = modifiedBesselFunction_k(nu, x) * exp(-x);
            Real tol_i = std::max(QL_EPSILON, 1e3 * QL_EPSILON * std::fabs(expected_i) * std::max(exp(x), 1.0));
            Real tol_k = std::max(QL_EPSILON, 1e3 * QL_EPSILON * std::fabs(expected_k) * std::max(exp(x), 1.0));
            if (std::abs(expected_i - calculated_i) > tol_i)
            {
                BOOST_ERROR("failed to verify exponentially weighted"
                            << " modified Bessel function of first kind"
                            << "\n order      : " << nu << "\n argument   : " << x << "\n calculated : " << calculated_i
                            << "\n expected   : " << expected_i << "\n tolerance  : " << tol_i
                            << "\n difference : " << (expected_i - calculated_i));
            }
            if (std::abs(expected_k - calculated_k) > tol_k)
            {
                BOOST_ERROR("failed to verify exponentially weighted"
                            << " modified Bessel function of second kind"
                            << "\n order      : " << nu << "\n argument   : " << x << "\n calculated : " << calculated_k
                            << "\n expected   : " << expected_k << "\n tolerance  : " << tol_k
                            << "\n difference : " << (expected_k - calculated_k));
            }
        }
    }
    for (Real nu = -5.0; nu <= 5.0; nu += 0.5)
    {
        for (Real x = -5.0; x <= 5.0; x += 0.5)
        {
            for (Real y = -5.0; y <= 5.0; y += 0.5)
            {
                std::complex<Real> z(x, y);
                std::complex<Real> calculated_i = modifiedBesselFunction_i_exponentiallyWeighted(nu, z);
                std::complex<Real> expected_i = modifiedBesselFunction_i(nu, z) * exp(-z);
                std::complex<Real> calculated_k = modifiedBesselFunction_k_exponentiallyWeighted(nu, z);
                std::complex<Real> expected_k = modifiedBesselFunction_k(nu, z) * exp(-z);
                Real tol_i = std::max(QL_EPSILON, 1e3 * QL_EPSILON * std::abs(calculated_i));
                Real tol_k = std::max(QL_EPSILON, 1e3 * QL_EPSILON * std::abs(calculated_k));
                if (std::abs(calculated_i - expected_i) > tol_i)
                {
                    BOOST_ERROR("failed to verify exponentially weighted"
                                << " modified Bessel function of first kind"
                                << "\n order      : " << nu << "\n argument   : " << x
                                << "\n calculated : " << calculated_i << "\n expected   : " << expected_i
                                << "\n tolerance  : " << tol_i << "\n difference : " << (expected_i - calculated_i));
                }
                if (std::abs(expected_k - calculated_k) > tol_k)
                {
                    BOOST_ERROR("failed to verify exponentially weighted"
                                << " modified Bessel function of second kind"
                                << "\n order      : " << nu << "\n argument   : " << x
                                << "\n calculated : " << calculated_k << "\n expected   : " << expected_k
                                << "\n tolerance  : " << tol_k << "\n difference : " << (expected_k - calculated_k));
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testExpm1)
{
    BOOST_TEST_MESSAGE("Testing complex valued expm1...");

    const std::complex<Real> z = std::complex<Real>(1.2, 0.5);
    QL_CHECK_SMALL(std::abs(std::exp(z) - 1.0 - expm1(z)), 10 * QL_EPSILON);

    const std::complex<Real> calculated = expm1(std::complex<Real>(5e-6, 5e-5));
    // scipy reference value
    const std::complex<Real> expected(4.998762493771078e-06, 5.000024997979157e-05);
    const Real tol = std::max(2.2e-14, 100 * QL_EPSILON);
    QL_CHECK_CLOSE_FRACTION(calculated.real(), expected.real(), tol);
    QL_CHECK_CLOSE_FRACTION(calculated.imag(), expected.imag(), tol);
}

BOOST_AUTO_TEST_CASE(testLog1p)
{
    BOOST_TEST_MESSAGE("Testing complex valued log1p...");

    const std::complex<Real> z = std::complex<Real>(1.2, 0.57);
    QL_CHECK_SMALL(std::abs(std::log(1.0 + z) - log1p(z)), 10 * QL_EPSILON);

    const std::complex<Real> calculated = log1p(std::complex<Real>(5e-6, 5e-5));
    // scipy reference value
    const std::complex<Real> expected(5.0012374875401984e-06, 4.999974995958395e-05);
    const Real tol = std::max(2.2e-14, 100 * QL_EPSILON);
    QL_CHECK_CLOSE_FRACTION(calculated.real(), expected.real(), tol);
    QL_CHECK_CLOSE_FRACTION(calculated.imag(), expected.imag(), tol);
}

BOOST_AUTO_TEST_CASE(testAbcdMaximum)
{
    BOOST_TEST_MESSAGE("Testing the maximum of the abcd function...");

    Size checked = 0;
    for (Real a : {-0.06, -0.01, 0.0, 0.01, 0.2})
    {
        for (Real b : {-0.5, -0.1, -0.01, 0.0, 0.01, 0.17})
        {
            for (Real c : {0.5, 2.0})
            {
                for (Real d : {0.06, 0.17})
                {
                    try
                    {
                        AbcdMathFunction::validate(a, b, c, d);
                    }
                    catch (const Error&)
                    {
                        // Not every combination is a function that stays positive.
                        continue;
                    }
                    ++checked;

                    AbcdMathFunction f(a, b, c, d);

                    // The limit at infinity, and the rest on a grid.
                    Real expected = d;
                    for (Size i = 0; i <= 5000; ++i)
                        expected = std::max(expected, f(i * 0.01));

                    Time location = f.maximumLocation();
                    BOOST_TEST_CONTEXT("a " << a << ", b " << b << ", c " << c << ", d " << d)
                    {
                        BOOST_CHECK_SMALL(f.maximumValue() - expected, 1e-5);
                        BOOST_CHECK_SMALL((location == QL_MAX_REAL ? d : f(location)) - expected, 1e-5);
                    }
                }
            }
        }
    }
    BOOST_TEST(checked > 100U);
}

BOOST_AUTO_TEST_CASE(testModifiedBesselFunctionKOnRealArguments)
{
    BOOST_TEST_MESSAGE("Testing the modified Bessel function of the second kind on real arguments...");

    // From mpmath at 80 digits.  The difference of the two functions of the first kind that
    // this function used to be is 0/0 at integer orders, exactly 0 from x = 13 on, and worse
    // than useless close to an integer order as x grows.
    struct RealCase
    {
        Real nu, x, expected;
    };
    const RealCase realCases[] = {
        {0.0, 0.1, 2.4270690247020166},
        {0.0, 2.0, 0.11389387274953344},
        {0.0, 12.9, 8.6359338399729934e-7},
        {0.0, 13.0, 7.7845438614204963e-7},
        {0.0, 14.0, 2.7613708239816199e-7},
        {0.0, 30.0, 2.1324774964630564e-14},
        {0.0, 100.0, 4.6566282291759020e-45},
        {1.0, 0.1, 9.8538447808706061},
        {1.0, 2.0, 0.13986588181652243},
        {1.0, 12.9, 8.9646231982207976e-7},
        {1.0, 13.0, 8.0785884122023473e-7},
        {1.0, 14.0, 2.8583436534402497e-7},
        {1.0, 30.0, 2.1677320018915494e-14},
        {1.0, 100.0, 4.6798537356369093e-45},
        {2.0, 0.1, 199.50396464211414},
        {2.0, 2.0, 0.25375975456605586},
        {2.0, 12.9, 1.0025797901712652e-6},
        {2.0, 13.0, 9.0274036171439344e-7},
        {2.0, 14.0, 3.1697056316159413e-7},
        {2.0, 30.0, 2.2769929632558263e-14},
        {2.0, 100.0, 4.7502253038886402e-45},
        {5.0, 0.1, 38376009.995835928},
        {5.0, 2.0, 9.4310491005964674},
        {5.0, 12.9, 2.1773448429707602e-6},
        {5.0, 13.0, 1.9495008621248228e-6},
        {5.0, 14.0, 6.4970237051686762e-7},
        {5.0, 30.0, 3.2103335105890262e-14},
        {5.0, 100.0, 5.2732561132929499e-45},
        {-1.0, 0.1, 9.8538447808706061},
        {-1.0, 2.0, 0.13986588181652243},
        {-1.0, 12.9, 8.9646231982207976e-7},
        {-1.0, 13.0, 8.0785884122023473e-7},
        {-1.0, 14.0, 2.8583436534402497e-7},
        {-1.0, 30.0, 2.1677320018915494e-14},
        {-1.0, 100.0, 4.6798537356369093e-45},
        {-2.0, 0.1, 199.50396464211414},
        {-2.0, 2.0, 0.25375975456605586},
        {-2.0, 12.9, 1.0025797901712652e-6},
        {-2.0, 13.0, 9.0274036171439344e-7},
        {-2.0, 14.0, 3.1697056316159413e-7},
        {-2.0, 30.0, 2.2769929632558263e-14},
        {-2.0, 100.0, 4.7502253038886402e-45},
        {10.0, 0.1, 1.8574295846304010e+18},
        {10.0, 2.0, 162482.40397955915},
        {10.0, 12.9, 3.1556913025602075e-5},
        {10.0, 13.0, 2.7743457369510048e-5},
        {10.0, 14.0, 7.7998684144326625e-6},
        {10.0, 30.0, 1.0842816942222974e-13},
        {10.0, 100.0, 7.6554279773881006e-45},
        {20.0, 0.1, 6.3768675266611857e+42},
        {20.0, 2.0, 5.7708568527002410e+16},
        {20.0, 12.9, 0.49178283513380120},
        {20.0, 13.0, 0.40865746066182415},
        {20.0, 14.0, 0.067555492104997418},
        {20.0, 30.0, 1.2304516475442477e-11},
        {20.0, 100.0, 3.3852054148901701e-44},
        {50.0, 0.1, 3.4241477644707503e+127},
        {50.0, 2.0, 2.9799817396049217e+62},
        {50.0, 12.9, 4.3605869539157636e+21},
        {50.0, 13.0, 2.9256522170853681e+21},
        {50.0, 14.0, 6.2846184985670472e+19},
        {50.0, 30.0, 58.770686258007236},
        {50.0, 100.0, 9.2745226536133259e-40},
        {0.5, 13.0, 7.8570586973409698e-7},
        {0.5, 14.0, 2.7853076631767926e-7},
        {0.5, 30.0, 2.1412375659560114e-14},
        {0.5, 100.0, 4.6624238126346716e-45},
        {2.3, 13.0, 9.4681299189114511e-7},
        {2.3, 14.0, 3.3135859556823985e-7},
        {2.3, 30.0, 2.3256344452638288e-14},
        {2.3, 100.0, 4.7808084236534539e-45},
        {5.5, 13.0, 2.3586496904688041e-6},
        {5.5, 14.0, 7.7614277223443587e-7},
        {5.5, 30.0, 3.4975569190538256e-14},
        {5.5, 100.0, 5.4127455530679227e-45},
        {20.5, 13.0, 0.74797947521161221},
        {20.5, 14.0, 0.11994229784751161},
        {20.5, 30.0, 1.6813891497623431e-11},
        {20.5, 100.0, 3.7415633308864162e-44},
        {-2.3, 13.0, 9.4681299189114511e-7},
        {-2.3, 14.0, 3.3135859556823985e-7},
        {-2.3, 30.0, 2.3256344452638288e-14},
        {-2.3, 100.0, 4.7808084236534539e-45},
        {1.000000000001, 12.9, 8.9646231982214670e-7},
        {1.000000001, 12.9, 8.9646231988902498e-7},
        {1.000001, 12.9, 8.9646238676733924e-7},
        {1.001, 12.9, 8.9652930096430603e-7},
        {0.000000001, 5.0, 3.6910983340425943e-3},
        {5.0001, 10.0, 5.7544511860922993e-5},
        {1.999999, 12.0, 2.5826178957462277e-6},
    };

    for (const auto& c : realCases)
    {
        BOOST_TEST_CONTEXT("order: " << c.nu << ", argument: " << c.x)
        {
            BOOST_CHECK_SMALL(modifiedBesselFunction_k(c.nu, c.x) / c.expected - 1.0, 1e-13);
            BOOST_CHECK_SMALL(
                modifiedBesselFunction_k_exponentiallyWeighted(c.nu, c.x) / (c.expected * exp(-c.x)) - 1.0, 1e-13);
        }
    }

    // Below what a Real can hold, and not the NaN that infinity less infinity gives.
    BOOST_CHECK_EQUAL(modifiedBesselFunction_k(1.0, 1000.0), 0.0);
    BOOST_CHECK_EQUAL(modifiedBesselFunction_k_exponentiallyWeighted(1.0, 1000.0), 0.0);

    BOOST_CHECK(std::isinf(modifiedBesselFunction_k(0.5, 0.0)));
    BOOST_CHECK(std::isinf(modifiedBesselFunction_k(1.0, 0.0)));
    BOOST_CHECK(std::isinf(modifiedBesselFunction_k(200.0, 1e-3)));
    BOOST_CHECK_EXCEPTION(modifiedBesselFunction_k(1.0, -2.0), Error,
                          ExpectedErrorMessage("negative argument requires complex version"));
}

BOOST_AUTO_TEST_CASE(testModifiedBesselFunctionKOnComplexArguments)
{
    BOOST_TEST_MESSAGE("Testing the modified Bessel function of the second kind on complex arguments...");

    struct ComplexCase
    {
        Real nu;
        std::complex<Real> z, expected;
    };

    const auto check = [](const ComplexCase& c, Real tolerance)
    {
        BOOST_TEST_CONTEXT("order: " << c.nu << ", argument: " << c.z)
        {
            BOOST_CHECK_SMALL(std::abs(modifiedBesselFunction_k(c.nu, c.z) / c.expected - 1.0), tolerance);
            BOOST_CHECK_SMALL(
                std::abs(modifiedBesselFunction_k_exponentiallyWeighted(c.nu, c.z) / (c.expected * std::exp(-c.z)) -
                         1.0),
                tolerance);
        }
    };

    // From mpmath at 80 digits: integer orders, then arguments past |z| = 13, where the
    // functions used to switch to their large-argument expansions.
    const ComplexCase ordinary[] = {
        {0.0, {2.0, 1.0}, {0.037987722915986459, -0.10171357546139087}},
        {0.0, {1.5, 0.3}, {0.19631123266108719, -0.080286163745902481}},
        {0.0, {-1.5, 0.0}, {0.21380556264752574, -5.1733334754864648}},
        {0.0, {-1.5, 2.0}, {-3.6219738066561179, -0.30061284364291350}},
        {0.0, {0.0, 1.0}, {-0.13863371520405400, -1.2019697153172065}},
        {0.0, {3.0, -4.0}, {-7.2390512135701550e-3, -0.026510418350267677}},
        {0.0, {-3.0, -4.0}, {4.1520596356776255, -10.631304294689191}},
        {0.0, {0.0, 10.1}, {-0.048156500887958783, 0.39117486039551005}},
        {0.0, {-5.0, 3.0}, {-32.364622699308347, 71.538347562918405}},
        {0.0, {-10.0, 1.0}, {-7170.1442069845739, -5140.6736349132036}},
        {1.0, {2.0, 1.0}, {0.036291592400427046, -0.12406383457283476}},
        {1.0, {1.5, 0.3}, {0.24853429791540340, -0.11371027152502320}},
        {1.0, {-1.5, 0.0}, {-0.27738780045684382, -3.0839960402960839}},
        {1.0, {-1.5, 2.0}, {-3.2912186170639765, 0.37171129954430904}},
        {1.0, {0.0, 1.0}, {-0.69122984369208426, -1.2271262301435715}},
        {1.0, {3.0, -4.0}, {-5.6734204013233075e-3, -0.028666936579007819}},
        {1.0, {-3.0, -4.0}, {4.8155011855992771, -9.6680457760454749}},
        {1.0, {0.0, 10.1}, {-0.028895608110252194, 0.39402551906866766}},
        {1.0, {-5.0, 3.0}, {-26.492395041492396, 67.727469701169141}},
        {1.0, {-10.0, 1.0}, {-6832.4323841019993, -4841.6422668875200}},
        {2.0, {2.0, 1.0}, {0.017395463007194191, -0.21548128007982950}},
        {2.0, {1.5, 0.3}, {0.48578846805903432, -0.28979530619218951}},
        {2.0, {-1.5, 0.0}, {0.58365596325665082, -1.0613387550916862}},
        {2.0, {-1.5, 2.0}, {-1.8042936387570514, 1.6273456474967631}},
        {2.0, {0.0, 1.0}, {-2.5928861754911970, 0.18048997206696203}},
        {2.0, {3.0, -4.0}, {5.7274759539475327e-4, -0.035205977657653012}},
        {2.0, {-3.0, -4.0}, {6.0901139994683510, -6.7700129290465080}},
        {2.0, {0.0, 10.1}, {0.029868354373163525, 0.39689676299159960}},
        {2.0, {-5.0, 3.0}, {-12.620835386898383, 56.293632069896728}},
        {2.0, {-10.0, 1.0}, {-5913.0614035364061, -4046.6369011908828}},
        {5.0, {2.0, 1.0}, {-4.7284687805202415, -3.1493224423228796}},
        {5.0, {1.5, 0.3}, {20.292044245076185, -34.645985087892522}},
        {5.0, {-1.5, 0.0}, {-44.067781159301077, -6.8190139964559087e-3}},
        {5.0, {-1.5, 2.0}, {-1.2222863769467074, 4.0698938353718112}},
        {5.0, {0.0, 1.0}, {-3.9231852520443782e-4, -409.04457877166760}},
        {5.0, {3.0, -4.0}, {0.13684719097412746, -0.022384610559320370}},
        {5.0, {-3.0, -4.0}, {1.0512954053020401, 1.6549349529482941}},
        {5.0, {0.0, 10.1}, {0.38229847772557640, 0.17869883535185129}},
        {5.0, {-5.0, 3.0}, {7.2501581742045448, 8.7007000171959470}},
        {5.0, {-10.0, 1.0}, {-2171.5759158443228, -1168.6641873673010}},
        {-3.0, {2.0, 1.0}, {-0.10826069085192585, -0.48275025310631732}},
        {-3.0, {1.5, 0.3}, {1.3455327769425737, -1.1058974505096293}},
        {-3.0, {-1.5, 0.0}, {-1.8338037024745793, -0.25375936005158728}},
        {-3.0, {-1.5, 2.0}, {0.52390570493864963, 1.1189553355564422}},
        {-3.0, {0.0, 1.0}, {0.030730044575763844, 9.1444184718212164}},
        {-3.0, {3.0, -4.0}, {0.017133324145364102, -0.045199247393628623}},
        {-3.0, {-3.0, -4.0}, {6.2250547404442337, -2.5207666104434065}},
        {-3.0, {0.0, 10.1}, {0.12829122871810408, 0.38219646783177121}},
        {-3.0, {-5.0, 3.0}, {0.79996650488196824, 39.067980973076377}},
        {-3.0, {-10.0, 1.0}, {-4650.8887352238536, -3004.8331415233523}},
        {0.0, {13.0, 1.0}, {3.9504875729873617e-7, -6.6948062080445184e-7}},
        {0.0, {20.0, 10.0}, {-3.7692389434260423e-10, 3.9171613377869616e-10}},
        {0.0, {20.0, -10.0}, {-3.7692389434260423e-10, -3.9171613377869616e-10}},
        {0.0, {-20.0, 10.0}, {43197815.640299233, 1.2181151980992296e+8}},
        {0.0, {-20.0, -10.0}, {43197815.640299233, -1.2181151980992296e+8}},
        {0.0, {-20.0, 0.0}, {5.7412378153365243e-10, -1.3684238049208180e+8}},
        {0.0, {0.0, 14.1}, {-0.22483786215156764, -0.24654100272270103}},
        {0.0, {0.0, -14.1}, {-0.22483786215156764, 0.24654100272270103}},
        {0.0, {0.0, 180.1}, {0.022482654217680300, 0.090643794407475275}},
        {0.0, {100.0, 100.0}, {3.8804363039727764e-45, 5.4243729735082030e-46}},
        {0.0, {-100.0, 100.0}, {2.2627199888212093e+42, -1.7076813811546166e+42}},
        {1.0, {13.0, 1.0}, {4.0798473714394725e-7, -6.9574683598987086e-7}},
        {1.0, {20.0, 10.0}, {-3.8056380099625814e-10, 4.0319028807850712e-10}},
        {1.0, {20.0, -10.0}, {-3.8056380099625814e-10, -4.0319028807850712e-10}},
        {1.0, {-20.0, 10.0}, {43571220.847170608, 1.1891568012987139e+8}},
        {1.0, {-20.0, -10.0}, {43571220.847170608, -1.1891568012987139e+8}},
        {1.0, {-20.0, 0.0}, {-5.8830579695570382e-10, -1.3337623249506760e+8}},
        {1.0, {0.0, 14.1}, {-0.23370991250250787, -0.23873172035263829}},
        {1.0, {0.0, -14.1}, {-0.23370991250250787, 0.23873172035263829}},
        {1.0, {0.0, 180.1}, {0.022734387429533049, 0.090581727048358115}},
        {1.0, {100.0, 100.0}, {3.8914899953028699e-45, 5.3411641462767852e-46}},
        {1.0, {-100.0, 100.0}, {2.2527833308578224e+42, -1.7090832445794618e+42}},
        {2.3, {13.0, 1.0}, {4.6820814023407090e-7, -8.2023410708404277e-7}},
        {2.3, {20.0, 10.0}, {-3.9570504017153025e-10, 4.5542796978765976e-10}},
        {2.3, {20.0, -10.0}, {-3.9570504017153025e-10, -4.5542796978765976e-10}},
        {2.3, {-20.0, 10.0}, {44758511.093865339, 1.0710791215082835e+8}},
        {2.3, {-20.0, -10.0}, {44758511.093865339, -1.0710791215082835e+8}},
        {2.3, {-20.0, 0.0}, {3.8392028919954027e-10, -1.1949191421028711e+8}},
        {2.3, {0.0, 14.1}, {-0.26863964920755039, -0.20165554017382007}},
        {2.3, {0.0, -14.1}, {-0.26863964920755039, 0.20165554017382007}},
        {2.3, {0.0, 180.1}, {0.023812369207996586, 0.090307527618595169}},
        {2.3, {100.0, 100.0}, {3.9389879324675747e-45, 4.9787032614492455e-46}},
        {2.3, {-100.0, 100.0}, {2.2103971633696367e+42, -1.7147754910694260e+42}},
        {5.5, {13.0, 1.0}, {1.0248940565153851e-6, -2.1055450697166734e-6}},
        {5.5, {20.0, 10.0}, {-4.5352491919094393e-10, 8.7541061427328102e-10}},
        {5.5, {20.0, -10.0}, {-4.5352491919094393e-10, -8.7541061427328102e-10}},
        {5.5, {-20.0, 10.0}, {42474984.661460186, 55598058.744549324}},
        {5.5, {-20.0, -10.0}, {42474984.661460186, -55598058.744549324}},
        {5.5, {-20.0, 0.0}, {0.0, -63307877.358959459}},
        {5.5, {0.0, 14.1}, {-0.33657611938905728, 0.086662412090161111}},
        {5.5, {0.0, -14.1}, {-0.33657611938905728, -0.086662412090161111}},
        {5.5, {0.0, 180.1}, {0.030014293355056402, 0.088458911424306188}},
        {5.5, {100.0, 100.0}, {4.2174427935123253e-45, 2.6879627826825619e-46}},
        {5.5, {-100.0, 100.0}, {1.9716335699011815e+42, -1.7379958714761106e+42}},
    };
    for (const auto& c : ordinary)
        check(c, 1e-12);

    // Close to an integer order, where this used to take either the quotient, whose rounding
    // grows as it nears the integer, or the integer order itself, which is off by the distance.
    // Temme's series takes the distance as it is.
    const ComplexCase nearInteger[] = {
        {1.000000000001, {5.0, 5.0}, {2.1595269238119622e-3, 2.4903134910426084e-3}},
        {1.000000000001, {0.0, 5.0}, {0.51456010606336940, 0.23226288250718930}},
        {1.000000000001, {-5.0, 3.0}, {-26.492395041481324, 67.727469701161476}},
        {1.00000001, {5.0, 5.0}, {2.1595269282172894e-3, 2.4903134915579985e-3}},
        {1.00000001, {0.0, 5.0}, {0.51456010662125032, 0.23226288153804951}},
        {1.00000001, {-5.0, 3.0}, {-26.492394930775291, 67.727469624522708}},
        {1.000001, {5.0, 5.0}, {2.1595273643885430e-3, 2.4903135425867291e-3}},
        {1.000001, {0.0, 5.0}, {0.51456016185700426, 0.23226278558356044}},
        {1.000001, {-5.0, 3.0}, {-26.492383969777697, 67.727462036522028}},
        {2.0001, {5.0, 5.0}, {2.8752339982330549e-3, 2.5267608475671647e-3}},
        {2.0001, {0.0, 5.0}, {0.57752891877441678, 0.073121498704820881}},
        {2.0001, {-5.0, 3.0}, {-12.619321908322705, 56.292137615076128}},
        {1.999999999999, {1.0, 1.0}, {-0.35495344133013877, -0.84156523860969548}},
        {-1.000000000001, {1.0, 1.0}, {0.024568305523601809, -0.45971947380140810}},
        {1.00000000001, {-12.0, 1.0}, {-46659.760703397590, -32579.411356517057}},
        {1.001, {0.5, 12.0}, {0.21415945534612881, -0.049982693150032062}},
    };
    for (const auto& c : nearInteger)
        check(c, 1e-13);

    // Where the function of the first kind is largest against this one, just below the old
    // switch, which cost the quotient up to 1e-4.
    const ComplexCase nearTheSwitch[] = {
        {1.001, {12.0, 1.0}, {1.1496563486432280e-6, -1.9765014323790261e-6}},
        {5.000001, {12.0, 1.0}, {2.5713078039449610e-6, -5.2863219374721001e-6}},
        {2.0, {12.0, 1.0}, {1.2737998551066956e-6, -2.2386739555421913e-6}},
    };
    for (const auto& c : nearTheSwitch)
        check(c, 1e-13);

    BOOST_CHECK_EQUAL(std::abs(modifiedBesselFunction_k(1.0, std::complex<Real>(1000.0, 1.0))), 0.0);
}

BOOST_AUTO_TEST_CASE(testModifiedBesselFunctionIAtNegativeIntegerOrders)
{
    BOOST_TEST_MESSAGE("Testing the modified Bessel function of the first kind at negative integer orders...");

    for (Real nu : {1.0, 2.0, 3.0, 5.0})
    {
        for (Real x : {0.1, 2.0, 12.0})
        {
            BOOST_TEST_CONTEXT("order: " << -nu << ", argument: " << x)
            {
                BOOST_CHECK_EQUAL(modifiedBesselFunction_i(-nu, x), modifiedBesselFunction_i(nu, x));
                BOOST_CHECK_EQUAL(modifiedBesselFunction_i_exponentiallyWeighted(-nu, x),
                                  modifiedBesselFunction_i_exponentiallyWeighted(nu, x));
            }
            const std::complex<Real> z(x, 1.0);
            BOOST_TEST_CONTEXT("order: " << -nu << ", argument: " << z)
            {
                BOOST_CHECK_EQUAL(modifiedBesselFunction_i(-nu, z), modifiedBesselFunction_i(nu, z));
            }
        }
    }

    // I_1(2) and I_2(12), so that these aren't only the same as each other.
    BOOST_CHECK_SMALL(modifiedBesselFunction_i(-1.0, 2.0) / 1.5906368546373291 - 1.0, 1e-14);
    BOOST_CHECK_SMALL(modifiedBesselFunction_i(-2.0, 12.0) / 15925.367219023170 - 1.0, 1e-14);
}

BOOST_AUTO_TEST_CASE(testModifiedBesselFunctionIOnRealArguments)
{
    BOOST_TEST_MESSAGE("Testing the modified Bessel function of the first kind on real arguments...");

    // From mpmath at 50 digits: I, its condition number |x I'(x) / I(x)|, the weighted function
    // and its own, |x I'(x) / I(x) - x|.  A relative error of a few QL_EPSILON in x moves a value
    // by its condition number times as much, so the tolerance scales with it.  Large orders,
    // arguments around 13, where this used to switch to 30 terms of the large-argument
    // expansion, and around 710, where exp(x) overflows.
    struct RealCase
    {
        Real nu, x, expected, kappa, weighted, kappaWeighted;
    };
    const RealCase cases[] = {
        {20.0, 13.0, 0.05128416382353676, 23.7, 1.1591910360277408e-7, 10.7},
        {50.0, 13.0, 3.308021124332481e-24, 51.6, 7.4772174262432373e-30, 38.6},
        {300.0, 30.0, 4.6324735572700024e-262, 301.0, 4.3348940962054524e-275, 271.0},
        {0.5, 710.0, 3.3447452278080108e+306, 710.0, 0.014972036991629623, 0.5},
        {0.5, 712.0, 2.4679774324006396e+307, 711.0, 0.014950994062057319, 0.5},
        {1.25, 13.00005, 46447.830880079822, 12.6, 0.10498214879005776, 0.445},
        {1.0, 13.07, 50819.251331365566, 12.6, 0.10710244474845591, 0.469},
        {-5.5, 14.0, 42835.295462249717, 14.6, 0.035618778368147381, 0.606},
        {100.25, 19.0, 8.5419014963444419e-61, 102.0, 4.7858535273512894e-69, 83.0},
        {2.0, 18.5, 9.0478142781912922e+6, 18.1, 0.083578728945650874, 0.393},
        {10.0, 30.0, 1.4583180997596712e+11, 31.2, 0.013646390946186457, 1.17},
        {50.0, 705.0, 3.8393112690991032e+303, 706.0, 0.0025506072030961704, 1.27},
        {-1.5, 5.0, 21.181557144697875, 4.75, 0.14272020939907438, 0.249},
        {0.31, 2.7, 3.7385437595285818, 2.17, 0.25125075025911004, 0.531},
        {3.7, 18.5, 6.9200423182771508e+6, 18.4, 0.063923542573791478, 0.12},
        {15.3, 50.0, 2.8080520080193178e+19, 51.8, 0.0054160298835420332, 1.83},
        {-50.5, 200.0, 3.534736512027489e+82, 206.0, 4.8917095819243722e-5, 5.81},
    };

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("order: " << c.nu << ", argument: " << c.x)
        {
            BOOST_CHECK_SMALL(modifiedBesselFunction_i(c.nu, c.x) / c.expected - 1.0,
                              32 * QL_EPSILON * std::max(1.0, c.kappa));
            BOOST_CHECK_SMALL(modifiedBesselFunction_i_exponentiallyWeighted(c.nu, c.x) / c.weighted - 1.0,
                              32 * QL_EPSILON * std::max(1.0, c.kappaWeighted));
        }
    }

    // Too large to hold, though I_300(1000) exp(-1000) isn't: it's 4.91e-22, with a condition
    // number of 43.6.
    BOOST_CHECK_EQUAL(modifiedBesselFunction_i(0.5, 720.0), std::numeric_limits<Real>::infinity());
    BOOST_CHECK_EQUAL(modifiedBesselFunction_i(300.0, 1000.0), std::numeric_limits<Real>::infinity());
    BOOST_CHECK_SMALL(modifiedBesselFunction_i_exponentiallyWeighted(300.0, 1000.0) / 4.9103801083448402e-22 - 1.0,
                      32 * QL_EPSILON * 43.6);
}

BOOST_AUTO_TEST_CASE(testModifiedBesselFunctionsOnComplexArguments)
{
    BOOST_TEST_MESSAGE("Testing modified Bessel functions on complex arguments...");

    // From mpmath at 50 digits, as in the real case: each function with its condition number
    // |z f'(z) / f(z)|, and the weighted one with its own, |z f'(z) / f(z) - z|.  Large orders,
    // all four quadrants, and arguments around |z| = 13, where both functions used to switch to
    // 30 terms of their expansions, I's in the form of DLMF 10.40.5 that holds only for
    // arg z > -pi/2.  Then arguments by the imaginary axis past |z| = 18, where which side of it
    // z is on decides the form, and Re z past 700, where exp(z) overflows.
    struct ComplexCase
    {
        Real nu;
        std::complex<Real> z, expected;
        Real kappa;
    };
    using Function = std::complex<Real> (*)(Real, const std::complex<Real>&);

    const auto check = [](const ComplexCase& c, Function f, const char* name)
    {
        BOOST_TEST_CONTEXT(name << ", order: " << c.nu << ", argument: " << c.z)
        {
            BOOST_CHECK_SMALL(std::abs(f(c.nu, c.z) / c.expected - 1.0), 32 * QL_EPSILON * std::max(1.0, c.kappa));
        }
    };

    const ComplexCase firstKind[] = {
        {0.308, {-9.2, -9.2}, {20.607851161286335, 1099.0458560923659}, 12.7},
        {1.2, {-20.0, -10.0}, {3.8479871480491503e+7, -1.0754179214184416e+7}, 21.9},
        {1.2, {-9.0, -9.0}, {139.65611965061516, -865.53605926544816}, 12.4},
        {0.0, {-0.5, -14.1}, {0.17560402855156822, 0.077735862335457476}, 13.6},
        {5.5, {-13.0, -1.0}, {-13119.50416184314, 7479.7370008323305}, 13.7},
        {20.0, {13.0, -2.0}, {-0.055280258195098022, 0.028979040218085241}, 23.7},
        {50.0, {13.0, 1.0}, {-2.5962873330280546e-24, -2.7964286781354417e-24}, 51.6},
        {50.0, {-13.0, 1.0}, {-2.5962873330280546e-24, 2.7964286781354417e-24}, 51.6},
        {100.25, {0.0, 200.0}, {-0.0059875475660258376, -0.0024801234072019167}, 1610.0},
        {300.0, {30.0, 30.0}, {-2.3820861055423661e-218, -3.1364042362621443e-217}, 300.0},
        {300.0, {10.0, -1000.0}, {3.1111085659523954, -179.4723468498017}, 954.0},
        {-20.3, {20.0, -20.0}, {150354.2565969619, 82205.398805330718}, 29.6},
        {-50.5, {5.0, 5.0}, {-1.6490566842453159e+35, -2.1908879777673575e+35}, 50.5},
        {2.3, {-300.0, 50.0}, {1.2248536999902771e+128, 4.2335880363504181e+128}, 304.0},
        {0.5, {712.0, 10.0}, {-2.0800838686729353e+307, -1.3279921110147033e+307}, 712.0},
        {10.0, {0.1, 12.0}, {-0.3009269285645271, -0.001998028121609178}, 0.872},
        {-0.92, {0.001, 0.002}, {22.708024112303317, -36.852880482618591}, 0.92},
        {1.25, {3.0, -0.03}, {3.5359235662253477, -0.10000376013153796}, 2.83},
        {15.3, {-0.17, -17.0}, {0.12197541429838729, 0.23250289888223008}, 1.33},
        {0.25, {40.0, -200.0}, {-1.166182288378588e+15, 6.4716598432473947e+15}, 204.0},
        {1.2, {0.0, -21.0}, {-0.047464159650143458, -0.14607966276110449}, 10.7},
        {0.308, {1.0, -25.0}, {0.1248329251480539, 0.14232958558478734}, 32.3},
        {20.0, {0.5, 17.5}, {0.048358194734768456, -0.015503445713481198}, 10.9},
    };
    for (const auto& c : firstKind)
        check(c, modifiedBesselFunction_i, "I");

    const ComplexCase firstKindWeighted[] = {
        {0.308, {-9.2, -9.2}, {-2.6232901609951945e+6, -1.0558302413420938e+7}, 25.7},
        {1.2, {-20.0, -10.0}, {-1.8503164785691383e+16, -5.7784808967310682e+15}, 44.3},
        {1.2, {-9.0, -9.0}, {1.8593214250171552e+6, 6.85659434512987e+6}, 25.1},
        {0.0, {-0.5, -14.1}, {-0.1173180842073394, 0.29408454570582999}, 25.9},
        {5.5, {-13.0, -1.0}, {-5.9205883750187022e+9, -3.0961694860961257e+9}, 26.8},
        {20.0, {13.0, -2.0}, {-7.5627505659143858e-9, -1.4087668585478495e-7}, 10.7},
        {50.0, {13.0, 1.0}, {-8.4895568133960193e-30, 1.5229728759237594e-30}, 38.6},
        {50.0, {-13.0, 1.0}, {4.2044029426386135e-19, 1.6349905949751391e-18}, 64.6},
        {100.25, {0.0, 200.0}, {-7.5117430941938786e-4, -0.0064371946628357514}, 1620.0},
        {300.0, {30.0, 30.0}, {2.8654188286810093e-230, -6.7295583379679299e-231}, 271.0},
        {300.0, {10.0, -1000.0}, {0.006816873640179106, -0.0044654908116374622}, 46.1},
        {-20.3, {20.0, -20.0}, {-2.8221631556699611e-5, 3.5206926370568769e-4}, 6.95},
        {-50.5, {5.0, 5.0}, {1.1003880328524273e+33, -1.4842299497003928e+33}, 55.8},
        {2.3, {-300.0, 50.0}, {1.3821367780837142e+257, 8.5595727704387371e+258}, 608.0},
        {0.5, {712.0, 10.0}, {0.014949888256194791, -1.0497999349362228e-4}, 0.5},
        {10.0, {0.1, 12.0}, {-0.22880288292350713, -0.14762900600836089}, 11.7},
        {-0.92, {0.001, 0.002}, {22.611650024822773, -36.861343014997843}, 0.921},
        {1.25, {3.0, -0.03}, {0.17611339922300028, 3.0385218257081062e-4}, 0.173},
        {15.3, {-0.17, -17.0}, {0.22516589283396906, -0.21482836260938323}, 17.6},
        {0.25, {40.0, -200.0}, {0.02159665080230617, 0.01772131624472476}, 0.5},
        {1.2, {0.0, -21.0}, {0.14821588257686325, 0.040300948818282294}, 23.6},
        {0.308, {1.0, -25.0}, {0.052449423996463647, 0.045821455312616973}, 7.57},
        {20.0, {0.5, 17.5}, {0.015610452623733634, 0.026552357625086146}, 19.8},
    };
    for (const auto& c : firstKindWeighted)
        check(c, modifiedBesselFunction_i_exponentiallyWeighted, "weighted I");

    const ComplexCase secondKind[] = {
        {0.308, {-9.2, -9.2}, {-2011.9570913352437, -2806.7299870291373}, 12.7},
        {1.2, {-20.0, -10.0}, {4.3723389770802385e+7, -1.1765898433347662e+8}, 21.9},
        {20.0, {13.0, -2.0}, {-0.29033683441397753, -0.16961563274020324}, 24.0},
        {50.0, {13.0, 1.0}, {-1.7168838562742504e+21, 1.8674048139425367e+21}, 51.7},
        {50.0, {-13.0, 1.0}, {-1.7168838562742504e+21, -1.8674048139425367e+21}, 51.7},
        {100.25, {0.0, 200.0}, {0.091410145557475725, -0.026844424058020578}, 173.0},
        {300.0, {30.0, 30.0}, {-3.4839472161040884e+212, 5.2866835348181729e+213}, 300.0},
        {-20.3, {20.0, -20.0}, {9.7172970708086883e-8, 4.5720296358019716e-9}, 30.4},
        {2.3, {-300.0, 50.0}, {4.7045763205381687e+128, -1.302188775647013e+129}, 304.0},
        {5.0, {0.1, 1.9}, {4.5619500713125384, -18.935948712459042}, 4.51},
        {170.5, {1.0, -13.0}, {-5.1501792205856873e+166, -1.5963313807460768e+166}, 170.0},
        {-0.92, {0.001, 0.002}, {143.42377721134094, -232.78400328156401}, 0.92},
        {0.25, {-2.0, 0.5}, {-2.3418204422679146, -6.3889250596823436}, 1.55},
        {30.5, {21.0, -21.0}, {1.8275209173433474e-5, -4.9236713651972516e-6}, 36.1},
        {2.0, {-0.02, 2.1}, {-0.89803358791478727, 0.60385502758685857}, 1.63},
        {300.0, {-5.0, -500.0}, {-3.4131179787612059, -0.24163173482180953}, 400.0},
        {1.2, {-0.5, 21.0}, {-0.44469171728658363, -0.074286939149452362}, 21.0},
        {0.308, {-1.0, -25.0}, {0.53408592179114564, 0.42273736180288781}, 25.0},
    };
    for (const auto& c : secondKind)
        check(c, modifiedBesselFunction_k, "K");

    const ComplexCase secondKindWeighted[] = {
        {0.308, {-9.2, -9.2}, {2.5603232953709337e+7, 2.2641443239009755e+7}, 25.7},
        {1.2, {-20.0, -10.0}, {-4.8854205614946829e+16, 3.635723994733228e+16}, 44.3},
        {20.0, {13.0, -2.0}, {6.2171222332487377e-7, -4.3718732512434067e-7}, 37.1},
        {50.0, {13.0, 1.0}, {1.4550433440710404e+15, 5.5461048888249194e+15}, 64.7},
        {50.0, {-13.0, 1.0}, {-1.1055925152138414e+27, 1.9277934491833467e+26}, 38.7},
        {100.25, {0.0, 200.0}, {0.067977059261334292, 0.066749960509552204}, 373.0},
        {300.0, {30.0, 30.0}, {-4.9381588564637506e+200, 4.4098134902144919e+199}, 332.0},
        {-20.3, {20.0, -20.0}, {7.3130830143181977e-17, 1.866979870866165e-16}, 58.3},
        {2.3, {-300.0, 50.0}, {1.5454667415971053e+259, -2.2010248821833865e+259}, 608.0},
        {5.0, {0.1, 1.9}, {-17.548345212902575, 1.6330694875995621}, 5.01},
        {170.5, {1.0, -13.0}, {-1.4725433138181354e+166, -1.3289723228858461e+166}, 172.0},
        {-0.92, {0.001, 0.002}, {142.81503620017088, -232.83743118818394}, 0.921},
        {0.25, {-2.0, 0.5}, {-37.818331617960427, -33.133123805661884}, 3.6},
        {30.5, {21.0, -21.0}, {-4.4664688454948443e-15, 1.3638672336491452e-14}, 64.5},
        {2.0, {-0.02, 2.1}, {0.99431074649556714, 0.47983858685134243}, 3.57},
        {300.0, {-5.0, -500.0}, {430.94036406598567, 268.6465762063222}, 900.0},
        {1.2, {-0.5, 21.0}, {0.29910784545371435, 0.68049810226956891}, 42.0},
        {0.308, {-1.0, -25.0}, {1.5911122819435362, 0.94686252142259891}, 50.0},
    };
    for (const auto& c : secondKindWeighted)
        check(c, modifiedBesselFunction_k_exponentiallyWeighted, "weighted K");

    // Too large to hold: infinite, though not with the value's phase
    BOOST_CHECK(std::isinf(std::abs(modifiedBesselFunction_i(0.5, std::complex<Real>(720.0, 1.0)))));
    BOOST_CHECK(std::isinf(std::abs(modifiedBesselFunction_k(0.5, std::complex<Real>(-720.0, 1.0)))));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
