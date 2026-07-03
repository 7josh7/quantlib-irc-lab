#pragma once
#include <ql/time/date.hpp>

namespace irc {

// Discount-curve abstraction. Depend on THIS, not on a concrete curve, so
// Phase 2's bootstrapped curve drops in with no change to the legs
// (open-closed — CPP Ch.2-3).
//
// Math: P_D(t0, T) in docs/math_notes/01_sofr_swap.md §1.
class YieldCurve {
public:
    virtual ~YieldCurve() = default;

    // Discount factor P(reference_date, d). P(ref, ref) == 1.0.
    // Throws std::invalid_argument if d < reference_date().
    virtual double discount(const QuantLib::Date& d) const = 0;
    virtual QuantLib::Date reference_date() const = 0;

protected:
    // rule of zero
    YieldCurve() = default;
    YieldCurve(const YieldCurve&) = default;
    YieldCurve& operator=(const YieldCurve&) = default;
    YieldCurve(YieldCurve&&) noexcept = default;
    YieldCurve& operator=(YieldCurve&&) noexcept = default;
};  // class YieldCurve

} // namespace irc