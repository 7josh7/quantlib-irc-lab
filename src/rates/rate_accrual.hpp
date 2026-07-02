#pragma once
#include "core/yield_curve.hpp"

#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>

namespace irc {

// Strategy (CPP Ch.5): the projected floating rate F_i for one accrual
// period [start, end]. Single-curve only in v1: the curve passed in is
// both discount and projection (math note, Scope).
class RateAccrual {
public:
    virtual ~RateAccrual() = default;

    virtual double forward_rate(const YieldCurve& curve,
                                const QuantLib::Date& start,
                                const QuantLib::Date& end,
                                double year_fraction) const = 0;
};

// IBOR-style simple forward:  F = (P(start)/P(end) - 1) / tau.
// Math note §4 (single-curve forward).
class SimpleForwardRate final : public RateAccrual {
public:
    double forward_rate(const YieldCurve& curve,
                        const QuantLib::Date& start,
                        const QuantLib::Date& end,
                        double year_fraction) const override;
};

// SOFR daily-compounded overnight rate (math note §2, RFR block):
//   R = [ prod_k (1 + delta_k * r_k) - 1 ] / tau
// Iterates the business days in [start, end), compounding overnight
// forwards read off the curve. On a single curve this telescopes to
// SimpleForwardRate — the tests assert that agreement, so this class
// must genuinely iterate rather than shortcut to the telescoped form.
class CompoundedOvernightRate final : public RateAccrual {
public:
    explicit CompoundedOvernightRate(QuantLib::Calendar calendar);

    double forward_rate(const YieldCurve& curve,
                        const QuantLib::Date& start,
                        const QuantLib::Date& end,
                        double year_fraction) const override;

private:
    QuantLib::Calendar calendar_;
};

}  // namespace irc
