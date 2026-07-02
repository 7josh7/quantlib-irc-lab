#include "rates/floating_leg.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

FloatingLeg::FloatingLeg(QuantLib::Schedule schedule, QuantLib::DayCounter day_counter,
                         double notional, std::shared_ptr<const RateAccrual> accrual, double spread)
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
    // PV = N * sum_i tau_i * (F_i + s) * P(t, T_i), with F_i from the
    // injected accrual strategy. Math note §2 (projected coupon PV).
    double pv = 0.0;
    for (QuantLib::Size i = 1; i < schedule_.size(); ++i) {
        const QuantLib::Date& period_start = schedule_[i - 1];
        const QuantLib::Date& payment = schedule_[i];
        const double tau = day_counter_.yearFraction(period_start, payment);
        const double forward = accrual_->forward_rate(curve, period_start, payment, tau);
        pv += tau * (forward + spread_) * curve.discount(payment);
    }
    return notional_ * pv;
}

}  // namespace irc
