# 02 — SOFR curve bootstrap + DV01: implementation contract (Phase 2)

> **Not a math note.** The math lives in
> [`docs/math_notes/02_curve_bootstrapping.md`](../math_notes/02_curve_bootstrapping.md);
> this file is the code-facing spec: conventions pinned, architecture decided,
> interfaces approved, tests planned. The owner approved the headers below
> under [AGENTS.md](../../AGENTS.md) workflow step 2.
> Per step 4, the owner implements; AI writes the red tests and stubs.
>
> Revision 10 — signed payment lag (2026-07-19): `make_coupon_periods` takes
> `QuantLib::Integer` (matching QuantLib's own `paymentLag` parameters)
> instead of `Natural`, so a negative lag arrives intact and is rejected with
> `std::invalid_argument` rather than wrapping to a large unsigned value;
> test 35 gains the negative-lag rejection.
>
> Revision 9 — owner-approved `CouponPeriod` interface (2026-07-17): pins the
> exact §2a header (`src/rates/coupon_period.hpp`), its validation contract,
> the zero-lag delegation rule, the CMake source line, and the group I red
> tests. The red-test/stub materialization accompanying this revision is
> workflow step 1 of §7.
>
> Revision 8 — owner-approved payment-lag representation (2026-07-16): adds
> a `CouponPeriod` value type, QuantLib-based period construction, and
> period-based fixed/floating legs while preserving the Phase 1 zero-lag
> constructors through delegation.
>
> Revision 7 — fixture data (2026-07-16): downloads and pins the 19 final
> New York Fed SOFR effective-date observations authorized by the owner; the
> next workflow gate is red tests and validation-only stubs.
>
> Revision 6 — owner decisions (2026-07-16): separates USNY OIS
> schedule/payment dates from USGS SOFR fixing dates per the CFTC
> specification; records the payment-lag pricer and finite-input hardening that
> the owner will address during implementation; updates the workflow status.
>
> Revision 5 — owner-approved interface review (2026-07-15): retains rev 4's
> deterministic hybrid fixture and partially accrued SR3Z25 treatment; adds a
> persisted UTC as-of timestamp, a finance-free callable bisection contract,
> validated triangular Jacobian input, central-DV01-only wording, and the
> correct two-segment node-bump test. Curve-level batching is removed; the
> vectorized interview contract remains isolated in `LinearFlatInterpolator`.

## 0. Scope

Build an auditable USD SOFR single-curve prototype from SR3 futures + OIS par
quotes, then compute first-order risk. Two tiers:

- **Required (gates `v0.3-curve-dv01`):** bootstrap (including the partially
  accrued first future), repricing diagnostics, QuantLib benchmark, **direct
  quote-level DV01** (bump quote → full re-bootstrap → reprice), pinned
  quote + fixings files in, `output/curve.csv` out.
- **In-phase stretch (after the required tier is green):** the
  finite-difference calibration Jacobian $\mathcal J^{\mathrm{FD}}$ and the
  two-way DV01 cross-check. Pulled forward from post-MVP by owner decision;
  the roadmap's ordering constraint is respected — the Jacobian path is built
  only *after* the direct bump implementation it is checked against is
  trusted. The **analytic** Jacobian remains post-MVP.

Out of scope (per math note / roadmap): futures convexity adjustment,
multi-curve basis, xCcy collateral, fixing-correction / valuation-roll
scenarios (the math note's DV01 convention explicitly treats those as
separate scenario types, not DV01). All derivatives in Phase 2 are
finite-difference.

## 1. Conventions (pinned)

| Item | Value (Phase 2) | Notes |
|------|-----------------|-------|
| Fixture as-of | **2026-01-15 (Thursday, NYC business day) at 3:00 p.m. ET** | persisted as `2026-01-15T20:00:00Z`; after the ~2:30 p.m. same-day revision window, so all published fixings are final (math note §Market Instruments) |
| Fixture provenance | **Deterministic hybrid**: historical SOFR fixings are **real final NY Fed values**; SR3 and OIS quotes are **synthetic but internally consistent**. Not a historical market snapshot. | math note §Market Instruments / §Assumptions |
| Futures set | **SR3Z25 … SR3Z28 — 13 contiguous quarterly IMM contracts** | reference quarters chain end-to-start |
| First future | **SR3Z25 is partially accrued**: quarter $[2025\text{-}12\text{-}17,\,2026\text{-}03\text{-}18)$, 91 calendar days, $\tau_1 = 91/360$; realized window $[2025\text{-}12\text{-}17,\,2026\text{-}01\text{-}15)$ = 29 calendar days | all later contracts fully forward |
| Historical fixings | rate dates **2025-12-17 … 2026-01-14** (business days), final NY Fed values; each fixing carries its Act/360 **calendar-day weight through the next business-day rate date**; no weekend/holiday rate dates; never projected or filled | weights over the realized window sum to 29 days |
| First pillar equation | $P(0,T_{e,1}) = A^{\mathrm{SOFR}}(T_{s,1},0)\,/\,(1+\tau_1 R_{\mathrm{fut},1})$, with $A$ the product of $(1+r_k\delta_k)$ over realized fixings | math note §Calibration + §Repricing Diagnostics (inverse form + positivity conditions) |
| OIS set | **4Y, 5Y, 6Y, 7Y, 10Y, 12Y, 15Y, 20Y, and 30Y**, selected from the CFTC spot-starting par SOFR OIS tenor set; 2Y and 3Y are omitted because the SR3 futures strip covers the short end | spot from 2026-01-15 is 2026-01-20 (MLK Monday 2026-01-19 skipped — deliberate calendar exercise) |
| Instrument count | $M = 13 + 9 = 22$ | diagnostics, DV01 vectors, Jacobian dimension |
| OIS business / payment calendar | `UnitedStates(Settlement)` | QuantLib mapping used here for CFTC `USNY`; governs spot, accrual schedules, and payment dates |
| SOFR fixing calendar | `UnitedStates(SOFR)` | QuantLib `Sofr` index calendar; maps the CFTC `USGS` fixing role and governs fixing dates/calendar-day weights |
| Futures reference dates | Unadjusted IMM third Wednesdays | fixing coverage inside each reference quarter uses `UnitedStates(SOFR)` |
| Curve day counter | Act/365F | node times $t_m$; math note §Interpolation |
| Leg / futures accrual day counter | Act/360 | $\tau_i,\alpha_j,\tau_m,\delta_k$ |
| OIS payment delay | 2 business days after accrual end | $U_i, V_j$ |
| Business-day convention | ModifiedFollowing (OIS); IMM dates as-given (futures) | |
| Curve state | $x_m = \log P(0, L_m)$ at pillar dates | |
| Anchor node | storage index 0: $(t{=}0,\ \log P{=}0)$, **immutable** | §2b indexing contract; $A^{\mathrm{SOFR}}$ is *not* a curve node — it multiplies externally in the bootstrapper |
| Pillar rule | futures: reference-quarter end; OIS: last payment date (incl. delay) | `Pillar::LastRelevantDate` on the QL side |
| Interpolation | linear in $\log P$ vs Act/365F time | constant IFR between nodes; the anchor→first-pillar segment is an ordinary interpolation segment (used e.g. to discount the OIS effective date 2026-01-20) |
| Extrapolation | **curve throws** beyond last pillar and before reference | past dates (e.g. $T_{s,1}$) are never curve queries — realized accumulation handles them |
| Root solver | finance-free callable-based bracketed bisection on $\epsilon_m$; residual tol | $\epsilon_m\le10^{-12}$ (rate units), max 200 midpoint iterations. `src/core/bracketed_bisection.hpp`; no solver inheritance |
| Solver bracket | segment forward rate $f_m \in [-10\%, +50\%]$ (mapped to $x_m = x_{m-1} - f_m\,(t_m - t_{m-1})$); expand once to $[-50\%, +100\%]$; then **throw** with the instrument id | explicit two-stage rule; no silent fallback |
| Bump semantics | §4a — one formula; reference implementation sets $h_q = \Delta_{\mathrm{bp}} = 10^{-4}$ | matches math note "reference implementation" |
| DV01 scenario convention | `MarketAsOf`, fixings, and $A^{\mathrm{SOFR}}$ **bit-for-bit immutable**; bump the quoted **full-quarter** futures rate ($R\pm h \leftrightarrow Q\mp 100h$); **complete re-bootstrap per scenario** | math note §Quote DV01 Convention, items 1–5 |
| DV01 sign | upward-oriented signed DV01: positive when $\partial V/\partial q_m>0$ (payer +, receiver −) | central formula in §4a; math note §Sign convention |

## 1a. Pinned market data — two files

Real data and synthetic data are kept in separate files (provenance is
visible in the filesystem):

**`data/market/sofr_fixings_2025-12-17_2026-01-14.csv`** — real NY Fed final
fixings. Schema:

```csv
# Final SOFR fixings published by the New York Fed (rate dates
# 2025-12-17 .. 2026-01-14). Real data downloaded from the official API.
rate_date,rate
2025-12-17,0.0369
...
2026-01-14,0.0364
```

The file was downloaded from the official New York Fed Markets Data API on
2026-07-16. Its 19 `percentRate` values were divided by 100 and stored as
decimal rates. The loader validates the exact effective-date set against the
`UnitedStates(SOFR)` calendar, including the Christmas and New Year holidays.

**`data/market/sofr_quotes_2026-01-15.csv`** — synthetic quotes, single
source of truth for tests **and** the example. Accepted as calibration data
only when both bootstrap and QuantLib benchmark tests pass; the prose does
not assert arbitrage-freedom in advance:

```csv
# Synthetic SOFR market snapshot — deterministic test fixture (hybrid:
# fixings file carries the real data). NOT a market transcription.
# As-of 2026-01-15T20:00:00Z (2026-01-15 3:00 p.m. ET).
# UTC is the persisted timestamp convention; quote_unit is explicit.
type,id,valuation_date,as_of_utc,start,end,quote,quote_unit
valuation,,2026-01-15,2026-01-15T20:00:00Z,,,,
future,SR3Z25,,,2025-12-17,2026-03-18,95.75,imm_price
future,SR3H26,,,2026-03-18,2026-06-17,95.80,imm_price
future,SR3M26,,,2026-06-17,2026-09-16,95.85,imm_price
future,SR3U26,,,2026-09-16,2026-12-16,95.90,imm_price
future,SR3Z26,,,2026-12-16,2027-03-17,95.95,imm_price
future,SR3H27,,,2027-03-17,2027-06-16,96.00,imm_price
future,SR3M27,,,2027-06-16,2027-09-15,96.03,imm_price
future,SR3U27,,,2027-09-15,2027-12-15,96.06,imm_price
future,SR3Z27,,,2027-12-15,2028-03-15,96.09,imm_price
future,SR3H28,,,2028-03-15,2028-06-21,96.12,imm_price
future,SR3M28,,,2028-06-21,2028-09-20,96.15,imm_price
future,SR3U28,,,2028-09-20,2028-12-20,96.18,imm_price
future,SR3Z28,,,2028-12-20,2029-03-21,96.20,imm_price
ois,4Y,,,,,0.0375,decimal_rate
ois,5Y,,,,,0.0370,decimal_rate
ois,6Y,,,,,0.0365,decimal_rate
ois,7Y,,,,,0.0360,decimal_rate
ois,10Y,,,,,0.0350,decimal_rate
ois,12Y,,,,,0.0345,decimal_rate
ois,15Y,,,,,0.0340,decimal_rate
ois,20Y,,,,,0.0335,decimal_rate
ois,30Y,,,,,0.0330,decimal_rate
```

(IMM third-Wednesday dates verified for 2025–2029; each contract's end
equals the next contract's start, so the strip telescopes. SR3Z25's quarter
contains 91 calendar days; the numbers in the math note's tests 1–3 were
verified numerically to $10^{-12}$.)

**Example output** — `output/curve.csv`, one row per node (anchor included),
byte-deterministic:

```csv
date,t_act365f,discount,zero_cc,fwd_section
```

with `zero_cc` $= -\log P(0,T)\,/\,t$ (t > 0; empty for the anchor row) and
`fwd_section` the constant IFR of the segment *ending* at this node. These
are output conveniences defined here, not new math-note quantities.

The byte contract is explicit so a golden-file test is meaningful:

- rows are in curve storage order; the anchor row is first;
- dates use ASCII ISO `YYYY-MM-DD`;
- every populated floating-point field uses lowercase scientific notation
  with 16 digits after the decimal point (17 significant digits), generated
  with locale-independent `std::to_chars`;
- the anchor has `t_act365f = 0`, `discount = 1`, and empty `zero_cc` and
  `fwd_section` fields;
- the file is UTF-8/ASCII without a BOM, uses `\n` line endings on every
  platform, and ends with one newline;
- writing uses binary/truncate mode and throws `std::runtime_error` on open,
  write, flush, or close failure. No partial-success status is returned.

## 2. Architecture — separated responsibilities, one dependency direction

Dependency arrows below point from a consumer to the lower-level component it
uses:

```
market_data_io ------------> SofrMarketData
SofrCurveBootstrapper -----> SofrMarketData
          |----------------> bracketed_bisection
          `----------------> PiecewiseLogLinearCurve
PiecewiseLogLinearCurve ---> LinearFlatInterpolator
curve_io ------------------> PiecewiseLogLinearCurve
risk/dv01 -----------------> SofrCurveBootstrapper
          `----------------> YieldCurve
```

The runtime data flow remains: quote and fixing CSVs → `market_data_io` →
`SofrMarketData` → `SofrCurveBootstrapper` → `BootstrapResult`.

Approved decisions:

- **The interpolator is finance-independent.** Knot arrays and queries only —
  no dates, discount factors, or instruments. It never takes a curve
  (circular dependency; untestable in isolation).
- **The bootstrapper is not a curve method.** Calibration coupled into the
  curve would drag futures/OIS conventions, fixings, root solvers, and quote
  validation into a numerical term structure — and make it impossible to
  construct a curve from known nodes in unit tests.
- **Realized accumulation lives in the bootstrapper layer, not the curve.**
  $A^{\mathrm{SOFR}}$ is computed from the fixings input and multiplies
  externally (math note: $P(0,T_{e,1}) = P(0;T_{s,1},T_{e,1})\,A$); the curve
  itself never answers queries before its reference date.
- **Vectorized = batch API, not SIMD.** `evaluate()` takes many query points
  in one call, returns one flat vector. Binary search per query,
  $O(n\log k + nm)$.
- **Extrapolation split:** the generic interpolator does flat extrapolation
  (clamp to end values). The curve **rejects** queries outside
  `[reference, last pillar]` — flat log-DF extrapolation would imply a zero
  forward rate, which is financially wrong.

### 2a. Owner implementation requirements

Two prerequisites are recorded here rather than silently invented while the
tests are being implemented:

- **Payment-lag-capable OIS pricing (owner-approved design, 2026-07-16).** The
  existing Phase 1 legs use each schedule date as both accrual end and payment
  date, so they cannot represent this phase's two-USNY-business-day payment
  delay. Introduce a small `CouponPeriod` value type containing
  `accrual_start`, `accrual_end`, `payment_date`, and the accrual
  `year_fraction`. Build period vectors from a QuantLib `Schedule`, the leg day
  counter, and the applicable QuantLib payment calendar and payment lag. For
  Phase 2 OIS coupons, the payment date is the accrual end advanced by two
  USNY business days; the accrual boundaries are not replaced by the delayed
  payment date.

  Add period-based constructors to `FixedLeg` and `FloatingLeg`, and make their
  pricing methods consume the resulting `CouponPeriod` sequence. Preserve
  each existing Phase 1 constructor as the zero-lag convenience overload; it
  delegates to the period-based representation with
  `payment_date == accrual_end`. The bootstrap residuals, the 10Y target-swap
  PV, and its DV01 must all use these same leg implementations. Tests 25–30
  must not substitute the Phase 1 zero-lag convention or duplicate an
  undocumented pricer inside a test. The exact header and validation contract
  are pinned in §3 (`src/rates/coupon_period.hpp`).
- **Finite-input hardening.** Before Phase 2 depends on the Phase 1 pricing
  classes, add red regression tests to `tests/test_mini_pricer.cpp` and then
  make `FlatCurve`, `FixedLeg`, `FloatingLeg`, and both rate-accrual strategies
  reject non-finite numeric inputs explicitly. Finiteness checks apply to curve zero
  rates, fixed rates, notionals, spreads, and caller-supplied year fractions;
  existing domain checks such as positive notional and year fraction remain.
  NaN must not pass a positivity check merely because comparisons with NaN are
  false, and no public pricing path may return a silent NaN or infinity.

### 2b. Node indexing contract (anchor vs calibrated nodes)

The math note indexes **calibrated** nodes $m = 1,\dots,M$ ($M = 22$: one per
instrument). The curve *stores* $M{+}1 = 23$ nodes because the constructor
prepends the immutable anchor. The mapping is fixed here once:

| Math note | Curve storage |
|-----------|---------------|
| (anchor, $P(0,0)=1$, not a parameter) | index $0$ |
| calibrated node $m$ ($m = 1..M$) | index $m$ |

Rules: risk and Jacobian code iterates storage indices $1..M$ and **never
bumps index 0**; `bump_node(curve, 0, …)` throws `std::invalid_argument`;
`curve_node_sensitivities` returns exactly $M$ entries aligned with storage
indices $1..M$ (and with instrument order).

## 3. Approved interfaces (headers only — owner implements)

### `src/core/bracketed_bisection.hpp` — generic callable solver

```cpp
#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>

namespace irc {

struct BisectionOptions {
    double residual_tolerance = 1e-12;
    std::size_t max_iterations = 200;
};

struct BisectionResult {
    double root;
    double residual;
    std::size_t iterations;
};

template <typename F>
concept ScalarResidual =
    std::invocable<F&, double> &&
    std::convertible_to<std::invoke_result_t<F&, double>, double>;

// Solves residual(x) = 0 on an already-bracketed finite interval.
// The callable may capture auxiliary state; it is invoked as an lvalue and is
// neither copied into persistent storage nor retained after this call.
template <ScalarResidual F>
BisectionResult bracketed_bisection(F&& residual, double lower, double upper,
                                    BisectionOptions options = {});

}  // namespace irc
```

This low-level function performs no financial bracket construction or
expansion. It requires finite `lower < upper`, a finite positive residual
tolerance, a positive iteration limit, finite residual evaluations, and either
an endpoint root or opposite endpoint signs. Invalid arguments throw
`std::invalid_argument`; a non-finite evaluation or exhausted iteration limit
throws `std::runtime_error` with the final numerical bracket and residuals.
`iterations` counts midpoint evaluations; endpoint validation is excluded.

The solver is a generic callable-based function because only the scalar
contract `double residual(double x)` belongs in the numerical layer. A normal
function can be passed when the residual has no auxiliary state. During OIS
calibration, however, a lambda captures the instrument, market quote,
previously solved nodes, and candidate-curve construction and exposes that
stateful calculation through the same one-argument contract. This avoids
global state and avoids adding finance-specific parameters to the solver.
The forwarding-reference parameter accepts named or temporary callables
without a mandatory copy; the `ScalarResidual` concept validates the contract
at compile time. That concept checks `std::invocable<F&, double>` (lvalue),
not bare `F`, because the solver invokes the named `residual` parameter as an
lvalue on every iteration; validating bare `F` would admit rvalue-only
callables that then fail to compile inside the loop. (Language mechanics:
[`docs/cpp_notes.md`](../cpp_notes.md).) A template also avoids the type
erasure of `std::function` and the virtual dispatch of a solver/residual
inheritance hierarchy; the solver requires no heap allocation or
polymorphic-object ownership.

The bootstrapper owns the two-stage forward-rate bracket rule in §1. It captures
the candidate-instrument state in a lambda, maps each financial bracket to node
space, and calls `bracketed_bisection` only after establishing a sign change. It
wraps any solver exception with the instrument ID while preserving the
numerical context. This follows CPP Ch.9's function-object pattern without a
solver inheritance hierarchy.

### `src/core/linear_interpolator.hpp` — generic, finance-free

```cpp
#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace irc {

// Vectorized piecewise-linear interpolator with flat extrapolation.
// Knots xs (strictly increasing) each carry m = ys.size()/xs.size() outputs,
// stored row-major: ys = { y(x0)[0..m), y(x1)[0..m), ... }.
// evaluate() returns queries.size()*m values, row-major per query.
// Queries need not be sorted. Outside [xs.front(), xs.back()] the end
// values are returned unchanged (flat extrapolation).
class LinearFlatInterpolator {
public:
    LinearFlatInterpolator(std::vector<double> xs, std::vector<double> ys);

    std::vector<double> evaluate(std::span<const double> queries) const;

    std::size_t outputs_per_knot() const;

private:
    std::vector<double> xs_;
    std::vector<double> ys_;
    std::size_t m_;
};

}  // namespace irc
```

Constructor throws `std::invalid_argument` on: empty `xs`; non-finite or
non-strictly-increasing `xs`; `ys.size()` not a positive multiple of
`xs.size()`; non-finite `ys`. `evaluate()` throws `std::invalid_argument` if
any query is non-finite.

### `src/rates/coupon_period.hpp` — §2a payment-lag representation

```cpp
#pragma once
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>
#include <ql/types.hpp>

#include <vector>

namespace irc {

// One coupon accrual period with its (possibly delayed) payment date.
// Accrual boundaries are never replaced by the payment date (Section 2a); the
// stored year_fraction is the leg day counter's accrual over
// [accrual_start, accrual_end], so pricing never re-derives it.
struct CouponPeriod {
    QuantLib::Date accrual_start;
    QuantLib::Date accrual_end;
    QuantLib::Date payment_date;
    double year_fraction;

    bool operator==(const CouponPeriod&) const = default;
};

// One period per consecutive schedule date pair:
//   payment_date  = payment_calendar.advance(accrual_end, lag, Days)
//   year_fraction = day_counter.yearFraction(accrual_start, accrual_end)
// payment_lag_business_days == 0 returns payment_date == accrual_end
// unchanged - the builder never calls advance(d, 0, Days), because QuantLib
// defines that as adjust(d), which could silently move an unadjusted date.
// The lag is signed (as in QuantLib's own paymentLag parameters) so a
// negative input arrives intact and throws std::invalid_argument instead of
// wrapping to a large Natural.
std::vector<CouponPeriod> make_coupon_periods(const QuantLib::Schedule& schedule,
                                              const QuantLib::DayCounter& day_counter,
                                              const QuantLib::Calendar& payment_calendar,
                                              QuantLib::Integer payment_lag_business_days);

}  // namespace irc
```

`make_coupon_periods` throws `std::invalid_argument` on a schedule with fewer
than two dates, an empty day counter, an empty calendar, or a negative
payment lag.

The period-based leg constructors are added to the existing Phase 1 headers:

```cpp
// src/rates/fixed_leg.hpp — added alongside the Phase 1 constructor
FixedLeg(std::vector<CouponPeriod> periods, double notional, double fixed_rate);

// src/rates/floating_leg.hpp — added alongside the Phase 1 constructor
FloatingLeg(std::vector<CouponPeriod> periods, double notional,
            std::shared_ptr<const RateAccrual> accrual, double spread = 0.0);
```

Their validation throws `std::invalid_argument` on: empty `periods`; any null
date; `accrual_start >= accrual_end`; `payment_date < accrual_end`;
non-finite or non-positive `year_fraction`; unordered or overlapping periods
(`accrual_start[i+1] < accrual_end[i]`). The §2a finiteness rules for
notional, fixed rate, and spread apply to these constructors from day one.
Contiguity (`accrual_start[i+1] == accrual_end[i]`) is *not* required: the
builder produces contiguous periods from a schedule, but the constructors
accept any ordered non-overlapping sequence.

Delegation rule (pinned so nobody "simplifies" it later): the Phase 1
`(Schedule, DayCounter, ...)` constructors delegate by building periods
directly with `payment_date = accrual_end`. They do **not** route through
`make_coupon_periods` with lag 0 — they have no payment calendar to pass, and
the identity must hold even for dates a calendar would adjust. Once the owner
implements §2a, the pricing methods consume only the period representation;
until then the stubs keep the Phase 1 schedule-based pricing path untouched
(green) and throw `std::logic_error` from the period-constructed path.

### `src/curves/piecewise_log_linear_curve.hpp`

```cpp
#pragma once
#include "core/linear_interpolator.hpp"
#include "core/yield_curve.hpp"

#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace irc {

struct CurveNode {
    QuantLib::Date date;
    double log_discount;
};

// Discount curve stored as (pillar date, log P(0, pillar)) nodes,
// interpolated linearly in log-DF vs Act/365F time (math note
// §Interpolation). Constant instantaneous forward between nodes.
// Storage index 0 is the immutable anchor (reference date, log P = 0).
// Callers supply calibrated pillars strictly after reference; the constructor
// always prepends the anchor (impl note §2b).
// Throws on queries before reference or beyond the last pillar.
class PiecewiseLogLinearCurve final : public YieldCurve {
public:
    PiecewiseLogLinearCurve(QuantLib::Date reference,
                            std::vector<CurveNode> pillars,
                            QuantLib::DayCounter day_counter);

    double discount(const QuantLib::Date& d) const override;
    QuantLib::Date reference_date() const override;
    const QuantLib::DayCounter& day_counter() const;

    // Read-only node view for risk and diagnostics (index 0 = anchor).
    std::span<const CurveNode> nodes() const;

private:
    QuantLib::Date reference_;
    std::vector<CurveNode> nodes_;
    QuantLib::DayCounter day_counter_;
    LinearFlatInterpolator interpolator_;  // built once in the constructor
                                           // over (Act/365F times, log-DFs);
                                           // domain enforcement lives in
                                           // discount(), which throws before
                                           // delegating.
};

// Free function (keeps mutation out of the curve API): a copy of `curve`
// with storage node i bumped by delta in log-DF space.
// Throws std::invalid_argument if node_index == 0 (anchor is immutable)
// or node_index is out of range.
PiecewiseLogLinearCurve bump_node(const PiecewiseLogLinearCurve& curve,
                                  std::size_t node_index, double delta);

}  // namespace irc
```

Constructor validates: reference not null; pillars non-empty; pillar dates
strictly increasing and all strictly greater than reference; log-DF values
finite with finite, strictly positive `exp(log_df)`; day counter non-empty.
There is no second "anchor already supplied" input form. Grouping each date
with its value in `CurveNode` makes a date/value size mismatch
unrepresentable.

### `src/curves/curve_instruments.hpp` — calibration inputs

```cpp
#pragma once
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace irc {

// Auditable market-snapshot identity. valuation_date is the New York business
// date used for scheduling; as_of_utc is the absolute observation instant.
struct MarketAsOf {
    QuantLib::Date valuation_date;
    std::chrono::sys_seconds as_of_utc;

    bool operator==(const MarketAsOf&) const = default;
};

// One CME 3M SOFR future (SR3): IMM reference quarter + quoted price.
struct SofrFutureQuote {
    std::string id;                  // stable ID, e.g. "SR3Z25"
    QuantLib::Date reference_start;  // IMM quarter start
    QuantLib::Date reference_end;    // IMM quarter end (= pillar)
    double price;                    // Q_m, e.g. 95.75
};

// One spot-starting USD SOFR OIS par quote.
struct OisQuote {
    std::string id;          // stable ID from input, e.g. "4Y"
    QuantLib::Period tenor;  // 4Y ... 30Y
    double par_rate;         // S_m, decimal
};

// One historical SOFR fixing: business-day rate date + final published
// decimal rate. The fixing covers [rate_date, next business day) and
// carries the Act/360 calendar-day weight of that interval (math note
// Appendix: Daily SOFR).
struct SofrFixing {
    QuantLib::Date rate_date;
    double rate;
};

// Canonical futures quote conversion. Calibration, residuals, and risk use
// decimal-rate units even though persisted market data uses IMM price units.
double sofr_future_rate_from_price(double price);
double sofr_future_price_from_rate(double rate);

// Realized SOFR accumulation factor A^SOFR(start, end_exclusive):
//   prod_k (1 + r_k * delta_k),  delta_k = calendar days covered / 360,
// where fixing k covers [rate_date_k, next business rate date) clipped to
// [start, end_exclusive). Math note §Calibration (A^SOFR) and test 2.
// Throws std::invalid_argument on: missing coverage anywhere in
// [start, end_exclusive); duplicate, out-of-window, weekend/holiday, or
// non-finite fixings. Never projects or fills a missing fixing.
double realized_accumulation(const std::vector<SofrFixing>& fixings,
                             const QuantLib::Date& start,
                             const QuantLib::Date& end_exclusive,
                             const QuantLib::Calendar& fixing_calendar);

// Everything the bootstrapper consumes. Phase 2's fixture has exactly one
// partially accrued contract (SR3Z25); `fixings` must cover its realized
// window [reference_start, as_of.valuation_date). Fully-forward-only inputs
// are also valid, with empty fixings.
struct SofrMarketData {
    MarketAsOf as_of;
    std::vector<SofrFutureQuote> futures;  // sorted by reference_start
    std::vector<OisQuote> ois;             // sorted by tenor
    std::vector<SofrFixing> fixings;       // sorted by rate_date
};

}  // namespace irc
```

The conversion functions throw `std::invalid_argument` for non-finite
inputs. They perform only the unit conversion; the bootstrapper validates
the contract-specific positivity condition $1+\tau_m R_{\mathrm{fut},m}>0$.

### `src/curves/market_data_io.hpp` — pinned-file loaders

```cpp
#pragma once
#include "curves/curve_instruments.hpp"

#include <filesystem>

namespace irc {

// Reads the pinned quotes CSV (impl note §1a) and, when fixings_csv is
// non-empty, the fixings CSV. Throws std::runtime_error naming the path and
// offending line on malformed CSV or invalid field text. Semantic validation
// (described below) names the instrument ID, rate date, or as-of field.
SofrMarketData load_sofr_market_data(
    const std::filesystem::path& quotes_csv,
    const std::filesystem::path& fixings_csv = {});

}  // namespace irc
```

Parsing is strict: exactly one header and valuation row; no missing or extra
fields; dates, timestamps, and numbers must consume the complete field;
instrument IDs must be non-empty and unique. The valuation row must contain
both `valuation_date` and `as_of_utc`; `as_of_utc` accepts exactly second-
precision UTC text `YYYY-MM-DDTHH:MM:SSZ` and is parsed into
`std::chrono::sys_seconds`. Fields that do not apply to a row type must be
empty; `quote_unit` must be exactly `imm_price` for futures and
`decimal_rate` for OIS rows. The accepted CSV subset has unquoted fields only:
blank lines and `#` comment lines are ignored, surrounding ASCII whitespace
and a trailing `\r` are removed, and quoted fields are rejected. Numeric
parsing is locale-independent.

Semantic validation requires: a non-null valuation date that is a
`UnitedStates(Settlement)` business day; valid ordered third-Wednesday IMM dates with
`reference_end > reference_start`; contiguous non-overlapping futures
quarters; unique increasing OIS tenors; finite quotes. Negative OIS rates and
futures prices above 100 are valid. A futures quote is invalid only when its
Act/360 accrual factor is non-positive or $1+\tau R_{\mathrm{fut}}\le0$.

`valuation_date` and `as_of_utc` are intentionally distinct. The first is the
New York business date used by financial conventions; the second is the
absolute market-data observation instant. The loader parses both but does not
compare their UTC calendar dates. The pinned-fixture test asserts the exact
agreed pair `2026-01-15` and `2026-01-15T20:00:00Z`. If this loader is later
generalized to arbitrary snapshot times, any cross-field consistency check
must compare `valuation_date` with the **America/New_York local date** of
`as_of_utc` under an explicit timezone policy; Phase 2 does not add a timezone-
database dependency.
Fixings validation (math note §Inputs): every `rate_date` is a
`UnitedStates(SOFR)` business day in ascending order with no duplicates; rates
finite; missing, duplicate, out-of-range, or non-finite records are errors and
are never projected or silently filled. OIS spot, accrual, and payment dates
are generated with `UnitedStates(Settlement)`; the two calendars must not be
interchanged merely to make the hand-rolled and QuantLib curves agree.

### `src/curves/sofr_bootstrapper.hpp`

```cpp
#pragma once
#include "curves/curve_instruments.hpp"
#include "curves/piecewise_log_linear_curve.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace irc {

// One successful calibration check, in market-instrument order. Both quote
// fields are decimal rates: a future's persisted IMM price is normalized via
// sofr_future_rate_from_price before it reaches this boundary.
struct CalibrationDiagnostic {
    std::string instrument_id;
    double market_quote;
    double model_quote;
    double residual;                // model_quote - market_quote, rate units
    std::size_t solver_iterations;  // zero for closed-form futures nodes
    bool used_expanded_bracket;     // false for futures
};

struct BootstrapResult {
    MarketAsOf as_of;  // copied unchanged from the calibrated market snapshot
    PiecewiseLogLinearCurve curve;
    std::vector<CalibrationDiagnostic> diagnostics;
};

// Sequential bootstrap per math note §Calibration:
//  1. anchor P(0,0) = 1;
//  2. first future (partially accrued, T_s,1 < 0 < T_e,1):
//       A = realized_accumulation(fixings, T_s,1,
//                                 market.as_of.valuation_date, calendar);
//       P(0, T_e,1) = A / (1 + tau_1 * R_fut,1);
//     its model quote for diagnostics/repricing inverts this:
//       R_model,1 = (A / P(0, T_e,1) - 1) / tau_1        (math note
//     §Repricing Diagnostics, with positivity conditions tau_1 > 0, A > 0,
//     P(0, T_e,1) > 0);
//  3. remaining futures: telescope P(0,T_e) = P_fwd * P(0,T_s) — each
//     T_s equals the previous pillar because the strip is contiguous;
//  4. OIS: for each quote solve eps_m(x_m) = K_model,m(x_m) - S_m = 0 by
//     bracketed bisection (bracket rule in impl note §1), earlier nodes
//     frozen.
// Validation (throws std::invalid_argument, naming the stable instrument ID
// or rate date):
//   empty future strip; malformed/duplicate IDs; null or non-business
//   valuation date; non-IMM or unordered dates; non-contiguous or overlapping
//   quarters;
//   more than one contract straddling market.as_of.valuation_date (impossible
//   in a contiguous strip — checked anyway); a contract already settled
//   (reference_end <= market.as_of.valuation_date); a straddling contract whose realized
//   window is not fully covered by fixings; fixings supplied that a
//   fully-forward fixture does not need are allowed but validated;
//   non-positive accumulation factor; 1 + tau*R <= 0; duplicate pillar
//   dates; OIS pillar not beyond the last futures pillar; non-finite
//   quotes. This boundary validates programmatically constructed
//   SofrMarketData as well as CSV-loaded data.
// Solver failure (bracket exhausted / max iterations) throws
// std::runtime_error naming the instrument and reporting both attempted
// forward-rate brackets and endpoint residuals. No silent fallback.
class SofrCurveBootstrapper {
public:
    BootstrapResult bootstrap(const SofrMarketData& market) const;

    // Model-implied decimal rate of every calibration instrument off an
    // arbitrary candidate curve — g(theta) in the math note. Order is
    // futures then OIS, matching SofrMarketData. Futures are returned as
    // rates, never IMM prices; the partially accrued contract uses the
    // inverse formula above with A recomputed from market.fixings.
    std::vector<double> model_quotes(const SofrMarketData& market,
                                     const PiecewiseLogLinearCurve& curve) const;
};

}  // namespace irc
```

`bootstrap()` returns only after populating one diagnostic per instrument
(22 for the pinned fixture) and verifying `model_quote - market_quote`
against the calibration tolerance. Failed validation or calibration throws;
a failed curve is never returned. `BootstrapResult::as_of` is copied
unchanged from `SofrMarketData::as_of`, so calibration output retains the
snapshot timestamp without coupling it to the numerical curve class.
Diagnostics preserve stable input IDs so logs, CSV reports, and exceptions do
not depend on vector positions.

### `src/curves/curve_io.hpp` — deterministic output

```cpp
#pragma once
#include "curves/piecewise_log_linear_curve.hpp"

#include <filesystem>
#include <string>

namespace irc {

// Pure serializer implementing the byte contract in §1a. Throws
// std::invalid_argument unless curve.day_counter() is Actual365Fixed, because
// the schema labels its time column t_act365f.
std::string serialize_curve_csv(const PiecewiseLogLinearCurve& curve);

// Writes serialize_curve_csv(curve) to path in binary/truncate mode and checks
// every I/O operation. Throws std::runtime_error naming path on failure.
void write_curve_csv(const PiecewiseLogLinearCurve& curve,
                     const std::filesystem::path& path);

}  // namespace irc
```

Keeping serialization pure makes exact testing independent of the filesystem;
the thin writer is covered separately with a temporary-directory round trip.
Neither function creates missing parent directories implicitly.

### `src/risk/dv01.hpp`

#### §4a Bump semantics — one formula, stated once

For any parameter vector $\boldsymbol\psi$ (quotes $\mathbf q$ or nodes
$\boldsymbol\theta$), all sensitivities use the **central difference with
half-width $h$** (math note §The Two Sensitivity Spaces):

$$
s_m \;=\; \frac{V(\boldsymbol\psi + h\,e_m) - V(\boldsymbol\psi - h\,e_m)}{2h},
\qquad
\mathrm{DV01}_m \;=\; s_m \times 10^{-4}.
$$

- `half_width` parameters below are **$h$, in the bumped parameter's units**
  (rate units for quotes, log-DF units for nodes). The *total* spread is $2h$
  — this is the factor-of-two trap, killed here. The reference implementation
  sets $h_q = \Delta_{\mathrm{bp}} = 10^{-4}$, so
  $\mathrm{DV01}_m \approx [V(\mathbf q+\Delta_{\mathrm{bp}}e_m) -
  V(\mathbf q-\Delta_{\mathrm{bp}}e_m)]/2$ (math note).
- All quote bumps are normalized to **decimal-rate units**. For an OIS,
  `par_rate += h`. For a future, an upward rate bump is stored as
  `price -= 100*h` (math note: $R\pm h \leftrightarrow Q\mp100h$); the
  **full-quarter** quoted rate is bumped — never the remaining-period
  forward.
- **Scenario invariants** (math note §Quote DV01 Convention): published
  `MarketAsOf`, fixings, and the realized accumulation factor are held
  bit-for-bit unchanged in every scenario; each scenario performs a
  **complete re-bootstrap**. Fixing corrections and valuation rolls are
  separate scenario types, not DV01. Dependency-aware partial recalibration
  is a possible later optimization, only after correctness and benchmark
  tests are green.
- Functions named `*_dv01_*` return **DV01 in currency units**
  ($s_m \times 10^{-4}$), already carrying the sign convention
  (upward-oriented: positive when $\partial V/\partial q_m>0$; payer +,
  receiver −). They never substitute a one-sided $V(+1\mathrm{bp})-V$
  calculation for the central formula above.
- `curve_node_sensitivities` returns **raw $s_n$** (currency per unit
  log-DF), *not* DV01 — it feeds the Jacobian solve, which does its own
  $\times 10^{-4}$ at the end.

```cpp
#pragma once
#include "curves/curve_instruments.hpp"
#include "curves/piecewise_log_linear_curve.hpp"
#include "curves/sofr_bootstrapper.hpp"

#include <functional>
#include <vector>

namespace irc {

// PV functional: prices the target portfolio off a candidate curve.
using CurvePricer = std::function<double(const YieldCurve&)>;

// --- Required tier ---------------------------------------------------------

// Raw node sensitivities s_n = dV/d(theta_n), central difference with
// half-width `node_half_width` (log-DF units). Returns M entries for
// storage indices 1..M (anchor never bumped — impl note §2b).
std::vector<double> curve_node_sensitivities(
    const PiecewiseLogLinearCurve& curve, const CurvePricer& pv,
    double node_half_width = 1e-6);

// Direct quote DV01 (currency units, per instrument, order = futures then
// OIS): bump normalized rate quote m by +/- `quote_half_width`, rebuild the
// complete curve in each scenario (MarketAsOf and fixings immutable), reprice,
// central-difference, scale by 1e-4.
// For futures, +h rate means -100*h in the stored IMM price.
std::vector<double> quote_dv01_direct(const SofrMarketData& market,
                                      const SofrCurveBootstrapper& bootstrapper,
                                      const CurvePricer& pv,
                                      double quote_half_width = 1e-4);

// --- In-phase stretch (build only after the direct tier is green) ----------

// Finite-difference calibration Jacobian J^FD_mn = dg_m/d(theta_n), central
// difference, node half-width in log-DF units (math note
// §Finite-Difference Jacobian Cross-Check). Lower-triangular by
// construction; the test asserts the strict upper part ~ 0.
std::vector<std::vector<double>> calibration_jacobian(
    const SofrMarketData& market, const SofrCurveBootstrapper& bootstrapper,
    const PiecewiseLogLinearCurve& curve, double node_half_width = 1e-6);

// Quote DV01 (currency units) via the Jacobian path: solve
// (J^FD)^T s_quote = s_curve by back substitution (the transpose is
// upper-triangular; no inverse is formed), then scale by 1e-4. The validated
// input contract below is part of this public API.
std::vector<double> quote_dv01_via_jacobian(
    const std::vector<std::vector<double>>& jacobian,
    const std::vector<double>& curve_sensitivities);

}  // namespace irc
```

Every half-width must be finite and strictly positive; otherwise the risk
function throws `std::invalid_argument`.

`quote_dv01_via_jacobian` validates before indexing or dividing: $M>0$;
exactly $M$ rows of length $M$; exactly $M$ curve sensitivities; every matrix
and sensitivity entry finite; and lower-triangular structure, with every
strict-upper entry satisfying $|J_{ij}|\le10^{-10}$ for $j>i$. A diagonal
pivot is numerically singular when

$$
|J_{ii}| \le
100\,\epsilon_{\mathrm{mach}}
\max\!\left(1,\max_{r=i,\dots,M-1}|J_{ri}|\right),
$$

where the inner maximum is the scale of row $i$ of $J^\top$. Any shape,
finiteness, triangularity, or pivot violation throws `std::invalid_argument`
with the offending row/column. A non-finite intermediate or output during
back substitution throws `std::runtime_error`.

(Plain `vector<vector<double>>` + hand-written back substitution keeps the
linear algebra auditable; Eigen — already in vcpkg.json — is a permitted
implementation choice if the owner prefers.)

### Examples

- `examples/02_sofr_curve_bootstrap.cpp <quotes.csv> <fixings.csv> <output.csv>`
  — own engine end-to-end: load both explicit input paths, bootstrap, write
  the explicit output path with `write_curve_csv`, and print calibration
  diagnostics plus direct DV01 for a 10Y payer swap.
- `examples/02_quantlib_sofr_curve_bootstrap.cpp <quotes.csv> <fixings.csv>`
  — QuantLib twin: same inputs through `SofrFutureRateHelper` +
  `OISRateHelper` → `PiecewiseYieldCurve<Discount, LogLinear>`, loading the
  identical fixing history into the `Sofr` index **before** constructing the
  helpers, printing benchmark diagnostics.

Both executables reject missing/extra arguments with a usage message and a
nonzero exit code. Every file path is an explicit argument; neither
executable infers the repository root from the process working directory.

## 4. Symbol → code map

| Math note | Code |
|-----------|------|
| $x_m,\ \theta_m$ ($m=1..M$) | `curve.nodes()[m].log_discount` (storage; index 0 = anchor) |
| $L_m$ | `curve.nodes()[m].date` |
| $t_m$ | `curve.day_counter().yearFraction(curve.reference_date(), L_m)` |
| $P^{\mathrm{SOFR}}(0,T)$ | `curve.discount(T)` |
| $A^{\mathrm{SOFR}}(T_{s,1},0)$ | `realized_accumulation(market.fixings, T_s1, market.as_of.valuation_date, calendar)` |
| $R_{\mathrm{model},1}$ (inverse first-future quote) | `model_quotes(...)[0]` |
| $g_m(\boldsymbol\theta)$ | `bootstrapper.model_quotes(market, curve)[m-1]` |
| $\epsilon_m$ | `BootstrapResult::diagnostics[m-1].residual` |
| $\mathbf s^{\mathrm{curve}}$ (raw $s_n$) | `curve_node_sensitivities(...)` |
| $\mathrm{DV01}^{\uparrow}_m$ (currency) | `quote_dv01_direct(...)` |
| $\mathcal J^{\mathrm{FD}}$ | `calibration_jacobian(...)` *(stretch)* |
| solve $(\mathcal J^{\mathrm{FD}})^\top\mathbf s^{q}=\mathbf s^{c}$ | `quote_dv01_via_jacobian(...)` *(stretch)* |

## 5. Test plan — `tests/test_curve_bootstrap.cpp` (red first)

Stubs: constructors and validation work; every computational method throws
`std::logic_error("... not implemented (Phase 2 step 4)")`. Owner implements
to green, group by group (AGENTS step 4). All market-data tests load the
pinned files of §1a — no duplicated hardcoded market quotes. Math-note tests
1–8 are incorporated below (references in parentheses).

Before Phase 2 code relies on the Phase 1 pricer, add the finite-input
regression tests specified in §2a to `tests/test_mini_pricer.cpp`. They are
part of the red-first workflow even though they live in the existing test
target. The `CouponPeriod` construction and payment-lag leg tests (group I)
also live in `tests/test_mini_pricer.cpp`, since they extend the Phase 1
rates layer.

**A. Finance-free numerics**

*Interpolator — the interview kata is the fixture:*

1. Knots `xs={1,2,3}`, `ys={10,100, 20,200, 30,300}` (m=2), queries
   `{1.5,2.5}` → `{15,150, 25,250}` exactly.
2. Flat extrapolation: query 0.5 → `{10,100}`; query 9 → `{30,300}`.
3. Unsorted queries return per-query rows in input order.
4. Validation throws: empty xs; non-increasing xs; `ys.size() % xs.size() != 0`;
   NaN in xs, ys, or a query.
5. m=1 scalar case round-trips knot values exactly.

*Bracketed bisection — CPP Ch.9 function-object contract:*

6. A lambda capturing `target=2.0`, with residual $x^2-\text{target}$ on
   `[0,2]`, returns $\sqrt2$ and a residual within `1e-12`; its reported
   midpoint-iteration count is positive and at most 200. An exact endpoint
   root returns without a midpoint iteration.
7. Validation/failure behavior: non-finite or unordered bounds, non-positive
   or non-finite tolerance, zero iteration limit, an unbracketed residual, a
   non-finite callable result, and deliberate iteration exhaustion each throw
   the specified exception type with numerical context.

**B. Curve (known nodes, no bootstrap)**

8. Constructor accepts calibrated pillars strictly after reference, prepends
   exactly one anchor, and rejects a caller-supplied reference-date pillar.
   `discount(reference) == 1`; node dates reproduce their DFs to 1e-15.
9. Off-node date matches the closed-form log-linear formula to 1e-14.
10. Strictly positive DFs; monotone decreasing for positive-rate nodes.
11. Throws beyond last pillar and before reference.
12. `bump_node(curve, 0, δ)` throws (anchor immutable). Bumping interior node
    1 changes its value and interior discounts on both adjacent segments
    `[L_0,L_1]` and `[L_1,L_2]`; the anchor, node $L_2$, and dates after $L_2$
    remain exactly unchanged.

**C. Fixings + realized accumulation (math tests 1, 2, 4)**

13. **Accumulation golden (math test 2):** synthetic fixings $r_1=0.0400$
    (1 calendar day) and $r_2=0.0410$ (3 calendar days) →
    $A = 1.00045281574074$ within $10^{-12}$ (dimensionless).
    *(Verified numerically: exact to 8.9e-16.)*
14. **Weight rule (math test 4):** a Friday fixing followed by a Monday rate
    date carries a 3-calendar-day weight; missing, duplicate, out-of-range,
    weekend/holiday, and non-finite fixing records each fail explicitly.
15. **SR3Z25 boundary (math test 1):** reference quarter
    $[2025\text{-}12\text{-}17, 2026\text{-}03\text{-}18)$ contains 91
    calendar days ⇒ $\tau_1 = 91/360$; realized window
    $[2025\text{-}12\text{-}17, 2026\text{-}01\text{-}15)$ contains 29
    calendar days; the pinned fixings file has business-day rate dates
    through 2026-01-14, none on 2026-01-15, and weights over the realized
    window sum to 29. *(91/29 verified against the calendar.)*

**D. Market data + bootstrap (math tests 3, 5)**

16. Quote conversion: price 95.75 ↔ rate 0.0425 round-trips; an upward 1bp
    rate move maps to a −0.01 IMM price move.
17. Loader round-trip: pinned files → 13 futures + 9 OIS + 19 fixings +
    `MarketAsOf{2026-01-15, 2026-01-15T20:00:00Z}`. The loader rejects a
    missing, malformed, or non-UTC timestamp and malformed copies with bad
    type, wrong quote unit, non-empty inapplicable field, NaN quote, duplicate
    ID, overlapping quarters, or duplicate tenor. A separate valid fixture
    with a negative OIS rate is accepted. No UTC-date-equality rule is tested.
    A calendar-role regression asserts that 2026-04-03 (Good Friday) is a
    `UnitedStates(Settlement)` business day but not a
    `UnitedStates(SOFR)` fixing day, preventing the two CFTC calendar roles
    from collapsing back into one calendar.
18. **First pillar golden (math test 3):** with constructed inputs
    $A = 1.00045281574074$, $\tau = 91/360$, $R_{\mathrm{fut}} = 0.0430$:
    $P(0,T_e) = A/(1+\tau R) = 0.989695376825414$ within $10^{-12}$; the
    inverse model quote recovers $0.0430$ within $10^{-12}$.
    *(Verified numerically: exact to 4.5e-16.)*
19. Futures sub-curve on the pinned fixture: first pillar via the
    $A/(1+\tau_1 R_1)$ formula with $A$ from the pinned fixings; subsequent
    nodes via telescoped products — agree with hand computation to 1e-12.
20. Full-curve repricing (math test 5): `BootstrapResult::as_of` exactly
    equals the input snapshot; every `CalibrationDiagnostic` preserves its
    stable input ID, stores quotes in decimal-rate units, and satisfies
    $|\epsilon_m| \le 10^{-10}$; exactly **22** diagnostics in futures-then-OIS
    order; storage count = 1 (anchor) + 13 + 9 = 23; node dates strictly
    increasing; every in-domain DF finite and positive.
21. Explicit failures (math test 5) use contextual messages naming the stable
    instrument ID or rate date:
    a. empty futures strip, non-business valuation date, or non-IMM future
       date → throw;
    b. straddling contract with incomplete fixing coverage → throw (never
       projected/filled);
    c. already-settled contract
       (`reference_end <= as_of.valuation_date`) → throw;
    d. non-contiguous futures quarters → throw;
    e. duplicate pillar dates → throw;
    f. NaN/inf quote or fixing, or $1+\tau R \le 0$ → throw;
    g. solver bracket exhausted (constructed pathological quote) → throw
       `std::runtime_error` after the two-stage bracket rule, reporting both
       brackets and endpoint residuals.

**E. Deterministic output**

22. `serialize_curve_csv` for a tiny known-node curve equals one exact golden
    string, including number formatting, blank anchor fields, `\n` endings,
    and the final newline; a non-Act/365F curve is rejected rather than
    mislabeled.
23. `write_curve_csv` round-trips those exact bytes through a test-owned
    temporary directory and throws when its parent directory does not exist.

**F. QuantLib oracle (math test 6)**

24. Same pinned files → QL `PiecewiseYieldCurve<Discount, LogLinear>`
    (helpers per math note §QuantLib Benchmark; identical fixing history
    loaded into the `Sofr` index **before** helper construction; empty
    discount handle; `Pillar::LastRelevantDate`). Compare the SR3Z25
    model-implied full-quarter rate, first pillar date and DF, every downstream
    pillar DF, and an off-pillar grid (all futures/OIS accrual + payment dates):
    $\max_T|\delta_P(T)| \le 10^{-8}$. A missing required historical fixing is
    an error, not a warning.
25. A 10Y par swap priced off both curves: the hand-rolled side uses the
    owner-specified payment-lag-capable component from §2a, with accrual and
    payment dates represented separately; assert the two-USNY-business-day
    payment dates directly, then compare PV and par-rate diffs within the math
    note's combined abs/rel tolerance.

**G. DV01 — required tier (math test 7)**

26. Sign: 10Y **payer** swap has **positive** total quote DV01; receiver is
    its negative (math note sign convention).
27. Magnitude sanity: total DV01 within ±30% of `annuity × N × 1e-4`
    computed off the same curve.
28. Central-difference symmetry: DV01 from half-width 1e-4 and 0.5e-4 agree
    to 1e-3 relative (guards the half-vs-full-spread bug); zero, negative,
    NaN, and infinite half-widths throw.
29. Scenario semantics (math test 7): the +$h$ scenario passed to the
    bootstrapper has price $Q-100h$, the −$h$ scenario $Q+100h$;
    `MarketAsOf`, the fixings vector, and the recomputed
    $A^{\mathrm{SOFR}}$ are identical across base and both scenarios; each
    scenario performs a complete re-bootstrap.
30. QL comparison: the same central normalized-rate bumps are applied to the
    QL helpers with the identical unchanged fixing history, every scenario
    rebuilds its complete curve, and totals agree under the math note's
    normalized tolerance.

**H. DV01 — in-phase stretch (Jacobian path; math test 8; after G is green)**

31. Jacobian shape: strict upper triangle of `calibration_jacobian` satisfies
    $|J_{ij}|\le10^{-10}$ for $j>i$; every diagonal satisfies the scaled pivot
    criterion in §4a.
32. Jacobian input validation rejects an empty, non-square, ragged, non-finite,
    dimension-mismatched, materially non-lower-triangular, or numerically
    singular input before indexing or division; messages name the offending
    row/column.
33. **Two-way cross-check:** `quote_dv01_direct` vs
    `quote_dv01_via_jacobian` (both central-difference) agree per entry under
    $|a-b| \le \max(10^{-10}\mathcal N,\ 10^{-6}|a|)$. This cross-validates the
    bootstrap, Jacobian, and back-substitution transform and becomes the
    finite-difference reference for the post-MVP analytic Jacobian.

**I. CouponPeriod + payment-lag legs (`tests/test_mini_pricer.cpp`; red
before the §2a implementation)**

34. `make_coupon_periods` golden over MLK: a USNY-settlement schedule period
    ending Friday 2026-01-16 with a 2-business-day lag pays Wednesday
    2026-01-21 (skipping the weekend and MLK Monday 2026-01-19); lag 0
    returns `payment_date == accrual_end` unchanged; each `year_fraction`
    equals the day counter's accrual over its own
    `[accrual_start, accrual_end]`; periods built from a schedule are
    contiguous.
35. Validation throws: `make_coupon_periods` rejects a one-date schedule, an
    empty day counter, an empty calendar, and a negative payment lag; the
    period-based leg constructors reject empty periods, null dates,
    `accrual_start >= accrual_end`, `payment_date < accrual_end`, non-finite
    or non-positive year fractions, overlapping periods, and non-finite
    notional/rate/spread.
36. Delegation + lag effect: legs built from the Phase 1 constructors price
    identically (PV and annuity within 1e-12) to legs built from the
    equivalent zero-lag periods; the same fixed leg with a 2-USNY-day payment
    lag has a strictly smaller PV on a positive flat curve (later payments
    discount more).

## 6. CMake wiring

```cmake
# extend the existing library
target_sources(irc_pricing PRIVATE
  src/rates/coupon_period.cpp
  src/core/linear_interpolator.cpp
  src/curves/curve_instruments.cpp
  src/curves/piecewise_log_linear_curve.cpp
  src/curves/market_data_io.cpp
  src/curves/sofr_bootstrapper.cpp
  src/curves/curve_io.cpp
  src/risk/dv01.cpp)

add_executable(02_sofr_curve_bootstrap examples/02_sofr_curve_bootstrap.cpp)
target_link_libraries(02_sofr_curve_bootstrap PRIVATE irc_pricing)

add_executable(02_quantlib_sofr_curve_bootstrap
  examples/02_quantlib_sofr_curve_bootstrap.cpp)
target_link_libraries(02_quantlib_sofr_curve_bootstrap PRIVATE
  irc_pricing QuantLib::QuantLib)

add_executable(test_curve_bootstrap tests/test_curve_bootstrap.cpp)
target_link_libraries(test_curve_bootstrap PRIVATE irc_pricing GTest::gtest_main)
list(APPEND IRC_TEST_TARGETS test_curve_bootstrap)

set(PHASE2_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_data")
file(MAKE_DIRECTORY "${PHASE2_TEST_DATA_DIR}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/data/market/sofr_quotes_2026-01-15.csv"
  "${PHASE2_TEST_DATA_DIR}/sofr_quotes_2026-01-15.csv"
  COPYONLY)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/data/market/sofr_fixings_2025-12-17_2026-01-14.csv"
  "${PHASE2_TEST_DATA_DIR}/sofr_fixings_2025-12-17_2026-01-14.csv"
  COPYONLY)
target_compile_definitions(test_curve_bootstrap PRIVATE
  IRC_TEST_DATA_DIR=\"${PHASE2_TEST_DATA_DIR}\")

gtest_discover_tests(test_curve_bootstrap)
```

Tests form fixture paths from `IRC_TEST_DATA_DIR`; CMake supplies an absolute
forward-slash path and copies the immutable source fixtures at configure
time. All example file paths are explicit command-line arguments. No code
searches parent directories or assumes a working directory.
`IRC_TEST_TARGETS` is also the dependency list for the MSVC `coverage` target;
every future CTest executable must be appended when it is added.

## 7. Build order (AGENTS workflow)

The math-note, interface-approval, and fixing-data gates are complete. The
remaining order is:

1. **Completed (2026-07-17):** AI materialized the approved headers,
   validation-only stubs, `tests/test_curve_bootstrap.cpp`, the Phase 1
   finite-input regression tests, and the pinned quotes CSV. The MSVC Release
   build succeeds; 51 tests are discovered, with 17 validation/regression
   tests green and 34 implementation tests intentionally red.
2. **Current — owner implements**, suggested order: Phase 1 finite-input hardening →
   bracketed bisection → interpolator → curve → fixings + accumulation →
   loader → futures leg (including SR3Z25) → the approved `CouponPeriod` and
   payment-lag-capable legs from §2a → OIS pricing → deterministic output →
   direct DV01, green through group G. Each component's red tests must exist
   before its implementation.
3. AI reviews the green implementation, then shows its own diff.
4. **Stretch:** owner implements the Jacobian path (group H), with the now-
   trusted direct DV01 as its reference.
5. AI runs the QuantLib comparison and reports diffs.
6. Owner commits on `phase-2-curves`; tag `v0.3-curve-dv01` when G is green
   (H is not gating).

## 8. Deferred (tracked, not forgotten)

- **Production short-end instrument set:** add overnight and short-dated OIS
  quotes between valuation and the first pillar so the front segment is
  market-constrained rather than implied by one partially accrued future +
  interpolation. Phase 2 deliberately keeps the smaller futures/OIS exercise.
- **Fixing corrections / valuation-date rolls / publication updates** as
  scenario types (math note §Quote DV01 Convention item 5 — explicitly not
  DV01).
- **Analytic Jacobian** (roadmap: post-MVP; test 33's bumped version is its
  finite-difference reference).
- Futures convexity adjustment (needs a vol model — Phase 6+).
- Curve extrapolation policy beyond the last pillar (throws for now).
- Trade-file CSV input for portfolios (Phase 3 scope; Phase 2's CSVs are
  market quotes and fixings only).
