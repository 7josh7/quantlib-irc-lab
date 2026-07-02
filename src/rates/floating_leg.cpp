#include "rates/floating_leg.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

FloatingLeg::FloatingLeg(QuantLib::Schedule schedule,
                         QuantLib::DayCounter day_counter,
                         double notional,
                         std::shared_ptr<const RateAccrual> accrual,
                         double spread)
    : schedule_(std::move(schedule)),
      day_counter_(std::move(day_counter)),
      notional_(notional),
      accrual_(std::move(accrual)),
      spread_(spread) {
    if (schedule_.size() < 2) {
        throw std::invalid_argument("FloatingLeg: schedule needs at least 2 dates");
    }
    if (day_counter_.empty()) {
        throw std::invalid_argument("FloatingLeg: day counter is empty");
    }
    if (notional_ <= 0.0) {
        throw std::invalid_argument("FloatingLeg: notional must be positive");
    }
    if (!accrual_) {
        throw std::invalid_argument("FloatingLeg: accrual strategy is null");
    }
}

double FloatingLeg::present_value(const YieldCurve& curve) const {
    // TODO(step 4): N * sum_i tau_i * (F_i + spread_) * P(t, T_i), with
    // F_i = accrual_->forward_rate(curve, T_{i-1}, T_i, tau_i).
    // Math note §2 (projected coupon PV).
    (void)curve;
    throw std::logic_error(
        "FloatingLeg::present_value: not implemented (Phase 1 step 4)");
}

}  // namespace irc
