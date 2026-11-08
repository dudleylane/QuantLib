/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003, 2004 Ferdinando Ametrano
 Copyright (C) 2003 StatPro Italia srl
 Copyright (C) 2005 Gary Kennedy
 Copyright (C) 2013 Fabien Le Floc'h
 Copyright (C) 2016 Klaus Spanderen


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
#include <ql/math/comparison.hpp>
#include <ql/math/distributions/binomialdistribution.hpp>
#include <ql/math/distributions/bivariatenormaldistribution.hpp>
#include <ql/math/distributions/bivariatestudenttdistribution.hpp>
#include <ql/math/distributions/chisquaredistribution.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/distributions/poissondistribution.hpp>
#include <ql/math/errorfunction.hpp>
#include <ql/math/randomnumbers/stochasticcollocationinvcdf.hpp>
#include <boost/math/distributions/non_central_chi_squared.hpp>
#include <iomanip>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DistributionTests)

Real average = 1.0, sigma = 2.0;

Real gaussian(Real x)
{
    Real normFact = sigma * std::sqrt(2 * M_PI);
    Real dx = x - average;
    return std::exp(-dx * dx / (2.0 * sigma * sigma)) / normFact;
}

Real gaussianDerivative(Real x)
{
    Real normFact = sigma * sigma * sigma * std::sqrt(2 * M_PI);
    Real dx = x - average;
    return -dx * std::exp(-dx * dx / (2.0 * sigma * sigma)) / normFact;
}

struct BivariateTestData
{
    Real a;
    Real b;
    Real rho;
    Real result;
};

template <class Bivariate>
void checkBivariate(const char* tag)
{

    BivariateTestData values[] = {/* The data below are from
                                     "Option pricing formulas", E.G. Haug, McGraw-Hill 1998
                                     pag 193
                                  */
                                  {0.0, 0.0, 0.0, 0.250000},
                                  {0.0, 0.0, -0.5, 0.166667},
                                  {0.0, 0.0, 0.5, 1.0 / 3},
                                  {0.0, -0.5, 0.0, 0.154269},
                                  {0.0, -0.5, -0.5, 0.081660},
                                  {0.0, -0.5, 0.5, 0.226878},
                                  {0.0, 0.5, 0.0, 0.345731},
                                  {0.0, 0.5, -0.5, 0.273122},
                                  {0.0, 0.5, 0.5, 0.418340},

                                  {-0.5, 0.0, 0.0, 0.154269},
                                  {-0.5, 0.0, -0.5, 0.081660},
                                  {-0.5, 0.0, 0.5, 0.226878},
                                  {-0.5, -0.5, 0.0, 0.095195},
                                  {-0.5, -0.5, -0.5, 0.036298},
                                  {-0.5, -0.5, 0.5, 0.163319},
                                  {-0.5, 0.5, 0.0, 0.213342},
                                  {-0.5, 0.5, -0.5, 0.145218},
                                  {-0.5, 0.5, 0.5, 0.272239},

                                  {0.5, 0.0, 0.0, 0.345731},
                                  {0.5, 0.0, -0.5, 0.273122},
                                  {0.5, 0.0, 0.5, 0.418340},
                                  {0.5, -0.5, 0.0, 0.213342},
                                  {0.5, -0.5, -0.5, 0.145218},
                                  {0.5, -0.5, 0.5, 0.272239},
                                  {0.5, 0.5, 0.0, 0.478120},
                                  {0.5, 0.5, -0.5, 0.419223},
                                  {0.5, 0.5, 0.5, 0.546244},

                                  // known analytical values
                                  {0.0, 0.0, std::sqrt(1 / 2.0), 3.0 / 8},

                                  // {  0.0,  big,  any, 0.500000 },
                                  {0.0, 30, -1.0, 0.500000},
                                  {0.0, 30, 0.0, 0.500000},
                                  {0.0, 30, 1.0, 0.500000},

                                  // { big,  big,   any, 1.000000 },
                                  {30, 30, -1.0, 1.000000},
                                  {30, 30, 0.0, 1.000000},
                                  {30, 30, 1.0, 1.000000},

                                  // {-big,  any,   any, 0.000000 }
                                  {-30, -1.0, -1.0, 0.000000},
                                  {-30, 0.0, -1.0, 0.000000},
                                  {-30, 1.0, -1.0, 0.000000},
                                  {-30, -1.0, 0.0, 0.000000},
                                  {-30, 0.0, 0.0, 0.000000},
                                  {-30, 1.0, 0.0, 0.000000},
                                  {-30, -1.0, 1.0, 0.000000},
                                  {-30, 0.0, 1.0, 0.000000},
                                  {-30, 1.0, 1.0, 0.000000}};

    for (Size i = 0; i < std::size(values); i++)
    {
        Bivariate bcd(values[i].rho);
        Real value = bcd(values[i].a, values[i].b);

        Real tolerance = 1.0e-6;
        if (std::fabs(value - values[i].result) >= tolerance)
        {
            BOOST_ERROR(tag << " bivariate cumulative distribution\n"
                            << "    case: " << i + 1 << "\n"
                            << std::fixed << "    a:    " << values[i].a << "\n"
                            << "    b:    " << values[i].b << "\n"
                            << "    rho:  " << values[i].rho << "\n"
                            << std::scientific << "    tabulated value:  " << values[i].result << "\n"
                            << "    result:           " << value);
        }
    }
}

template <class Bivariate>
void checkBivariateAtZero(const char* tag, Real tolerance)
{

    /*
      BVN(0.0,0.0,rho) = 1/4 + arcsin(rho)/(2*M_PI)
      "Handbook of the Normal Distribution",
      J.K. Patel & C.B.Read, 2nd Ed, 1996
    */
    const Real rho[] = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.99999};
    const Real x(0.0);
    const Real y(0.0);

    for (Real i : rho)
    {
        for (Integer sgn = -1; sgn < 2; sgn += 2)
        {
            Bivariate bvn(sgn * i);
            Real expected = 0.25 + std::asin(sgn * i) / (2 * M_PI);
            Real realised = bvn(x, y);

            if (std::fabs(realised - expected) >= tolerance)
            {
                BOOST_ERROR(tag << " bivariate cumulative distribution\n"
                                << std::scientific << "    rho: " << sgn * i << "\n"
                                << "    expected:  " << expected << "\n"
                                << "    realised:  " << realised << "\n"
                                << "    tolerance: " << tolerance);
            }
        }
    }
}

template <class Bivariate>
void checkBivariateTail(const char* tag, Real tolerance)
{

    /* make sure numerical greeks are sensible, numerical error in
     * the tails can make garbage greeks for partial time barrier
     * option */
    Real x = -6.9;
    Real y = 6.9;
    Real corr = -0.999;
    Bivariate bvn(corr);
    for (int i = 0; i < 10; i++)
    {
        Real cdf0 = bvn(x, y);
        y = y + tolerance;
        Real cdf1 = bvn(x, y);
        if (cdf0 > cdf1)
        {
            BOOST_ERROR(tag << " cdf must be decreasing in the tails\n"
                            << std::scientific << "    cdf0: " << cdf0 << "\n"
                            << "    cdf1: " << cdf1 << "\n"
                            << "    x: " << x << "\n"
                            << "    y: " << y << "\n"
                            << "    rho: " << corr);
        }
    }
}

struct BivariateStudentTestData
{
    Natural n;
    Real rho;
    Real x;
    Real y;
    Real result;
};

class InverseNonCentralChiSquared
{
  public:
    InverseNonCentralChiSquared(Real df, Real ncp) : dist_(df, ncp) {}

    Real operator()(Real x) const { return boost::math::quantile(dist_, x); }

  private:
    const boost::math::non_central_chi_squared_distribution<Real> dist_;
};


BOOST_AUTO_TEST_CASE(testNormal)
{

    BOOST_TEST_MESSAGE("Testing normal distributions...");

    InverseCumulativeNormal invCumStandardNormal;
    Real check = invCumStandardNormal(0.5);
    if (check != 0.0e0)
    {
        BOOST_ERROR("C++ inverse cumulative of the standard normal at 0.5 is "
                    << std::scientific << check << "\n instead of zero: something is wrong!");
    }

    NormalDistribution normal(average, sigma);
    CumulativeNormalDistribution cum(average, sigma);
    InverseCumulativeNormal invCum(average, sigma);

    Size numberOfStandardDeviation = 6;
    Real xMin = average - numberOfStandardDeviation * sigma, xMax = average + numberOfStandardDeviation * sigma;
    Size N = 100001;
    Real h = (xMax - xMin) / (N - 1);

    std::vector<Real> x(N), y(N), yd(N), temp(N), diff(N);

    Size i;
    for (i = 0; i < N; i++)
        x[i] = xMin + h * i;
    std::transform(x.begin(), x.end(), y.begin(), gaussian);
    std::transform(x.begin(), x.end(), yd.begin(), gaussianDerivative);

    // check that normal = Gaussian
    std::transform(x.begin(), x.end(), temp.begin(), normal);
    std::transform(y.begin(), y.end(), temp.begin(), diff.begin(), std::minus<>());
    Real e = norm(diff.begin(), diff.end(), h);
    if (e > 1.0e-16)
    {
        BOOST_ERROR("norm of C++ NormalDistribution minus analytic Gaussian: " << std::scientific << e << "\n"
                                                                               << "tolerance exceeded");
    }

    // check that invCum . cum = identity
    std::transform(x.begin(), x.end(), temp.begin(), cum);
    std::transform(temp.begin(), temp.end(), temp.begin(), invCum);
    std::transform(x.begin(), x.end(), temp.begin(), diff.begin(), std::minus<>());
    e = norm(diff.begin(), diff.end(), h);
    if (e > 1.0e-7)
    {
        BOOST_ERROR("norm of invCum . cum minus identity: " << std::scientific << e << "\n"
                                                            << "tolerance exceeded");
    }

    MaddockInverseCumulativeNormal mInvCum(average, sigma);
    std::transform(x.begin(), x.end(), diff.begin(), [&](Real x) -> Real { return x - mInvCum(cum(x)); });

    e = norm(diff.begin(), diff.end(), h);
    if (e > 1.0e-7)
    {
        BOOST_ERROR("norm of MaddokInvCum . cum minus identity: " << std::scientific << e << "\n"
                                                                  << "tolerance exceeded");
    }

    // check that cum.derivative = Gaussian
    for (i = 0; i < x.size(); i++)
        temp[i] = cum.derivative(x[i]);
    std::transform(y.begin(), y.end(), temp.begin(), diff.begin(), std::minus<>());
    e = norm(diff.begin(), diff.end(), h);
    if (e > 1.0e-16)
    {
        BOOST_ERROR("norm of C++ Cumulative.derivative minus analytic Gaussian: " << std::scientific << e << "\n"
                                                                                  << "tolerance exceeded");
    }

    // check that normal.derivative = gaussianDerivative
    for (i = 0; i < x.size(); i++)
        temp[i] = normal.derivative(x[i]);
    std::transform(yd.begin(), yd.end(), temp.begin(), diff.begin(), std::minus<>());
    e = norm(diff.begin(), diff.end(), h);
    if (e > 1.0e-16)
    {
        BOOST_ERROR("norm of C++ Normal.derivative minus analytic derivative: " << std::scientific << e << "\n"
                                                                                << "tolerance exceeded");
    }
}

BOOST_AUTO_TEST_CASE(testBivariate)
{

    BOOST_TEST_MESSAGE("Testing bivariate cumulative normal distribution...");

    checkBivariateAtZero<BivariateCumulativeNormalDistributionDr78>("Drezner 1978", 1.0e-6);
    checkBivariate<BivariateCumulativeNormalDistributionDr78>("Drezner 1978");

    // due to relative low accuracy of Dr78, it does not pass with a
    // smaller perturbation
    checkBivariateTail<BivariateCumulativeNormalDistributionDr78>("Drezner 1978", 1.0e-5);

    checkBivariateAtZero<BivariateCumulativeNormalDistributionWe04DP>("West 2004", 1.0e-15);
    checkBivariate<BivariateCumulativeNormalDistributionWe04DP>("West 2004");

    checkBivariateTail<BivariateCumulativeNormalDistributionWe04DP>("West 2004", 1.0e-6);
    checkBivariateTail<BivariateCumulativeNormalDistributionWe04DP>("West 2004", 1.0e-8);
}

BOOST_AUTO_TEST_CASE(testPoisson)
{

    BOOST_TEST_MESSAGE("Testing Poisson distribution...");

    for (Real mean = 0.0; mean <= 10.0; mean += 0.5)
    {
        BigNatural i = 0;
        PoissonDistribution pdf(mean);
        Real calculated = pdf(i);
        Real logHelper = -mean;
        Real expected = std::exp(logHelper);
        Real error = std::fabs(calculated - expected);
        if (error > 1.0e-16)
            BOOST_ERROR("Poisson pdf(" << mean << ")(" << i << ")\n"
                                       << std::setprecision(16) << "    calculated: " << calculated << "\n"
                                       << "    expected:   " << expected << "\n"
                                       << "    error:      " << error);

        for (i = 1; i < 25; i++)
        {
            calculated = pdf(i);
            if (mean == 0.0)
            {
                expected = 0.0;
            }
            else
            {
                logHelper = logHelper + std::log(mean) - std::log(Real(i));
                expected = std::exp(logHelper);
            }
            error = std::fabs(calculated - expected);
            if (error > 1.0e-13)
                BOOST_ERROR("Poisson pdf(" << mean << ")(" << i << ")\n"
                                           << std::setprecision(13) << "    calculated: " << calculated << "\n"
                                           << "    expected:   " << expected << "\n"
                                           << "    error:      " << error);
        }
    }
}

BOOST_AUTO_TEST_CASE(testCumulativePoisson)
{

    BOOST_TEST_MESSAGE("Testing cumulative Poisson distribution...");

    for (Real mean = 0.0; mean <= 10.0; mean += 0.5)
    {
        BigNatural i = 0;
        CumulativePoissonDistribution cdf(mean);
        Real cumCalculated = cdf(i);
        Real logHelper = -mean;
        Real cumExpected = std::exp(logHelper);
        Real error = std::fabs(cumCalculated - cumExpected);
        if (error > 1.0e-13)
            BOOST_ERROR("Poisson cdf(" << mean << ")(" << i << ")\n"
                                       << std::setprecision(13) << "    calculated: " << cumCalculated << "\n"
                                       << "    expected:   " << cumExpected << "\n"
                                       << "    error:      " << error);
        for (i = 1; i < 25; i++)
        {
            cumCalculated = cdf(i);
            if (mean == 0.0)
            {
                cumExpected = 1.0;
            }
            else
            {
                logHelper = logHelper + std::log(mean) - std::log(Real(i));
                cumExpected += std::exp(logHelper);
            }
            error = std::fabs(cumCalculated - cumExpected);
            if (error > 1.0e-12)
                BOOST_ERROR("Poisson cdf(" << mean << ")(" << i << ")\n"
                                           << std::setprecision(12) << "    calculated: " << cumCalculated << "\n"
                                           << "    expected:   " << cumExpected << "\n"
                                           << "    error:      " << error);
        }
    }
}

BOOST_AUTO_TEST_CASE(testInverseCumulativePoisson)
{

    BOOST_TEST_MESSAGE("Testing inverse cumulative Poisson distribution...");

    InverseCumulativePoisson icp(1.0);

    Real data[] = {0.2, 0.5, 0.9, 0.98, 0.99, 0.999, 0.9999, 0.99995, 0.99999, 0.999999, 0.9999999, 0.99999999};

    for (Size i = 0; i < std::size(data); i++)
    {
        if (!close(icp(data[i]), static_cast<Real>(i)))
        {
            BOOST_ERROR(std::setprecision(8) << "failed to reproduce known value for x = " << data[i] << "\n"
                                             << "    calculated: " << icp(data[i]) << "\n"
                                             << "    expected:   " << Real(i));
        }
    }
}

BOOST_AUTO_TEST_CASE(testBivariateCumulativeStudent)
{
    BOOST_TEST_MESSAGE("Testing bivariate cumulative Student t distribution...");

    Real xs[14] = {0.00, 0.50, 1.00, 1.50, 2.00, 2.50, 3.00, 4.00, 5.00, 6.00, 7.00, 8.00, 9.00, 10.00};
    Natural ns[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 25, 30, 60, 90, 120, 150, 300, 600};
    // Part of table 1 from the reference paper
    Real expected1[280] = {
        0.33333, 0.50000, 0.63497, 0.72338, 0.78063, 0.81943, 0.84704, 0.88332, 0.90590, 0.92124, 0.93231, 0.94066,
        0.94719, 0.95243, 0.33333, 0.52017, 0.68114, 0.78925, 0.85607, 0.89754, 0.92417, 0.95433, 0.96978, 0.97862,
        0.98411, 0.98774, 0.99026, 0.99208, 0.33333, 0.52818, 0.70018, 0.81702, 0.88720, 0.92812, 0.95238, 0.97667,
        0.98712, 0.99222, 0.99497, 0.99657, 0.99756, 0.99821, 0.33333, 0.53245, 0.71052, 0.83231, 0.90402, 0.94394,
        0.96612, 0.98616, 0.99353, 0.99664, 0.99810, 0.99885, 0.99927, 0.99951, 0.33333, 0.53510, 0.71701, 0.84196,
        0.91449, 0.95344, 0.97397, 0.99095, 0.99637, 0.99836, 0.99918, 0.99956, 0.99975, 0.99985, 0.33333, 0.53689,
        0.72146, 0.84862, 0.92163, 0.95972, 0.97893, 0.99365, 0.99779, 0.99913, 0.99962, 0.99982, 0.99990, 0.99995,
        0.33333, 0.53819, 0.72470, 0.85348, 0.92679, 0.96415, 0.98230, 0.99531, 0.99857, 0.99950, 0.99981, 0.99992,
        0.99996, 0.99998, 0.33333, 0.53917, 0.72716, 0.85719, 0.93070, 0.96743, 0.98470, 0.99639, 0.99903, 0.99970,
        0.99990, 0.99996, 0.99998, 0.99999, 0.33333, 0.53994, 0.72909, 0.86011, 0.93375, 0.96995, 0.98650, 0.99713,
        0.99931, 0.99981, 0.99994, 0.99998, 0.99999, 1.00000, 0.33333, 0.54056, 0.73065, 0.86247, 0.93621, 0.97194,
        0.98788, 0.99766, 0.99950, 0.99988, 0.99996, 0.99999, 1.00000, 1.00000, 0.33333, 0.54243, 0.73540, 0.86968,
        0.94362, 0.97774, 0.99168, 0.99890, 0.99985, 0.99998, 1.00000, 1.00000, 1.00000, 1.00000, 0.33333, 0.54338,
        0.73781, 0.87336, 0.94735, 0.98053, 0.99337, 0.99932, 0.99993, 0.99999, 1.00000, 1.00000, 1.00000, 1.00000,
        0.33333, 0.54395, 0.73927, 0.87560, 0.94959, 0.98216, 0.99430, 0.99952, 0.99996, 1.00000, 1.00000, 1.00000,
        1.00000, 1.00000, 0.33333, 0.54433, 0.74025, 0.87709, 0.95108, 0.98322, 0.99489, 0.99963, 0.99998, 1.00000,
        1.00000, 1.00000, 1.00000, 1.00000, 0.33333, 0.54528, 0.74271, 0.88087, 0.95482, 0.98580, 0.99623, 0.99983,
        0.99999, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 0.33333, 0.54560, 0.74354, 0.88215, 0.95607, 0.98663,
        0.99664, 0.99987, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 0.33333, 0.54576, 0.74396, 0.88279,
        0.95669, 0.98704, 0.99683, 0.99989, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 0.33333, 0.54586,
        0.74420, 0.88317, 0.95706, 0.98729, 0.99695, 0.99990, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000,
        0.33333, 0.54605, 0.74470, 0.88394, 0.95781, 0.98777, 0.99717, 0.99992, 1.00000, 1.00000, 1.00000, 1.00000,
        1.00000, 1.00000, 0.33333, 0.54615, 0.74495, 0.88432, 0.95818, 0.98801, 0.99728, 0.99993, 1.00000, 1.00000,
        1.00000, 1.00000, 1.00000, 1.00000};
    // Part of table 2 from the reference paper
    Real expected2[280] = {
        0.16667, 0.36554, 0.54022, 0.65333, 0.72582, 0.77465, 0.80928, 0.85466, 0.88284, 0.90196, 0.91575, 0.92616,
        0.93429, 0.94081, 0.16667, 0.38889, 0.59968, 0.73892, 0.82320, 0.87479, 0.90763, 0.94458, 0.96339, 0.97412,
        0.98078, 0.98518, 0.98823, 0.99044, 0.16667, 0.39817, 0.62478, 0.77566, 0.86365, 0.91391, 0.94330, 0.97241,
        0.98483, 0.99086, 0.99410, 0.99598, 0.99714, 0.99790, 0.16667, 0.40313, 0.63863, 0.79605, 0.88547, 0.93396,
        0.96043, 0.98400, 0.99256, 0.99614, 0.99782, 0.99868, 0.99916, 0.99944, 0.16667, 0.40620, 0.64740, 0.80900,
        0.89902, 0.94588, 0.97007, 0.98972, 0.99591, 0.99816, 0.99909, 0.99951, 0.99972, 0.99983, 0.16667, 0.40829,
        0.65345, 0.81794, 0.90820, 0.95368, 0.97607, 0.99290, 0.99755, 0.99904, 0.99958, 0.99980, 0.99989, 0.99994,
        0.16667, 0.40980, 0.65788, 0.82449, 0.91482, 0.95914, 0.98010, 0.99482, 0.99844, 0.99946, 0.99979, 0.99991,
        0.99996, 0.99998, 0.16667, 0.41095, 0.66126, 0.82948, 0.91981, 0.96314, 0.98295, 0.99605, 0.99895, 0.99968,
        0.99989, 0.99996, 0.99998, 0.99999, 0.16667, 0.41185, 0.66393, 0.83342, 0.92369, 0.96619, 0.98506, 0.99689,
        0.99926, 0.99980, 0.99994, 0.99998, 0.99999, 1.00000, 0.16667, 0.41257, 0.66608, 0.83661, 0.92681, 0.96859,
        0.98667, 0.99748, 0.99946, 0.99987, 0.99996, 0.99999, 1.00000, 1.00000, 0.16667, 0.41476, 0.67268, 0.84633,
        0.93614, 0.97550, 0.99103, 0.99884, 0.99984, 0.99998, 1.00000, 1.00000, 1.00000, 1.00000, 0.16667, 0.41586,
        0.67605, 0.85129, 0.94078, 0.97877, 0.99292, 0.99930, 0.99993, 0.99999, 1.00000, 1.00000, 1.00000, 1.00000,
        0.16667, 0.41653, 0.67810, 0.85430, 0.94356, 0.98066, 0.99396, 0.99950, 0.99996, 1.00000, 1.00000, 1.00000,
        1.00000, 1.00000, 0.16667, 0.41698, 0.67947, 0.85632, 0.94540, 0.98189, 0.99461, 0.99962, 0.99998, 1.00000,
        1.00000, 1.00000, 1.00000, 1.00000, 0.16667, 0.41810, 0.68294, 0.86141, 0.94998, 0.98483, 0.99607, 0.99982,
        0.99999, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 0.16667, 0.41847, 0.68411, 0.86312, 0.95149, 0.98577,
        0.99651, 0.99987, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 0.16667, 0.41866, 0.68470, 0.86398,
        0.95225, 0.98623, 0.99672, 0.99989, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 0.16667, 0.41877,
        0.68505, 0.86449, 0.95270, 0.98650, 0.99684, 0.99990, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000,
        0.16667, 0.41900, 0.68576, 0.86552, 0.95360, 0.98705, 0.99707, 0.99992, 1.00000, 1.00000, 1.00000, 1.00000,
        1.00000, 1.00000, 0.16667, 0.41911, 0.68612, 0.86604, 0.95405, 0.98731, 0.99719, 0.99993, 1.00000, 1.00000,
        1.00000, 1.00000, 1.00000, 1.00000};

    Real tolerance = 1.0e-5;
    for (Size i = 0; i < std::size(ns); ++i)
    {
        BivariateCumulativeStudentDistribution f1(ns[i], 0.5);
        BivariateCumulativeStudentDistribution f2(ns[i], -0.5);
        for (Size j = 0; j < std::size(xs); ++j)
        {
            Real calculated1 = f1(xs[j], xs[j]);
            Real reference1 = expected1[i * std::size(xs) + j];
            Real calculated2 = f2(xs[j], xs[j]);
            Real reference2 = expected2[i * std::size(xs) + j];
            if (std::fabs(calculated1 - reference1) > tolerance)
                BOOST_ERROR("Failed to reproduce CDF value at " << xs[j] << "\n    calculated: " << calculated1
                                                                << "\n    expected:   " << reference1);
            if (std::fabs(calculated2 - reference2) > tolerance)
                BOOST_ERROR("Failed to reproduce CDF value at " << xs[j] << "\n    calculated: " << calculated2
                                                                << "\n    expected:   " << reference1);
        }
    }

    // a few more random cases
    BivariateStudentTestData cases[] = {
        {2, -1.0, 5.0, 8.0, 0.973491},      {2, 1.0, -2.0, 8.0, 0.091752},     {2, 1.0, 5.25, -9.5, 0.005450},
        {3, -0.5, -5.0, -5.0, 0.000220},    {4, -1.0, -8.0, 7.5, 0.0},         {4, 0.5, -5.5, 10.0, 0.002655},
        {4, 1.0, -5.0, 6.0, 0.003745},      {4, 1.0, 6.0, 5.5, 0.997336},      {5, -0.5, -7.0, -6.25, 0.000004},
        {5, -0.5, 3.75, -7.25, 0.000166},   {5, -0.5, 7.75, -1.25, 0.133073},  {6, 0.0, 7.5, 3.25, 0.991149},
        {7, -0.5, -1.0, -8.5, 0.000001},    {7, -1.0, -4.25, -4.0, 0.0},       {7, 0.0, 0.5, -2.25, 0.018819},
        {8, -1.0, 8.25, 1.75, 0.940866},    {8, 0.0, 2.25, 4.75, 0.972105},    {9, -0.5, -4.0, 8.25, 0.001550},
        {9, -1.0, -1.25, -8.75, 0.0},       {9, -1.0, 5.75, -6.0, 0.0},        {9, 0.5, -6.5, -9.5, 0.000001},
        {9, 1.0, -2.0, 9.25, 0.038276},     {10, -1.0, -0.5, 6.0, 0.313881},   {10, 0.5, 0.0, 9.25, 0.5},
        {10, 0.5, 6.75, -2.25, 0.024090},   {10, 1.0, -1.75, -1.0, 0.055341},  {15, 0.0, -1.25, -4.75, 0.000029},
        {15, 0.0, -2.0, -1.5, 0.003411},    {15, 0.5, 3.0, -3.25, 0.002691},   {20, -0.5, 2.0, -1.25, 0.098333},
        {20, -1.0, 3.0, 8.0, 0.996462},     {20, 0.0, -7.5, 1.5, 0.0},         {20, 0.5, 1.25, 9.75, 0.887136},
        {25, -1.0, -4.25, 5.0, 0.000111},   {25, 0.5, 9.5, -1.5, 0.073069},    {25, 1.0, -6.5, -3.25, 0.0},
        {30, -1.0, -7.75, 10.0, 0.0},       {30, 1.0, 0.5, 9.5, 0.689638},     {60, -1.0, -3.5, -8.25, 0.0},
        {60, -1.0, 4.25, 0.75, 0.771869},   {60, -1.0, 5.75, 3.75, 0.9998},    {60, 0.5, -4.5, 8.25, 0.000016},
        {60, 1.0, 6.5, -4.0, 0.000088},     {90, -0.5, -3.75, -2.75, 0.0},     {90, 0.5, 8.75, -7.0, 0.0},
        {120, 0.0, -3.5, -9.25, 0.0},       {120, 0.0, -8.25, 5.0, 0.0},       {120, 1.0, -0.75, 3.75, 0.227361},
        {120, 1.0, -3.5, -8.0, 0.0},        {150, 0.0, 10.0, -1.75, 0.041082}, {300, -0.5, -6.0, 3.75, 0.0},
        {300, -0.5, 3.5, -4.5, 0.000004},   {300, 0.0, 6.5, -5.0, 0.0},        {600, -0.5, 9.25, 1.5, 0.93293},
        {600, -1.0, -9.25, 1.5, 0.0},       {600, 0.5, -5.0, 8.0, 0.0},        {600, 1.0, -2.75, -9.0, 0.0},
        {1000, -0.5, -2.5, 0.25, 0.000589}, {1000, -0.5, 3.0, 1.0, 0.839842},  {2000, -1.0, 9.0, -4.75, 0.000001},
        {2000, 0.5, 9.75, 7.25, 1.0},       {2000, 1.0, 0.75, -9.0, 0.0},      {5000, -0.5, 9.75, 5.5, 1.0},
        {5000, -1.0, 6.0, 1.0, 0.841321},   {5000, 1.0, 4.0, -7.75, 0.0},      {10000, 0.5, 1.5, 6.0, 0.933177}};

    tolerance = 1.0e-6;
    for (auto& i : cases)
    {
        BivariateCumulativeStudentDistribution f(i.n, i.rho);
        Real calculated = f(i.x, i.y);
        Real expected = i.result;
        if (std::fabs(calculated - expected) > tolerance)
            BOOST_ERROR("Failed to reproduce CDF value:" << "\n    n:   " << i.n << "\n    rho: " << i.rho
                                                         << "\n    x:   " << i.x << "\n    y:   " << i.y
                                                         << "\n    calculated: " << calculated
                                                         << "\n    expected:   " << expected);
    }
}

BOOST_AUTO_TEST_CASE(testBivariateCumulativeStudentVsBivariate)
{
    BOOST_TEST_MESSAGE("Testing bivariate cumulative Student t distribution for large N...");

    Natural n = 10000; // for this value, the distribution should be
                       // close to a bivariate normal distribution.

    for (Real rho = -1.0; rho < 1.01; rho += 0.25)
    {
        BivariateCumulativeStudentDistribution T(n, rho);
        BivariateCumulativeNormalDistribution N(rho);

        Real avgDiff = 0.0;
        Size m = 0;
        Real tolerance = 4.0e-5;
        for (Real x = -10; x < 10.1; x += 0.5)
        {
            for (Real y = -10; y < 10.1; y += 0.5)
            {
                Real calculated = T(x, y);
                Real expected = N(x, y);
                Real diff = std::fabs(calculated - expected);
                if (diff > tolerance)
                    BOOST_ERROR("Failed to reproduce limit value:" << "\n    rho: " << rho << "\n    x:   " << x
                                                                   << "\n    y:   " << y << "\n    calculated: "
                                                                   << calculated << "\n    expected:   " << expected);

                avgDiff += diff;
                ++m;
            }
        }
        avgDiff /= m;
        if (avgDiff > 3.0e-6)
            BOOST_ERROR("Failed to reproduce average limit value:" << "\n    rho: " << rho
                                                                   << "\n    average error: " << avgDiff);
    }
}

BOOST_AUTO_TEST_CASE(testInvCDFviaStochasticCollocation)
{
    BOOST_TEST_MESSAGE("Testing inverse CDF based on stochastic collocation...");

    const Real k = 3.0;
    const Real lambda = 1.0;

    const InverseCumulativeNormal invNormalCDF;
    const CumulativeNormalDistribution normalCDF;
    const InverseNonCentralChiSquared invCDF(k, lambda);

    const StochasticCollocationInvCDF scInvCDF10(invCDF, 10);

    // low precision
    for (Real x = -3.0; x < 3.0; x += 0.1)
    {
        const Real u = normalCDF(x);

        const Real calculated1 = scInvCDF10(u);
        const Real calculated2 = scInvCDF10.value(x);
        const Real expected = invCDF(u);

        if (std::fabs(calculated1 - calculated2) > 1e-6)
        {
            BOOST_FAIL("Failed to reproduce equal stochastic collocation "
                       "inverse CDF"
                       << "\n    x: " << x << "\n    calculated via normal distribution : " << calculated2
                       << "\n    calculated via uniform distribution: " << calculated1
                       << "\n    diff: " << calculated1 - calculated2);
        }

        const Real tol = 1e-2;
        if (std::fabs(calculated2 - expected) > tol)
        {
            BOOST_FAIL("Failed to reproduce invCDF with "
                       "stochastic collocation method"
                       << "\n    x: " << x << "\n    invCDF  :" << expected << "\n    scInvCDF: " << calculated2
                       << "\n    diff    : " << std::fabs(expected - calculated2) << "\n    tol     : " << tol);
        }
    }

    // high precision
    const StochasticCollocationInvCDF scInvCDF30(invCDF, 30, 0.9999999);
    for (Real x = -4.0; x < 4.0; x += 0.1)
    {
        const Real u = normalCDF(x);

        const Real expected = invCDF(u);
        const Real calculated = scInvCDF30(u);

        const Real tol = 1e-6;
        if (std::fabs(calculated - expected) > tol)
        {
            BOOST_FAIL("Failed to reproduce invCDF with "
                       "stochastic collocation method"
                       << "\n    x: " << x << "\n    invCDF  :" << expected << "\n    scInvCDF: " << calculated
                       << "\n    diff    : " << std::fabs(expected - calculated) << "\n    tol     : " << tol);
        }
    }
}

BOOST_AUTO_TEST_CASE(testSankaranApproximation)
{
    BOOST_TEST_MESSAGE("Testing Sankaran approximation for the "
                       "non-central cumulative chi-square distribution...");

    const Real dfs[] = {2, 2, 2, 4, 4};
    const Real ncps[] = {1, 2, 3, 1, 2, 3};

    const Real tol = 0.01;
    for (Real df : dfs)
    {
        for (Real ncp : ncps)
        {
            const NonCentralCumulativeChiSquareDistribution d(df, ncp);
            const NonCentralCumulativeChiSquareSankaranApprox sankaran(df, ncp);

            for (Real x = 0.25; x < 10; x += 0.1)
            {
                const Real expected = d(x);
                const Real calculated = sankaran(x);
                const Real diff = std::fabs(expected - calculated);

                if (diff > tol)
                {
                    BOOST_ERROR("Failed to match accuracy of Sankaran approximation"
                                ""
                                "\n    df        : "
                                << df << "\n    ncp       : " << ncp << "\n    x         : " << x
                                << "\n    expected  : " << expected << "\n    calculated: " << calculated
                                << "\n    diff      : " << diff << "\n    tol       : " << tol);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testPeizerPrattInversion)
{
    BOOST_TEST_MESSAGE("Testing the Peizer-Pratt method 2 inversion...");

    struct Case
    {
        Real z;
        BigNatural n;
        Real expected;
    };

    // These were evaluated from the definition with 120 digits.
    const Case cases[] = {{0.0, 5, 0.5},
                          {1.0, 5, 0.7031970887828103},
                          {-1.0, 5, 0.29680291121718977},
                          {5.0, 101, 0.7336195122592148},
                          {-5.0, 101, 0.26638048774078515},
                          {-20.0, 101, 0.004882000575164496},
                          {-40.0, 101, 3.5650828031884475e-08},
                          {205.0, 801, 1.0},
                          {-205.0, 801, 4.232649749828366e-24},
                          {-400.0, 801, 5.030718277654203e-88},
                          {0.001, 101, 0.5000496286192958},
                          {-0.001, 101, 0.4999503713807042},
                          {1.0e-9, 801, 0.5000000000176611},
                          {-1.0e-9, 801, 0.4999999999823389}};

    for (const auto& c : cases)
    {
        const Real calculated = PeizerPrattMethod2Inversion(c.z, c.n);
        if (!(std::fabs(calculated - c.expected) <= 1.0e-13 * c.expected))
            BOOST_ERROR("Peizer-Pratt inversion at z = " << c.z << " with n = " << c.n << "\n"
                                                         << std::setprecision(17) << "    calculated: " << calculated
                                                         << "\n    expected:   " << c.expected);
    }

    BOOST_CHECK_EQUAL(PeizerPrattMethod2Inversion(1.0e4, 5), 1.0);
    BOOST_CHECK_EQUAL(PeizerPrattMethod2Inversion(-1.0e4, 5), 0.0);
    BOOST_CHECK_EXCEPTION(PeizerPrattMethod2Inversion(1.0, 4), Error, ExpectedErrorMessage("n must be an odd number"));
}

BOOST_AUTO_TEST_CASE(testLogCumulativeNormal)
{
    BOOST_TEST_MESSAGE("Testing the logarithm of the cumulative normal distribution...");

    // Calculated with 60 digits as log(erfc(-z / sqrt(2)) / 2).
    const struct
    {
        Real x, average, sigma, expected;
    } cases[] = {{0.0, 0.0, 1.0, -0.69314718055994530942},    {3.0, 0.0, 1.0, -0.0013508099647481937988},
                 {10.0, 0.0, 1.0, -7.619853024160526066e-24}, {-1.0, 0.0, 1.0, -1.8410216450092635058},
                 {-5.0, 0.0, 1.0, -15.064998393988725736},    {-5.6, 0.0, 1.0, -18.351379495909332066},
                 {-6.0, 0.0, 1.0, -20.736768949974705655},    {-10.0, 0.0, 1.0, -53.231285150512470578},
                 {-36.9, 0.0, 1.0, -685.33288316535061216},   {-37.0, 0.0, 1.0, -689.0305855768905936},
                 {-37.1, 0.0, 1.0, -692.73828071562329266},   {-37.5, 0.0, 1.0, -707.66898931750719107},
                 {-38.0, 0.0, 1.0, -726.5572160188201301},    {-39.0, 0.0, 1.0, -765.0831565643775444},
                 {-40.0, 0.0, 1.0, -804.60844201375378817},   {-100.0, 0.0, 1.0, -5005.5242086942050886},
                 {-1000.0, 0.0, 1.0, -500007.82669481218431}, {-1.0e5, 0.0, 1.0, -5000000012.4318639983},
                 {0.0, 1.0, 2.0, -1.1759117615936186089},     {-79.0, 1.0, 2.0, -804.60844201375378817},
                 {-1000.0, 5.0, 0.5, -2020058.5248287817763}};

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("x " << c.x << ", average " << c.average << ", sigma " << c.sigma)
        {
            const CumulativeNormalDistribution f(c.average, c.sigma);
            const Real scale = std::max(std::fabs(c.expected), Real(1.0));
            BOOST_CHECK_SMALL((f.logValue(c.x) - c.expected) / scale, 1.0e-14);

            // Where the value hasn't underflowed, it agrees with the operator.
            const Real value = f(c.x);
            if (value > 0.0)
                BOOST_CHECK_SMALL(f.logValue(c.x) - std::log(value), 1.0e-7);
        }
    }
}

BOOST_AUTO_TEST_CASE(testErrorFunctionAndCumulativeNormalWhereTheySaturate)
{
    BOOST_TEST_MESSAGE("Testing the error function and cumulative normal where they saturate...");

    // erfc(6) is about 2e-17, so erf rounds to exactly 1 from there on.
    const ErrorFunction erf;
    for (Real x : {6.0, 6.5, 7.0, 10.0, 28.0, 100.0})
    {
        BOOST_CHECK_EQUAL(erf(x), 1.0);
        BOOST_CHECK_EQUAL(erf(-x), -1.0);
    }

    Size decreases = 0;
    Real previous = erf(5.5);
    for (Size i = 1; i <= 1000; ++i)
    {
        const Real y = erf(5.5 + i * 0.001);
        if (y < previous || y > 1.0)
            ++decreases;
        previous = y;
    }
    BOOST_CHECK_EQUAL(decreases, 0U);

    const CumulativeNormalDistribution cumulative;
    for (Real z : {8.5, 9.0, 10.0, 20.0, 40.0})
        BOOST_CHECK_EQUAL(cumulative(z), 1.0);

    decreases = 0;
    previous = cumulative(7.0);
    for (Size i = 1; i <= 3000; ++i)
    {
        const Real p = cumulative(7.0 + i * 0.001);
        if (p < previous || p > 1.0)
            ++decreases;
        previous = p;
    }
    BOOST_CHECK_EQUAL(decreases, 0U);
}

BOOST_AUTO_TEST_CASE(testBivariateDr78AtDegenerateCorrelations)
{
    BOOST_TEST_MESSAGE("Testing Drezner's bivariate normal at degenerate correlations...");

    const Real arguments[][2] = {{0.1, 0.0},  {-0.5, 0.3}, {0.5, 0.5}, {-0.5, -0.5},     {0.0, 0.0},
                                 {0.5, -0.5}, {0.3, 0.5},  {0.5, 0.3}, {5.6e-17, 0.766}, {-0.766, -1e-9}};
    const CumulativeNormalDistribution normal;
    for (Real rho : {-1.0, 1.0})
    {
        const BivariateCumulativeNormalDistributionDr78 drezner(rho);
        for (const auto& pair : arguments)
        {
            const Real a = pair[0], b = pair[1];
            BOOST_TEST_CONTEXT("a = " << a << ", b = " << b << ", rho = " << rho)
            {
                // At rho = 1, Y = X, and at rho = -1, Y = -X.
                const Real expected =
                    rho == 1.0 ? normal(std::min(a, b)) : std::max<Real>(0.0, normal(a) + normal(b) - 1.0);
                BOOST_CHECK_SMALL(drezner(a, b) - expected, 1e-15);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testBivariateDr78NearDegenerateInputs)
{
    BOOST_TEST_MESSAGE("Testing Drezner's bivariate normal near zero and near degenerate correlations...");

    // Drezner's quadrature is only good to about 1e-6.
    const Real tolerance = 1e-6;
    const Real nearOne = std::nextafter(Real(1.0), Real(0.0));
    const std::vector<Real> rhos = {nearOne, 1.0 - 1e-12, 1.0 - 1e-6, 0.999, 0.707, 0.5, 1e-9};
    // The smallest of these make a * b * rho underflow.
    const std::vector<Real> sizes = {1e-300, 1e-100, 5.6e-17, 1e-9, 3e-9, 1e-4, 0.5, 0.766, 1.0};

    for (Real r : rhos)
    {
        for (Real rho : {-r, r})
        {
            const BivariateCumulativeNormalDistributionDr78 drezner(rho);
            const BivariateCumulativeNormalDistributionWe04DP west(rho);
            for (Real h : sizes)
            {
                for (Real k : sizes)
                {
                    for (Real a : {-h, h})
                    {
                        for (Real b : {-k, k})
                        {
                            BOOST_TEST_CONTEXT("a = " << a << ", b = " << b << ", rho = " << rho)
                            {
                                Real calculated = Null<Real>();
                                BOOST_CHECK_NO_THROW(calculated = drezner(a, b));
                                BOOST_CHECK_SMALL(calculated - west(a, b), tolerance);
                            }
                        }
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testInverseCumulativePoissonAgainstExactQuantiles)
{
    BOOST_TEST_MESSAGE("Testing inverse cumulative Poisson distribution against exact quantiles...");

    const Real aboveHalf = std::nextafter(0.5, 1.0), belowOne = std::nextafter(1.0, 0.0);

    // The smallest k with P(N <= k) >= p, from the terms of the sum at 80 digits.
    const struct
    {
        Real lambda, probability;
        BigNatural expected;
    } cases[] = {
        {0.05, 0.001, 0},        {0.05, 0.96, 1},
        {0.05, 0.999, 2},        {0.05, 1.0 - 1e-12, 6},
        {0.05, belowOne, 8},     {1.0, 0.3, 0},
        {1.0, 0.5, 1},           {1.0, aboveHalf, 1},
        {1.0, 0.9, 2},           {1.0, 1.0 - 1e-12, 14},
        {1.0, belowOne, 17},     {4.0, 0.3, 3},
        {4.0, 0.5, 4},           {4.0, 0.9, 7},
        {4.0, 0.999999999, 21},  {4.0, 1.0 - 1e-12, 25},
        {4.0, belowOne, 29},     {5.0, 0.3, 4},
        {20.0, 0.01, 10},        {20.0, 0.5, 20},
        {20.0, 0.999, 35},       {20.0, 1.0 - 1e-12, 59},
        {20.0, belowOne, 67},    {100.0, 1e-30, 10},
        {100.0, 0.5, 100},       {100.0, 0.999999999, 166},
        {100.0, belowOne, 193},  {130.0, 0.5, 130},
        {130.0, 0.999, 167},     {150.0, 0.3, 143},
        {150.0, 0.5, 150},       {150.0, aboveHalf, 150},
        {150.0, 0.9, 166},       {200.0, 0.001, 158},
        {200.0, 0.5, 200},       {200.0, 0.9, 218},
        {500.0, 1e-10, 365},     {500.0, 0.5, 500},
        {745.0, 1e-100, 246},    {745.0, 0.5, 745},
        {746.0, 1e-320, 2},      {746.0, 1e-308, 7},
        {746.0, 1e-300, 11},     {746.0, 1e-100, 247},
        {746.0, 0.5, 746},       {746.0, belowOne, 981},
        {800.0, 1e-300, 26},     {800.0, 0.5, 800},
        {1000.0, 1e-320, 74},    {1000.0, 1e-310, 83},
        {1000.0, 1e-308, 85},    {1000.0, 1e-300, 93},
        {1000.0, 1e-30, 660},    {1000.0, 1e-10, 806},
        {1000.0, 0.5, 1000},     {1000.0, aboveHalf, 1000},
        {1000.0, 0.9, 1041},     {1000.0, belowOne, 1270},
        {10000.0, 1e-100, 7949}, {10000.0, 0.5, 10000},
        {10000.0, 0.999, 10310}, {100000.0, 1e-100, 93348},
        {100000.0, 0.5, 100000}, {100000.0, belowOne, 102607},
        {1e+06, 1e-320, 961976}, {1e+06, 1e-308, 962695},
        {1e+06, 1e-300, 963182}, {1e+06, 1e-10, 993645},
        {1e+06, 0.5, 1000000},   {1e+06, belowOne, 1008221},
        {4e+07, 0.5, 40000000},
    };

    for (const auto& data : cases)
    {
        BOOST_TEST_CONTEXT("lambda = " << data.lambda << ", p = " << std::setprecision(17) << data.probability)
        {
            BOOST_CHECK_EQUAL(InverseCumulativePoisson(data.lambda)(data.probability), Real(data.expected));
        }
    }
}

BOOST_AUTO_TEST_CASE(testInverseCumulativePoissonAtTheProbabilityOfZero)
{
    BOOST_TEST_MESSAGE("Testing inverse cumulative Poisson distribution at the probability of zero...");

    for (Real lambda : {1e-3, 0.5, 1.0, 4.0, 100.0, 745.0, 746.0, 1000.0, 1e6})
    {
        BOOST_TEST_CONTEXT("lambda = " << lambda)
        {
            const InverseCumulativePoisson inverse(lambda);
            BOOST_CHECK_EQUAL(inverse(0.0), 0.0);
            if (lambda < 745.0)
            {
                BOOST_CHECK_EQUAL(inverse(std::exp(-lambda)), 0.0);
                BOOST_CHECK_EQUAL(inverse(std::nextafter(std::exp(-lambda), 1.0)), 1.0);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testInverseCumulativePoissonAtTheLimitsOfItsRange)
{
    BOOST_TEST_MESSAGE("Testing inverse cumulative Poisson distribution at the limits of its range...");

    const Real naturalMax = Real((std::numeric_limits<Natural>::max)());

    for (Real lambda : {naturalMax, std::ldexp(1.0, 64), 1e300, std::numeric_limits<Real>::infinity()})
    {
        BOOST_TEST_CONTEXT("lambda = " << lambda)
        {
            const InverseCumulativePoisson inverse(lambda);
            BOOST_CHECK_EXCEPTION(inverse(0.9), Error, ExpectedErrorMessage("exceeds the supported range"));
            BOOST_CHECK_EQUAL(inverse(0.0), 0.0);
        }
    }

    // The term at the mode comes from logs, which are good to about a part in 1e5 here,
    // so a few steps out of a standard deviation of 65,536.
    const InverseCumulativePoisson inverse(naturalMax - 1.0);
    BOOST_CHECK_SMALL(inverse(1e-10) - 4294550404.0, 16.0);
    BOOST_CHECK_SMALL(inverse(0.5) - 4294967294.0, 16.0);
    BOOST_CHECK_SMALL(inverse(1.0 - 1e-12) - 4295428314.0, 16.0);
    BOOST_CHECK_SMALL(inverse(std::nextafter(1.0, 0.0)) - 4295505325.0, 16.0);
}

BOOST_AUTO_TEST_CASE(testNonCentralChiSquareLargeArguments)
{
    BOOST_TEST_MESSAGE("Testing the non-central chi-square distribution at large arguments...");

    // From the same series in extended precision, starting from logarithms at 60 digits.
    // Scipy's distribution agrees with all of them to 2e-12.
    struct Case
    {
        Real df, ncp, x, expected;
    };
    const Case cases[] = {
        {0.5, 1500.0, 1113.169392, 4.19135243790489e-08},
        {0.5, 1500.0, 1423.033878, 0.15861385532739386},
        {0.5, 1500.0, 1500.5, 0.5051500367005389},
        {0.5, 1500.0, 1577.966122, 0.8413840605198919},
        {0.5, 1500.0, 1887.830608, 0.9999988518600118},
        {4.8, 1500.0, 1117.191951, 4.2032369968378124e-08},
        {4.8, 1500.0, 1427.27839, 0.15861391612318904},
        {4.8, 1500.0, 1504.8, 0.5051438950272767},
        {4.8, 1500.0, 1582.32161, 0.841384004045095},
        {4.8, 1500.0, 1892.408049, 0.9999988534699408},
        {1000.0, 1500.0, 2052.786405, 6.666596254645121e-08},
        {1000.0, 1500.0, 2410.557281, 0.15862496956473318},
        {1000.0, 1500.0, 2500.0, 0.5040888313713823},
        {1000.0, 1500.0, 2589.442719, 0.8413737301676228},
        {1000.0, 1500.0, 2947.213595, 0.9999991066617216},
        {0.5, 5000.0, 4293.375541, 1.0908394404062739e-07},
        {0.5, 5000.0, 4859.075108, 0.15864298136406033},
        {0.5, 5000.0, 5000.5, 0.5028209009086403},
        {0.5, 5000.0, 5141.924892, 0.8413566763485716},
        {0.5, 5000.0, 5707.624459, 0.999999355892552},
        {4.8, 5000.0, 4297.523534, 1.0912602135127735e-07},
        {4.8, 5000.0, 4863.344707, 0.15864298765053436},
        {4.8, 5000.0, 5004.8, 0.5028198905675442},
        {4.8, 5000.0, 5146.255293, 0.8413566703002905},
        {4.8, 5000.0, 5712.076466, 0.9999993560623925},
        {1000.0, 5000.0, 5258.380151, 1.182159610564638e-07},
        {1000.0, 5000.0, 5851.67603, 0.1586441345254822},
        {1000.0, 5000.0, 6000.0, 0.5026082202232409},
        {1000.0, 5000.0, 6148.32397, 0.8413555693299892},
        {1000.0, 5000.0, 6741.619849, 0.9999993909329751},
        {0.5, 1000000.0, 990000.49875, 2.6924974449364617e-07},
        {0.5, 1000000.0, 998000.49975, 0.15865519337821582},
        {0.5, 1000000.0, 1000000.5, 0.5001994711235781},
        {0.5, 1000000.0, 1002000.50025, 0.8413448065007987},
        {0.5, 1000000.0, 1010000.50125, 0.99999969505823},
        {4.8, 1000000.0, 990004.788, 2.6924977491816146e-07},
        {4.8, 1000000.0, 998004.7976, 0.1586551933781844},
        {4.8, 1000000.0, 1000004.8, 0.5001994707662023},
        {4.8, 1000000.0, 1002004.8024, 0.841344806500841},
        {4.8, 1000000.0, 1010004.812, 0.9999996950582631},
        {1000.0, 1000000.0, 990997.500312, 2.6925681368149025e-07},
        {1000.0, 1000000.0, 998999.500062, 0.15865519334984113},
        {1000.0, 1000000.0, 1001000.0, 0.5001993880957443},
        {1000.0, 1000000.0, 1003000.499938, 0.8413448065291429},
        {1000.0, 1000000.0, 1011002.499688, 0.9999996950660257},
        {0.5, 100000000.0, 99900000.499875, 2.8487194465984237e-07},
        {0.5, 100000000.0, 99980000.499975, 0.15865525332641955},
        {0.5, 100000000.0, 100000000.5, 0.5000199471140034},
        {0.5, 100000000.0, 100020000.500025, 0.8413447466734595},
        {0.5, 100000000.0, 100100000.500125, 0.9999997115599177},
        {4.8, 100000000.0, 99900004.7988, 2.848719446925609e-07},
        {4.8, 100000000.0, 99980004.79976, 0.15865525332645516},
        {4.8, 100000000.0, 100000004.8, 0.5000199471135819},
        {4.8, 100000000.0, 100020004.80024, 0.8413447466733422},
        {4.8, 100000000.0, 100100004.8012, 0.9999997115599071},
        {1000.0, 100000000.0, 99900999.75, 2.8487195203076447e-07},
        {1000.0, 100000000.0, 99980999.95, 0.1586552533257526},
        {1000.0, 100000000.0, 100001000.0, 0.5000199470309324},
        {1000.0, 100000000.0, 100021000.05, 0.8413447466741263},
        {1000.0, 100000000.0, 100101000.25, 0.9999997115599253},
        {0.5, 1000000000.0, 999683772.733944, 2.860878454875545e-07},
        {0.5, 1000000000.0, 999936754.946789, 0.1586552538721085},
        {0.5, 1000000000.0, 1000000000.5, 0.5000063078313045},
        {0.5, 1000000000.0, 1000063246.053211, 0.8413447461278878},
        {0.5, 1000000000.0, 1000316228.266056, 0.9999997127838134},
        {4.8, 1000000000.0, 999683777.033604, 2.8608784548650815e-07},
        {4.8, 1000000000.0, 999936759.246721, 0.1586552538720797},
        {4.8, 1000000000.0, 1000000004.8, 0.5000063078309984},
        {4.8, 1000000000.0, 1000063250.353279, 0.8413447461280198},
        {4.8, 1000000000.0, 1000316232.566396, 0.9999997127838269},
        {1000.0, 1000000000.0, 999684772.154926, 2.8608784570541715e-07},
        {1000.0, 1000000000.0, 999937754.430985, 0.15865525386992113},
        {1000.0, 1000000000.0, 1000001000.0, 0.5000063078286776},
        {1000.0, 1000000000.0, 1000064245.569015, 0.841344746130075},
        {1000.0, 1000000000.0, 1000317227.845074, 0.9999997127838135},
        {4.8, 10.0, 1500.0, 1.0},
        {4.8, 10.0, 2000.0, 1.0},
        {4.8, 10.0, 10000.0, 1.0},
        {100.0, 10.0, 1500.0, 1.0},
        {100.0, 10.0, 2000.0, 1.0},
        {100.0, 10.0, 10000.0, 1.0},
        {10000.0, 100.0, 9385.857157, 1.5422861898605878e-07},
        {10000.0, 100.0, 10100.0, 0.5018803611574579},
        {10000.0, 100.0, 10814.142843, 0.9999994996114697},
        {10000.0, 100.0, 11385.457117, 1.0},
        {1000000.0, 0.0, 992928.932188, 2.702302792987702e-07},
        {1000000.0, 0.0, 1000000.0, 0.5001880631966055},
        {1000000.0, 0.0, 1007071.067812, 0.9999996961192954},
        {1000000.0, 0.0, 1012727.922061, 1.0},
        {1000000.0, 1000.0, 993921.864652, 2.702303029918246e-07},
        {1000000.0, 1000.0, 1001000.0, 0.5001880629154491},
        {1000000.0, 1000.0, 1008078.135348, 0.999999696119322},
        {1000000.0, 1000.0, 1013740.643626, 1.0},
        {200000000.0, 1000.0, 199900999.500001, 2.8546421397837515e-07},
        {200000000.0, 1000.0, 200001000.0, 0.5000132980760136},
        {200000000.0, 1000.0, 200101000.499999, 0.9999997121570313},
        {200000000.0, 1000.0, 200181000.899998, 1.0},
        {19.0, 10.0, 29.0, 0.537969338975302},
        {19.0, 10.0, 55.495283, 0.9939587333800997},
        {19.0, 2000.0, 1929.345106, 0.15862445952500637},
        {19.0, 2000.0, 2287.964682, 0.9982314885240675},
        {21.0, 10.0, 31.0, 0.5366489429006447},
        {21.0, 10.0, 58.166155, 0.9941432246877262},
        {21.0, 2000.0, 1931.322801, 0.15862447500935864},
        {21.0, 2000.0, 2290.031597, 0.9982316695883051},
        {40.0, 10.0, 50.0, 0.5283615644474651},
        {40.0, 10.0, 82.863353, 0.995292971899427},
        {40.0, 2000.0, 1950.11118, 0.15862462284818804},
        {40.0, 2000.0, 2309.666461, 0.9982333800596184},
        {1.0, 1400.0, 1500.0, 0.90545225557805575},
        {1.0, 1400.0, 1612.0, 0.996863373460395},
        {1.0, 1400.0, 1800.0, 0.99999972761356726},
        {1.0, 1400.0, 1930.0, 0.99999999996370148},
        {4.8, 1200.0, 1600.0, 0.99999994457661268},
        {100.0, 1400.0, 1900.0, 0.99999962521940899},
        {28.0, 0.0, 28.0, 0.53555243510314332},
        {28.0, 0.0, 40.0, 0.9338723590408341},
        {28.0, 0.0, 60.0, 0.99959271629471314},
        {28.0, 10.0, 45.0, 0.77544084704670635},
        {28.0, 10.0, 70.0, 0.99690402219694607},
        {1.0, 2000000000.0, 2000000001.0, 0.5000044603102897},
        {4.8, 10.0, 1000000000000.0, 1.0},
        {1000000.0, 100000000.0, 1000000000000000.0, 1.0},
        {4.8, 5000.0, 2000.0, 0.0},
        {4.8, 1000000.0, 100000.0, 0.0},
        {1000.0, 1.0, 1.0, 0.0},
        {1000.0, 0.0, 1e-300, 0.0},
        {4.8, 10000000000.0, 200000000000.0, 1.0},
    };

    // The series stops when what's left is below 1e-12.
    const Real tolerance = 2e-12;
    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("df: " << c.df << ", ncp: " << c.ncp << ", x: " << c.x)
        {
            const Real calculated = NonCentralCumulativeChiSquareDistribution(c.df, c.ncp)(c.x);
            BOOST_CHECK_SMALL(calculated - c.expected, tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testNonCentralChiSquareBeyondItsRange)
{
    BOOST_TEST_MESSAGE("Testing the non-central chi-square distribution beyond its range...");

    // These need more terms than the limit allows.  The first used to return what it had summed
    // by then, which is about 0 where 1 is right.
    BOOST_CHECK_EXCEPTION(NonCentralCumulativeChiSquareDistribution(2e13, 0.0)(2e13 + 1.2e8), Error,
                          ExpectedErrorMessage("didn't converge"));
    BOOST_CHECK_EXCEPTION(NonCentralCumulativeChiSquareDistribution(1.0, 4e9)(4e9 + 6.0 * std::sqrt(1.6e10)), Error,
                          ExpectedErrorMessage("didn't converge"));
    BOOST_CHECK_EXCEPTION(NonCentralCumulativeChiSquareDistribution(1.0, 1e10)(1e10), Error,
                          ExpectedErrorMessage("non-centrality parameter too large"));

    // The bound on the upper tail overflows here, so the terms would have to be walked through.
    BOOST_CHECK_EXCEPTION(NonCentralCumulativeChiSquareDistribution(4.8, 10.0)(1e308), Error,
                          ExpectedErrorMessage("didn't converge"));
}

BOOST_AUTO_TEST_CASE(testInverseNonCentralChiSquareLargeArguments)
{
    BOOST_TEST_MESSAGE("Testing the inverse non-central chi-square distribution at large arguments...");

    // The extended-precision series, bisected.
    struct Case
    {
        Real df, ncp, p, expected;
    };
    const Case cases[] = {
        {4.8, 5000.0, 0.0001, 4491.581138941798},
        {4.8, 5000.0, 0.5, 5003.8001266337415},
        {4.8, 5000.0, 0.9999, 5543.677776322924},
        {1.0, 1000000.0, 0.0001, 992575.7981127077},
        {1.0, 1000000.0, 0.5, 1000000.0},
        {1.0, 1000000.0, 0.9999, 1007451.8640545302},
        {1000.0, 20000.0, 0.0001, 19947.77424913588},
        {1000.0, 20000.0, 0.5, 20999.008122385378},
        {1000.0, 20000.0, 0.9999, 22077.67944589658},
    };

    for (const auto& c : cases)
    {
        BOOST_TEST_CONTEXT("df: " << c.df << ", ncp: " << c.ncp << ", p: " << c.p)
        {
            const Real calculated = InverseNonCentralCumulativeChiSquareDistribution(c.df, c.ncp, 100)(c.p);
            BOOST_CHECK_SMALL(calculated / c.expected - 1.0, 1e-10);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
