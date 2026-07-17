#pragma once

#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>
#include <ql/types.hpp>

#include <vector>

namespace irc {

// One coupon accrual period with its (possibly delayed) payment date.
struct CouponPeriod {
    QuantLib::Date accrual_start;
    QuantLib::Date accrual_end;
    QuantLib::Date payment_date;
    double year_fraction;

    bool operator==(const CouponPeriod&) const = default;
};

std::vector<CouponPeriod> make_coupon_periods(const QuantLib::Schedule& schedule,
                                              const QuantLib::DayCounter& day_counter,
                                              const QuantLib::Calendar& payment_calendar,
                                              QuantLib::Natural payment_lag_business_days);

}  // namespace irc
