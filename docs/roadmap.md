# Roadmap

Two parts: the **commitment** (Phase 0–4, ~4 months, ~10–15 hr/week) and
the **roadmap-only** stretch (Phase 5–10, not promised, re-evaluated at
Month 4). Within the committed range, Phase 4a (xCcy / foreign collateral)
and Phase 4.5 (xVA) are optional — they do not gate the v1.0 MVP.

Source citations per phase use the IDs from [sources.md](sources.md). The
four-part series (I–IV) is primary for Phases 0–7; IRC is primary for
Phases 8–10.

**Code architecture:** the math sources say *what* to compute; **CPP**
(Joshi, *C++ Design Patterns and Derivatives Pricing*) says *how* to
structure the C++. It's cross-cutting, not tied to one phase — port the
patterns, modernize the pre-C++11 idioms. Pointers are noted inline below;
full map in [sources.md](sources.md#where-cpp-applies-code-architecture-cross-cutting).

**Testing expectation (stated up front so it isn't a Month-4 scramble):**
every phase ships with its own validation tests — they are never deferred.
The target is **>70% line coverage on `src/`, tracked continuously from
Phase 2 onward**, not measured for the first time at Phase 8. Phase 8's job
is to *confirm* the target has held and backfill residual gaps, not to
establish it.

## Prior art to port from QuantBricker

Reference: [Tiffany-YQY/QuantBricker](https://github.com/Tiffany-YQY/QuantBricker)
(Python, Phases 0–7 scope, no docs/tests). Patterns worth stealing:

| Pattern | Where | Notes |
|---|---|---|
| **Jacobian model→market risk propagation** (`model_jacobian`, `risk_postprocess`, per-component `..._gradient_wrt_state`) | **Phase 2 — promote from stretch to deliverable** | Her cleanest idea. Every component exposes analytic gradient w.r.t. its state; the model composes them into a Jacobian, then post-multiplies gradients into market-quote space (Source II §3.4). Match this. |
| **Registry / plugin architecture** (`ModelDeserializerRegistry`, `IndexRegistry`, `ProductBuilderRegistry`) | Phase 2 onward | Self-registering pricers, curves, products. CPP Ch.10, 14 (factory). Cleaner than a big switch. |
| **Vol model as decorator over curve** (`SABRModel(YieldCurve)` with `sub_model_`) | Phase 7 | Curve ops delegate to sub-model; SABR only adds vol. Right composition for vol-on-top-of-rates. |
| **Serialize/deserialize protocol on every model** (versioned dict, `deserialize` via registry) | Phase 2+ | Cheap state persistence + regression fixtures. |
| **Instrument vocabulary** (`RFRSwap`, `RFRFuture`, `OvernightIndexBasisSwap`, `CrossCurrencyBasisSwapNonMTM`, `FundingIdentifier`) | Phase 2+ naming | Steal the names; matches Source II conventions. |

**Do not** copy her tests (Jupyter notebooks, no assertions),
`import *` style, 1900-line god-modules, or the lack of any README/docstrings.
Match her scope, beat her hygiene.

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

Math note: `docs/math_notes/01_sofr_swap.md` — owner-written. Covers
swap PV as sum of fixed/floating leg PVs, annuity, fair rate, par
condition. For floating leg, cover both an IBOR-style simplification
(simple rate over period) and SOFR-style (daily-compounded geometric
average) — see Source II §2.1–2.2.

Modules:

- `src/core/yield_curve.hpp` — abstract base, flat curve implementation.
- `src/rates/fixed_leg.hpp`, `floating_leg.hpp`, `vanilla_swap.hpp` —
  minimal hand-rolled pricer. Floating leg supports both simple-rate and
  daily-compounded modes via a strategy parameter (Strategy pattern —
  CPP Ch.5; open–closed `yield_curve` hierarchy — CPP Ch.2–3).

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

### Phase 4a — xCcy basis + foreign-collateralized discounting (optional, post-MVP)

**Sources:** II §3.2.3 (basis swap), II §3.2.4 (xCcy basis swap, non-MTM),
II §3.2.5 (FX forward and currency chain), II §3.3.2 (forward curve via
basis), II §3.3.3 (foreign-collateralized discount curves), I §2
(collateralized-pricing foundation — same framework as math note 01's
appendix, now with collateral currency ≠ cash-flow currency).

**Status: does NOT gate the v1.0 MVP.** Printed here for topic affinity
with Phase 2 (it's the multi-currency extension of the same bootstrap), but
**labeled 4a to make the execution order explicit**: it runs after Phase 4
ships — realistic slot is Month 2, alongside or instead of Phase 5, ~1–2
weeks. The earlier B-spline + Tikhonov regularized curve-fitting plan stays
research-grade and out of scope entirely.

**Goal:** extend the Phase 2 single-currency framework to the multi-currency
collateral-aware setting of Source II §3.3 — build the discount curve for
EUR cash flows collateralized in USD, i.e. $P^{€,\$}(0,\cdot)$ per II
Eq. (49)–(50).

Math note: `docs/math_notes/04_foreign_collateral_curves.md` — owner-written,
covering: covered interest parity and the FX-forward currency chain (§3.2.5),
the non-MTM xCcy basis swap's cash flows (§3.2.4), why USD collateral on a
EUR leg forces a funding-spread-adjusted discount curve, and the IFR
parameterization $\varphi^{€,\$}$ of Eq. (50).

Prerequisites (from Phase 2, reused not rebuilt):

- The single-currency bootstrap machinery, run **twice**: USD SOFR curve
  (already built) + a EUR OIS curve (ESTR — Source II's notation says
  EONIA; treat as the EUR RFR discount curve).
- Synthetic market data modeled on Source II Table 4
  (EURIBOR-3M/SOFR-3M basis swaps, USD-collateralized) +
  spot FX + FX forwards, bundled as `data/market/xccy_sample.csv`
  (same precedent as Phase 4's synthetic curve history).

Modules:

- `src/instruments/xccy_basis_swap.hpp` — non-MTM xCcy basis swap
  (name per QuantBricker's `CrossCurrencyBasisSwapNonMTM` vocabulary; MTM
  reset variant explicitly out of scope for v1).
- `src/curves/fx_forward_chain.hpp` — spot + forward points → implied
  FX forwards; covered-interest-parity utilities (II §3.2.5).
- `src/curves/foreign_collateral_curve.hpp` — bootstrap
  $P^{€,\$}(0,\cdot)$ from xCcy basis quotes given the two domestic
  curves + EURIBOR forwards (II §3.3.3, Eq. 49); interpolate in IFR
  space per Eq. (50), consistent with the Phase 2 curve's interpolation.
- `examples/03_xccy_collateral_bootstrap.cpp` — end-to-end: read
  `xccy_sample.csv`, build all three curves, write
  `output/xccy_curves.csv` (USD-SOFR, EUR-ESTR, EUR-under-USD-collateral
  discount factors side by side).

Tests:

- Reprice the calibrating xCcy basis swaps with the bootstrapped
  $P^{€,\$}$ → repricing error < 1e-8 (same standard as Phase 2).
- Covered interest parity: FX forward implied by the curve pair matches
  the input FX forwards.
- Sign sanity: negative EURIBOR/SOFR basis ⇒ $P^{€,\$}$ sits below /
  above $P^{€,€}$ in the direction the math note derives — write the
  expected direction down in the note *before* coding.
- Degenerate case: zero basis + flat identical curves ⇒
  $P^{€,\$} = P^{€,€}$ to machine precision.
- Benchmark vs QuantLib where possible: `FxSwapRateHelper` +
  `CrossCurrencyBasisSwapRateHelper` — note the latter lives in
  `ql/experimental/`, so treat it as a sanity check, not gospel.

Milestone: tag `v1.0.2-xccy` (post-MVP optional series, alongside
`v1.0.1-xva`).

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

**Sources:** I §3.2 (CVA framework + survival probability), IRC Ch.2–3 (CDS depth). Complement: BM Ch.21–22 (counterparty risk + intensity/hazard-rate models for the survival curve).

Add `src/credit/`. Hazard rate, survival probability, risky discount
factor, CDS premium leg, CDS protection leg, par spread, bootstrap
hazard curve from CDS quotes, repricing diagnostics, QuantLib CDS engine
comparison. Source I's CVA framework is the same math at the integrand
level — reuse the survival-probability machinery.

Milestone: tag `v1.1-cds`.

### Phase 6 — Hull–White short-rate (Month 3, first half)

**Sources:** IRC Ch.10. (Not covered by the four-part series.) Complement:
BM Ch.3 (one-factor short-rate models — Vasicek, CIR, Hull–White extended
Vasicek, the CIR++ deterministic-shift fit to the initial curve), BM Ch.4
(two-factor / G2++ if a second factor is ever added).

Add `src/models/`. Vasicek and Hull–White dynamics, trinomial tree, ZCB
price matches analytic, calibration to initial curve, comparison with
QuantLib `HullWhite`. (Tree class design — CPP Ch.8.)

Milestone: tag `v1.2-hw`.

### Phase 7 — Vol modeling: SABR + backward-looking RFR caplet (Month 3, second half)

**Sources:** III §4.3–4.4 (SABR specialization, Hagan semi-analytic
implied-vol formula, smile dynamics), III §4.5 (alternative / ATM-vol
parameterization), III §4.6 (managing smile risk — the SABR Greeks),
III §4.8 (Bartlett's modified delta / modified alpha), IV §3 (time-decay
factor, time-decay SABR, backward-looking RFR caplet workflow), IV §3.4
(forward- vs backward-looking parameters), IV §4.1.2 (the 1/3 rule).
Complement: BM Ch.1 (Black's cap/floor and swaption formulas and the
Bachelier/normal analogue), BM Ch.11 (SABR as a stochastic-vol extension
of the market model), IRC Ch.8–9 (caplet/swaption pricing and calibration
depth). Code architecture: CPP Ch.9 (solver as a function object +
Newton–Raphson), CPP Ch.5 (Strategy — swap the vol model behind one
pricing interface).

**Goal:** price the standard rates vanilla-vol products — caps/floors and
swaptions — under both a flat vol and a calibrated SABR smile, then extend
to the modern OTC standard: a **backward-looking (compounded) RFR caplet**
under **time-decay SABR**. This is the phase's differentiator — a plain
Black/SABR cap pricer is a textbook exercise, whereas a time-decay-SABR RFR
caplet is current market practice (Source IV) and is not exposed by
QuantLib out of the box.

Convention note: SOFR-era rates vol is quoted **normal (Bachelier)**, not
lognormal, because rates can go negative. Build the Bachelier caplet /
swaption as the *primary* formula and keep Black-76 (lognormal) only as the
comparison/legacy path — do **not** default to Black. Source III's baseline
model is Bachelier for exactly this reason.

Math note: `docs/math_notes/07_sabr_rfr_caplet.md` — owner-written. Cover:
(1) Bachelier vs Black-76 caplet/floorlet and the implied-vol inversion;
(2) cap = strip of caplets, swaption on the annuity-measure forward swap
rate (reuse the Phase 2 annuity); (3) the SABR SDE in (F, α, β, ρ, ν) and
the Hagan asymptotic implied-vol formula, including its ATM special case
(III §4.5); (4) the SABR Greeks and why raw delta is unstable under a
smile re-mark, leading to **Bartlett's delta** — the α-adjusted delta that
accounts for the ρ-correlation between F and α (III §4.8); (5) the RFR
complication — the fixing is a compounded average over the accrual period,
so vol decays as fixings accrue: introduce the **time-decay factor** and
**time-decay SABR** (IV §3) and the backward- vs forward-looking parameter
distinction (IV §3.4). Write the expected sign/limit behavior down *before*
coding (same discipline as Phase 4a).

Modules:

- `src/vol/bachelier_black.hpp` — Bachelier (normal) and Black-76
  (lognormal) caplet/floorlet + implied-vol inversion. Bachelier is the
  primary path.
- `src/instruments/cap_floor.hpp` — cap/floor as a strip of
  caplets/floorlets over a schedule.
- `src/instruments/swaption.hpp` — European swaption on the forward swap
  rate, priced off the Phase 2 annuity.
- `src/vol/sabr.hpp` — SABR implied vol (Hagan formula) with (α, β, ρ, ν);
  ATM-vol parameterization (III §4.5) selectable via a parameter.
- `src/vol/sabr_calibration.hpp` — calibrate (α, ρ, ν) to a market smile at
  fixed β; solver via function objects + Newton–Raphson /
  Levenberg–Marquardt (CPP Ch.9).
- `src/vol/sabr_greeks.hpp` — SABR delta/vega plus **Bartlett's modified
  delta / modified alpha** (III §4.8).
- `src/vol/rfr_caplet.hpp` — time-decay factor + time-decay SABR (IV §3);
  price a backward-looking compounded-RFR caplet.
- `src/vol/vol_surface.hpp` — SABR-per-expiry **decorator over the Phase 2
  curve** (QuantBricker's `SABRModel(YieldCurve)` pattern: curve ops
  delegate to the sub-model, SABR only adds vol — CPP Ch.5).
- `examples/07_sabr_swaption_smile.cpp` — calibrate SABR to a synthetic
  swaption smile, write `output/sabr_smile.csv` (strike, market vol, SABR
  vol, error).
- `examples/07_rfr_caplet.cpp` — price a backward-looking RFR caplet under
  time-decay SABR; print premium and Bartlett delta.

Tests:

- Bachelier and Black caplet each match QuantLib (`BachelierCapFloorEngine`
  / `BlackCapFloorEngine`) → diff < 1e-8.
- Implied-vol inversion round-trips: price → implied vol → price to machine
  precision.
- Put–call parity: caplet − floorlet = discounted (forward − strike);
  cap − floor = par swap value.
- SABR Hagan vol matches QuantLib `sabrVolatility` across a strike grid →
  diff < 1e-6; the ATM limit matches the closed-form ATM expression
  (III §4.5).
- SABR calibration round-trip: generate a smile from known (α, ρ, ν),
  calibrate back, recover the params within solver tol.
- Swaption via Bachelier matches QuantLib's normal-vol swaption engine →
  diff < 1e-8.
- Backward-looking RFR caplet: as the time-decay factor → 1 (all fixings
  still in the future, i.e. at trade inception) the price collapses to the
  forward-looking caplet (IV §3.4); mid-accrual variance follows the **1/3
  rule** from the time-decay model (IV §4.1.2).
- Bartlett's delta (III §4.8): under a joint (F, α) re-mark it is more
  stable than raw SABR delta — verify it differs from raw delta by the
  expected ρν term and reduces re-hedging P&L noise on a bumped smile.
- Reproducibility: same input smile CSV → same calibrated params and same
  output CSV.

**Prior-work note:** port the SABR calibration from prior FX-SABR work, but
watch the convention differences — rates use the ATM-vol parameterization
(III §4.5) and **normal** vol; FX uses delta-based strikes and
lognormal/premium-adjusted delta. The solver structure ports cleanly; the
strike axis and vol convention do not.

Milestone: tag `v1.3-sabr-rfr`.

### Phase 8 — Decision point (Month 4)

- Polish tests; confirm the >70% coverage target on `src/` has held
  (tracked since Phase 2) and backfill any residual gaps.
- Write a 2-page project summary suitable for résumé use or for sharing
  when consulting others for feedback.
- **Decide** USD continuation vs. pivot to CNY — worth consulting others
  for guidance here. Confirm specific CNY rate conventions (FR007 / SHIBOR
  / LPR) before starting any Month 5 work — they're not drop-in
  replacements for SOFR.
- Re-evaluate the rest of the roadmap.

---

## Roadmap-only (not committed)

These are written down so the long-term vision is visible. Do not start
them without re-evaluating at Month 4. Time estimates are honest, not
optimistic.

### LMM Monte Carlo — ≥6 weeks

**Sources:** IRC Ch.11 (LMM depth). Complement: BM Ch.6 (the LIBOR/swap
market models, LFM/LSM, and the drift under different measures), BM Ch.7
(calibration to caplet/swaption vols and correlation parameterization),
BM Ch.8 (Monte Carlo tests of the LFM analytical approximations). MC
methodology: GLA (see below). Not covered by the four-part series.

LIBOR Market Model is famously where self-implementations die. Drift
calculation under different measures, calibration to caplet vols,
correlation parametrization — none of it is fast. Budget 6–8 weeks if
pursued.

When doing this, **use QuantLib's MC infrastructure** (PathGenerator,
RNG, Brownian bridge). Hand-roll only the model logic. Two references,
different layers: for the *numerical methodology*, **GLA** is canonical —
RNG and normal-vector generation (Ch.2), Brownian path / bridge
construction (Ch.3), Euler / second-order SDE discretization (Ch.6),
variance reduction (Ch.4). For the *code structure* around that logic,
CPP's MC framework is the reference — RNG class (Ch.6), statistics
gatherer (Ch.5), exotics/path-generation engine (Ch.7). CPP says how to
lay out the classes; GLA says what the numerics should actually do. This
is where both earn their place most.

### LSM Bermudan — 3–4 weeks, but only after Hull–White

**Sources:** IRC Ch.12 (optimal stopping, Bermudan-specific depth), I §3.4 (MC regression framework — basis functions + least-squares), GLA Ch.8 §8.6 (regression-based American-option pricing — the Longstaff–Schwartz algorithm, with §8.7 duality as an upper-bound check).

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

**Sources:** IRC Ch.15 (CCR depth), I §3.2–3.3 (EE construction, MPoR, nested-MC critique → regression-based exposure), GLA Ch.9 (loss probabilities, VaR/PFE quantiles, delta–gamma variance reduction, §9.4 credit risk).

Realistic v1 scope: **single-trade IRS exposure under Hull–White**, with
EE / EPE / PFE quantiles and a flat-hazard CVA approximation.

Out of scope for v1: portfolio netting, collateral (threshold/MTA),
margin period of risk (beyond a constant), wrong-way risk, ORE-style
XML workflows.

### ORE integration — only when ready

Open Source Risk Engine is large. Don't open the source until Phase 0–7
is solid. Then read examples first, source second.

---

## Possible CNY pivot (decided at Month 4)

If the answer at Month 4 is "pivot to CNY":

- Curves: FR007 IRS, SHIBOR IRS, LPR-linked swap, CNY discount curve,
  repo/funding curve.
- Conventions are **not drop-in replacements** for SOFR. Confirm the
  specific conventions when consulting others before building.
- The pricing engine and risk report code should be largely reusable;
  the conventions and market data layer are what change.

---

## What this roadmap is not

- Not a Gantt chart. Dates slip; the order matters more than the dates.
- Not a backlog of features for users. There are no users.
- Not a substitute for the math notes. The math notes are where
  understanding lives; this file is just the order in which to write
  them.
