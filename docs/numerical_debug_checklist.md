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
    implementations may legitimately return adjacent doubles.

    Basic arithmetic (`+ - * /`) and `sqrt` are correctly rounded — **but only
    under conditions worth checking before relying on it**:

    - the platform must actually be IEEE-754. Verify with
      `static_assert(std::numeric_limits<double>::is_iec559)` rather than
      assuming;
    - fast-math must be off. `/fp:fast` (MSVC) and `-ffast-math` (GCC/Clang)
      permit reassociation and other rewrites that break correct rounding.
      This repo does not currently pass an explicit `/fp` option, so its
      Visual Studio 2022 build uses the `/fp:precise` default unless an
      external flag overrides it. If bit-level behavior becomes a build
      contract, pin `/fp:precise` in CMake rather than relying on that default;
    - the compiler must not contract `a*b + c` into a single FMA, which is
      rounded once and can therefore produce a different answer. Control with
      `#pragma STDC FP_CONTRACT` or `/fp:precise`;
    - the rounding mode must still be the default. The floating-point
      environment is thread-local: `std::fesetround` changes the current
      thread's mode, so a library call on that thread can affect later
      operations. MSVC `/fp:precise` assumes the environment is not changed;
      code that intentionally changes or observes it must enable floating-
      environment access, for example with `/fp:strict`;
    - x87 80-bit intermediates (32-bit x86) can double-round.

    Decimal/binary conversion is a separate question and depends on the API:
    `std::from_chars` and `std::to_chars` are locale-independent, but that is
    not a universal correctly-rounded guarantee. The shortest, no-precision
    `to_chars` overload guarantees that `from_chars` on the **same
    implementation** recovers the original value. Floating `from_chars` is
    otherwise permitted to return one of the two closest representable
    values. Phase 2 deliberately uses the precision overload to impose a
    stable scientific shape; its 17-digit round trip is verified on the target
    MSVC implementation by `CurveIoTest.SerializedValuesMatchTheClosedForm`,
    rather than claimed as a cross-implementation theorem. C99 `strtod`,
    `printf`, and iostreams have different accuracy and locale rules; inspect
    the chosen API instead of grouping them under one guarantee. See WG21
    [P0067R5](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0067r5.html)
    and item 13 below.

    So a last-bit discrepancy in plain arithmetic is *usually* a real bug, but
    rule out the list above before concluding it.

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
