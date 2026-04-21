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

## New build-time options

- **`QL_ENABLE_AAD`** (default `OFF`) — when `ON`, pulls CoDi-Pack v2.2.0
  via `FetchContent` (or picks up a locally-installed `codi.hpp`), sets
  `QL_ENABLE_AAD` as a compile definition, and activates the reverse-
  mode AAD utilities in `ql/math/aad.hpp`. Default builds unchanged.

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

## Promotions out of `ql/experimental/`

| Module | From | To |
|---|---|---|
| No-arbitrage SABR (Doust 2012) — 9 files incl. a 7.9 MB absorption-probability table | `ql/experimental/volatility/noarbsabr*` | `ql/termstructures/volatility/noarbsabr*` |
| Himalaya / Everest / Pagoda instruments + MC engines (12 files) | `ql/experimental/exoticoptions/` | `ql/pricingengines/exotic/` |
| Discrete geometric-average Asian under Heston (analytic, control-variate for the public MC Asian engine) | `ql/experimental/asian/analytic_discr_geom_av_price_heston.{hpp,cpp}` | `ql/pricingengines/asian/analytic_discr_geom_av_price_heston.{hpp,cpp}` |

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
  fork of upstream SWIG bindings. Created but not yet updated for the
  fork's additions. Any language-binding work lands there, not here.

## Explicit follow-up list

Not done in this series of commits, tracked here for next pass:

- Credit-derivatives extensions (promoting ~45 files from
  `ql/experimental/credit/` — CDOs, CDS options, nth-to-default).
- Engine-level AAD retrofit: template-parameterising `Real` across all
  pricing engines so AAD types propagate through the call graph.
- Regulatory calculators (FRTB SA, SIMM, SA-CCR) built on top of
  `CurveBucketer` output.
- XVA follow-ups: wrong-way risk, collateral thresholds / MTAs,
  netting-set aggregation, stochastic exposure-simulation framework.
- Autocallable extensions: worst-of / best-of, memory / snowball
  coupons, Heston / SLV engine variants, knock-in barriers observed
  during life, issuer-callable variants.
- Market-data loaders: vendor formats (Bloomberg BVAL, Refinitiv),
  Arrow / Parquet, streaming updates.
- SWIG interface-file updates in the sibling fork for every class
  listed in the "New classes" table above.
- CI wiring (GitHub Actions matrix across GCC / Clang / C++23 /
  nonstandard-options).
