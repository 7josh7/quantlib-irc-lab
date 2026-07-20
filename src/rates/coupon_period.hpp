#pragma once

#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>
#include <ql/types.hpp>

#include <vector>

namespace irc {

// One coupon accrual period with its (possibly delayed) payment date.
// Accrual boundaries are never replaced by the payment date (Section 2a); the
// stored year_fraction is the leg day counter's accrual over
// [accrual_start, accrual_end], so pricing never re-derives it.
struct CouponPeriod {
    QuantLib::Date accrual_start;
    QuantLib::Date accrual_end;
    QuantLib::Date payment_date;
    double year_fraction;

    bool operator==(const CouponPeriod&) const = default;
};

// One period per consecutive schedule date pair:
//   payment_date  = payment_calendar.advance(accrual_end, lag, Days)
//   year_fraction = day_counter.yearFraction(accrual_start, accrual_end)
// payment_lag_business_days == 0 returns payment_date == accrual_end
// unchanged - the builder never calls advance(d, 0, Days), because QuantLib
// defines that as adjust(d), which could silently move an unadjusted date.
// The lag is signed (as in QuantLib's own paymentLag parameters) so a
// negative input arrives intact and throws std::invalid_argument instead of
// wrapping to a large Natural.
std::vector<CouponPeriod> make_coupon_periods(const QuantLib::Schedule& schedule,
                                              const QuantLib::DayCounter& day_counter,
                                              const QuantLib::Calendar& payment_calendar,
                                              QuantLib::Integer payment_lag_business_days);

}  // namespace irc
