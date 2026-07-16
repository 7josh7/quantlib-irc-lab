# AGENTS.md — Working rules for this repo

This file constrains how AI assistants (Claude Code, Codex, etc.) work in
this repository. Read it before any non-trivial change.

## Project context

This is a C++ / QuantLib implementation lab. Goal: build a small,
auditable fixed-income analytics engine with math notes, unit tests, and
QuantLib benchmarks. Scope and pacing are in `docs/roadmap.md`. Source
PDFs and their phase mapping are in `docs/sources.md` — the four-part
series (`note/I–IV`) is primary for Phases 0–7; `IRC.pdf` is primary for
Phases 8–10.

## Hard rules

1. **The repo owner decides the content of the math note.** If a math note for
   the module does not exist in `docs/math_notes/`, stop and say so. An AI may
   apply or revise decisions that the owner has explicitly provided, including
   prose and formatting changes. It must not invent mathematical or scope
   decisions, or complete note sections that the owner has not addressed. The
   point of the project is that the owner learns and controls the math.

2. **No implementation before a math note exists** for that module. The
   note must cover at minimum: formula, assumptions (including explicit scope
   boundaries), inputs, and outputs. Scope boundaries may be recorded with the
   assumptions; a separate `Known Limitations` section is not required.

3. **Every pricer has a unit test before merge.** Every numerical method
   has either an analytic sanity check or a QuantLib comparison.

4. **Prefer small, auditable modules.** Do not write 400-line god classes.
   If a header file exceeds ~150 lines or a class has more than ~5 public
   methods, split it.

5. **Do not optimize before correctness tests pass.** No SIMD, no template
   metaprogramming, no parallelism in v1. Readable single-threaded code
   first.

6. **Do not invent formulas.** If a formula is not clearly stated in one
   of the source PDFs (see `docs/sources.md`), a cited reference, or the
   math note, stop and ask. Do not guess. When the four-part series and
   `IRC.pdf` disagree on market practice, the four-part series wins
   (it's post-LIBOR-transition aware).

7. **Errors are explicit.** Throw with informative messages or return
   `tl::expected`-style results. No silent NaN returns, no "fallback"
   default values that hide bugs.

8. **Reproducibility.** Every example with randomness uses a fixed seed
   documented at the top of the file. Every CSV output is deterministic
   given the same input.

## Coding conventions

- C++20. MSVC on Windows is the primary toolchain.
- Use `std::shared_ptr` / `std::unique_ptr`; no raw `new`/`delete` in
  user code.
- `const`-correctness applies; member functions that don't mutate are
  `const`.
- Header guards: `#pragma once`.
- Naming: `snake_case` for files, `CamelCase` for types, `snake_case` for
  functions and variables.
- One class per header where reasonable. Implementation in `.cpp` unless
  the class is a small template.
- `clang-format` runs before commit, using the checked-in `.clang-format`
  at the repo root (Google base, 4-space indent, 100 columns, left-aligned
  references, include blocks preserved). From a Developer PowerShell:
  `clang-format -i src/**/*.hpp src/**/*.cpp tests/*.cpp examples/*.cpp`.
  If a style knob changes, reformat the whole tree in the same commit.
- `clang-tidy` warnings should be addressed or explicitly suppressed with
  a comment explaining why.

## QuantLib usage policy

QuantLib is used in two roles:

- **Market conventions** (calendars, day counts, schedules, IborIndex):
  always use QuantLib. Do not reimplement.
- **Model core** (curve fitting, swap pricer, short-rate model, MC):
  hand-roll a minimal version first, then compare against QuantLib's
  implementation. The point is to understand, not to ship.

Past a certain point — specifically Monte Carlo infrastructure (RNG,
PathGenerator, Brownian bridge) — use QuantLib's machinery. Do not
reinvent the MC framework.

## Required workflow for any new module

1. Owner writes `docs/math_notes/<chapter>_<topic>.md`.
2. AI proposes a minimal C++ interface (headers only, no implementation).
   Owner approves.
3. AI writes the test file first (GoogleTest), based on the math note,
   plus validation-only stubs so the suite builds and runs **red**. The
   red tests are the exercise spec; QuantLib is the answer key.
4. **Owner writes the implementation** until the tests pass. This is a
   C++ learning project as much as a math one — the owner fights the
   compiler. AI implements only when the owner explicitly asks for that
   module (Phase 1 was AI-implemented as a worked example to read).
5. AI reviews the owner's green implementation — const-correctness,
   unnecessary copies, idiom, error handling — and only then shows how
   it would have written it, as a diff with explanations. Never before
   the owner's version works.
6. AI compares against QuantLib where applicable and reports the diff.
7. Owner reviews, then commits.

Do not skip steps. Do not bundle steps. If a step has no obvious next
move, stop and ask.

## Git discipline

- One branch per phase (`phase-1-mini-pricer`, `phase-2-curves`, ...).
- Tag at each milestone (`v0.1-env`, `v0.2-mini-pricer`, `v0.3-curve-dv01`,
  `v1.0-mvp`, ...).
- No direct commits to `main` past the initial scaffold.
- Commit messages: imperative mood, scope prefix when useful
  (`curves: add piecewise log-linear interpolator`).

## Numerical debugging protocol

When a pricer/model fails to match its benchmark, walk this checklist
**in order** before flailing:

1. Seed reproducibility — same seed gives same output?
2. Discount factor — sign and direction (df at t=0 should be 1.0)?
3. Drift sign — under which measure are we?
4. Vol scaling — is vol per year or per period? sqrt(dt) applied?
5. Measure consistency — pricing measure matches numeraire choice?
6. Numeraire — divided/multiplied correctly?
7. Day count — actual/360 vs 30/360 vs actual/365?
8. Calendar — business day adjustment direction?

This list lives at `docs/numerical_debug_checklist.md` once we have
debugging to do. Add new entries as we hit them.

## What "done" means for a phase

A phase is done when:

- All math notes for that phase exist and are owner-written.
- All planned modules have tests, and tests pass on a clean build.
- Examples in `examples/` produce the expected CSVs.
- README's "How to reproduce" section is accurate for that phase.
- The milestone tag is pushed.

Not done just because the code compiles. Not done just because one
example runs.
