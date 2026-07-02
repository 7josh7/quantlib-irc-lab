#pragma once
#include "core/yield_curve.hpp"

#include <ql/time/daycounter.hpp>

namespace irc {

// Flat continuously-compounded zero curve: P(t,T) = exp(-r * tau(t,T)).
// The curve day counter (Act/365F in Phase 1) is a separate concern from
// the leg accrual day counter (Act/360) — see impl note §1.
class FlatCurve final : public YieldCurve {
public:
    FlatCurve(QuantLib::Date reference,
              double zero_rate,
              QuantLib::DayCounter day_counter);

    double discount(const QuantLib::Date& d) const override;
    QuantLib::Date reference_date() const override;

private:
    QuantLib::Date reference_;
    double zero_rate_;
    QuantLib::DayCounter day_counter_;
};

}  // namespace irc
