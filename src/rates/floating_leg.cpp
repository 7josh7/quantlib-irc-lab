#include "rates/floating_leg.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace irc {
namespace {
std::vector<CouponPeriod> make_float_zero_lag_periods(const QuantLib::Schedule& schedule,
                                                      const QuantLib::DayCounter& day_counter) {
    if (schedule.size() < 2) {
        throw std::invalid_argument("FloatingLeg: schedule needs at least 2 dates");
    }
    if (day_counter.empty()) {
        throw std::invalid_argument("FloatingLeg: day counter is empty");
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
FloatingLeg::FloatingLeg(QuantLib::Schedule schedule, QuantLib::DayCounter day_counter,
                         double notional, std::shared_ptr<const RateAccrual> accrual, double spread)
    : FloatingLeg(make_float_zero_lag_periods(schedule, day_counter), notional, std::move(accrual),
                  spread) {}

FloatingLeg::FloatingLeg(std::vector<CouponPeriod> periods, double notional,
                         std::shared_ptr<const RateAccrual> accrual, double spread)
    : periods_(std::move(periods)),
      notional_(notional),
      accrual_(std::move(accrual)),
      spread_(spread) {
    if (periods_.empty()) {
        throw std::invalid_argument("FloatingLeg: periods must not be empty");
    }
    for (std::size_t i = 0; i < periods_.size(); ++i) {
        const CouponPeriod& period = periods_[i];
        if (period.accrual_start == QuantLib::Date() || period.accrual_end == QuantLib::Date() ||
            period.payment_date == QuantLib::Date()) {
            throw std::invalid_argument("FloatingLeg: coupon period contains a null date");
        }
        if (period.accrual_start >= period.accrual_end) {
            throw std::invalid_argument("FloatingLeg: accrual start must precede accrual end");
        }
        if (period.payment_date < period.accrual_end) {
            throw std::invalid_argument("FloatingLeg: payment date precedes accrual end");
        }
        if (!std::isfinite(period.year_fraction) || period.year_fraction <= 0.0) {
            throw std::invalid_argument(
                "FloatingLeg: coupon year fraction must be finite and positive");
        }
        if (i > 0 && period.accrual_start < periods_[i - 1].accrual_end) {
            throw std::invalid_argument("FloatingLeg: coupon periods overlap or are unordered");
        }
    }
    if (!std::isfinite(notional_) || notional_ <= 0.0) {
        throw std::invalid_argument("FloatingLeg: notional must be finite and positive");
    }
    if (!accrual_) {
        throw std::invalid_argument("FloatingLeg: accrual strategy is null");
    }
    if (!std::isfinite(spread_)) {
        throw std::invalid_argument("FloatingLeg: spread must be finite");
    }
}

double FloatingLeg::present_value(const YieldCurve& curve) const {
    // PV = N * sum_i tau_i * (F_i + s) * P(t, T_i), with F_i from the
    // injected accrual strategy. Math note §2 (projected coupon PV).
    double pv = 0.0;
    for (const CouponPeriod& period : periods_) {
        const double tau = period.year_fraction;
        const double forward = accrual_->forward_rate(curve, period.accrual_start,
                                                      period.accrual_end, period.year_fraction);
        pv += tau * (forward + spread_) * curve.discount(period.payment_date);
    }
    return notional_ * pv;
}

}  // namespace irc
