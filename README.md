# IRC - Interest Rate and Credit Models Implementation Lab

A C++ / QuantLib implementation project building a fixed-income analytics
engine: SOFR curve construction, swap and CDS pricing, vol modeling, short-rate
models, and risk analytics. Each module is backed by math notes, unit tests,
and a QuantLib benchmark where possible.

The reference spine is a modern four-part series (foundations, curves, vol,
RFR) plus Lesniewski's *Interest Rate and Credit Models*. Brigo–Mercurio and
Glasserman provide model-theory and numerical back-stops; Joshi and Ballabio's
*Implementing QuantLib* provide C++/library-design context. Local working copies
live in `docs/ref/`. See [docs/sources.md](docs/sources.md) for the complete
nine-source phase mapping and rules of priority.

This is a learning and portfolio project, not a production risk system. The
MVP is Phases 0–3: environment, mini pricer, SOFR curve + quote DV01, and a
deterministic portfolio risk report. Phase 4 and later work does not begin
until that MVP is reproducible.

## Status

| Phase | State | Evidence / next gate |
|---|---|---|
| 0 — Environment | Implementation complete | MSVC/CMake/vcpkg build, legacy QuantLib swap scaffold, GoogleTest wiring; no standalone `v0.1-env` tag was retained |
| 1 — Mini pricer | Implementation complete | 12 green tests, analytic checks, QuantLib SOFR OIS comparison, tag `v0.2-mini-pricer`; owner math-note cleanup remains |
| 2 — SOFR curve + quote DV01 | Current | Owner completes `02_curve_bootstrapping.md` before interfaces, red tests, or implementation |
| 3 — Portfolio risk report | Planned | Produces the four MVP CSVs after Phase 2 is green |

Automated line-coverage measurement is not configured yet; the current
coverage percentage is unknown. Phase 2 adds the first coverage report.

See [docs/roadmap.md](docs/roadmap.md) for the full plan.

## MVP contract

When Phase 3 is complete, this command must produce four CSVs from pinned sample
inputs:

```powershell
cmake --build "$env:USERPROFILE\irc-build" --config Release
& "$env:USERPROFILE\irc-build\Release\03_swap_portfolio_risk.exe"
```

Outputs:

```text
output/curve.csv            zero/discount/forward curve
output/swap_npv.csv         per-trade NPV and fair rate
output/dv01_report.csv      DV01 and key-rate durations (2Y/5Y/10Y)
output/scenario_pnl.csv     parallel +/-25bp, steepener, flattener P&L
```

PCA eigen-scenarios from Source II §§4.2–4.3 are a post-MVP stretch and may be
added as separately tagged rows in `scenario_pnl.csv`; they do not gate
`v1.0-mvp`.

The MVP also requires passing `ctest`, build instructions that work from a
clean clone, and three completed owner-written math notes in
`docs/math_notes/`.

## Tech Stack

- C++20, MSVC (Visual Studio 2022) on Windows 11
- CMake >= 3.20
- vcpkg in manifest mode (`vcpkg.json` in repo root)
- QuantLib, Eigen
- GoogleTest
- clang-format, clang-tidy

## Build

Prerequisites:

- Visual Studio 2022 with the C++ workload
- CMake from Visual Studio or on PATH
- vcpkg from Visual Studio or a standalone clone

Use **Developer PowerShell for VS 2022**. Keep the build directory outside the
source tree; this avoids build clutter and also avoids timestamp problems if a
clone happens to live in a synced folder.

If using the Visual Studio bundled vcpkg:

```powershell
$env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg"
```

From the repository root (`quantlib-irc-lab/`):

```powershell
cmake -S . -B "$env:USERPROFILE\irc-build" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build "$env:USERPROFILE\irc-build" --config Release
ctest --test-dir "$env:USERPROFILE\irc-build" -C Release --output-on-failure
& "$env:USERPROFILE\irc-build\Release\01_quantlib_hello_swap.exe"
```

### Visual Studio

If opening the folder directly in Visual Studio, use the preset:

```text
vs2022-x64-static
```

If Visual Studio shows "CMake Generation Failed", it is usually using its
default Debug configuration without vcpkg. In Visual Studio:

1. Select the `vs2022-x64-static` configure preset.
2. Delete the old CMake cache if Visual Studio already generated one.
3. Generate the cache again.

The preset writes build output to:

```text
C:\Users\<you>\irc-build
```

Expected output from the retained Phase 0 **legacy LIBOR scaffold**:

```text
Valuation date = May 23rd, 2026
Settlement date = May 27th, 2026
Maturity date = May 27th, 2031
NPV of vanilla IRS = 388.107960
Fair fixed rate = 0.040087
```

## Repo Layout

The current repository is intentionally slim. Planned `data/`, `curves/`,
`risk/`, and `output/` directories are added only when their phase begins.

```text
quantlib-irc-lab/
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json
  README.md
  AGENTS.md
  docs/
    roadmap.md
    sources.md
    impl_notes/
    math_notes/
    ref/                    local source material; ignored by git
  src/
    core/
    rates/
  tests/
  examples/
```

## Operating Rules

Detailed rules for working in this repo, including AI assistants, are in
[AGENTS.md](AGENTS.md). The two that matter most:

1. **The repo owner writes the math note and the implementation.** AI writes
   the interface proposal, the red tests, and the post-green review—not the
   implementation, unless explicitly asked for a given module. Phase 1 is the
   documented worked-example exception.
2. **No implementation lands before a math note exists** for it.

## Source Material

> **Note:** the reference works listed below are **not committed to this
> repo**—their redistribution rights are unclear.
> If you're following along, obtain your own copies and place them under
> `docs/ref/`. That directory, including extracted Markdown working copies, is
> ignored by git. The committed code, owner math notes, source map, and roadmap
> should stand on their own.

Phase mapping in [docs/sources.md](docs/sources.md).

- `docs/ref/i-modern-pricing-theory-in-practice.md` — foundations, collateralized pricing, xVA, and MC regression
- `docs/ref/ii-yield-curve-and-all-that.md` — SOFR bootstrap, multi-curve/xCcy construction, Jacobian risk, PCA, and curve scenarios
- `docs/ref/iii-volatility-modeling.md` — Bachelier, Dupire local vol, SABR, and smile-risk Greeks
- `docs/ref/iv-breaking-down-risk-free-rate-rfr-modeling.md` — backward-looking RFR caplets, time-decay SABR, and basket aggregation
- `docs/ref/IRC.pdf` — Lesniewski, *Interest Rate and Credit Models* (Hull–White, LMM, Bermudan/LSM, CCR)
- `docs/ref/[Springer 2006] Interest Rate Models Theory and Practice With Smile, Inflation and Credit.pdf` — Brigo & Mercurio; model-theory back-stop, not SOFR conventions
- `docs/ref/Monte_Carlo_Methods_In_Financial_Enginee.pdf` — Glasserman; Monte Carlo numerics
- `docs/ref/cpp-design-patterns-and-derivatives-pricing.pdf` — Joshi; code architecture with pre-C++11 idioms modernized
- `docs/ref/IMPLEMENTING QUANTLIB.pdf` — Ballabio; QuantLib instruments/engines, conventions, quotes, interpolation, solvers, handles, and design patterns
- QuantLib — https://www.quantlib.org/
