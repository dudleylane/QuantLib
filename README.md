
# QuantLib — dudleylane fork

[![Licensed under AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0--or--later-red.svg)](https://github.com/dudleylane/QuantLib/blob/master/LICENSE)
[![Upstream BSD-3-Clause](https://img.shields.io/badge/Upstream-BSD--3--Clause-blue.svg)](https://github.com/dudleylane/QuantLib/blob/master/LICENSE.TXT)

> **This is a personal fork of [lballabio/QuantLib](https://github.com/lballabio/QuantLib).**
> Master history has been rewritten to remove a 79 MB legacy binary blob (2014)
> and several correctness fixes have been applied that are **not** in upstream.
> Commit SHAs diverge from upstream starting at `51d4bb377`, so syncing
> upstream changes requires rebase-across-divergence rather than a clean pull.
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

## What's different in this fork

### Toolchain floor (raised vs. upstream)

Enforced at configure time in `CMakeLists.txt` and `acinclude.m4`:

- **GCC ≥ 15.2.1**
- **C++ ≥ 23**
- **Boost ≥ 1.83.0**

Upstream still accepts GCC 9+, C++17, and Boost 1.58. This fork will
refuse to configure on an older toolchain.

### Correctness fixes applied (not in upstream)

| ID | File | Fix |
|---|---|---|
| F1 | `ql/models/shortrate/onefactormodels/hullwhite.cpp` | Bond-option variance rewritten in numerically stable form `(u-v)² · expm1(2am)` (Brigo-Mercurio) — no more flooring at 0 |
| F2 | `ql/processes/gsrprocesscore.cpp` | Out-of-range `time2()` now validates `T_ ≥ times_.back()` on demand |
| F3 | `ql/models/volatility/garch.cpp` | GARCH(1,1) calibration now checks optimizer `EndCriteria::Type` |
| FE1 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | BAW Newton loop bounded (`maxIterations = 100`) — fixes infinite-loop on pathological inputs |
| FE2 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | BAW negative-rate error message now points users to `BjerksundStenslandApproximationEngine`, which handles negative rates natively |
| FE3 | `ql/pricingengines/vanilla/baroneadesiwhaleyengine.cpp` | T → 0⁺ short-circuit to European + intrinsic |
| FE6 | `ql/termstructures/volatility/sabr.hpp` | Public `sabrIsRiskyRegime()` detector (Doust 2012 absorption threshold) + documentation pointing to `NoArbSabrSmileSection` for arbitrage-free pricing |
| FE11 | `ql/pricingengines/asian/analytic_discr_geom_av_price_heston.{hpp,cpp}` | Moved out of `ql/experimental/`; the public Heston-Asian MC engine no longer transitively imports experimental code |

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
  pulls v3.11.3 via `FetchContent`. When the flag is off, the header
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

- **GCC 15.2.1 + C++23 + Boost 1.83** (default): 1304 cases, 0 failures
- **Clang 21.1.6 + C++23 + Boost 1.83**: 1274 cases, 0 failures (taken at the Phase-0 baseline; new-module tests land on top)
- **`linux-ci-build-with-nonstandard-options` preset** (sessions on, thread-safe observer on, `QL_FASTER_LAZY_OBJECTS=OFF`, `QL_THROW_IN_CYCLES=ON`, high-res date on, indexed coupons, warnings-as-errors): 1280 cases, 0 failures (Phase-0 baseline)
- **`QL_ENABLE_AAD=ON`** (GCC 15.2.1, CoDi-Pack v2.2.0 auto-fetched): `AadTests/*` pass end-to-end

CI is not wired on this fork; all verification is local.

### Language bindings

Python / R / Java / .NET bindings for this fork live in the sibling
repository **[dudleylane/QuantLib-SWIG](https://github.com/dudleylane/QuantLib-SWIG)**,
which is itself a fork of upstream `lballabio/QuantLib-SWIG`. A
`SWIG/dudleylane_fork.i` scaffold is already committed there with
full Python bindings for `CsvQuoteLoader` and TODO markers for the
remaining fork-specific classes: `FallbackIborIndex`, `CurveBucketer`,
`XvaCalculator`, `AutocallableNote` + `MCAutocallableNoteEngine`,
`FrtbSaGirrDelta`, `JsonQuoteLoader`, `blackFormulaT`, and the
`aad.hpp` helpers (plus include-path adjustments for the promoted
no-arb SABR + Heston Asian control variate + CDS option). Bindings
work continues on that sibling, not here.

## Build, test, run

See `CLAUDE.md` in the repo root for the canonical build recipe (CMake
preset + test-suite filter syntax). Quick reference:

```bash
cmake --preset linux-gcc-ninja-release
cmake --build build/linux-gcc-ninja-release -j
./build/linux-gcc-ninja-release/test-suite/quantlib-test-suite --log_level=message
```

## Issues and questions

- **Fork-specific issues, PRs, and discussions:** <https://github.com/dudleylane/QuantLib/issues>
- **Upstream general questions / the wider QuantLib community:** [quantlib-users mailing list](https://www.quantlib.org/mailinglists.shtml) — maintained by the upstream project, not this fork.
- **Upstream bugs / patches:** report to [lballabio/QuantLib/issues](https://github.com/lballabio/QuantLib/issues); anything merged there will need to be brought across the divergence point.

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
- **If you extract files that are new in this fork** (currently
  `ql/indexes/fallbackiborindex.{hpp,cpp}`), they are AGPL-3.0+ only.

Upstream copyright holders are not altered by this fork. BSD-3-Clause and
AGPL-3.0 are legally compatible in the BSD → AGPL direction, which is what
this arrangement relies on. For anything at the edge of this policy consult
a lawyer — I am not one.
