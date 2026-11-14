
# QuantLib — dudleylane fork

[![Licensed under AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0--or--later-red.svg)](https://github.com/dudleylane/QuantLib/blob/master/LICENSE)
[![Upstream BSD-3-Clause](https://img.shields.io/badge/Upstream-BSD--3--Clause-blue.svg)](https://github.com/dudleylane/QuantLib/blob/master/LICENSE.TXT)

> **This is a personal fork of [lballabio/QuantLib](https://github.com/lballabio/QuantLib).**
> It was taken from upstream's master on 2026-04-19 and does not track
> upstream: nothing upstream has merged since, including its 1.43 and 1.44
> releases, has come across. Several correctness fixes have been applied
> that are **not** in upstream. Master history has been rewritten to remove a
> 79 MB legacy binary blob (2014), so every commit from upstream's
> `51d4bb377` on has a different SHA here.
>
> **License note:** this fork's *combined work* and all *new* files introduced in
> the fork (e.g. `ql/indexes/fallbackiborindex.{hpp,cpp}`) are distributed under
> the **GNU Affero General Public License v3.0 or later (AGPL-3.0+)**. Upstream
> QuantLib files retain their original **BSD-3-Clause** license — see the
> [License](#license) section for the full picture.

---

## About upstream QuantLib

The QuantLib project (<https://www.quantlib.org>) is aimed at providing a
comprehensive software framework for quantitative finance. QuantLib is
a free/open-source library for modeling, trading, and risk management
in real-life.

QuantLib is Non-Copylefted Free Software and OSI Certified Open Source
Software. License is BSD-3-Clause (unchanged in this fork).

Upstream documentation: <https://www.quantlib.org/docs.shtml>.
Upstream release history: <https://www.quantlib.org/reference/history.html>.

## Is this fork right for you?

See **[USE_CASES.md](./USE_CASES.md)** for honest per-use-case verdicts
and effort-to-production estimates: quant-model prototyping, vanilla
/ common-exotic pricing, desk-level risk + XVA, bank-wide regulatory
capital, and HFT / low-latency. The summary one-liner:
production-ready for model prototyping and vanilla pricing; partial
foundations for risk/XVA (expect 2–4 engineer-months on top); MVP for
FRTB-SA GIRR; not a target for HFT.

## What's different in this fork

### Toolchain floor (raised vs. upstream)

Enforced at configure time in `CMakeLists.txt` and `acinclude.m4`:

- **GCC ≥ 15.2.1**
- **C++ ≥ 23**
- **Boost ≥ 1.83.0**

Upstream still accepts GCC 9+, C++17, and Boost 1.58. This fork will
refuse to configure on an older toolchain.

### Supported platform

RHEL/CentOS 10 and later is the only supported platform, and the only
one CI builds. The Windows and macOS build files are upstream's:
`QuantLib.sln`, the `*.vcxproj` files, and the `windows-*` and
`apple-arm64-*` presets in `CMakePresets.json`. The project files stay
in the tree, but the file-list gate (`tools/check_filelists_diffs.py`)
no longer checks them, and no CI job builds any of them.

### Correctness fixes applied (not in upstream)

| ID | File | Fix |
|---|---|---|
| F1 | `ql/models/shortrate/onefactormodels/hullwhite.cpp` | Bond-option volatility in a numerically stable factored form, the spot-start volatility times `e^{-a(S-M)}` (Brigo-Mercurio; factored by #113) — no more flooring at 0 |
| F2 | `ql/processes/gsrprocesscore.cpp` | Out-of-range `time2()` now validates `T_ ≥ times_.back()` on demand |
| F3 | `ql/models/volatility/garch.cpp` | GARCH(1,1) calibration now checks optimizer `EndCriteria::Type` |
| FE1 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | BAW Newton loop bounded (`maxIterations = 100`) — fixes infinite-loop on pathological inputs |
| FE2 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | BAW negative-rate error message now points users to `BjerksundStenslandApproximationEngine`, which handles negative rates natively |
| FE3 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp`, `juquadraticengine.cpp` | T → 0⁺ short-circuit to the value of the best deterministic exercise time; until #37 it was European + intrinsic, in BAW only |
| #36 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp`, `juquadraticengine.cpp` | BAW and Ju return the European value wherever early exercise is never optimal. Calls with r < q ≤ 0 no longer get the European value, which can fall below intrinsic (they get FE2's error), puts at negative rates no longer throw, and Ju no longer returns NaN at r = 0 |
| #37 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.{hpp,cpp}`, `juquadraticengine.cpp` | BAW and Ju no longer throw at low volatility where the carry works against early exercise (puts with r > q, calls with q > r): the critical-price seed falls back to the perpetual critical price when it leaves its range. Ju no longer throws at zero volatility |
| FE6 | `ql/termstructures/volatility/sabr.hpp` | Public `sabrIsRiskyRegime()` detector (Doust 2012 absorption threshold) + documentation pointing to `NoArbSabrSmileSection` for arbitrage-free pricing |
| FE11 | `ql/pricingengines/asian/analytic_discr_geom_av_price_heston.{hpp,cpp}` | Moved out of `ql/experimental/`; the public Heston-Asian MC engine no longer transitively imports experimental code |
| #41 | `ql/time/date.cpp` | `Date::todaysDate()` is thread-safe: `localtime_r` replaces `std::localtime`, whose buffer every thread shared. A time-zone change made while the program runs now applies once the program calls `tzset()` |
| #84, #125, #126 | `ql/indexes/iborindex.hpp`, `ql/indexes/fallbackiborindex.{hpp,cpp}`, `ql/experimental/coupons/proxyibor.{hpp,cpp}` | Coupons on a `FallbackIborIndex` use the fallback, where after cessation they priced off the legacy curve with no spread, and coupons on a `ProxyIbor` forecast through the proxied index, where they threw. `IborIndex`'s three-argument `forecastFixing` is protected and virtual now, which changes its ABI |
| #85 | `ql/pricingengines/blackformulatemplate.hpp` | `blackFormulaT<T>` computes as `blackFormula` does, with no subtraction of normal tails, so a put no longer cancels, down to a negative price, once N(d) nears 1 |
| #46, #47, #108, #128 | `ql/pricingengines/vanilla/` | BAW and Ju are floored at the European value, the intrinsic value and the value of exercising at any fixed time, and Ju's correction saturates rather than reaching its pole, so Ju departs from the published formula there. The QD engines are floored at intrinsic, and FD at the payoff where the option can be exercised today. Bjerksund-Stensland no longer returns NaN, and takes the best flat trigger where the value still rises above the 1993 paper's, where it collapsed to the European value under negative carry |
| #114 | `ql/cashflows/cashflows.{hpp,cpp}`, `ql/time/daycounters/actualactual.cpp`, `ql/interestrate.cpp`, `ql/time/period.cpp`, `ql/time/schedule.cpp` | `modifiedDuration`, `simpleDuration` and `convexity` at a yield are the npv's derivatives under every compounding rule, which changes them for plain `Simple` on any leg of more than one flow. `BondFunctions::yield` costs 75.5 % fewer instructions, `basisPointValue`'s second-order term is the right size, and Act/Act ISMA keeps its dates on a long first coupon with an irregular last one |
| #86, #87, #123 | `ql/indexes/indexmanager.{hpp,cpp}`, `ql/index.hpp`, `ql/time/daycounters/business252.cpp` | Threads pricing their own graphs no longer race on `IndexManager`, whose fixing lookups stop writing to it and whose notifier map takes a lock, or on `Business252`'s caches. Writes of fixings still need the caller to serialize them, and building one index from several threads still needs thread-safe observers |
| #109–#113, #115–#118, #129, #138, #141, #142 | see `FORK_CHANGES.md` | Barrier, digital, lookback, Asian and exotic engines, trees, short-rate models, swaps, FX forwards, convertible and callable bonds, and out-of-bounds accesses. Upstream also fixed most of these after the fork point; each is the fork's own fix, written against a case that fails here. `FORK_CHANGES.md` says what changes in each, values included |
| #120, #163–#169 | `ql/time/calendars/` | **The default `Israel()` opens on Fridays**: it follows the Bank of Israel's Zahav settlement system, Sunday to Friday, and no longer equals `Israel(TASE)`, which trades Monday to Friday from January 4th, 2026. India follows NSE Clearing's settlement holidays from 2012, Saudi Arabia the exchange's notices and sessions, and Indonesia has its holidays from 2015. France's Ascension and Whit Monday move with Easter, and South Korea, Hong Kong, Singapore, South Africa, Romania, Botswana, Italy and Taiwan gain or move holidays. Schedules built on these calendars move |
| #121, #156–#158 | `ql/experimental/credit/` | Loss models shared between baskets no longer give a tranche the other basket's numbers. Once a name defaults, baskets and their models keep each name's data with it, where they read a neighbour's, read past a vector, or refused, as base correlation did. `Pool::defaultKeys()` follows `names()`. Layouts change on `Basket`, `Pool` and every loss model |
| #119 | `ql/pricingengines/blackformula.cpp` | Li-RS and the RS approximation keep their digits near the money, far out of it and at intrinsic value, where they threw or gave NaN, and Bachelier's inversion takes prices on their intrinsic value. In-the-money probabilities have their limits at zero volatility |
| #118, #147 | `ql/math/`, `ql/experimental/math/` | **`ZigguratGaussianRng`'s sequence changes**, since its wedge test had a misplaced parenthesis. `InverseCumulativePoisson`, the non-central chi-square, `modifiedBesselFunction_k`, Dr78's bivariate normal, Gauss weights at high orders, Gauss-Jacobi near its singular parameters, and the Farlie-Gumbel-Morgenstern and Clayton copulas no longer give wrong values or NaN, and `GaussKronrodAdaptive` stops where an interval can't be split, where it overflowed the stack |
| #118, #119, #121, #155 | see `FORK_CHANGES.md` | Smile sections at large strikes and forwards, swaption cubes, CMS calibration and spread pricing, YoY optionlets, inflation volatility surfaces, Heston SLV's Monte Carlo calibration, default-curve jumps, ZABR strike lists, GARCH calibration, `AlphaFinder`, hybrid simulated annealing, float-float swaptions and cat bonds. `FORK_CHANGES.md` says what changes in each, values included |
| #131, #174–#178 | short-rate processes and pricers | Hull-White, hybrid Heston-Hull-White, G2, Ornstein-Uhlenbeck and Geman-Roncoroni processes, the arithmetic-averaged overnight pricer, `AnalyticBSMHullWhiteEngine` and `LinearTsrPricer` stay right at small mean reversions, where they gave wrong values, NaN or infinity. **Negative mean reversions move at ordinary sizes** in `HullWhiteForwardProcess` and the generalized Hull-White classes |
| #135, #136, #180–#185, #187, #189 | `ql/pricingengines/barrier/`, `ql/pricingengines/exotic/`, `ql/pricingengines/americanpayoffathit.cpp`, `ql/math/distributions/bivariatenormaldistribution.cpp` | Soft barriers are priced by their definition, where Haug's formula was wrong past the strike or the spot. Partial-time, two-asset and compound options use the double-precision bivariate normal, and partial-time, two-asset and double-barrier binary options price at low and zero volatility. Rebates and at-hit digitals price where a negative rate made them NaN. Some partial-time options are now refused (a spot past the barrier, a cover date passed). `BivariateCumulativeNormalDistributionWe04DP::logValue()` is new |
| #134, #137, #140, #144, #145, #152, #153, #172 | see `FORK_CHANGES.md` | The perturbative barrier engine is thread-safe, the XABR interpolations no longer keep a dangling forward, and implied standard deviations below the forward and at tiny prices are solved properly. Also partial lookbacks, the Choi basket engine, Lagrange derivatives and ZABR's FD prices |
| #188 | `ql/pricingengines/barrier/analyticdoublebarrierengine.cpp` | Double knock-outs and knock-ins stay right where the volatility is wide against the corridor, where five images left errors of up to 3.0e-2 on options worth almost nothing; the engine sums the eigenfunction expansion there |
| #139, #194 | `ql/pricingengines/bacheliercalculator.cpp` | `BachelierCalculator` values cash-or-nothing, asset-or-nothing and gap payoffs, which it valued as vanilla options, and its vanna, volga and theta are right for vanilla options too |
| #179 | `ql/math/distributions/normaldistribution.cpp` | `CumulativeNormalDistribution` keeps its relative accuracy in the lower tail, where it was off by up to 1.2e-7 of itself |
| #143 | `ql/pricingengines/vanilla/analyticeuropeanvasicekengine.cpp` | Black-Vasicek with a volatility curve and a correlation takes the stock's instantaneous volatility in the term it shares with the rate, where it took the volatility to maturity as flat and was off by up to 1.8e-2 of the value. **With the volatility on Business/252 and the rate on Actual/365 Fixed, the stock now covaries with the rate on business days only**, a variance that falls between two dates is refused, and a price costs about 500 instructions a day to maturity (#197) |
| #146 | `ql/math/integrals/kronrodintegral.cpp` | `GaussKronrodAdaptive` holds its tables to full precision, so it converges to within a few ulps where it threw, and throws at once where a tolerance is below what an interval's value can resolve |
| #199 | `ql/math/distributions/poissondistribution.hpp` | `InverseCumulativePoisson` returns 1, not 0, just above `exp(-lambda)`, where its sums' rounding had put the step |
| #150 | `ql/math/distributions/gammadistribution.cpp` | `GammaFunction::logValue` takes `lgamma_r`, within 2 ulps where a six-term fit drifted to 2e-10, so `value()`, `Factorial::get` and the functions built on them lose that drift |
| #186 | `ql/math/integrals/gaussianquadratures.cpp` | `TabulatedGaussLegendre`'s nodes and weights are rounded to the nearest double, not given to 15 decimals, so its rules integrate polynomials to within the sum's own rounding, not up to 33 eps off |
| #193 | `ql/pricingengines/blackcalculator.cpp` | `BlackCalculator`'s vanna and volga are right for cash-or-nothing, asset-or-nothing and gap payoffs, and volga is taken in the volatility, not off by √T |
| #201 | `ql/pricingengines/blackcalculator.cpp` | `BlackCalculator`'s gamma, theta, vega, strike Greeks, vanna and volga have their limits at a zero strike, not NaN |
| #200 | `ql/math/incompletegamma.cpp`, `ql/math/distributions/gammadistribution.cpp`, `ql/math/distributions/poissondistribution.hpp` | The incomplete gamma function and the gamma, chi-square and Poisson CDFs no longer throw near the mean from shapes in the hundreds; the gamma CDF is good to about 1e-16, where it was off by up to 1.3e-6; and the Poisson CDF keeps its relative accuracy in its left tail |
| #151 | `ql/math/modifiedbessel.cpp` | The modified Bessel functions I and K are right at large orders, in the third quadrant past \|z\| = 13, and from x = 710 while I still fits in a `Real`, where they were off by any amount or infinite, and within a small multiple of their conditioning everywhere |
| #154 | `ql/math/optimization/simplex.cpp` | `Simplex` no longer stays put until its iteration limit when all its vertices cost the same, as near a smooth minimum at a tight tolerance; GARCH calibrations from a guess are no longer refused there |
| #159 | `ql/experimental/credit/` | `Basket::expectedShortfall` lies between the percentile and the tranche on the saddle-point, binomial and recursive loss models, where it returned a fraction of the tranche, the portfolio's tail, or a read before the distribution; the saddle point's is still low on senior tranches (#204) |
| #160 | `ql/experimental/credit/saddlepointlossmodel.hpp` | The saddle-point loss model's probabilities stay in [0, 1] where names differ, where they reached 2e7, and below one default's loss its percentile and shortfall are exact, where the percentile threw |
| #161 | `ql/experimental/credit/{basket.cpp,defaultprobabilitylatentmodel.hpp,integralntdengine.cpp}` | An nth to default after a default prices as the lower rank it now is on the names left, where its fair premium was up to 2.4 times too low, and one the defaults have triggered has no fair premium |
| #162 | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | The random models' probabilities of each name being the nth to default add up to the probability of n defaults or more, where names defaulting on the same day were dropped (44 % short at the 20th of twenty names at correlation 0.9) |
| #191 | `ql/experimental/barrieroption/suowangdoublebarrierengine.cpp` | Suo and Wang's double knock-outs and knock-ins are right at low, zero and wide volatility, where they gave NaN or errors up to 8.1e-4 on options worth almost nothing, and struck beyond the far barrier, where knock-outs priced below zero |
| #192 | `ql/experimental/barrieroption/vannavolgadoublebarrierengine.hpp` | Vanna-Volga double-barrier prices weight the smile adjustment by the right probability of no touch where the volatility is wide against the corridor, where it was off by orders of magnitude |
| #171 | `ql/time/calendars/saudiarabia.cpp` | Saudi Arabia's exchange opens on Thursdays from May 1994 to June 2006, National Day is a holiday only from 2005, and the Eid holidays of 2002 to 2007 follow the sessions held, so schedules on those dates move |
| #170 | `ql/time/calendars/israel.cpp` | Israel's exchange and settlement calendars close on the election days of 2003 to 2018, so schedules on those dates move |
| #215, #216, #217 | `ql/time/calendars/israel.cpp` | Israel's Tishrei holidays of 2001 and Independence Day of 2038 move to their days, TASE closes on the strike days of February 2012, and SHIR closes on election days and on two more days of 2026, so schedules on those dates move |
| #210 | `ql/pricingengines/barrier/analyticdoublebarrierengine.cpp`, `ql/experimental/barrieroption/suowangdoublebarrierengine.cpp`, `ql/experimental/barrieroption/vannavolgadoublebarrierengine.hpp` | The closed-form and Vanna-Volga double-barrier engines price the rebate, paid at the first touch for a knock-out and at expiry for a knock-in, where they left it out |
| #213 | `ql/experimental/barrieroption/vannavolgabarrierengine.cpp` | The Vanna-Volga barrier engine's survival probability stays finite at low volatility, where it was NaN and a knock-out the forward crosses was priced at the vanilla |
| #212 | `ql/experimental/barrieroption/vannavolgadoublebarrierengine.hpp` | The Vanna-Volga double-barrier engine reads its smile at the forward, where it took the spot as the forward, and prices the vanilla, the caps and the knock-in from it |
| #202 | `ql/experimental/forward/analytichestonforwardeuropeanengine.cpp` | The analytic Heston forward-start engine prices resets within weeks, where it returned NaN, and integrates the variance's density where it is |
| #122 | `ql/time/daycounters/business252.cpp`, `ql/time/calendar.cpp` | Business/252 counts across months follow holidays added or removed after first use, and no longer mix up calendars of the same name |
| #206 | `ql/experimental/credit/{integralntdengine,integralcdoengine,midpointcdoengine}.cpp` | An upfront due before the discount curve's date no longer makes the nth-to-default and CDO engines throw inside the first coupon, and an nth to default's upfront is on its own notional, not the basket's |
| #229 | `ql/experimental/math/multidimquadrature.hpp` | The latent models' vector integration counts its first node, the factor's most adverse value, at its weight, so the binomial and recursive models' loss distributions and the saddle point's VaR split no longer overweight their tail |
| #208 | `ql/experimental/credit/integralntdengine.cpp`, `defaultprobabilitylatentmodel.hpp` | An nth to default pays its protection at the recovery of the name that triggers it, not the pool's first name's, so its protection is no longer 35-40 % high on names whose recoveries differ; models that can't give each name's probability of being the nth default refuse those baskets; the random default model gives it since #231 |
| #209 | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | The random models' loss split gives names defaulting on the same day their part of a tranche's loss averaged over their orders, not in whatever order the standard library's sort leaves them, which moved a name's split by up to 5 % |
| #207 | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | An nth to default prices on the random default model inside a coupon, where it threw; on default curves dated before today its values carried the shift of #232 until PR #252 |
| #195 | `ql/pricingengines/bacheliercalculator.hpp` | `BachelierCalculator::itmAssetProbability` gives the asset-or-nothing value over the discounted forward, as `BlackCalculator`'s does for a call, where it gave the cash probability |
| #225 | `ql/experimental/forward/analytichestonforwardeuropeanengine.cpp` | The analytic Heston forward-start engine integrates P1 and P2 out to where they decay, where it cut them at phi = 100 and priced resets near expiry up to 43 % low; at rho = -1 or 1 it throws at the forward and on one side of it, where the integrand doesn't decay (#238) |
| #224 | `ql/experimental/forward/analytichestonforwardeuropeanengine.cpp` | The analytic Heston forward-start engine integrates the variance's density where the Feller condition fails, where it was up to 14.8 % off |
| #223, #237 | `ql/experimental/forward/analytichestonforwardeuropeanengine.cpp` | The analytic Heston forward-start engine prices at v0 = 0, at kappa - rho sigma = 0, at a vol of vol at or below 0.1, at long resets where the density's Bessel function underflowed, and resets within hours through its integral, where it returned NaN or 0, threw, or priced a vanilla option; it refuses the band of resets where the density still underflows (#237) |
| #221 | `ql/experimental/barrieroption/vannavolgabarrierengine.cpp` | The Vanna-Volga barrier engine adds the rebate at the ATM volatility outside its smile adjustment, where a knock-in lost it and the cap clipped a knock-out's |
| #220 | `ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.cpp` | The double-barrier binary engine pays the rebate, where it left it out, for all four barrier types |
| #222 | `ql/experimental/barrieroption/vannavolgainterpolation.hpp` | The Vanna-Volga interpolation's update() no longer keeps the old quotes' premia, and it says why it can't interpolate rather than returning NaN |
| #241 | `ql/pricingengines/barrier/analyticbarrierengine.cpp`, `ql/pricingengines/barrier/discretizedbarrieroption.cpp`, `ql/experimental/barrieroption/discretizeddoublebarrieroption.cpp` | The analytic barrier engine and the Derman-Kani lattices pay a negative rebate, where the one dropped it and the other floored it at 0 |
| #247 | `ql/experimental/credit/randomdefaultlatentmodel.hpp`, `randomlosslatentmodel.hpp` | The random default and loss models record each default on its own day, where about half of a day's defaults came on the next, so a statistic at today + D was about 0.5/D low |
| #232 | `ql/experimental/credit/randomdefaultlatentmodel.hpp`, `randomlosslatentmodel.hpp` | The random models' statistics count each default's day from its curve's date, as their paths do, where they read it from today and moved every default later by the curves' age |
| #244 | `ql/experimental/credit/randomlosslatentmodel.hpp`, `randomdefaultlatentmodel.hpp` | The random models refuse a basket or curves their records can't hold, where the loss model wrapped a day past 4095 round to an early one; it now takes up to 2048 names, where 2049 to 4096 fitted before |
| #245 | `ql/experimental/credit/randomlosslatentmodel.hpp` | The random loss model keeps a recovery of 0.998 or more at 255/256, where it stored it as 0 |
| #246 | `ql/experimental/credit/randomlosslatentmodel.hpp`, `spotlosslatentmodel.hpp` | The random loss model takes each default's recovery at its own probability, where a default on a curve's first day recovered nothing |
| #233 | `ql/experimental/credit/basket.cpp` | `Basket::probsBeingNthEvent` gives one probability per name in the pool once the defaults so far reach n, where it gave one per name left |
| #231 | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | An nth to default on the random default model prices where the names' recoveries differ, where the engine threw; the random models give each name's probability of being the nth default in a period |
| #234 | `ql/experimental/credit/randomlosslatentmodel.hpp` | The random loss model says why it gives no expected recovery |
| #251 | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | The random models give 0s for each name's probability of being the nth default at a rank above the names left, where a rank above the pool threw while no name had defaulted |
| #253 | `ql/experimental/credit/spotlosslatentmodel.hpp` | The spot recovery model takes the correlation of a name's default and recovery variables on several factors and on loadings of opposite signs, so the random loss model keeps the single-name loss there, where it lost up to 12.5 % more |
| #254 | `ql/experimental/credit/spotlosslatentmodel.hpp` | The spot recovery model's expected recovery given the factors takes the recovery's loadings, where it took the default's |
| #248 | `ql/experimental/credit/spotlosslatentmodel.hpp` | The spot recovery model's expected loss compiles, and gives the single-name loss on the Gaussian copula; it refuses the T copula (#255) |
| #265 | `ql/experimental/credit/defaultprobabilitylatentmodel.hpp` | The default latent model inverts each name's probability once a call and counts the defaults one name at a time, where it repeated both at every node |
| #264 | `ql/experimental/credit/defaultprobabilitylatentmodel.hpp` | The probability of at least n defaults counts on 64 names or more, where it gave 0, so an nth to default there had no protection value |
| #266 | `ql/experimental/credit/saddlepointlossmodel.hpp` | The saddle-point loss model takes each name's probability and loss once a node in its search, where it took them at every step |
| #230 | `ql/experimental/math/latentmodel.hpp` | Latent models on t factors integrate over the factors' tails, where Gauss-Hermite of order 25 left them out: a name's default probability came out 1.4 % low on t3 factors at correlation 0.3 |
| #261 | `ql/experimental/credit/recursivelossmodel.hpp` | The recursive loss model's loss distribution, percentiles and expected shortfall put each probability at its loss, where they put the i-th attainable loss at i units: with four buckets every loss came out a quarter of its size |
| #269 | `ql/experimental/math/convolvedstudentt.cpp` | Loss models on t factors take a default probability of 0, at the evaluation date, on curves as of today or at hazard 0, where they and both CDO engines threw |
| #270 | `ql/experimental/credit/defaultprobabilitylatentmodel.hpp` | The default correlation refuses a name whose default probability is 0 or 1, where it gave NaN |
| #272 | `ql/experimental/credit/constantlosslatentmodel.hpp` | The constant loss model builds without its copula traits, where leaving them out didn't compile |
| #255 | `ql/experimental/credit/spotlosslatentmodel.hpp` | The spot recovery model's class doc says it keeps the single-name loss only on the Gaussian copula |
| #258 | `ql/experimental/math/latentmodel.hpp` | Latent models on Gaussian factors integrate by the trapezoid rule on [−8.5, 8.5], where Gauss-Hermite of order 25 left Hull-White's tranche spreads 12.5 % off at correlation 0.9 |
| #273 | `ql/experimental/credit/binomiallossmodel.hpp` | The binomial loss model's distribution given the factor no longer underflows to zeros where a default is nearly certain, which left tranche losses up to 83 % low on 1,000 names |
| #227 | `ql/experimental/credit/{integralntdengine,integralcdoengine,midpointcdoengine}.cpp` | The nth-to-default and CDO engines count an upfront only if it's paid on or after today, where they counted one paid on the first coupon's accrual start until that coupon was paid |
| #228 | `ql/experimental/credit/{integralntdengine,integralcdoengine,midpointcdoengine}.cpp` | Inside a coupon the nth-to-default and CDO engines count both legs from today, where the CDO engines threw inside the first coupon on default curves as of today and an nth to default's fair premium dipped until the coupon was paid |
| #250 | `ql/experimental/credit/{defaultlossmodel.hpp,basket.cpp,randomlosslatentmodel.hpp,integralntdengine.cpp}` | An nth to default prices on the random loss model, each trigger paying at the mean of the recoveries its defaults drew, where the engine threw; `DefaultLossModel::recoveriesBeingNthEventBetween` is new |
| #280 | `ql/experimental/credit/saddlepointlossmodel.hpp` | Where a factor makes names certain to default, the saddle-point model counts their loss as certain, so within rounding of it its tail probabilities and densities are no longer NaN and its percentile and shortfall no longer throw |
| #283 | `ql/experimental/credit/saddlepointlossmodel.hpp` | A name that loses nothing no longer takes the saddle-point model's exact regions away from the whole pool |

### New in this fork

These are classes added by the fork that do not exist upstream. All
ship with regression tests, full build-system integration, and
AGPL-3.0+ file headers. Each module's header carries a `\warning MVP
scope` block listing what is explicitly out of scope for the first
cut.

- **`FallbackIborIndex`** (`ql/indexes/fallbackiborindex.hpp`) — ISDA
  2020 IBOR Fallbacks Protocol wrapper. Pre-cessation: delegates
  fixings to the wrapped legacy IBOR index. Post-cessation: compounds
  the replacement RFR (e.g. SOFR for USD LIBOR) over the legacy tenor
  using the RFR's discount curve, plus a fixed spread adjustment.
  Regression test in `test-suite/indexes.cpp`.

- **`CurveBucketer`** (`ql/risk/curvebucketer.hpp`) — tenor-bucketed
  sensitivities for curves built from `SimpleQuote` handles.
  `bucketedDelta()` and `bucketedGamma()` centered-difference each
  quote one at a time through the observer chain; `parallelDelta()`
  gives the sum-of-shocks as a sanity check. RAII-guarded quote
  restore so an exception inside the pricing engine can't leave stale
  state. MVP: tenor (1D) only, no cross-gamma, no vol-surface (2D)
  bucketing yet. Primitive for future FRTB / SIMM sensitivities.

- **`AutocallableNote` + `MCAutocallableNoteEngine`**
  (`ql/instruments/autocallablenote.hpp`,
  `ql/pricingengines/autocallable/mcautocallablenoteengine.hpp`) —
  single-underlying step-down autocallable with fixed coupons and a
  European-style maturity protection barrier, priced by Monte Carlo
  on a Black-Scholes process. MVP: one underlying, one MC engine; no
  worst-of / best-of, no memory coupons, no stochastic-vol engine yet.

- **`XvaCalculator`** (`ql/risk/xvacalculator.hpp`) — exposure-driven
  XVA adjustment layer (CVA/DVA/FVA/KVA/MVA) for any instrument whose
  EPE/ENE profile the caller can produce. Designed to plug on top of
  `CurveBucketer` (bump-and-reprice exposures) or an MC forward-
  valuation framework, without committing the fork to one exposure-
  simulation architecture. MVP: no wrong-way risk, no collateral
  thresholds or netting aggregation, MVA needs a user-supplied IM
  profile.

- **`aad.hpp` + `QL_ENABLE_AAD`** (`ql/math/aad.hpp`) — opt-in reverse-
  mode AAD via [CoDi-Pack](https://github.com/SciCompKL/CoDiPack).
  Configure with `-DQL_ENABLE_AAD=ON`; CoDi-Pack is auto-fetched by
  CMake if no installed copy is found. Exposes `AADReal` (=
  `codi::RealReverse`) and a single tested utility `aadDerivative(f, x)`
  that computes df/dx via the CoDi tape. MVP: scalar wrapper only; a
  full retrofit of the QuantLib pricing stack to AAD types is a
  multi-week follow-up. Default build does not depend on CoDi-Pack.

- **`CsvQuoteLoader`** (`ql/marketdata/csvquoteloader.hpp`) — minimum
  viable market-data snapshot loader. Reads a two-column
  `id,value` CSV into a `std::map<std::string,
  ext::shared_ptr<SimpleQuote>>` that plugs directly into the
  existing `RateHelper` pipeline. Supports comments, blank lines, and
  an optional header row.

- **`JsonQuoteLoader`** (`ql/marketdata/jsonquoteloader.hpp`) — JSON
  companion to `CsvQuoteLoader`. Auto-detects two schemas: a flat
  `{"id": value, …}` object and a `{"quotes": [{"id": …, "value": …}]}`
  array. Gated on CMake option **`QL_ENABLE_JSON_MARKETDATA`**
  (default `ON`) which either picks up an installed nlohmann/json or
  pulls v3.12.0 via `FetchContent`. When the flag is off, the header
  still compiles and the loader throws a clear message pointing at
  the flag. Follow-ups: Arrow/Parquet, multi-column quotes (bid/ask/
  mid + timestamps), streaming loaders, vendor formats.

- **`blackFormulaT<T>`** (`ql/pricingengines/blackformulatemplate.hpp`)
  — engine-level AAD proof-of-concept. A scalar-templated Black-1976
  call/put formula that instantiates with `T=Real` to match the
  existing `blackFormula()` at machine precision, or with
  `T=AADReal` to propagate derivatives through the CoDi-Pack tape.
  First file in the fork demonstrating the template-parameterise-Real
  pattern; generalising to BlackCalculator, `AnalyticEuropeanEngine`,
  and the process/curve hierarchy is an explicit follow-up.

- **`FrtbSaGirrDelta`** (`ql/risk/frtbsagirr.hpp`) — first regulatory
  calculator, sitting on top of `CurveBucketer`. Computes the BCBS
  d457 FRTB-SA General-Interest-Rate-Risk delta bucket charge
  `K_b = sqrt(sum_i WS_i² + 2·sum_{i<j} ρ_{ij}·WS_i·WS_j)` with the
  standard intra-bucket correlation `max(exp(−θ|dT|/min(T_i,T_j)),
  φ_floor)`. Risk weights, θ, and φ_floor are caller-supplied so
  users can match their jurisdiction's current BCBS / CRR / PRA
  guidance. Single bucket, delta only; vega / curvature / other risk
  classes / cross-bucket aggregation are follow-ups.

### Verified on this fork

The three case counts are from CI's full run 41806352468 on 2026-11-04, with no failures. They grow as tests are added, so CI's latest full run on a pull request has the current figures.

- **GCC 15.2.1 + C++23 + Boost 1.83** (`linux / Default`): 1,331 cases
- **Clang 22.1.8 + C++23 + Boost 1.83** (the `tsan` job, CI's only Clang build, with sessions and the thread-safe observer on): 1,333 cases
- **`linux-ci-build-with-nonstandard-options` preset** (`cmake-linux-with-options`: sessions on, thread-safe observer on, `QL_FASTER_LAZY_OBJECTS=OFF`, `QL_THROW_IN_CYCLES=ON`, high-res date on, indexed coupons, warnings-as-errors): 1,337 cases
- **`QL_ENABLE_AAD=ON`** (GCC 15.2.1, CoDi-Pack v2.2.0 auto-fetched): `AadTests/*` pass end-to-end. That run used v2.2.0. The pin moved to v3.1.0 in `d742765be`, and no build with AAD on has been verified since; no CI job builds one.

CI runs on four self-hosted runners on one machine, which take a run's
jobs as each comes free: a single `.github/workflows/ci.yml`
covering twelve configuration cells, five non-default cells, three
sanitizers, both build systems, single-header compilation, doxygen,
coverage and clang-tidy — 31 jobs on a pull request that changes
code. After it merges, master runs 5 of them: the format and file-list
gates, the default build and coverage. Changes to `.github/` or
git metadata alone run a 4-job subset, markdown-only commits run nothing,
and `gh workflow run ci.yml --ref master` runs everything on demand.
`check-test-times` is advisory rather than gating: it compares absolute
wall-clock against fixed thresholds, and the runners share the machine
with each other and with other repositories' CI runners.

### Language bindings

**There are currently no language bindings for this fork.** They lived in
a sibling repository, `dudleylane/QuantLib-SWIG`, which was deleted on
2026-08-15.

That repository held a `SWIG/dudleylane_fork.i` scaffold with full Python
bindings for `CsvQuoteLoader` and TODO markers for the remaining
fork-specific classes: `FallbackIborIndex`, `CurveBucketer`,
`XvaCalculator`, `AutocallableNote` + `MCAutocallableNoteEngine`,
`FrtbSaGirrDelta`, `JsonQuoteLoader`, `blackFormulaT` and the `aad.hpp`
helpers, plus include-path adjustments for the promoted no-arb SABR,
Heston Asian control variate and CDS option.

Reviving that work means either a new bindings repository or moving it
into this one. See *Sibling repositories* in `FORK_CHANGES.md` for what
was preserved from the deleted repository and where.

## Build, test, run

The compiler floor is GCC 15.2.1. On RHEL/CentOS Stream 10, whose
default `g++` is GCC 14, configuration stops with "QuantLib requires
GCC 15.2.1 or higher", under CMake and autotools alike, unless
gcc-toolset-15 comes first on the path. CI sets it the same way:

```bash
export PATH=/opt/rh/gcc-toolset-15/root/usr/bin:$PATH
export LD_LIBRARY_PATH=/opt/rh/gcc-toolset-15/root/usr/lib64
export CC=/opt/rh/gcc-toolset-15/root/usr/bin/gcc
export CXX=/opt/rh/gcc-toolset-15/root/usr/bin/g++

cmake --preset linux-gcc-ninja-release
cmake --build build/linux-gcc-ninja-release -j
./build/linux-gcc-ninja-release/test-suite/quantlib-test-suite --log_level=message
```

The test suite is a single Boost.Test binary. Select a suite or a case
with `--run_test`. QuantLib's own speed flags go after `--`:

```bash
T=./build/linux-gcc-ninja-release/test-suite/quantlib-test-suite
$T --list_content                                        # every suite and case
$T --run_test=QuantLibTests/CurveBucketerTests           # one suite
$T --run_test=QuantLibTests/CurveBucketerTests/testBucketedDeltaSign
$T --log_level=message -- --faster                       # skip cases tagged Slow or Fast
```

The `linux-ci-build-with-nonstandard-options` preset builds the stacked
non-default configuration that CI tests. It turns on sessions,
thread-safe observers, high-resolution dates and indexed coupons, among
others. The autotools build (`./autogen.sh && ./configure && make`) is
supported too, and most CI jobs use it. `CONTRIBUTING.md` has the checks
to run before pushing.

## Issues and questions

- **Fork-specific issues, PRs, and discussions:** <https://github.com/dudleylane/QuantLib/issues>
- **Upstream general questions / the wider QuantLib community:** [quantlib-users mailing list](https://www.quantlib.org/mailinglists.shtml) — maintained by the upstream project, not this fork.
- **Upstream bugs / patches:** report to [lballabio/QuantLib/issues](https://github.com/lballabio/QuantLib/issues). A fix merged there does not reach this fork, which does not track upstream, so if the bug affects the fork too, open an issue here as well.

## License

This fork applies a **dual-license** arrangement. The combined work is
distributed under the more restrictive of the two.

- **`LICENSE`** — GNU Affero General Public License v3.0 or later
  (AGPL-3.0+). Governs the *combined distribution* of this repository and
  any *new* files introduced in the fork. This is the license GitHub
  classifies the repository under. Full text: [LICENSE](./LICENSE).
- **`LICENSE.TXT`** — BSD-3-Clause. Governs the original upstream QuantLib
  files retained in this fork. Per the BSD-3-Clause redistribution terms,
  the original copyright notice and license text **must** remain with those
  files; this fork has not modified any upstream BSD header. Full text:
  [LICENSE.TXT](./LICENSE.TXT).

### Practical summary

- **If you redistribute this fork** (source or binary, modified or not), your
  distribution is governed by AGPL-3.0+ as the aggregate license.
- **If you offer this fork over a network** (e.g. as a service), the AGPL-3.0+
  network-use clause requires you to make the corresponding source available
  to users of that network service.
- **If you extract individual upstream files** (e.g. copy
  `ql/pricingengines/vanilla/analyticeuropeanengine.cpp` into your own
  project), those files remain under BSD-3-Clause per their file header, and
  BSD terms continue to apply to your use of them in isolation.
- **If you extract files that are new in this fork**, they are AGPL-3.0+
  only. Each one says so in its header, and
  `git grep -l 'new in the dudleylane fork'` lists them.

Upstream copyright holders are not altered by this fork. BSD-3-Clause and
AGPL-3.0 are legally compatible in the BSD → AGPL direction, which is what
this arrangement relies on. For anything at the edge of this policy consult
a lawyer — I am not one.
