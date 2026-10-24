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
| F1 / FE5 | `ql/models/shortrate/onefactormodels/hullwhite.cpp` | Bond-option variance rewritten as `(e^{-aS} - e^{-aM})^2 · expm1(2am)` — non-negative by construction; no more `sqrt(std::max(c, 0.0))` band-aid (Brigo-Mercurio, ch. 3). |
| F2 | `ql/processes/gsrprocesscore.cpp` | Out-of-range `time2()` validates `T_ ≥ times_.back()` only on the unsafe branch, preserving valid over-specified parameter sets for in-grid queries. |
| F3 | `ql/models/volatility/garch.cpp` | Captures `EndCriteria::Type` from `minimize()`; rejects `None` and `Unknown`. |
| FE1 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | BAW Newton loop bounded at 100 iterations with explicit non-convergence error. |
| FE2 | same | The rejection of negative rates in `criticalPrice()` is upstream's (`c75a3fb7e`, lballabio/QuantLib#1291); FE2 rewrote its message to point users to `BjerksundStenslandApproximationEngine` (which handles them natively). Until #36, calls with r < q ≤ 0 never reached it and got the European value, which can fall below intrinsic, and puts reached it where the European value is exact. Since #36 only cases where early exercise can be optimal reach it, and since #37 neither engine reaches it at zero variance (FE3). `testBaroneAdesiWhaleyNegativeRates` checks the rejection for BAW and Ju, and that BS prices the rejected cases. |
| FE3 | same, and `juquadraticengine.cpp` since #37 | T → 0⁺ short-circuit (variance below `QL_EPSILON`) instead of running Newton on a near-degenerate variance. Since #36 it comes after the European shortcut, so it applies only where early exercise can be optimal. Until #37 it returned `max(european, immediate-exercise)`, which misses the case where the best exercise time falls inside the option's life (a call at S = 240, K = 100, r = 5 %, q = 2 %, 3 years: 140.000 against 140.134); it now returns the value of the best deterministic exercise time, which is that maximum everywhere else, bitwise. Ju had no such branch and threw at zero variance; it now has it, with its delta and gamma. At an interior exercise time the gamma comes from that time moving with the spot; where the European value wins they are the European's at that variance, so near the money, just below the threshold, Ju's gamma can reach the order of 1e7, as `AnalyticEuropeanEngine`'s does (1.4e7 for an at-the-money call at σ = 1e-9 and 30 days). BAW's stays value-only. |
| [#36](https://github.com/dudleylane/QuantLib/issues/36) (2026-10-22) | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp`, `juquadraticengine.cpp` | Both engines return the European value and Greeks wherever early exercise is never optimal: calls with q ≤ 0 and q ≤ r, puts with r ≤ 0 and r ≤ q. They tested only q ≤ 0, for calls, so calls with r < q ≤ 0 got the European value, which can fall below intrinsic (S = 100, K = 80, r = −5 %, q = 0, σ = 3 %, 3 years: 7.05 against 20); they now get FE2's error. Puts at negative rates threw, and Ju returned NaN for puts at r = 0. Ju also divided by α and h, both 0 at r = 0, so it returned NaN wherever early exercise can be optimal there; it now works with α/h, whose limit is 2/σ²T. At r > 0 BAW is unchanged and Ju moves by rounding only (at most 2.4e-13 over a 6,300-point grid). Both engines' test data give identical values, except BAW's put at r = q = 0, which moves 1.1e-7 onto the European value. Ju's values at r = 0 are the limit of those just above it, inaccuracy included: for puts with q = −1 %, σ = 40 % and 3 years they are up to 8.1 off a finite-difference price at r = 0, and up to 1.1 at r = 1e-4, where BAW is within 0.2. Upstream fixed the same defects in 1.44 (lballabio/QuantLib#2728, #2739, #2754); this is the fork's own change. Tests: `testBaroneAdesiWhaleyAndJuWhereEarlyExerciseIsNeverOptimal`, `testJuValuesAtZeroRate`, and a rewritten `testBaroneAdesiWhaleyNegativeRates`. |
| [#37](https://github.com/dudleylane/QuantLib/issues/37) (2026-10-22) | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.{hpp,cpp}`, `juquadraticengine.cpp` | Both engines threw at low volatility wherever the carry works against early exercise, for puts with r > q and calls with q > r. As the variance vanishes, `criticalPrice()`'s seed leaves its range between Su, the perpetual critical price, and the strike: h grows like 2b²T/σ², a put's seed goes to +∞ and a call's to −∞, and `blackFormula` throws. A put seed that stays finite but huge fails too, because Newton then loses the strike to cancellation. The seed now falls back to Su wherever it leaves that range; Su is finite there and tends to the strike, as the critical price does. Ju, which also threw at zero variance, gets FE3's branch. Both engines take the zero-variance value from one helper, `detail::zeroVarianceResults()`, declared in `baroneadesiwhaleyengine.hpp` and defined in its `.cpp`; a file-local copy in each engine broke `--enable-unity-build`, which compiles a directory's sources as one file (PR #39's first run, 39899945305). A probe of 60,154 cases (σ from 0 to 20 %, 30 days to 10 years, with dense sweeps at low σ and just above FE3's threshold) has 32,182 where early exercise can be optimal at r ≥ 0. BAW threw in 12,402 of them and Ju in 16,901; neither throws now. Values that move: 495 BAW and 488 Ju values at σ from 0.03 % to 3 %, where Newton converged anyway from a seed outside the range, by at most 3.6e-5 and 1.7e-6 (Newton stops within its 1e-6 tolerance at a different point, and neither seed's values are systematically nearer a critical price converged to 1e-12); 24 BAW values at zero variance, by up to 0.134 (FE3's interior case); and 816 Ju values below FE3's threshold, which now take the exact zero-variance value instead of Ju's approximation, by up to 0.82. Both engines' test data give identical values. Upstream fixed the put half in 1.44 (lballabio/QuantLib#2749, #2755). Applied to the fork's files, its put-only clamp left 7,290 of the 12,402 seed throws: every call, and the puts whose seed is finite but huge. This is the fork's own change. Not changed: where Q stays finite as σ → 0 (puts with q > r, calls with r > q), the engines' small-σ limit is not the exact zero-volatility value, so the value jumps at FE3's threshold, by up to 0.26 for BAW and 0.037 for Ju on the probe's 10-year cases. Tests: `testBaroneAdesiWhaleyAndJuAtLowVolatility`, `testBaroneAdesiWhaleyAndJuAtZeroVolatility`. |
| FE6 | `ql/termstructures/volatility/sabr.hpp/.cpp` | Added public `sabrIsRiskyRegime()` detector for the Hagan 2002 unsafe-regime; documented `NoArbSabrSmileSection` as the arbitrage-free alternative. |
| FE9 | *(withdrawn)* — binary-barrier KO Greeks returning `0` is mathematically correct under continuous monitoring; the audit finding was incorrect. |
| FE11 | `ql/pricingengines/asian/mc_discr_arith_av_price_heston.hpp` | Include path updated after control-variate promotion; public MC engine no longer transitively imports experimental code. |
| A1 (2026-04-24) | `ql/pricingengines/blackformulatemplate.hpp` | Added `displacement` parameter and `stddev==0` short-circuit to `blackFormulaT`; Real-path now dispatches to `CumulativeNormalDistribution` via `if constexpr` (matches `blackFormula()` bit-for-bit); AAD path keeps `std::erf` for CoDi tape overload. Precondition checks match upstream `checkParameters()`. |
| A2 (2026-04-24) | `ql/quantlib.hpp`, `ql/{instruments,pricingengines,math}/all.hpp`, `ql/instruments/Makefile.am` | Umbrella-header chain fixed: `autocallablenote.hpp`, `aad.hpp`, `blackformulatemplate.hpp`, `autocallable/all.hpp`, `risk/all.hpp`, `marketdata/all.hpp` now transitively reachable from `<ql/quantlib.hpp>`. |
| A3 (2026-04-24) | `configure.ac` | Added `--enable-aad` and `--enable-json-marketdata` autotools flags with `AC_CHECK_HEADER` probes. |
| A4 (2026-04-24) | `ql/instruments/autocallablenote.{hpp,cpp}` | `isExpired()` uses `<=`; `arguments::validate()` now mirrors every ctor invariant. |
| A5 (2026-04-24) | `ql/marketdata/csvquoteloader.cpp` | Replaced locale-sensitive `std::stod` with `std::from_chars`; added UTF-8 BOM strip; `std::isfinite` validation with file:line error context. |
| A6 (2026-04-24) | `ql/marketdata/jsonquoteloader.cpp` | Rejects ambiguous schema (Schema A + B siblings); rejects empty `id`; validates `std::isfinite` on every value. |
| A7 (2026-04-24) | `ql/risk/xvacalculator.{hpp,cpp}` | CVA/DVA/FVA/KVA/MVA integrals switched from left-endpoint Riemann to trapezoidal quadrature; matches Brigo-Morini-Pallavicini convention. |
| A8 (2026-04-24) | `ql/risk/curvebucketer.cpp` | `ScopedQuoteRestore` now holds the `shared_ptr` by value (was by const-ref) to harden against future refactors that capture the guard. |
| A9 (2026-04-24) | `ql/indexes/fallbackiborindex.cpp` | Removed dead-code null-fallback ternaries via a `checkLegacy` helper; added ctor assertion that the legacy and RFR indices share a day counter (otherwise the post-cessation tau/discount pairing is inconsistent). |
| A10 (2026-04-25) | `test-suite/curvebucketer.cpp` | `testBucketedDeltaSign` was over-specified (required ≥2 strictly-positive buckets for a 5Y swap with a 10Y curve quote, which doesn't hold under `QL_USE_INDEXED_COUPON=ON` because the 10Y quote never feeds the 5Y swap's cashflow dates). Relaxed to the financial invariant: every bucket non-negative, at least one positive, sum positive. Surfaced by the `linux-ci-build-with-nonstandard-options` preset. |

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

Caveats:
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
  - **Code scanning.** The six alerts are still open. Deleting the
    `ci.yml:analyze` configuration's nine analyses of `master`, as the
    stale `codeql-analysis.yml` configuration's three were deleted
    earlier that day, waits on the owner.
    [#40](https://github.com/dudleylane/QuantLib/issues/40) removed that
    stale configuration so that pull requests' `CodeQL` checks would stop
    being neutral. With CodeQL gone there is no such check, so #40 closed
    as superseded.
  - **The runner.** The two CodeQL bundles in its tool cache, 2.27.2 and
    2.27.3 at 1.4 GB each, were removed with the owner's OK. That emptied
    `~/actions-runner-quantlib/_work/_tool`.
  - **Untested until the next pull request that changes code.** #42
    touched only `.github/`, so its run and its merge's push were both
    tier 1. The next code pull request should run 31 jobs, and its
    merge's push should log tier 2 with 5.
  - For CodeQL only, this reverses the 2026-10-17 decision (#30, above)
    that kept CodeQL and coverage in the run after a merge.

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
