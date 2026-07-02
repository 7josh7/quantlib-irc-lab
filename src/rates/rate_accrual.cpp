#include "rates/rate_accrual.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

double SimpleForwardRate::forward_rate(const YieldCurve& curve, const QuantLib::Date& start,
                                       const QuantLib::Date& end, double year_fraction) const {
    // F = (P(start)/P(end) - 1) / tau.  Math note §4 (single-curve forward).
    if (end <= start) {
        throw std::invalid_argument("SimpleForwardRate: end must be after start");
    }
    if (year_fraction <= 0.0) {
        throw std::invalid_argument("SimpleForwardRate: year_fraction must be positive");
    }
    return (curve.discount(start) / curve.discount(end) - 1.0) / year_fraction;
}

CompoundedOvernightRate::CompoundedOvernightRate(QuantLib::Calendar calendar)
    : calendar_(std::move(calendar)) {
    if (calendar_.empty()) {
        throw std::invalid_argument("CompoundedOvernightRate: calendar is empty");
    }
}

double CompoundedOvernightRate::forward_rate(const YieldCurve& curve, const QuantLib::Date& start,
                                             const QuantLib::Date& end,
                                             double year_fraction) const {
    // Math note §2 (RFR block):  R = [ prod_k (1 + delta_k r_k) - 1 ] / tau.
    // On a single curve the overnight factor (1 + delta_k r_k) equals
    // P(d_k)/P(d_{k+1}), so the product telescopes to P(start)/P(end). We
    // iterate business day by business day rather than shortcut, so the
    // "strategies agree" test exercises the real compounding path.
    if (end <= start) {
        throw std::invalid_argument("CompoundedOvernightRate: end must be after start");
    }
    if (year_fraction <= 0.0) {
        throw std::invalid_argument("CompoundedOvernightRate: year_fraction must be positive");
    }

    double compound = 1.0;
    QuantLib::Date current = start;
    while (current < end) {
        QuantLib::Date next = calendar_.advance(current, 1, QuantLib::Days);
        if (next > end) {
            next = end;  // final stub: clamp to the accrual end
        }
        compound *= curve.discount(current) / curve.discount(next);
        current = next;
    }
    return (compound - 1.0) / year_fraction;
}

}  // namespace irc
