#include "core/flat_curve.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace irc {

FlatCurve::FlatCurve(QuantLib::Date reference, double zero_rate, QuantLib::DayCounter day_counter)
    : reference_(reference), zero_rate_(zero_rate), day_counter_(std::move(day_counter)) {
    if (reference_ == QuantLib::Date()) {
        throw std::invalid_argument("FlatCurve: reference date is null");
    }
    if (day_counter_.empty()) {
        throw std::invalid_argument("FlatCurve: day counter is empty");
    }
}

double FlatCurve::discount(const QuantLib::Date& d) const {
    // P(t,T) = exp(-r * tau(t,T)).  Math note §1 (P_D), flat-curve convention
    // from impl note §1. tau(t,t) = 0 gives discount(reference) == 1.
    if (d < reference_) {
        throw std::invalid_argument("FlatCurve::discount: date is before the reference date");
    }
    const double tau = day_counter_.yearFraction(reference_, d);
    return std::exp(-zero_rate_ * tau);
}

QuantLib::Date FlatCurve::reference_date() const {
    return reference_;
}

}  // namespace irc
