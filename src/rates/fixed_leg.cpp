#include "rates/fixed_leg.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

FixedLeg::FixedLeg(QuantLib::Schedule schedule, QuantLib::DayCounter day_counter, double notional,
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
    // PV = K * annuity.  Math note §3.
    return fixed_rate_ * annuity(curve);
}

double FixedLeg::annuity(const YieldCurve& curve) const {
    // annuity = N * sum_i tau_i * P(t, T_i), payment date T_i = schedule_[i]
    // (payment lag 0 in v1). Math note §3 A(t), scaled by the notional.
    double sum = 0.0;
    for (QuantLib::Size i = 1; i < schedule_.size(); ++i) {
        const QuantLib::Date& period_start = schedule_[i - 1];
        const QuantLib::Date& payment = schedule_[i];
        const double tau = day_counter_.yearFraction(period_start, payment);
        sum += tau * curve.discount(payment);
    }
    return notional_ * sum;
}

}  // namespace irc
