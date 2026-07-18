# Source mapping

Eleven source works: **seven math/finance references** (the four-part series
I–IV, IRC, plus two textbook back-stops — **BM** for rate-model theory and
**GLA** for Monte Carlo methodology) and **four implementation references**:
**CPP** for small pricing-library design, **IQ** for QuantLib architecture and
utilities, **PRIMER** for the C++11 core language in depth, and **TOUR** for
C++20 features PRIMER predates. The finance references determine what to
compute; CPP, IQ, PRIMER, and TOUR inform how to structure or benchmark the
implementation.
Local working copies and extracted Markdown live under `docs/ref/`; that
directory is intentionally ignored because redistribution rights are unclear.

## Sources

| ID | Title | Role |
|----|-------|------|
| **IRC** | Lesniewski, *Interest Rate and Credit Models* (`docs/ref/IRC.pdf`) | Single-author full-arc text. Spine for back-half phases (LMM, Bermudan/LSM, CCR depth). |
| **I** | *Modern Pricing Theory in Practice* (`docs/ref/i-modern-pricing-theory-in-practice.md`) | Foundations: martingale measures, **CSA-collateralized pricing**, xVA decomposition (CVA/DVA/FVA/MVA/KVA), CVA integral with EE/PD/LGD, **Monte Carlo regression** (basis functions + least-squares). |
| **II** | *Yield Curve And All That* (`docs/ref/ii-yield-curve-and-all-that.md`) | Curve construction, modern: simple vs compound rates, multi-curve, **SOFR-centric bootstrap with futures + OIS** (worked Table 1), cross-currency basis swaps, FX-forward currency chains, **Jacobian risk representation**, **PCA hedging and eigen-scenarios** (worked numerical example). |
| **III** | *Volatility Modeling* (`docs/ref/iii-volatility-modeling.md`) | Vol theory: Bachelier, Dupire local vol, **SABR** (Hagan formula, ATM parameterization, smile-risk Greeks, Bartlett's modified delta). |
| **IV** | *Breaking Down RFR Modeling* (`docs/ref/iv-breaking-down-risk-free-rate-rfr-modeling.md`) | RFR-specific vol: **backward-looking vs forward-looking caplets**, **time-decay SABR** for compounded rates, bottom-up basket aggregation. |
| **BM** | Brigo & Mercurio, *Interest Rate Models — Theory and Practice (With Smile, Inflation and Credit)*, 2nd ed. (Springer 2006) (`docs/ref/[Springer 2006] Interest Rate Models Theory and Practice With Smile, Inflation and Credit.pdf`) | Deep theory back-stop for the back half. Cap/floor & swaption Black/Bachelier formulas (Ch.1); **short-rate models** — Vasicek, CIR, Hull–White, CIR++ fit to the initial curve (Ch.3), G2++ two-factor (Ch.4); the **LIBOR/swap market models** and calibration (Ch.6–7), MC tests of LFM approximations (Ch.8); **SABR as a stochastic-vol extension** (Ch.11); counterparty risk + intensity/CDS models (Ch.21–22). Rigorous and rates-native — complements IRC on model depth. **LIBOR-era (2006): take the model theory, not the pre-SOFR curve plumbing.** |
| **GLA** | Glasserman, *Monte Carlo Methods in Financial Engineering* (Springer 2003) (`docs/ref/Monte_Carlo_Methods_In_Financial_Enginee.pdf`) | **Numerical methodology, not instruments.** The MC reference: RNG + normal-vector generation (Ch.2), Brownian path / bridge construction (Ch.3), variance reduction (Ch.4), quasi-MC (Ch.5), SDE discretization — Euler / second-order (Ch.6), MC Greeks (Ch.7), **American-option pricing incl. regression-based Longstaff–Schwartz and duality upper bounds** (Ch.8 §8.6–8.7), loss probabilities / VaR / credit risk (Ch.9). Pairs with CPP: **GLA is what the numerics do, CPP is how to structure the classes.** |
| **CPP** | Joshi, *C++ Design Patterns and Derivatives Pricing*, 2nd ed. (`docs/ref/cpp-design-patterns-and-derivatives-pricing.pdf`) | **Code architecture, not math.** Design patterns for a pricing library: open–closed payoff/curve hierarchy (Ch.2–3), bridge + virtual constructor + parameters class (Ch.4), strategy + decoration + statistics gatherer (Ch.5), a random-numbers class (Ch.6), a templatized exotics/MC engine (Ch.7), trees (Ch.8), solver function-objects (Ch.9), the factory (Ch.10, 14), exception-safe smart-pointer idioms (Ch.13), decoupling/levelization (Ch.16). Equity/FX/MC-flavored and pre-C++11 — **port the patterns, modernize the idioms**. |
| **IQ** | Ballabio, *Implementing QuantLib* (`docs/ref/IMPLEMENTING QUANTLIB.pdf`) | **QuantLib architecture and utilities.** Instrument/pricing-engine separation and swap example (Ch.2); dates, calendars, day counts, schedules, quotes, rates, and indexes (App. A); interpolation, one-dimensional solvers, optimizers, statistics, and linear algebra; smart pointers/handles, explicit error reporting, and Observer/Visitor patterns. Use it to understand the oracle and conventions, not to copy QuantLib wholesale. |
| **PRIMER** | Lippman, Lajoie & Moo, *C++ Primer*, 5th ed. (Addison-Wesley 2013) (`docs/ref/C++.Primer.5th.Edition_2013.pdf`) | **The language itself — not math, not architecture.** The reference for the modern idioms CPP predates. Copy control and the rule of five (Ch.13); **move semantics: rvalue references (§13.6.1), move constructor/assignment (§13.6.2), reference-qualified members (§13.6.3)**; dynamic memory and smart pointers (§12.1); overloaded operators, function objects, and `std::function` (Ch.14, §14.8.3); inheritance and virtual dispatch (Ch.15); templates — function, class, member, variadic — and argument deduction (Ch.16), including **reference collapsing (§16.2.5), `std::move` explained (§16.2.6), and perfect forwarding (§16.2.7)**; pointers to functions (§6.7); scoped enums (§19.3). **C++11 only (2013)** — see the gap note below. |
| **TOUR** | Stroustrup, *A Tour of C++*, 3rd ed. (Addison-Wesley 2023, ISBN 978-0-13-681648-5) (`docs/ref/a-tour-of-cpp-3rd-ed.pdf`) | **C++20, broad not deep.** Covers what PRIMER predates: **concepts and generic programming (Ch.8 — §8.2 Concepts, §8.3 Generic Programming, §8.4 Variadic Templates)**, ranges (Ch.14, incl. §14.5 Concepts Overview), and parallel algorithms (§13.6). Also re-covers, briefly, ground PRIMER treats in depth: copy and move (§6.2), resource management (§6.3), operator overloading (§6.4), templates (Ch.7), class hierarchies and virtuals (§5.3–5.5), smart pointers (Ch.15), `<random>` (§17.5). **Deliberately a tour** — ~300 pages for all of C++20. For anything mechanism-level, prefer PRIMER: e.g. `move`/`forward` get one subsection here (§16.6) versus three derived sections there (§16.2.5–16.2.7). |

## Rule of priority

For **Phases 0–7**, the **four-part series (I–IV)** is primary, IRC is
complement. The four-part series is post-LIBOR-transition aware, the
notation is consistent, and it covers exactly the topics each early phase
needs (curves, vol, xVA framework).

For **Phases 8–10**, **IRC** is primary, the four-part series is complement
(it does not cover LMM, Bermudan/LSM in depth, or short-rate models).
Source I provides the foundational CVA framework, but IRC Ch.15 goes deeper
on CCR.

When the two sources disagree on market practice, the four-part series
wins (it reflects current SOFR-era conventions). When they disagree on
mathematical depth or coverage of LMM/Bermudan/CCR, IRC wins.

**BM** and **GLA** are textbook back-stops, not per-phase primaries. Reach
for **BM** when a back-half model needs more rigor or a second derivation
than IRC gives (short-rate calibration, LMM drifts, SABR-as-stochastic-vol)
— but for anything SOFR/RFR-era, II–IV override BM's LIBOR-era conventions.
Reach for **GLA** whenever Monte Carlo enters the picture (LMM paths, LSM
regression, CCR exposure simulation): it governs the numerics, while CPP
governs the class design.

## Phase → source mapping

The first two columns establish the finance; the third names the C++ language
features that phase actually exercises. Only the finance columns authorize a
formula. Prose detail for the third column is in the next section.

| Phase | Primary | Secondary | C++ language |
|-------|---------|-----------|--------------|
| 0 — Environment + legacy QuantLib scaffold | IQ Ch.2 and App. A; II §3.2.2 | IRC Lecture 1 | PRIMER Ch.1 (build, first program) |
| 1 — Mini swap pricer | II §2.2, §3.2.2, §6.1 | I §2; IQ Ch.2 swap/engine design; IRC Lecture 1 | PRIMER Ch.7 (classes, `const` members), Ch.15 (abstract base + virtuals — `YieldCurve`, `RateAccrual`), §12.1 (`shared_ptr`), Ch.13 (rule of zero) |
| 2 — SOFR curve bootstrap + quote DV01 | II §3.3.1 | IRC Lecture 1 curve stripping; IQ quotes/interpolation/solvers; GLA Ch.7 finite differences; II §3.4 deferred Jacobian | PRIMER Ch.16 (templates; §16.2.5–16.2.7 for the `F&&` solver), §6.7 (function pointers), Ch.11 (associative containers, §11.2.2 custom comparator), §10.3.2 (lambdas), §14.8.3 (`std::function`); **TOUR §8.2** (concepts) |
| 3 — Portfolio risk report | II §3.4 and §5 | IRC Lecture 14 sensitivity risk; II §4.2–4.3 optional PCA; GLA Ch.9 risk context | PRIMER Ch.10 (algorithms), Ch.9/11 (containers), Ch.8 (file IO) |
| 4 (optional, post-MVP) — xCcy / foreign-collateralized discounting | II §3.2.3–3.2.5 (basis, xCcy non-MTM, FX chain), II §3.3.2–3.3.3 (forward via basis, foreign-collateral curves) | I §2 (collateralized pricing) | — (reuses Phase 1–2 machinery) |
| 4.5 (optional) — xVA awareness | I §2 (CSA replication), I §3.1 (xVA decomposition), I §3.2 (CVA) | — | — |
| 5 — CDS, survival curve | I §3.2, IRC Lectures 2–3 | BM Ch.21–22 | PRIMER Ch.15 (extends the curve hierarchy) |
| 6 — Hull–White | IRC Lecture 10 | BM Ch.3–4; GLA §3.3 and Ch.6 for simulation/discretization | PRIMER Ch.9/12 (tree storage), §17.4 (`<random>`) |
| 7 — SABR / swaption / RFR caplet | III §4.3–4.8, IV §3–4 | IRC Lectures 5–6 and 14; BM Ch.10–11 | PRIMER Ch.16 + §14.8 (calibration objectives as function objects) |
| 8 — LMM | IRC Lecture 11, BM Ch.6–7 | BM Ch.8; GLA §3.7 and Ch.4–6 | PRIMER §17.4 (`<random>` engines/distributions), Ch.16 (path-engine templates); TOUR §13.6 (parallel algorithms) if paths are parallelised |
| 9 — LSM Bermudan | IRC Lecture 12, I §3.4, GLA §8.6–8.7 | BM §13.15 | PRIMER Ch.10 (regression over path containers) |
| 10 — CCR / exposure / CVA | IRC Ch.15, I §3.2–3.3 (EE, MPoR, nested-MC critique) | GLA Ch.9 (VaR/PFE, credit risk) | PRIMER Ch.9/10 (exposure grids and aggregation) |

## Where the implementation references apply

CPP, IQ, PRIMER, and TOUR do not authorize a formula; they guide C++
structure, QuantLib usage, and language idiom after the relevant owner math
note establishes the finance.

| Phase / area | Implementation material |
|--------------|-------------------------|
| 0–1 — QuantLib conventions and mini pricer | IQ Ch.2 instrument/engine split and swap example; IQ App. A dates, calendars, day counts, schedules, rates, and indexes. CPP Ch.2–5 for the open–closed curve and accrual strategies. |
| 2 — Bootstrap + bump risk | IQ App. A quotes, interpolation, one-dimensional solvers, handles, and errors. CPP Ch.9 solver function-objects and Ch.16 levelization. PRIMER §16.2.5–16.2.7 for the forwarding-reference callable solver, §6.7 for the function-pointer comparator; TOUR §8.2 for the `ScalarResidual` concept constraining it. Keep the model core hand-rolled and use QuantLib as the answer key. |
| 3 — Scenario engine | CPP Ch.5 Strategy/statistics ideas; IQ Observer/handles help explain QuantLib quote bumps, while the hand-rolled MVP can use explicit immutable scenarios. |
| 6 — Hull–White | CPP Ch.8 tree design; IQ pricing-engine separation for the QuantLib benchmark. |
| 7 — Implied vol / SABR calibration | CPP Ch.9 solver function-objects; IQ optimizers and error reporting. |
| 8–9 — LMM / LSM Monte Carlo | CPP Ch.5–7 for RNG/statistics/path-engine boundaries; GLA governs the numerical method. PRIMER §17.4 for the `<random>` engine/distribution split (TOUR §17.5 covers the same ground more briefly); PRIMER Ch.16 for the template machinery behind a path-generator interface. Use QuantLib's MC infrastructure as required by `AGENTS.md`. |
| Throughout | CPP Ch.13 and Ch.16 for exception safety/decoupling; IQ handles and explicit error reporting. Modernize all pre-C++11 idioms — **PRIMER is the reference for what "modernize" means**: Ch.13 (copy control, rule of five), §13.6 (move), §12.1 (smart pointers over raw `new`/`delete`), Ch.16 (templates). Reach for **TOUR** only when the feature postdates C++11 (concepts, ranges). |

## What each source does NOT cover

- **IRC** doesn't deeply cover: SOFR-era multi-curve construction with
  explicit instrument tables, xCcy currency chains, time-decay SABR for
  RFR, PCA-based stress scenarios, modern xVA decomposition. Use II–IV
  for these.
- **Four-part series** doesn't cover: Hull–White, Vasicek, LMM, Bermudan
  swaption / LSM in depth, MBS, full CCR exposure simulation methodology.
  Use IRC for these.
- **BM** predates the SOFR multi-curve / RFR transition (2006), so its
  curve-construction and caplet material is LIBOR-era. Take BM for model
  *theory* (short-rate, LMM drifts, SABR dynamics, calibration); use II–IV
  for present-day conventions and plumbing.
- **GLA** is instrument-agnostic MC methodology — no rates conventions, no
  curve construction, no SABR. Take the numerical machinery (RNG, paths,
  variance reduction, discretization, regression); get the finance from the
  other sources.
- **CPP** covers no financial math at all, and its worked examples are
  equity/FX/MC (Black–Scholes, Asian options), not rates. Take the design,
  not the instruments — and modernize the pre-C++11 idioms.
- **IQ** documents QuantLib design and supporting utilities; it is not a
  current market-conventions source and not a mandate to reproduce QuantLib's
  inheritance, lazy evaluation, or Observer machinery in this small engine.
- **PRIMER** is a C++11 book (2013). It does **not** cover C++20: no concepts
  (`concept`, `requires`, `std::invocable`), no ranges, no `<=>`, no modules.
  Phase 2's `ScalarResidual` concept in `src/core/bracketed_bisection.hpp` is
  outside its scope — use TOUR Ch.8 for that. Everything else the repo
  currently uses — move, forwarding, templates, smart pointers, virtuals — is
  covered, and covered in depth. It contains no financial math whatsoever.
- **TOUR** is an overview, not a manual: it names features and shows idiomatic
  use, but rarely derives mechanism. It will tell you `std::forward` preserves
  value category; it will not derive the reference-collapsing rules that make
  it work. Use it to learn *that* a C++20 feature exists and how it reads in
  practice, then go to PRIMER (pre-C++20 topics) or cppreference (C++20 ones)
  for detail. Like PRIMER, no financial math.

## Citation discipline

Every `docs/math_notes/<chapter>_<topic>.md` must start with a `## Sources`
section listing which PDF section(s) the note draws from. Example:

```markdown
# 01 — Swap valuation

## Sources
- Primary: II §3.2.2 (Vanilla IRS), II §6.1 (day count)
- Secondary: IRC Ch.1
```

This makes it easy for AI assistants and for future-you to verify
formulas against the right source.
