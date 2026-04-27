# Changes in this fork vs. upstream lballabio/QuantLib

This file tracks changes introduced by the `dudleylane/QuantLib` fork
that are **not** in upstream. It replaces the upstream `News.md`
release-notes file (removed on this fork — it described upstream's
1.42.1 release, not fork activity) and complements the per-commit
messages. Entries are grouped by category and, within a category, most
recent first.

Divergence point: all commits from `51d4bb377` onwards have different
SHAs on this fork due to the 2014 binary-blob history rewrite. Merging
upstream into this fork requires rebase-across-divergence.

License: the fork's aggregate distribution is **AGPL-3.0-or-later**
(see `LICENSE`). Upstream files retain their original **BSD-3-Clause**
per-file headers (see `LICENSE.TXT`). New files added by this fork
carry AGPL-3.0+ file-level copyright notices.

---

## Toolchain floor raised

- **GCC ≥ 15.2.1**, **C++ ≥ 23**, **Boost ≥ 1.83.0** enforced in
  `CMakeLists.txt` and `acinclude.m4`. Upstream still compiles on GCC
  9 / C++17 / Boost 1.58.
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

- **`QL_ENABLE_AAD`** (default `OFF`) — when `ON`, pulls CoDi-Pack v2.2.0
  via `FetchContent` (or picks up a locally-installed `codi.hpp`), sets
  `QL_ENABLE_AAD` as a compile definition, and activates the reverse-
  mode AAD utilities in `ql/math/aad.hpp`. Default builds unchanged.
- **`QL_ENABLE_JSON_MARKETDATA`** (default `ON`) — when `ON`, looks for
  an installed `nlohmann_json` and falls back to `FetchContent` on
  nlohmann/json v3.11.3 otherwise. Sets `QL_ENABLE_JSON_MARKETDATA` as
  a compile definition and links `nlohmann_json::nlohmann_json` into
  `ql_library`. When `OFF`, `ql/marketdata/jsonquoteloader.hpp` still
  compiles but the constructor throws.

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

IDs reference the session-level audit. All have regression tests that
accompany them on the same commit.

| ID | File | Fix |
|---|---|---|
| F1 / FE5 | `ql/models/shortrate/onefactormodels/hullwhite.cpp` | Bond-option variance rewritten as `(e^{-aS} - e^{-aM})^2 · expm1(2am)` — non-negative by construction; no more `sqrt(std::max(c, 0.0))` band-aid (Brigo-Mercurio, ch. 3). |
| F2 | `ql/processes/gsrprocesscore.cpp` | Out-of-range `time2()` validates `T_ ≥ times_.back()` only on the unsafe branch, preserving valid over-specified parameter sets for in-grid queries. |
| F3 | `ql/models/volatility/garch.cpp` | Captures `EndCriteria::Type` from `minimize()`; rejects `None` and `Unknown`. |
| FE1 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | BAW Newton loop bounded at 100 iterations with explicit non-convergence error. |
| FE2 | same | Rejects negative rates with a message pointing users to `BjerksundStenslandApproximationEngine` (which handles them natively). Test verifies both the rejection and that BS works on the same input. |
| FE3 | same | T → 0⁺ short-circuit returns `max(european, immediate-exercise)` instead of running Newton on a near-degenerate variance. |
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

Subclasses overriding `TermStructure::update()` *must* call
`TermStructure::update()` for the cache invalidation to fire;
existing subclasses that bypass the base (e.g. some smile sections
that call `notifyObservers()` directly) were already violating the
observer contract and are not regressed.  Verified under
`linux-ci-build-with-nonstandard-options` (sessions on, thread-safe
observer on, `QL_THROW_IN_CYCLES=ON`, `QL_FASTER_LAZY_OBJECTS=OFF`).

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

## Sibling repositories

- **[dudleylane/QuantLib-SWIG](https://github.com/dudleylane/QuantLib-SWIG)** —
  fork of upstream SWIG bindings. `SWIG/dudleylane_fork.i` now ships
  full Python bindings for `CsvQuoteLoader` and TODO scaffolds for the
  other fork-specific classes. Remaining language-binding work lands
  there, not here.

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
- SWIG interface-file updates: **partially addressed** — SWIG fork
  ships full bindings for `CsvQuoteLoader` and TODO scaffolds for
  all other fork-specific classes. Remaining work is mechanical.
- CI wiring (GitHub Actions matrix across GCC / Clang / C++23 /
  nonstandard-options).
