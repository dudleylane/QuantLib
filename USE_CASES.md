# Use-case guide for the dudleylane fork

Five honest assessments of what `dudleylane/QuantLib` is good for today
and what you would still have to build on top. All claims reflect the
fork **as of the most recent commit on `master`**, not upstream.

See `FORK_CHANGES.md` for the full list of classes and behaviours that
differ from upstream `lballabio/QuantLib`.

---

## 1. Quant researcher prototyping a model

**Verdict:** **Production-ready.**

**What works today:**
- Wide model menu: Heston (Gatheral-stable characteristic function as
  default, `QuadraticExponentialMartingale` MC discretization per
  Andersen 2008), Bates, CIR, Vasicek, Hull-White 1F/2F, G2++,
  Black-Karasinski, GSR, Markov-Functional, LMM / BGM with pathwise
  Greeks for smooth payoffs, Heston-SLV, GJR-GARCH.
- Full numerical stack: Gauss-Legendre / Laguerre / Hermite / Kronrod
  quadrature, Brent / Newton / Ridder / Halley / Nelder-Mead /
  Levenberg-Marquardt / differential-evolution / simulated-annealing
  solvers, cubic-spline / convex-monotone / kernel interpolation, Sobol
  / Halton / Faure low-discrepancy sequences, Brownian bridge.
- No-arbitrage SABR (Doust 2012) now in public `ql/termstructures/volatility/`,
  so smile-model research doesn't need to dip into `ql/experimental/`.
- Reverse-mode AAD via CoDi-Pack (opt-in `QL_ENABLE_AAD`) for
  sensitivity prototyping on your own scalar functions.

**What you need to build:** typically nothing — you're writing model
code against existing curves, processes, and engines.

**Effort estimate to productive use:** hours to days for model
prototyping, not months.

**Completion roadmap** *(optional — close the comprehensive-model-menu gap)*:

| Missing canonical model | Status | Effort |
|---|---|---|
| Double Heston (Christoffersen et al. 2009) | Absent | 2 weeks |
| Kou jump-diffusion | Absent (Merton is present) | 1-2 weeks |
| CGMY / tempered-stable (Carr-Geman-Madan-Yor 2002) | Absent | 2-3 weeks |
| NIG (Normal Inverse Gaussian, Barndorff-Nielsen) | Absent | 1-2 weeks |
| SSVI / eSSVI (Gatheral-Jacquier 2014) | Absent (only basic SVI exists in experimental) | 2 weeks |
| Shifted SABR as a standalone class | Currently only `unsafeShiftedSabrVolatility` free fn | 1 week |
| Rough Heston / rough volatility (Jaisson-Rosenbaum, El Euch-Rosenbaum) | Absent | 1-2 months |
| Ho-Lee as a named class | Mathematically trivial but absent | 3 days |
| LGM named class | GSR is mathematically LGM, just not branded | 1 week (rename + docs) |
| Cheyette | 2-factor HW variant | 2-3 weeks |
| Quadratic Gaussian (BG / Pelsser / Pietersz) | Absent | 2-3 weeks |

Aggregate: **3-4 engineer-months** to close the menu. Not required
for the use case to be production-ready — quant research rarely
needs the full menu.

**Key entry points:** `ql/models/`, `ql/processes/`, `ql/math/`,
`ql/termstructures/volatility/`.

---

## 2. Front-office pricing on vanilla and common exotic instruments

**Verdict:** **Production-ready.**

**What works today:**
- Vanilla European, American (BAW + Bjerksund-Stensland, both with the
  fork's correctness fixes — BAW Newton now bounded; T→0⁺ short-
  circuit; BS handles negative rates natively), Bermudan.
- Single + double barrier, Asian (discrete + continuous, geometric +
  arithmetic via Turnbull-Wakeman or MC), lookback, cliquet, chooser,
  compound, forward-start.
- Rates: caps/floors, vanilla swaps, OIS, FRAs, Bermudan swaptions
  (tree/PDE engines under Hull-White / G2++ / BK).
- Structured: autocallables (MVP, single-underlying BS MC).
- Credit: CDS analytic pricing, and CDS option promoted out of
  experimental into `ql/instruments/cdsoption.hpp`.
- Multi-asset exotics: Himalaya, Everest, Pagoda promoted into
  `ql/pricingengines/exotic/`.
- FX forwards, inflation swaps, CPI bonds, variance swaps, convertible
  bonds (binomial).

**What you need to build:** price testing / validation, trade booking,
the instrument-lifecycle layer. QuantLib is a pricing engine, not a
trading system.

**Effort estimate to productive use:** a week to a month to wire the
pricing layer into an existing booking system; hours for one-off
valuations.

**Completion roadmap** *(optional — fill the modern structured-products catalogue)*:

| Missing product | Status | Effort |
|---|---|---|
| Autocallable: worst-of / best-of (multi-underlying) | Absent; MVP is single-underlying only | 3-4 weeks |
| Autocallable: memory / snowball coupons | Absent | 2 weeks |
| Autocallable: Heston / SLV engine | Absent; MVP is BS-only | 3-4 weeks |
| Autocallable: knock-in during life (continuous monitoring) | Absent | 2-3 weeks |
| TARF (Target Accrual Redemption Forward) — FX / IR flavours | Absent | 3-4 weeks each |
| PRDC (Power Reverse Dual Currency) | Absent | 4-6 weeks |
| Accumulator | Absent | 2-3 weeks |
| Reverse convertibles | Absent (close cousin of short autocallable put) | 1-2 weeks |
| CLN (Credit-Linked Notes) | Absent | 2-3 weeks |
| CDX / iTraxx index options | Absent | 1-2 months |
| Rainbow barriers (multi-asset) | Absent | 2-3 weeks |

Aggregate: **6-9 engineer-months** for a full modern structured-
products catalogue. Not required; use case is already production-
ready for the common 80%.

**Key entry points:** `ql/instruments/`, `ql/pricingengines/`.

---

## 3. Desk-level risk and XVA

**Verdict:** **Partial — foundations shipped; aggregation still yours to build.**

**What works today (ships with the fork):**
- `ql/risk/CurveBucketer` — tenor-bucketed delta and (diagonal) gamma
  for an instrument priced off a `SimpleQuote`-driven curve. Observer-
  graph-correct, RAII-guarded, bumps at 1 bp by default.
- `ql/risk/XvaCalculator` — CVA / DVA / FVA / KVA / MVA adjustments
  from a caller-supplied EPE / ENE profile plus default curves and
  spreads. Plugs on top of either `CurveBucketer` (bump-and-reprice
  exposures) or a user-built MC forward-valuation path.
- `cvaswapengine` (upstream) for single-trade CVA on vanilla swaps.
- Greek infrastructure: rich analytic Greeks on European options;
  bump-and-reprice for American, Asian, barrier.

**What you still need to build:**
- Cross-gamma (off-diagonal) and higher-order Greeks (vomma, zomma,
  vanna beyond the European-only set).
- 2D vol-surface bucketing (maturity × strike).
- Stochastic exposure-simulation framework (path-based EPE/ENE
  generation across a netting set).
- Collateral / CSA modelling (thresholds, MTAs, independent amounts,
  two-way posting).
- Wrong-way risk.
- Netting-set and portfolio aggregation above individual trades.
- Scenario generation + historical / Monte-Carlo VaR + expected
  shortfall.
- Market-data loading orchestration (for daily risk reports).

**Effort estimate to production:** **2–4 engineer-months** down from
the 3–6 cited in the prior assessment, because `CurveBucketer` and
`XvaCalculator` remove roughly a month of primitive-building. The
remaining time is almost entirely exposure-simulation plus collateral
/ netting architecture.

**Completion roadmap** *(required to reach production)*:

| Missing primitive | Status | Effort |
|---|---|---|
| Exposure-simulation framework (`PortfolioExposureSimulator` walking a netting set through time on a stochastic-rate + stochastic-vol scenario lattice; EPE/ENE per bucket) | Absent | 1-2 months |
| Collateral / CSA modelling (`CsaAgreement`: threshold, MTA, independent amount, frequency, eligible collateral; feeds exposure sim) | Absent | 3-4 weeks |
| Wrong-way risk (correlation between counterparty default intensity and exposure; Hull-White dependence or copula) | Absent | 2-3 weeks |
| Netting aggregation (`NettingSet`: multiple trades, same counterparty, same CSA) | Absent | 2 weeks |
| 2D vol-surface bucketer (`VolSurfaceBucketer`: maturity × strike, matching `CurveBucketer` pattern) | Absent | 1-2 weeks |
| Cross-gamma + higher-order Greeks (vomma, zomma, vanna for non-European) | Partial: European has them; exotics via bump-and-reprice | 2-3 weeks |
| Scenario generation (scenario DSL, curve/surface application, aggregation) | Absent | 3-4 weeks |
| VaR / Expected Shortfall (historical sim, MC VaR, parametric VaR, 10-day scaling) | Absent | 3-4 weeks |
| Portfolio-level consolidated pricing (common numeraire, cross-trade consistency) | Absent | 2-3 weeks |

**Parallelism:** ~4-6 months of serial work; compresses to **2-4
months with two engineers working in parallel** on independent
sub-components.

**Key entry points:** `ql/risk/CurveBucketer`, `ql/risk/XvaCalculator`,
`ql/pricingengines/swap/cvaswapengine.hpp`.

---

## 4. Bank-wide regulatory capital (FRTB-SA, FRTB-IMA, SIMM, SA-CCR)

**Verdict:** **Partial MVP for FRTB-SA GIRR delta; the rest is out of scope.**

**What works today:**
- `ql/risk/FrtbSaGirrDelta` — the BCBS d457 aggregation math for the
  General Interest Rate Risk delta bucket charge:
  `K_b = sqrt(sum_i WS_i² + 2·sum_{i<j} ρ_{ij}·WS_i·WS_j)` with the
  standard `max(exp(−θ|dT|/min(T_i,T_j)), φ_floor)` correlation. Risk
  weights and `(θ, φ_floor)` are caller-supplied so you match your
  jurisdiction's current BCBS / CRR / PRA / FCA parameter set.
- Composes directly with `CurveBucketer::bucketedDelta()` output.

**What you still need to build:**
- **FRTB-SA:** vega + curvature charges; Credit-Spread Risk
  (non-securitisations, securitisations, correlation trading
  portfolio); Equity, Commodity, FX risk classes; cross-bucket γ_bc
  aggregation; diversified vs undiversified charge selection; default
  liquidity-horizon scaling; Default Risk Charge.
- **FRTB-IMA:** Expected Shortfall under stressed-period calibration,
  Non-Modellable Risk Factors, P&L-attribution test.
- **SIMM:** all risk classes per ISDA SIMM Methodology (currently
  SIMM v2.6) — IR, FX, credit qualifying / non-qualifying, equity,
  commodity — plus the correlation / aggregation rules and the
  ISDA-licensed parameter updates.
- **SA-CCR:** PFE add-on formulae, supervisory duration, hedging-set
  aggregation, cross-asset aggregation.

**Effort estimate (revised):** earlier versions of this file cited
4-8 engineer-months for "FRTB-SA + SIMM + SA-CCR." That was
optimistic. Honest breakdown below: **12-20 engineer-months for all
four regulatory frameworks at production quality.** FRTB-SA + SIMM +
SA-CCR without FRTB-IMA is **9-14 months** — still the closest thing
to the prior estimate's scope, but substantially larger than "4-8
months."

**Completion roadmap — FRTB-SA (Standardised Approach, BCBS d457):**

| Component | Status | Effort |
|---|---|---|
| GIRR delta | ✅ `FrtbSaGirrDelta` shipped | Done |
| GIRR vega | Absent | 3-4 weeks |
| GIRR curvature | Absent | 2-3 weeks |
| CSR non-securitisations (Rating × Sector × Seniority × Currency buckets) | Absent | 1-2 months |
| CSR securitisations (CTP + non-CTP) | Absent | 1 month |
| Equity (13-bucket scheme) | Absent | 3-4 weeks |
| Commodity (11-bucket scheme) | Absent | 3-4 weeks |
| FX (per-currency buckets) | Absent | 2-3 weeks |
| Cross-bucket γ_bc aggregation framework (shared across risk classes) | Absent | 2-3 weeks |
| Default Risk Charge (jump-to-default, non-sec + sec) | Absent | 1-2 months |
| **Aggregate FRTB-SA** | | **6-10 months** |

**Completion roadmap — FRTB-IMA (Internal Models):**

| Component | Status | Effort |
|---|---|---|
| Expected Shortfall at 97.5% | Absent | 3-4 weeks |
| 10-day + longer liquidity-horizon scaling | Absent | 2-3 weeks |
| Non-Modellable Risk Factors (NMRF) SES | Absent | 1-2 months |
| P&L attribution test (per-desk) | Absent | 3-4 weeks |
| Backtesting framework | Absent | 2-3 weeks |
| **Aggregate FRTB-IMA** | | **3-6 months** |

**Completion roadmap — SIMM v2.6 (ISDA IM Model):**

| Component | Status | Effort |
|---|---|---|
| IR risk class + correlation matrix | Absent | 3-4 weeks |
| FX risk class | Absent | 2 weeks |
| Credit Qualifying + Non-Qualifying | Absent | 4-5 weeks |
| Equity risk class | Absent | 2-3 weeks |
| Commodity risk class | Absent | 2-3 weeks |
| ISDA parameter-file loader + quarterly update cadence | Absent | 2 weeks |
| **Aggregate SIMM** | | **2-3 months** |

**Completion roadmap — SA-CCR (Basel Counterparty Credit Risk):**

| Component | Status | Effort |
|---|---|---|
| Replacement cost + PFE multiplier | Absent | 1-2 weeks |
| IR add-on + supervisory duration | Absent | 2-3 weeks |
| FX / Credit / Equity / Commodity add-ons | Absent | 3-4 weeks |
| Hedging-set + asset-class aggregation | Absent | 2-3 weeks |
| **Aggregate SA-CCR** | | **1-2 months** |

**Not what this fork is for:** a bank running Basel III capital
reporting end-to-end today. Use a dedicated vendor library (Numerix,
BlackRock Solutions, RiskMetrics) or commit to the 12-20-month build
on top of QuantLib's primitives.

**Key entry points:** `ql/risk/FrtbSaGirrDelta`, `ql/risk/CurveBucketer`.

---

## 5. High-frequency / low-latency

**Verdict:** **Not a target use case. Acceptable for EOD; intraday wants profiling; HFT wants something else.**

**Latency profile:**
- QuantLib is **header-heavy and template-heavy**. Compile times are
  significant; binary sizes are large; inlining choices cross
  translation-unit boundaries.
- The observer graph (`Observable` / `LazyObject`) incurs per-update
  cache-invalidation dispatches that are cheap individually but add up
  under intraday re-quote churn. `QL_FASTER_LAZY_OBJECTS=ON` (default)
  helps.
- Pricing kernels are written for numerical clarity, not cache-locality
  or SIMD. Hot-loop profiling on a Heston MC run will not show you the
  same access patterns as a hand-tuned quant kernel.

**What works today:**
- End-of-day batch pricing of book-sized portfolios is fine on modern
  hardware — tens of thousands of trades in minutes.
- Intraday P&L / risk recomputes are workable if you (a) keep the
  observer graph clean, (b) reuse engines / processes rather than
  reconstructing per-quote, (c) profile the specific hot loops and
  pool objects where warranted.

**What you should not expect:**
- Sub-millisecond option pricing in a tight quote-streaming loop.
  QuantLib isn't built for that; a hand-written closed-form or a
  SIMD-vectorised pricer is the right tool.
- Lock-free multi-threaded hot paths. `QL_ENABLE_THREAD_SAFE_OBSERVER_PATTERN`
  uses `recursive_mutex` + proxy-pointer deactivation — correct but
  not optimised for contention.

**What you need to build for intraday use:**
- Hot-path profiling (perf + flamegraphs) on your specific trade
  population. Engines that look cheap in aggregate can have
  surprising tails.
- Object pools / arenas for `ext::shared_ptr` allocations that
  dominate some repriced-per-quote loops.
- A thin pricing-wrapper layer that reuses engine + process objects
  across quote updates rather than building fresh ones.
- For genuinely low-latency paths: accept that QuantLib is the slow
  path (model calibration, overnight rebuilds) and write a separate
  fast path for the runtime-critical subset.

**Effort estimate:** weeks to months of performance engineering per
hot-loop use case.

**Completion roadmap** *(if you truly want QuantLib intraday-capable,
not HFT)*:

| Intervention | Status | Effort |
|---|---|---|
| Object pools / arena allocators for `ext::shared_ptr` churn (hot repriced-per-quote loops) | Absent | 2-3 weeks |
| Engine + process reuse patterns + benchmark suite identifying redundant copies | Absent | 2-4 weeks |
| Hand-written fast-path pricer for top-N option types (SIMD where applicable); run alongside QuantLib, not replacing it | Absent | 2-3 months |
| Lock-free observer graph in thread-safe mode (replace `recursive_mutex` with atomics + flags) | Absent; **high risk of destabilisation** | 1-2 months |

**Aggregate:** **3-6 engineer-months** to make QuantLib intraday-
capable. Even then, not sub-microsecond. Honest recommendation: keep
QuantLib for calibration / overnight / risk; write the runtime-
critical hot path separately against a hand-tuned Black or Heston
FFT pricer.

**Key entry points:** the profiling tools, not specific QuantLib
headers.

---

## Summary matrix

| Use case | Verdict | Rough effort to production | Optional "fully complete" effort |
|---|---|---|---|
| Quant model prototyping | Production-ready | Hours – days | 3-4 months to close canonical-model-menu gap |
| Vanilla / common-exotic pricing | Production-ready | Week – month (wiring) | 6-9 months for full structured-products catalogue |
| Desk-level risk + XVA | Partial foundations | 2-4 engineer-months parallel / 4-6 serial | Same — this is already the "complete" number |
| Bank-wide regulatory capital | FRTB-SA GIRR delta MVP | FRTB-SA + SIMM + SA-CCR: 9-14 months | All four frameworks (+FRTB-IMA): 12-20 months |
| HFT / low-latency | Not a target | Build a separate fast path | 3-6 months of performance engineering to make QuantLib intraday-capable |

## Updating this file

This use-case guide is meant to stay honest. When a fork commit
materially changes any of the five assessments (a new primitive
reduces the effort estimate, a new module closes part of a gap),
update the relevant section in the same PR. The rule of thumb: if
`FORK_CHANGES.md`'s "New classes" or "Promotions" table grows, check
whether one of the five use-case verdicts should move.
