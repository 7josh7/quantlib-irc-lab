# 02 SOFR Curve Bootstrapping + DV01

## Sources
- Source II 3.3
- Source II 3.3.1
- Source II 3.4
- QuantLib docs/examples

## Notation

| Symbol | Definition |
| --- | --- |
| $t$ | Valuation date / pricing time. Usually $t=0$ in this note. |
| $T_i$ | Curve node date or cash-flow date. |
| $\tau_i$ | Year fraction for accrual period $[T_{i-1}, T_i]$. |
| $P(0,T)$ | Discount factor from valuation date $0$ to maturity $T$. |
| $P(0,T_i)$ | Discount factor at curve node $T_i$. |
| $z(0,T)$ | Continuously compounded zero rate for maturity $T$. |
| $f(0,T)$ | Instantaneous forward rate at maturity $T$. |
| $F(0; T_{i-1}, T_i)$ | Simple forward rate over $[T_{i-1}, T_i]$. |
| $R_i$ | Market-implied futures/forward rate for period $i$. |
| $Q_i$ | Quoted futures price for contract $i$. |
| $S_N$ | Par OIS swap rate for maturity $T_N$. |
| $A_N$ | Fixed-leg annuity for swap maturity $T_N$. |
| $N$ | Swap notional. Avoid using this as both notional and maturity index in the same formula. |
| $K$ | Fixed coupon rate on a swap. |
| $\text{PV}_{\text{fixed}}$ | Present value of the fixed leg. |
| $\text{PV}_{\text{float}}$ | Present value of the floating leg. |
| $\theta$ | Vector of curve parameters / bootstrapped node values. |
| $X^M$ | Vector of market quotes, such as futures rates and OIS par rates. |
| $X^I$ | Vector of internal curve coordinates, such as zero rates, log discount factors, or IFR nodes. |
| $\mathcal{J}$ | Jacobian matrix mapping sensitivities between market quotes and internal curve parameters. |
| $\Delta_{\text{bp}}$ | One basis point bump, usually $10^{-4}$. |
| $\text{DV01}$ | Change in PV for a one-basis-point rate bump, with sign convention stated separately. |
| $\epsilon_i$ | Repricing error for calibration instrument $i$. |

## Market Instruments
### SOFR Futures
SOFR OIS 3M futures is used for front-to-belly curve construction. We assume no convexity adjustment here and leave it to future implementation where stochastic model comes in.

The contract unit is $2,500 x contract-frade IMM index, where contract-grade IMM Index = 100 - Q. Q is the business-day compounded SOFR per annum during contract Reference Quarter. (1M SOFR Future use simple average instead) Since we assume no convexity adjustment, R here is forward rate per annum during contract Reference Quarter. More precisely,
$$
R_{\mathrm{fut}}(0;T_s,T_e) = \frac{1 }{\tau} *( \frac{P^{SOFR}(0,T_s)}{P^{SOFR}(0,T_e)}  - 1)
$$
Note that SOFR is published on every U.S. business day at approximately
8:00am EST. But the Fed has the ability to correct and republish this rate until 2:30pm EST. Also note that the published rate represent the repo transactions enter into on the previous businese day. 



### OIS Swaps
- What the par OIS quote means
- Fixed leg vs floating leg interpretation
- Which maturities are used

OIS is quoted at par rate. That is, the swap rate K that make $\text{PV}_{\text{fixed}} = \text{PV}_{\text{float}}$ where
$$
\text{PV}_{\text{fixed}} = NK \sum_i \tau_i P(t,T^{pay}_i) \\
PV_{float} = N \sum_i P(t,T^{pay}_i) \tau_i \mathbb E_{t}[R(T_{i-1}, T_i)]
$$
$R$ is the business-day compounded SOFR per annum during contract accrual Period. Therefore the par rate is
$$
K_{par} = \frac{\sum_i P(t,T^{pay}_i) \tau_i \mathbb E_{t}[R(T_{i-1}, T_i)]}{K \sum_i \tau_i P(t,T^{pay}_i)}
$$

For a standard USD SOFR OIS, the payment date is two business days after the accrual-period end date. That is, $T^{pay}_i = T_i + 2$

Instrument specifictaion used is summarize below

### Calibration Instrument Conventions

| Convention | Three-Month SOFR Futures | USD SOFR OIS |
| --- | --- | --- |
| Instrument | CME Three-Month SOFR future (`SR3`) | Spot-starting par SOFR OIS |
| Curve region | Front and intermediate part of the curve | Medium and long end of the curve |
| Instruments used | `SR3H26` through `SR3Z28` | 4Y, 5Y, 7Y, 10Y, 12Y, 15Y, 20Y, 25Y and 30Y |
| Market quote | IMM price index $Q_i$ | Par fixed rate $S_N$ |
| Rate conversion | $R_i=(100-Q_i)/100$ | $S_N$ is already quoted as a decimal annual rate |
| Underlying rate | Compounded daily SOFR over the contract reference quarter | Compounded daily SOFR over each OIS accrual period |
| Start date | Contract-specific IMM reference-quarter start | Spot effective date, normally two business days after the trade date |
| End date | Contract-specific IMM reference-quarter end | Contractual OIS maturity date |
| Accrual tenor | Approximately three months | Annual coupon periods for the maturities used here |
| Day count | Actual/360 | Actual/360 for both legs |
| Fixing calendar | U.S. Government Securities calendar | U.S. Government Securities calendar |
| Business-day convention | Determined by the CME reference-quarter specification | Modified Following |
| Payment frequency | Final settlement for each futures contract, with daily variation margin before settlement | Annual fixed- and floating-leg payments |
| Payment delay | Not represented as an OIS-style coupon payment delay | Two business days after each accrual-period end |
| Fixed rate | Not applicable | Set to the par rate at inception |
| Upfront payment | Futures margining applies | None for a par OIS at inception |
| Convexity treatment | Ignored in Phase 2 | No separate futures convexity adjustment |
| Curve equation | Futures-implied rate constrains the discount-factor ratio over the reference quarter | Par fixed-leg PV equals floating-leg PV |


## Curve Representation (delet?)
- What the curve stores
- Discount factors vs zero rates vs instantaneous forwards
- Node dates / node times
- Interpolation choice: piecewise log-linear, per roadmap
- Extrapolation policy, if any

## Discount, Zero, and Forward Relationships (delet?)
- Relationship between discount factors and zero rates
- Relationship between discount factors and simple forward rates
- Relationship between curve representation and instrument pricing

## Calibration Equations and Bootstrap Algorithm
- Write the equation that links a futures-implied rate to curve discount factors
- Write the par swap equation
- Fixed-leg PV
- Floating-leg PV
- Par-rate condition
- Inputs
- Initial node / anchor
- Sequential solving process
- How each quote determines the next curve node
- Solver choice and tolerances
- Failure cases

Ignoring the futures convexity adjustment, we identify this rate with the
generalized forward compounded-SOFR rate:

$$
R_{\mathrm{fut}}(0;T_s,T_e)
\approx
R^{\mathrm{SOFR}}(0;T_s,T_e)
=
\frac{1}{\tau_{s,e}}
\left(
\frac{P^{\mathrm{SOFR}}(0,T_s)}
     {P^{\mathrm{SOFR}}(0,T_e)}
-1
\right).
$$
It follows that

$$
P^{\mathrm{SOFR}}(0;T_s,T_e)
=
\frac{1}
     {1+\tau_{s,e}R_{\mathrm{fut}}(0;T_s,T_e)}.
$$
where forward discount factor $P^{\mathrm{SOFR}}(0;T_s,T_e)$ is defined as 
$$
P^{\mathrm{SOFR}}(0;T_s,T_e)
:=
\frac{P^{\mathrm{SOFR}}(0,T_e)}
     {P^{\mathrm{SOFR}}(0,T_s)}.
$$
For convinence, $P^{\mathrm{}}(t,T)$ denotes the extended  zero-coupon bond price $\frac{B(t)}{B(T)}$ for bank account $B(t)$. Such notation has been used for numeriare of hybrid T-forward measures and heaviley discussed for the Forward Market Model. See Glasserman and Zhao (2000) or Section 4.2.4 in Andersen and Piterbarg (2010). 

We set the current valuation time equal to $0$ with $P(0,0) = 1$. If the first selected SR3 contract has already entered its reference quarter, then $T_s<0<T_e$. Under the extended zero-coupon bond definition,

$$
P^{\mathrm{SOFR}}(0,T_s)
=
A^{\mathrm{SOFR}}(T_s,0),
$$

where the realized SOFR accumulation factor is

$$
A^{\mathrm{SOFR}}(T_s,0)
=
\prod_{\text{realized fixings }i}
\left(1+r_i\delta_i\right).
$$

Therefore, the first future determines the discount factor at its reference-
quarter end:

$$
P^{\mathrm{SOFR}}(0,T_e)
=
P^{\mathrm{SOFR}}(0;T_s,T_e)
P^{\mathrm{SOFR}}(0,T_s)
=
P^{\mathrm{SOFR}}(0;T_s,T_e)A^{\mathrm{SOFR}}(T_s,0).
$$

Once this first future node has been obtained, consecutive SR3 contracts can
be used sequentially. For a subsequent contract whose start-date discount
factor has already been bootstrapped,

$$
P^{\mathrm{SOFR}}(0,T_e)
=
P^{\mathrm{SOFR}}(0;T_s,T_e)P^{\mathrm{SOFR}}(0,T_s).
$$

Hecse we construct front-end to belly of curve by using 3 months SOFR future as market implied forward rate. 

For longer maturities, SOFR OIS quotes are used to extend the discount curve
beyond the region calibrated with SOFR futures. Let
$[T_{i-1},T_i]$ denote floating-leg accrual period $i$, let $U_i$
denote its payment date, and let $V_j$ denote fixed-leg payment date $j$.
The market par rate is

$$
K_{\mathrm{par}}
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
fractions, respectively.

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
N\sum_i
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
N K_{\mathrm{par}}
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
or OIS pillar. For the next OIS market quote $S_M$, introduce one new curve
parameter

$$
x_M
=
\log P^{\mathrm{SOFR}}(0,L_M),
$$

where $L_M$ is the chosen pillar date for the OIS. All previously calibrated
nodes are held fixed. For each trial value of $x_M$, the discount factors at
the OIS accrual and payment dates are obtained from the candidate curve using
the specified interpolation rule.

Let $K_{\mathrm{model}}(x_M)$ denote the par rate calculated from this
candidate curve. The new node is determined by solving

$$
K_{\mathrm{model}}(x_M)-S_M=0.
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
## Repricing Diagnostics
- Reprice each calibration instrument
- Define repricing error
- State expected tolerance

## DV01 / Bump-and-Reprice
- What gets bumped
- Parallel 1 bp bump vs quote-level bump
- PV change convention
- Expected sign intuition
- Difference between curve DV01 and market quote DV01, if relevant


## Interpolation


## QuantLib Benchmark
- What QuantLib object/helper is the answer key
- What outputs you compare
- Expected tolerances

## Inputs / Outputs for Implementation
### Inputs
- Market quotes
- Valuation date
- Conventions
- Curve nodes
- Instrument definitions

### Outputs
- Bootstrapped curve
- Discount factors
- Zero rates
- Forward rates
- Repricing errors
- DV01 values

## Assumptions
- No futures convexity adjustment
- Deterministic curve
- No multi-curve basis
- No xCcy collateral effects
- Clean SOFR collateralization

## Known Limitations
- Ignoring futures convexity
- Simplified interpolation
- Simplified market conventions
- No calibrated volatility model
- No analytic Jacobian unless added as stretch

## Tests Implied by This Note
- Calibration instruments reprice
- Discount factors are positive
- Own curve close to QuantLib curve
- DV01 sign and rough magnitude are sensible
- Bad inputs fail explicitly

## Open Questions / Future Extensions
- Add market-supplied futures convexity adjustments
- Add Hull-White-implied convexity adjustment after vol calibration
- Add Jacobian market-risk representation
- Extend to basis curves / xCcy curves


## Appendix: Summary of SOFR Rate Definitions 

### Daily SOFR

Let $r_i$ denote the SOFR fixing applicable on business day $i$.

SOFR is an annualized overnight rate based on eligible U.S. Treasury repo transactions. It is published by the Federal Reserve Bank of New York and uses an Actual/360 day-count convention.

A fixing may apply for several calendar days when the following day is a weekend or holiday.

### Compounded SOFR Rate

The compounded SOFR rate over an accrual period $[T_s,T_e]$ is

$$
R_{\mathrm{cmp}}(T_s,T_e)
=
\frac{1}{\tau_{s,e}}
\left[
\prod_{i=1}^{n}
\left(1+r_i\delta_i\right)-1
\right],
$$

where:

- $r_i$ is the daily SOFR fixing expressed as a decimal;
- $\delta_i=d_i/360$;
- $d_i$ is the number of calendar days for which $r_i$ applies;
- $\tau_{s,e}=\sum_i\delta_i$.

This is a backward-looking rate. It is unknown at the start of the accrual period and becomes known near or at the end of the period.

It is a geometrically compounded rate, not an arithmetic average.

### New York Fed SOFR Averages

The New York Fed publishes backward-looking 30-day, 90-day and 180-day SOFR Averages.

Each published average measures compounded SOFR over the historical observation window ending on the rate's reference date. It therefore describes rates that have already occurred.

A published SOFR Average should not be confused with CME Term SOFR.

### Forward Compounded SOFR Rate

For $t\leq T_s$, the forward compounded SOFR rate $F(t;T_s,T_e)$ is the fixed rate $K$ that makes the following forward contract have zero value at time $t$:

$$
N\tau_{s,e}
\left[
R_{\mathrm{cmp}}(T_s,T_e)-K
\right].
$$

Under the simplified single-curve assumption, with payment at $T_e$,

$$
F(t;T_s,T_e)
=
\frac{1}{\tau_{s,e}}
\left[
\frac{P(t,T_s)}{P(t,T_e)}-1
\right].
$$

The forward rate is known at time $t$, while the future realized compounded SOFR rate remains unknown.

### CME Term SOFR

CME Term SOFR is a published forward-looking benchmark for standardized tenors such as one, three, six and twelve months.

It is derived from prices observed in SOFR derivatives markets and represents the market-implied rate for a future SOFR accrual period.

CME Term SOFR is fixed near the beginning of the accrual period. It is therefore known before the corresponding realized compounded SOFR rate.

A Term SOFR fixing and a realized compounded SOFR rate may have similar value when the period begins, but they generally produce different realized payments because the compounded rate changes with subsequent daily SOFR fixings.

### SOFR Futures-Implied Rate

Let $Q$ denote the quoted price of a Three-Month SOFR futures contract. Its implied futures rate is

$$
R_{\mathrm{fut}}
=
\frac{100-Q}{100}.
$$

The contract ultimately settles against annualized compounded SOFR over its specified IMM reference quarter.

Before settlement, $R_{\mathrm{fut}}$ is the market futures rate for that future reference quarter. It is not yet the realized compounded SOFR rate.

Ignoring the futures convexity adjustment,

$$
R_{\mathrm{fut}}
\approx
F(t;T_s,T_e).
$$

In general, the futures rate and forward rate are not exactly equal because a futures contract is settled daily.

### OIS Par Rate

The OIS par rate $S_N$ is the fixed coupon rate that makes a spot-starting overnight indexed swap have zero value at inception:

$$
\mathrm{PV}_{\mathrm{fixed}}(S_N)
=
\mathrm{PV}_{\mathrm{float}}.
$$

The floating leg pays compounded overnight SOFR over each accrual period.

Under the simplified single-curve, period-end-payment assumptions,

$$
S_N
\sum_{j=1}^{N}
\tau_j P(0,T_j)
=
P(0,T_0)-P(0,T_N).
$$

For a single accrual period, the OIS par rate reduces to the corresponding forward compounded SOFR rate. For a multi-period OIS, it is a discount-weighted average of the forward rates across all accrual periods.