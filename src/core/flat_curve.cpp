#include "core/flat_curve.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

FlatCurve::FlatCurve(QuantLib::Date reference,
                     double zero_rate,
                     QuantLib::DayCounter day_counter)
    : reference_(reference),
      zero_rate_(zero_rate),
      day_counter_(std::move(day_counter)) {
    if (reference_ == QuantLib::Date()) {
        throw std::invalid_argument("FlatCurve: reference date is null");
    }
    if (day_counter_.empty()) {
        throw std::invalid_argument("FlatCurve: day counter is empty");
    }
}

double FlatCurve::discount(const QuantLib::Date& d) const {
    // TODO(step 4): validate d >= reference_, then
    //   P(t,T) = exp(-zero_rate_ * day_counter_.yearFraction(reference_, d)).
    // Math note §1 (P_D) with the flat-curve convention from impl note §1.
    (void)d;
    throw std::logic_error("FlatCurve::discount: not implemented (Phase 1 step 4)");
}

QuantLib::Date FlatCurve::reference_date() const {
    return reference_;
}

}  // namespace irc
