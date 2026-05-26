# Roadmap

Two parts: the **commitment** (Phase 0–4, ~4 months, ~10–15 hr/week) and
the **roadmap-only** stretch (Phase 5–10, not promised, re-evaluated at
Month 4).

Source citations per phase use the IDs from [sources.md](sources.md). The
four-part series (I–IV) is primary for Phases 0–7; IRC is primary for
Phases 8–10.

---

## Current scope — Phase 0–4 (committed)

### Phase 0 — Environment (Week 1)

**Sources:** II §3.3.1 (SOFR curve calibration instruments), II §3.2.2 (IRS).

**Goal:** clean build from a fresh clone produces a working SOFR OIS swap
example.

Notes on the SOFR pivot: the original plan targeted a LIBOR vanilla IRS for
the hello-world. We pivoted to a **SOFR OIS swap** because USD LIBOR is dead
(2023) and Source II builds the entire curve framework around SOFR. The
SOFR floating leg is daily-compounded in arrears — slightly fiddlier than
LIBOR but more relevant. Keep any pre-existing LIBOR sanity check as
scaffolding; the canonical Phase 0 deliverable is SOFR.

Tasks:

- Install Visual Studio 2022 (C++ workload), CMake, vcpkg, clang-format,
  clang-tidy.
- Set `VCPKG_ROOT`, run `vcpkg integrate install` once.
- Create `vcpkg.json` manifest with `quantlib`, `eigen3`, `gtest`.
- Minimal `CMakeLists.txt`. One example target. One test target.
- `examples/01_sofr_hello_swap.cpp`: build a flat SOFR curve, price one
  USD SOFR OIS swap using QuantLib's `OvernightIndexedSwap`, print NPV
  and fair rate.
- README "How to build" section actually works.

Deliverable: `cmake --build && ctest` both succeed, `01_sofr_hello_swap`
prints a sensible NPV.

Milestone: tag `v0.1-env`.

---

### Phase 1 — Hand-rolled mini pricer (Week 2)

**Sources:** II §3.2.2 (IRS), II §6.1 (day count), II §2.2 (compound interest).

**Goal:** understand what QuantLib does by writing a simplified SOFR-aware
version.

Math note: `docs/math_notes/01_swap_valuation.md` — owner-written. Covers
swap PV as sum of fixed/floating leg PVs, annuity, fair rate, par
condition. For floating leg, cover both an IBOR-style simplification
(simple rate over period) and SOFR-style (daily-compounded geometric
average) — see Source II §2.1–2.2.

Modules:

- `src/core/yield_curve.hpp` — abstract base, flat curve implementation.
- `src/rates/fixed_leg.hpp`, `floating_leg.hpp`, `vanilla_swap.hpp` —
  minimal hand-rolled pricer. Floating leg supports both simple-rate and
  daily-compounded modes via a strategy parameter.

Tests:

- Par swap NPV ≈ 0 at fair rate.
- Leg sign sanity (payer vs receiver).
- Fair rate sanity (positive, in reasonable range).
- Same trade priced by own engine vs QuantLib → diff < 1e-6.

Milestone: tag `v0.2-mini-pricer`.

---

### Phase 2 — Curve bootstrapping + DV01 (Week 3)

**Sources:** II §3.3 (multi-curve construction), II §3.3.1 (SOFR curve calibration set), II §3.4 (Jacobian risk representation).

**Goal:** build a real SOFR curve and compute first-order risk.

Math note: `docs/math_notes/02_curve_bootstrapping.md`.

Modules:

- `examples/02_quantlib_sofr_curve_bootstrap.cpp` — use SOFR futures
  helpers + OIS swap helpers → `PiecewiseYieldCurve`. Calibration set
  modeled on Source II Table 1 (SR3H26 ... SR3Z28 futures + 4Y/5Y/7Y/
  10Y/12Y/15Y/20Y/25Y/30Y OIS swaps).
- `src/curves/piecewise_zero_curve.hpp` — own simplified piecewise
  log-linear curve.
- DV01 by bump-and-reprice.

Tests:

- Reprice input swaps with bootstrapped curve → repricing error < 1e-8.
- Bump 1bp → DV01 has correct sign and rough magnitude.
- Own curve vs QuantLib curve discount factors → diff small.

Stretch: implement the Jacobian-based risk representation from Source II
§3.4 (project model-IFR sensitivities onto market-quote sensitivities).
This is the proper hedging math but is optional for the MVP.

Milestone: tag `v0.3-curve-dv01`.

---

### Phase 3 — (deferred / merged into Phase 4)

The original 18-week plan had Phase 3 as a separate "advanced curve
construction" block including B-spline + Tikhonov regularization. That's
research-grade work and stays optional. Skip in v1.

---

### Phase 4 — Portfolio risk report (Week 4)

**Sources:** II §4.2 (PCA hedging), II §4.3 (eigen-scenarios with worked numerical example), II §5 (DV01-neutral steepener).

**Goal:** the MVP. Read a portfolio, write four CSVs including PCA-based
eigen-scenarios alongside the standard parallel/steepener/flattener shocks.

Math note: `docs/math_notes/03_dv01_krd_scenarios.md`. Cover DV01, KRD
decomposition, parallel/steepener/flattener shock semantics, and the PCA
construction from Source II §4 (eigen-decomposition of curve-change
covariance, eigen-scenario shocks scaled by √λ).

Modules:

- `src/risk/dv01.hpp`
- `src/risk/key_rate_duration.hpp`
- `src/risk/scenario_engine.hpp` — supports both deterministic shocks
  (parallel, steepener, flattener) and PCA-derived eigen-scenarios.
- `src/risk/curve_pca.hpp` — PCA on a synthetic historical curve series
  (we don't have real market history, so generate plausible series or
  use a small bundled CSV).
- `examples/04_swap_portfolio_risk.cpp` — reads
  `data/trades/swaps_sample.csv`, writes the four MVP CSVs.

Tests:

- KRD at 2Y, 5Y, 10Y key tenors. Sum of KRDs ≈ DV01 (within
  interpolation error).
- Scenario: parallel ±25bp, steepener (long-end up, short-end down),
  flattener (reverse). Sign of P&L matches DV01 expectation.
- PCA: top 3 eigenvalues explain >90% of variance on the synthetic
  series. PC1 shape is roughly flat (level), PC2 is monotone (slope),
  PC3 is hump-shaped (curvature).
- Reproducibility: same input CSV → same output CSV byte-for-byte.

Deliverable: the four MVP CSVs listed in README. The `scenario_pnl.csv`
now contains both deterministic and PCA-based eigen-scenarios in
separate rows tagged by scenario type.

Milestone: tag `v1.0-mvp`. **This is the contract.**

---

### Phase 4.5 — xVA awareness (optional, only if Phase 4 ships early)

**Sources:** I §2 (CSA replication argument), I §3.1 (xVA decomposition), I §3.2 (CVA integral with EE/PD/LGD).

**Goal:** demonstrate the modern collateralized-pricing framework on the
existing swap portfolio. Not required for the MVP — listed because Source I
provides the framework cleanly enough that this is achievable in 1 week if
Phase 4 finishes ahead of schedule.

Modules:

- `src/risk/cva.hpp` — flat-hazard CVA approximation using
  EE = (annuity × σ × √MPoR) / √(2π) under a Gaussian MTM proxy
  (Source I §3.3).
- `examples/045_swap_cva.cpp` — compute clean PV + simple CVA for each
  swap in the portfolio, output to `output/cva_report.csv`.

Tests:

- CVA > 0 for swaps with positive expected exposure.
- CVA scales roughly linearly with hazard rate at small rates.

Milestone: tag `v1.0.1-xva` if delivered. Otherwise defer to Phase 10.

---

## Month 2–4 — extensions if MVP ships on time

### Phase 5 — CDS + survival curve (Month 2)

**Sources:** I §3.2 (CVA framework + survival probability), IRC Ch.2–3 (CDS depth).

Add `src/credit/`. Hazard rate, survival probability, risky discount
factor, CDS premium leg, CDS protection leg, par spread, bootstrap
hazard curve from CDS quotes, repricing diagnostics, QuantLib CDS engine
comparison. Source I's CVA framework is the same math at the integrand
level — reuse the survival-probability machinery.

Milestone: tag `v1.1-cds`.

### Phase 6 — Hull–White short-rate (Month 3, first half)

**Sources:** IRC Ch.10. (Not covered by the four-part series.)

Add `src/models/`. Vasicek and Hull–White dynamics, trinomial tree, ZCB
price matches analytic, calibration to initial curve, comparison with
QuantLib `HullWhite`.

Milestone: tag `v1.2-hw`.

### Phase 7 — Vol modeling: SABR + RFR caplet (Month 3, second half)

**Sources:** III §4.3–4.8 (SABR, Hagan formula, ATM parameterization, smile-risk Greeks, Bartlett's delta), IV §3 (time-decay SABR for backward-looking RFR caplets).

Black caplet formula, cap/floor pricer, Black swaption pricer, SABR
implied vol, SABR calibration to swaption smile. Then add **time-decay
SABR** per Source IV §3 and price a **backward-looking RFR caplet** —
this is the modern OTC standard and the differentiator vs a vanilla
Black/SABR exercise.

Port the SABR calibration from prior FX-SABR work — watch convention
differences (rates use ATM-vol parameterization per III §4.5; FX uses
delta).

Milestone: tag `v1.3-sabr-rfr`.

### Phase 8 — Decision point (Month 4)

- Polish tests, get >70% coverage on `src/`.
- Write a 2-page project summary suitable for showing 董老師 / résumé.
- **Ask 董老師:** USD continuation, or pivot to CNY? Confirm specific
  CNY rate conventions (FR007 / SHIBOR / LPR) before starting any
  Month 5 work — they're not drop-in replacements for SOFR.
- Re-evaluate the rest of the roadmap.

---

## Roadmap-only (not committed)

These are written down so the long-term vision is visible. Do not start
them without re-evaluating at Month 4. Time estimates are honest, not
optimistic.

### LMM Monte Carlo — ≥6 weeks

**Sources:** IRC Ch.11. (Not covered by the four-part series.)

LIBOR Market Model is famously where self-implementations die. Drift
calculation under different measures, calibration to caplet vols,
correlation parametrization — none of it is fast. Budget 6–8 weeks if
pursued.

When doing this, **use QuantLib's MC infrastructure** (PathGenerator,
RNG, Brownian bridge). Hand-roll only the model logic.

### LSM Bermudan — 3–4 weeks, but only after Hull–White

**Sources:** IRC Ch.12 (optimal stopping, Bermudan-specific depth), I §3.4 (MC regression framework — basis functions + least-squares).

The right path is:

1. Implement Longstaff–Schwartz regression on **Hull–White paths first**.
   Hull–White has analytic European swaption prices, so the regression
   logic can be validated independently. Source I §3.4 gives the
   regression scaffold cleanly; IRC Ch.12 gives the optimal-stopping
   wrapping.
2. Only after that works, swap in LMM paths.

Do not implement LSM Bermudan on top of an unvalidated LMM. That couples
two debugging surfaces and is the most likely place for the project to
stall.

### CCR / Exposure / CVA — scope tightly

**Sources:** IRC Ch.15 (CCR depth), I §3.2–3.3 (EE construction, MPoR, nested-MC critique → regression-based exposure).

Realistic v1 scope: **single-trade IRS exposure under Hull–White**, with
EE / EPE / PFE quantiles and a flat-hazard CVA approximation.

Out of scope for v1: portfolio netting, collateral (threshold/MTA),
margin period of risk (beyond a constant), wrong-way risk, ORE-style
XML workflows.

### ORE integration — only when ready

Open Source Risk Engine is large. Don't open the source until Phase 0–7
is solid. Then read examples first, source second.

---

## Possible CNY pivot (decided at Month 4 with 董老師)

If the answer at Month 4 is "pivot to CNY":

- Curves: FR007 IRS, SHIBOR IRS, LPR-linked swap, CNY discount curve,
  repo/funding curve.
- Conventions are **not drop-in replacements** for SOFR. Confirm
  specific conventions with him before building.
- The pricing engine and risk report code should be largely reusable;
  the conventions and market data layer are what change.

---

## What this roadmap is not

- Not a Gantt chart. Dates slip; the order matters more than the dates.
- Not a backlog of features for users. There are no users.
- Not a substitute for the math notes. The math notes are where
  understanding lives; this file is just the order in which to write
  them.
