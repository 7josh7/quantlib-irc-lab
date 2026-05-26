# Source mapping

Five source PDFs, with different scopes. Use both source sets — they cover
different phases of the project.

## Sources

| ID | Title | Role |
|----|-------|------|
| **IRC** | Lesniewski, *Interest Rate and Credit Models* (`IRC.pdf`) | Single-author full-arc text. Spine for back-half phases (LMM, Bermudan/LSM, CCR depth). |
| **I** | *Modern Pricing Theory in Practice* (`note/I. ...pdf`) | Foundations: martingale measures, **CSA-collateralized pricing**, xVA decomposition (CVA/DVA/FVA/MVA/KVA), CVA integral with EE/PD/LGD, **Monte Carlo regression** (basis functions + least-squares). |
| **II** | *Yield Curve And All That* (`note/II. ...pdf`) | Curve construction, modern: simple vs compound rates, multi-curve, **SOFR-centric bootstrap with futures + OIS** (worked Table 1), cross-currency basis swaps, FX-forward currency chains, **Jacobian risk representation**, **PCA hedging and eigen-scenarios** (worked numerical example). |
| **III** | *Volatility Modeling* (`note/III. ...pdf`) | Vol theory: Bachelier, Dupire local vol, **SABR** (Hagan formula, ATM parameterization, smile-risk Greeks, Bartlett's modified delta). |
| **IV** | *Breaking Down RFR Modeling* (`note/IV. ...pdf`) | RFR-specific vol: **backward-looking vs forward-looking caplets**, **time-decay SABR** for compounded rates, bottom-up basket aggregation. |

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

## What each source does NOT cover

- **IRC** doesn't deeply cover: SOFR-era multi-curve construction with
  explicit instrument tables, xCcy currency chains, time-decay SABR for
  RFR, PCA-based stress scenarios, modern xVA decomposition. Use II–IV
  for these.
- **Four-part series** doesn't cover: Hull–White, Vasicek, LMM, Bermudan
  swaption / LSM in depth, MBS, full CCR exposure simulation methodology.
  Use IRC for these.

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
