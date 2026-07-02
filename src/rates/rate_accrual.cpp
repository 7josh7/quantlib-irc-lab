#include "rates/rate_accrual.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

double SimpleForwardRate::forward_rate(const YieldCurve& curve,
                                       const QuantLib::Date& start,
                                       const QuantLib::Date& end,
                                       double year_fraction) const {
    // TODO(step 4): validate end > start and year_fraction > 0, then
    //   F = (curve.discount(start) / curve.discount(end) - 1) / year_fraction.
    // Math note §4 (single-curve simple forward).
    (void)curve;
    (void)start;
    (void)end;
    (void)year_fraction;
    throw std::logic_error(
        "SimpleForwardRate::forward_rate: not implemented (Phase 1 step 4)");
}

CompoundedOvernightRate::CompoundedOvernightRate(QuantLib::Calendar calendar)
    : calendar_(std::move(calendar)) {
    if (calendar_.empty()) {
        throw std::invalid_argument("CompoundedOvernightRate: calendar is empty");
    }
}

double CompoundedOvernightRate::forward_rate(const YieldCurve& curve,
                                             const QuantLib::Date& start,
                                             const QuantLib::Date& end,
                                             double year_fraction) const {
    // TODO(step 4): iterate business days d_0=start < d_1 < ... < d_n=end,
    // read the overnight forward for each [d_k, d_k+1] off the curve,
    // compound  prod_k (1 + delta_k * r_k),  return (prod - 1)/year_fraction.
    // Math note §2 (RFR block). Must genuinely iterate — the tests check
    // agreement with SimpleForwardRate via telescoping, and that check is
    // vacuous if this shortcuts to P(start)/P(end).
    (void)curve;
    (void)start;
    (void)end;
    (void)year_fraction;
    throw std::logic_error(
        "CompoundedOvernightRate::forward_rate: not implemented (Phase 1 step 4)");
}

}  // namespace irc
