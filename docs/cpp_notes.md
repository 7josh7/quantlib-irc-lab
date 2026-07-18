# C++ Notes

This note explains C++ language features used in this repo that are not
obvious from reading the code. It is a language guide, not an API guide (see
`docs/quantlib_cheatsheet.md`) and not a math note.

Current scope: the generic solver interface in
`src/core/bracketed_bisection.hpp`, approved in
`docs/impl_notes/02_curve_bootstrap.md` §3.

The main idea:

```text
lvalue / rvalue      = value categories: "has a name" vs "is a temporary"
T&  /  T&&           = lvalue reference / rvalue reference
reference collapsing = what happens when a reference lands on a reference
forwarding reference = the special T&& that binds to both categories
concept              = a compile-time yes/no question about a type
```

None of this is beginner material. Forwarding references and reference
collapsing occupy an entire chapter of Meyers' *Effective Modern C++*
(Items 23-30), and the standard did not adopt the term "forwarding
reference" until C++17. Concepts are C++20. Do not expect it to feel obvious.

Where to read more (see `docs/sources.md`):

```text
References, move, and forwarding — PRIMER (C++11, derives the mechanism)
  §13.6.1   Rvalue References
  §13.6.3   Rvalue References and Member Functions
  §16.2.5   Template Argument Deduction and References
            (contains the Reference Collapsing Rules)
  §16.2.6   Understanding std::move
  §16.2.7   Forwarding

Concepts — TOUR (C++20; PRIMER predates these entirely)
  §8.2      Concepts
  §8.3      Generic Programming
  §14.5     Concepts Overview (the standard library's own concepts)
```

The split is deliberate. PRIMER derives *why* `F&` collapses the way it does;
TOUR shows *how* a concept reads but not the deduction machinery underneath.
For this file's topic you need both.

## Value Categories

Example:

```cpp
auto residual = [](double x) { return x * x - 2.0; };

residual                        // lvalue: it has a name
[](double x) { return x; }      // rvalue: a temporary with no name
```

Rule of thumb:

```text
If you can refer to it again by name, it is an lvalue.
If it is a temporary that dies at the end of the statement, it is an rvalue.
```

One consequence matters a great deal below:

```cpp
void f(SomeType&& p) {
    // p has a NAME here.
    // So p is an LVALUE inside f, even though f was called with a temporary.
}
```

Named things are lvalues. Always. This is why the solver's residual parameter
is invoked as an lvalue on every iteration regardless of what the caller
passed.

## Reference Collapsing

You cannot write a reference to a reference by hand:

```cpp
int x = 5;
int& & r = x;   // does not compile; this syntax does not exist
```

But template substitution can produce that situation. When it does, C++
collapses the pair into one reference.

The rule:

```text
&    &     ->  &
&    &&    ->  &
&&   &     ->  &
&&   &&    ->  &&
```

Read it as:

```text
Any & wins. Only && landing on && stays &&.
```

Example with aliases, no templates involved:

```cpp
using LRef = int&;
using RRef = int&&;

LRef&   a;   // int&  &   ->  int&
LRef&&  b;   // int&  &&  ->  int&
RRef&   c;   // int&& &   ->  int&
RRef&&  d;   // int&& &&  ->  int&&
```

Only `d` is an rvalue reference. Three of four collapse to `int&`.

Used in this repo for:

- making `F&&` bind both named and temporary callables;
- making the `ScalarResidual` concept normalize to an lvalue check.

## Forwarding Reference

Example from this repo:

```cpp
template <ScalarResidual F>
BisectionResult bracketed_bisection(F&& residual, double lower, double upper,
                                    BisectionOptions options = {});
```

`F&& residual` is a **forwarding reference**, not an ordinary rvalue
reference.

A reference is a forwarding reference only when both of these hold:

```text
1. The form is exactly T&&, with nothing wrapped around T.
2. T is a template parameter deduced from the argument.
```

Counter-example:

```cpp
template <typename F>
void g(std::vector<F>&& v);   // NOT a forwarding reference
```

The form is `vector<F>&&`, not bare `F&&`, so this is a plain rvalue
reference and rejects lvalues.

## The Deduction Rule

Forwarding references get a special deduction rule:

```text
Pass an lvalue of type A  ->  F deduces to A&
Pass an rvalue of type A  ->  F deduces to A
```

This is an exception. Ordinary deduction strips references:

```cpp
template <typename T> void by_value(T  p);   // lvalue int -> T = int
template <typename T> void by_ref  (T& p);   // lvalue int -> T = int
template <typename T> void fwd     (T&& p);  // lvalue int -> T = int&   <- exception
```

So the asymmetry is real and it is deliberate. The next section explains why.

## Why the Deduction Is Asymmetric

The natural expectation is that both categories should deduce to plain `A`.
Try it and watch it fail.

Suppose an lvalue deduced to plain `A`:

```cpp
Residual r;                        // lvalue
bracketed_bisection(r, 0.0, 2.0);

// hypothetical: F deduces to Residual
// parameter F&& becomes         Residual&&
```

Now bind an lvalue to `Residual&&`:

```cpp
Residual&& param = r;   // ERROR: cannot bind rvalue reference to lvalue
```

Rvalue references refuse lvalues. That is their defining property. Under
symmetric deduction, forwarding references would not work for lvalues at all.

So read the rule backwards, as a constraint solved in reverse:

```text
Want parameter A&   (only lvalue refs bind lvalues)
  -> need F&& to collapse to A&
  -> need F to be A&          because  A& && -> A&

Want parameter A&&  (preserves movability)
  -> need F&& to be A&&
  -> need F to be A           because  A && -> A&&
```

The `&` in the lvalue case is the only input that makes collapsing produce
`A&`. The deduction rule and the collapsing rule were co-designed; neither
makes sense alone.

The reframe that makes it click:

```text
F is not "the type of the argument".
F is a carrier that encodes the type AND the value category.
```

| What you passed | F holds | The & means |
| --- | --- | --- |
| lvalue of type A | `A&` | "this was an lvalue" |
| rvalue of type A | `A` | "this was an rvalue" |

The presence or absence of `&` on `F` *is the record* of how the argument
arrived. That is also why `std::forward` is spelled with `<F>` — it reads
that encoding to restore the original category:

```cpp
std::forward<F>(residual)   // F == A&  -> yields A&   (lvalue)
                            // F == A   -> yields A&&  (rvalue)
```

If deduction were symmetric, that information would be gone and
`std::forward` would have nothing to consult.

## Why This Repo Wants a Forwarding Reference

There are only four ways to write the solver's callable parameter. Three of
them lose something:

```text
F residual          by value          copies a possibly fat stateful lambda
F& residual         lvalue ref only   rejects temporaries; must name every lambda
const F& residual   const lvalue ref  accepts both, but forces const
F&& residual        forwarding ref    accepts both, no copy, no forced const
```

The `const F&` option is the closest competitor and fails on `mutable`
callables:

```cpp
auto lam = [n = 0.0](double x) mutable { n += 1; return x - n; };
// operator() is non-const, so it cannot be called through const&
```

The OIS calibration lambda captures the instrument, the market quote, the
previously solved nodes, and candidate-curve construction, so the by-value
copy is not free either.

Honest caveat: the textbook motivation for `F&&` is *perfect forwarding* —
passing an argument onward with its category intact via `std::forward`. This
solver never forwards; it just invokes `residual` in a loop. The benefit
actually bought here is narrower: accept temporaries, do not copy, do not
force const. That is exactly what the impl note claims and nothing more.

## Concepts

A **concept** is a named compile-time predicate about *types*. Not about
values — about types.

```cpp
template <typename F>
concept ScalarResidual =
    std::invocable<F&, double> &&
    std::convertible_to<std::invoke_result_t<F&, double>, double>;
```

Read it as:

```text
F satisfies ScalarResidual when:
  (a) an F& can be called with one double, AND
  (b) the return type of that call converts to double.
```

`ScalarResidual<int>` is `false`. `ScalarResidual<some lambda type>` is
`true`. The compiler evaluates it while compiling; nothing runs.

The `&&` is ordinary logical AND, except both operands are compile-time
bools, so the result is a compile-time bool.

## std::invocable

```cpp
std::invocable< Callable, ArgTypes... >
```

Asks: can a `Callable` be called with arguments of these types? True when
`std::invoke(f, args...)` would compile.

So `std::invocable<F&, double>` asks: given an lvalue of type `F`, can I call
it with one `double`?

What counts as callable:

```cpp
double plain(double x);                    // free function       -> yes
auto lam = [](double x){ return x*x; };    // lambda              -> yes
struct Fn { double operator()(double); };  // functor             -> yes
```

An `int`, a `std::string`, or a lambda taking two doubles does not satisfy
it.

`std::invocable` is defined in terms of `std::invoke`, which also handles
exotic callables such as pointer-to-member-function. This repo does not need
those; that generality is simply why the standard concept is written this
way.

## Type Traits and the _t / _v Suffixes

`std::invoke_result_t` is **not** a function. It is a type alias template: it
takes types in and produces a type out, at compile time.

The giveaway is the brackets:

```text
( )  = runtime function call on values
< >  = compile-time template instantiation on types
```

The machinery behind the suffix:

```cpp
// provided by <type_traits>, roughly:
template <class F, class... Args>
struct invoke_result {
    using type = /* the deduced return type */;
};

template <class F, class... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;
```

So `_t` means "the `::type` member, pre-extracted so you do not have to write
`typename ...::type`". It is pure ergonomics.

The sibling convention:

| Suffix | Means | Produces | Example |
| --- | --- | --- | --- |
| `_t` | the `::type` member | a **type** | `std::remove_reference_t<T>` |
| `_v` | the `::value` member | a **value** | `std::is_integral_v<T>` |

The suffix is not a keyword. The compiler ignores those characters. It is a
naming habit the standard follows, and one you can reuse for your own
aliases:

```cpp
template <typename T>
using node_value_t = typename NodeTraits<T>::value_type;   // "_t" is style only
```

## std::convertible_to

```cpp
std::convertible_to< From, To >
```

Asks: can a value of type `From` be implicitly converted to `To`?

Plugging the trait in:

```cpp
std::convertible_to< std::invoke_result_t<F&, double>, double >
//                   \_____ the return type _____/    \_ target _/
```

`std::invocable` alone would accept a callable returning `std::string` or
`void`, because it only checks *callability*. This second clause enforces
"and it gives me back a number".

| F is | invocable? | return type | converts? | ScalarResidual |
| --- | --- | --- | --- | --- |
| `double(double)` | yes | `double` | yes | **yes** |
| `[](double){return 1;}` | yes | `int` | yes | **yes** |
| `[](double){return std::string{};}` | yes | `std::string` | no | **no** |
| `[](double){}` | yes | `void` | no | **no** |
| `[](std::string){return 0.0;}` | no | — | — | **no** |
| `int` | no | — | — | **no** |

## Why the Concept Checks F& and Not F

This is the pinned invariant in the impl note. Do not "simplify" `F&` to `F`.

Three-step procedure for reading any of these:

```text
Step 1. What did F deduce to?   (lvalue -> A&, rvalue -> A)
Step 2. Paste that into the expression.
Step 3. Collapse. Any & wins.
```

Run it on both call sites, for the parameter and for the concept:

```cpp
struct Residual { double operator()(double x) const { return x*x - 2.0; } };

Residual r;
bracketed_bisection(r, 0.0, 2.0);            // lvalue
bracketed_bisection(Residual{}, 0.0, 2.0);   // rvalue
```

Parameter `F&&`:

```text
lvalue:  F = Residual&  ->  Residual& &&  ->  Residual&
rvalue:  F = Residual   ->  Residual&&    ->  Residual&&
```

Concept `F&`:

```text
lvalue:  F = Residual&  ->  Residual& &   ->  Residual&
rvalue:  F = Residual   ->  Residual&     ->  Residual&
```

The parameter type differs — that is the universality. The concept check is
identical both times — that is the normalization.

Compare what bare `F` would have checked:

```text
lvalue:  invocable<Residual&,  double>    lvalue call
rvalue:  invocable<Residual,   double>    rvalue call   <- drifted
```

Bare `F` asks a different question depending on the call site, while the body
always performs an lvalue call. That mismatch is a real defect, not a style
preference.

## The Failure Case That Motivates It

A callable can be rvalue-only via an `&&`-qualified `operator()`:

```cpp
struct RvalueOnly {
    double operator()(double x) && { return x - 1.0; }   // note trailing &&
};
```

```text
std::invocable<RvalueOnly,  double>  ->  true    (callable as an rvalue)
std::invocable<RvalueOnly&, double>  ->  false   (not callable as an lvalue)
```

Now call with a temporary, so `F` deduces to `RvalueOnly`:

| Concept spelling | Check performed | Result |
| --- | --- | --- |
| bare `F` | `invocable<RvalueOnly, double>` | passes, then fails deep inside the loop |
| `F&` (ours) | `invocable<RvalueOnly&, double>` | fails cleanly at the call site |

With `F&` you get "constraint not satisfied" pointing at your own call.
With bare `F` you get a wall of template errors from inside the solver.
Same bug, caught much earlier.

## Gotcha: const Plus mutable

```cpp
const auto r = [](double x){ return x*x - 2.0; };
bracketed_bisection(r, 0.0, 2.0);
```

```text
F deduces to  const Lam&
F& collapses to  const Lam&
check:  std::invocable<const Lam&, double>
```

That requires the closure's `operator()` to be const. A normal lambda's
`operator()` *is* const, so this passes. But:

```cpp
const auto r = [n = 0.0](double x) mutable { return x + n; };
bracketed_bisection(r, 0.0, 2.0);   // constraint not satisfied
```

A `mutable` lambda has a **non-const** `operator()`, so `const Lam&` cannot
call it.

Fix: drop the `const` on `r`, so `F` deduces to `Lam&`; or drop `mutable`.

Relevant when writing the OIS calibration lambda — if it is made `mutable` to
cache state, it must not also be declared `const`.

## Summary

```text
Any & wins when collapsing; only && && stays &&.
Forwarding ref deduces lvalue -> A&, rvalue -> A, so F carries value category.
F&& therefore mirrors the argument: accepts both, copies neither.
F& therefore normalizes to A& always: one consistent lvalue check.
Named parameters are lvalues, so the lvalue check is the honest one.
```

Used in this repo for:

- `bracketed_bisection`'s callable parameter and its `ScalarResidual`
  constraint (`src/core/bracketed_bisection.hpp`);
- the OIS calibration lambda that captures instrument state and exposes it
  through the scalar residual contract.
