# Source mapping

Eight source PDFs: **seven math/finance references** (the four-part series
I–IV, IRC, plus two textbook back-stops — **BM** for rate-model theory and
**GLA** for Monte Carlo methodology) and **one code-architecture reference**
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
| **BM** | Brigo & Mercurio, *Interest Rate Models — Theory and Practice (With Smile, Inflation and Credit)*, 2nd ed. (Springer 2006) (`note/[Springer 2006] Interest Rate Models ...pdf`) | Deep theory back-stop for the back half. Cap/floor & swaption Black/Bachelier formulas (Ch.1); **short-rate models** — Vasicek, CIR, Hull–White, CIR++ fit to the initial curve (Ch.3), G2++ two-factor (Ch.4); the **LIBOR/swap market models** and calibration (Ch.6–7), MC tests of LFM approximations (Ch.8); **SABR as a stochastic-vol extension** (Ch.11); counterparty risk + intensity/CDS models (Ch.21–22). Rigorous and rates-native — complements IRC on model depth. **LIBOR-era (2006): take the model theory, not the pre-SOFR curve plumbing.** |
| **GLA** | Glasserman, *Monte Carlo Methods in Financial Engineering* (Springer 2003) (`note/Monte_Carlo_Methods_In_Financial_Enginee.pdf`) | **Numerical methodology, not instruments.** The MC reference: RNG + normal-vector generation (Ch.2), Brownian path / bridge construction (Ch.3), variance reduction (Ch.4), quasi-MC (Ch.5), SDE discretization — Euler / second-order (Ch.6), MC Greeks (Ch.7), **American-option pricing incl. regression-based Longstaff–Schwartz and duality upper bounds** (Ch.8 §8.6–8.7), loss probabilities / VaR / credit risk (Ch.9). Pairs with CPP: **GLA is what the numerics do, CPP is how to structure the classes.** |
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

**BM** and **GLA** are textbook back-stops, not per-phase primaries. Reach
for **BM** when a back-half model needs more rigor or a second derivation
than IRC gives (short-rate calibration, LMM drifts, SABR-as-stochastic-vol)
— but for anything SOFR/RFR-era, II–IV override BM's LIBOR-era conventions.
Reach for **GLA** whenever Monte Carlo enters the picture (LMM paths, LSM
regression, CCR exposure simulation): it governs the numerics, while CPP
governs the class design.

## Phase → source mapping

| Phase | Primary | Secondary |
|-------|---------|-----------|
| 0 — Env, SOFR hello-swap | II §3.3.1 (SOFR curve calibration), II §3.2.2 (IRS) | IRC Ch.1 |
| 1 — Mini swap pricer | II §3.2.2, II §6.1 (day count) | IRC Ch.1 |
| 2 — Curve bootstrap + DV01 | II §3.3 (multi-curve), II §3.4 (Jacobian risk) | IRC Ch.1 |
| 4a (optional, post-MVP) — xCcy / foreign-collateralized discounting | II §3.2.3–3.2.5 (basis, xCcy non-MTM, FX chain), II §3.3.2–3.3.3 (forward via basis, foreign-collateral curves) | I §2 (collateralized pricing) |
| 4 — Risk report (DV01/KRD/scenarios + PCA) | II §4.2–4.3 (PCA hedging + eigen-scenarios), II §5 (steepener) | — |
| 4.5 (optional) — xVA awareness | I §2 (CSA replication), I §3.1 (xVA decomposition), I §3.2 (CVA) | — |
| 5 — CDS, survival curve | I §3.2 (CVA framework), IRC Ch.2–3 (CDS depth) | BM Ch.21–22 (intensity / CDS models) |
| 6 — Hull–White | IRC Ch.10 | BM Ch.3–4 (short-rate models, CIR++ curve fit, G2++) |
| 7 — SABR / swaption / RFR caplet | III §4.3–4.8 (SABR + smile Greeks), IV §3 (time-decay SABR for RFR) | IRC Ch.8–9; BM Ch.1 & Ch.11 (Black/Bachelier formulas; SABR) |
| 8 — LMM | IRC Ch.11, BM Ch.6–7 (LFM/LSM + calibration) | GLA Ch.2–6 (MC methodology); BM Ch.8; III §4.5 (parameterization) |
| 9 — LSM Bermudan | IRC Ch.12 (optimal stopping), I §3.4 (MC regression), GLA Ch.8 §8.6 (Longstaff–Schwartz) | BM Ch.8 (LFM MC tests) |
| 10 — CCR / exposure / CVA | IRC Ch.15, I §3.2–3.3 (EE, MPoR, nested-MC critique) | GLA Ch.9 (VaR/PFE, credit risk) |

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
