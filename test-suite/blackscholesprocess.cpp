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
#include <ql/math/interpolations/cubicinterpolation.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <cstring>
#include <functional>
#include <iomanip>
#include <typeinfo>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackScholesProcessTests)

namespace
{
    // A spot and two flat curves on quotes. The curves move with the evaluation date, so that moving it notifies a
    // process built on them.
    struct LocalVolMarket
    {
        Date today = Date(23, October, 2026);
        DayCounter dayCounter = Actual365Fixed();
        Calendar calendar = TARGET();
        ext::shared_ptr<SimpleQuote> spot = ext::make_shared<SimpleQuote>(100.0);
        ext::shared_ptr<SimpleQuote> rate = ext::make_shared<SimpleQuote>(0.03);
        ext::shared_ptr<SimpleQuote> dividend = ext::make_shared<SimpleQuote>(0.01);
        Handle<YieldTermStructure> riskFree =
            Handle<YieldTermStructure>(ext::make_shared<FlatForward>(0, calendar, Handle<Quote>(rate), dayCounter));
        Handle<YieldTermStructure> dividendYield =
            Handle<YieldTermStructure>(ext::make_shared<FlatForward>(0, calendar, Handle<Quote>(dividend), dayCounter));

        LocalVolMarket() { Settings::instance().evaluationDate() = today; }

        ext::shared_ptr<BlackScholesMertonProcess> process(const Handle<BlackVolTermStructure>& vol) const
        {
            return ext::make_shared<BlackScholesMertonProcess>(Handle<Quote>(spot), dividendYield, riskFree, vol);
        }

        ext::shared_ptr<BlackVolTermStructure> constant(Volatility vol) const
        {
            return ext::make_shared<BlackConstantVol>(today, calendar, vol, dayCounter);
        }

        // Black volatilities falling from level over two years, on a fixed reference date.
        ext::shared_ptr<BlackVarianceCurve> curve(Volatility level) const
        {
            std::vector<Date> dates;
            std::vector<Volatility> vols;
            for (Integer months : {3, 6, 12, 24})
            {
                dates.push_back(today + Period(months, Months));
                vols.push_back(level - 0.005 * Real(vols.size()));
            }
            return ext::make_shared<BlackVarianceCurve>(today, dates, vols, dayCounter);
        }

        // Black volatilities with a smile, on a fixed reference date.
        ext::shared_ptr<BlackVolTermStructure> surface() const
        {
            std::vector<Date> dates;
            for (Integer months : {3, 6, 12, 24})
                dates.push_back(today + Period(months, Months));
            const std::vector<Real> strikes = {70.0, 85.0, 100.0, 115.0, 130.0};
            Matrix vols(strikes.size(), dates.size());
            for (Size i = 0; i < strikes.size(); ++i)
                for (Size j = 0; j < dates.size(); ++j)
                    vols[i][j] = 0.20 + 0.0004 * (strikes[i] - 100.0) * (strikes[i] - 100.0) / 100.0 - 0.005 * Real(j);
            return ext::make_shared<BlackVarianceSurface>(today, calendar, dates, strikes, vols, dayCounter);
        }
    };

    bool sameBits(Real x, Real y)
    {
        return std::memcmp(&x, &y, sizeof(Real)) == 0;
    }

    // Asks the process for its local volatility, and checks it against the one that a new process on the same
    // handles links: the same type, reference date, maximum date, day counter and extrapolation, and the same local
    // volatilities, bit for bit; then the two processes' drift, diffusion, variance and evolve, bit for bit.
    void checkLocalVolAgainstNewProcess(const GeneralizedBlackScholesProcess& process, const std::string& when)
    {
        const ext::shared_ptr<LocalVolTermStructure> linked = process.localVolatility().currentLink();
        const GeneralizedBlackScholesProcess newProcess(process.stateVariable(), process.dividendYield(),
                                                        process.riskFreeRate(), process.blackVolatility());
        const ext::shared_ptr<LocalVolTermStructure> expected = newProcess.localVolatility().currentLink();
        const LocalVolTermStructure& got = *linked;
        const LocalVolTermStructure& want = *expected;
        if (typeid(got) != typeid(want))
        {
            BOOST_ERROR(when << ": the local volatility isn't of the type a new process links");
            return;
        }
        BOOST_CHECK_MESSAGE(got.referenceDate() == want.referenceDate(), when << ": reference date "
                                                                              << got.referenceDate() << ", expected "
                                                                              << want.referenceDate());
        BOOST_CHECK_MESSAGE(got.maxDate() == want.maxDate(),
                            when << ": maximum date " << got.maxDate() << ", expected " << want.maxDate());
        BOOST_CHECK_MESSAGE(got.dayCounter() == want.dayCounter(),
                            when << ": day counter " << got.dayCounter() << ", expected " << want.dayCounter());
        BOOST_CHECK_MESSAGE(got.allowsExtrapolation() == want.allowsExtrapolation(),
                            when << ": extrapolation " << got.allowsExtrapolation() << ", expected "
                                 << want.allowsExtrapolation());
        for (Time t : {0.1, 0.5, 1.0, 1.5})
            for (Real s : {80.0, 100.0, 120.0})
            {
                const Volatility g = got.localVol(t, s, true);
                const Volatility w = want.localVol(t, s, true);
                BOOST_CHECK_MESSAGE(sameBits(g, w), when << ": local volatility at t = " << t << " and s = " << s
                                                         << " is " << std::setprecision(17) << g << ", expected " << w);
            }
        for (Time t : {0.1, 0.5, 1.0})
            for (Real x : {80.0, 100.0, 120.0})
            {
                const std::pair<const char*, std::pair<Real, Real>> outputs[] = {
                    {"drift", {process.drift(t, x), newProcess.drift(t, x)}},
                    {"diffusion", {process.diffusion(t, x), newProcess.diffusion(t, x)}},
                    {"variance", {process.variance(t, x, 0.25), newProcess.variance(t, x, 0.25)}},
                    {"evolve", {process.evolve(t, x, 0.25, 0.3), newProcess.evolve(t, x, 0.25, 0.3)}}};
                for (const auto& [what, values] : outputs)
                    BOOST_CHECK_MESSAGE(sameBits(values.first, values.second),
                                        when << ": " << what << " at t = " << t << " and x = " << x << " is "
                                             << std::setprecision(17) << values.first << ", expected "
                                             << values.second);
            }
    }
}

BOOST_AUTO_TEST_CASE(testLocalVolKeptWhileItsBlackVolIsUnchanged)
{
    BOOST_TEST_MESSAGE("Testing that a Black-Scholes process keeps the local volatility of a constant or curve Black "
                       "volatility while that is unchanged...");

    LocalVolMarket market;
    const std::pair<std::string, ext::shared_ptr<BlackVolTermStructure>> vols[] = {{"constant", market.constant(0.20)},
                                                                                   {"curve", market.curve(0.22)}};
    for (const auto& [name, vol] : vols)
    {
        Settings::instance().evaluationDate() = market.today;
        const auto process = market.process(Handle<BlackVolTermStructure>(vol));
        const ext::shared_ptr<LocalVolTermStructure> first = process->localVolatility().currentLink();
        Flag notified, relinked;
        notified.registerWith(process);
        relinked.registerWith(process->localVolatility());
        const std::pair<std::string, std::function<void()>> steps[] = {
            {"a spot tick", [&market]() { market.spot->setValue(101.0); }},
            {"a rate tick", [&market]() { market.rate->setValue(0.035); }},
            {"a dividend tick", [&market]() { market.dividend->setValue(0.015); }},
            {"a move of the evaluation date",
             [&market]() { Settings::instance().evaluationDate() = market.today + 3; }}};
        for (const auto& [what, step] : steps)
        {
            const std::string when = name + ", after " + what;
            notified.lower();
            step();
            BOOST_CHECK_MESSAGE(notified.isUp(), when << ": the process wasn't notified");
            checkLocalVolAgainstNewProcess(*process, when);
            BOOST_CHECK_MESSAGE(process->localVolatility().currentLink() == first,
                                when << ": the local volatility was rebuilt");
            BOOST_CHECK_MESSAGE(!relinked.isUp(), when << ": the local volatility's observers were notified");
        }
        market.spot->setValue(100.0);
        market.rate->setValue(0.03);
        market.dividend->setValue(0.01);
    }

    // The local volatility of a BlackVarianceCurve reads the curve as it goes, so it is kept when the curve changes.
    Settings::instance().evaluationDate() = market.today;
    const ext::shared_ptr<BlackVarianceCurve> curve = market.curve(0.22);
    const auto process = market.process(Handle<BlackVolTermStructure>(curve));
    const ext::shared_ptr<LocalVolTermStructure> first = process->localVolatility().currentLink();
    curve->setInterpolation<Cubic>();
    checkLocalVolAgainstNewProcess(*process, "curve, after its interpolation changed");
    BOOST_CHECK_MESSAGE(process->localVolatility().currentLink() == first,
                        "curve, after its interpolation changed: the local volatility was rebuilt");
}

BOOST_AUTO_TEST_CASE(testLocalVolRebuiltWhenItsBlackVolChanges)
{
    BOOST_TEST_MESSAGE("Testing that a Black-Scholes process rebuilds the local volatility of a constant or curve "
                       "Black volatility when that changes...");

    LocalVolMarket market;
    const auto checkRebuilt =
        [](const GeneralizedBlackScholesProcess& process, const std::function<void()>& change, const std::string& when)
    {
        // Held, so that a new local volatility can't be where this one is.
        const ext::shared_ptr<LocalVolTermStructure> before = process.localVolatility().currentLink();
        Flag relinked;
        relinked.registerWith(process.localVolatility());
        change();
        checkLocalVolAgainstNewProcess(process, when);
        BOOST_CHECK_MESSAGE(process.localVolatility().currentLink() != before,
                            when << ": the local volatility wasn't rebuilt");
        BOOST_CHECK_MESSAGE(relinked.isUp(), when << ": the local volatility's observers weren't notified");
    };

    // A constant Black volatility that moves with the evaluation date, on a quote.
    const auto firstQuote = ext::make_shared<SimpleQuote>(0.20);
    RelinkableHandle<Quote> volQuote(firstQuote);
    RelinkableHandle<BlackVolTermStructure> constant(
        ext::make_shared<BlackConstantVol>(0, market.calendar, Handle<Quote>(volQuote), market.dayCounter));
    const auto onConstant = market.process(constant);
    checkRebuilt(*onConstant, [&firstQuote]() { firstQuote->setValue(0.25); }, "constant, after its value changed");
    checkRebuilt(
        *onConstant, [&market]() { Settings::instance().evaluationDate() = market.today + 3; },
        "constant, after its reference date moved");
    checkRebuilt(
        *onConstant,
        [&]()
        {
            constant.linkTo(
                ext::make_shared<BlackConstantVol>(0, market.calendar, Handle<Quote>(volQuote), Actual360()));
        },
        "constant, after another one with the same reference date and value but another day counter");
    checkRebuilt(
        *onConstant, [&volQuote]() { volQuote.linkTo(ext::make_shared<SimpleQuote>(0.0)); },
        "constant, after its value went to 0");
    checkRebuilt(
        *onConstant, [&volQuote]() { volQuote.linkTo(ext::make_shared<SimpleQuote>(-0.0)); },
        "constant, after its value went from 0 to -0");
    checkRebuilt(
        *onConstant,
        [&]()
        {
            onConstant->localVolatility()->enableExtrapolation();
            market.spot->setValue(101.0);
        },
        "constant, after extrapolation was enabled on its local volatility and the spot moved");

    RelinkableHandle<BlackVolTermStructure> curve(market.curve(0.22));
    const auto onCurve = market.process(curve);
    checkRebuilt(*onCurve, [&]() { curve.linkTo(market.curve(0.25)); }, "curve, after another curve");
    checkRebuilt(
        *onCurve,
        [&]()
        {
            onCurve->localVolatility()->enableExtrapolation();
            market.spot->setValue(102.0);
        },
        "curve, after extrapolation was enabled on its local volatility and the spot moved");
}

BOOST_AUTO_TEST_CASE(testLocalVolAfterACopyRelinksIt)
{
    BOOST_TEST_MESSAGE("Testing a Black-Scholes process's local volatility after a copy of the process has relinked "
                       "it...");

    LocalVolMarket market;
    const std::pair<std::string, std::function<ext::shared_ptr<BlackVolTermStructure>(Volatility)>> kinds[] = {
        {"constant", [&market](Volatility v) { return market.constant(v); }},
        {"curve", [&market](Volatility v) { return market.curve(v); }}};
    for (const auto& [name, make] : kinds)
    {
        // A copy shares the process's handles, its local volatility's included. Here the copy relinks it to the
        // local volatility of another Black volatility, and the process's comes back.
        const ext::shared_ptr<BlackVolTermStructure> first = make(0.20);
        RelinkableHandle<BlackVolTermStructure> vol(first);
        const auto process = market.process(vol);
        process->localVolatility();
        const auto copy = ext::make_shared<BlackScholesMertonProcess>(*process);
        vol.linkTo(make(0.30));
        copy->localVolatility();
        vol.linkTo(first);
        checkLocalVolAgainstNewProcess(*process, name + ", after a copy relinked it");
        checkLocalVolAgainstNewProcess(*copy, name + ", the copy, after the process relinked it");
    }

    // The same, with the copy relinking twice before the process's Black volatility comes back, which lets a new
    // local volatility take the memory of the one the process linked. A process that compared addresses would take
    // it for its own.
    {
        auto volQuote = ext::make_shared<SimpleQuote>(0.20);
        const auto process = market.process(Handle<BlackVolTermStructure>(ext::make_shared<BlackConstantVol>(
            market.today, market.calendar, Handle<Quote>(volQuote), market.dayCounter)));
        process->localVolatility();
        const auto copy = ext::make_shared<BlackScholesMertonProcess>(*process);
        volQuote->setValue(0.25);
        copy->localVolatility();
        volQuote->setValue(0.30);
        copy->localVolatility();
        volQuote->setValue(0.20);
        checkLocalVolAgainstNewProcess(*process, "constant, after a copy relinked it twice");
    }
    {
        const ext::shared_ptr<BlackVolTermStructure> first = market.curve(0.20);
        RelinkableHandle<BlackVolTermStructure> vol(first);
        const auto process = market.process(vol);
        process->localVolatility();
        const auto copy = ext::make_shared<BlackScholesMertonProcess>(*process);
        vol.linkTo(market.curve(0.25));
        copy->localVolatility();
        vol.linkTo(market.curve(0.30));
        copy->localVolatility();
        vol.linkTo(first);
        checkLocalVolAgainstNewProcess(*process, "curve, after a copy relinked it twice");
    }

    // A Black volatility replaced by another with the same reference date and value but another day counter, twice,
    // which lets the second take the memory of the first. A process that compared addresses would keep the local
    // volatility of the first.
    {
        RelinkableHandle<BlackVolTermStructure> vol(market.constant(0.20));
        const auto process = market.process(vol);
        process->localVolatility();
        vol.linkTo(market.constant(0.20));
        vol.linkTo(ext::make_shared<BlackConstantVol>(market.today, market.calendar, 0.20, Actual360()));
        checkLocalVolAgainstNewProcess(*process, "constant, after it was replaced twice");
    }
}

BOOST_AUTO_TEST_CASE(testLocalVolAfterItsBlackVolChangesKindAndBack)
{
    BOOST_TEST_MESSAGE("Testing a Black-Scholes process's local volatility as its Black volatility changes from "
                       "constant to curve to surface and back...");

    LocalVolMarket market;
    const ext::shared_ptr<BlackVolTermStructure> constant = market.constant(0.20);
    const ext::shared_ptr<BlackVolTermStructure> curve = market.curve(0.22);
    const ext::shared_ptr<BlackVolTermStructure> surface = market.surface();
    RelinkableHandle<BlackVolTermStructure> vol(constant);
    const auto process = market.process(vol);
    // Held, so that each stays alive and only the handle's link shows that the process no longer links it.
    const ext::shared_ptr<LocalVolTermStructure> constantLocalVol = process->localVolatility().currentLink();
    const std::pair<std::string, ext::shared_ptr<BlackVolTermStructure>> sequence[] = {
        {"surface", surface}, {"constant", constant}, {"curve", curve}, {"surface", surface},
        {"curve", curve},     {"constant", constant}, {"curve", curve}, {"constant", constant}};
    std::string history = "constant";
    ext::shared_ptr<LocalVolTermStructure> curveLocalVol;
    for (const auto& [name, next] : sequence)
    {
        vol.linkTo(next);
        history += " to " + name;
        checkLocalVolAgainstNewProcess(*process, history);
        if (name == "curve" && !curveLocalVol)
            curveLocalVol = process->localVolatility().currentLink();
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
