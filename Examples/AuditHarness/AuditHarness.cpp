// Audit harness: numerical probes flagged by the financial-engineering audit.
// - Greeks: analytic vs bump-and-reprice
// - FD Asian engine: hardcoded spot*0.01 bump behavior at extreme spots
// - Piecewise yield curve bootstrap: inverted / flat / extreme quotes

#include <ql/quantlib.hpp>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace QuantLib;

namespace
{

    struct GreekDiff
    {
        std::string name;
        Real analytic;
        Real bumpAndReprice;
        Real absDiff() const { return std::abs(analytic - bumpAndReprice); }
        Real relDiff() const { return std::abs(analytic) > 1e-12 ? absDiff() / std::abs(analytic) : absDiff(); }
    };

    ext::shared_ptr<BlackScholesMertonProcess> makeBSMProcess(Real spot, Real r, Real q, Real vol)
    {
        Date today = Settings::instance().evaluationDate();
        Calendar cal = TARGET();
        DayCounter dc = Actual365Fixed();
        auto spotQ = ext::make_shared<SimpleQuote>(spot);
        auto rateTS = ext::make_shared<FlatForward>(today, r, dc);
        auto divTS = ext::make_shared<FlatForward>(today, q, dc);
        auto volTS = ext::make_shared<BlackConstantVol>(today, cal, vol, dc);
        return ext::make_shared<BlackScholesMertonProcess>(Handle<Quote>(spotQ), Handle<YieldTermStructure>(divTS),
                                                           Handle<YieldTermStructure>(rateTS),
                                                           Handle<BlackVolTermStructure>(volTS));
    }

    // Analytic European vs bump-and-reprice
    void testEuropeanGreeks()
    {
        std::cout << "\n=== Test 1: European Greeks analytic vs bump ===\n";
        Date today(20, April, 2026);
        Settings::instance().evaluationDate() = today;
        Date expiry = today + Period(6, Months);

        auto payoff = ext::make_shared<PlainVanillaPayoff>(Option::Call, 100.0);
        auto exercise = ext::make_shared<EuropeanExercise>(expiry);

        VanillaOption opt(payoff, exercise);
        Real spot = 100.0, r = 0.03, q = 0.01, vol = 0.20;
        auto proc = makeBSMProcess(spot, r, q, vol);
        opt.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(proc));

        Real npv = opt.NPV();
        Real dAnalytic = opt.delta();
        Real gAnalytic = opt.gamma();
        Real vAnalytic = opt.vega();

        // bump-and-reprice (centered)
        Real bump = 0.01;
        auto p_up = makeBSMProcess(spot * (1 + bump), r, q, vol);
        auto p_dn = makeBSMProcess(spot * (1 - bump), r, q, vol);
        VanillaOption up(payoff, exercise), dn(payoff, exercise);
        up.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(p_up));
        dn.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(p_dn));
        Real dBump = (up.NPV() - dn.NPV()) / (2.0 * spot * bump);
        Real gBump = (up.NPV() - 2.0 * npv + dn.NPV()) / std::pow(spot * bump, 2);

        Real vbump = 0.0001;
        auto p_vu = makeBSMProcess(spot, r, q, vol + vbump);
        auto p_vd = makeBSMProcess(spot, r, q, vol - vbump);
        VanillaOption vu(payoff, exercise), vd(payoff, exercise);
        vu.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(p_vu));
        vd.setPricingEngine(ext::make_shared<AnalyticEuropeanEngine>(p_vd));
        Real vBump = (vu.NPV() - vd.NPV()) / (2.0 * vbump);

        std::vector<GreekDiff> diffs{
            {"delta", dAnalytic, dBump}, {"gamma", gAnalytic, gBump}, {"vega ", vAnalytic, vBump}};
        std::cout << std::fixed << std::setprecision(10);
        for (const auto& d : diffs)
            std::cout << "  " << d.name << "  analytic=" << d.analytic << "  bump=" << d.bumpAndReprice
                      << "  rel_err=" << d.relDiff() << "\n";
    }

    // FE8: FD Asian engine hardcoded spot*0.01 bump — test at tiny and huge spots
    void testAsianFdBumpSensitivity()
    {
        std::cout << "\n=== Test 2: FD Asian hardcoded bump at extreme spots (FE8) ===\n";
        Date today(20, April, 2026);
        Settings::instance().evaluationDate() = today;
        Date expiry = today + Period(1, Years);

        std::vector<Date> fixings;
        for (int m = 1; m <= 12; ++m)
            fixings.push_back(today + Period(m, Months));

        Calendar cal = TARGET();
        Real r = 0.03, q = 0.01, vol = 0.25;

        auto priceAsian = [&](Real spot)
        {
            auto proc = makeBSMProcess(spot, r, q, vol);
            auto payoff = ext::make_shared<PlainVanillaPayoff>(Option::Call, spot);
            auto exercise = ext::make_shared<EuropeanExercise>(expiry);
            DiscreteAveragingAsianOption opt(Average::Arithmetic, 0.0, 0, fixings, payoff, exercise);
            auto eng = ext::make_shared<FdBlackScholesAsianEngine>(proc, 100, 100, 50);
            opt.setPricingEngine(eng);
            return std::make_tuple(opt.NPV(), opt.delta(), opt.gamma());
        };

        for (Real spot : {0.01, 1.0, 100.0, 10000.0})
        {
            auto [npv, delta, gamma] = priceAsian(spot);
            std::cout << "  spot=" << std::setw(9) << spot << "  NPV=" << std::setprecision(6) << npv
                      << "  delta=" << delta << "  gamma=" << std::setprecision(4) << gamma
                      << "  (FD bump used: spot*0.01 = " << spot * 0.01 << ")\n";
        }
        std::cout << "  -- Note: delta should be ~0.5 for ATM Asian regardless of spot.\n";
        std::cout << "  -- Deviation at spot=0.01 or 10000 = evidence FE8 is real.\n";
    }

    // Bootstrap under pathological curves
    void testBootstrapPathological()
    {
        std::cout << "\n=== Test 3: Piecewise yield curve bootstrap — pathological quotes ===\n";
        Date today(20, April, 2026);
        Settings::instance().evaluationDate() = today;
        DayCounter dc = Actual365Fixed();
        Calendar cal = TARGET();

        auto tryBootstrap = [&](const std::string& label, const std::vector<Real>& depositRates,
                                const std::vector<Period>& depositPeriods)
        {
            std::vector<ext::shared_ptr<RateHelper>> helpers;
            for (Size i = 0; i < depositRates.size(); ++i)
            {
                auto q = ext::make_shared<SimpleQuote>(depositRates[i]);
                helpers.push_back(ext::make_shared<DepositRateHelper>(Handle<Quote>(q), depositPeriods[i], 2, cal,
                                                                      Following, false, dc));
            }
            std::cout << "  [" << label << "] rates: ";
            for (Real r : depositRates)
                std::cout << r << " ";
            std::cout << "\n";
            try
            {
                auto curve = ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear>>(today, helpers, dc);
                Real zeroRate5Y = curve->zeroRate(today + Period(5, Years), dc, Continuous).rate();
                std::cout << "    -> bootstrapped OK, 5Y zero = " << std::fixed << std::setprecision(6) << zeroRate5Y
                          << "\n";
            }
            catch (const std::exception& e)
            {
                std::cout << "    -> THREW: " << e.what() << "\n";
            }
        };

        std::vector<Period> periods = {Period(1, Months), Period(3, Months), Period(6, Months),
                                       Period(1, Years),  Period(2, Years),  Period(5, Years)};
        tryBootstrap("normal upward", {0.030, 0.032, 0.034, 0.036, 0.040, 0.045}, periods);
        tryBootstrap("inverted (steep)", {0.080, 0.070, 0.060, 0.050, 0.040, 0.030}, periods);
        tryBootstrap("inverted (mild)", {0.045, 0.044, 0.043, 0.042, 0.041, 0.040}, periods);
        tryBootstrap("flat very-low", {0.001, 0.001, 0.001, 0.001, 0.001, 0.001}, periods);
        tryBootstrap("negative rates", {-0.005, -0.003, -0.001, 0.001, 0.005, 0.010}, periods);
        tryBootstrap("extreme positive", {0.15, 0.20, 0.25, 0.30, 0.35, 0.40}, periods);
        tryBootstrap("humped", {0.02, 0.03, 0.05, 0.07, 0.05, 0.03}, periods);
    }

} // namespace

int main()
{
    std::cout << "QuantLib Audit Harness\n";
    std::cout << "=====================================================\n";
    try
    {
        testEuropeanGreeks();
        testAsianFdBumpSensitivity();
        testBootstrapPathological();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
