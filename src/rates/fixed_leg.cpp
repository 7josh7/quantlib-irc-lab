#include "rates/fixed_leg.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace irc {

FixedLeg::FixedLeg(QuantLib::Schedule schedule, QuantLib::DayCounter day_counter, double notional,
                   double fixed_rate)
    : schedule_(std::move(schedule)),
      day_counter_(std::move(day_counter)),
      periods_(),
      notional_(notional),
      fixed_rate_(fixed_rate),
      uses_periods_(false) {
    if (schedule_.size() < 2) {
        throw std::invalid_argument("FixedLeg: schedule needs at least 2 dates");
    }
    if (day_counter_.empty()) {
        throw std::invalid_argument("FixedLeg: day counter is empty");
    }
    if (notional_ <= 0.0 || !std::isfinite(notional_)) {
        throw std::invalid_argument("FixedLeg: notional must be positive and finite");
    }
    if (!std::isfinite(fixed_rate_)) {
        throw std::invalid_argument("FixedLeg: rate must be finite");
    }
}

FixedLeg::FixedLeg(std::vector<CouponPeriod> periods, double notional, double fixed_rate)
    : schedule_(),
      day_counter_(),
      periods_(std::move(periods)),
      notional_(notional),
      fixed_rate_(fixed_rate),
      uses_periods_(true) {
    if (periods_.empty()) {
        throw std::invalid_argument("FixedLeg: periods must not be empty");
    }
    for (std::size_t i = 0; i < periods_.size(); ++i) {
        const CouponPeriod& period = periods_[i];
        if (period.accrual_start == QuantLib::Date() || period.accrual_end == QuantLib::Date() ||
            period.payment_date == QuantLib::Date()) {
            throw std::invalid_argument("FixedLeg: coupon period contains a null date");
        }
        if (period.accrual_start >= period.accrual_end) {
            throw std::invalid_argument("FixedLeg: accrual start must precede accrual end");
        }
        if (period.payment_date < period.accrual_end) {
            throw std::invalid_argument("FixedLeg: payment date precedes accrual end");
        }
        if (!std::isfinite(period.year_fraction) || period.year_fraction <= 0.0) {
            throw std::invalid_argument(
                "FixedLeg: coupon year fraction must be finite and positive");
        }
        if (i > 0 && period.accrual_start < periods_[i - 1].accrual_end) {
            throw std::invalid_argument("FixedLeg: coupon periods overlap or are unordered");
        }
    }
    if (!std::isfinite(notional_) || notional_ <= 0.0) {
        throw std::invalid_argument("FixedLeg: notional must be finite and positive");
    }
    if (!std::isfinite(fixed_rate_)) {
        throw std::invalid_argument("FixedLeg: fixed rate must be finite");
    }
}

double FixedLeg::present_value(const YieldCurve& curve) const {
    if (uses_periods_) {
        throw std::logic_error("FixedLeg period-based pricing: not implemented (Phase 2 step 4)");
    }
    // PV = K * annuity.  Math note §3.
    return fixed_rate_ * annuity(curve);
}

double FixedLeg::annuity(const YieldCurve& curve) const {
    if (uses_periods_) {
        throw std::logic_error("FixedLeg period-based annuity: not implemented (Phase 2 step 4)");
    }
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
