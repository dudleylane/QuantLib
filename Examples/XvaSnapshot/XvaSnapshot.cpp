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

/*
  End-to-end demo of three fork-specific primitives:

    1. CsvQuoteLoader         -- load a name -> value quote snapshot
    2. CurveBucketer          -- tenor-bucketed delta of a priced trade
    3. XvaCalculator          -- CVA from a toy EPE profile

  The example writes a tiny CSV snapshot to a temp file, loads it
  through CsvQuoteLoader, builds a flat-forward discount curve from
  one of the quotes, prices a fixed-rate bond, runs CurveBucketer on
  the input quote set, and computes a CVA adjustment from a hand-
  rolled exposure profile.

  This is a smoke test of the integration surface, not a production
  risk pipeline; no bootstrap, no real exposure simulation.
*/

#include <ql/qldefines.hpp>
#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#  include <ql/auto_link.hpp>
#endif
#include <ql/marketdata/csvquoteloader.hpp>
#include <ql/risk/curvebucketer.hpp>
#include <ql/risk/xvacalculator.hpp>

#include <ql/instruments/bonds/fixedratebond.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/schedule.hpp>
#include <ql/settings.hpp>

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace QuantLib;

namespace {

    // Write a throwaway CSV snapshot so the example is self-contained;
    // in a real flow the path would come from a config file.
    std::string writeSnapshot() {
        char tmpl[] = "/tmp/xva_snapshot_XXXXXX";
        int fd = mkstemp(tmpl);
        QL_REQUIRE(fd != -1, "mkstemp failed");
        std::string path(tmpl);
        close(fd);
        std::ofstream out(path);
        out << "# toy market snapshot\n"
            << "id,value\n"
            << "USD_DISC_FLAT,0.035\n"
            << "USD_HAZ_CPTY,0.02\n"
            << "USD_HAZ_OWN,0.008\n";
        out.close();
        return path;
    }

}

int main(int, char**) {
    try {
        Date today(15, April, 2026);
        Settings::instance().evaluationDate() = today;
        DayCounter dc = Actual365Fixed();
        Calendar cal = TARGET();

        // 1. Load the snapshot.
        std::string path = writeSnapshot();
        CsvQuoteLoader snapshot(path);
        std::remove(path.c_str());
        std::cout << "Loaded " << snapshot.size() << " quotes from snapshot."
                  << std::endl;

        // 2. Build a flat-forward discount curve off one of the loaded
        //    quotes. A real pipeline would feed many quotes into a
        //    PiecewiseYieldCurve bootstrap; this is the minimal wiring
        //    to prove the Handle<Quote> chain is live.
        auto discQuote = snapshot["USD_DISC_FLAT"];
        Handle<YieldTermStructure> discount(ext::make_shared<FlatForward>(
            today, Handle<Quote>(discQuote), dc));

        // 3. Price a 5y 3% semiannual fixed-rate bond.
        Date issue = today;
        Date maturity = today + 5 * Years;
        Schedule schedule(issue, maturity, 6 * Months, cal,
                          ModifiedFollowing, ModifiedFollowing,
                          DateGeneration::Backward, false);
        FixedRateBond bond(/*settlementDays*/ 2, /*faceAmount*/ 100.0,
                           schedule, {0.03}, dc);
        bond.setPricingEngine(
            ext::make_shared<DiscountingBondEngine>(discount));
        std::cout << "Bond NPV = "
                  << std::fixed << std::setprecision(4)
                  << bond.NPV() << std::endl;

        // 4. Bucketed delta. The only SimpleQuote feeding this trade's
        //    curve is USD_DISC_FLAT, so the bucket vector has a single
        //    element equal to the parallel-shift delta.
        std::vector<ext::shared_ptr<SimpleQuote>> bucketQuotes{discQuote};
        CurveBucketer bucketer(bucketQuotes, /*bump=*/ 1.0e-4);
        auto delta = bucketer.bucketedDelta(bond);
        std::cout << "Delta buckets (1bp bump):";
        for (Real d : delta)
            std::cout << " " << d;
        std::cout << std::endl;

        // 5. CVA from a toy EPE profile. In practice this EPE vector
        //    would come from a forward-valuation simulation; here it
        //    is hand-rolled to showcase the integrator.
        std::vector<Date> grid{today,
                                today + 1 * Years,
                                today + 2 * Years,
                                today + 3 * Years,
                                today + 4 * Years,
                                today + 5 * Years};
        std::vector<Real> epe{0.0, 40.0, 60.0, 55.0, 35.0, 0.0};
        std::vector<Real> ene(grid.size(), 0.0);

        auto cptyHaz = snapshot["USD_HAZ_CPTY"]->value();
        auto ownHaz  = snapshot["USD_HAZ_OWN"]->value();
        Handle<DefaultProbabilityTermStructure> cptyPd(
            ext::make_shared<FlatHazardRate>(today, cptyHaz, dc));
        Handle<DefaultProbabilityTermStructure> ownPd(
            ext::make_shared<FlatHazardRate>(today, ownHaz, dc));

        XvaCalculator xva(grid, epe, ene, cptyPd, ownPd,
                          /*cptyRec*/ 0.40, /*ownRec*/ 0.40,
                          /*fundSpread*/ 0.005, /*capSpread*/ 0.010,
                          /*margSpread*/ 0.000,
                          discount);
        std::cout << "CVA = " << xva.cva()
                  << ", DVA = " << xva.dva()
                  << ", FVA = " << xva.fva()
                  << ", KVA = " << xva.kva()
                  << ", Total = " << xva.totalXvaAdjustment()
                  << std::endl;
        return 0;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
