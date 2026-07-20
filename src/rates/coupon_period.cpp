#include "rates/coupon_period.hpp"

#include <stdexcept>

namespace irc {

std::vector<CouponPeriod> make_coupon_periods(const QuantLib::Schedule& schedule,
                                              const QuantLib::DayCounter& day_counter,
                                              const QuantLib::Calendar& payment_calendar,
                                              QuantLib::Integer payment_lag_business_days) {
    if (schedule.size() < 2) {
        throw std::invalid_argument("make_coupon_periods: schedule needs at least 2 dates");
    }
    if (day_counter.empty()) {
        throw std::invalid_argument("make_coupon_periods: day counter is empty");
    }
    if (payment_calendar.empty()) {
        throw std::invalid_argument("make_coupon_periods: payment calendar is empty");
    }
    if (payment_lag_business_days < 0) {
        throw std::invalid_argument("make_coupon_periods: payment lag must not be negative");
    }

    std::vector<CouponPeriod> periods;
    periods.reserve(schedule.size() - 1);
    for (std::size_t i = 0; i < schedule.size() - 1; ++i) {
        const QuantLib::Date& start = schedule[i];
        const QuantLib::Date& end = schedule[i + 1];
        const QuantLib::Date pay_date =
            payment_lag_business_days == 0
                ? end
                : payment_calendar.advance(end, payment_lag_business_days, QuantLib::Days);
        periods.push_back(CouponPeriod{start, end, pay_date, day_counter.yearFraction(start, end)});
    }
    return periods;
}

}  // namespace irc
