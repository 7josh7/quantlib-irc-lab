# Roadmap

The execution plan has three layers:

1. **MVP — Phases 0–3:** environment, mini pricer, SOFR curve + bump risk,
   then the portfolio risk report. The four "weeks" below describe four
   focused stages, not a calendar promise.
2. **Optional post-MVP work — Phase 4 / 4.5:** foreign-collateral curves and
   a small xVA demonstration. Neither gates `v1.0-mvp`.
3. **Extensions — Phases 5–10:** re-evaluated after the MVP and again at the
   Month-4 decision gate. These are roadmap, not commitments.

Source citations use the IDs from [sources.md](sources.md). The local working
copies live under `docs/ref/` and are intentionally untracked. The four-part
series (I–IV) is primary for Phases 0–7; IRC is primary for Phases 8–10.

**Code architecture:** the finance sources say *what* to compute. **CPP**
(Joshi) provides small-library design patterns, while **IQ** (Ballabio,
*Implementing QuantLib*) explains the architecture and utilities behind the
QuantLib oracle. Add an abstraction when a second real use requires it—not
merely because a later phase might need it. The full source map is in
[sources.md](sources.md#where-the-implementation-references-apply).

**Testing and coverage:** every phase ships with validation tests. Starting in
Phase 2, add a coverage report to the automated build. "70% line coverage"
means the tests execute at least 70% of the executable lines under `src/`; it
does **not** mean the code is 70% correct. Coverage exposes untested paths,
while analytic checks and QuantLib comparisons establish numerical confidence.
The repository does not measure coverage yet, so the current value is unknown.

## Reference roles across the roadmap

Every work in `docs/ref/` has a defined role; inclusion does not mean every
phase must use every source.

| ID | Roadmap role |
|---|---|
| I | Collateralized-pricing foundation in Phase 1; xVA/CVA and regression in Phases 4.5, 5, 9, and 10. |
| II | Primary modern source for Phases 0–4: RFR compounding, SOFR bootstrap, risk transformation, PCA, and xCcy curves. |
| III | Phase 7 volatility source: Bachelier/local vol/SABR, ATM parameterization, and smile-risk Greeks. |
| IV | Phase 7 RFR-volatility source: backward-looking caplets, time-decay SABR, and bottom-up aggregation. |
| IRC | Secondary swap/curve/risk reference in Phases 0–3; primary source for CDS, Hull–White, LMM, Bermudan, and CCR in Phases 5–10. |
| BM | LIBOR-era theory back-stop for short-rate models, LMM calibration, SABR, Bermudan methods, and credit—not modern SOFR plumbing. |
| GLA | Numerical reference for finite differences, simulation, variance reduction, discretization, LSM regression, and risk/credit Monte Carlo. |
| CPP | C++ design reference for strategies, solvers, trees, MC component boundaries, exception safety, and levelization. |
| IQ | QuantLib implementation reference for instrument/engine separation, conventions, quotes, interpolation, solvers, handles, errors, and Observer behavior. |

## Architecture decisions by need

| Decision | When | Scope |
|---|---|---|
| Curve abstraction + floating-rate strategy | Phase 1 | Already justified by flat/bootstrapped curves and simple/compounded accrual variants. |
| Quote bump-and-reprice | Phase 2 | Required first-order risk implementation for the MVP. |
| QuantLib quote/handle machinery | Phase 2 benchmark | IQ explains how mutable quotes notify dependent term structures; the hand-rolled core remains explicit and minimal. |
| Finite-difference calibration Jacobian cross-check | Phase 2 stretch | Build only after direct quote bump-and-reprice is green; it is non-gating and provides a two-way validation of market-risk propagation. |
| Analytic Jacobian model→market risk propagation | Post-MVP | Source II §3.4 follows curve calibration. Add it only after finite-difference bump risk provides a trusted reference. |
| PCA risk and eigen-scenarios | Phase 3 stretch | Source II §§4.2–4.3 consume market-risk vectors and historical curve moves; they do not belong in the bootstrap. |
| Registry / plugin architecture | Phase 8+ if needed | Defer until multiple independently constructed model or product families make a factory useful. |
| Model serialization | When stable persistence is needed | Stable CSV input/output schemas are sufficient for the MVP. |

Keep modules small, tests assertion-based, inputs deterministic, and generated
CSVs byte-reproducible. Do not copy the source material's notebook architecture.

---

## MVP scope — Phases 0–3

### Phase 0 — Environment (implementation complete)

**Sources:** IQ Ch.2 and App. A for QuantLib wiring/conventions; II §3.2.2
(IRS); IRC Lecture 1 as a secondary rates reference.

**Goal:** establish a reproducible MSVC/CMake/vcpkg build with QuantLib and
GoogleTest.

The retained Phase 0 executable, `01_quantlib_hello_swap`, is a legacy
USD-LIBOR `VanillaSwap` scaffold. It proves the toolchain and QuantLib wiring;
it is not presented as current market practice. The Phase 1 QuantLib
`OvernightIndexedSwap` comparison is the canonical SOFR validation.

Evidence:

- `cmake --build` and `ctest` run successfully.
- `examples/01_quantlib_hello_swap.cpp` produces the documented deterministic
  output.
- `vcpkg.json` pins the dependency baseline.

There is no retained `v0.1-env` tag; `v0.2-mini-pricer` is the first milestone
tag in the repository. Do not manufacture a historical tag retroactively.

---

### Phase 1 — Hand-rolled mini pricer (implementation complete)

**Sources:** II §2.2, §3.2.2, and §6.1; I §2 for collateralized discounting;
IQ Ch.2 for the swap/instrument-engine comparison; IRC Lecture 1 as secondary.

**Goal:** understand the pricing mechanics by implementing a simplified,
single-curve SOFR-aware swap pricer and comparing it with QuantLib.

Implemented:

- `YieldCurve` with a flat continuously compounded implementation.
- Fixed and floating legs plus payer/receiver swap valuation.
- Simple-forward and projected daily-compounded accrual strategies.
- Analytic sanity checks and a QuantLib `OvernightIndexedSwap` oracle.

The implementation and 12 current tests are green. Before Phase 1 is fully
"done" under this repository's definition, the owner should remove the
remaining scaffold prompts and complete assumptions, inputs/outputs, and known
limitations in `docs/math_notes/01_sofr_swap.md`.

Milestone: tag `v0.2-mini-pricer`.

---

### Phase 2 — SOFR curve bootstrap + quote DV01 (current)

**Sources:** II §3.3.1, especially Table 1 and Eq. (47), is primary. IRC
Lecture 1 provides a second curve-stripping treatment. IQ App. A covers the
QuantLib quote, interpolation, solver, handle, and error machinery used by the
benchmark. GLA Ch.7 gives the broader finite-difference sensitivity context.
Source II §3.4 is a later risk extension, not a Phase 2 MVP dependency.

**Goal:** transform a pinned set of SOFR futures and OIS quotes into a
validated single-currency discount curve, then compute quote DV01 by
bump-and-reprice.

**Current gate:** `docs/math_notes/02_curve_bootstrapping.md` is an owner draft.
No interface, red tests, or implementation begins until the owner completes
the formulas, assumptions, inputs, outputs, conventions, tolerances, and known
limitations required by `AGENTS.md`.

#### Required decisions in the owner math note

- Pin the valuation date and use only instruments active on that date. If the
  Source II Table 1 quotes are reproduced, keep their observation date and
  contract strip together; do not combine `SR3H26` with the Phase 1 valuation
  date of May 23, 2026.
- State how futures prices become implied rates and that Phase 2 ignores the
  futures convexity adjustment, matching Source II §3.3.1.
- Define the curve state precisely: node dates, anchor, interpolation
  coordinate, and extrapolation policy. "Piecewise log-linear" alone is
  ambiguous unless it says what quantity is logged and interpolated.
- Define calibration residuals and tolerances with units. A rate error, a
  discount-factor error, and a currency NPV error are not interchangeable.
- State the DV01 sign convention and whether each bump applies to a market
  quote, an internal curve node, or the full quote set.
- Specify explicit failures for invalid dates, duplicate nodes, non-finite
  quotes, non-positive discount factors, and an unsolved calibration.

#### Workflow after the note is complete

1. AI proposes the smallest header-only interfaces; owner approves them.
2. AI writes GoogleTest cases and validation-only stubs so the suite builds red.
3. Owner implements the curve representation and sequential bootstrap.
4. Compare calibration repricing and curve outputs with a QuantLib
   `PiecewiseYieldCurve` built from matching futures/OIS helpers.
5. Add deterministic quote bump-and-reprice DV01.
6. Add `examples/02_quantlib_sofr_curve_bootstrap.cpp` and write
   `output/curve.csv` from a pinned sample quote file.

Exact class and file names are intentionally left to the interface-proposal
step. The minimum implementation needs a quote/input representation, an
immutable curve with discount lookup, and a bootstrap result that reports
diagnostics rather than hiding failures.

Tests must cover calibration repricing, positive discount factors, interpolation
at and between nodes, bad inputs, bump sign/magnitude, determinism, and comparison
with QuantLib. Every numerical tolerance must say what is being compared and in
which units.

**Deferred:** the analytic Jacobian transformation in Source II §3.4. *Owner
decision (2026-07):* a **finite-difference** calibration Jacobian + two-way
DV01 cross-check is an **in-phase stretch**, sequenced strictly after direct
quote DV01 is green and not gating `v0.3-curve-dv01`. That stretch becomes the
trusted numerical reference for the post-MVP analytic implementation.

Milestone: tag `v0.3-curve-dv01`.

---

### Phase 3 — Portfolio risk report (MVP)

**Sources:** II §3.4 (market-risk representation) and II §5 (DV01 and the
DV01-neutral steepener). IRC Lecture 14 provides a second treatment of input
and forward-curve sensitivities. Source II §§4.2–4.3 support the optional PCA
extension; GLA Ch.9 supplies broader risk-measure context.

**Goal:** consume the Phase 2 curve and a small swap portfolio, then produce the
four deterministic CSV reports promised in the README.

Math note: `docs/math_notes/03_dv01_krd_scenarios.md` — owner-written before
any Phase 3 interface or test work. It must define DV01/KRD signs, bump shapes,
scenario semantics, aggregation, inputs/outputs, and limitations.

Dependency order:

1. Pin the trade and market-data CSV schemas.
2. Price every trade and report NPV/fair rate.
3. Compute portfolio DV01 and 2Y/5Y/10Y key-rate risk.
4. Add parallel ±25 bp, steepener, and flattener scenarios.
5. Write all four outputs deterministically and verify them byte-for-byte.
6. **Stretch only after steps 1–5 are green:** generate a fixed-seed synthetic
   curve history, then add PCA factors and eigen-scenarios from Source II
   §§4.2–4.3.

Planned executable: `03_swap_portfolio_risk`.

Required outputs:

- `output/curve.csv`
- `output/swap_npv.csv`
- `output/dv01_report.csv`
- `output/scenario_pnl.csv`

Required tests include KRD aggregation versus parallel DV01 within a tolerance
defined in the note, scenario P&L sign checks, invalid CSV/input handling, and
byte-identical repeat output. PCA rows may be added to `scenario_pnl.csv` if the
stretch is completed, but PCA does not gate the MVP.

Milestone: tag `v1.0-mvp`. **This is the contract.**

---

## Optional post-MVP work

### Phase 4 — xCcy basis + foreign-collateralized discounting

**Sources:** II §§3.2.3–3.2.5 and §§3.3.2–3.3.3; I §2 for the
collateralized-pricing foundation.

**Status:** optional and strictly after Phase 3. It does not gate `v1.0-mvp`.

**Goal:** reuse the Phase 2 single-currency bootstrap to build USD SOFR and EUR
RFR curves, then construct the curve for EUR cash flows collateralized in USD
from xCcy basis quotes. Source II deliberately develops this after the domestic
RFR curve and basis-forward curves, so the implementation should follow the
same dependency order.

Math note: `docs/math_notes/04_foreign_collateral_curves.md` — owner-written
before implementation. It must cover the non-MTM xCcy cash flows, FX-forward
currency chain, collateral-currency effect, curve parameterization, signs,
inputs/outputs, and limitations.

Keep the first version to non-MTM xCcy basis swaps, covered-interest-parity
checks, a deterministic synthetic quote file, explicit calibration diagnostics,
and a QuantLib comparison where its experimental helpers are usable. MTM resets,
B-splines, and Tikhonov regularization remain out of scope.

Milestone: `v1.0.2-xccy` if delivered.

### Phase 4.5 — xVA awareness

**Sources:** I §2, §3.1, and §§3.2–3.3.

**Status:** optional and strictly after Phase 3. It does not gate `v1.0-mvp`.

**Goal:** add a clearly labeled educational CVA approximation to the existing
swap portfolio, separating the clean collateralized price from the valuation
adjustment. The owner math note must define exposure direction, discounting,
hazard/default assumptions, LGD, and the Gaussian margin-period-of-risk proxy
before any implementation.

Milestone: `v1.0.1-xva` if delivered; otherwise defer to Phase 10.

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
coding (same discipline as Phase 4).

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
  curve** (decorator pattern: curve ops
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

### Month-4 decision gate

- Polish tests; inspect the automated coverage report, keep line coverage above
  70%, and backfill meaningful untested branches rather than chasing the number.
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

### Phase 8 — LMM Monte Carlo (≥6 weeks)

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

### Phase 9 — LSM Bermudan (3–4 weeks, only after Hull–White)

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

### Phase 10 — CCR / Exposure / CVA (scope tightly)

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
