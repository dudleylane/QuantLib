# Changes in this fork vs. upstream lballabio/QuantLib

This file tracks changes introduced by the `dudleylane/QuantLib` fork
that are **not** in upstream. It replaces the upstream `News.md`
release-notes file (removed on this fork — it described upstream's
1.42.1 release, not fork activity) and complements the per-commit
messages. Entries are grouped by category and, within a category, most
recent first.

Fork point: upstream's master at `98afd0b22436` (2026-04-19), whose
tree is this fork's `e74ef42a7`. The fork does not track upstream
(owner, 2026-10-21). Nothing upstream has merged since, including its
1.43 and 1.44 releases, has been brought across, and no sync is
planned. Where upstream has since fixed a bug that the fork shares, the
fix here is the fork's own change and is recorded below like any other;
[#36](https://github.com/dudleylane/QuantLib/issues/36) is the first
case. The fork's version string, `1.43-dev`, is not upstream's 1.43.

SHAs: master's history was rewritten in 2026-04 to remove a 79 MB
binary committed in 2014. Every commit from upstream's `51d4bb377`
(2014-04-11) on has a different SHA here, and `51d4bb377` itself does
not resolve in this repository.

License: the fork's aggregate distribution is **AGPL-3.0-or-later**
(see `LICENSE`). Upstream files retain their original **BSD-3-Clause**
per-file headers (see `LICENSE.TXT`). New files added by this fork
carry AGPL-3.0+ file-level copyright notices.

---

## Toolchain floor raised

- **GCC ≥ 15.2.1**, **C++ ≥ 23**, **Boost ≥ 1.83.0**, enforced in
  `CMakeLists.txt` and `acinclude.m4`. Upstream still compiles on GCC
  9 / C++17 / Boost 1.58.
- The autotools build has checked the GCC version since 2026-10-16
  ([#29](https://github.com/dudleylane/QuantLib/pull/29), `f9bb2ed25`), through
  `QL_CHECK_GCC_VERSION`. Until then `acinclude.m4` checked only for
  C++23: `QL_CHECK_CPP23` compiles `std::expected`, so `./configure`
  accepted RHEL 10's default g++ 14.4.1. Like CMake's, the check covers
  GCC only. Clang defines `__GNUC__` too, as 4.2.1, so it is excluded by
  name.
- The autotools floor check in `acinclude.m4` was hardened: the prior
  `QL_CHECK_CPP23` macro silently prepended `-std=c++23` to `CXXFLAGS`
  but lost to user-supplied `-std=c++17` via GCC's "last `-std=` wins"
  rule, allowing C++17/C++20 CI modes to silently slip below the
  stated floor. It now retries once with `-std=c++23` and aborts
  configure if `<expected>` still doesn't compile.
- CI matrices (`linux.yml`, `linux-nondefault.yml`) were trimmed to
  drop sub-floor compilers (GCC 8–14, Clang 7–18) and the
  `c++17`/`c++20` mode rows; remaining coverage is GCC 15.x, Clang 19+,
  and explicit `c++23`/`c++26` modes plus the existing non-default
  configuration tests.
- `tools/check_header.py` was bumped from `-std=c++17` to `-std=c++23`,
  and `headers.yml` now runs in the `quantlib-devenv:rolling`
  container (Ubuntu's `ubuntu-latest` g++ is GCC 13, below the floor).

## New build-time options

- **`QL_ENABLE_AAD`** (default `OFF`) — when `ON`, pulls CoDi-Pack v3.1.0
  (v2.2.0 until `d742765be`) via `FetchContent`, or picks up a
  locally-installed `codi.hpp`. It then sets `QL_ENABLE_AAD` as a
  compile definition and activates the reverse-mode AAD utilities in
  `ql/math/aad.hpp`. Default builds unchanged.
- **`QL_ENABLE_JSON_MARKETDATA`** (default `ON`) — when `ON`, looks for
  an installed `nlohmann_json` and otherwise falls back to
  `FetchContent` on nlohmann/json v3.12.0 (v3.11.3 until `d742765be`).
  Sets `QL_ENABLE_JSON_MARKETDATA` as a compile definition and links
  `nlohmann_json::nlohmann_json` into `ql_library`. When `OFF`,
  `ql/marketdata/jsonquoteloader.hpp` still compiles but the
  constructor throws.

## New top-level subdirectories

- **`ql/risk/`** — sensitivity + XVA primitives (`CurveBucketer`,
  `XvaCalculator`).
- **`ql/marketdata/`** — market-data snapshot loaders (`CsvQuoteLoader`).
- **`ql/pricingengines/autocallable/`** — structured-product engines
  (`MCAutocallableNoteEngine`).

## New classes

| Class | Header | Purpose | Status |
|---|---|---|---|
| `FallbackIborIndex` | `ql/indexes/fallbackiborindex.hpp` | ISDA 2020 IBOR Fallbacks Protocol: pre-cessation delegate to legacy, post-cessation compound RFR + fixed spread. | MVP |
| `CurveBucketer` | `ql/risk/curvebucketer.hpp` | Tenor-bucketed delta / gamma / parallel-delta for a curve of `SimpleQuote` handles. RAII-guarded; observer-graph-correct. | MVP |
| `AutocallableNote`, `MCAutocallableNoteEngine` | `ql/instruments/autocallablenote.hpp`, `ql/pricingengines/autocallable/mcautocallablenoteengine.hpp` | Single-underlying step-down autocallable with fixed coupons + protection barrier. Black-Scholes Monte-Carlo engine. | MVP |
| `XvaCalculator` | `ql/risk/xvacalculator.hpp` | Exposure-driven CVA / DVA / FVA / KVA / MVA integrator. Caller supplies the EPE / ENE grid. | MVP |
| `AADReal`, `aadDerivative` | `ql/math/aad.hpp` | Opt-in reverse-mode AAD scalar wrapper over CoDi-Pack, gated on `QL_ENABLE_AAD`. Stub-throws when off. | Scaffolding |
| `CsvQuoteLoader` | `ql/marketdata/csvquoteloader.hpp` | Minimum-viable CSV quote-snapshot loader: `(id, value)` rows → `std::map<string, shared_ptr<SimpleQuote>>`. | MVP |
| `JsonQuoteLoader` | `ql/marketdata/jsonquoteloader.hpp` | JSON companion to `CsvQuoteLoader`. Two auto-detected schemas. nlohmann/json via the `QL_ENABLE_JSON_MARKETDATA` flag (default ON). | MVP |
| `blackFormulaT<T>` | `ql/pricingengines/blackformulatemplate.hpp` | Scalar-templated Black-1976 formula. Matches `blackFormula()` at `T=Real`; propagates derivatives through CoDi tape at `T=AADReal`. First engine-level AAD primitive in the fork. | Proof-of-concept |
| `FrtbSaGirrDelta` | `ql/risk/frtbsagirr.hpp` | BCBS d457 FRTB-SA General Interest Rate Risk delta bucket charge. Composes with `CurveBucketer` output. Single bucket, delta only; vega/curvature/other risk classes are follow-ups. | MVP |

## Promotions out of `ql/experimental/`

| Module | From | To |
|---|---|---|
| No-arbitrage SABR (Doust 2012) — 9 files incl. a 7.9 MB absorption-probability table | `ql/experimental/volatility/noarbsabr*` | `ql/termstructures/volatility/noarbsabr*` |
| Himalaya / Everest / Pagoda instruments + MC engines (12 files) | `ql/experimental/exoticoptions/` | `ql/pricingengines/exotic/` |
| Discrete geometric-average Asian under Heston (analytic, control-variate for the public MC Asian engine) | `ql/experimental/asian/analytic_discr_geom_av_price_heston.{hpp,cpp}` | `ql/pricingengines/asian/analytic_discr_geom_av_price_heston.{hpp,cpp}` |
| CDS option + Black CDS-option engine (4 files) | `ql/experimental/credit/{cdsoption,blackcdsoptionengine}.{hpp,cpp}` | `ql/instruments/cdsoption.{hpp,cpp}` + `ql/pricingengines/credit/blackcdsoptionengine.{hpp,cpp}` |

## Correctness fixes

IDs reference the session-level audit, or for later fixes the issue. All
have regression tests that accompany them on the same commit.

| ID | File | Fix |
|---|---|---|
| F1 / FE5 | `ql/models/shortrate/onefactormodels/hullwhite.cpp` | Bond-option variance rewritten as `(e^{-aS} - e^{-aM})^2 · expm1(2am)` — non-negative by construction; no more `sqrt(std::max(c, 0.0))` band-aid (Brigo-Mercurio, ch. 3). Since [#113](https://github.com/dudleylane/QuantLib/issues/113) it's the factored form, the spot-start volatility times `e^{-a(S-M)}`, still non-negative by construction and with no floor. |
| F2 | `ql/processes/gsrprocesscore.cpp` | Out-of-range `time2()` validates `T_ ≥ times_.back()` only on the unsafe branch, preserving valid over-specified parameter sets for in-grid queries. |
| F3 | `ql/models/volatility/garch.cpp` | Captures `EndCriteria::Type` from `minimize()`; rejects `None` and `Unknown`. Since [#121](https://github.com/dudleylane/QuantLib/issues/121), a calibration from an explicit guess also refuses `MaxIterations`. |
| FE1 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | BAW Newton loop bounded at 100 iterations with explicit non-convergence error. |
| FE2 | same | The rejection of negative rates in `criticalPrice()` is upstream's (`c75a3fb7e`, lballabio/QuantLib#1291); FE2 rewrote its message to point users to `BjerksundStenslandApproximationEngine` (which handles them natively). Until #36, calls with r < q ≤ 0 never reached it and got the European value, which can fall below intrinsic, and puts reached it where the European value is exact. Since #36 only cases where early exercise can be optimal reach it, and since #37 neither engine reaches it at zero variance (FE3). `testBaroneAdesiWhaleyNegativeRates` checks the rejection for BAW and Ju, and that BS prices the rejected cases. |
| FE3 | same, and `juquadraticengine.cpp` since #37 | T → 0⁺ short-circuit (variance below `QL_EPSILON`) instead of running Newton on a near-degenerate variance. Since #36 it comes after the European shortcut, so it applies only where early exercise can be optimal. Until #37 it returned `max(european, immediate-exercise)`, which misses the case where the best exercise time falls inside the option's life (a call at S = 240, K = 100, r = 5 %, q = 2 %, 3 years: 140.000 against 140.134); it now returns the value of the best deterministic exercise time, which is that maximum everywhere else, bitwise. Ju had no such branch and threw at zero variance; it now has it, with its delta and gamma. At an interior exercise time the gamma comes from that time moving with the spot; where the European value wins they are the European's at that variance, so near the money, just below the threshold, Ju's gamma can reach the order of 1e7, as `AnalyticEuropeanEngine`'s does (1.4e7 for an at-the-money call at σ = 1e-9 and 30 days). BAW's stays value-only. |
| [#36](https://github.com/dudleylane/QuantLib/issues/36) (2026-10-22) | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp`, `juquadraticengine.cpp` | Both engines return the European value and Greeks wherever early exercise is never optimal: calls with q ≤ 0 and q ≤ r, puts with r ≤ 0 and r ≤ q. They tested only q ≤ 0, for calls, so calls with r < q ≤ 0 got the European value, which can fall below intrinsic (S = 100, K = 80, r = −5 %, q = 0, σ = 3 %, 3 years: 7.05 against 20); they now get FE2's error. Puts at negative rates threw, and Ju returned NaN for puts at r = 0. Ju also divided by α and h, both 0 at r = 0, so it returned NaN wherever early exercise can be optimal there; it now works with α/h, whose limit is 2/σ²T. At r > 0 BAW is unchanged and Ju moves by rounding only (at most 2.4e-13 over a 6,300-point grid). Both engines' test data give identical values, except BAW's put at r = q = 0, which moves 1.1e-7 onto the European value. Ju's values at r = 0 are the limit of those just above it, inaccuracy included: for puts with q = −1 %, σ = 40 % and 3 years they are up to 8.1 off a finite-difference price at r = 0, and up to 1.1 at r = 1e-4, where BAW is within 0.2. Upstream fixed the same defects in 1.44 (lballabio/QuantLib#2728, #2739, #2754); this is the fork's own change. Tests: `testBaroneAdesiWhaleyAndJuWhereEarlyExerciseIsNeverOptimal`, `testJuValuesAtZeroRate`, and a rewritten `testBaroneAdesiWhaleyNegativeRates`. |
| [#37](https://github.com/dudleylane/QuantLib/issues/37) (2026-10-22) | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.{hpp,cpp}`, `juquadraticengine.cpp` | Both engines threw at low volatility wherever the carry works against early exercise, for puts with r > q and calls with q > r. As the variance vanishes, `criticalPrice()`'s seed leaves its range between Su, the perpetual critical price, and the strike: h grows like 2b²T/σ², a put's seed goes to +∞ and a call's to −∞, and `blackFormula` throws. A put seed that stays finite but huge fails too, because Newton then loses the strike to cancellation. The seed now falls back to Su wherever it leaves that range; Su is finite there and tends to the strike, as the critical price does. Ju, which also threw at zero variance, gets FE3's branch. Both engines take the zero-variance value from one helper, `detail::zeroVarianceResults()` (`detail::bestFixedExercise()` since [#46](https://github.com/dudleylane/QuantLib/issues/46)), declared in `baroneadesiwhaleyengine.hpp` and defined in its `.cpp`; a file-local copy in each engine broke `--enable-unity-build`, which compiles a directory's sources as one file (PR #39's first run, 39899945305). A probe of 60,154 cases (σ from 0 to 20 %, 30 days to 10 years, with dense sweeps at low σ and just above FE3's threshold) has 32,182 where early exercise can be optimal at r ≥ 0. BAW threw in 12,402 of them and Ju in 16,901; neither throws now. Values that move: 495 BAW and 488 Ju values at σ from 0.03 % to 3 %, where Newton converged anyway from a seed outside the range, by at most 3.6e-5 and 1.7e-6 (Newton stops within its 1e-6 tolerance at a different point, and neither seed's values are systematically nearer a critical price converged to 1e-12); 24 BAW values at zero variance, by up to 0.134 (FE3's interior case); and 816 Ju values below FE3's threshold, which now take the exact zero-variance value instead of Ju's approximation, by up to 0.82. Both engines' test data give identical values. Upstream fixed the put half in 1.44 (lballabio/QuantLib#2749, #2755). Applied to the fork's files, its put-only clamp left 7,290 of the 12,402 seed throws: every call, and the puts whose seed is finite but huge. This is the fork's own change. Not changed: where Q stays finite as σ → 0 (puts with q > r, calls with r > q), the engines' small-σ limit is not the exact zero-volatility value, so the value jumps at FE3's threshold, by up to 0.26 for BAW and 0.037 for Ju on the probe's 10-year cases. Tests: `testBaroneAdesiWhaleyAndJuAtLowVolatility`, `testBaroneAdesiWhaleyAndJuAtZeroVolatility`. |
| FE6 | `ql/termstructures/volatility/sabr.hpp/.cpp` | Added public `sabrIsRiskyRegime()` detector for the Hagan 2002 unsafe-regime; documented `NoArbSabrSmileSection` as the arbitrage-free alternative. |
| FE9 | *(withdrawn)* — binary-barrier KO Greeks returning `0` is mathematically correct under continuous monitoring; the audit finding was incorrect. |
| FE11 | `ql/pricingengines/asian/mc_discr_arith_av_price_heston.hpp` | Include path updated after control-variate promotion; public MC engine no longer transitively imports experimental code. |
| A1 (2026-04-24) | `ql/pricingengines/blackformulatemplate.hpp` | Added `displacement` parameter and `stddev==0` short-circuit to `blackFormulaT`; Real-path now dispatches to `CumulativeNormalDistribution` via `if constexpr` (matches `blackFormula()` bit-for-bit); AAD path keeps `std::erf` for CoDi tape overload. Precondition checks match upstream `checkParameters()`. For puts, "bit-for-bit" only holds since [#85](https://github.com/dudleylane/QuantLib/issues/85). |
| A2 (2026-04-24) | `ql/quantlib.hpp`, `ql/{instruments,pricingengines,math}/all.hpp`, `ql/instruments/Makefile.am` | Umbrella-header chain fixed: `autocallablenote.hpp`, `aad.hpp`, `blackformulatemplate.hpp`, `autocallable/all.hpp`, `risk/all.hpp`, `marketdata/all.hpp` now transitively reachable from `<ql/quantlib.hpp>`. |
| A3 (2026-04-24) | `configure.ac` | Added `--enable-aad` and `--enable-json-marketdata` autotools flags with `AC_CHECK_HEADER` probes. |
| A4 (2026-04-24) | `ql/instruments/autocallablenote.{hpp,cpp}` | `isExpired()` uses `<=`; `arguments::validate()` now mirrors every ctor invariant. |
| A5 (2026-04-24) | `ql/marketdata/csvquoteloader.cpp` | Replaced locale-sensitive `std::stod` with `std::from_chars`; added UTF-8 BOM strip; `std::isfinite` validation with file:line error context. |
| A6 (2026-04-24) | `ql/marketdata/jsonquoteloader.cpp` | Rejects ambiguous schema (Schema A + B siblings); rejects empty `id`; validates `std::isfinite` on every value. |
| A7 (2026-04-24) | `ql/risk/xvacalculator.{hpp,cpp}` | CVA/DVA/FVA/KVA/MVA integrals switched from left-endpoint Riemann to trapezoidal quadrature; matches Brigo-Morini-Pallavicini convention. |
| A8 (2026-04-24) | `ql/risk/curvebucketer.cpp` | `ScopedQuoteRestore` now holds the `shared_ptr` by value (was by const-ref) to harden against future refactors that capture the guard. |
| A9 (2026-04-24) | `ql/indexes/fallbackiborindex.cpp` | Removed dead-code null-fallback ternaries via a `checkLegacy` helper; added ctor assertion that the legacy and RFR indices share a day counter (otherwise the post-cessation tau/discount pairing is inconsistent). |
| A10 (2026-04-25) | `test-suite/curvebucketer.cpp` | `testBucketedDeltaSign` was over-specified (required ≥2 strictly-positive buckets for a 5Y swap with a 10Y curve quote, which doesn't hold under `QL_USE_INDEXED_COUPON=ON` because the 10Y quote never feeds the 5Y swap's cashflow dates). Relaxed to the financial invariant: every bucket non-negative, at least one positive, sum positive. Surfaced by the `linux-ci-build-with-nonstandard-options` preset. |
| [#41](https://github.com/dudleylane/QuantLib/issues/41) (2026-10-24) | `ql/time/date.cpp` | `Date::todaysDate()` converted the time with `std::localtime`, which returns a static `std::tm` that every thread shares and that any `localtime` or `gmtime` call may overwrite, and it used the returned pointer unchecked. It now calls `localtime_r` into a buffer of its own, and returns a null `Date` if the conversion fails, as it already did when `std::time` failed. Both `Date` implementations share the function, and `Settings` calls it on every read of an unset evaluation date, from each thread under sessions. Under TSan, eight threads calling the old version raced with `TZ` unset, where glibc frees and copies its saved zone name on every call under a lock TSan can't see, and with `TZ` set, on the shared buffer. `localtime_r` was clean in both cases. Calling `tzset()` before it would have kept `std::localtime`'s re-reading of the zone, but it reproduced the first report, so the fix doesn't. POSIX has `localtime()` act as though it calls `tzset()`, and `localtime_r()` need not. glibc's `localtime_r` reads the zone on its first call (checked with `TZ` set and unset), so the only difference is a time-zone change made after that, by setting `TZ` or replacing `/etc/localtime`: it now applies once the program calls `tzset()`, which POSIX's `tzset()` page already advises portable programs to call before `localtime_r()`. With `TZ` unset the call also gets faster, from about 3.3 µs to 77 ns, because `std::localtime` re-read the zone and checked `/etc/localtime` each time (10⁶ calls, three repetitions, load about 1.5). Upstream fixed the same call in lballabio/QuantLib#2801, with branches for MSVC and MinGW that the fork, built only on RHEL, doesn't need. This is the fork's own change. Tests: `todaysDate` and `todaysDateFromSeveralThreads`. Under the `tsan` cell, the second would catch a return to `std::localtime`; the evidence for that is the TSan probe above, since CI runs only the new code. |
| [#86](https://github.com/dudleylane/QuantLib/issues/86) (2026-11-07) | `ql/indexes/indexmanager.{hpp,cpp}`, `ql/index.hpp` | `IndexManager`'s fixing lookups no longer write to it. `getHistory()` inserted an empty history for an unknown name, `hasHistoricalFixing()` a Null entry at the date it was asked about, and `addFixings()` a Null entry at a date it then rejected. `data_` stops being `mutable`, so the compiler refuses a return of this. Without sessions, two threads pricing their own graphs raced on that map. Single-threaded, `hasHistoricalFixing(today)` moved `timeSeries().lastDate()`, so `LastFixingQuote` reported today and forecast, or threw with no curve. `timeSeries()` on an index with no fixings now returns a shared empty series that later fixings don't fill, and `index.hpp`'s `\warning` says so. `notifier()`'s own race was left for #123. Merged as `9d72f8a16` after its own green run (42191020522). |
| [#85](https://github.com/dudleylane/QuantLib/issues/85) (2026-11-07) | `ql/pricingengines/blackformulatemplate.hpp` | `blackFormulaT<T>` computes as `blackFormula` does, with no subtraction of normal tails. A put cancelled once N(d) neared 1, down to a negative price at forward 100, strike 20, stddev 0.2; under AAD the put at strike 10 and the call at strike 800 came out 0 with a delta of the wrong sign. A1's "bit for bit" claim held for calls only until this. |
| [#60](https://github.com/dudleylane/QuantLib/issues/60) (2026-11-07) | `test-suite/csvquoteloader.cpp`, `test-suite/jsonquoteloader.cpp`, `Examples/XvaSnapshot/XvaSnapshot.cpp` | Their `mkstemp` templates start from `std::filesystem::temp_directory_path()`, which honours `TMPDIR`, rather than a literal `/tmp/`. CI points `TMPDIR` at the job's own directory because `/tmp` is on the runner's small root filesystem. |
| [#49](https://github.com/dudleylane/QuantLib/issues/49) (2026-11-07) | `test-suite/paralleltestrunner.hpp` | Both `RuntimeLog` records are value-initialised, and the name is copied with `std::string::copy` and terminated on each send, rather than relying on `strncpy`'s padding. Nothing changes for the tests. No CI cell sets `QL_ENABLE_PARALLEL_UNIT_TEST_RUNNER`, so CI doesn't compile this header; it was built and run locally. |
| [#84](https://github.com/dudleylane/QuantLib/issues/84), [#126](https://github.com/dudleylane/QuantLib/issues/126) (2026-11-07) | `ql/indexes/iborindex.hpp`, `ql/indexes/fallbackiborindex.{hpp,cpp}` | Coupons on a `FallbackIborIndex` use the fallback. `IborCoupon` called `IborIndex`'s three-argument `forecastFixing`, private and non-virtual, so a coupon after cessation priced off the legacy curve with no spread: 5.03 % against 3.27 % in #84's setup. That overload is now protected and virtual, which changes `IborIndex`'s ABI. The index also takes its fixing, value and maturity dates from the legacy index, which differed on 80 of 1,264 USD LIBOR 3M fixing dates over five years. |
| [#125](https://github.com/dudleylane/QuantLib/issues/125) (2026-11-07) | `ql/experimental/coupons/proxyibor.{hpp,cpp}` | Coupons on a `ProxyIbor` forecast through the proxied index, where they threw "null term structure", and it registers with its two quotes. `Cdi` keeps its behaviour on purpose: a coupon accrues simply and Cdi's fixings compound. Its spread is #127's. |
| [#47](https://github.com/dudleylane/QuantLib/issues/47) (2026-11-07) | `ql/pricingengines/vanilla/bjerksundstenslandengine.{hpp,cpp}` | Bjerksund-Stensland no longer returns NaN: at zero variance it takes FE3's value, beta is arranged so that nothing cancels (exactly 1 at q = 0 for -variance/2 <= r < 0, which ordinary negative rates reach), and the `phi` powers survive overflow at low variance. 8,242 NaN values on a grid of 163,800 inputs go to none. Its Greeks agree with bumps. |
| [#46](https://github.com/dudleylane/QuantLib/issues/46) (2026-11-07) | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.{hpp,cpp}`, `juquadraticengine.{hpp,cpp}` | Both engines are floored at `detail::bestFixedExercise` (FE3's helper, renamed), the largest of the European value, the intrinsic value and the value of exercising at any fixed time. Ju's correction, which divides by 1 - chi and has a pole, saturates smoothly above chi = 0.4 towards 0.6. So Ju departs from the published formula there. On 327,184 inputs it was below intrinsic by up to 136.9 and read 90.24 for an option worth 6.28; none now. Against max(QD fixed-point, floors) its error goes from +84/-157 to +1.63/-0.56. Low-volatility long-expiry overshoot isn't fixed, and both class docs say so. |
| [#108](https://github.com/dudleylane/QuantLib/issues/108) (2026-11-07) | `ql/pricingengines/vanilla/qdplusamericanengine.cpp`, `fdblackscholesvanillaengine.cpp`, `fdcirvanillaengine.cpp`, `fdhestonvanillaengine.cpp`, `ql/instruments/impliedvolatility.cpp` | The QD engines are floored at intrinsic (2,681 and 3,082 below it on 228,690 inputs). FD is floored at the payoff where the option can be exercised today, since its interpolant undershoots between nodes. The FD engines require a striked payoff rather than dereferencing null. `ImpliedVolatilityHelper` returns minVol when the target is at most 1e-7 of itself below the price there: "root not bracketed" on FD's own prices goes from 109 of 2,304 to 12, and the rest are documented (#130). |
| [#128](https://github.com/dudleylane/QuantLib/issues/128) (2026-11-07) | `ql/pricingengines/vanilla/bjerksundstenslandengine.{hpp,cpp}` | Where the value still rises above the 1993 paper's flat trigger, Bjerksund-Stensland takes the best flat trigger, and its far-boundary guard measures from the larger of spot and forward. So it departs from the paper. It collapsed to the European value under negative carry, and fell with maturity: 1,412 falls on 54,880 prices, none now. Against max(QD fixed-point, intrinsic) on 96,096 inputs, RMS 0.574 to 0.050, worst -8.17 to -0.67. Cached values changed with independent references: `testBjerksundStenslandValues`' 95.12289 to 99.20350, and `testAmericanExchangeTwoAssets`' 2.0001 and 2.0110 to 2.0042 and 2.0147 with a tolerance of 2.5e-3. It costs 12-18 % more instructions per price where the search doesn't run, and 3.9 times where it does. |
| [#129](https://github.com/dudleylane/QuantLib/issues/129) (2026-11-07) | `ql/methods/finitedifferences/stepconditions/fdmstepconditioncomposite.cpp`, `ql/pricingengines/vanilla/discretizedvanillaoption.cpp`, `qdplusamericanengine.cpp` | The FD, lattice and QD engines refuse `payoffAtExpiry`, as the approximation engines did, rather than pricing the option as one that pays at once. |
| [#130](https://github.com/dudleylane/QuantLib/issues/130) (2026-11-07) | `ql/instruments/vanillaoption.hpp` | Documentation only: `VanillaOption::impliedVolatility` can throw "root not bracketed" on a finite-difference engine's own price deep in the money, 12 of 2,304 inputs on the default grid after #108. The throw is right, since the volatility can't be identified from such a price. |
| [#87](https://github.com/dudleylane/QuantLib/issues/87) (2026-11-07) | `ql/time/daycounters/business252.cpp` | `Business252`'s two caches are `thread_local`, since every day count across a month writes to them: TSan halted 5 of 5 runs. Their stale entries after `addHoliday` were #122's, fixed later. |
| [#123](https://github.com/dudleylane/QuantLib/issues/123) (2026-11-07) | `ql/indexes/indexmanager.{hpp,cpp}` | `IndexManager::notifier()` holds a mutex for its find and insert, released before anyone is notified, so threads building indexes no longer race on the map where the manager is shared. Without it, eight threads asking for the same new names corrupted the map on every run. The map also compares names case-insensitively now, as `data_` does: a fixing added through `PROBE` was visible through `probe`, whose observers weren't told. The lock adds 75 instructions per call and the comparator about 1,410 on a 20-character name. Writes to `data_` still need the caller to serialize them, and constructing a whole index from several threads still needs thread-safe observers. |
| [#114](https://github.com/dudleylane/QuantLib/issues/114), row `2946` (2026-11-07) | `ql/time/daycounters/actualactual.cpp` | Act/Act ISMA with a schedule keeps its dates on a long first coupon with an irregular last one. The notional last coupon overwrote the last regular coupon date, so a regular period came to 0.50410959 instead of 0.5 and a short last coupon to 0.24383562 instead of 0.24585635. |
| [#117](https://github.com/dudleylane/QuantLib/issues/117) (2026-11-07) | `ql/math/fastfouriertransform.hpp`, `ql/math/integrals/discreteintegrals.cpp`, `ql/math/interpolations/{cubic,mixed}interpolation.hpp`, `ql/experimental/volatility/extendedblackvariancesurface.cpp`, `ql/methods/finitedifferences/operators/fdmcevop.hpp`, `ql/methods/finitedifferences/schemes/trbdf2scheme.hpp` | Out-of-bounds accesses and dangling references, each reproduced under AddressSanitizer. `FastFourierTransform(0)` is a copy; discrete integrals on fewer than two points are 0; `CubicInterpolation` refuses Akima on 3 points and a not-a-knot spline on too few, narrower than upstream's checks because 2-point Akima and non-spline approximations with that condition work; `MixedInterpolation` refuses a switch point past the last point; `ExtendedBlackVarianceSurface`, which crashed on any surface, stays inside its dimensions; `FdmCEVOp` and `TrBDF2Scheme` hold their arguments by value. The last changes both classes' layouts. |
| [#114](https://github.com/dudleylane/QuantLib/issues/114), rows `2667`, `2589`, `2769` (2026-11-07) | `ql/cashflows/cashflows.{hpp,cpp}` | The yield methods build their discount one interval between flows at a time and take their derivatives from that product. So `modifiedDuration`, `simpleDuration` and `convexity` are the npv's derivatives under every rule, which changes them for plain `Simple` on any leg of more than one flow: 1.6725 against a finite difference of 1.7571, and convexity 6.415 against 4.931, on a long-first-coupon bond. `SimpleThenCompounded` and `CompoundedThenSimple` go by the interval's position on a leg (a long first coupon had the two swapped, and the last period took two different yields), while a lone `InterestRate` keeps its documented rule by length: upstream refuses them there, and that isn't adopted. `bps` at a yield follows `npv`'s loop: it's 0.0454407 against a coupon bump's 0.0452084 under `Simple` before, and it no longer accepts an npvDate after a flow still to come, as `npv` never did. `IrrFinder::derivative` returned the duration rather than the slope, a factor of the npv, which `NewtonSafe` survived by bisecting: `BondFunctions::yield` goes from 904,774 to 221,819 instructions per call (-75.5 %). `basisPointValue`'s second-order term was a hundredth of itself, which moved its pins, -795.459834 to -795.098043 and -793.149033 to -792.789400. On the same bond `npv`, duration and convexity cost 3.9-6.4 % more instructions, and `bps` 33 % fewer. |
| [#114](https://github.com/dudleylane/QuantLib/issues/114), rows `2898`, `2830`, `2993`, `2833` (2026-11-07) | `ql/interestrate.cpp`, `ql/time/period.cpp`, `ql/time/schedule.cpp`, `ql/experimental/catbonds/catbond.cpp`, `ql/instruments/makemultipleresetsswap.cpp`, `ql/termstructures/yield/multipleresetsswaphelper.cpp` | A null `InterestRate` has a value in every member: `compounding()` read an uninitialized enum, and now returns `Simple`, where upstream throws. A negative month or year compares with days the right way round: 20 of 324 pairs disagreed under negation. `ThirdWednesdayInclusive` takes stub dates, where it threw "unknown rule (4)". `MakeMultipleResetsSwap` leaves its end date unadjusted, since a holiday maturity shifted every date and left a stub, and the helper's curve reaches the last fixing's end. |
| [#116](https://github.com/dudleylane/QuantLib/issues/116) (2026-11-07) | `ql/pricingengines/bond/{discretizedconvertible.cpp,binomialconvertibleengine.hpp}`, `ql/experimental/callablebonds/{callablebond.hpp,callablebond.cpp,discretizedcallablefixedratebond.cpp}` | Convertible bonds: `DiscretizedConvertible` indexes its dividends from the filtered list, where a dividend inside the settlement lag wrote past the array, and the tree starts from the stock's forward at settlement, including the dividend yield, which upstream's roll leaves out: at q = 4 %, 0.18 under the forward before, 0.77 over it with upstream's, 0.014 over it here, the lattice's own error. Callable bonds: a callability on a coupon date pays the coupon a snapped neighbour moved ahead, where an unexercisable call moved a bond by -2.10; the tree reads each callability's type from the filtered list, where a settled put moved a bond with later calls by +3.31, so `CallableBond::arguments` has a new member; and `effectiveDuration` and `effectiveConvexity` divide by the dirty price, as documented. |
| [#115](https://github.com/dudleylane/QuantLib/issues/115) (2026-11-07) | `ql/pricingengines/forward/discountingfxforwardengine.{hpp,cpp}`, `ql/instruments/{makevanillaswap,overnightindexedswap,fixedvsfloatingswap,cpiswap,yearonyearinflationswap,floatfloatswap,assetswap,bmaswap,equitytotalreturnswap,overnightindexfuture,nonstandardswap,makeswaption,makecapfloor}.cpp`, `ql/termstructures/yield/{ratehelpers,overnightindexfutureratehelper}.cpp`, `ql/termstructures/credit/defaultprobabilityhelpers.{hpp,cpp}` | An FX forward's fair rate is the spot rate times the source discount over the target's, where it was inverted (1.3601 against 1.3399), and its NPV is discounted from settlement to the reference date rather than from the value date. `MakeVanillaSwap` takes its default spot date from the index alone, and upstream's Saturday spot rule (`2653`) isn't adopted, since it adds a failing bootstrap and no failing case on the fork goes away. `OvernightIndexedSwap` passes its payment adjustment to its fixed leg. A deposit or indexed FRA helper forecasts its fixing from its curve, even past a stored fixing. Fair rates and spreads refuse a zero leg BPS, beyond upstream in `AssetSwap`, `BMASwap` and `EquityTotalReturnSwap`. An overnight future stops accruing today's fixing at its maturity. `NonstandardSwap` keeps a varying vanilla swap's nominals. `MakeSwaption`'s fixing-date constructor initializes its nominal. An overnight future's helper doesn't hear about its own curve. `MakeCapFloor` finds an ATM strike with a Bachelier engine. CDS helpers take a `tradeDate`, defaulting as a CDS's does, so a helper's CDS reprices to zero with a settlement lag; a layout change for those three classes. |
| [#113](https://github.com/dudleylane/QuantLib/issues/113) (2026-11-07) | `ql/models/shortrate/onefactormodels/{vasicek,hullwhite,extendedcoxingersollross,gaussian1dmodel,markovfunctional}`, `ql/models/shortrate/twofactormodels/g2.cpp`, `ql/processes/{gsrprocess.hpp,gsrprocesscore.{hpp,cpp}}` | Vasicek's A, B and option variance, Hull-White's bond options, and G2's V, B, sigmaP and swaption terms don't cancel or switch at a threshold for small mean reversion, against references in 120-digit arithmetic: G2's discount was 58,421 where 0.88 is right at a = 1e-7, Vasicek's 0 at a = 1e-12, and G2's swaption threw from 1e-7 down. The swaption's terms go beyond upstream's row, which leaves it at 0.857 against 0.0230 at a = 1e-10. `HullWhite::r0()` is virtual and reads the curve on demand, where upstream assigns it on every notification, so the curve stays lazy and a failed bootstrap doesn't throw at whoever set a quote; a vtable change for `Vasicek`. The extended CIR model observes its curve. A Gaussian1d forward rate ends on the index's `maturityDate()`. A GSR process's core follows `setForwardMeasureTime`, with no check at that point, as F2 checks it where it's used. Markov-functional: the flat payoff tail starts from the last grid point in its pricing loops (its calibration loop stays, on evidence), null indexes are refused by name, the smile check refuses any two treatments, a grid needs two points, `modelOutputs()` and the time-zero numeraire calculate first, `atm_` and `annuity_` don't grow, and the upper rate bound isn't shifted in one place of three. The rest of the expression is #131. |
| [#112](https://github.com/dudleylane/QuantLib/issues/112) (2026-11-07) | `ql/pricingengines/vanilla/{binomialengine,discretizedvanillaoption}`, `ql/pricingengines/barrier/{binomialbarrierengine.hpp,discretizedbarrieroption.cpp}`, `ql/experimental/barrieroption/{binomialdoublebarrierengine.hpp,discretizeddoublebarrieroption.cpp}`, `ql/pricingengines/bond/discretizedconvertible.cpp`, `ql/math/distributions/binomialdistribution.hpp`, `ql/methods/lattices/{binomialtree,trinomialtree,lattice2d}`, `ql/experimental/lattices/extendedbinomialtree.cpp` | Tree engines: an American option's exercise at maturity survives a grid that rounds below it (one call priced 16.0528 against the European 16.0745). Leisen-Reimer's up and down probabilities each come from their own Peizer-Pratt expression, so far-from-the-money trees are finite and right (a put worth 399.96 priced at 0, and 126 NaN cases at 801 steps are gone); at zero volatility it now throws. Joshi refuses strikes too far from the money rather than pricing with inverted moves, stricter than upstream, and the additive EQP tree refuses drifts too large for its steps. Greeks a collapsed tree can't give are not provided rather than NaN. An American option exercised at the root has a theta of 0. A trinomial tree keeps its spacing over a stretch of short steps that diffuses over less than a tenth of it, and `TreeLattice2D` scales its correlation term to match, so a close pair of times no longer multiplies its nodes (6,025 where 9 do); this differs from upstream on the rule, on the evidence in the commit. #133 and #132 record what's left. |
| [#109](https://github.com/dudleylane/QuantLib/issues/109) (2026-11-07) | `ql/math/distributions/normaldistribution.{hpp,cpp}`, `ql/pricingengines/{americanpayoffathit,americanpayoffatexpiry}.cpp`, `ql/pricingengines/vanilla/analyticdigitalamericanengine.cpp`, `ql/pricingengines/barrier/{analyticbarrierengine,analyticbinarybarrierengine,analyticdoublebarrierengine,analyticpartialtimebarrieroptionengine,analyticsoftbarrierengine,analytictwoassetbarrierengine}`, `ql/experimental/barrieroption/perturbativebarrieroptionengine.{hpp,cpp}` | Barrier and digital engines at low volatility. `CumulativeNormalDistribution::logValue()` is new, and the vanilla and binary barrier engines and the at-hit and at-expiry digitals take a power times `N` through it where the power overflows or `N` is 0, with exponents that don't cancel. The double knock-out, which returned 0 below a volatility of about 1.7% and off that by 5e-3 at 2%, now computes every term from logarithms, at about a quarter of the cost. All of these, double knock-outs included, price at zero volatility, where they gave NaN or a wrong number. Double knock-outs price with strikes outside the corridor, soft barriers refuse the four directions the formula doesn't cover, partial-time knock-in puts and two-asset knock-ins with dividends meet in-out parity, and the perturbative engine's trivariate normal is corrected. Beyond upstream, on the evidence in the commits: a zero `N` also goes through logarithms, the values at zero volatility, the stable exponents, the nesting in the trivariate normal and the double knock-out's low-volatility work. #135 and #136 record what's left. |
| [#138](https://github.com/dudleylane/QuantLib/issues/138) (2026-11-07) | `ql/pricingengines/blackcalculator.cpp`, `ql/pricingengines/bacheliercalculator.{hpp,cpp}` | At zero volatility `BlackCalculator` gave an out-of-the-money put a call's delta, forward delta and strike sensitivity, because it told them apart by `alpha_`, which is 0 for that put. Away from the strike, alpha and beta don't move as the volatility goes to 0, so the limits are `D α F/S`, `D α` and `D β dx/dK` for every payoff. `BachelierCalculator` had the same test six times and now keeps `Option::Type type_`, so its layout changed. `BlackCalculatorTests/testBlackCalculatorZeroVolatilityLimit` fails 38 checks on the previous library and `BachelierCalculatorTests/testBachelierCalculatorZeroVolatilityLimit` 11. Upstream's master doesn't have this. |
| [#110](https://github.com/dudleylane/QuantLib/issues/110) (2026-11-07) | `ql/pricingengines/lookback/analyticcontinuous{fixed,floating}lookback.cpp`, `ql/pricingengines/lookback/analyticcontinuouspartial{fixed,floating}lookback.cpp`, `ql/pricingengines/asian/{continuousarithmeticasianlevyengine,turnbullwakemanasianengine,choiasianengine,analytic_discr_geom_av_strike}.cpp` | Six Asian and lookback rows from upstream 1.44, each with a failing case first. **Lookbacks with no carry:** they gave NaN. Full lookbacks now switch to the limit below a carry of 1e-8, where truncation and rounding balance, rather than upstream's 1e-7, on the evidence of 2,976 cases against 120 digits (previous 1,099 off by more than 1e-6, here none). Partial-time lookbacks take the limit of the combined ratio of their terms over the carry, and handle a boundary today; 456 cases against 40 digits (previous 122 off, here none). **Levy:** at low volatility the variance cancelled, so an at-the-money Asian worth 2e-5 at σ = 1e-6 priced at 92. It's now a series with doubling and nothing subtracted, and a covered strike has no variance; 24,480 cases against 200 digits (previous 13,778 off, of which 9,614 NaN, here none). An infinite carry or variance is refused. **Turnbull-Wakeman:** every fixing known read `times.back()` on an empty vector, a memory access violation, and low volatility cancelled; the variance is now summed through `expm1`, and 1,728 cases against 800 digits have none off (previous 574). **Choi:** zero volatility, a covered strike and a surface with no variance up to the first fixing all threw. A fixing with a standard deviation below `QL_EPSILON` now comes off the strike at its forward, known averages get delta and gamma, and 756 ordinary cases stay bit-identical. Upstream's test for a variance above 0 still throws `map::at` from 1e-100 to 1e-28 (the basket engine's own case is #140). **Geometric average-strike:** times ran from the first fixing, so `testAnalyticDiscreteGeometricAverageStrike`'s pinned 4.97109 was wrong. Times from today give 4.95620 by the joint law and 4.95617 ± 0.00064 by an independent Monte Carlo. A fixing after the exercise is refused, beyond upstream. Gaps: #137 (partial lookbacks with the boundary today and the level at the spot), #140. |
| [#111](https://github.com/dudleylane/QuantLib/issues/111), [#141](https://github.com/dudleylane/QuantLib/issues/141), [#142](https://github.com/dudleylane/QuantLib/issues/142) (2026-11-07) | `ql/pricingengines/exotic/{analyticeuropeanmargrabeengine,analyticcomplexchooserengine,analyticholderextensibleoptionengine,analyticwriterextensibleoptionengine}.cpp`, `analytic{complexchooser,holderextensibleoption}engine.hpp`, `ql/pricingengines/vanilla/analyticeuropeanvasicekengine.{hpp,cpp}`, `ql/pricingengines/forward/replicatingvarianceswapengine.hpp` | Five exotic rows from upstream 1.44, each with a failing case first, and two gaps found along the way. **Margrabe:** the gammas and theta no longer mishandle quantities other than one. **Complex chooser:** its exercise dates now mean what they say, counted from today, so a caller who wrote `choosingDate + n` gets a different value; it uses `We04DP` and a Newton tolerance relative to the strikes; 432 cases against the definition (previous 146 throw, 34 NaN, 252 off, worst 3.95; here none off, worst 1.7e-13). **Holder-extensible:** rewritten from where extending pays, with a second interval for negative forward yields that the published formula doesn't have; the file is 150 lines shorter, puts cost about 69 times fewer instructions, and 1,152 cases have none off where the previous library had 1,003 off and upstream's row 580. **On curves (#141, #142):** the chooser and both extensible engines were right only on flat curves, up to 4.2 and 17.7 off; they now work from discount factors and total variances, and with no variance before the first date they take the choice at the forward, where they gave NaN. **Black-Vasicek:** dividends and the volatility to maturity, which it ignored, as a variance so that its own day counter applies; digital payoffs no longer priced as vanillas; no NaN at zero variance; small mean reversions keep their digits. With a correlation and a volatility term structure, its cross term is still flat ([#143](https://github.com/dudleylane/QuantLib/issues/143)). **Replicating variance swap:** the dividend yield enters the forward and the carry; with a constant volatility of 20 %, a rate of 10 % and a yield of 8 % gave 0.0194 for 0.04. |
| [#118](https://github.com/dudleylane/QuantLib/issues/118), rows `2841`, `3010`, `2998`, `2952`, `2799` (2026-11-07) | `ql/math/abcdmathfunction.{hpp,cpp}`, `ql/math/interpolations/{xabr,lagrange,cubic}interpolation.hpp`, `ql/math/errorfunction.cpp` | `AbcdMathFunction` finds its maximum whatever the signs of a and b: the usual a = -0.06, b = 0.17, c = 0.54, d = 0.17 peaks at 0.2657 after 2.2 years, where it said 0.17, and `maximumLocation` is `QL_MAX_REAL` where the maximum is only in the limit. XABR interpolations refuse a negative volatility when calibrating; the test names its forward, since the holder keeps a reference to it (#144). Lagrange interpolation takes the node's value at and around a node at zero, where it gave NaN, with a floor on the search's tolerance that upstream's row lacks. The Kruger cubic takes a zero slope where a slope beside it is zero, where -0 beside +0 gave NaN. `erf` reaches exactly 1 from \|x\| = 6, so the cumulative normal does from about z = 8.49, deep in-the-money Black and Bachelier prices stop falling below intrinsic, and `blackFormulaImpliedStdDev` stops throwing on them. |
| [#118](https://github.com/dudleylane/QuantLib/issues/118), rows `2880`, `3008`, `2779`, `2819`, `2984`, `2958`, `2722`, `2767`, `2783`, and [#147](https://github.com/dudleylane/QuantLib/issues/147) (2026-11-08) | `ql/math/distributions/{bivariatenormaldistribution.cpp,poissondistribution.hpp}`, `ql/math/integrals/{gaussianorthogonalpolynomial,gaussianquadratures,kronrodintegral}`, `ql/math/randomnumbers/zigguratgaussianrng.hpp`, `ql/experimental/math/{clayton,farliegumbelmorgenstern,frank}copularng.hpp`, `ql/math/copulas/claytoncopula.cpp`, `ql/experimental/math/hybridsimulatedannealing.hpp`, `Examples/GlobalOptimizer/`, new `test-suite/copulas.cpp` | Dr78's bivariate normal takes rho = +-1 and values near it through closed forms and a distance that doesn't cancel, and tests signs rather than a product that underflows. Gauss-Jacobi uses closed forms where its recurrence is 0/0. Gauss weights too small for the eigenvectors to hold come from the polynomials (Christoffel), so Hermite and hyperbolic rules stop being silently wrong from order 48 and 32, with a new virtual `logW`. The Poisson inverse steps from the mode instead of overflowing from a mean of about 130, and handles 0 and values near 1. Adaptive Gauss-Kronrod stops where an interval can't be split, rather than overflowing the stack, and its evaluation limit is exact. The Ziggurat's wedge test gets its missing parenthesis. The Farlie-Gumbel-Morgenstern sampler its missing term, and the Clayton copula takes its maximum before its power and its sampler works at negative theta. Hybrid simulated annealing honours its end temperature. |
| [#118](https://github.com/dudleylane/QuantLib/issues/118), rows `2908`, `2800`, `2910` (2026-11-08) | `ql/math/distributions/chisquaredistribution.cpp`, `ql/methods/finitedifferences/meshers/fdmhestonvariancemesher.cpp`, `ql/math/modifiedbessel.{hpp,cpp}`, `ql/models/marketmodels/models/alphafinder.cpp` | The non-central chi-square series starts where it can be represented, from closed-form bounds, so a CIR bond option near expiry no longer prices at 0 and Heston's exact variance step stops throwing; within 1.06e-12 at ncp = 1e8. `FdmHestonVarianceMesher` now tests the underflow that used to choose its mesh, and falls back where every step's quantiles stay below v0 (#149 has the limit). The modified Bessel K comes from Boost on real arguments, and from the expansion or an integer order's series on complex ones, where it was 0/0 at integers, 0 past 13, and off by 17 near integers; I at a negative integer order was 0. `AlphaFinder`'s searches stop at a tolerance they can reach, step towards each bound separately, and `solve` no longer reports success with a wrong total variance (7,665 of 19,735 random successes), so a calibration that relied on that now throws. |
| [#119](https://github.com/dudleylane/QuantLib/issues/119), rows `2829`, `2792`, `2924`, `2835`, `2974`, `2838` (2026-11-08) | `ql/pricingengines/blackformula.cpp` | Asset in-the-money probabilities have their limits at zero volatility, in both Black and Bachelier. Bachelier's inversion takes a price on its intrinsic value or below it by what rounding explains as 0, and inverts prices just above it instead of returning 0. The RS approximation goes through a form where none of its terms cancels, so it has no NaN on a valid price and its worst error on well-conditioned prices falls from 2.7e6 to 2e-4; at the most an option can be worth it gives infinity. Li-RS takes the out-of-the-money price straight from the time value, returns 0 at intrinsic, and says so at or above the maximum, its guess no longer has the displacement twice, and its step no longer depends on whether the compiler contracts it into an FMA, which threw on low-volatility prices under Clang and at `-O1`. `blackFormulaImpliedStdDev`'s parity check takes the same tolerance (#152 has its low-price limit). |
| [#119](https://github.com/dudleylane/QuantLib/issues/119), rows `2656`, `2753`, `2768`, `2877`, `2828`, `2860`, `2874`, `2747`, `2771`, `2957` (2026-11-08) | `ql/termstructures/volatility/swaption/{sabrswaptionvolatilitycube.hpp,cmsmarketcalibration.cpp}`, `ql/termstructures/volatility/smilesection.cpp`, `ql/pricingengines/inflation/inflationcapfloorengines.cpp`, `ql/models/equity/hestonslvmcmodel.cpp`, `ql/experimental/coupons/lognormalcmsspreadpricer.cpp`, `ql/termstructures/volatility/inflation/{constantcpivolatility,yoyinflationoptionletvolatilitystructure}.cpp`, `ql/termstructures/defaulttermstructure.cpp`, `ql/termstructures/volatility/{zabr.cpp,zabrsmilesection.hpp}`, `ql/instruments/equitytotalreturnswap.cpp` | A normal cube's second pass interpolates by K - F, not K / F, which took it off by up to 2e-4 in volatility. An ATM-calibrated cube on a surface with no nodes refuses, where it hit a null pointer. `SmileSection`'s differencing gaps scale with the larger of the strike and the forward: at a level of 1e8 a digital was 2.7e-3 off and a density 3.3e9 times its peak. Year-on-year optionlets within a lag of the volatility's base date price with volatility rather than at intrinsic value. `CmsMarketCalibration` reads and writes the right elements throughout, and a cube recalibrates before its first use; one of its defects corrupted the heap. Heston SLV's Monte Carlo calibration reads local volatility at the end of its step (0.27 off before, 4.6e-7 now). `LognormalCmsSpreadPricer` tests the displaced strike and shifts each rate by its own shift (4.6e-2 off and NaN before, 3.8e-15 now). The constant inflation volatility surfaces observe their quote. Jumps in a default curve are treated as a yield curve treats them. `ZabrModel` refuses an empty list, where it crashed, and fdPrice and a section refuse strikes out of order, where a section's smile was silently wrong; such a list that used to price now throws. `2581` doesn't apply, as the fork has no `atmLevel(Time)`. Gap: #153. |
| [#121](https://github.com/dudleylane/QuantLib/issues/121), rows `2844`, `2593`, `2558`, `2742`, `2839`, and [#155](https://github.com/dudleylane/QuantLib/issues/155) (2026-11-08) | `ql/models/volatility/garch.{hpp,cpp}`, `ql/termstructures/yieldtermstructure.cpp`, `ql/termstructures/volatility/equityfx/blackvoltimeextrapolation.hpp`, `ql/experimental/commodities/commoditytype.hpp`, `ql/experimental/catbonds/{catbond.hpp,catbond.cpp,montecarlocatbondengine.cpp}` | A `Garch11` calibration from an explicit guess refuses a run stopped at its iteration limit, which returned alpha 0.251 where 0.1744 is right with nothing said. The mode-based calibrations keep the optimizer's point at the limit, unlike upstream's row: 134 of 5,259 minimizations on 4,500 default calibrations end there at the optimum, through a simplex stall (#154), and a refusal inside their `try` would swap that point for the guess, which moved 105 of the 4,500, none for the better. `forwardRate`'s refusal names both times. `blackvoltimeextrapolation.hpp` includes `<vector>`, with no failing case on the platform. `CommodityType`'s header names its arguments as the definition does. `MonteCarloCatBondEngine` refuses a simulation with no paths, which gave NaN throughout. `CatBond`'s three loss figures price the bond when read, where they returned an uninitialised double or the previous engine's figures, and an expired bond reports 0. |
| [#121](https://github.com/dudleylane/QuantLib/issues/121), row `2625`, and [#156](https://github.com/dudleylane/QuantLib/issues/156) (2026-11-08) | `ql/experimental/credit/{defaultlossmodel.hpp,basket.hpp,basket.cpp,gaussianlhplossmodel.hpp,basecorrelationlossmodel.hpp,randomdefaultlatentmodel.hpp,randomlosslatentmodel.hpp,defaultprobabilitylatentmodel.hpp,homogeneouspooldef.hpp,inhomogeneouspooldef.hpp,binomiallossmodel.hpp,recursivelossmodel.hpp,saddlepointlossmodel.hpp}` | Loss models shared by baskets, or outliving them. `DefaultLossModel` inherits `Observable` virtually, so each model has one list of observers. `setBasket` notifies on every take, before the reset, and the basket taking the model ignores that (`Basket::takingLossModel_`), because upstream's same-basket shortcut leaves stale what the reset caches once a name defaults, and virtual inheritance alone costs 17 times the instructions on the CDO tests. So re-pricing one of two tranches on a model no longer gives the other's value (0.005908 for 0.060563), a correlation move reaches every tranche, and a second pool no longer gets the first's simulation (0.02994 for 0.09383) or a second set of notionals the first's weights. Models notify through their own observers, not a basket pointer that may be gone (heap-use-after-free under ASan; a memory access violation on `BaseCorrelationLossModel` before its first basket). The pool models observe their latent model, which adds an `Observer` base. The random models register with their basket's curves, and never unregister, so a hazard move simulates again (0.02994 for 0.09383). Layouts change on `Basket` and every model. Instructions on the pinned CDO case: 0.7 % more over all. Upstream's master has all of #156. |
| [#157](https://github.com/dudleylane/QuantLib/issues/157) (2026-11-08) | `ql/experimental/credit/{basket.hpp,basket.cpp,binomiallossmodel.hpp,gaussianlhplossmodel.hpp,gaussianlhplossmodel.cpp,homogeneouspooldef.hpp,inhomogeneouspooldef.hpp,recursivelossmodel.hpp,saddlepointlossmodel.hpp,randomdefaultlatentmodel.hpp,randomlosslatentmodel.hpp,basecorrelationlossmodel.hpp,integralcdoengine.cpp,midpointcdoengine.cpp,midpointcdoengine.hpp,syntheticcdo.cpp}` | Baskets after a default. `remainingNotionals(d)` and `remainingProbabilities(d)` read positions 0 to n - 1, not the live list, and the models read per-name data by position in those lists, so each name after a defaulted one took its neighbour's loading and recovery (up to 16 % off on names that differ, nothing on uniform names), and the LHP and pool models read past the end of a vector or refused. All now read today's lists and map through `liveList()`. `remainingAttachmentAmount` and `remainingDetachmentAmount` follow `basket.hpp`, `max(0, a - L)` and `max(0, d - L)`, and `expectedTrancheLoss` adds what the tranche has lost rather than the basket's whole loss (60 on a tranche the loss never reached). A tranche the defaults have wiped out, or a basket with no names left, no longer goes to the model, and the CDO engines start from what's been lost (a wiped-out 0 to 2 % tranche showed protection of 58.87). The random models test for a default only among the names left, and `splitVaRAndError` no longer writes past its vector, which aborted. `BaseCorrelationLossModel` builds its inner baskets on a pool of the names left, where any default made it refuse. A future default needs a `BankruptcyEvent` to show; RAA and RDA stay as documented. `SaddlePointLossModel`'s layout changes. Upstream's master has these too. |
| [#158](https://github.com/dudleylane/QuantLib/issues/158) (2026-11-08) | `ql/experimental/credit/{basket.hpp,pool.hpp,pool.cpp}` | `Pool::defaultKeys()` walked a map keyed by name, so it came in sorted order where `names()` comes in the order added, and every caller pairs them by position: with names like `issuer-10` and `issuer-2`, a name read another's key, so another seniority's curve or a refusal. `Pool` now keeps the keys in a vector beside the names and returns it by const reference, as `Basket::defaultKeys()` does, which takes 50 % of the instructions off `testHW` and 97 % off a base correlation re-price. `Pool::clear()` clears them too. `Pool`'s layout changes, and both functions return a reference, which doesn't show in their mangled names, so rebuild what was compiled against the old headers. Upstream's master has this too. |
| [#121](https://github.com/dudleylane/QuantLib/issues/121), row `2845` (2026-11-08) | `ql/pricingengines/swaption/gaussian1dfloatfloatswaptionengine.cpp` | `Gaussian1dFloatFloatSwaptionEngine` refuses a first guess that divides by no resets or by notionals that cancel, where it went on with NaN and failed later on something else, and it builds its payoff splines once per event rather than at each grid point (20 to 23 % fewer instructions, the same values to 17 digits). The engine's convention for coupons between a fixing and an exercise is documented and stays. Row `2560` closes without a change: it refuses `boost::optional` under Boost 1.91 or later, and the fork's only supported platform has 1.83. |
| [#120](https://github.com/dudleylane/QuantLib/issues/120), rows `2598`, `2894`, `2673`, `2687`, `2697`, `2893`, `2919`, `2945`, `2836`, `2920`, and [#163](https://github.com/dudleylane/QuantLib/issues/163), [#164](https://github.com/dudleylane/QuantLib/issues/164) (2026-11-08) | `ql/time/calendars/{southkorea,hongkong,singapore,southafrica,france,romania,botswana,italy,taiwan}.{hpp,cpp}` | Calendars that called holidays business days, or the reverse. South Korea's local election of 2026, Constitution Day from 2026, and the two extra holidays of 2025. Hong Kong's, Singapore's and South Africa's declared holidays. France's Ascension and Whit Monday move with Easter, where they were fixed dates. Romania closes on Orthodox Good Friday and on January 6th and 7th. Botswana follows its Public Holidays Act rather than upstream's list of rules, with May 15th, 2026. Italy closes on Saint Francis' day from 2026, and Milan stays open on December 24th and 31st from 2027. Taiwan has its three new holidays, and moves fixed holidays off weekends by rule from 2015. Each list was checked against a source of its own where one could be reached. |
| [#120](https://github.com/dudleylane/QuantLib/issues/120), rows `2688`, `2676`, and [#165](https://github.com/dudleylane/QuantLib/issues/165), [#167](https://github.com/dudleylane/QuantLib/issues/167) (2026-11-08) | `ql/time/calendars/{saudiarabia,indonesia}.{hpp,cpp}` | **Saudi Arabia** takes its Eid holidays from the exchange's own notices, as last session and first session after, rather than upstream's table, which fails 26 of the same checks; the rule stays before 2016 ([#166](https://github.com/dudleylane/QuantLib/issues/166)). Founding Day and National Day move off weekends by the rule of 2023, and the days declared around National Day close. The test holds the calendar to the exchange's reported trading days per year. **Indonesia** has its holidays from 2015 to 2026, checked against the sessions the exchange held, which shows five days upstream's table misses; 2026 comes from the calendar and December can still move. The blocks before 2015 gain 13 closures and correct a month. |
| [#120](https://github.com/dudleylane/QuantLib/issues/120), rows `2586`, `3022`, and [#168](https://github.com/dudleylane/QuantLib/issues/168) (2026-11-08) | `ql/time/calendars/india.{hpp,cpp}` | **India** equals NSE Clearing's "CM - Settlement Holidays for the Calendar Year" circulars from 2012 to 2026, weekday for weekday, with the changes NSE made during each year. Days that traded stay closed when they're on the list, since a clearing holiday isn't always a day without trades. November 14th and 15th, 2013 both close: trading moved to the 15th, and nothing settled on either day. Five closures before 2012 rest on two sources each. NSE's archive doesn't reach before about October 2011, so 2005 to 2011 may still lack settlement-only days. Upstream's table has all of the gaps from 2012 to 2022. |
| [#120](https://github.com/dudleylane/QuantLib/issues/120), row `2613`, and [#169](https://github.com/dudleylane/QuantLib/issues/169) (2026-11-08) | `ql/time/calendars/israel.{hpp,cpp}` | **`Israel::TASE`** trades Monday to Friday from January 4th, 2026, not upstream's 5th, and closes on every holiday's eve, on a Friday before a closed Sunday, and on election days, from 2003 since [#170](https://github.com/dudleylane/QuantLib/issues/170). It's checked against 29 of the trading-day counts the exchange reports; [#218](https://github.com/dudleylane/QuantLib/issues/218) has what's left after [#170](https://github.com/dudleylane/QuantLib/issues/170). `Calendar::isWeekend` takes only a weekday, so `TASE`'s answers for its week since 2026, and `holidayList` without `includeWeekEnds` is wrong before that. **`Israel()` and `Israel::Settlement`** follow the Bank of Israel's Zahav system, Sunday to Friday, closed on the holidays but not their eves, with Yom Kippur's apart, and on election days. So every Friday of the default `Israel()` becomes a business day, and it no longer compares equal to `Israel(TASE)`. Upstream instead deprecates `Settlement` and makes `TASE` the default. Upstream's new TELBOR market isn't built. |
| [#166](https://github.com/dudleylane/QuantLib/issues/166) (2026-11-08) | `ql/time/calendars/saudiarabia.{hpp,cpp}` | Eid holidays from 2008 to 2015 follow the sessions the exchange held, rather than Eid-1 to Eid+4, and September 24th, 2011, September 22nd, 2013 and January 25th, 2015 close. 20 days move, 17 closing and 3 opening. The sessions come from two daily series, trades in 199 shares and the TASI index, which agree on every day from February 2nd, 2008; they give the exchange's reported trading days for 2012 to 2015, and the two notices found fit. 2008 to 2011 have no reported count, and the reasons for the three closures are a reading of their dates. The calendar equals TASI's rows on all 5,387 days to November 1st, 2022. Before 2008 there's one series; #171 takes 2002 to 2007 from it. |
| [#131](https://github.com/dudleylane/QuantLib/issues/131), [#174](https://github.com/dudleylane/QuantLib/issues/174), [#175](https://github.com/dudleylane/QuantLib/issues/175), [#176](https://github.com/dudleylane/QuantLib/issues/176), [#177](https://github.com/dudleylane/QuantLib/issues/177), [#178](https://github.com/dudleylane/QuantLib/issues/178) (2026-11-09) | `ql/math/expm1.{hpp,cpp}`, `ql/processes/{hullwhiteprocess,ornsteinuhlenbeckprocess,hybridhestonhullwhiteprocess,g2process}.cpp`, `ql/models/shortrate/onefactormodels/{hullwhite,vasicek}`, `ql/models/shortrate/twofactormodels/g2`, `ql/experimental/shortrate/{generalizedhullwhite.hpp,generalizedornsteinuhlenbeckprocess.cpp}`, `ql/experimental/processes/{extendedornsteinuhlenbeckprocess,gemanroncoroniprocess}.cpp`, `ql/cashflows/{overnightindexedcouponpricer,lineartsrpricer}.cpp`, `ql/pricingengines/vanilla/analyticbsmhullwhiteengine.cpp` | Closed forms in a mean reversion that subtracted terms of order one and divided by its powers, or switched to a limit or a series below a threshold, go through `detail::decayRatio`, `decayIntegral`, `decayRatioDifference` and `decayPhiAndH` in `ql/math/expm1.hpp`, which have no difference to take and no threshold. `HullWhiteForwardProcess`'s expectation was −555,111 at a = 1e-13 where 0.0293 is right, and both Hull-White processes' drift was NaN at a = 0. `HybridHestonHullWhiteProcess::evolve` gave 0, infinity or a throw at 1e-7 and below. `G2Process` and `G2ForwardProcess` were wrong from 1e-8 down and divided by zero at 0, and the ratio difference they share with `G2` holds for arguments of either sign. `ExtendedOrnsteinUhlenbeckProcess`'s trapezoidal mean and `GemanRoncoroniProcess`'s deviation and jump were NaN at zero speeds and decays. The arithmetic-averaged overnight pricer's approximate convexity adjustment moved a coupon's rate by −2.6e-3, or 26 bp, at a mean reversion of 1e-6, where −1.35e-5 is right. `AnalyticBSMHullWhiteEngine` lost digits just above its threshold, and `LinearTsrPricer` ignored mean reversions below 1e-4 and jumped there. **Negative mean reversions move at ordinary sizes**: `HullWhiteForwardProcess` and the generalized Hull-White classes took the limit for any negative value, and the generalized process's variance at a = −0.1 goes from 1.0e-4 to 1.107e-4. Elsewhere values move in their last digits. A hybrid step costs 5 % more. |
| [#135](https://github.com/dudleylane/QuantLib/issues/135), [#180](https://github.com/dudleylane/QuantLib/issues/180) (2026-11-09) | `ql/pricingengines/americanpayoffathit.{hpp,cpp}`, `ql/pricingengines/barrier/analyticbarrierengine.cpp` | Where a negative rate makes λ² = μ² + 2r/σ² negative, at-hit digitals (`AmericanPayoffAtHit`, Greeks included) and `AnalyticBarrierEngine`'s rebate were NaN. They integrate the first-passage density over the variance elapsed there (`detail::unitPayoffAtHit`, tanh-sinh to 1e-12). The digital also takes the integral where λ < \|1 + μ\| / 100, where its closed-form rho cancels: at r = 0 and q = −σ²/2 it was 2.3 % off at 20 % volatility, and had the wrong sign at 45 %. Elsewhere nothing moves. `AmericanPayoffAtHit` grows from 200 to 248 bytes. A digital with its Greeks costs about 70,800 instructions on the integral, against 1,550 in closed form. |
| [#134](https://github.com/dudleylane/QuantLib/issues/134) (2026-11-09) | `ql/experimental/barrieroption/perturbativebarrieroptionengine.cpp` | `PerturbativeBarrierOptionEngine`'s helpers, translated from Fortran, kept their working variables in function-local statics, and the trivariate normal passed its integrand's arguments through file-scope variables, so two threads pricing at once raced: 445 TSan reports, and 9 of 200 results differed from one thread's. The variables are automatic, and the arguments travel in a struct. Prices are bit-identical. |
| [#181](https://github.com/dudleylane/QuantLib/issues/181), part of [#136](https://github.com/dudleylane/QuantLib/issues/136) (2026-11-09) | `ql/pricingengines/barrier/analyticsoftbarrierengine.{hpp,cpp}`, `ql/instruments/softbarrieroption.hpp` | `AnalyticSoftBarrierEngine` prices by the definition, the average over barriers H from L to U of single-barrier knock-ins, in closed form piece by piece, each end of each integral taking whichever of four equal forms is accurate there. Haug's formula, after Hart and Ross, is that average only where the whole range lies short of both the strike and the spot: with the spot at 85 inside [80, 90] a down-and-in call struck at 100 was 3.69 against 2.55, and at b = −σ² a down call was 7.98 against 0.45. Narrow ranges no longer switch to a single barrier on curves rebuilt on Act/360 and TARGET, the r = q + 1e-6 tweak at b = 0 is gone, zero volatility is accepted where the engine threw, and was NaN up to about 2 %, and U = L prices through `AnalyticBarrierEngine` on the engine's own process. Haug's down-and-out call on [90, 95] at 30 % is back in the test, at 7.2044. A price costs 15–70 % more, and the engine shrinks from 440 to 376 bytes. |
| [#182](https://github.com/dudleylane/QuantLib/issues/182), [#183](https://github.com/dudleylane/QuantLib/issues/183), [#184](https://github.com/dudleylane/QuantLib/issues/184), [#185](https://github.com/dudleylane/QuantLib/issues/185), part of [#136](https://github.com/dudleylane/QuantLib/issues/136) (2026-11-09) | `ql/math/distributions/bivariatenormaldistribution.{hpp,cpp}`, `ql/pricingengines/barrier/{analyticpartialtimebarrieroptionengine,analytictwoassetbarrierengine}.{hpp,cpp}`, `ql/pricingengines/exotic/{analyticcompoundoptionengine,analytictwoassetcorrelationengine}.cpp` | The partial-time, two-asset barrier, two-asset correlation and compound engines use the double-precision bivariate normal, where Dr78's six decimals put them off by up to 1.2e-4, with medians of 8.5e-7 to 1.1e-5, and cost 16–42 % less. The Genz-West bivariate normal returns its limits past 40 standard deviations, where it was NaN at infinite and very large arguments, so `GaussianCopula` has values on the edges of the unit square and `StulzEngine` prices with one asset at zero volatility. It gains `logValue()`, with relative accuracy however small the value. The partial-time and two-asset barrier engines follow the forward at zero variance, and take image terms whose power overflows through logs, where both were NaN below about 0.5 % volatility; a term goes through logs only where its power is above 10 and its bivariate normal below 0.1, and that costs 4.5 to 8 times on the prices it reaches ([#179](https://github.com/dudleylane/QuantLib/issues/179) has the cost). The partial-time engine prices with the cover date today, refuses a cover date that has passed, and refuses a Start window, or an EndB2 window covering from today, whose spot is already past the barrier ("barrier touched"). Its puts reflect the barrier and strike as S (S/H), so a spot on the barrier stays on it. The two-asset engine prices an expiry of today. |
| [#187](https://github.com/dudleylane/QuantLib/issues/187), [#189](https://github.com/dudleylane/QuantLib/issues/189), the rest of [#136](https://github.com/dudleylane/QuantLib/issues/136) (2026-11-09) | `ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.{hpp,cpp}` | `AnalyticDoubleBarrierBinaryEngine` sums images of the spot in the barriers up to σ√T = ln(U/L)/2 for Double Knock options and ln(U/L) for KIKO/KOKI, and Hui's series above, with the KIKO/KOKI series' expiry-independent part in closed form, and `detail::unitPayoffAtHit` for image terms where ρ² < 0. Double Knock options threw at 1 % volatility and below and KIKO/KOKI at 2 % and below, and the Double Knock Out at 2 % was off by 4.35e-8. KIKO and KOKI were off at every volatility, a KOKI at 5 % by 2.4e-7, since their series falls like 1/i³. At zero variance the underlying follows its forward. A Double Knock In already knocked in is worth the cash discounted to expiry, with a rho of −t times that, where it was the cash itself. A price costs 2,400 to 9,200 instructions, against 35,100 to 165,600. |
| [#172](https://github.com/dudleylane/QuantLib/issues/172), [#152](https://github.com/dudleylane/QuantLib/issues/152) (2026-11-09) | `ql/pricingengines/blackformula.cpp` | `blackFormulaImpliedStdDev`'s solver had minus the vega as its derivative for puts, so it bisected for every strike below the forward: 20 to 28 evaluations where 4 to 6 do, and a result only as good as the accuracy asked for. Every implied standard deviation below the forward moves, by up to that accuracy, to a better value, and `testNoArbSabrInterpolation`'s five pinned volatilities below the forward are re-pinned. Prices below 8.7e-29 returned a standard deviation of 0, since the solver took the value at its lower bound for zero. The helper now scales such prices to about 1, so `KahaleSmileSection` and `NoArbSabrSmileSection` move at far strikes, and such a price whose standard deviation is above the solver's limit of 24 throws "root not bracketed". |
| [#137](https://github.com/dudleylane/QuantLib/issues/137), [#140](https://github.com/dudleylane/QuantLib/issues/140), [#144](https://github.com/dudleylane/QuantLib/issues/144), [#145](https://github.com/dudleylane/QuantLib/issues/145), [#153](https://github.com/dudleylane/QuantLib/issues/153) (2026-11-09) | `ql/math/interpolations/{xabr,lagrange}interpolation.hpp`, `ql/pricingengines/lookback/analyticcontinuouspartial{fixed,floating}lookback.cpp`, `ql/pricingengines/basket/singlefactorbsmbasketengine.cpp`, `ql/termstructures/volatility/zabr.cpp` | The XABR interpolations' holder keeps its own copy of the forward, where a literal or a factory's temporary dangled; a named forward changed later is no longer seen. `LagrangeInterpolation::derivative` takes the nearest node's term from differences of the values, where it lost every digit within reach of a node (−563.7 where 0.248 is right), and every derivative moves by rounding, for the better or not at all. Partial lookbacks with their boundary today and the level at the spot price, where they were NaN, and a boundary in the past throws in the engines' own words. `SingleFactorBsmBasketEngine` reports d's limits at zero volatility, so `ChoiBasketEngine` prices a basket with no volatility, where it threw `map::at`. `ZabrModel::fdPrice` refuses a negative lowest strike in its own words. |
| [#188](https://github.com/dudleylane/QuantLib/issues/188) (2026-11-10) | `ql/pricingengines/barrier/analyticdoublebarrierengine.{hpp,cpp}` | `AnalyticDoubleBarrierEngine` summed Ikeda and Kunitomo's images from n = −5 to 5 whatever the volatility, and the images left out grow with σ√T / ln(U/L): at three times, a knock-out worth 5.8e-20 came out at 1.9e-4, and on a grid of 3,456 knock-outs the worst was 3.0e-2. Above σ√T = ln(U/L)/2 the engine sums the eigenfunction expansion of the density between the barriers instead, as `AnalyticDoubleBarrierBinaryEngine` does at the same switch, to convergence: at most six terms, each in closed form. On the grid the worst absolute error there is 5.2e-15, and the worst relative error 9.1e-8, on strikes within 0.1 % of a barrier. Below the switch prices are bit-identical, and `series` applies only there, as the class documentation says. Knock-ins, `DoubleBarrierOption::impliedVolatility` and `VannaVolgaDoubleBarrierEngine<AnalyticDoubleBarrierEngine>` take the change. A price above the switch costs 7,060–8,640 instructions, against 24,800–28,500, and 4.5 % less below it. |
| [#139](https://github.com/dudleylane/QuantLib/issues/139), [#194](https://github.com/dudleylane/QuantLib/issues/194) (2026-11-10) | `ql/pricingengines/bacheliercalculator.{hpp,cpp}` | `BachelierCalculator` valued cash-or-nothing, asset-or-nothing and gap payoffs as the vanilla option, Greeks included. A call struck at 100 with the forward at 110 and s = 5 came out at 10.042454 for each of them, against 9.772499 (cash 10), 107.767440 and 19.814952 (second strike 90). Its visitors now set what each payoff pays in the money, slope F_T + intercept, and the value and every Greek follow in closed form under the normal model. Applied to vanilla options, the general Greeks also fix #194: vanna takes the discount factor, volga is taken in the volatility rather than the standard deviation (they differed by √T), and theta's gamma term is the forward's, as the volatility is. At zero volatility the Greeks leave out the payoff's jump at the strike, as `BlackCalculator`'s do. Vanilla values and the other Greeks are unchanged, to 3 ulps. `BlackCalculator`'s vanna and volga were fixed later by #193, and `itmAssetProbability` by #195. |
| [#179](https://github.com/dudleylane/QuantLib/issues/179) (2026-11-10) | `ql/math/distributions/normaldistribution.{hpp,cpp}` | `CumulativeNormalDistribution` took `0.5 (1 + erf(z/√2))`, which keeps only absolute accuracy as the value falls, and below a value of 1e-8 an asymptotic series that was off by up to 1.2e-7 of the value, at z = −5.62. It now takes `0.5 erfc(−z/√2)` from `std::erfc`: within 9e-15 of the value on (−8, 0], and below that within N's own conditioning in z, 1.8e-13 at z = −36.4, where the series did about 3.6 times better in the median. It is still 0 exactly where its derivative is, below z = −37.15, so it never goes subnormal. `erfc` alone did, which broke the callers that switch to `logValue()` where N is 0 and `blackFormula`'s check against negative values. `AmericanPayoffAtHit`'s closed form now agrees with its first-passage integral to 1.9e-13 rather than 2.7e-8. The class no longer holds an `ErrorFunction`, so it shrinks from 64 bytes to 56. Raising #136's power cutoff, which this allows, is #196. |
| [#196](https://github.com/dudleylane/QuantLib/issues/196) (2026-11-10) | `ql/pricingengines/barrier/{analyticpartialtimebarrieroptionengine,analytictwoassetbarrierengine}.cpp` | Performance, after #179. #136's fix takes an image term (a power times the bivariate normal M) through `logValue()` where M is below 0.1 and the power above 10, because M's absolute error reached 1.2e-15 where it took N from the old tail. With #179's N, M's error below 0.1 is at most about 1.1e-16 on 400 sampled triples, We04DP's own, so the cutoff is now 100 at about the same bound (1.2e-14 of a term's prefactor). On #136's random set of 20,000 partial-time options a price at 5–20 % volatility costs about 18 % fewer instructions, and prices move by at most 1.8e-14; reference sets don't move. The cutoff stays, because M's error isn't relative where We04DP cancels. |
| [#143](https://github.com/dudleylane/QuantLib/issues/143) (2026-11-10) | `ql/pricingengines/vanilla/analyticeuropeanvasicekengine.{hpp,cpp}` | **Black-Vasicek with a volatility curve and a correlation.** The term between the stock's and the rate's volatilities takes the stock's instantaneous volatility, from each day's forward variance per unit of the rate's time, where it took the volatility to maturity as flat: #143's one-year case was off by up to 6.6e-3 of its value, and a grid of 252 rows by up to 1.8e-2. Now within 2.1e-15 of the exact value for a term structure linear in variance between dates (`BlackVarianceCurve`'s and `BlackVarianceSurface`'s defaults), and within 8.5e-7 on a cubic curve with a steep short end, where the day's average stands in. The rate's term, and the cross term for a `BlackConstantVol` on the rate curve's day counter and reference date, are closed forms through `detail::decayPhiAndH`, so the Simpson integral and its 1e-5 tolerance on the variance (1.3e-7 of the value on a flat volatility) are gone. With the volatility on Business/252 and the rate on Actual/365 Fixed, the stock now covaries with the rate on business days only. A variance that decreases between two dates is refused when there's a correlation, as `LocalVolSurface` refuses it. Cost per price: about half the old (6k against 12-14k) for a flat volatility or no correlation; with a curve and a correlation about 500 instructions a day to maturity (11.8k → 181k at a year, 13.4k → 1.86M at ten); [#197](https://github.com/dudleylane/QuantLib/issues/197) records what would cut it to the pillars. Layout: the engine loses its integrator member. |
| [#146](https://github.com/dudleylane/QuantLib/issues/146) (2026-11-10) | `ql/math/integrals/kronrodintegral.{hpp,cpp}` | `GaussKronrodAdaptive`'s G7 and K15 tables held 15 digits, with three K15 weights and an abscissa truncated, so its weights summed to 2 only within 7e-15 and anything asked below about 3.5e-15 of a value couldn't be reached: over [0, 1] at 1e-15, x^0 to x^2 threw and x^3 to x^13 came out 1.69 to 1.87 times outside 8ε/(k + 1), and 1e3 cos(x) over [0, 10] threw at 1e-11. The tables hold 33 decimals, as the non-adaptive rule's do. Exact tables alone would hang, more than 5e7 evaluations on that cosine at 1e-13, with no limit on evaluations by default, so every interval throws "cannot reach the required accuracy" where its tolerance is below half the spacing of doubles at its value, ε\|k15\|/2, and a NaN or infinite value throws at once. Integrations that can't converge throw sooner: the test's smooth cases and its NaN in 15 to 30 evaluations, where they took 795 to 16,170. Values move by a few 1e-15 relative. |
| [#199](https://github.com/dudleylane/QuantLib/issues/199) (2026-11-11) | `ql/math/distributions/poissondistribution.hpp` | `InverseCumulativePoisson` returns 0 for x up to `exp(-lambda)`, but its sums reach the probability at zero only to within their rounding, so just above it they still returned 0 for 1,133 of 74,400 lambdas from 0.01 to 744: 14 between 0.34 and 0.69 through the sum from the right, and 1,119 between 1.03 and 27.31 through the steps down from the mode. Above 28, `GammaFunction::logValue`'s old fit made every term about 1e-10 too small, which hid it; with #150's fix alone it would have been 32,025 in all. It now returns 1 where the sum from the right falls short only through rounding, and takes the term at zero as `exp(-lambda)` where the steps reach it. |
| [#150](https://github.com/dudleylane/QuantLib/issues/150) (2026-11-11) | `ql/math/distributions/gammadistribution.{hpp,cpp}` | `GammaFunction::logValue` was Numerical Recipes' six-term Lanczos fit, off by up to 2.0e-10 against mpmath (1.0e-10 by 100, 8.8e-13 at 11), and `value()`, `Factorial::get` above 27, the incomplete gamma and beta functions and the distributions, quadrature rules, Bessel functions, engines and meshers built on them carried that as a relative error. It now returns `lgamma_r`, the reentrant lgamma, within 1.7 ulps, at 130 instructions per call against 160; `std::lgamma` writes the global `signgam`, which halted TSan in 5 of 5 runs with two threads. `value()` stays exp(logValue) from 1, now within 1.2e-13 relative below 171.6. |
| [#186](https://github.com/dudleylane/QuantLib/issues/186) (2026-11-11) | `ql/math/integrals/gaussianquadratures.cpp` | `TabulatedGaussLegendre`'s tables were Abramowitz and Stegun's to 15 decimals, off by up to 7.4e-16 with errors that don't cancel (order 20's weights summed to 2 - 2.0e-15), so its rules missed polynomials they integrate exactly by up to 33 eps of the sum, against 2.4 for tables rounded to the nearest double. They are now the nodes and weights at 60 digits, given to 20 significant digits, which round to the nearest double. `BivariateCumulativeNormalDistributionWe04DP`, which uses orders 6, 12 and 20, moves by at most 1.9e-16, and at order 20 toward the closed form at a = b = 0 (2.4e-15 to 3.3e-16 relative at rho = -0.9). |
| [#193](https://github.com/dudleylane/QuantLib/issues/193) (2026-11-11) | `ql/pricingengines/blackcalculator.{hpp,cpp}` | `BlackCalculator::vanna` and `volga` used the vanilla option's formulas for every payoff, so they were wrong, sign included, for cash-or-nothing, asset-or-nothing and gap payoffs, and `volga` was vega's derivative in the standard deviation, off by √T. Both now come from each payoff's alpha and beta, with the payoff's jump at the strike carried in a new protected member, `jump_`, so that a vanilla option's terms don't have to cancel; `volga` is taken in the volatility. Vanilla vanna and volga (the parent's times √T) move only by the old path's rounding, toward 50-digit values (9.3e-11 to 6.9e-14 at worst). At a zero strike they stay NaN with six other Greeks (#201). |
| [#201](https://github.com/dudleylane/QuantLib/issues/201) (2026-11-11) | `ql/pricingengines/blackcalculator.cpp` | At a strike `close()` takes for zero, `BlackCalculator::initialize` takes d1 and d2 as infinite and the densities as zero, and `gamma`, `gammaForward`, `theta`, `vega`, `strikeSensitivity`, `strikeGamma`, `vanna` and `volga` multiplied those zeros by something infinite: NaN for every payoff, and through `AnalyticEuropeanEngine` for a call struck at zero. The Greeks now share `initialize()`'s test for the zero strike and take their zero-volatility branches there, which give the limits (zero, or -D for a vanilla call's strike sensitivity, and theta from the value and delta). Nothing else moves. |
| [#200](https://github.com/dudleylane/QuantLib/issues/200) (2026-11-11) | `ql/math/incompletegamma.{hpp,cpp}`, `ql/math/distributions/gammadistribution.{hpp,cpp}`, `ql/math/distributions/poissondistribution.hpp` | `incompleteGammaFunction`, `CumulativeGammaDistribution` (so `CumulativeChiSquareDistribution`) and `CumulativePoissonDistribution` were Numerical Recipes' series and continued fraction, stopped after 100 terms. Near the mean they ran out: they threw from a shape of 156, a Poisson mean of 159 and 2,000 degrees of freedom. The gamma CDF's series also stopped at 3e-7 of its sum, so it was off by up to 1.3e-6, and the Poisson CDF, as 1 - P, came out 0 in its left tail where it is 1.9e-22. All three now come from Boost's `gamma_p` and `gamma_q` under the default policy, as the library's other callers use them: within 1.1e-16 up to a shape of 1,000 and 2e-14 at 1e6. The Poisson CDF is Q itself, through a new `incompleteGammaFunctionComplement`. `incompleteGammaFunction` keeps its `accuracy` and `maxIteration` arguments and ignores them, and the `...SeriesRepr` and `...ContinuedFractionRepr` functions are unchanged. A call costs 3 to 18 times as much (150 ns to 1.35 us), and Boost 1.83 throws its own `evaluation_error` near the mean from a shape of about 2e10. |
| [#151](https://github.com/dudleylane/QuantLib/issues/151) (2026-11-11) | `ql/math/modifiedbessel.{hpp,cpp}` | `modifiedBesselFunction_i` switched at \|z\| = 13 from its power series to 30 terms of the large-argument expansion whatever the order, and the complex `modifiedBesselFunction_k` took the same 30 terms past 13. At large orders both were off by any amount (the weighted real I was -7.2e23 at order 50, x = 13, where 7.5e-30 is right), the complex I was off by O(1) for arg z from -pi to -pi/2 past 13, except at half-integer orders, where it took DLMF 10.40.5's form for arg z > -pi/2, the unweighted real I was infinite from x = 710, and the complex K lost up to 3.9e-4 below 13. I now comes from its series while \|z\|^2 <= 4 (\|nu\| + 1), then from the expansions where their sums converge cleanly, then from Boost's `cyl_bessel_i` on the real axis, or, on a complex argument, from Thompson and Barnett's algorithm, scaled by exp(z): Temme's series or Steed's continued fraction for K, K's recurrence, and I from its continued fraction and the Wronskian. The complex K comes from the expansion or the same algorithm, and the left half-plane from -z on the side of the cut that Im z's sign gives. Measured against mpmath on 9,843 points with orders -50.5 to 300 and \|z\| to 1,000: within 22 QL_EPSILON times the condition number, and 6.1 times it away from that algorithm. A complex I through it costs 1.3 to 2.7 us at orders up to 50 and 9.7 us at 300, against 0.27 to 0.42 for the old, wrong values; at 20 of the Heston suite's own arguments the complex I takes 119 ns against 111. |
| [#154](https://github.com/dudleylane/QuantLib/issues/154) (2026-11-11) | `ql/math/optimization/simplex.cpp` | With every vertex at the same cost, `Simplex::minimize` counted a reflection costing the same as no worse than the best vertex, and tried the expansion instead of the contraction. When that cost no less, nothing changed, and every later iteration repeated the same steps until the iteration limit: NR's amoeba stops once the costs agree to within its tolerance, but QuantLib's 2007 criterion on the simplex's size has no such exit. GARCH's calibration, at a rootEpsilon of 1e-8, ran all 10,000 iterations in 134 of 5,259 minimizations, and since #121's row `2844` a calibration from a guess was refused there (35, 11 and 3 of 1,125 at 1e-8, 1e-7 and 1e-6). When neither point replaces the worst vertex the simplex now contracts, and shrinks if that fails. Every other state is unchanged, so minimizations that didn't end at the limit are the same to the bit; the 134 end at StationaryPoint after 108 to 511 iterations, at the same cost or one lower by at most 7.9e-16 relative, and no calibration from a guess is refused. In the test suite, built as the release preset builds it, only `testCallableSwapAnderson`'s exercise fits move, whose piecewise-constant cost stalled all 27 of each case until their limit of 1,000; they stop after 20 to 29 iterations, and the six cases take 3.0-3.7 s, where they took 5.2-6.1 s. Which minimizations stall depends on the build's rounding: in CI's coverage and tsan builds one of `testFlatExtrapolation`'s fits does too, and now stops at StationaryPoint, with the coverage flags at the same cost and parameters, bit for bit, so `ceac6144c` lets upstream's check of how each fit ends accept either. |
| [#159](https://github.com/dudleylane/QuantLib/issues/159) (2026-11-11) | `ql/experimental/credit/{defaultlossmodel,binomiallossmodel,recursivelossmodel,saddlepointlossmodel}.hpp` | A tranche's expected shortfall is now the mean of its percentile over the levels above, on three loss models that got it wrong. `SaddlePointLossModel` compared the percentile with the tranche's width applied twice and returned it (80 above a percentile of 256 on a tranche of 400), and otherwise integrated the untranched portfolio's tail (117 on a tranche of 100, 585,560 at 99.99 %). It now takes the percentile plus the expected excess over it, divided by 1 - p, from the model's own expansions. The excess is floored at 0 at each factor, where those expansions disagree (#204), and the result is capped at the tranche. `BinomialLossModel` and `RecursiveLossModel` read before the distribution's first point at levels below the probability of no loss (3,307 on a tranche of 400), and drew chords across the kinks at the attachment and detachment (98.58 below a percentile of 100). Both now share one percentile and its exact mean (`detail::trancheLossPercentile`, `detail::trancheLossExpectedShortfall`). The recursive percentile no longer returns a probability at 0 and 1, nor the binomial one the untranched loss at 1. The saddle point's percentile after a default is a fraction of the capped tranche, where it came out 2.1-2.9 % high, and its shortfall on the evaluation date is 0 where it threw. On senior tranches the saddle point's shortfall is still low (#204); `Distribution::expectedShortfall`'s are #203. |
| [#160](https://github.com/dudleylane/QuantLib/issues/160) (2026-11-11) | `ql/experimental/credit/saddlepointlossmodel.hpp` | `SaddlePointLossModel` bracketed its search for the saddle point with bounds worked out from the name with the largest loss alone, which aren't bounds where names differ: on 19 names that lose 80 to 99 each, `Basket::probOverLoss` reached 2.13e7, and was outside [0, 1] at 327 of 401 levels of 0–3 %. The bracket now widens from s = 0 until K′ crosses the level, and where the level is beyond every loss the factor makes possible, or below those it makes certain, the limits are taken. Up to the smallest loss of a name, and above the total less it, the loss distribution given the factor is taken exactly. There the expansions had put the probability of any loss at 0.74 where it is 0.44, so the percentile threw "root not bracketed" below about 0.26 on equity tranches of #159's pool, and the shortfall at 30 % was 27 % low on 0–3 %; it is now within 0.1 %. On the names that differ, the shortfall is within 4.3 % of the exact one on three equity tranches, where it was up to 54 % high. It still falls a little where the level crosses the smallest loss, by up to 3 % of the tranche ([#205](https://github.com/dudleylane/QuantLib/issues/205)), and two tranches of #159's pool lost a cancellation of errors: 3–6 % is 6.4 % high where it was 1.2 %, and 40–60 % 14.7 % low where it was 11.4 % ([#204](https://github.com/dudleylane/QuantLib/issues/204)). |
| [#161](https://github.com/dudleylane/QuantLib/issues/161) (2026-11-11) | `ql/experimental/credit/{basket.hpp,basket.cpp,defaultlossmodel.hpp,defaultprobabilitylatentmodel.hpp,integralntdengine.cpp,nthtodefault.hpp}` | An nth to default on a basket with a default. `Basket::probAtLeastNEvents` handed the contract's rank to the loss model as it was, where `probsBeingNthEvent` takes off the defaults so far, and `DefaultLatentModel` counted a defaulted name as one that could still default. So on eight names with one defaulted, a second to default priced at 0.0353, where as a first to default on the seven left it's 0.0836. The basket now gives 1 up to the defaults so far and asks the model for the rest, and the latent model counts only the names left: the eight give the seven's values to every digit printed. A rank the defaults have reached has no premium or protection left and no fair premium, so `NthToDefault::fairPremium()` refuses, as `SyntheticCDO`'s does on a zero premium leg, where it priced as if live (0.090778 at rank 1); a zero premium rate refuses too, where it gave NaN. The per-name recovery branch ([#208](https://github.com/dudleylane/QuantLib/issues/208)), the random models ([#207](https://github.com/dudleylane/QuantLib/issues/207)) and the upfront ([#206](https://github.com/dudleylane/QuantLib/issues/206)) were left, and fixed later, but for `RandomLossLM` ([#234](https://github.com/dudleylane/QuantLib/issues/234)). Upstream's master has the same code. |
| [#162](https://github.com/dudleylane/QuantLib/issues/162) (2026-11-11) | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | The random models' probability of each name being the nth to default. `probsBeingNthEvent` put a path's defaults in a map keyed by the day, so a second name defaulting on a day was dropped and every later default moved up a place; at correlation 0.9 on twenty names, the probabilities of being the 20th added up to 0.0057, where P(at least 20) is 0.0102. The models simulate defaults to the day, so where the nth falls among same-day defaults each now takes an equal share, and every path with n defaults or more adds exactly 1 across the names: the sums equal P(at least n) on the same paths. Names moved even where nothing was dropped, as the map credited the lowest-numbered name of a tie (name 0's probability of being the 20th doubled). The loss split's order on a tied day ([#209](https://github.com/dudleylane/QuantLib/issues/209)) was left, and fixed later. The vector's layout, left for [#208](https://github.com/dudleylane/QuantLib/issues/208), whose engine no longer reads it, is [#233](https://github.com/dudleylane/QuantLib/issues/233)'s. Upstream's master has the same code. |
| [#191](https://github.com/dudleylane/QuantLib/issues/191) (2026-11-11) | `ql/experimental/barrieroption/suowangdoublebarrierengine.{hpp,cpp}`, `ql/pricingengines/barrier/analyticdoublebarrierengine.{hpp,cpp}` | `SuoWangDoubleBarrierEngine` summed ten images with `std::pow` weights whatever the volatility, as `AnalyticDoubleBarrierEngine` did before #109 and #188. Through the analytic engine's six tests it failed 105 of 150 checks: NaN from 1.5 % volatility down, at zero volatility and with a series of 1,000; 1.8173 where the value is 1.6323 at 1 %; 8.1e-4 for a knock-out worth 5.8e-20 where the volatility is wide; and knock-outs struck beyond the far barrier below zero (−0.139). It now does what the analytic engine does, in the same order: the forward at zero variance, 0 beyond the far barrier, the eigenfunction expansion above σ√T = ln(U/L)/2, and below it Suo and Wang's images with the weights as logs, through the analytic engine's weighted difference, both helpers now `detail::` in its header. It passes all 150, and on #188's grid of 3,456 its worst error below the switch is 6.2e-14, against 2.2e-6, with no negative value. The analytic engine's values are bit-identical. A price costs 57,200–58,800 instructions below the switch and 42,100–44,100 above, against 70,200–73,700. The rebate, which neither engine priced, and the `"rebateIn"` result ([#210](https://github.com/dudleylane/QuantLib/issues/210)), and `VannaVolgaDoubleBarrierEngine`'s own no-touch sum ([#192](https://github.com/dudleylane/QuantLib/issues/192)), were left, and fixed later. |
| [#192](https://github.com/dudleylane/QuantLib/issues/192) (2026-11-11) | `ql/experimental/barrieroption/vannavolgadoublebarrierengine.hpp`, `ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.{hpp,cpp}` | `VannaVolgaDoubleBarrierEngine` weighted its smile adjustment by a no-touch probability, its `"lambda"` result, summed over ten images of the spot in the barriers, which left out most of it where σ√T is wide against ln(U/L): on 95–105 over a year, 8.17e-6 where it's 3.48e-9 at 20 % and 1.05e-3 where it's 7.05e-20 at 30 %. With more terms the weights overflow (NaN at 80–120, 10 %). It now takes the probability, still to the quotes' maturity, from the function `AnalyticDoubleBarrierBinaryEngine` prices Double Knock options with, now `detail::doubleBarrierNoTouchProbability` in its header, and agrees with references to 50 digits within 8.8e-14 relative. The binary engine's values are bit-identical, and the engine's test prices move by at most 2.8e-17. The two horizons the engine mixes ([#211](https://github.com/dudleylane/QuantLib/issues/211)) are left. Its smile's forward ([#212](https://github.com/dudleylane/QuantLib/issues/212)) was left, and fixed later. |
| [#171](https://github.com/dudleylane/QuantLib/issues/171) (2026-11-11) | `ql/time/calendars/saudiarabia.{hpp,cpp}` | `SaudiArabia` gives Thursdays a session from May 28th, 1994 to June 14th, 2006, by the CMA's history of the market and its notice of June 2006, where it closed every Thursday until 2013. National Day closes from 2005, when it became a public holiday, and September 24th, 2005 and the four Thursdays of the 2006 share split close. The Eid holidays of 2002 to 2007 follow the sessions in the one daily series that reaches those years, rather than Eid-1 to Eid+4. Only Eid Al-Fitr of 2005 has a second source, and the series has no other day without trades. The calendar equals the series on every day from December 31st, 2001 to December 31st, 2007, 303 sessions in 2002 where it had 251. The twelve breaks that rest on the series alone, and the rule before 2002, are [#214](https://github.com/dudleylane/QuantLib/issues/214). |
| [#170](https://github.com/dudleylane/QuantLib/issues/170) (2026-11-11) | `ql/time/calendars/israel.{hpp,cpp}` | `Israel::TASE` and `Israel::Settlement`, the default, close on the Knesset elections of 2003, 2006, 2009, 2013 and 2015 and the municipal elections of 2018, where they closed on election days only from 2019. Basic Law: The Knesset makes election day a day of rest, municipal elections have been days off since 2018, and a daily series of the exchange's TA-125 index has no session on any of the six. Against that series, on the 5,217 Mondays to Thursdays from 2000 to 2024, the exchange's calendar then differed on 23 days: five that [#215](https://github.com/dudleylane/QuantLib/issues/215) fixes, two that [#216](https://github.com/dudleylane/QuantLib/issues/216) does, and 16 that [#218](https://github.com/dudleylane/QuantLib/issues/218) keeps, with the Sundays the series can't see. TASE's release for the third quarter of 2026, 61 trading days, confirms the rule for a Friday before a closed Sunday, which had rested on one case, and the test takes that quarter from it. |
| [#215](https://github.com/dudleylane/QuantLib/issues/215) (2026-11-11) | `ql/time/calendars/israel.{hpp,cpp}` | `Israel`'s tables had the Jewish New Year of 2001 on September 17th, where Rosh Hashana 5762 fell on the 18th, so all of 2001's Tishrei holidays were a day early in all three markets, and Independence Day of 2038 on May 10th, where Iyar 5th on a Monday moves it to the Tuesday, the 11th, and Memorial Day with it. Both entries move; a Hebrew calendar computed by Reingold and Dershowitz's arithmetic checks every entry of the six tables from 2000 to 2050, and these were the only differences. Upstream has both. The class documentation now gives the tables' range as 2000 to 2050 and Memorial Day as the day before Independence Day, not Nisan 27th. |
| [#216](https://github.com/dudleylane/QuantLib/issues/216) (2026-11-11) | `ql/time/calendars/israel.cpp` | `Israel::TASE` closes on February 8th and 9th, 2012, when the Histadrut's general strike closed the exchange (Reuters; Globes; no session in the TA-125 series). Settlement and Sunday the 12th stay open, for want of a source; [#218](https://github.com/dudleylane/QuantLib/issues/218) keeps both. |
| [#217](https://github.com/dudleylane/QuantLib/issues/217) (2026-11-11) | `ql/time/calendars/israel.cpp` | `Israel::SHIR` closes on election days, as `TASE` and `Settlement` do, where it had only two by date, one of them commented "no idea why", and missed October 27th, 2026. It also closes Easter Monday and December 28th, 2026. The Bank of Israel's lists of days without a SHIR fixing now equal the calendar from 2024 to 2026; the one for 2027 has no holidays abroad yet. |
| [#210](https://github.com/dudleylane/QuantLib/issues/210) (2026-11-12) | `ql/pricingengines/barrier/analyticdoublebarrier{,binary}engine.{hpp,cpp}`, `ql/experimental/barrieroption/suowangdoublebarrierengine.{hpp,cpp}`, `ql/experimental/barrieroption/vannavolgadoublebarrierengine.hpp`, `ql/instruments/doublebarrieroption.hpp` | `AnalyticDoubleBarrierEngine` and `SuoWangDoubleBarrierEngine` left `DoubleBarrierOption`'s rebate out of their values, silently, and `VannaVolgaDoubleBarrierEngine` with them: the issue's knock-out call with a rebate of 5 came out at 1.114682 where the lattice gives 4.187953. They now pay it as the lattice and Monte Carlo engines do, a knock-out's at the first touch of either barrier and a knock-in's at expiry if neither is touched, through `detail::doubleBarrierRebate`, from `AnalyticDoubleBarrierBinaryEngine`'s KIKO and KOKI sums and its no-touch probability: 4.186872 against the lattice's 4.187953 at 2,000 steps, and at r = 0 a knock-out's part and a knock-in's add up to the rebate to 5.9e-16. Without a rebate, values are bitwise the same. The Vanna/Volga engine adds the rebate at the ATM volatility, outside its smile adjustment, as `"RebatePrice"`. Suo-Wang's `"rebateIn"`, which wasn't a rebate's value, is gone, `"rebate"` is the rebate's part, and a price costs 35,100 instructions against 51,800. `DoubleBarrierOption::impliedVolatility` now inverts a value with the rebate, which needn't be monotone in the volatility; its documentation says so. `AnalyticDoubleBarrierBinaryEngine`'s own rebate ([#220](https://github.com/dudleylane/QuantLib/issues/220)) is left. |
| [#213](https://github.com/dudleylane/QuantLib/issues/213) (2026-11-12) | `ql/experimental/barrieroption/vannavolgabarrierengine.cpp` | `VannaVolgaBarrierEngine` weights its smile adjustment by the probability at the ATM volatility that the barrier isn't touched, its `"lambda"`. With the drift toward the barrier at low volatility, its `(B/S)^(2 mu / vol^2) N(y)` was overflow times underflow, so `"lambda"` was NaN, and the caps at `[0, vanilla]` turned the NaN price into the vanilla. An up-and-out call at 100 with the barrier at 102, r 5 %, on a flat smile at 0.1 %, came out at the whole vanilla, 4.87705755, where it's worth 0, and its knock-in at 0. Just above the overflow, `N(y)` came out as 0 under a finite power, and lambda lost up to 0.0107. The product now goes through `CumulativeNormalDistribution::logValue()` there, as #109's engines do. On a flat smile the engine gives `AnalyticBarrierEngine`'s values down to 1e-5, and lambda is within 1.4e-13 of 60-digit references down to 0.1 %. Elsewhere, on the release build, values are bitwise the same. The engine's rebate ([#221](https://github.com/dudleylane/QuantLib/issues/221)) and its smile interpolation at a zero ATM volatility ([#222](https://github.com/dudleylane/QuantLib/issues/222)) are left. |
| [#212](https://github.com/dudleylane/QuantLib/issues/212) (2026-11-12) | `ql/experimental/barrieroption/vannavolgadoublebarrierengine.hpp` | `VannaVolgaDoubleBarrierEngine` read its smile at the strike with the spot as the forward: it passed `VannaVolga` the foreign discount twice, where `VannaVolgaBarrierEngine` passes the domestic and the foreign. So `"VanillaPrice"`, which caps the knock-out, makes the knock-in and enters the price under `adaptVanDelta`, was wrong away from the pillars: on spot 100, r 5 %, a year, the put at 80 came out at 0.339622 where the smile gives 0.190856. It now passes the domestic discount, and its vanilla equals the single-barrier engine's, and a 60-digit evaluation of Castagna and Mercurio's method, within the implied volatility's accuracy. `testVannaVolgaDoubleBarrierValues`' expected values were the engine's own output with the spot as the forward; the five the fix moves past their last decimal are re-derived, each by lambda times the change in the vanilla. |
| [#202](https://github.com/dudleylane/QuantLib/issues/202) (2026-11-12) | `ql/experimental/forward/analytichestonforwardeuropeanengine.{hpp,cpp}` | `AnalyticHestonForwardEuropeanEngine` priced forward-start options at NaN for resets within weeks: its variance density, `propagator()`, multiplied an exponential that had underflowed by a Bessel function that had overflowed, every reset from a day to 14 with sigma 0.2 on a year's option, and to 47 with sigma 0.11. The density is now taken in logs with the exponentially weighted Bessel function, within 2.9e-14 of Boost's noncentral chi-squared density. Its outer rule, 128 nodes on [0, at least 2], also put a density 0.002 wide between its nodes, and above a variance of 2 could leave it out (77.5 % low, without a NaN). The range is now the variance's mean less 20 standard deviations, floored at 0, to the mean plus 20, keeping the old upper end where it lies 10 to 20 above the mean and reaching at least 10. Against a Gauss-Kronrod reference, all 13 resets sampled from a day to 182 on the issue's two sets price within 2e-7, the residue of an inner 1e-8 node shift (#223); Monte Carlo with 400,000 paths agrees within 0.7 standard errors. Where the Feller condition fails the density is singular at 0 and the rule still can't integrate it (#224), and a reset days before expiry loses up to 43 % at the inner cut phi = 100 (#225), before and after. |
| [#122](https://github.com/dudleylane/QuantLib/issues/122) (2026-11-12) | `ql/time/daycounters/business252.{hpp,cpp}`, `ql/time/calendar.{hpp,cpp}`, `ql/time/calendars/bespokecalendar.cpp` | `Business252` kept its figures for whole months and years by calendar name, so a count across months ignored a holiday added or removed after first use, and took another calendar's figures under the same name: 109 business days against the calendar's 108, and 113 against 130, from 15 January to 15 June 2030, with no error. The figures are now kept per calendar implementation, each entry holding a copy of its calendar, at most eight per thread, and dropped when any calendar's holidays or weekend change: `Calendar` counts those changes (`detail::calendarChangeCount()`, bumped by `addHoliday`, `removeHoliday`, `resetAddedAndRemovedHolidays` and `BespokeCalendar::addWeekend`), globally, so that a joint calendar's components count too. Counts cost 1.3 % to 3.5 % fewer instructions; a calendar built separately no longer shares figures by name, so one built per instrument fills its own (1.42 million instructions per ten-year count on a new joint calendar each time, against 21,000; #226). |
| [#206](https://github.com/dudleylane/QuantLib/issues/206) (2026-11-12) | `ql/experimental/credit/{integralntdengine,integralcdoengine,midpointcdoengine}.{hpp,cpp}` | `IntegralNtdEngine`, `IntegralCDOEngine` and `MidPointCDOEngine` took the upfront as paid on the first coupon's accrual start, and discounted it from there until that coupon was paid. So inside the first coupon, on a discount curve as of today, all three threw "negative time", whatever the upfront. `IntegralNtdEngine` also took it on the basket's remaining notional: 1 % on a contract of 100 on eight names of 100 came to 7.9989, not 0.99986. Now the upfront doesn't count if it falls before the discount curve's reference date, as `CDO`'s doesn't, which changes nothing that priced, and the NTD's is on the contract's notional. On a discount curve dated on or before the accrual start, an upfront already paid still counts until the first coupon is paid (#227). On default curves as of today the CDO engines still throw inside the first coupon, where their protection starts (#228). |
| [#229](https://github.com/dudleylane/QuantLib/issues/229) (2026-11-12) | `ql/experimental/math/multidimquadrature.hpp` | `GaussianQuadMultidimIntegrator`'s vector integration multiplied its first node's term by the weight with `std::for_each`, which discards the products. So the node at -6.164, the latent factor's most adverse value at the latent models' order 25, counted with weight 1 instead of 0.8624, at every level. That moved what the latent models integrate as vectors: the binomial and recursive loss models' loss distributions, percentiles and expected shortfalls, and the saddle point's VaR split. On 100 names at correlation 0.3 the Gaussian copula's distribution held 1 + 3.07e-10 of mass, and a Student t copula with 3 degrees moved its 99 % expected shortfall by 1.35e-4 (4793.2926 against 4793.9407). Now the first term is weighted, so each component equals its scalar integral. Premiums and expected tranche losses integrate scalars and didn't move. Gauss-Hermite's own shortfall on t factors is #230. Upstream's master has the same line. |
| [#208](https://github.com/dudleylane/QuantLib/issues/208) (2026-11-12) | `ql/experimental/credit/{integralntdengine,basket}.{hpp,cpp}`, `{defaultlossmodel,defaultprobabilitylatentmodel,constantlosslatentmodel}.hpp` | `IntegralNtdEngine` priced every trigger at the recovery of the pool's first name, even once it had defaulted: its `basketIsHomogeneous` was hard-coded to true, and the other branch was dead and couldn't have worked. On eight names with rising recoveries (20 % + 5 % i), the protection was 35-40 % above a Monte Carlo of the same copula at ranks 1 to 3. Now each step pays the claim on the name that triggers it: at the names left's shared recovery where they share one, and otherwise by each name's probability of being the nth default in the step, `DefaultLossModel::probsBeingNthEventBetween`, a new statistic that `DefaultLatentModel` gives exactly in total and to second order in the step among the names. The engine now agrees with the Monte Carlo to 0.23 %. Where recoveries differ, a model without the statistic throws, where it priced at the first name's: `ConstantLossModel` on Trapezoid, and `RandomDefaultLM`, whose own statistic could serve (#231). There a price costs 13-17 % more at ranks 1 and 3, and 64-70 % more at rank 8. The new virtual moves `DefaultLossModel`'s later vtable slots under the same soname, so whatever was compiled against the old header has to be rebuilt. |
| [#209](https://github.com/dudleylane/QuantLib/issues/209) (2026-11-12) | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | `RandomLM::splitVaRAndError`, behind `Basket::splitVaRLevel` on `RandomDefaultLM` and `RandomLossLM`, sorted a path's defaults by the day alone with `std::sort`, so names defaulting on the same day came in an order the standard leaves unspecified, and where the attachment or detachment fell among their losses that order decided which of them took the loss across it: on 20 names at correlation 0.9, by up to 2.5-5.0 % of a name's split between ascending and descending order. Now a day's names take what their defaults add to the tranche's loss averaged over their orders, each one's Shapley value from the portfolio's loss before that day (`detail::sameDayTrancheLosses`), as #162 shares the nth default among a day's names: by the day's subsets up to 16 names whose losses differ, and in proportion to the losses past that. A lone default takes its increment as before. A split costs 2.4-2.5 % more instructions, and 3.2 % on `RandomLossLM`, where on the same names with recoveries drawn at a correlation of 0.9 it moved a name's split by up to 1.92 %. |
| [#207](https://github.com/dudleylane/QuantLib/issues/207) (2026-11-12) | `ql/experimental/credit/randomdefaultlatentmodel.hpp` | `RandomLM::probAtLeastNEvents` and `probsBeingNthEvent`, behind `Basket`'s, required a date after today, and `IntegralNtdEngine` asks for the first at the start of each coupon's protection, which is today inside a coupon on a discount curve as of today, so no nth to default priced on `RandomDefaultLM` once its first coupon had begun. On and before today they now give 0 for one default or more, as the paths, whose days the statistics read from today, have none there. On default curves dated today the engine then gives `ConstantLossModel`'s values within 0.65 of a standard error. On default curves dated before today, where it threw, it now prices with [#232](https://github.com/dudleylane/QuantLib/issues/232)'s shift: on curves 62 days old, 4.4 % above `ConstantLossModel`'s protection at rank 1, and 1.6 % and 4.6 % below at ranks 2 and 3. `RandomLossLM` answers at today too, but still can't price an nth to default ([#234](https://github.com/dudleylane/QuantLib/issues/234)). |
| [#127](https://github.com/dudleylane/QuantLib/issues/127) (2026-11-12) | `ql/experimental/coupons/proxyibor.hpp` | Documentation only: `ProxyIbor`'s spread multiplies. The index forecasts the gearing times the proxied index's fixing times the spread, as it has since the class came in (`5c75b3a88`, 2010, whose message gives that formula), so a spread of 1 means none and 0 gives a rate of 0, where the name suggests an addition. The owner chose to keep the formula and document it. As for any index, a fixing stored under the proxy's own name comes first, and a past fixing date needs one; where the formula applies, the proxied index's stored fixing for today enters it. `testProxyIborFixing` pins each of these. |
| [#195](https://github.com/dudleylane/QuantLib/issues/195) (2026-11-12) | `ql/pricingengines/bacheliercalculator.hpp` | `BachelierCalculator::itmAssetProbability` returned the cash probability, N(d) for a call, where its documentation promised the probability of being in the money in the asset measure, which `BlackCalculator`'s gives for a call. A normal forward can be negative, so there is no such measure; the owner chose that quantity anyway, the asset-or-nothing value over the discounted forward: N(d) + s n(d)/F for a call and N(-d) - s n(d)/F for a put. On #195's case (spot 98, r 3 %, q 1 %, a normal volatility of 20, strike 105, a year) it's 0.478232, where it was 0.400902. A put gets its own, as `itmCashProbability` already gave each type its own; `BlackCalculator` gives a put the call's ([#235](https://github.com/dudleylane/QuantLib/issues/235)). It can lie outside [0, 1], 8.4888 for a call at F 1, K 0 and s 20, and the documentation says so; at a forward of 0 it throws. At zero volatility it's the cash probability, as before. `testBachelierCalculatorGreeks`' expected value, from upstream, was the cash probability and changes. |

The rows dated 2026-11-07 from #85 on merged together in [PR #148](https://github.com/dudleylane/QuantLib/pull/148) as `33a153b27`, after its own green run (42326780182: 31 jobs, 15 h 23 min). There's one commit per fix, or per row of a tracker issue, and each carries its failing case and evidence.

The rows dated 2026-11-08 merged together in [PR #173](https://github.com/dudleylane/QuantLib/pull/173) as `1a01e46c8`, after its own green run (42412874489: 31 jobs, 12 h 12 min). There's one commit per fix, or per row of a tracker issue, and each carries its own evidence.

The rows dated 2026-11-09 merged together in [PR #190](https://github.com/dudleylane/QuantLib/pull/190) as `13f7d5086`, after its own green run (42476494791: 31 jobs, 10 h 45 min). There's one commit per fix, and each carries its own evidence.

The rows dated 2026-11-10 merged together in [PR #198](https://github.com/dudleylane/QuantLib/pull/198) as `079081076`, after its own green run (42713013369: 31 jobs, 12 h 12 min). There's one commit per fix, or per pair, and each carries its own evidence.

The rows dated 2026-11-11 merged together in [PR #219](https://github.com/dudleylane/QuantLib/pull/219) as `8b2d18ad5`, after its own green run (42881988547: 31 jobs, 12 h 26 min). There's one commit per fix, and each carries its own evidence. Two more change only tests: `ceac6144c` lets `testFlatExtrapolation`'s fits stop at a stationary point, as #154's simplex can, and `c203cdabe` takes TASE's third quarter of 2026 from its release.

The rows dated 2026-11-12 merged together in [PR #236](https://github.com/dudleylane/QuantLib/pull/236) as `a49d860dc`, after its own green run (43038316532: 31 jobs, 8 h 19 min), the first on four runners. There's one commit per fix, and each carries its own evidence. `c59e1d2d6`, a merge of `master` between #207's commit and #127's, brought in only documentation.

## C++23 feature audit (2026-04-25)

Surveyed `ql/` (excluding `ql/legacy/`, `ql/experimental/`) for C++23
language and library feature adoption opportunities. Findings:

**Adopted** (commit `36aee6a3b`):
- `[[nodiscard]]` on 9 zero-discard-site getters in `Instrument`,
  `Quote`, and `Handle`. `NPV()`, `Quote::value()`, and
  `Solver1D::solve()` deliberately omitted — each has legitimate
  side-effecting discard call sites (lazy-calc forcing in lazyobject
  tests, validity-check exceptions, bootstrap lambda mutations).
- C++23 multidimensional `operator[](Size, Size)` on `Matrix`, gated
  on `__cpp_multidimensional_subscript >= 202110L`. Existing
  `operator()(i, j)` retained.

**Empty surface — verified by grep, no fix needed**:
- `std::to_underlying`: 0 enum-class casts of the form
  `static_cast<integral>(EnumName::Member)` exist in `ql/` proper.
  Earlier audit hits were arithmetic-difference casts.
- `std::unreachable()`: 0 sites with `assert(false)`,
  `// unreachable`, or `QL_FAIL("unreachable"…)`. The codebase uses
  `QL_FAIL` for control-flow termination; no marker pattern to
  replace.
- Deducing-this CRTP simplification: only 2 CRTP base classes
  (`Solver1D`, `Tree`); `this->impl()` cast removal would be purely
  cosmetic and would touch numerical machinery unnecessarily.

**Architectural mismatch — design decisions, not adoption**:
- `std::expected<T, E>` would replace QuantLib's exception-based
  error model (`QL_FAIL`/`QL_REQUIRE`); mixing creates two error
  idioms.
- `std::generator` (coroutines) interacts unanalysed with
  `Singleton<Settings>` and the thread-safe-observer machinery.
- `std::mdspan` has no surface — `Matrix`/`Array` already wrap
  storage; no raw-pointer-stride APIs in the codebase.
- Bulk loop migration (`views::enumerate`/`zip`) over the 177
  indexed loops in `ql/cashflows/` carries subtle parallel-array
  correctness risk in numerical hot paths; framework rejects sweeps
  in this category.

**Profiling-resolved** (data captured 2026-04-25 from `perf record -F
999 -g` over `AmericanOptionTests`, ~6k samples; static counts from
`grep` over `ql/`):
- `std::flat_map` for `Instrument::additionalResults_` — **rejected
  on the data**. `additionalResults_.find()` has exactly one caller
  (`Instrument::result<T>` at `instrument.hpp:198`), populated 86
  insertion sites add ~5-10 keys per pricing, and zero samples for
  `_Rb_tree_find` / `std::map::find` showed up at ≥0.5% in the
  perf profile. The map is cold per pricing.
- `std::move_only_function` replacement of `std::function` slots —
  **the data confirms `std::function` IS in hot paths but the C++23
  feature does not address the observed cost**. `_Function_handler::
  _M_invoke` for `QdFpAmericanEngine` lambdas totals ~3.8% self-time
  and `GaussLobattoIntegral` taking `std::function<double(double)>`
  is 1.23%. But `move_only_function` has the same indirect-call cost
  as `std::function` (both type-erase); the only win is storage size
  / move-only support. To actually reduce the observed cost the
  fix is to template-parameterise these callables, which is a
  different change.
- `[[assume]]` on FD / integration kernel preconditions —
  **deferred**. Hot kernels exist (`LagrangeInterpolationImpl::value`
  9.39%, `TripleBandLinearOp::solve_splitting` 5.33%,
  `CumulativeNormalDistribution::operator()` 4.31%, `ErrorFunction::
  operator()` 5.48%), but `[[assume]]` typically buys 1–5% on the
  affected function with carefully-placed predicates. Expected
  overall gain <1% per attempt; a per-kernel before/after benchmark
  is needed before any annotation lands.

Bonus finding from the same profile: `TermStructure::checkRange`
accounts for **8.55%** of self-time on the FD path — a bounds-check
function eating nearly a tenth of the workload. Not a Tier 3 item
(separate from the C++23 audit) but worth tracking; likely called
once per `discount(t, extrapolate)` and could be inlined or
short-circuited under `extrapolate=false`.  *Followed up in
[issue #1](https://github.com/dudleylane/QuantLib/issues/1) on
2026-04-26 — see "Performance optimizations" below.*

## Performance optimizations (2026-04-26)

Two follow-up issues opened against the C++23-audit profiling data
above, each closed after a measured improvement.  Wall-clock
proxies use `perf record -F 999` sample counts at fixed sampling
rate.

### Issue [#1](https://github.com/dudleylane/QuantLib/issues/1) — `TermStructure::checkRange` (FD path)

Self-time on the `AmericanOptionTests` FD path went from **8.55%** to
**3.18%** (~14.7% workload wall-clock improvement).  Three commits,
each scoped to one fix path so the contribution of each is
independently measurable:

| commit | fix | what changed | next %  |
|---|---|---|---|
| `106bc7498` | cache `maxTime()` on `TermStructure` | added `mutable Time maxTime_;` and `mutable bool maxTimeUpdated_ = false;` to `ql/termstructure.hpp`; invalidated unconditionally in `TermStructure::update()` (broader than `updated_` which only invalidates for `moving_` curves, because `maxDate()` is virtual and can change for non-moving curves too) | 5.96% |
| `33e4bdac2` | inline both `checkRange` overloads + extrapolate fast-path | moved `checkRange(Date,bool)` and `checkRange(Time,bool)` from `ql/termstructure.cpp` to the header as `inline`; added explicit early `return` after the first `QL_REQUIRE` when `extrapolate=true \|\| allowsExtrapolation()`, skipping the second `QL_REQUIRE`'s `maxTime()` lookup that all FD schemes (`extrapolate=true`) were paying for nothing | 3.18% |
| `2fb438e6c` | apply the same inline + fast-path treatment to `InflationTermStructure::checkRange` | same idea, `ql/termstructures/inflationtermstructure.{hpp,cpp}`.  No separate baseline measurement for inflation — `maxTime()` cache is inherited from the `TermStructure` base | n/a (cold path; pre-emptive symmetry) |

The three commits were verified under
`linux-ci-build-with-nonstandard-options` (sessions on, thread-safe
observer on, `QL_THROW_IN_CYCLES=ON`, `QL_FASTER_LAZY_OBJECTS=OFF`).

**Fix 1's cache was stale on moving piecewise curves until
[#35](https://github.com/dudleylane/QuantLib/pull/35)** (`a4799af5d`,
2026-10-22). This entry used to say that subclasses bypassing
`TermStructure::update()` were already violating the observer contract
and were not regressed. That was wrong.

- `PiecewiseYieldCurve::update()` and `PiecewiseDefaultCurve::update()`
  are upstream code that skip the base on purpose ("do not use
  base_curve::update() as it would always notify observers"). So once
  used, a moving curve of either kind went on checking its range against
  its first span after the evaluation date moved. `recalculate()` had
  the same hole on any lazy curve, because it re-bootstraps without a
  notification.
- It threw where the span had grown, and let lookups past its end
  through without extrapolation where the span had shrunk. Over 84
  start dates in 2026-2028, with the evaluation date advanced 100
  business days, a moving yield curve threw on 16 with par coupons and
  20 with indexed coupons, and a moving default curve on 48
  ([#34](https://github.com/dudleylane/QuantLib/issues/34)). It also
  broke the bootstrap's retry in #23's case
  ([Sibling repositories](#sibling-repositories)).
- The entry's example, smile sections, are not term structures and
  never had the cache.
- #35 keeps the reference date and max date the cache was computed
  from, and `maxTime()` recomputes when either differs. `update()` still
  invalidates it, which covers a change of day counter. The value is
  what upstream's uncached `maxTime()` returns, whether or not a
  subclass reaches the base.
- The check makes two virtual calls that the old cache skipped, so part
  of fix 1's gain goes back. On `AmericanOptionTests` it costs 7.8 %
  more instructions than the stale cache did, where upstream's uncached
  `maxTime()` costs 14.1 % more. Cycles came out at about +5.5 % against
  +32 %, on a loaded box.
- The tests are `TermStructureTests/testRangeAfterEvaluationDateChange`
  and `testRangeAfterFrozenRecalculation`, and
  `DefaultProbabilityCurveTests/testRangeAfterEvaluationDateChange`.

### Issue [#2](https://github.com/dudleylane/QuantLib/issues/2) — `Date::year/month/yearOffset/isLeap` (inflation pricing)

Surfaced as a side-finding while profiling inflation curves to
decide whether to apply issue #1's fixes to
`InflationTermStructure::checkRange` (which turned out to be at 0.35%
— cold).  The actual hotspot was `Date` arithmetic at **~52%**
combined of self-time on the inflation suites, dominated by
out-of-line PLT roundtrips on the small lookup accessors.

| commit | fix | what changed | wall-clock vs baseline |
|---|---|---|---|
| `1dcfd3f05` | inline lookup helpers | moved `Date::isLeap`, `Date::monthLength`, `Date::monthOffset`, `Date::yearOffset`, and `Date::year` to `ql/time/date.hpp` as `inline` (inside `#ifndef QL_HIGH_RESOLUTION_DATE`).  Hoisted the static lookup tables (`YearIsLeap`, `MonthLength`, etc.) to `QuantLib::detail::*Table` `extern const` arrays defined exactly once in `date.cpp`.  Added `#include <ql/errors.hpp>` to `date.hpp` since the inlined `Date::isLeap` uses `QL_REQUIRE`. | **32% faster** |
| `9e0de600e` | inline `Date::Date(Day, Month, Year)` ctor | moved the 12-line constructor body from `date.cpp` to `date.hpp` inside the same `#ifndef QL_HIGH_RESOLUTION_DATE` block; calls to the now-inline helpers flatten and `QL_REQUIRE` message-formatting can be DCE'd at sites with provably-valid inputs | **38% faster** |
| `6f2a023e4` | cache `(year, month, day)` decomposition on `Date` | added three private `mutable` members: `cachedYear_` (sentinel `0` = invalid; valid range is 1900–2200), `cachedMonth_` (`uint8_t`), `cachedDay_` (`uint8_t`).  Lazy: `Date(serial_type)` leaves cache invalid; only on first `year()/month()/dayOfMonth()` does `computeYMD()` populate it.  `Date(d, m, y)` populates eagerly (free since values are in registers from the existing checks).  All six mutators (`operator++/--`, `operator+=/-=` × `serial_type`/`Period`) invalidate via `cachedYear_ = 0`.  `sizeof(Date)` doubles 8 → 16 bytes. | **40% faster** |

Compounding all three commits: `1129 → 675` perf samples on the
inflation suites at fixed 999 Hz = **~40% wall-clock improvement**.
`Date::month()` is gone from the visible hot list entirely after fix
path 3 (cached path inlines to a single load).  `Date::Date(...)`
shows higher % post-fix-3 than mid-stream because of inlining
accounting (the cache-population work is now visible in the ctor's
self-time instead of distributed across previously-out-of-line
helpers); absolute work is similar.

Caveats, as written in April. The first two ended with
[#45](https://github.com/dudleylane/QuantLib/issues/45), below:
- `Date` is now **not thread-safe** (it never was, but the cache
  makes the data race more visible).  Concurrent `year()` / `month()`
  on the same `Date` race on the cache; both threads write the same
  value, so the result is correct on x86_64 but is technically UB
  under the C++ memory model.  Matches the existing pattern of
  `mutable bool` on `TermStructure` — `Date` doesn't claim
  thread-safety as a value type.
- `sizeof(Date)` doubling makes `std::vector<Date>` schedules 2×
  larger.  Negligible in practice (schedules of hundreds of dates
  fit easily in L1 either way) but worth noting for any code that
  cares about wire format / serialisation.
- The high-resolution date branch (`QL_HIGH_RESOLUTION_DATE=ON`)
  was deliberately left untouched; its `Date` implementation is
  separate.  Verified under the strict preset.

**Fix 3's cache was a data race until
[#106](https://github.com/dudleylane/QuantLib/pull/106)** (`7eebe7eab`,
2026-11-05), the fix for
[#45](https://github.com/dudleylane/QuantLib/issues/45), which removed
it. The first caveat above called the race harmless on
x86_64 because both threads write the same values. Under the C++ memory
model a data race is undefined behaviour whatever the values, and
nothing ordered the three stores that published the cache.

- `year()`, `month()` and `dayOfMonth()` filled the cache on a `Date`'s
  first read, and a `Date` made by arithmetic or changed by a mutator
  starts with it empty. So threads sharing a `Date` that nobody had read
  yet all wrote to it.
- `Date` holds only its serial number again: 8 bytes, with nothing
  mutable. `year()` reads the year-offset table as before. `month()` and
  `dayOfMonth()` no longer search the month-offset table.
  - They take the day of the year counted from March 1st, r, and read
    both fields from n = 2141 r + 197913. The month is n >> 16, from 3
    to 14 (January and February are 13 and 14), and the day is
    (n & 0xFFFF) / 2141 + 1.
  - That is Neri and Schneider, "Euclidean affine functions and
    applications to calendar algorithms", arXiv:2102.06959.
  - Over every valid serial, 367 to 109,574, it gives the same month and
    day as the table search.
- Dropping the cache with the table search kept was measured first. On
  a quiet box it cost 10–22 % more cycles on the calendar, day-count,
  date and inflation suites, and 43 % more per schedule build.
- The closed form was measured against the cache on a quiet box, pinned
  to one core, taking the median of 10 rounds.
  - Fewer cycles: `CalendarTests` 13.2 %, `DayCounterTests` 11.0 %, the
    seven inflation suites 4.0 %, `DateTests` 1.1 %.
  - More cycles: `ScheduleTests` 0.6 %, within its 2.7 % spread.
  - All five groups run fewer instructions.
  - Per call:
    - TARGET `advance` by two business days: 71 → 58 ns;
    - 30/360 `yearFraction`: 39 → 33 ns;
    - Act/Act ISDA: 53 → 42 ns;
    - a 10-year semiannual `Schedule`: 3.16 → 3.10 µs, allocating 640
      bytes instead of 1,144.
- `sizeof(Date)` is 8 again. That changes the layout of everything that
  holds a `Date`, so code compiled against the fork's earlier headers
  must be rebuilt.
- **A null `Date`** is where this differs from upstream.
  - `month()` and `dayOfMonth()` now return meaningless values. Upstream's
    throw, from `isLeap()`'s range check. The cache's `computeYMD()` ran
    that check inside a `noexcept` function, so it terminated the
    program, in `year()` too.
  - `year()` returns 1899, as upstream's does.
- `std::chrono::year_month_day` does the same conversion, but it would
  put `<chrono>` into a header that nearly every file includes.
- **The test** is `DateTests/inspectorsFromSeveralThreads`.
  - Eight threads read the year, month and day of shared `Date`s that
    nothing has read first. The results are checked against fresh
    `Date`s.
  - Built with the `tsan` cell's compiler setup against the cache, it
    stopped on ThreadSanitizer's report of `year()`'s check racing with
    the store that fills the cache. Against the closed form it is clean.
- **Verified in CI.** #106's run, 41806352468, ran all 31 jobs green in
  14 h 26 min. The `tsan` cell, which halts on a report, ran the whole
  suite with the new test and reported nothing, as `asan` and `ubsan`
  did. `date.hpp` reaches most of the library: `linux / Default`
  missed 882 of its 1,214 compiles. The merge's push, run 41982691183,
  took the light tier, its five jobs green in 59 minutes.

## New infrastructure from audit (2026-04-24)

- `ql/pricingengines/autocallable/mcautocallablenoteengine.hpp` now exposes a
  second constructor `MCAutocallableNoteEngine(process, requiredTolerance,
  maxSamples, minSamples, seed)` with adaptive batch-based convergence.
  Original `(process, requiredSamples, seed)` constructor unchanged.
- `Docs/pages/forkgroups.docs` declares `\defgroup riskanalysis` and
  `\defgroup marketdata` for Doxygen; the ten fork-specific classes now
  carry `\ingroup` tags (AutocallableNote, CdsOption → instruments;
  MCAutocallableNoteEngine, BlackCdsOptionEngine, blackFormulaT → engines;
  AADReal → math; CurveBucketer/XvaCalculator/FrtbSaGirrDelta → riskanalysis;
  CsvQuoteLoader/JsonQuoteLoader → marketdata).
- `Examples/XvaSnapshot/` — end-to-end smoke-test Example exercising
  `CsvQuoteLoader` → flat-forward curve → `FixedRateBond` pricing →
  `CurveBucketer` → `XvaCalculator` CVA in one ~130-line program.

## Infrastructure

- `Examples/AuditHarness/` — numerical harness for spot-checking
  Greeks vs. bump and piecewise-yield-curve bootstrap under
  pathological input shapes. Not part of the test suite; executable
  under `build/.../Examples/AuditHarness/AuditHarness`.
- History rewrite (2026-04-20): the 79 MB `Examples/LatentModel/LatentModel`
  binary committed in upstream in 2014 has been purged via
  `git filter-repo`. Repository object-graph size dropped ~45% (90 MB → 45 MB).
- CI consolidation (2026-08-07 → 2026-08-15): the thirteen workflow files
  inherited from upstream became a single `.github/workflows/ci.yml`. That
  layout suits GitHub-hosted runners, where thirteen workflows mean thirteen
  machines; this fork runs on one self-hosted runner sharing four cores with
  five other repositories, so they only produced thirteen runs and thirteen
  concurrency groups per commit. Consolidating buys `needs:` ordering —
  `clang-format` and `filelists` now gate the other twelve jobs — and one
  run per commit.

  Alongside it: push runs keyed by SHA so a merge commit's validation cannot
  be evicted by a newer arrival; `linux-full-tests.yml` deleted as a
  name-for-name duplicate of `linux.yml` (43 jobs per push → 31); the weekly
  cron sweeps converted to event-driven; and path-based gating, so a commit
  touching only `.github/` or git metadata costs 4 jobs instead of 32, and a
  markdown-only commit runs none. The classifier does not exempt markdown,
  so a commit pairing a CI change with a markdown edit runs the full sweep.

  Two defects were fixed rather than worked around. The `clang-tidy` job had
  never once completed a build: its preset ran tidy with `-fix`, so tidy
  rewrote `std::unique` into `std::ranges::unique` mid-compile and the
  compiler then failed on code nobody had written. It now reports. And
  `check-test-times` is advisory: it compares absolute wall-clock against
  fixed thresholds, and on this shared host the same commit measured 8.42 s
  and 47.57 s for `BasketOptionTests.testBarraquandThreeValues` against a
  30 s ceiling — it measures contention as much as test cost.
- `.claude/` is gitignored (2026-08-15). It had been tracked, and served an
  invoice and a receipt PDF publicly from master between 6f933450c and
  fb4bb64a5. Untracked, not deleted; the blobs remain reachable by SHA in
  history, so a purge would need `git filter-repo` plus GitHub Support.
  `CLAUDE.md` is gitignored for the same reason — this repository is public.
- Repository hardening (2026-10-14,
  [#26](https://github.com/dudleylane/QuantLib/issues/26)). The
  repository is public, and every CI job runs on its self-hosted runner
  as the maintainer's user. Three gaps that sibling repositories on the
  same account had already closed are now closed here:
  - **`master` is protected** against force pushes and deletion, with
    no required status checks or reviews, the same settings as
    quickfix. Direct documentation pushes still land, and `ci.yml`'s
    `paths-ignore` filter cannot leave a required check pending.
  - **Private vulnerability reporting is on**, and `SECURITY.md` says
    what to report privately.
  - **The runner refuses jobs from forks' pull requests, and
    Dependabot's.** It does so through a job-started hook installed on
    2026-10-14, ported with its decision logic unchanged from
    dudleylane/OrderProcessor#143 and the Dependabot clause of that
    repository's #168. Dependabot pushes its branches into this
    repository, so its pull requests (#3, #4) pass the fork check and
    would otherwise run a new action release on the runner before
    review. The owner ruled on 2026-10-14 that they are refused. To take
    a Dependabot update, review its diff and re-create it on a branch of
    your own, as #13 did for Dependabot's #3. That pull request's jobs
    run, and it merges on green like any other. Since #33 (2026-10-19)
    Dependabot is off and opens no pull requests here (below), but the
    clause stays. The hook's tracked copy,
    `.github/runner/job-started.sh`, came with
    [#27](https://github.com/dudleylane/QuantLib/pull/27) (`2fdf940c3`).
    Its header has the install commands and the rollback. GitHub's
    approval policy (`all_external_contributors`) remains the first gate.

  README's build section and CONTRIBUTING's pre-commit checklist are now
  self-contained. Both used to point at `CLAUDE.md`, which is gitignored.
- Merging ahead of CI ended (2026-10-14). Each of the eight merges of
  #14–#21 (2026-08-11 to 08-15) carries a `Deviation: merged ahead of CI`
  trailer: the pull request merged before its own run finished, because
  the single runner's queue was saturated. The owner ruled on 2026-10-14
  to end the practice. Every pull request now waits for its own green
  run, and an early merge is a deviation that needs its own
  justification; "the queue is long" no longer serves. Two facts settled
  it:
  - The queue pressure has mostly gone. Markdown-only commits run no CI,
    and CI-config-only diffs run 4 jobs.
  - The trailers argued that `master`'s post-merge run would verify each
    change. That held for only half of them: of the eight runs, three
    succeeded, one failed only on `check-test-times`, and four were
    cancelled.
- Pull-request tiering fixed (2026-10-14,
  [#28](https://github.com/dudleylane/QuantLib/pull/28), `ada0feb39`).
  The `changes` job diffed a pull request's test merge against the
  payload's `pull_request.base.sha`, which need not be the base GitHub
  merged into. In #27's run 38628809009 it was `94bcae858`, while the
  merge was built on `0dd2dad42`. So `master`'s own `FORK_CHANGES.md`
  commits counted as #27's, and a `.github`-only change ran the full
  sweep. The job now diffs against the test merge's first parent, which
  is that base, and runs the full sweep when the head is not a merge
  commit. Push runs are unchanged. The evidence is a local reproduction,
  with the step taken from both versions of `ci.yml`: the old one
  repeats the run's misclassification, and the new one calls the same
  merge tier 1. No live run has yet tested the case it fixes. #27's
  later run used the new step, but rebasing #27 had moved its base to
  `master`'s tip, where the two versions agree.
- Full verification moved before the merge (2026-10-17,
  [#30](https://github.com/dudleylane/QuantLib/pull/30), `fd1914290`).
  This is quickfix's model, at the owner's choice. A merged code change
  used to run the full sweep twice: 27 jobs on the pull request and 32
  on `master`'s push. The second run repeated the first on the same tree
  whenever `master` stayed put during review: #29's test merge and its
  merge share tree `b4fe95156`. Only the five `linux-nondefault` cells,
  kept off pull requests since #8, were new. That push run, 38855037743,
  also ran into GitHub's 24-hour queue limit for self-hosted runners.
  `linux / Sessions enabled` was cancelled unstarted exactly 24 hours
  after it was queued. It had passed on #29's pull request on the same
  tree, so it was not re-run.
  - **Pull requests that change code run all 32 jobs** (31 since #42,
    below), the non-default cells included, so that configuration is
    tested before it merges.
  - **The push of a pull request's merge runs 6** (5 since #42): the
    gates, the default build, CodeQL and coverage. The last two stay
    because code scanning and Coveralls record `master`'s results only
    from runs on `master`. The `changes` job recognises GitHub's merge
    commit by its two parents, a first parent equal to the push's
    `before`, and the subject "Merge pull request #N from". Anything
    else, a squash included, runs the full sweep.
  - **Unchanged:** a diff of CI config only runs 4 jobs, and a dispatch
    runs everything.
  - **Trade-offs.** If `master` moves while a pull request is under
    review, its run tested a different tree from the one that lands, so
    update the branch and wait for a new green run before merging.
    Pull-request runs grow from 27 jobs to 32, so on a busy day they can
    meet the 24-hour limit instead; `gh run rerun <id> --failed`
    finishes them.
  - **The compile cache is mostly cold.** In both of #29's runs every
    job using ccache except `linux / Default` found no cache. GitHub
    evicts entries unused for 7 days, and a pull request's entries are
    invisible to other refs. Those jobs compiled from scratch, and that,
    more than the other runners' load, is why they took two to four
    times as long as in August. Test execution itself was 1.2–1.9×
    slower. #29's merge commit put `check-test-times`' 49 minutes,
    against 13, down to that load. Most of the difference was the cold
    compile, though its slower test passes still support reading the
    timing flags as contention.
  - **First merge of a code change** (2026-10-22). #35 was the first
    pull request since #30 to change code, so its merge's push, run
    39705380139, was the first to take the 6-job path. `changes`
    logged `ci_only=false merged=true light=true`, and all six jobs
    succeeded. Code scanning recorded an analysis of the merge commit,
    `a4799af5d`, with no new alert, and Coveralls received its
    coverage. `linux / Default` hit all 1,214 compiles, because #35's
    own run had cached that tree. The run took 1 h 46 min. Before #30,
    `master` would have run the full sweep again.
- Compile cache kept on the runner, and `TMPDIR` moved off the root
  filesystem (2026-10-18,
  [#32](https://github.com/dudleylane/QuantLib/pull/32), `483d48860`,
  for [#31](https://github.com/dudleylane/QuantLib/issues/31)). The
  runner is one persistent machine, so the cache now stays on it
  instead of in GitHub's cache service, where it was mostly cold
  (above). Every job's first step sets four variables for the rest of
  the job: `CCACHE_DIR=~/.cache/ccache-quantlib-ci`, one directory for
  all of this repository's CI; `CCACHE_MAXSIZE=10G`, enforced by
  ccache's own cleanup; `CCACHE_STATSLOG`, so each job's new "Show
  compile cache statistics" step prints that job's own hits; and
  `TMPDIR`, the runner's per-job temporary directory.
  - **More jobs are cached.** `linux-nondefault`'s five cells and the
    three sanitizers now compile through ccache too, which makes 24
    jobs from seven definitions. CodeQL stays out, because its tracer
    extracts each file by watching the compiler run, and a cache hit
    skips the compiler. Coverage and clang-tidy stay out until they are
    checked separately.
  - **`TMPDIR`.** gcc writes its intermediate files to `$TMPDIR`, else
    `/tmp`, which is on the root filesystem here. That filesystem was
    down to 847 MB free on 2026-10-17, and the runner's temporary
    directory is on `/home`. The `mkstemp()` templates in two test files
    and `Examples/XvaSnapshot` still name `/tmp`, for a few hundred
    bytes each.
  - **The old action had rewritten the account's ccache config.** It
    ran `ccache --set-config` without `CCACHE_DIR`, so every run wrote
    `~/.config/ccache/ccache.conf`. That pointed every other ccache use
    on the account at this runner's workspace, capped at 500 MB and
    wiped by each checkout. With `CCACHE_DIR` set, ccache ignores the
    file. The file itself was host state, and the owner had it deleted
    on 2026-10-20, so ccache outside CI now uses its defaults
    (`~/.cache/ccache`, 5 GiB).
  - **Trust.** A pull request's jobs now write objects that `master`'s
    jobs read. That is safe only while the job-started hook refuses
    forks' and Dependabot's jobs. `ci.yml`'s note says so and gives the
    command that empties the cache.
  - **Measured.** In the full sweep dispatched before the merge, run
    39063135539, `linux / Default` hit 1,214 of 1,214 compiles from the
    objects the pull request's own run had written. GitHub's cache never
    let one ref read another's entries like that. Its build took 3.3
    minutes against 33.5 cold, and the job 17 minutes against 52. The
    other configurations filled the cache in that sweep: Indexed coupons
    hit 92.8 % through ccache's preprocessed mode, seven more cells hit
    12–43 %, and the other 15 jobs that use ccache hit 0–6.7 %. The
    directory then held 2.0 GB. `master`'s run for the merge,
    39160881696, was tier 1, and its `linux / Default` hit 1,214 of
    1,214 again, with a 1.5-minute build.
  - **Measured warm.** The first full sweep with a warm cache, run
    39374746533, was dispatched on `master` (`a4fddb644`) on 2026-10-20
    while the box was quiet. All 32 jobs succeeded, and all 24 that use
    ccache hit 100 %. Their compile steps took 1.1–1.6 minutes, against
    3.3–49.7 in the fill sweep. The run took 9 h 14 min, against 24
    hours, and its jobs 553 minutes in all, against 1,446. Part of that
    is the quiet box: CodeQL, coverage and clang-tidy, which use no
    cache, were 28–42 % faster too. The directory stayed at 2.0 GB.
    `check-test-times` passed, with test passes of 1.5, 3.2 and 6.6
    minutes against August's quiet 1.6, 3.4 and 7.1. #31 is closed, and
    its last comment has the per-job table.
- Dependabot turned off (2026-10-19,
  [#33](https://github.com/dudleylane/QuantLib/pull/33), `d8240276a`),
  as in quickfix, at the owner's request. Upstream's
  `.github/dependabot.yml`, which had Dependabot check the workflows'
  actions every week, is deleted. Dependabot's alerts and security
  updates were already disabled, so none of its features is on now.
  - **Why.** Since #26 the runner refuses every job that Dependabot's
    events start, so its pull requests could never pass CI or merge, and
    each update had to be re-created by hand. In four months it opened
    two: #3, re-created as #13, and #4, for an action the fork had
    stopped using.
  - **What stops.** Nothing announces a new major version of the
    actions `ci.yml` uses: `actions/checkout@v7`,
    `actions/upload-artifact@v7` and `coverallsapp/github-action@v2`,
    and `github/codeql-action@v4` until #42 removed it. Minor and patch
    releases still arrive through those floating major tags. A new
    major is a manual bump, as `741c59410` was.
  - **What stays.** The hook's Dependabot clause. Security updates can
    be switched on in the repository's settings without any
    configuration file, and the clause keeps their jobs off the runner.
    #32's trust argument (above) rests on it.
- CodeQL dropped (2026-10-24,
  [#42](https://github.com/dudleylane/QuantLib/pull/42), `9726112aa`), at
  the owner's request. The `analyze` job, its `security-events: write`
  permission and `.github/codeql/codeql-config.yml` are gone. A pull
  request that changes code now runs 31 jobs, and a merge's push runs 5:
  the gates, the default build and coverage.
  - **Why.** It took 58–61 minutes of the one runner in each of the
    eight runs from 2026-10-20 to 10-23, and 55–117 in the nine before.
    Its tracer watches the compiler run, so it couldn't use the compile
    cache. Every code change waited for it twice: on its pull request,
    and again after its merge, where it took 59–61 of the run's 103–105
    minutes. All 12 analyses of `master` since 2026-08-08 reported the
    same six alerts, and no pull-request analysis reported more.
    - Five were false positives in `test-suite/timegrid.cpp`. Boost.Test's
      `report_assertion` takes a count before its variadic arguments, and
      CodeQL read the trailing count of 0 as a terminator.
    - The sixth, `std::localtime` in `Date::todaysDate()`, is real and
      minor. [#41](https://github.com/dudleylane/QuantLib/issues/41)
      records it.
    - None of the six was triaged in two and a half months.
  - **What stops.** Static analysis of the C++, on pull requests and on
    `master`; it was CI's only static security analysis. The sanitizers
    still run the whole suite on every pull request that changes code,
    and clang-tidy still runs, report-only.
  - **Code scanning.** No analysis of `master` will run again to close
    the six alerts, so they were dismissed on 2026-10-25, at the owner's
    choice: the five in `test-suite/timegrid.cpp` as false positives,
    and `std::localtime` as "won't fix", with a comment that #43 fixed
    it, since GitHub has no dismissal reason for fixed code. A dismissal
    can be undone. The `ci.yml:analyze` configuration and its nine
    analyses of `master` stay. The stale `codeql-analysis.yml`
    configuration's three analyses were deleted on 2026-10-24.
    [#40](https://github.com/dudleylane/QuantLib/issues/40) removed that
    stale configuration so that pull requests' `CodeQL` checks would stop
    being neutral. With CodeQL gone there is no such check, so #40 closed
    as superseded.
  - **The runner.** The two CodeQL bundles in its tool cache, 2.27.2 and
    2.27.3 at 1.4 GB each, were removed with the owner's OK. That emptied
    `~/actions-runner-quantlib/_work/_tool`.
  - **Tested by #43** (2026-10-24), the first pull request since #42 to
    change code. #42 touched only `.github/`, so its own run and its
    merge's push were both tier 1. #43's run, 40100434340, logged
    `ci_only=false merged=false light=false` and ran 31 jobs, all
    green, in 9 h 12 min. Its merge's push, run 40142200854 on
    `193d57fe7`, logged `ci_only=false merged=true light=true` and ran
    5: the gates, `linux / Default` and coverage, with the other nine
    job definitions skipped. Coveralls received its coverage. The run
    took 44 minutes, against 1 h 43 min for each of #38's and #39's,
    which ran CodeQL.
  - For CodeQL only, this reverses the 2026-10-17 decision (#30, above)
    that kept CodeQL and coverage in the run after a merge.
- Workflow lint added to the `clang-format` gate (2026-11-06,
  [#107](https://github.com/dudleylane/QuantLib/pull/107), `6e28a984b`,
  for [#76](https://github.com/dudleylane/QuantLib/issues/76)).
  `actionlint` exited 1 on `master` with five shellcheck findings and no
  job ran it, so a new finding hid among the five. They are now cleared
  or marked as intended, and a "Lint the workflow" step fails the gate.
  It uses the image OrderProcessor's lint job uses, pinned by digest,
  with no network and no mount. It ignores only the label `quantlib-dev`,
  so a mistyped label fails. The tiers stay at 4, 5 and 31 jobs.
  - **Verified.** Run 42179777926 ran the step on the runner, all 4 jobs
    green in 34 minutes. Locally it exits 0 on the file and 1 on each of
    a planted unquoted `$(ls)`, duplicate key, unclosed expression and
    mistyped label.
  - **New risk.** The gate now depends on podman: if it or the pull
    fails, the gate fails and skips the rest of the run. The step is
    limited to 10 minutes, because a hung pull would hold the only runner.
- Speed tiers after #173 (2026-11-09,
  [PR #190](https://github.com/dudleylane/QuantLib/pull/190), `70e34b787`).
  `check-test-times` fails a case that runs in a slower tier than its
  time calls for, as well as one too slow for its tier, and failed in
  #173's run, 42412874489: `CdoTests` carried a Slow precondition that
  its 32 new cases inherited, and #173 sped up `testHW`'s Gaussian data
  sets and `FdHestonTests`' `testFdmHestonBarrierVsBlackScholes` below
  their tiers. The suite drops the precondition, `testHW` splits into
  `testHW` (Gaussian, Fast) and `testHWStudentT` (Slow), and the FD
  Heston case loses its Fast precondition. `check-test-times` passed in
  #190's own run, 42476494791.
- Four runners (2026-11-11, for
  [#73](https://github.com/dudleylane/QuantLib/issues/73)). At the
  owner's request, `quantlib-localhost-2` to `-4` joined
  `quantlib-localhost` on the same machine, all labelled `quantlib-dev`,
  so GitHub hands a run's queued jobs to whichever runner is idle.
  `ci.yml` doesn't change, and no job runs faster alone. Each runner has
  its own directory, user unit and copy of the job-started hook, since
  the hook refuses a payload outside its own runner's `_work`; each copy
  was checked on five payloads before its runner started. A QuantLib run
  starts only while the machine's other projects are idle (owner,
  2026-11-11).
  - **Measured.** #236's run, 43038316532, the first on four runners,
    took 8 h 19 min for 31 jobs. #106's, which edited a header about as
    widely included, took 14 h 26 min on one runner. The jobs ran 3.90
    at a time on average and took 1,948 minutes in all, against #106's
    865, since four jobs each run `make -j"$(nproc)"` on 8 threads.
    MemAvailable never fell below 9.73 GB
    ([#73](https://github.com/dudleylane/QuantLib/issues/73#issuecomment-6809263828)).
  - **New risk.** Memory: the machine has no OOM policy, and four
    compiles at once are measured on one run only. `check-test-times`
    reads wall-clock under more load, so it fails more often. A flag
    from a parallel run is no reason by itself to re-tier a case: the
    flagged cases' instruction counts against `master`'s say whether
    load explains it. The tracked hook's header gives runner 1's
    install commands only.

## Sibling repositories

- **dudleylane/QuantLib-SWIG — deleted 2026-08-15.** It was a real GitHub
  fork of `lballabio/QuantLib-SWIG` (created 2026-04-21, 12 fork commits,
  134 ahead / 12 behind upstream at deletion) and carried
  `SWIG/dudleylane_fork.i`: full Python bindings for `CsvQuoteLoader` plus
  TODO scaffolds for the other fork-specific classes.

  **The language-binding work now has no home.** Anything targeting
  `AutocallableNote`, `CdsOption`, `MCAutocallableNoteEngine`,
  `BlackCdsOptionEngine`, `AADReal`, `CurveBucketer`, `XvaCalculator`,
  `FrtbSaGirrDelta`, `CsvQuoteLoader` or `JsonQuoteLoader` needs a new
  repository, or must move into this one.

  This was a deliberate retirement, not an accident: the repository, its
  self-hosted runner and service, the runner installation, and the local
  working clone were all removed on 2026-08-15. The bindings work is not
  being carried forward. A `git bundle` and the text of the repository's
  open issue were taken under `/tmp` at the time, but `/tmp` is
  disposable and they are not a preservation plan — treat the binding
  code as gone.

  One thing from it is worth keeping and is *not* a bindings problem: the
  repository's open issue #1 reported that `PiecewiseFlatForward` fails
  to extend its curve span on `recalculate()` once the evaluation date
  comes within roughly thirty years of the longest pillar. It surfaced
  through the Python test
  `test_termstructures.py::TermStructureTest::testLazyObject` and was
  carried over as [#23](https://github.com/dudleylane/QuantLib/issues/23).
  - **Diagnosed, twice.** The Python test was recovered from the bundle
    and is preserved in #23, beside a C++ reproduction. It builds its
    curve on a fixed reference date from tenor-based helpers.
    `IterativeBootstrap::calculate()` re-initialises only when
    `!initialized_ || ts_->moving_`, so after the evaluation date moves,
    the first solve uses the old pillars and fails.
    - On 2026-10-20 the diagnosis stopped at that guard, which is
      upstream's, and called the failure upstream behaviour. The owner
      closed #23 on that diagnosis, with no library change.
    - It is not the whole cause. A failed solve on top of a valid curve
      makes the bootstrap re-initialise and retry
      (`iterativebootstrap.hpp:355`), and upstream's retry succeeds with
      the pillars moved. Upstream's QuantLib-SWIG still runs the Python
      test, which asserts exactly that.
    - Here the retry checked its range against the stale `maxTime()`
      cache and failed too, so #23 is the same bug as
      [#34](https://github.com/dudleylane/QuantLib/issues/34)
      ([Performance optimizations](#performance-optimizations-2026-04-26),
      issue #1). #23 was reopened with the correction on 2026-10-21.
  - **Tested.** [#24](https://github.com/dudleylane/QuantLib/pull/24)
    (`bcd41138e`) ported the test to C++ as `testLazyObject`, built with
    `settlementDays` so the curve moves. It also added
    `testLazyObjectWithFixedReferenceDate`, which asserted that the
    fixed-date curve throws.
    - Since #35 that case asserts upstream's behaviour: the reference
      date stays and every other node moves.
    - `0965a969f` had made both cases skip under indexed coupons, on the
      wrong root cause. #35 removed the guards, so both run in every
      configuration.
  - **Fixed** by [#35](https://github.com/dudleylane/QuantLib/pull/35)
    (`a4799af5d`, 2026-10-22), which closed #23 and #34.

## Explicit follow-up list

Items with **partially addressed** status this round are noted
inline; the remaining bullets are still-pending work.

- Credit-derivatives extensions: **partially addressed** — CDS option
  promoted. Remaining ~43 files (CDO, nth-to-default, copulas,
  loss-distribution models, basket CDS, `syntheticcdo`, credit-risk+)
  continue to live in `ql/experimental/credit/`.
- Engine-level AAD retrofit: **partially addressed** — `blackFormulaT`
  demonstrates the template-parameterise-Real pattern on one
  primitive. Retrofit of `BlackCalculator`, `AnalyticEuropeanEngine`,
  process / curve hierarchy, and instruments still pending.
- Regulatory calculators: **partially addressed** — `FrtbSaGirrDelta`
  ships the aggregation math for the GIRR delta bucket charge.
  Vega / curvature, other FRTB risk classes (CSR non-sec / sec /
  corr, equity, commodity, FX), cross-bucket γ_bc aggregation, SIMM,
  and SA-CCR are pending.
- XVA follow-ups: wrong-way risk, collateral thresholds / MTAs,
  netting-set aggregation, stochastic exposure-simulation framework.
- Autocallable extensions: worst-of / best-of, memory / snowball
  coupons, Heston / SLV engine variants, knock-in barriers observed
  during life, issuer-callable variants.
- Market-data loaders: **partially addressed** — JSON via nlohmann
  shipped alongside CSV. Arrow / Parquet, streaming (Kafka / ZeroMQ /
  WebSocket), and vendor formats (Bloomberg BVAL, Refinitiv, CME FIX,
  ICE) are pending.
- SWIG interface-file updates: **dropped** — the sibling repository that
  held them was retired on 2026-08-15 (see *Sibling repositories*). This
  fork currently has no language bindings, and reviving them means
  starting a new repository or bringing the interface files in here.
- CI wiring (GitHub Actions matrix across GCC / Clang / C++23 /
  nonstandard-options): **addressed** — see the CI entry under
  *Infrastructure*. The matrix runs twelve configuration cells plus five
  non-default cells, three sanitizers, both build systems and clang-tidy
  on every pull request that changes code. Since #30, master runs only
  the gates, the default build and coverage after a merge, and CodeQL
  until #42 dropped it.
