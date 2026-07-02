#pragma once
#include "core/yield_curve.hpp"

#include <ql/time/daycounter.hpp>
#include <ql/time/schedule.hpp>

namespace irc {

// Fixed leg of a vanilla swap. Math note §3:
//   CF_i = N * K * tau_i,   PV = N * K * sum_i tau_i P(t,T_i).
class FixedLeg {
public:
    FixedLeg(QuantLib::Schedule schedule,
             QuantLib::DayCounter day_counter,
             double notional,
             double fixed_rate);

    // PV = fixed_rate * annuity(curve).
    double present_value(const YieldCurve& curve) const;

    // annuity() = N * sum_i tau_i P(t,T_i)  — money per unit rate.
    // NOTE: includes the notional; the math note's A(t) is per-notional.
    double annuity(const YieldCurve& curve) const;

private:
    QuantLib::Schedule schedule_;
    QuantLib::DayCounter day_counter_;
    double notional_;
    double fixed_rate_;
};

}  // namespace irc
