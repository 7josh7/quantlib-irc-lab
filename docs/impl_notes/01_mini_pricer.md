# 01 — Mini pricer: implementation contract (Phase 1)

> **Not a math note.** The math lives in
> [`docs/math_notes/01_sofr_swap.md`](../math_notes/01_sofr_swap.md); this
> file is the code-facing spec: conventions pinned, interfaces proposed,
> tests planned. Per [AGENTS.md](../../AGENTS.md) workflow step 2, the
> headers below are a **proposal for owner approval** — no `.cpp` yet.

## 0. Scope (what v1 is and is not)

The hello-world case from the math note: **single flat curve**,
discount curve = projection curve, no spread, period-end payment (so §4's
telescoping is exact). QuantLib supplies calendars / day counts / schedules;
we hand-roll the pricing. QuantLib is the **oracle**, not the engine.

Out of scope for v1 (deferred, tracked in the math note §8/§10): payment lag,
look-back, lockout, multi-curve, convexity adjustment.

## 1. Conventions (pinned for the hello-world)

Decide these once here so the schedule and curve are reproducible. Chosen to
match standard USD SOFR OIS where it's free, and to keep the QuantLib oracle
trivial to line up.

| Item | Value (v1) | Notes |
|------|-----------|-------|
| Currency | USD | |
| Valuation date $t_0$ | 2026-05-23 | matches existing Phase 0 example |
| Calendar | `UnitedStates(SOFR)` | for schedule generation |
| Spot lag | 2 business days | effective date = `advance(t0, 2, Days)` |
| Tenor | 5Y | |
| Notional $N$ | 1,000,000 | |
| Fixed freq | Annual | |
| Float freq | Annual | OIS: legs coincide → $M^{\text{fix}}=M^{\text{flt}}$ |
| Day count (both legs) | Act/360 | USD SOFR OIS standard |
| Business-day convention | ModifiedFollowing | both legs |
| Payment lag | **0** (v1) | keeps §4 telescoping exact; real SOFR OIS is 2 BD — deferred |
| Curve | flat, $r = 0.04$, **Continuous**, Act/365 Fixed | $P(t_0,T)=e^{-r\,\tau_{365F}(t_0,T)}$ |

**Why the curve is continuous/Act365F:** it makes `FlatCurve::discount` a
one-liner and makes the QuantLib `FlatForward(..., Continuous, Actual365Fixed())`
oracle match to machine precision. The *leg accrual* day count (Act/360) is a
separate thing from the *curve* day count (Act/365F) — don't conflate them.

## 2. Symbol → code map

| Math note | Code |
|-----------|------|
| $P_{\mathcal D}(t_0,T)$ | `YieldCurve::discount(T)` |
| $\tau_i$ | `day_counter.yearFraction(T_{i-1}, T_i)` (QuantLib) |
| $F_i$ (simple) | `SimpleForwardRate::forward_rate(...)` |
| $R_{\text{RFR}}$ compounded | `CompoundedOvernightRate::forward_rate(...)` |
| $A(t)=\sum_i\tau_i P(t,T_i)$ | note is per-notional; code `FixedLeg::annuity()` returns $N\sum_i\tau_i P_i$ (money/unit-rate). Divide by $N$ to recover the note's $A(t)$. |
| $N K A(t)$ | `FixedLeg::present_value()` = $K\cdot$`annuity()` |
| $N\sum_i\tau_i(F_i+s)P_i$ | `FloatingLeg::present_value()` |
| $V^{\text{rec}}$ | `VanillaSwap::npv()` with `SwapSide::Receiver` |
| par $S_t$ | `VanillaSwap::fair_rate()` = float\_pv / `annuity()` |

## 3. Proposed interfaces (headers only — TODO bodies)

Layout follows the README repo map: `src/core/`, `src/rates/`. Dates,
day counters and schedules are QuantLib types (AGENTS §QuantLib usage:
"Market conventions: always use QuantLib. Do not reimplement").

### `src/core/yield_curve.hpp`

```cpp
#pragma once
#include <ql/time/date.hpp>

namespace irc {

// Discount-curve abstraction. Depend on THIS, not on a concrete curve, so
// Phase 2's bootstrapped curve drops in with no change to the legs
// (open-closed — CPP Ch.2-3).
class YieldCurve {
public:
    virtual ~YieldCurve() = default;

    // Discount factor P(reference_date, d). P(ref, ref) == 1.0.
    // Throws if d < reference_date().
    virtual double discount(const QuantLib::Date& d) const = 0;

    virtual QuantLib::Date reference_date() const = 0;
};

}  // namespace irc
```

### `src/core/flat_curve.hpp`

```cpp
#pragma once
#include "core/yield_curve.hpp"
#include <ql/time/daycounter.hpp>

namespace irc {

// Flat continuously-compounded zero curve: P(t,T) = exp(-r * tau(t,T)).
class FlatCurve final : public YieldCurve {
public:
    FlatCurve(QuantLib::Date reference,
              double zero_rate,
              QuantLib::DayCounter day_counter);

    double discount(const QuantLib::Date& d) const override;   // TODO
    QuantLib::Date reference_date() const override;             // TODO

private:
    QuantLib::Date reference_;
    double zero_rate_;
    QuantLib::DayCounter day_counter_;
};

}  // namespace irc
```

### `src/rates/rate_accrual.hpp` — the Strategy (CPP Ch.5)

```cpp
#pragma once
#include "core/yield_curve.hpp"
#include <ql/time/date.hpp>

namespace irc {

// Strategy: the projected floating rate F_i for one accrual period.
// Single-curve only in v1 (discount curve == projection curve).
class RateAccrual {
public:
    virtual ~RateAccrual() = default;
    virtual double forward_rate(const YieldCurve& curve,
                                const QuantLib::Date& start,
                                const QuantLib::Date& end,
                                double year_fraction) const = 0;
};

// IBOR-style simple forward:  F = (P(start)/P(end) - 1) / tau.   (note §4)
class SimpleForwardRate final : public RateAccrual {
public:
    double forward_rate(const YieldCurve&, const QuantLib::Date&,
                        const QuantLib::Date&, double) const override;  // TODO
};

// SOFR daily-compounded overnight rate (note §2, RFR block). Iterates the
// business days in [start, end], compounding overnight forwards from the
// curve. In single-curve this telescopes to SimpleForwardRate — Test 2
// asserts that agreement.
class CompoundedOvernightRate final : public RateAccrual {
public:
    explicit CompoundedOvernightRate(QuantLib::Calendar calendar);
    double forward_rate(const YieldCurve&, const QuantLib::Date&,
                        const QuantLib::Date&, double) const override;  // TODO
private:
    QuantLib::Calendar calendar_;
};

}  // namespace irc
```

### `src/rates/fixed_leg.hpp`

```cpp
#pragma once
#include "core/yield_curve.hpp"
#include <ql/time/schedule.hpp>
#include <ql/time/daycounter.hpp>

namespace irc {

class FixedLeg {
public:
    FixedLeg(QuantLib::Schedule schedule,
             QuantLib::DayCounter day_counter,
             double notional,
             double fixed_rate);

    // PV = K * annuity()  =  N * K * sum_i tau_i P(t,T_i)
    double present_value(const YieldCurve& curve) const;  // TODO
    // annuity() = N * sum_i tau_i P(t,T_i)   (money per unit rate)
    double annuity(const YieldCurve& curve) const;        // TODO

private:
    QuantLib::Schedule schedule_;
    QuantLib::DayCounter day_counter_;
    double notional_;
    double fixed_rate_;
};

}  // namespace irc
```

### `src/rates/floating_leg.hpp`

```cpp
#pragma once
#include "core/yield_curve.hpp"
#include "rates/rate_accrual.hpp"
#include <ql/time/schedule.hpp>
#include <ql/time/daycounter.hpp>
#include <memory>

namespace irc {

class FloatingLeg {
public:
    FloatingLeg(QuantLib::Schedule schedule,
                QuantLib::DayCounter day_counter,
                double notional,
                std::shared_ptr<const RateAccrual> accrual,
                double spread = 0.0);

    // PV = N * sum_i tau_i (F_i + s) P(t,T_i)
    double present_value(const YieldCurve& curve) const;  // TODO

private:
    QuantLib::Schedule schedule_;
    QuantLib::DayCounter day_counter_;
    double notional_;
    std::shared_ptr<const RateAccrual> accrual_;
    double spread_;
};

}  // namespace irc
```

### `src/rates/vanilla_swap.hpp`

```cpp
#pragma once
#include "rates/fixed_leg.hpp"
#include "rates/floating_leg.hpp"

namespace irc {

enum class SwapSide { Payer, Receiver };  // Payer pays fixed, receives float

class VanillaSwap {
public:
    VanillaSwap(SwapSide side, FixedLeg fixed_leg, FloatingLeg floating_leg);

    // Receiver: fixed_pv - float_pv.  Payer: -(that).
    double npv(const YieldCurve& curve) const;        // TODO
    // fair_rate = float_pv / annuity()   (side-independent)
    double fair_rate(const YieldCurve& curve) const;  // TODO

private:
    SwapSide side_;
    FixedLeg fixed_leg_;
    FloatingLeg floating_leg_;
};

}  // namespace irc
```

**Error handling (AGENTS §7):** constructors validate and throw on bad input
(empty schedule, `end <= start`, `d < reference_date()`). No silent NaN.

## 4. Test plan — `tests/test_mini_pricer.cpp` (GoogleTest)

Written **before** the implementations (AGENTS workflow step 3). Trade =
the §1 conventions; for the NPV test use fixed rate $K = 0.04$ (non-par, so
NPV is a meaningful nonzero number the oracle must reproduce).

| # | Test | Assertion |
|---|------|-----------|
| 1 | `FlatCurve` basics | `discount(ref)==1`; strictly decreasing; equals `exp(-r·τ365F)` to 1e-14 |
| 2 | **Strategies agree** | `SimpleForwardRate` vs `CompoundedOvernightRate` → same floating-leg PV to 1e-10 (the single-curve telescoping, note §4) |
| 3 | Annuity | `FixedLeg::annuity()` == `N·Σ τᵢ P(t,Tᵢ)` computed independently in the test |
| 4 | **Par NPV ≈ 0** | set `K = fair_rate(curve)`; `npv` within 1e-8 |
| 5 | Sign flip | `npv(Payer) == -npv(Receiver)` to 1e-12 |
| 6 | Fair-rate sanity | positive, within a few bp of 0.04 |
| 7 | **QuantLib oracle** | build the identical trade in QuantLib, compare (see below) |

**Oracle construction (Test 7):**
- Curve: `FlatForward(settlement, 0.04, Actual365Fixed(), Continuous)`.
- Compounded mode → `OvernightIndexedSwap` built with `Sofr` index + the flat
  curve, priced by `DiscountingSwapEngine`. Assert `|npv_ours − npv_ql| < 1e-6`
  and `|fair_ours − fair_ql| < 1e-8`.
- Simple mode → a `VanillaSwap`/`MakeVanillaSwap` with matching schedule as a
  second oracle for `SimpleForwardRate`.

If Test 7 misses, walk [AGENTS.md](../../AGENTS.md) §Numerical debugging
protocol in order (df direction → day count → calendar) before flailing.

## 5. CMake wiring (when implementations land)

`.cpp` bodies go in `src/core/` and `src/rates/`; collect them into a static
lib the tests and future examples link against:

```cmake
add_library(irc_pricing STATIC
  src/core/flat_curve.cpp
  src/rates/rate_accrual.cpp
  src/rates/fixed_leg.cpp
  src/rates/floating_leg.cpp
  src/rates/vanilla_swap.cpp)
target_include_directories(irc_pricing PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(irc_pricing PUBLIC QuantLib::QuantLib)

add_executable(test_mini_pricer tests/test_mini_pricer.cpp)
target_link_libraries(test_mini_pricer PRIVATE irc_pricing GTest::gtest_main)
gtest_discover_tests(test_mini_pricer)
```

## 6. Build order (AGENTS workflow steps 2→6)

1. **(this doc)** interfaces proposed → **owner approves**.
2. Create the headers in `src/` exactly as approved.
3. Write `tests/test_mini_pricer.cpp` (tests 1–7) against the headers.
4. Implement the `.cpp` bodies until tests 1–6 pass.
5. Wire the QuantLib oracle (test 7); report the diff.
6. Owner reviews → commit on `phase-1-mini-pricer` → tag `v0.2-mini-pricer`.
```
