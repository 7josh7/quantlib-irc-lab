# 02 SOFR Curve Bootstrapping + DV01

## Sources
- Source II 3.3
- Source II 3.3.1
- Source II 3.4
- QuantLib docs/examples
- CFTC SOFR OIS specification — https://www.cftc.gov/PressRoom/PressReleases/8745-23
- CME Three-Month SOFR futures specification —
  https://www.cmegroup.com/education/articles-and-reports/understanding-sofr-futures
- NY Fed SOFR publication — https://www.newyorkfed.org/markets/reference-rates/sofr
  (source of the historical fixings used in the deterministic hybrid fixture;
  publication timing and same-day revision window per this source, §Market Instruments)

## Notation

Indices are used consistently as follows:

- $m$: calibration instrument;
- $i$: floating-leg accrual period;
- $j$: fixed-leg accrual period;
- $k$: daily SOFR fixing.

| Symbol | Definition |
| --- | --- |
| $t$ | Generic valuation time. The curve-construction sections set $t=0$. |
| $T_s,T_e$ | Generic start and end dates of a compounded-SOFR accrual period. |
| $T_{s,m},T_{e,m}$ | Start and end dates of the reference period for futures contract $m$. |
| $T_{i-1},T_i$ | Start and end dates of floating-leg accrual period $i$. |
| $U_i$ | Payment date of floating-leg coupon $i$. |
| $V_j$ | Payment date of fixed-leg coupon $j$. |
| $L_m$ | Curve pillar date associated with calibration instrument $m$. |
| $n$ | Number of daily fixing subperiods in a generic compounded-SOFR accrual period. |
| $n_m$ | Number of accrual periods in OIS calibration instrument $m$. |
| $\tau_{s,e}$ | Actual/360 year fraction over generic accrual period $[T_s,T_e]$. |
| $\tau_m$ | Actual/360 year fraction over futures reference period $[T_{s,m},T_{e,m}]$. |
| $\tau_i$ | Floating-leg accrual fraction over $[T_{i-1},T_i]$. |
| $\alpha_j$ | Fixed-leg accrual fraction for fixed coupon $j$. |
| $r_k$ | Daily SOFR fixing applicable for $\delta_k$ years. |
| $P^{\mathrm{SOFR}}(t,T)$ | Extended SOFR zero-coupon bond price at time $t$ with maturity $T$. |
| $P^{\mathrm{SOFR}}(t;T_s,T_e)$ | Forward zero-coupon bond price $P^{\mathrm{SOFR}}(t,T_e)/P^{\mathrm{SOFR}}(t,T_s)$. The semicolon distinguishes this three-date object from the two-date extended bond price $P^{\mathrm{SOFR}}(t,T)$. |
| $A^{\mathrm{SOFR}}(T_s,t)$ | Realized SOFR accumulation factor from $T_s$ through $t$. |
| $f^{\mathrm{SOFR}}(0,T)$ | Instantaneous SOFR forward rate at maturity $T$. |
| $R_{\mathrm{cmp}}^{\mathrm{SOFR}}(T_s,T_e)$ | Realized annualized compounded-SOFR rate over $[T_s,T_e]$. |
| $F^{\mathrm{SOFR}}(t;T_s,T_e)$ | Curve-implied forward annualized compounded-SOFR rate. |
| $Q_m$ | Quoted IMM price for SR3 futures contract $m$. |
| $R_{\mathrm{fut},m}$ | Futures-implied rate $(100-Q_m)/100$. |
| $S_m$ | Market par-rate quote for OIS calibration instrument $m$. |
| $K$ | Contractual fixed rate of a generic swap. For a par calibration swap, $K=S_m$. |
| $K_{\mathrm{model},m}(x_m)$ | Model-implied par rate of OIS $m$ for trial curve parameter $x_m$. |
| $\mathcal N$ | Swap notional. |
| $\mathcal A_m^{\mathrm{fix}}$ | Fixed-leg annuity of OIS $m$. |
| $\text{PV}_{\text{fixed}}$ | Present value of the fixed leg. |
| $\text{PV}_{\text{float}}$ | Present value of the floating leg. |
| $x_m$ | New scalar curve parameter, usually $\log P^{\mathrm{SOFR}}(0,L_m)$. |
| $\Delta_{\text{bp}}$ | One basis point bump, usually $10^{-4}$. |
| $\text{DV01}$ | Change in PV for a one-basis-point rate bump, with sign convention stated separately. |
| $\epsilon_m$ | Repricing error for calibration instrument $m$. |
| $\mathbf q$ | Vector of market quotes $(q_1,\dots,q_M)$ — the futures rates $R_{\mathrm{fut},m}$ and OIS par rates $S_m$. |
| $\boldsymbol\theta$ | Vector of calibrated curve node values, $\theta_m=\log P^{\mathrm{SOFR}}(0,L_m)$; equals the vector of node coordinates $x_m$ after calibration. |
| $\mathcal J$ | Calibration Jacobian $\partial\mathbf g/\partial\boldsymbol\theta$, entries $\mathcal J_{mn}=\partial g_m/\partial\theta_n$ (model quotes w.r.t. curve nodes); lower-triangular for a sequential bootstrap. Phase 2 approximates it by central finite differences; an analytic Jacobian is future work. |

## Market Instruments
### SOFR Futures
Three-Month SOFR futures are used for front-to-belly curve construction. We assume no convexity adjustment here and leave it to future implementation where a stochastic model is introduced.

For futures contract $m$, the quoted IMM price is $Q_m=100-100R_{\mathrm{fut},m}$, where $R_{\mathrm{fut},m}$ is the annualized business-day-compounded SOFR rate implied for the contract reference quarter. One-Month SOFR futures use a simple arithmetic average instead. Ignoring convexity, the futures-implied rate is identified with the forward compounded-SOFR rate over the reference quarter; the constraint this places on the discount curve is written out in the Calibration Equations section.

SOFR is published on each U.S. business day at approximately 8:00 a.m. ET. The
New York Fed may publish a revision at approximately 2:30 p.m. ET on the same
day, and each publication carries the rate date of the preceding U.S. business
day.

The inputs form a **deterministic hybrid test fixture** as of **2026-01-15 at
3:00 p.m. ET**, after the approximately 2:30 p.m. revision window. Historical
SOFR fixings are final values published by the New York Fed, while the SR3 and
OIS quotes are synthetic but internally consistent. This fixture is not
presented as a historical market snapshot. The final SOFR fixing with rate date
2026-01-14 is known. SR3Z25 is the first instrument and is partially accrued,
so its realized accumulation uses final SOFR fixings from its 2025-12-17
reference-quarter start through 2026-01-14. The later contracts are fully
forward. The strip is SR3Z25--SR3Z28 (13 consecutive contracts).

### OIS Swaps
An OIS instrument $m$ is quoted at the par fixed rate $S_m$ that makes the fixed and floating legs have equal present value at inception. The fixed leg pays $S_m$ on an annual annuity; the floating leg pays the annualized business-day-compounded SOFR rate $R_{\mathrm{cmp}}^{\mathrm{SOFR}}(T_{i-1},T_i)$ over each accrual period. The medium-to-long end of the curve (4Y through 30Y here) is calibrated from these quotes. The leg present values, the model par rate, and the par-rate calibration condition are written out in the Calibration Equations section.

For a standard USD SOFR OIS according to the CFTC specification, the fixed-
and floating-leg business calendar is New York (`USNY`), while the overnight
fixing calendar is the U.S. Government Securities calendar (`USGS`). Payment
dates are two USNY business days after the corresponding accrual-period end
dates. Thus $U_i$ and $V_j$ are obtained by advancing their respective
period-end dates by two business days under the USNY payment calendar. Daily
SOFR observation dates and their calendar-day weights are determined under the
USGS fixing calendar. These calendar roles are deliberately separate.

The instrument specifications used are summarized below.

### Calibration Instrument Conventions

| Convention | Three-Month SOFR Futures | USD SOFR OIS |
| --- | --- | --- |
| Instrument | CME Three-Month SOFR future (`SR3`) | Spot-starting par SOFR OIS |
| Curve region | Front and intermediate part of the curve | Medium and long end of the curve |
| Instruments used | `SR3Z25` through `SR3Z28` | 4Y, 5Y, 6Y, 7Y, 10Y, 12Y, 15Y, 20Y and 30Y |
| Market quote | IMM price index $Q_m$ | Par fixed rate $S_m$ |
| Rate conversion | $R_{\mathrm{fut},m}=(100-Q_m)/100$ | $S_m$ is already quoted as a decimal annual rate |
| Underlying rate | Compounded daily SOFR over the contract reference quarter | Compounded daily SOFR over each OIS accrual period |
| Start date | Contract-specific IMM reference-quarter start | Spot effective date, normally two USNY business days after the trade date |
| End date | Contract-specific IMM reference-quarter end | Contractual OIS maturity date |
| Accrual tenor | Approximately three months | Annual coupon periods for the maturities used here |
| Day count | Actual/360 | Actual/360 for both legs |
| Business / payment calendar | Not applicable to the unadjusted IMM reference-quarter boundaries | New York (`USNY`) |
| Fixing calendar | U.S. Government Securities (`USGS`) | U.S. Government Securities (`USGS`) |
| Business-day convention | Determined by the CME reference-quarter specification | Modified Following |
| Payment frequency | Final settlement for each futures contract, with daily variation margin before settlement | Annual fixed- and floating-leg payments |
| Payment delay | Not represented as an OIS-style coupon payment delay | Two USNY business days after each accrual-period end |
| Fixed rate | Not applicable | Set to the par rate at inception |
| Upfront payment | Futures margining applies | None for a par OIS at inception |
| Convexity treatment | Ignored in Phase 2 | No separate futures convexity adjustment |
| Curve equation | Futures-implied rate constrains the forward zero-coupon bond price over the reference quarter | Par fixed-leg PV equals floating-leg PV |


## Calibration Equations and Bootstrap Algorithm

Ignoring the futures convexity adjustment, we identify this rate with the
generalized forward compounded-SOFR rate:

$$
R_{\mathrm{fut},m}
\approx
F^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})
=
\frac{1}{\tau_m}
\left(
\frac{P^{\mathrm{SOFR}}(0,T_{s,m})}
     {P^{\mathrm{SOFR}}(0,T_{e,m})}
-1
\right).
$$
It follows that

$$
P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})
=
\frac{1}
     {1+\tau_mR_{\mathrm{fut},m}}.
$$
where the forward zero-coupon bond price is defined as
$$
P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})
:=
\frac{P^{\mathrm{SOFR}}(0,T_{e,m})}
     {P^{\mathrm{SOFR}}(0,T_{s,m})}.
$$
For convenience, $P^{\mathrm{SOFR}}(t,T)$ denotes the extended zero-coupon bond price. For $T\geq t$, it is the ordinary zero-coupon bond price. For $T<t$, it is the value at time $t$ of reinvesting the bond's unit payoff from $T$ through $t$, so that $P^{\mathrm{SOFR}}(t,T)=B(t)/B(T)$. Such notation has been used for numeraires of hybrid $T$-forward measures and is discussed for the Forward Market Model. See Glasserman and Zhao (2000) or Section 4.2.4 in Andersen and Piterbarg (2010).

We set the current valuation time equal to $0$ with $P^{\mathrm{SOFR}}(0,0)=1$. Since the first selected SR3 contract has already entered its reference quarter, $T_{s,m}<0<T_{e,m}$. Under the extended zero-coupon bond definition,

$$
P^{\mathrm{SOFR}}(0,T_{s,m})
=
A^{\mathrm{SOFR}}(T_{s,m},0),
$$

where the realized SOFR accumulation factor is

$$
A^{\mathrm{SOFR}}(T_{s,m},0)
=
\prod_{\text{realized fixings }k}
\left(1+r_k\delta_k\right).
$$

Therefore, the first future determines the discount factor at its reference-
quarter end:

$$
P^{\mathrm{SOFR}}(0,T_{e,m})
=
P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})
P^{\mathrm{SOFR}}(0,T_{s,m})
=
P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})
A^{\mathrm{SOFR}}(T_{s,m},0).
$$

When repricing the partially accrued first contract, divide the calibrated
end-date discount factor by the realized accumulation to isolate the remaining
forward discount factor,
$P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})=
P^{\mathrm{SOFR}}(0,T_{e,m})/A^{\mathrm{SOFR}}(T_{s,m},0)$. The inverse
model-rate formula and its positivity conditions are stated in the Calibration
Diagnostics section.

Once this first future node has been obtained, consecutive SR3 contracts can
be used sequentially. For a subsequent contract whose start-date discount
factor has already been bootstrapped,

$$
P^{\mathrm{SOFR}}(0,T_{e,m})
=
P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})
P^{\mathrm{SOFR}}(0,T_{s,m}).
$$

Hence, we construct the front-to-belly region of the curve by using Three-Month SOFR futures as market-implied forward compounded-SOFR rates.

For longer maturities, SOFR OIS quotes are used to extend the discount curve
beyond the region calibrated with SOFR futures. Let
$[T_{i-1},T_i]$ denote floating-leg accrual period $i$, let $U_i$
denote its payment date, and let $V_j$ denote fixed-leg payment date $j$.
The model par rate is

$$
K_{\mathrm{model},m}
=
\frac{
\displaystyle\sum_i
P^{\mathrm{SOFR}}(0,U_i)\,
\tau_i F^{\mathrm{SOFR}}(0;T_{i-1},T_i)
}{
\displaystyle\sum_j
\alpha_jP^{\mathrm{SOFR}}(0,V_j)
},
$$

where $\tau_i$ and $\alpha_j$ are the floating- and fixed-leg accrual
fractions, respectively. At calibration, $K_{\mathrm{model},m}=S_m$.

The realized floating rate over $[T_{i-1},T_i]$ is the compounded SOFR rate

$$
R_{\mathrm{cmp}}^{\mathrm{SOFR}}(T_{i-1},T_i)
=
\frac{1}{\tau_i}
\left[
\prod_k(1+r_k\delta_k)-1
\right].
$$

This realized rate is not known at time $0$. Under the deterministic,
aligned single-curve approximation used in this phase, its curve-implied
forward rate is

$$
F^{\mathrm{SOFR}}(0;T_{i-1},T_i)
=
\frac{1}{\tau_i}
\left[
\frac{
P^{\mathrm{SOFR}}(0,T_{i-1})
}{
P^{\mathrm{SOFR}}(0,T_i)
}
-1
\right].
$$

Consequently, the projected floating-leg present value can be written as

$$
\mathrm{PV}_{\mathrm{float}}
=
\mathcal N\sum_i
P^{\mathrm{SOFR}}(0,U_i)
\left[
\frac{
P^{\mathrm{SOFR}}(0,T_{i-1})
}{
P^{\mathrm{SOFR}}(0,T_i)
}
-1
\right].
$$

The fixed-leg present value at the market par rate is

$$
\mathrm{PV}_{\mathrm{fixed}}
=
\mathcal N S_m
\sum_j
\alpha_jP^{\mathrm{SOFR}}(0,V_j).
$$

The par condition is

$$
\mathrm{PV}_{\mathrm{float}}
=
\mathrm{PV}_{\mathrm{fixed}}.
$$

Suppose the curve has already been calibrated through the previous futures
or OIS pillar. For the next OIS market quote $S_m$, introduce one new curve
parameter

$$
x_m
=
\log P^{\mathrm{SOFR}}(0,L_m),
$$

where $L_m$ is the chosen pillar date for the OIS. All previously calibrated
nodes are held fixed. For each trial value of $x_m$, the discount factors at
the OIS accrual and payment dates are obtained from the candidate curve using
the specified interpolation rule.

Let $K_{\mathrm{model},m}(x_m)$ denote the par rate calculated from this
candidate curve. The new node is determined by solving

$$
\epsilon_m(x_m)
:=
K_{\mathrm{model},m}(x_m)-S_m
=0.
$$

After solving this equation, the new node is fixed and the bootstrap proceeds
to the next OIS maturity. Thus, each OIS quote determines one new independent
curve parameter. Discount factors at intermediate dates that are not
calibration pillars are determined by interpolation and are not solved as
independent nodes.

Once the complete discount curve has been constructed, a three-month forward
compounded-SOFR rate over any future period $[T_a,T_b]$ is obtained from

$$
F_{3\mathrm{m}}^{\mathrm{SOFR}}(0;T_a,T_b)
=
\frac{1}{\tau_{a,b}}
\left[
\frac{
P^{\mathrm{SOFR}}(0,T_a)
}{
P^{\mathrm{SOFR}}(0,T_b)
}
-1
\right],
$$

where $T_b$ is approximately three months after $T_a$ and
$\tau_{a,b}$ is the corresponding Actual/360 year fraction. These
long-dated three-month forward rates are implied jointly by the OIS quotes
and the curve interpolation rule; they are not directly observed from an
individual OIS quote.

## Interpolation

piecewise log-linear is chosen. The curve object should store nodes

$$
\left(L_m,P^{\mathrm{SOFR}}(0,L_m)\right),
$$

where $L_m$ is a futures/OIS pillar date. Define the curve time

$$
t_m=\operatorname{yearFraction}(0,L_m)
$$

and the day-count convention Actual/365Fixed is chosen because it is simple, well behaved, and it gives approximately one year of fraction per calendar year.

The interpolation coordinate is

$$
x_m=\log P^{\mathrm{SOFR}}(0,L_m).
$$

For a date $T$ between adjacent pillars $L_m<T<L_{m+1}$, calculate

$$
w(T)
=
\frac{t(T)-t_m}{t_{m+1}-t_m}.
$$

Then interpolate linearly in log-discount-factor space:

$$
\log P^{\mathrm{SOFR}}(0,T)
=
\left[1-w(T)\right]x_m
+
w(T)x_{m+1}.
$$

Equivalently,

$$
P^{\mathrm{SOFR}}(0,T)
=
P^{\mathrm{SOFR}}(0,L_m)^{1-w(T)}
P^{\mathrm{SOFR}}(0,L_{m+1})^{w(T)}
$$

This has several useful properties:

- Discount factors remain strictly positive.
- The curve reproduces every node exactly.
- The instantaneous forward rate is constant between adjacent nodes:

$$
f^{\mathrm{SOFR}}(0,T)
=
-
\frac{x_{m+1}-x_m}
     {t_{m+1}-t_m},
\qquad L_m<T<L_{m+1}.
$$

- Discount factors do not need to be below one. Negative rates can legitimately produce $P(0,T)>1$, and the logarithm remains valid as long as $P(0,T)>0$.

During OIS calibration, the new terminal node is initially unknown. For each trial value

$$
x_m=\log P^{\mathrm{SOFR}}(0,L_m),
$$

you construct the candidate curve, interpolate all required OIS accrual and payment dates, calculate the model par rate, and solve

$$
K_{\mathrm{model},m}(x_m)-S_m=0.
$$

For this phase, we set each OIS pillar to its last relevant date, including its final payment delay. Then disable extrapolation beyond the final calibrated pillar and throw an informative error for $T>L_{\max}$.

## Repricing Diagnostics

Define every residual in decimal-rate units:
$$
\epsilon_m =
\begin{cases}
R_{\mathrm{model},m}(\hat{\mathbf x})-R_{\mathrm{fut},m},
& \text{SOFR future},\\[4pt]
K_{\mathrm{model},m}(\hat{\mathbf x})-S_m,
& \text{OIS}.
\end{cases}
$$
A correct bootstrap reprices its own calibration instruments to solver tolerance (exact by construction up to root-finding accuracy — each instrument determines exactly one new node), so the acceptance threshold is tight.
$$
\max_m|\epsilon_m|\le 10^{-10}
$$

For the first node,
$$
P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})=\frac{P(0,T_e)}{A_{\mathrm{realized}}} = \frac{1}{1+\tau R_{\mathrm{model}}}
$$
$$
R_{\mathrm{model}}
=
\frac{A_{\mathrm{realized}}/P(0,T_e)-1}{\tau}.
$$
we must have $\tau>0$, $A_{\mathrm{realized}}>0$, and $P(0,T_e)>0$

## DV01 / Bump-and-Reprice

### Setup and notation

- $M$ calibration instruments with market quotes $\mathbf q = (q_1,\dots,q_M)$ (the futures rates $R_{\mathrm{fut},m}$ and OIS par rates $S_m$).
- The curve has $M$ nodes; collect the node values in $\boldsymbol\theta = (\theta_1,\dots,\theta_M)$, with $\theta_m = \log P^{\mathrm{SOFR}}(0,L_m)$. After calibration $\theta_m$ equals the node coordinate $x_m$; the separate symbol $\boldsymbol\theta$ emphasizes that we now treat the full set of node values as one vector of model parameters to differentiate against.
- $g_m(\boldsymbol\theta)$ = the **model-implied quote** of instrument $m$ priced off curve $\boldsymbol\theta$ (for OIS, $g_m = K_{\mathrm{model},m}$; for a future, the curve-implied $R_{\mathrm{fut},m}$). Stack into $\mathbf g(\boldsymbol\theta)$.
- $V(\boldsymbol\theta)$ = present value of the target portfolio (or the swap/book whose DV01 is desired).

Calibration is the statement that the bootstrapped curve reprices every quote:
$$
\mathbf g(\boldsymbol\theta^\star) = \mathbf q,
$$
which implicitly defines the curve as a function of the quotes, $\boldsymbol\theta^\star(\mathbf q)$.

### The Two Sensitivity Spaces

The current setup distinguishes two sensitivities:

- **Curve-space sensitivity** — bump a node, reprice: $\;\mathbf s^{\mathrm{curve}} = \nabla_{\boldsymbol\theta} V$ (an $M$-vector, one entry per node).
- **Quote-space sensitivity** — bump a quote, *re-bootstrap*, reprice: $\;\mathbf s^{\mathrm{quote}} = \nabla_{\mathbf q} V$ (one entry per tradeable instrument).

For a parameter vector $\boldsymbol\psi\in\{\boldsymbol\theta,\mathbf q\}$,
the component sensitivity is estimated with a central finite difference of
half-width $h_\psi$:

$$
s_m
=
\frac{\partial V}{\partial\psi_m}
\approx
\frac{
V(\boldsymbol\psi+h_\psi\mathbf e_m)
-V(\boldsymbol\psi-h_\psi\mathbf e_m)
}{2h_\psi}.
$$

**Sign convention.** This project adopts an upward-oriented signed convention.
For market quote $q_m$ and $\Delta_{\mathrm{bp}}=10^{-4}$,

$$
\mathrm{DV01}^{\uparrow}_m
:=
\Delta_{\mathrm{bp}}\frac{\partial V}{\partial q_m}
\approx
\frac{\Delta_{\mathrm{bp}}}{2h_q}
\left[
V(\mathbf q+h_q\mathbf e_m)
-V(\mathbf q-h_q\mathbf e_m)
\right].
$$

The reference implementation sets $h_q=\Delta_{\mathrm{bp}}$, so

$$
\mathrm{DV01}^{\uparrow}_m
\approx
\frac{
V(\mathbf q+\Delta_{\mathrm{bp}}\mathbf e_m)
-V(\mathbf q-\Delta_{\mathrm{bp}}\mathbf e_m)
}{2}.
$$

Under this convention a payer swap has positive DV01 because it gains when
rates rise, and a receiver swap has negative DV01. The classic bond-market
"dollar value of an 01" is often reported as the opposite-signed positive
magnitude for a long-duration position; flip the sign when comparing with that
convention.

Both sensitivities can be estimated by bump-and-reprice; analytic
differentiation is separate future work.

There are three reasons quote-space sensitivity is preferred:

1. It expresses risk in buckets associated with tradeable instruments. A
   portfolio quote DV01 is not itself a hedge quantity: the hedge units for
   bucket $m$ are obtained by dividing the portfolio bucket DV01 by the signed
   DV01 of one unit of the corresponding hedge instrument, with the trade sign
   chosen to offset the portfolio exposure.
2. It's how desks attribute and report risk. Bucketed DV01 (or key rate DV01) by
   benchmark tenor is the language of a rates desk, and P&L-explain runs on *observable* quote moves, not model node moves.
3. Quote buckets are less parameterization-dependent than curve-node buckets
   because their labels are tied to observable calibration instruments. Their
   numerical values are not completely model-independent: they can still vary
   with the calibration instrument set, interpolation rule, and other curve
   construction choices. Curve-node sensitivities additionally depend directly
   on the internal node coordinates.

### Finite-Difference Jacobian Cross-Check


The exact Jacobian identity relates curve-node and market-quote sensitivities:
$$
\mathbf s^{\mathrm{quote}\,\top}
= \mathbf s^{\mathrm{curve}\,\top}\,\mathcal J^{-1}.
$$
Define the calibration Jacobian, model quotes w.r.t. curve nodes:
$$
\mathcal J = \frac{\partial \mathbf g}{\partial \boldsymbol\theta}, \qquad \mathcal J_{mn} = \frac{\partial g_m}{\partial \theta_n}.
$$

Phase 2 does not evaluate these derivatives analytically. For a node-coordinate
bump $h_\theta$, it estimates each entry by the central finite difference

$$
\mathcal J^{\mathrm{FD}}_{mn}
=
\frac{
g_m(\boldsymbol\theta+h_\theta\mathbf e_n)
-g_m(\boldsymbol\theta-h_\theta\mathbf e_n)
}{2h_\theta}.
$$

The formulas below use $\mathcal J$ for the exact mathematical identity; the
Phase 2 cross-check substitutes $\mathcal J^{\mathrm{FD}}$. Computing an
analytic Jacobian is explicitly reserved for future work.

Differentiate the calibration identity $\mathbf g(\boldsymbol\theta^\star(\mathbf q)) = \mathbf q$ with respect to $\mathbf q$. By the chain rule, it can be easily shown that:
$$
\frac{\partial \mathbf g}{\partial \boldsymbol\theta}\,\frac{\partial \boldsymbol\theta^\star}{\partial \mathbf q} = I
\quad\Longrightarrow\quad
{\;\frac{\partial \boldsymbol\theta^\star}{\partial \mathbf q} = \mathcal J^{-1}\;}
$$
That is: **the sensitivity of the curve nodes to the market quotes is $\mathcal J^{-1}$.**

Now chain PV through the curve:
$$
\mathbf s^{\mathrm{quote}\,\top}
= \frac{\partial V}{\partial \mathbf q}
= \underbrace{\frac{\partial V}{\partial \boldsymbol\theta}}_{\mathbf s^{\mathrm{curve}\,\top}}\;
\underbrace{\frac{\partial \boldsymbol\theta^\star}{\partial \mathbf q}}_{\mathcal J^{-1}}
= \mathbf s^{\mathrm{curve}\,\top}\,\mathcal J^{-1}.
$$

In column form:
$$
\boxed{\;\mathbf s^{\mathrm{quote}} = \mathcal J^{-\top}\,\mathbf s^{\mathrm{curve}}\;}
$$


Worth knowing, for cheap inversion and as a correctness check. In a sequential bootstrap, instrument $m$'s cashflows all fall on or before its pillar $L_m$, so its model quote $g_m$ depends only on nodes $\theta_1,\dots,\theta_m$ — never on later nodes:
$$
\frac{\partial g_m}{\partial \theta_n} = 0 \quad \text{for } n > m.
$$
So $\mathcal J$ is **lower-triangular**. Two consequences: (1) it's invertible iff every diagonal entry $\partial g_m/\partial\theta_m \neq 0$ — i.e., each instrument is genuinely sensitive to its own node, which is exactly the well-posedness condition for the bootstrap; (2) no matrix inverse is ever formed: the DV01 transformation solves $\mathcal J^\top\mathbf s^{\mathrm{quote}}=\mathbf s^{\mathrm{curve}}$ directly, and since $\mathcal J^\top$ is *upper*-triangular this is a single back-substitution pass.

The implementation can therefore compute $\mathbf s^{\mathrm{quote}}$ in two
independent ways and require agreement:

- **(a) Direct:** bump each quote $q_m$ by $\pm1$ bp, completely re-bootstrap
  both curves, and use the central difference defined above. This is the
  reference quote DV01.
- **(b) Via the finite-difference Jacobian:** estimate $\mathbf s^{\mathrm{curve}}$ and $\mathcal J^{\mathrm{FD}}$ with central node bumps, then solve $(\mathcal J^{\mathrm{FD}})^\top\mathbf s^{\mathrm{quote}}=\mathbf s^{\mathrm{curve}}$ by back-substitution.

### Quote DV01 Convention

For quote DV01, this project uses the following scenario convention:
1. Published fixings remain fixed. They are historical observations, not market variables.
2. The realized accumulation factor remains fixed, because it is calculated entirely from those fixings.
3. Bump the quoted full-quarter futures rate, not the remaining-period forward rate.
4. Re-bootstrap the entire curve for each scenario in the reference implementation.
5. Treat fixing corrections, valuation-date rolls, and publication updates as separate scenarios, not DV01.

And this is what we desired to implement in this phase. Recall that for the first period
$$
P^{\mathrm{SOFR}}(0,T_{e,m})
=
P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})
A^{\mathrm{SOFR}}(T_{s,m},0).
$$
With the published fixings and
$A^{\mathrm{SOFR}}(T_{s,m},0)$ held fixed, a bump to the observable
full-quarter rate can be interpreted as requiring calibration to move
$P^{\mathrm{SOFR}}(0;T_{s,m},T_{e,m})$, the remaining forward component that
still carries market risk. Operationally, the scenario bumps the full-quarter
quote and performs a complete re-bootstrap; it does not directly bump the
remaining-period forward rate.

Also note that $R\pm h \leftrightarrow Q\mp100h$ is used when bumping.

## QuantLib Benchmark

QuantLib is used as an independent answer key for the hand-rolled bootstrap. The
benchmark curve has the same state variable and interpolation rule:

```cpp
using QuantLibCurve = QuantLib::PiecewiseYieldCurve<
    QuantLib::Discount,
    QuantLib::LogLinear>;
```

Construct this curve with the same valuation date, `Actual365Fixed` curve day
counter, market quotes, instrument dates, and conventions as the hand-rolled
curve. Use the New York/USNY calendar for OIS scheduling and payment dates and
the `Sofr` index's USGS calendar for overnight observations and fixings. Do not
enable extrapolation.

For a standard CME Three-Month SOFR future, use `SofrFutureRateHelper` with
`Quarterly` frequency and a zero convexity adjustment. This is the
SOFR-specific subclass of `OvernightIndexFutureRateHelper`; the generic helper
is only needed when the reference-period dates or overnight index must be
specified directly. If a selected contract has already entered its reference
quarter, load the same realized SOFR fixings used by the hand-rolled bootstrap
into the QuantLib `Sofr` index before constructing the curve.

For each OIS quote, use `OISRateHelper` with a `Sofr` index. Leave its external
discounting-curve handle empty so that the bootstrapped curve is used for both
SOFR projection and discounting, consistently with the single-curve
assumption. Explicitly match the settlement convention, start and end dates,
fixed- and floating-leg payment frequencies, payment lag, calendars,
business-day conventions, compounded averaging, and
`Pillar::LastRelevantDate`. Relying on helper defaults without checking them
would no longer constitute a like-for-like benchmark.

The comparison is performed in the following order:

1. Verify that each curve reprices its own calibration instruments.
2. Compare the pillar dates and pillar discount factors.
3. Compare discount factors on an off-pillar date grid covering all futures
   reference dates and OIS accrual and payment dates.
4. Price the same test trades from both curves and compare their PVs and par
   rates.
5. Apply the same market-quote bump, rebuild each curve, and compare quote
   DV01.

QuantLib's futures helper returns an implied futures price, whereas the OIS
helper returns an implied par rate. To express both repricing residuals in
decimal-rate units, first convert the QuantLib futures price by

$$
R_{\mathrm{QL},m}
=
\frac{100-Q_{\mathrm{QL},m}}{100}.
$$

An upward one-basis-point bump to a futures-implied rate therefore corresponds
to a downward bump of $0.01$ in its IMM price:

$$
\Delta R_{\mathrm{fut},m}=10^{-4}
\quad\Longleftrightarrow\quad
\Delta Q_m=-0.01.
$$

After conversion to decimal-rate units, require the absolute calibration
residual of both implementations to satisfy

$$
\max_m|\epsilon_m|\leq 10^{-10}.
$$

This is a numerical acceptance tolerance, not machine precision. For the
cross-implementation discount-factor comparison, define

$$
\delta_P(T)
=
\frac{P^{\mathrm{SOFR}}_{\mathrm{ours}}(0,T)}
     {P^{\mathrm{SOFR}}_{\mathrm{QL}}(0,T)}
-1
$$

and require

$$
\max_T|\delta_P(T)|\leq 10^{-8}.
$$

DV01 is measured in currency units, so it must not be tested with the same
dimensionless tolerance. Normalize the difference by notional and use a
combined absolute-relative comparison, for example

$$
\left|
\frac{\mathrm{DV01}_{\mathrm{ours}}-\mathrm{DV01}_{\mathrm{QL}}}
     {\mathcal N}
\right|
\leq
\max\left(
10^{-10},
10^{-6}
\left|
\frac{\mathrm{DV01}_{\mathrm{QL}}}{\mathcal N}
\right|
\right).
$$

A tolerance breach should first trigger a comparison of quote units, fixing
history, reference date, curve day counter, helper pillar dates, schedules,
payment lag, calendars, convexity adjustment, interpolation, and bump
direction. The tolerance should not be relaxed to conceal a convention or model mismatch.

In conclusion
1. QuantLib’s evaluation date is set to 2026-01-15.
2. Load identical Sofr fixing records for business-day rate dates from the SR3Z25 quarter start through 2026-01-14.
3. Load fixings before constructing the curve helpers.
4. Construct SR3Z25 using SofrFutureRateHelper, Quarterly, zero convexity adjustment, and the agreed pillar rule.
5. Use the identical fixing history for the hand-rolled and QuantLib curves and keep it unchanged in every DV01 scenario.
6. Compare the SR3Z25 model-implied full-quarter decimal rate, first pillar date, first pillar DF, every downstream pillar DF, the off-pillar grid, and quote DV01.
7. Treat a missing required historical fixing as an error.


## Inputs / Outputs

Inputs form a deterministic hybrid test fixture. Exact file schemas and pinned
values live in the implementation note,
`docs/impl_notes/02_curve_bootstrap.md` §1a:

- fixture as-of timestamp: 2026-01-15 at 3:00 p.m. ET;
- historical SOFR records containing a business-day rate date and a decimal
  annualized rate, using final values published by the New York Fed from
  2025-12-17 through 2026-01-14 inclusive;
- no fixing record is supplied for a weekend or holiday; each fixing receives
  its Act/360 calendar-day weight through the next business-day rate date;
- missing, duplicate, out-of-range, or non-finite fixing records are errors and
  are never projected or silently filled;
- synthetic, internally consistent futures quotes: contract id, IMM
  reference-quarter start/end dates, and price $Q_m$;
- synthetic, internally consistent OIS quotes: tenor and par rate $S_m$;
- conventions: USNY OIS business/payment calendar, USGS SOFR fixing calendar,
  day counts (curve Act/365F, accrual Act/360), spot lag, payment delay,
  business-day rule, and interpolation choice.

Validation failures are explicit:

- instrument dates must be valid and strictly ordered, futures reference
  quarters must not overlap, and the selected futures strip must be contiguous;
- calibration pillar dates must be unique and strictly increasing;
- every market quote and fixing must be finite;
- each futures quote must satisfy
  $1+\tau_mR_{\mathrm{fut},m}>0$;
- every calibrated and interpolated discount factor must be finite and strictly
  positive;
- failure to bracket a calibration root or to converge within the configured
  iteration limit is an error that identifies the affected instrument.

Outputs:

- calibrated pillar set $(L_m,\ \log P^{\mathrm{SOFR}}(0,L_m))$ plus the
  fixed anchor;
- discount factors at any date within the calibrated domain (no
  extrapolation);
- forward compounded-SOFR rates over arbitrary in-domain periods;
- repricing residuals $\epsilon_m$;
- DV01 report per calibration instrument, in currency units, under the sign
  convention stated above.

## Assumptions
- The deterministic hybrid test fixture is as of 2026-01-15 at 3:00 p.m. ET,
  after the approximately 2:30 p.m. same-day revision window. It combines
  actual final SOFR fixings with synthetic SR3 and OIS quotes and is not a
  historical market snapshot.
- Latest known fixing rate date is 2026-01-14.
- Pinned fixings use final published values and Act/360 calendar-day weights.
- Missing historical fixings are errors and are never projected.
- Historical fixings and realized accumulation remain immutable during calibration and DV01.
- OIS spot, accrual, and payment dates use the USNY calendar; SOFR observation
  and fixing dates use the USGS calendar.
- SR3Z25 is partially accrued; all later futures are fully forward.
- Futures quotes represent full-quarter compounded rates.
- No futures convexity adjustment, deterministic single curve, clean SOFR collateralization, no basis or cross-currency effects.


## Tests Implied by This Note
1. **SR3Z25 boundary:** require the reference quarter to be
   $[2025\text{-}12\text{-}17,2026\text{-}03\text{-}18)$, containing 91
   calendar days. At the 2026-01-15 valuation date, the realized interval is
   $[2025\text{-}12\text{-}17,2026\text{-}01\text{-}15)$ and contains 29
   calendar days. The fixing input must contain business-day rate dates through
   2026-01-14, must not contain a 2026-01-15 rate date, and its calendar-day
   weights must sum to 29. The full-quarter accrual fraction must remain
   $\tau_m=91/360$.
2. **Realized accumulation:** for the synthetic decimal rates $r_1=0.0400$
   with $n_1=1$ calendar day and $r_2=0.0410$ with $n_2=3$ calendar days,
   require
   $$
   A_{\mathrm{realized}}
   =\left(1+\frac{0.0400}{360}\right)
    \left(1+\frac{3(0.0410)}{360}\right)
   =1.00045281574074
   $$
   within $10^{-12}$ in dimensionless accumulation-factor units.
3. **First pillar and inverse quote:** with the synthetic accumulation above,
   $\tau=91/360$, and $R_{\mathrm{fut}}=0.0430$, require
   $P(0,T_e)=A_{\mathrm{realized}}/(1+\tau R_{\mathrm{fut}})
   =0.989695376825414$ within $10^{-12}$. Recovering the model rate from that
   discount factor must return $0.0430$ within $10^{-12}$ in decimal-rate
   units.
4. **Fixing validation:** missing, duplicate, out-of-range, and non-finite
   fixing records must each fail explicitly. A Friday fixing followed by a
   Monday rate date must receive a three-calendar-day weight.
5. **Input and calibration failures:** invalid or non-increasing instrument
   dates, overlapping or non-contiguous futures quarters, duplicate pillars,
   non-finite quotes, $1+\tau_mR_{\mathrm{fut},m}\leq0$, non-positive discount
   factors, an unbracketed root, and failure to converge must each produce an
   explicit error. Every valid calibration instrument must satisfy
   $|\epsilon_m|\leq10^{-10}$ in decimal-rate units, and every in-domain
   discount factor must be finite and positive.
6. **QuantLib benchmark:** both curves must use the identical pinned fixing
   history. Compare first and downstream pillar discount factors and the
   off-pillar grid using $|\delta_P|\leq10^{-8}$, together with the DV01
   tolerance defined above.
7. **DV01 scenarios:** for each central $\pm1$ bp quote bump, keep the
   historical fixings and realized accumulation bit-for-bit unchanged, map a
   futures rate bump to the opposite IMM-price bump, and perform a complete
   re-bootstrap for both scenarios.
8. **Finite-difference Jacobian stretch:** when implemented, compare the direct
   quote DV01 with the finite-difference-Jacobian result using the DV01
   tolerance defined above.

## Open Questions / Future Extensions
- Add market-supplied futures convexity adjustments
- Add Hull-White-implied convexity adjustment after vol calibration
- Replace the finite-difference calibration Jacobian with an analytic Jacobian
- Extend to basis curves / xCcy curves


## Appendix: Summary of SOFR Rate Definitions 

### Daily SOFR

Let $r_k$ denote the SOFR fixing applicable on business day $k$.

SOFR is an annualized overnight rate based on eligible U.S. Treasury repo transactions. It is published by the Federal Reserve Bank of New York and uses an Actual/360 day-count convention.

A fixing may apply for several calendar days when the following day is a weekend or holiday.

### Compounded SOFR Rate

The compounded SOFR rate over an accrual period $[T_s,T_e]$ is

$$
R_{\mathrm{cmp}}^{\mathrm{SOFR}}(T_s,T_e)
=
\frac{1}{\tau_{s,e}}
\left[
\prod_{k=1}^{n}
\left(1+r_k\delta_k\right)-1
\right],
$$

where:

- $r_k$ is the daily SOFR fixing expressed as a decimal;
- $\delta_k=d_k/360$;
- $d_k$ is the number of calendar days for which $r_k$ applies;
- $\tau_{s,e}=\sum_k\delta_k$.

This is a backward-looking rate. It is unknown at the start of the accrual period and becomes known near or at the end of the period.

It is a geometrically compounded rate, not an arithmetic average.

### New York Fed SOFR Averages

The New York Fed publishes backward-looking 30-day, 90-day and 180-day SOFR Averages.

Each published average measures compounded SOFR over the historical observation window ending on the rate's reference date. It therefore describes rates that have already occurred.

A published SOFR Average should not be confused with CME Term SOFR.

### Forward Compounded SOFR Rate

For $t\leq T_s$, the forward compounded SOFR rate $F^{\mathrm{SOFR}}(t;T_s,T_e)$ is the fixed rate $K$ that makes the following forward contract have zero value at time $t$:

$$
\mathcal N\tau_{s,e}
\left[
R_{\mathrm{cmp}}^{\mathrm{SOFR}}(T_s,T_e)-K
\right].
$$

Under the simplified single-curve assumption, with payment at $T_e$,

$$
F^{\mathrm{SOFR}}(t;T_s,T_e)
=
\frac{1}{\tau_{s,e}}
\left[
\frac{P^{\mathrm{SOFR}}(t,T_s)}{P^{\mathrm{SOFR}}(t,T_e)}-1
\right]
=
\frac{1}{\tau_{s,e}}
\left[
\frac{1}{P^{\mathrm{SOFR}}(t;T_s,T_e)}-1
\right].
$$

The forward rate is known at time $t$, while the future realized compounded SOFR rate remains unknown.

### CME Term SOFR

CME Term SOFR is a published forward-looking benchmark for standardized tenors such as one, three, six and twelve months.

It is derived from prices observed in SOFR derivatives markets and represents the market-implied rate for a future SOFR accrual period.

CME Term SOFR is fixed near the beginning of the accrual period. It is therefore known before the corresponding realized compounded SOFR rate.

A Term SOFR fixing and a realized compounded SOFR rate may have similar value when the period begins, but they generally produce different realized payments because the compounded rate changes with subsequent daily SOFR fixings.

### SOFR Futures-Implied Rate

Let $Q_m$ denote the quoted price of Three-Month SOFR futures contract $m$. Its implied futures rate is

$$
R_{\mathrm{fut},m}
=
\frac{100-Q_m}{100}.
$$

The contract ultimately settles against annualized compounded SOFR over its specified IMM reference quarter.

Before settlement, $R_{\mathrm{fut},m}$ is the market futures rate for that future reference quarter. It is not yet the realized compounded SOFR rate.

Ignoring the futures convexity adjustment,

$$
R_{\mathrm{fut},m}
\approx
F^{\mathrm{SOFR}}(t;T_{s,m},T_{e,m}).
$$

In general, the futures rate and forward rate are not exactly equal because a futures contract is settled daily.

### OIS Par Rate

The OIS market quote $S_m$ is the fixed coupon rate that makes calibration instrument $m$ have zero value at inception:

$$
\mathrm{PV}_{\mathrm{fixed}}(S_m)
=
\mathrm{PV}_{\mathrm{float}}.
$$

The floating leg pays compounded overnight SOFR over each accrual period.

Under the simplified single-curve, period-end-payment assumptions,

$$
S_m
\sum_{i=1}^{n_m}
\tau_i P^{\mathrm{SOFR}}(0,T_i)
=
P^{\mathrm{SOFR}}(0,T_0)-P^{\mathrm{SOFR}}(0,T_{n_m}).
$$

For a single accrual period, the OIS par rate reduces to the corresponding forward compounded SOFR rate. For a multi-period OIS, it is a discount-weighted average of the forward rates across all accrual periods.
