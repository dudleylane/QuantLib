/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2005 StatPro Italia srl

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

#include <ql/methods/lattices/trinomialtree.hpp>
#include <ql/stochasticprocess.hpp>

namespace QuantLib
{

    namespace
    {

        // Whether the stretch of short steps starting with this one diffuses over less
        // than a tenth of the current spacing, or v sqrt(3) < 0.1 spacing for its variance.
        bool isShortStretch(const StochasticProcess1D& process, const TimeGrid& grid, Size step, Real spacing)
        {
            const Real limit = spacing * spacing / 300.0;
            Real total = 0.0;
            for (Size i = step; i < grid.size() - 1; ++i)
            {
                const Real v2 = process.variance(grid[i], 0.0, grid.dt(i));
                if (v2 >= limit)
                    break;
                total += v2;
                if (total >= limit)
                    return false;
            }
            return true;
        }

    }

    TrinomialTree::TrinomialTree(const ext::shared_ptr<StochasticProcess1D>& process,
                                 const TimeGrid& timeGrid,
                                 bool isPositive)
    : Tree<TrinomialTree>(timeGrid.size()), dx_(1, 0.0), timeGrid_(timeGrid)
    {
        x0_ = process->x0();

        Size nTimeSteps = timeGrid.size() - 1;
        QL_REQUIRE(nTimeSteps > 0, "null time steps for trinomial tree");

        Integer jMin = 0;
        Integer jMax = 0;

        for (Size i = 0; i < nTimeSteps; i++)
        {
            Time t = timeGrid[i];
            Time dt = timeGrid.dt(i);

            // Variance must be independent of x
            Real v2 = process->variance(t, 0.0, dt);
            Volatility v = std::sqrt(v2);
            const Real naturalDx = v * std::sqrt(3.0);
            const bool widened = naturalDx < 0.1 * dx_[i] && isShortStretch(*process, timeGrid, i, dx_[i]);
            const Real dx = widened ? dx_[i] : naturalDx;
            dx_.push_back(dx);
            widening_.push_back(widened ? dx / naturalDx : 1.0);

            Branching branching;
            for (Integer j = jMin; j <= jMax; j++)
            {
                Real x = x0_ + j * dx_[i];
                Real m = process->expectation(t, x, dt);
                auto temp = Integer(std::floor((m - x0_) / dx_[i + 1] + 0.5));

                if (isPositive)
                {
                    while (x0_ + (temp - 1) * dx_[i + 1] <= 0)
                    {
                        temp++;
                    }
                }

                Real e = m - (x0_ + temp * dx_[i + 1]);
                Real e2 = e * e;

                if (widened)
                {
                    // the below, for a spacing other than v sqrt(3)
                    const Real s = (v2 + e2) / (dx * dx);
                    const Real r = e / dx;
                    branching.add(temp, 0.5 * (s - r), 1.0 - s, 0.5 * (s + r));
                }
                else
                {
                    Real e3 = e * std::sqrt(3.0);

                    Real p1 = (1.0 + e2 / v2 - e3 / v) / 6.0;
                    Real p2 = (2.0 - e2 / v2) / 3.0;
                    Real p3 = (1.0 + e2 / v2 + e3 / v) / 6.0;

                    branching.add(temp, p1, p2, p3);
                }
            }
            branchings_.push_back(branching);

            jMin = branching.jMin();
            jMax = branching.jMax();
        }
    }

}
