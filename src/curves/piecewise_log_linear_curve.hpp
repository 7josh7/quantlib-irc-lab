#pragma once

#include "core/linear_interpolator.hpp"
#include "core/yield_curve.hpp"

#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace irc {

struct CurveNode {
    QuantLib::Date date;
    double log_discount;
};

// Discount curve stored as log discount factors at dated pillars.
class PiecewiseLogLinearCurve final : public YieldCurve {
public:
    PiecewiseLogLinearCurve(QuantLib::Date reference, std::vector<CurveNode> pillars,
                            QuantLib::DayCounter day_counter);

    double discount(const QuantLib::Date& d) const override;
    QuantLib::Date reference_date() const override;
    const QuantLib::DayCounter& day_counter() const;
    std::span<const CurveNode> nodes() const;

private:
    QuantLib::Date reference_;
    std::vector<CurveNode> nodes_;
    QuantLib::DayCounter day_counter_;
    LinearFlatInterpolator interpolator_;
};

PiecewiseLogLinearCurve bump_node(const PiecewiseLogLinearCurve& curve, std::size_t node_index,
                                  double delta);

}  // namespace irc
