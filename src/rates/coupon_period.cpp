#include "rates/coupon_period.hpp"

#include <stdexcept>

namespace irc {

std::vector<CouponPeriod> make_coupon_periods(const QuantLib::Schedule& schedule,
                                              const QuantLib::DayCounter& day_counter,
                                              const QuantLib::Calendar& payment_calendar,
                                              QuantLib::Natural payment_lag_business_days) {
    if (schedule.size() < 2) {
        throw std::invalid_argument("make_coupon_periods: schedule needs at least 2 dates");
    }
    if (day_counter.empty()) {
        throw std::invalid_argument("make_coupon_periods: day counter is empty");
    }
    if (payment_calendar.empty()) {
        throw std::invalid_argument("make_coupon_periods: payment calendar is empty");
    }

    (void)payment_lag_business_days;
    throw std::logic_error("make_coupon_periods: not implemented (Phase 2 step 4)");
}

}  // namespace irc
