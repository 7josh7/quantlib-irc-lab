#include "rates/fixed_leg.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

FixedLeg::FixedLeg(QuantLib::Schedule schedule,
                   QuantLib::DayCounter day_counter,
                   double notional,
                   double fixed_rate)
    : schedule_(std::move(schedule)),
      day_counter_(std::move(day_counter)),
      notional_(notional),
      fixed_rate_(fixed_rate) {
    if (schedule_.size() < 2) {
        throw std::invalid_argument("FixedLeg: schedule needs at least 2 dates");
    }
    if (day_counter_.empty()) {
        throw std::invalid_argument("FixedLeg: day counter is empty");
    }
    if (notional_ <= 0.0) {
        throw std::invalid_argument("FixedLeg: notional must be positive");
    }
}

double FixedLeg::present_value(const YieldCurve& curve) const {
    // TODO(step 4): fixed_rate_ * annuity(curve).  Math note §3.
    (void)curve;
    throw std::logic_error("FixedLeg::present_value: not implemented (Phase 1 step 4)");
}

double FixedLeg::annuity(const YieldCurve& curve) const {
    // TODO(step 4): N * sum_i tau_i * P(t, T_i) over consecutive schedule
    // date pairs, tau_i from day_counter_.  Math note §3 (A(t), times N).
    (void)curve;
    throw std::logic_error("FixedLeg::annuity: not implemented (Phase 1 step 4)");
}

}  // namespace irc
