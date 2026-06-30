# 01 — SOFR OIS Swap Valuation

> **Owner-written note.** AI assistants must not fill in the math sections.
> Structure and source citations are scaffolded; the body is yours.

## Sources

- **Primary:** II §3.2.2 (Vanilla IRS), II §2.2 (Compound interest rates),
  II §3.3.1 (SOFR curve calibration)
- **Secondary:** II §2.3.2 (RFR-style forward rate), IRC Ch.1
- See [`docs/sources.md`](../sources.md) for full mapping.

## Scope

This note covers the math for a **USD SOFR OIS swap** — the Phase 0
hello-world deliverable. It is the foundation for everything in Phases 1–4.

Key conventions to nail down here:

- SOFR is a daily overnight RFR, fixed-in-arrears.
- The floating leg pays a **geometrically compounded daily SOFR rate**
  over each accrual period (see II §2.2).
- Discounting is at the **SOFR collateral curve** (perfect-collateralization
  assumption per Source I §2.2).
- This is the post-LIBOR multi-curve framework reduced to its simplest
  case: discounting curve = projection curve = SOFR curve.

## 1. Instrument definition

> *(Fill in: payment schedule, fixed leg, floating leg, notional, day
> counts, business day convention, payment lag, look-back / lockout
> conventions if used.)*

## 2. Floating leg math

> *(Fill in: geometric compounding formula from II §2.2,
> $R_c = \frac{1}{\tau}\left[\prod_i (1 + \tau_i R_i) - 1\right]$,
> and the approximation $R_c \approx \frac{1}{\tau}(\exp\int r\,du - 1)$.
> Note that the rate is $\mathcal{F}(T_e)$-measurable, not
> $\mathcal{F}(T_s)$.)*

## 3. Fixed leg math

> *(Fill in: cash flow $\tau_i \cdot S$ per period, NPV, annuity
> $A(t) = \sum_i \tau_i P^c(t, T_i)$ per II §3.2.2.)*

## 4. Swap valuation formula

> *(Fill in: receiver NPV
> $V^{\text{rec}}(t; S) = S \cdot A(t) - \sum_i \tau_i P^c(t, T_i)
> \mathbb{E}_t^{T_i^c}[R^{\text{sofr}}(T_{i-1}, T_i)]$.
> Show what simplifies when the projection curve and discount curve
> coincide.)*

## 5. Par swap rate

> *(Fill in: $S^*(t) = $ floating-leg PV / annuity. State why this is
> what a "par" or "spot-starting" SOFR swap quotes against.)*

## 6. Assumptions

> *(Fill in. At minimum: perfect collateralization, SOFR collateral = SOFR
> projection, day count convention, business day convention, no
> convexity adjustment for OIS.)*

## 7. Inputs / outputs (for the implementation)

> *(Fill in what the pricer takes and returns. Example structure:)*
>
> **Inputs:** valuation date, notional, fixed rate, schedule, day counter,
> SOFR curve (discount + projection — same curve here).
>
> **Outputs:** NPV, fair fixed rate, annuity, leg PVs.

## 8. Known limitations

> *(Fill in. Examples to think about: no convexity adjustment for the
> daily-compounded approximation; assumes SOFR collateral; no payment
> lag / lock-out / look-back modeling in v1; flat curve only in
> hello-world.)*

## 9. Tests we'd want

> *(Fill in. Suggested:)*
>
> - Par swap NPV ≈ 0 at fair rate.
> - Receiver vs payer sign flip.
> - Fair rate is positive and in plausible range for the given curve.
> - Diff vs QuantLib `MakeOIS` on the same trade < 1e-6.
> - Annuity equals sum of $\tau_i P^c(t, T_i)$ over fixed schedule.

## 10. Open questions

> *(Anything you want to flag before the implementer touches it. e.g.,
> "Should we model payment lag in v1 or punt to Phase 2?")*
