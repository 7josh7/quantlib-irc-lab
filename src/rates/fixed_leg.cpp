#include "rates/fixed_leg.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace irc {
namespace {
std::vector<CouponPeriod> make_fixed_zero_lag_periods(const QuantLib::Schedule& schedule,
                                                      const QuantLib::DayCounter& day_counter) {
    if (schedule.size() < 2) {
        throw std::invalid_argument("FixedLeg: schedule needs at least 2 dates");
    }
    if (day_counter.empty()) {
        throw std::invalid_argument("FixedLeg: day counter is empty");
    }
    std::vector<CouponPeriod> periods;
    periods.reserve(schedule.size() - 1);

    for (QuantLib::Size i = 1; i < schedule.size(); ++i) {
        const QuantLib::Date& start = schedule[i - 1];
        const QuantLib::Date& end = schedule[i];

        periods.push_back(CouponPeriod{start, end, end, day_counter.yearFraction(start, end)});
    }
    return periods;
}
}  // namespace

FixedLeg::FixedLeg(QuantLib::Schedule schedule, QuantLib::DayCounter day_counter, double notional,
                   double fixed_rate)
    : FixedLeg(make_fixed_zero_lag_periods(schedule, day_counter), notional, fixed_rate) {}

FixedLeg::FixedLeg(std::vector<CouponPeriod> periods, double notional, double fixed_rate)
    : periods_(std::move(periods)), notional_(notional), fixed_rate_(fixed_rate) {
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
    // PV = K * annuity.  Math note §3.
    return fixed_rate_ * annuity(curve);
}

double FixedLeg::annuity(const YieldCurve& curve) const {
    // annuity = N * sum_i tau_i * P(t, U_i), where U_i is payment_date.
    // Includes the notional; the math note's A(t) is per-notional.
    double sum = 0.0;
    for (const CouponPeriod& period : periods_) {
        const QuantLib::Date& payment = period.payment_date;
        const double tau = period.year_fraction;
        sum += tau * curve.discount(payment);
    }
    return notional_ * sum;
}
}  // namespace irc
