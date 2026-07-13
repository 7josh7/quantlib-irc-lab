# 02 — SOFR curve bootstrap + DV01: implementation contract (Phase 2)

> **Not a math note.** The math lives in
> [`docs/math_notes/02_curve_bootstrapping.md`](../math_notes/02_curve_bootstrapping.md);
> this file is the code-facing spec: conventions pinned, architecture decided,
> interfaces proposed, tests planned. Per [AGENTS.md](../../AGENTS.md)
> workflow step 2, the headers below are a **proposal for owner approval**.
> Per step 4, the owner implements; AI writes the red tests and stubs.

## 0. Scope

Build a real USD SOFR discount curve from SR3 futures + OIS par quotes
(Source II Table 1 calibration set), then compute first-order risk:
curve-node sensitivities, quote-level DV01 (direct re-bootstrap), and the
Jacobian transformation between them, cross-checked both ways.

Out of scope (per math note / roadmap): futures convexity adjustment,
multi-curve basis, xCcy collateral, analytic (closed-form) Jacobian.
All derivatives in Phase 2 are finite-difference.

## 1. Conventions (pinned)

| Item | Value (Phase 2) | Notes |
|------|-----------------|-------|
| Valuation date $t_0$ | 2026-05-23 | continuity with Phases 0–1 |
| Calendar | `UnitedStates(SOFR)` | matches QL `Sofr` index → clean oracle |
| Curve day counter | Act/365F | node times $t_m$; math note §Interpolation |
| Leg / futures accrual day counter | Act/360 | $\tau_i,\alpha_j,\tau_m$ |
| Futures set | SR3H26 … SR3Z28 (12 quarterly IMM contracts) | front-to-belly |
| OIS set | 4Y 5Y 7Y 10Y 12Y 15Y 20Y 25Y 30Y, spot-start (+2bd), annual legs both sides | belly-to-long |
| OIS payment delay | 2 business days after accrual end | $U_i, V_j$ |
| Business-day convention | ModifiedFollowing (OIS); CME spec dates (futures) | |
| Curve state | $x_m = \log P(0, L_m)$ at pillar dates | |
| Anchor node | $(t{=}0,\ \log P{=}0)$ prepended | $P(0,0)=1$ |
| Pillar rule | futures: reference-quarter end; OIS: last payment date (incl. delay) | `Pillar::LastRelevantDate` on the QL side |
| Interpolation | linear in $\log P$ vs Act/365F time | constant IFR between nodes |
| Extrapolation | **curve throws** beyond last pillar (and before reference) | see §2 note |
| First future | SR3H26 is mid-quarter on $t_0$ ⇒ realized-fixings accumulation $A^{\mathrm{SOFR}}$ required | math note Eq. for extended bond price |
| Root solver | hand-rolled bracketed bisection on $\epsilon_m(x_m)$, tol $10^{-12}$ on rate, max 200 iters | simplest auditable choice; QL `Brent` acceptable later |
| Bump size | $\Delta_{\mathrm{bp}} = 10^{-4}$ quotes; $h = 10^{-6}$ node (log-DF) bumps | central differences |
| DV01 sign | $V(+1\mathrm{bp}) - V$ (signed; payer +, receiver −) | math note §DV01 sign convention |

**Market data:** synthetic quotes modeled on Source II Table 1, hardcoded in
the test fixture / example as arrays (CSV loading is Phase 4 scope).
**TODO(owner):** transcribe the exact Table 1 numbers — AI must not invent
quotes (AGENTS rule 6). Realized SOFR fixings for the SR3H26 partial quarter:
synthetic constant fixing (document the value in the fixture).

## 2. Architecture — three objects, one direction of dependency

```
market quotes (+ realized fixings)
        ↓
SofrCurveBootstrapper        (calibration: instruments → nodes)
        ↓
PiecewiseLogLinearCurve      (representation: nodes → discount factors)
        ↓
LinearFlatInterpolator       (numerics: knots → interpolated values)
```

Decisions (from design discussion, owner + Codex + Claude aligned):

- **The interpolator is finance-independent.** It knows knot arrays and
  queries — not dates, discount factors, or instruments. It must not take a
  curve as input (circular dependency; untestable in isolation).
- **The bootstrapper is not a curve method.** `bootstrap()` inside the curve
  would couple a numerical term structure to futures/OIS conventions, root
  solvers, and quote validation — and make it impossible to construct a curve
  directly from known nodes in unit tests.
- **Vectorized = batch API, not SIMD.** `evaluate()` accepts many query
  points in one call and returns one flat vector (interview-standard shape).
  Complexity target: binary search per query, $O(n\log k + nm)$.
- **Extrapolation split:** the *generic interpolator* supports flat
  extrapolation (clamp to end values — the interview spec). The *curve*
  rejects queries outside `[reference, last pillar]` instead of delegating,
  because flat extrapolation of log-DF implies a zero forward rate beyond the
  last node, which is financially wrong. The curve enforces its domain, then
  calls the interpolator.

## 3. Proposed interfaces (headers only — owner implements)

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

Constructor validates (throws `std::invalid_argument`): empty `xs`;
non-strictly-increasing `xs`; `ys.size()` not a positive multiple of
`xs.size()`.

### `src/curves/piecewise_log_linear_curve.hpp`

```cpp
#pragma once
#include "core/yield_curve.hpp"

#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>

#include <span>
#include <vector>

namespace irc {

// Discount curve stored as (pillar date, log P(0, pillar)) nodes,
// interpolated linearly in log-DF vs Act/365F time (math note
// §Interpolation). Constant instantaneous forward between nodes.
// Throws on queries before reference or beyond the last pillar —
// no financial extrapolation (impl note §2).
class PiecewiseLogLinearCurve final : public YieldCurve {
public:
    PiecewiseLogLinearCurve(QuantLib::Date reference,
                            std::vector<QuantLib::Date> node_dates,
                            std::vector<double> node_log_discounts,
                            QuantLib::DayCounter day_counter);

    double discount(const QuantLib::Date& d) const override;
    QuantLib::Date reference_date() const override;

    // Batch query (vectorized path used by pricers and risk).
    std::vector<double> discounts(std::span<const QuantLib::Date> dates) const;

    // Node access for risk bumps and diagnostics (dates aligned with values).
    const std::vector<QuantLib::Date>& node_dates() const;
    const std::vector<double>& node_log_discounts() const;

private:
    QuantLib::Date reference_;
    std::vector<QuantLib::Date> node_dates_;
    std::vector<double> node_log_discounts_;
    QuantLib::DayCounter day_counter_;
    // impl detail: internally builds a LinearFlatInterpolator over
    // (Act/365F times, log-DFs); domain enforcement happens here.
};

// Free function (not a member — keeps the class at 5 public methods):
// a copy of `curve` with node i bumped by delta in log-DF space.
PiecewiseLogLinearCurve bump_node(const PiecewiseLogLinearCurve& curve,
                                  std::size_t node_index, double delta);

}  // namespace irc
```

Constructor validates: reference not null; nodes non-empty; dates strictly
increasing and ≥ reference; sizes match; day counter non-empty. If the first
node is not the reference date itself, the anchor $(t{=}0, \log P{=}0)$ is
prepended by the constructor (so callers may pass calibrated pillars only).

### `src/curves/curve_instruments.hpp` — calibration inputs

```cpp
#pragma once
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>

#include <map>
#include <vector>

namespace irc {

// One CME 3M SOFR future (SR3): IMM reference quarter + quoted price.
struct SofrFutureQuote {
    QuantLib::Date reference_start;  // IMM quarter start
    QuantLib::Date reference_end;    // IMM quarter end (= pillar)
    double price;                    // Q_m, e.g. 95.83
};

// One spot-starting USD SOFR OIS par quote.
struct OisQuote {
    QuantLib::Period tenor;  // 4Y ... 30Y
    double par_rate;         // S_m, decimal
};

// Everything the bootstrapper consumes. Realized fixings cover the
// partially elapsed first futures quarter (math note: A^SOFR factor).
struct SofrMarketData {
    QuantLib::Date valuation;
    std::vector<SofrFutureQuote> futures;   // sorted by reference_start
    std::vector<OisQuote> ois;              // sorted by tenor
    std::map<QuantLib::Date, double> realized_fixings;
};

}  // namespace irc
```

### `src/curves/sofr_bootstrapper.hpp`

```cpp
#pragma once
#include "curves/curve_instruments.hpp"
#include "curves/piecewise_log_linear_curve.hpp"

namespace irc {

// Sequential bootstrap per math note §Calibration:
//  1. anchor P(0,0)=1;
//  2. futures: P(0,T_e) = P_fwd * P(0,T_s), first contract via realized
//     accumulation A^SOFR when T_s < 0;
//  3. OIS: for each quote solve eps_m(x_m) = K_model,m(x_m) - S_m = 0
//     by bracketed bisection, all earlier nodes frozen.
// Throws std::runtime_error with the instrument id on bracketing/convergence
// failure (AGENTS rule 7: no silent fallback).
class SofrCurveBootstrapper {
public:
    PiecewiseLogLinearCurve bootstrap(const SofrMarketData& market) const;

    // Model-implied quote of every calibration instrument off an arbitrary
    // candidate curve — g(theta) in the math note. Needed by the Jacobian
    // and by repricing diagnostics, hence public.
    std::vector<double> model_quotes(const SofrMarketData& market,
                                     const PiecewiseLogLinearCurve& curve) const;
};

}  // namespace irc
```

### `src/risk/dv01.hpp`

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

// s_curve: central-difference node sensitivities dV/d(theta_n), n = 1..M.
std::vector<double> curve_node_sensitivities(
    const PiecewiseLogLinearCurve& curve, const CurvePricer& pv,
    double node_bump = 1e-6);

// Direct quote DV01: bump quote m by +/- 0.5bp, re-bootstrap (only nodes
// >= m need re-solving — J is lower-triangular), reprice, central-difference,
// scale to 1bp. Sign: V(+1bp) - V (math note convention).
std::vector<double> quote_dv01_direct(const SofrMarketData& market,
                                      const SofrCurveBootstrapper& bootstrapper,
                                      const CurvePricer& pv,
                                      double bump = 1e-4);

// Finite-difference calibration Jacobian J_mn = dg_m/d(theta_n)
// (lower-triangular by construction; test asserts the upper part ~ 0).
std::vector<std::vector<double>> calibration_jacobian(
    const SofrMarketData& market, const SofrCurveBootstrapper& bootstrapper,
    const PiecewiseLogLinearCurve& curve, double node_bump = 1e-6);

// Quote DV01 via the Jacobian path: solve J^T s_quote = s_curve by back
// substitution (J^T is upper-triangular; no inverse is formed), scale to 1bp.
std::vector<double> quote_dv01_via_jacobian(
    const std::vector<std::vector<double>>& jacobian,
    const std::vector<double>& curve_sensitivities, double bump = 1e-4);

}  // namespace irc
```

(Plain `vector<vector<double>>` + hand-written back substitution keeps the
linear algebra auditable; switching to Eigen — already in vcpkg.json — is a
permitted implementation choice if the owner prefers.)

### Examples

- `examples/02_sofr_curve_bootstrap.cpp` — own engine end-to-end: build the
  curve, print pillar DFs/zero times, repricing residuals, and the DV01
  report (direct vs Jacobian) for a 10Y payer swap.
- `examples/02_quantlib_sofr_curve_bootstrap.cpp` — the roadmap-named
  QuantLib twin: same quotes through `SofrFutureRateHelper` +
  `OISRateHelper` → `PiecewiseYieldCurve<Discount, LogLinear>`.

## 4. Symbol → code map

| Math note | Code |
|-----------|------|
| $x_m,\ \theta_m$ | `curve.node_log_discounts()[m]` |
| $L_m$ | `curve.node_dates()[m]` |
| $t_m$ | `day_counter.yearFraction(reference, L_m)` |
| $P^{\mathrm{SOFR}}(0,T)$ | `curve.discount(T)` / `curve.discounts(dates)` |
| $A^{\mathrm{SOFR}}(T_s,0)$ | product over `market.realized_fixings` |
| $g_m(\boldsymbol\theta)$ | `bootstrapper.model_quotes(market, curve)[m]` |
| $\epsilon_m$ | `model_quotes(...)[m] - quote[m]` |
| $\mathbf s^{\mathrm{curve}}$ | `curve_node_sensitivities(...)` |
| $\mathbf s^{\mathrm{quote}}$ | `quote_dv01_direct(...)` (× $1/\Delta_{\mathrm{bp}}$) |
| $\mathcal J$ | `calibration_jacobian(...)` |
| solve $\mathcal J^\top\mathbf s^{q}=\mathbf s^{c}$ | `quote_dv01_via_jacobian(...)` |

## 5. Test plan — `tests/test_curve_bootstrap.cpp` (red first)

Stubs: constructors validate and work; every computational method throws
`std::logic_error("... not implemented (Phase 2 step 4)")`. Owner implements
to green (AGENTS step 4).

**A. Interpolator (finance-free — the interview kata is the fixture)**
1. Knots `xs={1,2,3}`, `ys={10,100, 20,200, 30,300}` (m=2), queries
   `{1.5,2.5}` → `{15,150, 25,250}` exactly.
2. Flat extrapolation: query 0.5 → `{10,100}`; query 9 → `{30,300}`.
3. Unsorted queries return per-query rows in input order.
4. Validation throws: empty xs; non-increasing xs; `ys.size() % xs.size() != 0`.
5. m=1 scalar case round-trips knot values exactly.

**B. Curve (known nodes, no bootstrap)**
6. `discount(reference) == 1` (anchor); node dates reproduce their DFs to 1e-15.
7. Off-node date matches the closed-form log-linear formula to 1e-14.
8. Strictly positive DFs; monotone decreasing for positive-rate nodes.
9. Throws beyond last pillar and before reference (`std::invalid_argument`
   or documented exception type).
10. `discounts(batch)` equals scalar `discount` per element to 1e-16.

**C. Bootstrap**
11. Futures-only sub-curve: each SR3 node equals the hand-telescoped
    product $\prod (1+\tau R_{\mathrm{fut}})^{-1}$ (with the first contract's
    realized-fixings factor $A^{\mathrm{SOFR}}$) to 1e-12.
12. Full curve repricing: `model_quotes` minus market quotes —
    $\max_m|\epsilon_m| \le 10^{-10}$ (math note tolerance).
13. Node dates strictly increasing; pillar count = #instruments (+ anchor).
14. Failure is explicit: an unbracketable OIS quote (e.g. 50%) throws with
    the instrument identifier in the message.

**D. QuantLib oracle**
15. Same quotes → QL `PiecewiseYieldCurve<Discount, LogLinear>`
    (helpers per math note §QuantLib Benchmark; empty discount handle;
    `Pillar::LastRelevantDate`; fixings loaded into `Sofr` index).
    Pillar DFs and an off-pillar grid (all futures/OIS accrual + payment
    dates): $\max_T|\delta_P(T)| \le 10^{-8}$.
16. A 10Y par swap priced off both curves: PV and par-rate diffs within the
    math note's combined abs/rel DV01-style tolerance.

**E. DV01 (the phase's risk deliverable)**
17. Sign: 10Y **payer** swap has **positive** total quote DV01; receiver
    the negative (math note sign convention).
18. Magnitude sanity: total DV01 within ±30% of `annuity × N × 1bp`
    computed off the same curve.
19. Jacobian shape: strictly-upper entries of `calibration_jacobian` are
    ~0 (≤ 1e-10 in rate units); diagonal entries all nonzero.
20. **Two-way cross-check (the load-bearing test):**
    `quote_dv01_direct` vs `quote_dv01_via_jacobian` (both central-
    difference) agree per entry under
    $|a-b| \le \max(10^{-10}\mathcal N,\ 10^{-6}|a|)$ — the math note's
    combined tolerance. This cross-validates bootstrap, Jacobian, and the
    back-substitution transform in one assertion.
21. QL DV01 comparison: same +1bp quote bump applied to the QL helpers,
    rebuild, reprice — compare with `quote_dv01_direct` total under the
    math note's normalized tolerance.

## 6. CMake wiring

```cmake
# extend the existing library
target_sources(irc_pricing PRIVATE
  src/core/linear_interpolator.cpp
  src/curves/piecewise_log_linear_curve.cpp
  src/curves/sofr_bootstrapper.cpp
  src/risk/dv01.cpp)

add_executable(02_sofr_curve_bootstrap examples/02_sofr_curve_bootstrap.cpp)
target_link_libraries(02_sofr_curve_bootstrap PRIVATE irc_pricing)

add_executable(02_quantlib_sofr_curve_bootstrap
  examples/02_quantlib_sofr_curve_bootstrap.cpp)
target_link_libraries(02_quantlib_sofr_curve_bootstrap PRIVATE QuantLib::QuantLib)

add_executable(test_curve_bootstrap tests/test_curve_bootstrap.cpp)
target_link_libraries(test_curve_bootstrap PRIVATE irc_pricing GTest::gtest_main)
gtest_discover_tests(test_curve_bootstrap)
```

## 7. Build order (AGENTS workflow)

1. **(this doc)** owner approves interfaces — or edits them here first.
2. AI materializes headers exactly as approved + validation-only stubs +
   `tests/test_curve_bootstrap.cpp`; suite builds and runs **red**.
   Owner transcribes Table 1 quotes into the fixture (TODO in §1).
3. **Owner implements** (suggested order: interpolator → curve →
   futures leg of the bootstrap → OIS leg → DV01 direct → Jacobian path),
   going green group by group (A → B → C → D → E).
4. AI reviews the green implementation, then shows its own diff.
5. AI runs the QuantLib comparison and reports diffs.
6. Owner commits on `phase-2-curves`; tag `v0.3-curve-dv01` when E is green.

## 8. Deferred (tracked, not forgotten)

- Analytic Jacobian (roadmap: post-MVP; test 20's bumped version becomes
  its finite-difference reference).
- Futures convexity adjustment (needs a vol model — Phase 6+).
- CSV market-data loading (Phase 4 example scope).
- Curve extrapolation policy beyond the last pillar (throws for now).
