# Numerical debugging checklist

When a pricer or model fails to match its benchmark, walk this list **in
order** before flailing. New entries are added as we hit them, per
[`AGENTS.md`](../AGENTS.md).

Two sections. The first asks whether the *model* is wrong; the second asks
whether the *machine* is doing what you assumed. Start with the first — it is
where almost every real discrepancy lives.

## A. Model and convention

1. **Seed reproducibility** — same seed gives same output?
2. **Discount factor** — sign and direction (df at $t=0$ should be 1.0)?
3. **Drift sign** — under which measure are we?
4. **Vol scaling** — is vol per year or per period? `sqrt(dt)` applied?
5. **Measure consistency** — pricing measure matches numeraire choice?
6. **Numeraire** — divided/multiplied correctly?
7. **Day count** — Actual/360 vs 30/360 vs Actual/365?
8. **Calendar** — business day adjustment direction?

## B. Floating point and the machine

Reach here only once section A is clean. These explain discrepancies in the
*last one or two digits*; they never explain a basis point.

9. **A golden value differs in the last one or two digits.** Ask where the
   golden came from: the mathematically exact value, or the `double` the code
   actually holds? These differ, and only the second one can ever appear in
   output.

   Diagnose by comparing bit patterns, not text. If both strings parse to the
   same `double`, the value was never in dispute — only its spelling:

   ```python
   python -c "print(float('9.6078943915232321e-01') == float('9.6078943915232318e-01'))"
   # True -> same double, the golden is a non-canonical rendering
   ```

   To find the correct rendering, print the exact decimal expansion of the
   double and round it yourself. This is pure arithmetic on the mantissa, so
   it does not depend on the code under test:

   ```python
   python -c "from decimal import Decimal; print(Decimal(0.96078943915232318))"
   # 0.96078943915232317696251129746087826788425445556640625  -> 17 s.f. -> ...318
   ```

   Worked example: `exp(-0.04)` is `0.96078943915232320863...` as a real
   number, which rounds to `...321`. But that value falls *between* two
   representable doubles, and the nearest one renders as `...318`. A golden
   taken from a high-precision calculator demands a string no IEEE-754
   formatter can produce, and no implementation can pass it. Phase 2's
   `serialize_curve_csv` test hit exactly this; see
   [`impl_notes/02_curve_bootstrap.md`](impl_notes/02_curve_bootstrap.md)
   test 22.

10. **Results differ across compilers or platforms by an ulp.** `exp`, `log`,
    `pow`, and the other transcendentals are **not** required by the C++
    standard or IEEE-754 to be correctly rounded. Two conforming libm
    implementations may legitimately return adjacent doubles. Basic
    arithmetic (`+ - * /`), `sqrt`, and conversions *are* exactly specified,
    so a discrepancy in those is a real bug, not a platform difference.

    Consequence for tests: never byte-compare output derived from a
    transcendental. Compare numerically with a tolerance. QuantLib's own
    curve tests do this — `test-suite/piecewiseyieldcurve.cpp` runs at
    `Real tolerance = 1.0e-9`, and 116 of its 174 test files use tolerances.

11. **A NaN passed a validation check.** Every comparison involving NaN is
    false, including `NaN <= 0.0`. Guards written as `if (x <= 0.0) reject;`
    therefore *accept* NaN. Write `if (!(x > 0.0)) reject;` instead, which
    rejects it. Check finiteness before any ordering comparison — the
    interpolator's strictly-increasing check depends on this ordering.

12. **A byte-exact file comparison fails with an invisible diff.** On Windows,
    a stream opened in text mode translates `\n` to `\r\n` on write. Open
    output streams with `std::ios::binary` when the bytes are a contract.

13. **Formatted numbers changed without the code changing.** `printf`,
    `ostream`, and `std::stod` all consult the global locale; a locale with a
    comma decimal separator or digit grouping silently changes output.
    `std::to_chars` and `std::from_chars` are locale-independent by
    specification and are what this repo uses for anything with a byte
    contract.
