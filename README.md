# Interest Rate and Credit Models Implementation Lab

An auditable C++20 / QuantLib learning project for fixed-income pricing, curve
construction, and risk.

The project builds small pieces of an interest-rate analytics engine by hand,
then checks the mathematics and numerical behavior with analytic identities and
QuantLib comparisons. The emphasis is on explicit assumptions, readable code,
deterministic inputs, and tests that explain what each module must do. It is a
learning and portfolio project, not a production pricing or risk system.

## Current state

The repository is partway through Phase 2. The simplified swap pricer is
complete; the SOFR curve foundations are implemented; the sequential bootstrap
and quote-DV01 calculations remain intentional test-first exercises.

| Phase | State | What exists |
|---|---|---|
| 0 — Environment | Complete | MSVC/CMake/vcpkg build, GoogleTest wiring, and a retained QuantLib swap example |
| 1 — Mini pricer | Complete | Flat discount curve, fixed and floating legs, SOFR-aware accrual strategies, swap NPV/fair rate, analytic tests, and a QuantLib OIS comparison; tagged `v0.2-mini-pricer` |
| 2 — SOFR curve + quote DV01 | In progress | Solver, interpolation, piecewise log-linear curve, curve instruments, payment-lag cash flows, market-data loading, and deterministic curve serialization are implemented. `SofrCurveBootstrapper` and the DV01 calculations are still validation-only stubs |
| 3 — Portfolio risk report | Planned | No Phase 3 executable or generated portfolio reports exist yet |

The current checkout therefore contains an intentionally red portion of the
test suite. Those tests specify the unfinished bootstrap and risk work; they
are not being presented as passing functionality. The last complete green
milestone is `v0.2-mini-pricer`.

The full execution plan, including what is MVP scope versus later research, is
in [docs/roadmap.md](docs/roadmap.md).

## Implemented capabilities

- A continuously compounded flat discount curve behind a small `YieldCurve`
  interface.
- Fixed and floating swap legs with payer/receiver NPV and fair-rate
  calculations.
- Simple-forward and projected daily-compounded floating-rate accrual.
- QuantLib-based calendars, day counts, coupon schedules, and payment lag.
- A bracketed bisection solver, vectorized linear-flat interpolation, and a
  piecewise log-linear discount curve.
- Parsing and validation of a pinned SOFR fixture, including realized
  calendar-day-weighted overnight accumulation.
- Deterministic curve CSV formatting and explicit failures for malformed or
  non-finite input.
- Analytic sanity checks and a green QuantLib OIS comparison for the mini
  pricer. Curve-level QuantLib comparisons are already specified in the red
  Phase 2 tests.

The Phase 2 fixture is deliberately hybrid: the historical SOFR fixings are
real final observations, while the SR3 futures and OIS quotes are synthetic and
internally deterministic. It is not represented as a historical market
snapshot. The exact conventions and fixture contract are documented in
[docs/impl_notes/02_curve_bootstrap.md](docs/impl_notes/02_curve_bootstrap.md).

## Build and run

The primary environment is Windows with:

- Visual Studio 2022 and the **Desktop development with C++** workload
- CMake 3.21 or later (required by the checked-in preset schema)
- vcpkg, either bundled with Visual Studio or installed separately

Dependencies are declared in [vcpkg.json](vcpkg.json). Use a **Developer
PowerShell for VS 2022** from the repository root.

The checked-in preset uses the `x64-windows-static` triplet and assumes the
default Visual Studio 2022 Community vcpkg location:

```powershell
cmake --preset vs2022-x64-static
cmake --build --preset release
```

If vcpkg is elsewhere, set `VCPKG_ROOT` and override the preset's toolchain
path when configuring:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

cmake --preset vs2022-x64-static `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build --preset release
```

Build products are written outside the source tree to
`C:\Users\<you>\irc-build`.

### Run the implemented green baseline

The smoke and mini-pricer test executables are green at the current checkpoint:

```powershell
& "$env:USERPROFILE\irc-build\Release\irc_smoke_tests.exe"
& "$env:USERPROFILE\irc-build\Release\test_mini_pricer.exe"
```

To run the complete suite, including the intentionally red Phase 2
specification:

```powershell
ctest --preset release --output-on-failure
```

The retained executable is a legacy USD-LIBOR swap used only to prove the
QuantLib toolchain and conventions wiring:

```powershell
& "$env:USERPROFILE\irc-build\Release\01_quantlib_hello_swap.exe"
```

Expected deterministic output:

```text
Valuation date = May 23rd, 2026
Settlement date = May 27th, 2026
Maturity date = May 27th, 2031
NPV of vanilla IRS = 388.107960
Fair fixed rate = 0.040087
```

It is not the project's statement of current SOFR market practice. The Phase 1
`OvernightIndexedSwap` test is the canonical QuantLib comparison for the
SOFR-aware mini pricer.

### Optional coverage target

An MSVC static native line-coverage target is configured with a 70% threshold
over `src/`. It requires a Visual Studio installation that provides the native
coverage collector and a green test suite:

```powershell
cmake --preset vs2022-x64-static-coverage
cmake --build --preset coverage --target coverage
```

The Cobertura report is written beneath
`C:\Users\<you>\irc-coverage-build\coverage`.

## Repository layout

```text
quantlib-irc-lab/
  cmake/                 MSVC coverage helper
  data/market/           pinned SOFR quotes and fixings
  docs/
    math_notes/          owner-written formulas, assumptions, inputs, outputs
    impl_notes/          approved interfaces and executable specifications
    roadmap.md           phase scope, gates, and future work
    sources.md           reference roles and phase mapping
  examples/              runnable QuantLib wiring example
  src/
    core/                curve abstraction, interpolation, and solver
    curves/              instruments, market data, curve I/O, bootstrap API
    rates/               accrual, cash-flow legs, and swap pricing
    risk/                Phase 2 DV01 API and validation stubs
  tests/                 analytic checks, validation tests, and QuantLib oracles
```

Generated build products and CSV output are ignored by Git.

## Development approach

QuantLib supplies market conventions such as calendars, day counts, schedules,
and benchmark implementations. The model core is kept small and hand-written
so its formulas and failure modes remain inspectable.

Every module starts from an owner-written math note covering its formula,
assumptions, scope boundaries, inputs, and outputs. Tests then provide analytic
checks or a like-for-like QuantLib comparison. Inputs are validated explicitly;
examples and CSV output are deterministic.

The detailed contribution and AI-assistance workflow is in
[AGENTS.md](AGENTS.md). Numerical mismatches are investigated using
[docs/numerical_debug_checklist.md](docs/numerical_debug_checklist.md).

## Roadmap and references

The MVP ends with a reproducible SOFR curve, complete quote bump-and-rebootstrap
DV01, and a deterministic small-portfolio risk report. Foreign-collateralized
curves, xVA, credit, volatility, and short-rate models are later roadmap topics,
not current repository capabilities.

The project's primary modern references are a four-part series covering
collateralized pricing, curve construction, volatility, and backward-looking
RFR products, supplemented by Lesniewski, Brigo–Mercurio, Glasserman, Joshi,
Ballabio, and QuantLib. Their exact roles and priority rules are recorded in
[docs/sources.md](docs/sources.md).

Local working copies of reference books and papers belong under `docs/ref/`
and are intentionally excluded from version control. The committed math notes,
implementation contracts, tests, and code are intended to stand on their own.
