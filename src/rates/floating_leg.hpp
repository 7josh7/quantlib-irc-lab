#pragma once
#include "core/yield_curve.hpp"
#include "rates/rate_accrual.hpp"

#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

#include <memory>

namespace irc {

// Floating leg of a vanilla swap. Math note §2:
//   PV = N * sum_i tau_i * (F_i + s) * P(t,T_i),
// where F_i comes from the injected RateAccrual strategy (simple IBOR-style
// or daily-compounded SOFR-style).
class FloatingLeg {
public:
    FloatingLeg(QuantLib::Schedule schedule, QuantLib::DayCounter day_counter, double notional,
                std::shared_ptr<const RateAccrual> accrual, double spread = 0.0);

    double present_value(const YieldCurve& curve) const;

private:
    QuantLib::Schedule schedule_;
    QuantLib::DayCounter day_counter_;
    double notional_;
    std::shared_ptr<const RateAccrual> accrual_;
    double spread_;
};

}  // namespace irc
