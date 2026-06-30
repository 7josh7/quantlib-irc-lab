# Source mapping

Six source PDFs in two categories: **five math/finance references** (the
four-part series I–IV plus IRC) and **one code-architecture reference**
(CPP). The math references map to phases by topic (table below); CPP is
cross-cutting — it informs how the C++ is structured, not what the math is.

## Sources

| ID | Title | Role |
|----|-------|------|
| **IRC** | Lesniewski, *Interest Rate and Credit Models* (`IRC.pdf`) | Single-author full-arc text. Spine for back-half phases (LMM, Bermudan/LSM, CCR depth). |
| **I** | *Modern Pricing Theory in Practice* (`note/I. ...pdf`) | Foundations: martingale measures, **CSA-collateralized pricing**, xVA decomposition (CVA/DVA/FVA/MVA/KVA), CVA integral with EE/PD/LGD, **Monte Carlo regression** (basis functions + least-squares). |
| **II** | *Yield Curve And All That* (`note/II. ...pdf`) | Curve construction, modern: simple vs compound rates, multi-curve, **SOFR-centric bootstrap with futures + OIS** (worked Table 1), cross-currency basis swaps, FX-forward currency chains, **Jacobian risk representation**, **PCA hedging and eigen-scenarios** (worked numerical example). |
| **III** | *Volatility Modeling* (`note/III. ...pdf`) | Vol theory: Bachelier, Dupire local vol, **SABR** (Hagan formula, ATM parameterization, smile-risk Greeks, Bartlett's modified delta). |
| **IV** | *Breaking Down RFR Modeling* (`note/IV. ...pdf`) | RFR-specific vol: **backward-looking vs forward-looking caplets**, **time-decay SABR** for compounded rates, bottom-up basket aggregation. |
| **CPP** | Joshi, *C++ Design Patterns and Derivatives Pricing*, 2nd ed. (`note/cpp-design-patterns-and-derivatives-pricing.pdf`) | **Code architecture, not math.** Design patterns for a pricing library: open–closed payoff/curve hierarchy (Ch.2–3), bridge + virtual constructor + parameters class (Ch.4), strategy + decoration + statistics gatherer (Ch.5), a random-numbers class (Ch.6), a templatized exotics/MC engine (Ch.7), trees (Ch.8), solver function-objects (Ch.9), the factory (Ch.10, 14), exception-safe smart-pointer idioms (Ch.13), decoupling/levelization (Ch.16). Equity/FX/MC-flavored and pre-C++11 — **port the patterns, modernize the idioms**. |

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

## Phase → source mapping

| Phase | Primary | Secondary |
|-------|---------|-----------|
| 0 — Env, SOFR hello-swap | II §3.3.1 (SOFR curve calibration), II §3.2.2 (IRS) | IRC Ch.1 |
| 1 — Mini swap pricer | II §3.2.2, II §6.1 (day count) | IRC Ch.1 |
| 2 — Curve bootstrap + DV01 | II §3.3 (multi-curve), II §3.4 (Jacobian risk) | IRC Ch.1 |
| 4 — Risk report (DV01/KRD/scenarios + PCA) | II §4.2–4.3 (PCA hedging + eigen-scenarios), II §5 (steepener) | — |
| 4.5 (optional) — xVA awareness | I §2 (CSA replication), I §3.1 (xVA decomposition), I §3.2 (CVA) | — |
| 5 — CDS, survival curve | I §3.2 (CVA framework), IRC Ch.2–3 (CDS depth) | — |
| 6 — Hull–White | IRC Ch.10 | — (not in four-part series) |
| 7 — SABR / swaption / RFR caplet | III §4.3–4.8 (SABR + smile Greeks), IV §3 (time-decay SABR for RFR) | IRC Ch.8–9 |
| 8 — LMM | IRC Ch.11 | III §4.5 (parameterization considerations) |
| 9 — LSM Bermudan | IRC Ch.12 (optimal stopping), I §3.4 (MC regression framework) | — |
| 10 — CCR / exposure / CVA | IRC Ch.15, I §3.2–3.3 (EE, MPoR, nested-MC critique) | — |

## Where CPP applies (code architecture, cross-cutting)

CPP is deliberately left out of the phase table above — it's not a per-phase
*math* source. It applies wherever a phase needs a non-trivial class design:

| Phase / area | CPP material |
|--------------|--------------|
| 1 — Mini pricer | Open–closed `yield_curve` base + flat impl (Ch.2–3); floating-leg simple vs daily-compounded **Strategy** (Ch.5); bridge + parameters class (Ch.4). |
| 4 — Scenario engine | Deterministic shocks + PCA eigen-scenarios behind one interface = **Strategy** (Ch.5). |
| 6 — Hull–White | Trinomial-tree class design (Ch.8). |
| 7 — Implied vol / SABR calibration | Solver via **function objects** + Newton–Raphson (Ch.9). |
| Roadmap-only — hand-rolled LMM / LSM Monte Carlo | **Most valuable here.** MC framework design: RNG class (Ch.6), statistics gatherer (Ch.5), exotics/path-generation engine (Ch.7), templatized factory (Ch.10, 14). |
| Throughout | Exception safety + smart pointers (Ch.13); header/levelization decoupling (Ch.16). |

## What each source does NOT cover

- **IRC** doesn't deeply cover: SOFR-era multi-curve construction with
  explicit instrument tables, xCcy currency chains, time-decay SABR for
  RFR, PCA-based stress scenarios, modern xVA decomposition. Use II–IV
  for these.
- **Four-part series** doesn't cover: Hull–White, Vasicek, LMM, Bermudan
  swaption / LSM in depth, MBS, full CCR exposure simulation methodology.
  Use IRC for these.
- **CPP** covers no financial math at all, and its worked examples are
  equity/FX/MC (Black–Scholes, Asian options), not rates. Take the design,
  not the instruments — and modernize the pre-C++11 idioms.

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
