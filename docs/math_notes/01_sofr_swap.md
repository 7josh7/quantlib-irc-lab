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

Consider a collateralized fixed-for-floating interest rate swap observed at valuation time $t_0$.

The trade has notional $N$, effective date $T_0$, maturity date $T^x_{M^x}$, and payment dates $T^x_i$ for $i \in [1, 2, \dots, M^x]$, where $x \in \{\mathrm{fix}, \mathrm{flt}\}$ labels the fixed and floating legs and $M^{\mathrm{fix}}$, $M^{\mathrm{flt}}$ are their respective numbers of payments.

The swap exchanges a fixed leg against a floating leg. We take the perspective of a **fixed-rate recevier**, meaning the holder receives fixed coupons and pays floating coupons. The opposite sign convention gives the fixed-rate payer.

### Collateralization and discounting

Assume the trade is perfectly collateralized under collateral index $\mathcal D$.
The collateral curve provides discount factors

$$
P_{\mathcal D}(t_0,T).
$$

In a USD SOFR-collateralized setup, $\mathcal D$ is typically the SOFR/OIS
discounting curve.

The floating index $\mathcal I$ and the collateral/discounting index
$\mathcal D$ may be different:

$$
\mathcal I
=
\text{projection / forwarding index},
$$

$$
\mathcal D
=
\text{collateral / discounting index}.
$$

For each floating accrual period

$$
\left[T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}\right],
$$

let

$$
R_{\mathcal I}
\left(
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right)
$$

denote the realized floating rate of index $\mathcal I$ over that accrual period.

Before the rate is fixed or realized, its collateralized projected value at
valuation time $t_0$ is

$$
F_{\mathcal I,\mathcal D}
\left(
t_0;T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right)
:=
\mathbb E_{t_0}^{T_j^{\mathrm{flt}},\mathcal D}
\left[
R_{\mathcal I}
\left(
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right)
\right].
$$



## 2. Floating leg math

> *(Fill in: geometric compounding formula from II §2.2,
> $R_c = \frac{1}{\tau}\left[\prod_i (1 + \tau_i R_i) - 1\right]$,
> and the approximation $R_c \approx \frac{1}{\tau}(\exp\int r\,du - 1)$.
> Note that the rate is $\mathcal{F}(T_e)$-measurable, not
> $\mathcal{F}(T_s)$.)*

The floating leg references an index $\mathcal I$ over each floating accrual period

$$
[T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}].
$$

Let the floating-leg year fraction be

$$
\tau_j^{\mathrm{flt}}
=
\tau^{\mathrm{flt}}
\left(
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right).
$$

The floating cash flow is

$$
\mathrm{CF}_j^{\mathrm{flt}}
=
N\tau_j^{\mathrm{flt}}
R_{\mathcal I}
\left(
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right).
$$

If the floating leg has a spread $s$, then

$$
\mathrm{CF}_j^{\mathrm{flt}}
=
N\tau_j^{\mathrm{flt}}
\left[
R_{\mathcal I}
\left(
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right)
+s
\right].
$$

Thus, the floating coupon projected at $t_0$ is

$$
N\tau_j^{\mathrm{flt}}
F_{\mathcal I,\mathcal D}
\left(
t_0;T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right),
$$

and its present value is

$$
N\tau_j^{\mathrm{flt}}
P_{\mathcal D}
\left(
t_0,T_j^{\mathrm{flt}}
\right)
F_{\mathcal I,\mathcal D}
\left(
t_0;T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right).
$$

---
### IBOR-style floating leg

For an IBOR-style floating leg, the rate is fixed in advance. The fixing date is

$$
T_j^{\mathrm{fix}}
\leq
T_{j-1}^{\mathrm{flt}}.
$$

The realized coupon rate is

$$
R_{\mathcal I}
\left(
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right)
=
L_{\mathcal I}
\left(
T_j^{\mathrm{fix}};
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right).
$$

So the coupon is known at or before the start of the accrual period.

---

### RFR / SOFR-style floating leg

For an overnight RFR floating leg, such as SOFR, the coupon is compounded from daily overnight fixings over the observation period.

Let the observation dates be

$$
u_0,u_1,\dots,u_n,
$$

with daily accrual fractions $\delta_k$. The geometrically compounded RFR rate is

$$
R_{\mathrm{RFR}}
\left(
T_{j-1}^{\mathrm{flt}},T_j^{\mathrm{flt}}
\right)
=
\frac{1}{\tau_j^{\mathrm{flt}}}
\left[
\prod_{k=0}^{n-1}
\left(
1+\delta_k r_{\mathrm{RFR}}(u_k)
\right)
-1
\right].
$$

This rate is fixed in arrears because the coupon is not fully known until the relevant overnight fixings have been observed.

Common RFR conventions include:

- **look-back:** the observation period is shifted backward by a fixed number of business days;
- **lockout:** the last observed rate is repeated for the final few days of the accrual period;
- **payment lag:** payment occurs a fixed number of business days after the accrual end date;
- **business day convention:** usually modified following;
- **day count:** usually Act/360 for USD SOFR.

## 3. Fixed leg math

> *(Fill in: cash flow $\tau_i \cdot S$ per period, NPV, annuity
> $A(t) = \sum_i \tau_i P^c(t, T_i)$ per II §3.2.2.)*

The fixed leg pays a deterministic coupon on each fixed-leg payment date $T_i^{\mathrm{fix,pay}}$.

For accrual period $[T_{i-1}^{\mathrm{fix}},T_i^{\mathrm{fix}}]$, define the fixed-leg year fraction

$$
\tau_i^{\mathrm{fix}}
=
\tau^{\mathrm{fix}}
\left(
T_{i-1}^{\mathrm{fix}},T_i^{\mathrm{fix}}
\right).
$$

If the fixed coupon rate is $K$, then the fixed-leg cash flow is

$$
\mathrm{CF}_i^{\mathrm{fix}}
=
N K \tau_i^{\mathrm{fix}}.
$$

Therefore, the present value of fixed leg can be represented by a function of annuity
$$
N K A(t) = N K \sum_i \tau_i^{\mathrm{fix}} P_{\mathcal D}(t, T_i)
$$

Typical fixed-leg conventions:

- payment frequency: annual or semiannual,
- day count: $30/360$ or Act/360 depending on market,
- business day convention: modified following,
- payment lag: usually $0$ or a small number of business days.
## 4. Swap valuation formula

> *(Fill in: receiver NPV
> $V^{\text{rec}}(t; S) = S \cdot A(t) - \sum_i \tau_i P^c(t, T_i)
> \mathbb{E}_t^{T_i^c}[R^{\text{sofr}}(T_{i-1}, T_i)]$.
> Show what simplifies when the projection curve and discount curve
> coincide.)*

Value of receiver swap is present value of fixed leg mimus present value of floating leg.  That is
$$
V^{\text{rec}}(t; K) = N [K \cdot A_{\mathcal D}(t) - \sum_i \tau_i P_{\mathcal D}(t, T_i) \mathbb{E}_t^{T_i,\mathcal D}[R_{\text{sofr}}(T_{i-1}, T_i)]] 
$$
where

$$
A_{\mathcal D}
=
\sum_i \tau_i P_{\mathcal D}(t,T_i).
$$

Again, define the collateralized projected forward rate

$$
F_i^{\mathcal I,\mathcal D}(t)
:=
\mathbb E_t^{T_i,\mathcal D}
\left[
R_{\mathcal I}(T_{i-1},T_i)
\right].
$$
we have
$$
V^{\text{rec}}(t; K) = N \sum_i \tau_i P_{\mathcal D}(t,T_i)[K - F_i^{\mathcal I,\mathcal D}(t)]
$$

In a single curve model, since we have $F_i(t) = \frac{1}{\tau_i}\frac{P(t, T_{i-1}) - P(t, T_i)}{P(t, T_i)}$, floating leg can  be further simplified as

$$
PV_{\mathrm{flt}}(t)
=
N\sum_i
\left[
P(t,T_{i-1})-P(t,T_i)
\right]
=
N
\left[
P(t,T_0)-P(t,T_n)
\right].
$$

The simplification above sets the spread $s=0$; a nonzero spread adds
$N\,s\sum_i \tau_i^{\text{flt}} P_{\mathcal D}(t,T_i)$ to the floating-leg PV.

So the receiver swap value becomes

$$
V^{\mathrm{rec}}(t;K)
=
N
\left[
K A(t)
-
\left(
P(t,T_0)-P(t,T_n)
\right)
\right],
$$

where

$$
A(t)
=
\sum_i \tau_i P(t,T_i).
$$

This closed form is **exact only** in the idealized case where each floating
coupon pays at its accrual-period end and the projection curve coincides with
the discount curve $\mathcal D$. Payment lag, look-back, or lockout (see §2)
break the telescoping and reintroduce a small timing/convexity adjustment, so
the identity above is the single-curve, period-end-payment case — not an
unconditional equality.

## 5. Par swap rate

> *(Fill in: $S^*(t) = $ floating-leg PV / annuity. State why this is
> what a "par" or "spot-starting" SOFR swap quotes against.)*

The par swap rate $S_t$ is defined by setting $V^{\mathrm{rec}}(t;S_t)=0$:

$$
S_t
=
\frac{\sum_i \tau_i P_{\mathcal D}(t,T_i)F_i^{\mathcal I,\mathcal D}(t)}{A_{\mathcal D}(t)}.
$$

## 6. Assumptions

> *(Fill in. At minimum: perfect collateralization, SOFR collateral = SOFR
> projection, day count convention, business day convention, no
> convexity adjustment for OIS.)*

perfect collateralization, SOFR collateral = SOFR

day count convention: TBD (refer quantlib), 

business day convention TBD (refer quantlib), 

no convexity adjustment for OIS (add it later)

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

## 11. Appendix

### Derivation of the Collateralized Forward-Rate Definition

Assume a payoff referencing an index $\mathcal I$, but collateralized under collateral index $\mathcal D$.  
Let

$$
t \le T_s < T_e,
$$

where:

- $t$ is the valuation time,
- $T_s$ is the accrual start date,
- $T_e$ is the accrual end / payment date,
- $\tau_{s,e}$ is the accrual year fraction,
- $R_{\mathcal I}(T_s,T_e)$ is the realized floating rate of index $\mathcal I$ over $[T_s,T_e]$,
- $r_{\mathcal D}(u)$ is the collateral remuneration rate.

Assume **perfect collateralization**, meaning the trade is continuously and fully collateralized:

$$
C_t = V_t.
$$

Under this assumption, the collateral account is the pricing numeraire:

$$
B_{\mathcal D}(t)
=
\exp\left(\int_0^t r_{\mathcal D}(u)\,du\right).
$$

Therefore, under the collateralized pricing measure $Q^{\mathcal D}$,

$$
\frac{V_t}{B_{\mathcal D}(t)}
$$

is a martingale.

---

### 1. Price a collateralized FRA-like payoff

Consider a payoff at $T_e$:

$$
X_{T_e}
=
N\tau_{s,e}
\left(
R_{\mathcal I}(T_s,T_e)-K
\right).
$$

By collateralized pricing,

$$
V_t
=
\mathbb E_t^{Q^{\mathcal D}}
\left[
\exp\left(-\int_t^{T_e} r_{\mathcal D}(u)\,du\right)
X_{T_e}
\right].
$$

Substituting the payoff gives

$$
V_t
=
N\tau_{s,e}
\mathbb E_t^{Q^{\mathcal D}}
\left[
\exp\left(-\int_t^{T_e} r_{\mathcal D}(u)\,du\right)
\left(
R_{\mathcal I}(T_s,T_e)-K
\right)
\right].
$$

Define the collateralized discount factor

$$
P_{\mathcal D}(t,T_e)
:=
\mathbb E_t^{Q^{\mathcal D}}
\left[
\exp\left(-\int_t^{T_e} r_{\mathcal D}(u)\,du\right)
\right].
$$

Now change numeraire to the $T_e$-forward measure associated with the collateral curve $\mathcal D$, denoted

$$
Q^{T_e,\mathcal D}.
$$

Then

$$
V_t
=
N\tau_{s,e}P_{\mathcal D}(t,T_e)
\mathbb E_t^{T_e,\mathcal D}
\left[
R_{\mathcal I}(T_s,T_e)-K
\right].
$$

So

$$
V_t
=
N\tau_{s,e}P_{\mathcal D}(t,T_e)
\left(
\mathbb E_t^{T_e,\mathcal D}
\left[
R_{\mathcal I}(T_s,T_e)
\right]
-K
\right).
$$

For a fair forward contract, $V_t=0$. Hence the fair fixed rate $K$ satisfies

$$
K
=
\mathbb E_t^{T_e,\mathcal D}
\left[
R_{\mathcal I}(T_s,T_e)
\right].
$$

Therefore define the collateralized forward rate as

$$
F_{\mathcal I,\mathcal D}(t;T_s,T_e)
:= K = 
\mathbb E_t^{T_e,\mathcal D}
\left[
R_{\mathcal I}(T_s,T_e)
\right].
$$

This is the forward rate of index $\mathcal I$, priced under collateralization index $\mathcal D$.

---

### 2. Define the instantaneous-forward representation

In the single-curve case, a finite-tenor forward rate can be written as

$$
F(t;T_s,T_e)
=
\frac{1}{\tau_{s,e}}
\left(
\exp\left(\int_{T_s}^{T_e} f(t,u)\,du\right)-1
\right),
$$

where $f(t,u)$ is the instantaneous forward rate.

In the multi-curve case, the forward index $\mathcal I$ and the collateral curve $\mathcal D$ are different, so the forward rate cannot generally be derived from a single discount curve. We then introduce a pseudo-instantaneous forward curve
$f_{\mathcal I,\mathcal D}(t,u)$ such that

$$
F_{\mathcal I,\mathcal D}(t;T_s,T_e)
=
\frac{1}{\tau_{s,e}}
\left(
\exp\left(
\int_{T_s}^{T_e}
f_{\mathcal I,\mathcal D}(t,u)\,du
\right)-1
\right).
$$

Equivalently,

$$
\int_{T_s}^{T_e}
f_{\mathcal I,\mathcal D}(t,u)\,du
=
\log\left(
1+\tau_{s,e}F_{\mathcal I,\mathcal D}(t;T_s,T_e)
\right).
$$


Combining this with the fair-forward identity gives

$$
\boxed{
\mathbb E_t^{T_e,\mathcal D}
\left[
R_{\mathcal I}(T_s,T_e)
\right] =
\frac{1}{\tau_{s,e}}
\left(
\exp\left(\int_{T_s}^{T_e}
f_{\mathcal I,\mathcal D}(t,u)\,du\right)-1
\right)
}
$$
So $F_{\mathcal I,\mathcal D}$ is defined by collateralized pricing, while
$f_{\mathcal I,\mathcal D}$ is introduced as a continuous curve representation
of those finite-tenor forward rates.

A single market forward rate does not uniquely determine the pointwise function
$u \mapsto f_{\mathcal I,\mathcal D}(t,u)$. It only determines the integral of
that function over $[T_s,T_e]$. To construct the full curve, we need multiple
market instruments plus a bootstrapping and interpolation rule.