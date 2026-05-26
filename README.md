# IRC - Interest Rate and Credit Models Implementation Lab

A C++ / QuantLib implementation project building a fixed-income analytics
engine: SOFR curve construction, swap and CDS pricing, vol modeling, short-rate
models, and risk analytics. Each module is backed by math notes, unit tests,
and a QuantLib benchmark where possible.

Two source sets: a modern four-part series (foundations, curves, vol, RFR) in
`note/`, and Andrew Lesniewski's *Interest Rate and Credit Models* (`IRC.pdf`).
The four-part series is primary for Phases 0–7 (it's post-LIBOR and covers
SOFR conventions, xVA, PCA, RFR caplets); IRC is primary for Phases 8–10 (LMM,
Bermudan/LSM, deep CCR). See [docs/sources.md](docs/sources.md) for the full
mapping.

This is a learning and portfolio project. It is not trying to be a complete
risk system. The pacing is honest: Phase 0-4 is the Month-1 MVP commitment,
and everything past that is roadmap-only until the MVP works.

## Status

Phase 0: environment setup. QuantLib hello-swap example and smoke test build.

See [docs/roadmap.md](docs/roadmap.md) for the full plan.

## Month-1 MVP

By end of Week 4, this command must produce four CSVs from sample inputs:

```powershell
cmake --build "$env:USERPROFILE\irc-build" --config Release
& "$env:USERPROFILE\irc-build\Release\04_swap_portfolio_risk.exe"
```

Outputs:

```text
output/curve.csv            zero/discount/forward curve
output/swap_npv.csv         per-trade NPV and fair rate
output/dv01_report.csv      DV01 and key-rate durations (2Y/5Y/10Y)
output/scenario_pnl.csv     parallel +/-25bp, steepener, flattener P&L
```

Plus: passing `ctest`, README build instructions that work from a clean
clone, and three owner-written math notes in `docs/math_notes/`.

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

Use **Developer PowerShell for VS 2022**. Because this repo lives under Google
Drive, keep the build directory outside the synced folder; otherwise Ninja or
vcpkg can fail on file timestamp checks.

If using the Visual Studio bundled vcpkg:

```powershell
$env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg"
```

From the repo root (`IRC/`):

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

Expected Phase 0 example output:

```text
Valuation date = May 23rd, 2026
Settlement date = May 27th, 2026
Maturity date = May 27th, 2031
NPV of vanilla IRS = 388.107960
Fair fixed rate = 0.040087
```

## Repo Layout

Intentionally slim. New directories are added only when their phase starts; no
empty `lmm/` or `cva/` placeholders. See `docs/roadmap.md` for when each gets
added.

```text
IRC/
  CMakeLists.txt
  vcpkg.json
  README.md
  AGENTS.md
  IRC.pdf
  docs/
    roadmap.md
    math_notes/
  data/
    market/
    trades/
  src/
    core/
    curves/
    rates/
    risk/
    ql_examples/
  tests/
  examples/
  output/
```

## Operating Rules

Detailed rules for working in this repo, including AI assistants, are in
[AGENTS.md](AGENTS.md). The two that matter most:

1. **The repo owner writes the math note. AI writes code.** Never the other way
   around.
2. **No implementation lands before a math note exists** for it.

## Source Material

> **Note:** the PDFs listed below are **not committed to this repo** — they
> are course-note style materials whose redistribution rights are unclear.
> If you're following along, obtain your own copies and place them at the
> paths indicated. The repo's code, math notes, and roadmap stand on their
> own; the PDFs are reference.

Phase mapping in [docs/sources.md](docs/sources.md).

- `note/I. Modern Pricing Theory in Practice.pdf` — foundations, CSA-collateralized pricing, xVA (CVA/DVA/FVA/MVA/KVA), MC regression
- `note/II. Yield Curve And All That.pdf` — multi-curve construction, SOFR-centric bootstrap, xCcy, Jacobian risk, PCA hedging/eigen-scenarios
- `note/III. Volatility Modeling.pdf` — Bachelier, Dupire local vol, SABR (Hagan formula, ATM parameterization, smile-risk Greeks, Bartlett's delta)
- `note/IV. Breaking Down RFR Modeling.pdf` — backward-looking RFR caplets, time-decay SABR, bottom-up basket aggregation
- `IRC.pdf` — Lesniewski, *Interest Rate and Credit Models* (Hull–White, LMM, Bermudan/LSM, CCR depth)
- QuantLib — https://www.quantlib.org/
