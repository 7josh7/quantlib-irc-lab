# 02 — SOFR curve bootstrap + DV01: implementation contract (Phase 2)

> **Not a math note.** The math lives in
> [`docs/math_notes/02_curve_bootstrapping.md`](../math_notes/02_curve_bootstrapping.md);
> this file is the code-facing spec: conventions pinned, architecture decided,
> interfaces proposed, tests planned. Per [AGENTS.md](../../AGENTS.md)
> workflow step 2, the headers below are a **proposal for owner approval**.
> Per step 4, the owner implements; AI writes the red tests and stubs.
>
> Revision 3 — incorporates the 2026-07-13 interface review: consistent market
> snapshot, normalized quote units, diagnostic bootstrap result, stable
> instrument IDs, one anchor-input convention, deterministic CSV I/O, explicit
> solver failures, and Jacobian sequencing.

## 0. Scope

Build an auditable USD SOFR single-curve prototype from SR3 futures + OIS par
quotes, then compute first-order risk. Two tiers:

- **Required (gates `v0.3-curve-dv01`):** bootstrap, repricing diagnostics,
  QuantLib benchmark, **direct quote-level DV01** (bump quote →
  re-bootstrap → reprice), pinned quote file in, `output/curve.csv` out.
- **In-phase stretch (after the required tier is green):** the
  finite-difference calibration Jacobian and the two-way DV01 cross-check.
  This was pulled forward from post-MVP by owner decision; the roadmap's
  ordering constraint is respected — the Jacobian path is built only *after*
  the direct bump implementation it is checked against is trusted. The
  **analytic** (closed-form) Jacobian remains post-MVP.

Out of scope (per math note / roadmap): futures convexity adjustment,
multi-curve basis, xCcy collateral, **partially accrued futures contracts**
(the math note's extended-bond-price / realized-accumulation treatment is
kept as documented math; pricing support and its test are deferred — see §8).
All derivatives in Phase 2 are finite-difference.

> **Owner math-note gate:** the pinned snapshot makes the first future fully
> forward, with $0<T_{s,1}<T_{e,1}$. The current owner math note does not yet
> derive how that contract determines the first pillar when its start-date DF
> is inside the first interpolation segment. The owner must add that derivation
> and identify the front-stub interpolation assumption before headers or red
> tests are materialized. This implementation note does not replace that step.
> The owner must also update the math note's generic "Add Jacobian market-risk
> representation" future-extension bullet to distinguish the Phase 2 finite-
> difference stretch from the deferred analytic Jacobian.

## 1. Conventions (pinned)

| Item | Value (Phase 2) | Notes |
|------|-----------------|-------|
| Valuation date $t_0$ | **2026-01-15 (Thursday, NYC business day)** | synthetic snapshot date; strictly before SR3H26's reference quarter starts (2026-03-18), so **every contract is fully forward — no realized fixings anywhere in the MVP** |
| Snapshot provenance | **Synthetic, deterministic, clearly labeled** — modeled on the *shape* of Source II Table 1, not a transcription | roadmap requires a deterministic synthetic quote file; owner may later substitute the true Table 1 observation date + quotes **together** (never mixing their strip with another date) |
| Calendar | `UnitedStates(SOFR)` | matches QL `Sofr` index → clean oracle |
| Curve day counter | Act/365F | node times $t_m$; math note §Interpolation |
| Leg / futures accrual day counter | Act/360 | $\tau_i,\alpha_j,\tau_m$ |
| Futures set | SR3H26 … SR3Z28 (12 contiguous quarterly IMM contracts) | reference quarters chain end-to-start |
| OIS set | 4Y 5Y 7Y 10Y 12Y 15Y 20Y 25Y 30Y, spot-start (+2bd), annual legs both sides | spot from 2026-01-15 is 2026-01-20 (MLK Monday 2026-01-19 is skipped — deliberate calendar exercise) |
| OIS payment delay | 2 business days after accrual end | $U_i, V_j$ |
| Business-day convention | ModifiedFollowing (OIS); IMM dates used as-given (futures) | |
| Curve state | $x_m = \log P(0, L_m)$ at pillar dates | |
| Anchor node | storage index 0: $(t{=}0,\ \log P{=}0)$, **immutable** | see §2a indexing contract |
| Front stub | between anchor and the first futures pillar the Phase 2 log-linear rule implies a constant forward $f_1$ on $[0,T_{e,1}]$; the proposed closed form is $f_1=\ln(1+\tau_1R_{\mathrm{fut},1})/(t_{e,1}-t_{s,1})$ | **owner gate:** this closure rule must be derived and accepted in the math note; production curves normally constrain the short end with additional short-dated instruments |
| Pillar rule | futures: reference-quarter end; OIS: last payment date (incl. delay) | `Pillar::LastRelevantDate` on the QL side |
| Interpolation | linear in $\log P$ vs Act/365F time | constant IFR between nodes |
| Extrapolation | **curve throws** beyond last pillar and before reference | see §2 |
| Root solver | bracketed bisection on $\epsilon_m$, tol $10^{-12}$ (rate units), max 200 iterations; bracket rule below | hand-rolled, auditable |
| Solver bracket | bracket the segment forward rate $f_m \in [-10\%, +50\%]$ (mapped to $x_m = x_{m-1} - f_m\,(t_m - t_{m-1})$); if $\epsilon_m$ does not change sign, expand once to $[-50\%, +100\%]$; if still unbracketed, **throw** with the instrument id | explicit two-stage rule; no silent fallback |
| Bump semantics | see §4a — one formula, stated once | fixes the ±0.5bp vs 1e-4 ambiguity |
| DV01 sign | $V(+1\mathrm{bp}) - V$ (signed; payer +, receiver −) | math note §DV01 sign convention |

## 1a. Pinned market data — `data/market/sofr_quotes_2026-01-15.csv`

Single source of truth for tests **and** the example (both read this file;
nothing is hardcoded twice). The quotes are synthetic and deterministic, with
smoothly declining futures rates and a gently inverted long end. They are
accepted as calibration data only when both bootstrap and QuantLib benchmark
tests pass; the prose does not assert arbitrage-freedom in advance:

```csv
# Synthetic SOFR market snapshot — deterministic test fixture.
# Modeled on the shape of Source II Table 1; NOT a transcription.
# valuation row first; quote_unit is explicit at the persistence boundary.
type,id,valuation_date,start,end,quote,quote_unit
valuation,,2026-01-15,,,,
future,SR3H26,,2026-03-18,2026-06-17,95.80,imm_price
future,SR3M26,,2026-06-17,2026-09-16,95.85,imm_price
future,SR3U26,,2026-09-16,2026-12-16,95.90,imm_price
future,SR3Z26,,2026-12-16,2027-03-17,95.95,imm_price
future,SR3H27,,2027-03-17,2027-06-16,96.00,imm_price
future,SR3M27,,2027-06-16,2027-09-15,96.03,imm_price
future,SR3U27,,2027-09-15,2027-12-15,96.06,imm_price
future,SR3Z27,,2027-12-15,2028-03-15,96.09,imm_price
future,SR3H28,,2028-03-15,2028-06-21,96.12,imm_price
future,SR3M28,,2028-06-21,2028-09-20,96.15,imm_price
future,SR3U28,,2028-09-20,2028-12-20,96.18,imm_price
future,SR3Z28,,2028-12-20,2029-03-21,96.20,imm_price
ois,4Y,,,,0.0375,decimal_rate
ois,5Y,,,,0.0370,decimal_rate
ois,7Y,,,,0.0360,decimal_rate
ois,10Y,,,,0.0350,decimal_rate
ois,12Y,,,,0.0345,decimal_rate
ois,15Y,,,,0.0340,decimal_rate
ois,20Y,,,,0.0335,decimal_rate
ois,25Y,,,,0.0332,decimal_rate
ois,30Y,,,,0.0330,decimal_rate
```

(IMM third-Wednesday dates verified by hand for 2026–2029; each contract's
end equals the next contract's start, so the strip telescopes.)

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

```
pinned quote CSV
        ↓  (market_data_io)
SofrMarketData
        ↓
SofrCurveBootstrapper        (calibration: instruments → nodes)
        ↓
PiecewiseLogLinearCurve      (representation: nodes → discount factors)
        ├──→ LinearFlatInterpolator  (numerics: knots → interpolated values)
        └──→ curve_io                (deterministic reporting only)
```

Proposed decisions for owner approval:

- **The interpolator is finance-independent.** Knot arrays and queries only —
  no dates, discount factors, or instruments. It never takes a curve
  (circular dependency; untestable in isolation).
- **The bootstrapper is not a curve method.** Calibration coupled into the
  curve would drag futures/OIS conventions, root solvers, and quote
  validation into a numerical term structure — and make it impossible to
  construct a curve from known nodes in unit tests.
- **Vectorized = batch API, not SIMD.** `evaluate()` takes many query points
  in one call, returns one flat vector. Binary search per query,
  $O(n\log k + nm)$.
- **Extrapolation split:** the generic interpolator does flat extrapolation
  (clamp to end values). The curve **rejects** queries outside
  `[reference, last pillar]` — flat log-DF extrapolation would imply a zero
  forward rate, which is financially wrong. The curve enforces its domain,
  then delegates.

### 2a. Node indexing contract (anchor vs calibrated nodes)

The math note indexes **calibrated** nodes $m = 1,\dots,M$ (one per
instrument). The curve *stores* $M{+}1$ nodes because the constructor
prepends the immutable anchor. The mapping is fixed here once:

| Math note | Curve storage |
|-----------|---------------|
| (anchor, $P(0,0)=1$, not a parameter) | index $0$ |
| calibrated node $m$ ($m = 1..M$) | index $m$ |

Rules: risk and Jacobian code iterates storage indices $1..M$ and **never
bumps index 0**; `bump_node(curve, 0, …)` throws `std::invalid_argument`;
`curve_node_sensitivities` returns exactly $M$ entries aligned with storage
indices $1..M$ (and with instrument order).

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

Constructor throws `std::invalid_argument` on: empty `xs`; non-finite or
non-strictly-increasing `xs`; `ys.size()` not a positive multiple of
`xs.size()`; non-finite `ys`. `evaluate()` throws `std::invalid_argument` if
any query is non-finite.

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
// always prepends the anchor (impl note §2a).
// Throws on queries before reference or beyond the last pillar.
class PiecewiseLogLinearCurve final : public YieldCurve {
public:
    PiecewiseLogLinearCurve(QuantLib::Date reference,
                            std::vector<CurveNode> pillars,
                            QuantLib::DayCounter day_counter);

    double discount(const QuantLib::Date& d) const override;
    QuantLib::Date reference_date() const override;
    const QuantLib::DayCounter& day_counter() const;

    // Batch query (vectorized path used by pricers and risk).
    std::vector<double> discounts(std::span<const QuantLib::Date> dates) const;

    // Read-only node view for risk and diagnostics (index 0 = anchor).
    std::span<const CurveNode> nodes() const;

private:
    QuantLib::Date reference_;
    std::vector<CurveNode> nodes_;
    QuantLib::DayCounter day_counter_;
    LinearFlatInterpolator interpolator_;  // built once in the constructor
                                           // over (Act/365F times, log-DFs);
                                           // domain enforcement lives in
                                           // discount()/discounts(), which
                                           // throw before delegating.
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
with its value in `CurveNode` makes a date/value size mismatch unrepresentable.
The real header includes
`"core/linear_interpolator.hpp"` directly and does not rely on a transitive
include.

### `src/curves/curve_instruments.hpp` — calibration inputs

```cpp
#pragma once
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>

#include <string>
#include <vector>

namespace irc {

// One CME 3M SOFR future (SR3): IMM reference quarter + quoted price.
struct SofrFutureQuote {
    std::string id;                  // stable ID, e.g. "SR3H26"
    QuantLib::Date reference_start;  // IMM quarter start
    QuantLib::Date reference_end;    // IMM quarter end (= pillar)
    double price;                    // Q_m, e.g. 95.80
};

// One spot-starting USD SOFR OIS par quote.
struct OisQuote {
    std::string id;          // stable ID from input, e.g. "4Y"
    QuantLib::Period tenor;  // 4Y ... 30Y
    double par_rate;         // S_m, decimal
};

// Canonical futures quote conversion. Calibration, residuals, and risk use
// decimal-rate units even though persisted market data uses IMM price units.
double sofr_future_rate_from_price(double price);
double sofr_future_price_from_rate(double rate);

// Everything the bootstrapper consumes. Phase 2 requires every futures
// contract to be fully forward (reference_start > valuation); partially
// accrued contracts are rejected — support is deferred (impl note §8),
// which is why there is deliberately no realized-fixings field yet.
struct SofrMarketData {
    QuantLib::Date valuation;
    std::vector<SofrFutureQuote> futures;  // sorted by reference_start
    std::vector<OisQuote> ois;             // sorted by tenor
};

}  // namespace irc
```

The two conversion functions throw `std::invalid_argument` for non-finite
inputs. They perform only the unit conversion; the bootstrapper validates the
contract-specific positivity condition $1+\tau R_{\mathrm{fut}}>0$.

### `src/curves/market_data_io.hpp` — pinned-file loader

```cpp
#pragma once
#include "curves/curve_instruments.hpp"

#include <filesystem>

namespace irc {

// Reads the pinned CSV schema of impl note §1a. Throws std::runtime_error
// naming the path and offending line on malformed CSV or invalid field text.
// Semantic validation is described below and names the instrument ID.
SofrMarketData load_sofr_market_data(const std::filesystem::path& csv_path);

}  // namespace irc
```

Parsing is strict: exactly one header and valuation row; no missing or extra
fields; dates and numbers must consume the complete field; instrument IDs must be
non-empty and unique. Fields that do not apply to a row type must be empty;
`quote_unit` must be exactly `imm_price` for futures and `decimal_rate` for
OIS rows. The accepted CSV subset has unquoted fields only: blank lines and
lines whose first non-whitespace character is `#` are ignored, surrounding
ASCII whitespace and a trailing `\r` are removed, and quoted fields are
rejected. Numeric parsing is locale-independent. Semantic validation requires
a non-null valuation date that is a `UnitedStates(SOFR)` business day, valid
ordered third-Wednesday IMM dates, `reference_end > reference_start`,
contiguous non-overlapping futures quarters, and unique increasing OIS tenors.
Quotes must be finite. Negative
OIS rates and futures prices above 100 are valid; do not reject them merely for
being non-positive rates. A futures quote is invalid only when its Act/360
accrual factor is non-positive or
$1+\tau R_{\mathrm{fut}}\leq 0$, because then its implied discount ratio is
undefined or non-positive.

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
    double residual;               // model_quote - market_quote, rate units
    std::size_t solver_iterations;  // zero for closed-form futures nodes
    bool used_expanded_bracket;     // false for futures
};

struct BootstrapResult {
    PiecewiseLogLinearCurve curve;
    std::vector<CalibrationDiagnostic> diagnostics;
};

// Sequential bootstrap per math note §Calibration:
//  1. anchor P(0,0) = 1;
//  2. first future: closed-form front-stub forward
//       f_1 = ln(1 + tau_1 R_fut,1) / (t_e1 - t_s1)   (impl note §1);
//  3. remaining futures: telescope P(0,T_e) = P_fwd * P(0,T_s) — each
//     T_s equals the previous pillar because the strip is contiguous;
//  4. OIS: for each quote solve eps_m(x_m) = K_model,m(x_m) - S_m = 0 by
//     bracketed bisection (bracket rule in impl note §1), earlier nodes
//     frozen.
// Validation (throws std::invalid_argument, naming the stable instrument ID):
//   empty future strip; malformed/duplicate IDs; null, non-business, non-IMM,
//   or unordered dates; any future with
//   reference_start <= valuation (partially accrued — unsupported in Phase 2);
//   non-contiguous or overlapping quarters; non-positive future accumulation
//   factor; duplicate pillar dates; OIS pillar not beyond the last futures
//   pillar; non-finite quotes. This boundary validates programmatically
//   constructed SofrMarketData as well as CSV-loaded data.
// Solver failure (bracket exhausted / max iterations) throws
// std::runtime_error naming the instrument and reporting both attempted
// forward-rate brackets and endpoint residuals. No silent fallback.
class SofrCurveBootstrapper {
public:
    BootstrapResult bootstrap(const SofrMarketData& market) const;

    // Model-implied decimal rate of every calibration instrument off an
    // arbitrary candidate curve — g(theta) in the math note. Order is futures
    // then OIS, matching SofrMarketData. Futures are returned as rates, never
    // IMM prices. Needed by diagnostics and the (stretch) Jacobian.
    std::vector<double> model_quotes(const SofrMarketData& market,
                                     const PiecewiseLogLinearCurve& curve) const;
};

}  // namespace irc
```

`bootstrap()` returns only after populating one diagnostic per instrument and
verifying `model_quote - market_quote` against the calibration tolerance.
Failed validation or calibration throws; a failed curve is never returned.
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
half-width $h$**:

$$
s_m \;=\; \frac{V(\boldsymbol\psi + h\,e_m) - V(\boldsymbol\psi - h\,e_m)}{2h},
\qquad
\mathrm{DV01}_m \;=\; s_m \times 10^{-4}.
$$

- `half_width` parameters below are **$h$, in the bumped parameter's units**
  (rate units for quotes, log-DF units for nodes). The *total* spread is $2h$
  — this is the factor-of-two trap, killed here.
- All quote bumps are normalized to **decimal-rate units**. For an OIS,
  `par_rate += h`. For a future, an upward rate bump is represented in the
  stored IMM price by `price -= 100*h`, while the downward scenario uses
  `price += 100*h`. Thus $h=10^{-4}$ maps to an IMM price move of $\mp0.01$.
- Functions named `*_dv01_*` return **DV01 in currency units**
  ($s_m \times 10^{-4}$), already carrying the math note's sign convention
  ($V(+1\mathrm{bp})-V$: payer +, receiver −).
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
// storage indices 1..M (anchor never bumped — impl note §2a).
std::vector<double> curve_node_sensitivities(
    const PiecewiseLogLinearCurve& curve, const CurvePricer& pv,
    double node_half_width = 1e-6);

// Direct quote DV01 (currency units, per instrument, order = futures then
// OIS): bump normalized rate quote m by +/- `quote_half_width`, rebuild the
// complete curve in each scenario, reprice, central-difference, scale by 1e-4.
// For futures, +h rate means -100*h in the stored IMM price.
std::vector<double> quote_dv01_direct(const SofrMarketData& market,
                                      const SofrCurveBootstrapper& bootstrapper,
                                      const CurvePricer& pv,
                                      double quote_half_width = 1e-4);

// --- In-phase stretch (build only after the direct tier is green) ----------

// Finite-difference calibration Jacobian J_mn = dg_m/d(theta_n), central
// difference, node half-width in log-DF units. Lower-triangular by
// construction; the test asserts the strict upper part ~ 0.
std::vector<std::vector<double>> calibration_jacobian(
    const SofrMarketData& market, const SofrCurveBootstrapper& bootstrapper,
    const PiecewiseLogLinearCurve& curve, double node_half_width = 1e-6);

// Quote DV01 (currency units) via the Jacobian path: solve
// J^T s_quote = s_curve by back substitution (J^T is upper-triangular; no
// inverse is formed), then scale by 1e-4.
std::vector<double> quote_dv01_via_jacobian(
    const std::vector<std::vector<double>>& jacobian,
    const std::vector<double>& curve_sensitivities);

}  // namespace irc
```

Every half-width must be finite and strictly positive; otherwise the risk
function throws `std::invalid_argument`. The required-tier implementation
always performs a full calibration for each bumped scenario. Dependency-aware
partial recalibration is a possible later optimization only after correctness
and benchmark tests are green.

(Plain `vector<vector<double>>` + hand-written back substitution keeps the
linear algebra auditable; Eigen — already in vcpkg.json — is a permitted
implementation choice if the owner prefers.)

### Examples

- `examples/02_sofr_curve_bootstrap.cpp <input.csv> <output.csv>` — own engine
  end-to-end: load the explicit input path, bootstrap, write the explicit
  output path with `write_curve_csv`, and print successful calibration
  diagnostics plus direct DV01 for a 10Y payer swap.
- `examples/02_quantlib_sofr_curve_bootstrap.cpp <input.csv>` — QuantLib twin:
  same CSV through `SofrFutureRateHelper` + `OISRateHelper` →
  `PiecewiseYieldCurve<Discount, LogLinear>`, printing benchmark diagnostics.

Both executables reject missing/extra arguments with a usage message and a
nonzero exit code. Every file path is an explicit argument; neither executable
infers the repository root from the process working directory.

## 4. Symbol → code map

| Math note | Code |
|-----------|------|
| $x_m,\ \theta_m$ ($m=1..M$) | `curve.nodes()[m].log_discount` (storage; index 0 = anchor) |
| $L_m$ | `curve.nodes()[m].date` |
| $t_m$ | `curve.day_counter().yearFraction(curve.reference_date(), L_m)` |
| $P^{\mathrm{SOFR}}(0,T)$ | `curve.discount(T)` / `curve.discounts(dates)` |
| $g_m(\boldsymbol\theta)$ | `bootstrapper.model_quotes(market, curve)[m-1]` |
| $\epsilon_m$ | `BootstrapResult::diagnostics[m-1].residual` |
| $\mathbf s^{\mathrm{curve}}$ (raw $s_n$) | `curve_node_sensitivities(...)` |
| $\mathrm{DV01}_m$ (currency) | `quote_dv01_direct(...)` |
| $\mathcal J$ | `calibration_jacobian(...)` *(stretch)* |
| solve $\mathcal J^\top\mathbf s^{q}=\mathbf s^{c}$ | `quote_dv01_via_jacobian(...)` *(stretch)* |

## 5. Test plan — `tests/test_curve_bootstrap.cpp` (red first)

Stubs: constructors and validation work; every computational method throws
`std::logic_error("... not implemented (Phase 2 step 4)")`. Owner implements
to green, group by group (AGENTS step 4). All market-data tests load the pinned
CSV of §1a — no duplicated hardcoded market quotes.

**A. Interpolator (finance-free — the interview kata is the fixture)**
1. Knots `xs={1,2,3}`, `ys={10,100, 20,200, 30,300}` (m=2), queries
   `{1.5,2.5}` → `{15,150, 25,250}` exactly.
2. Flat extrapolation: query 0.5 → `{10,100}`; query 9 → `{30,300}`.
3. Unsorted queries return per-query rows in input order.
4. Validation throws: empty xs; non-increasing xs; `ys.size() % xs.size() != 0`;
   NaN in xs, ys, or a query.
5. m=1 scalar case round-trips knot values exactly.

**B. Curve (known nodes, no bootstrap)**
6. Constructor accepts calibrated pillars strictly after reference, prepends
   exactly one anchor, and rejects a caller-supplied reference-date pillar.
   `discount(reference) == 1`; node dates reproduce their DFs to 1e-15.
7. Off-node date matches the closed-form log-linear formula to 1e-14.
8. Strictly positive DFs; monotone decreasing for positive-rate nodes.
9. Throws beyond last pillar and before reference.
10. `discounts(batch)` equals scalar `discount` per element exactly.
11. `bump_node(curve, 0, δ)` throws (anchor immutable); bumping node 1
    changes only segment-0/1-dependent discounts.

**C. Market data + bootstrap**
12. Quote conversion: price 95.80 maps to rate 0.042 and round-trips; an
    upward 1bp rate move maps to a -0.01 IMM price move.
13. Loader round-trip: pinned CSV → 12 futures + 9 OIS + valuation
    2026-01-15; loader throws on a malformed copy (bad type, wrong quote unit,
    non-empty inapplicable field, NaN quote, duplicate ID, overlapping
    quarters, duplicate tenor). A separate valid fixture with a negative OIS
    rate is accepted.
14. Futures-only sub-curve vs hand computation: first node via the
    front-stub closed form $f_1 = \ln(1+\tau_1 R_1)/(t_{e,1}-t_{s,1})$,
    subsequent nodes via telescoped products — agree to 1e-12.
15. Full-curve repricing: every returned `CalibrationDiagnostic` preserves
    the stable input ID, stores quotes in decimal-rate units, and satisfies
    $|\epsilon_m| \le 10^{-10}$ (math note tolerance). The result contains
    exactly 21 diagnostics in futures-then-OIS order.
16. Node dates strictly increasing; storage count = 1 (anchor) + 12 + 9.
17. Explicit failures use contextual messages; every instrument-specific
    failure names its stable ID:
    a. empty futures strip, non-business valuation date, or non-IMM future
       date → throw;
    b. a futures contract with `reference_start <= valuation` → throw
       (partially accrued unsupported in Phase 2);
    c. non-contiguous futures quarters → throw;
    d. duplicate pillar dates → throw;
    e. NaN/inf quote → throw (from loader or bootstrapper);
    f. solver bracket exhausted (constructed pathological quote) → throw
       `std::runtime_error` after the two-stage bracket rule, reporting both
       brackets and endpoint residuals.

**D. Deterministic output**
18. `serialize_curve_csv` for a tiny known-node curve equals one exact golden
    string, including number formatting, blank anchor fields, `\n` endings,
    and the final newline; a non-Act/365F curve is rejected rather than
    mislabeled.
19. `write_curve_csv` round-trips those exact bytes through a test-owned
    temporary directory and throws when its parent directory does not exist.

**E. QuantLib oracle**
20. Same CSV → QL `PiecewiseYieldCurve<Discount, LogLinear>` (helpers per
    math note §QuantLib Benchmark; empty discount handle;
    `Pillar::LastRelevantDate`). Pillar DFs and an off-pillar grid (all
    futures/OIS accrual + payment dates): $\max_T|\delta_P(T)| \le 10^{-8}$.
21. A 10Y par swap priced off both curves: PV and par-rate diffs within the
    math note's combined abs/rel tolerance.

**F. DV01 — required tier**
22. Sign: 10Y **payer** swap has **positive** total quote DV01; receiver is
    its negative (math note sign convention).
23. Magnitude sanity: total DV01 within ±30% of `annuity × N × 1e-4`
    computed off the same curve.
24. Central-difference symmetry: DV01 from half-width 1e-4 and 0.5e-4 agree
    to 1e-3 relative (guards against the half-vs-full-spread bug); zero,
    negative, NaN, and infinite half-widths throw.
25. Futures bucket semantics: the +$h$ scenario passed into the bootstrapper
    has price $Q-100h$ and the -$h$ scenario has $Q+100h$; each scenario
    performs a complete bootstrap.
26. QL comparison: the same central normalized-rate bumps are applied to the
    QL helpers, every scenario rebuilds its complete curve, and totals agree
    under the math note's normalized tolerance.

**G. DV01 — in-phase stretch (Jacobian path; build after F is green)**
27. Jacobian shape: strict upper triangle of `calibration_jacobian` ~ 0
    (≤ 1e-10 in rate units); all diagonal entries nonzero.
28. **Two-way cross-check:** `quote_dv01_direct` vs
    `quote_dv01_via_jacobian` (both central-difference) agree per entry
    under $|a-b| \le \max(10^{-10}\mathcal N,\ 10^{-6}|a|)$. Cross-validates
    bootstrap, Jacobian, and the back-substitution transform in one
    assertion — and becomes the finite-difference reference for the
    post-MVP analytic Jacobian.

## 6. CMake wiring

```cmake
# extend the existing library
target_sources(irc_pricing PRIVATE
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

set(PHASE2_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_data")
file(MAKE_DIRECTORY "${PHASE2_TEST_DATA_DIR}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/data/market/sofr_quotes_2026-01-15.csv"
  "${PHASE2_TEST_DATA_DIR}/sofr_quotes_2026-01-15.csv"
  COPYONLY)
target_compile_definitions(test_curve_bootstrap PRIVATE
  IRC_TEST_DATA_DIR=\"${PHASE2_TEST_DATA_DIR}\")

gtest_discover_tests(test_curve_bootstrap)
```

Tests form the fixture path from `IRC_TEST_DATA_DIR`; CMake supplies an absolute
forward-slash path and copies the immutable source fixture at configure time.
All example file paths are explicit command-line arguments. No code searches
parent directories or assumes a working directory.

## 7. Build order (AGENTS workflow)

1. **Owner closes the math-note gate in §0** by deriving and accepting the
   fully-forward first-future/front-stub case.
2. **Owner approves these interfaces** — or edits them here first.
3. AI materializes: headers exactly as approved, validation-only stubs,
   `tests/test_curve_bootstrap.cpp`, and the pinned CSV of §1a. Suite
   builds and runs **red**.
4. **Owner implements**, suggested order: interpolator → curve → loader →
   futures leg → OIS leg → deterministic output → direct DV01 — green through
   group F.
5. AI reviews the green implementation, then shows its own diff.
6. **Stretch:** owner implements the Jacobian path (group G), with the now-
   trusted direct DV01 as its reference.
7. AI runs the QuantLib comparison and reports diffs.
8. Owner commits on `phase-2-curves`; tag `v0.3-curve-dv01` when F is green
   (G is not gating).

## 8. Deferred (tracked, not forgotten)

- **Production short-end instrument set:** add overnight and short-dated OIS
  quotes before the first futures quarter so the front stub is market-
  constrained rather than closed solely by interpolation. Phase 2 deliberately
  keeps the smaller futures/OIS exercise after the owner documents its closure
  assumption.
- **Partially accrued futures** (valuation inside the first reference
  quarter): the math note's extended-bond-price / realized-accumulation
  ($A^{\mathrm{SOFR}}$) treatment. Needs: a realized-fixings input on
  `SofrMarketData`, accumulation logic in the bootstrapper, a fixings
  section in the CSV schema, and its own test fixture with a mid-quarter
  valuation date. Rejected with an explicit throw until then.
- **Analytic Jacobian** (roadmap: post-MVP; test 28's bumped version is its
  finite-difference reference).
- Futures convexity adjustment (needs a vol model — Phase 6+).
- Curve extrapolation policy beyond the last pillar (throws for now).
- Trade-file CSV input for portfolios (Phase 4 scope; Phase 2's CSV is
  market quotes only).
