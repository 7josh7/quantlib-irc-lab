#include "curves/piecewise_log_linear_curve.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace irc {
namespace {

std::vector<CurveNode> make_nodes(const QuantLib::Date& reference,
                                  const std::vector<CurveNode>& pillars,
                                  const QuantLib::DayCounter& day_counter) {
    if (reference == QuantLib::Date()) {
        throw std::invalid_argument("PiecewiseLogLinearCurve: reference date is null");
    }
    if (day_counter.empty()) {
        throw std::invalid_argument("PiecewiseLogLinearCurve: day counter is empty");
    }
    if (pillars.empty()) {
        throw std::invalid_argument("PiecewiseLogLinearCurve: pillars must not be empty");
    }

    std::vector<CurveNode> nodes;
    nodes.reserve(pillars.size() + 1);
    nodes.push_back({reference, 0.0});
    QuantLib::Date previous = reference;
    for (const CurveNode& pillar : pillars) {
        if (pillar.date == QuantLib::Date()) {
            throw std::invalid_argument("PiecewiseLogLinearCurve: pillar date is null");
        }
        if (pillar.date <= previous) {
            throw std::invalid_argument(
                "PiecewiseLogLinearCurve: pillar dates must be strictly increasing after "
                "reference");
        }
        if (!std::isfinite(pillar.log_discount)) {
            throw std::invalid_argument(
                "PiecewiseLogLinearCurve: log discount factors must be finite");
        }
        const double discount = std::exp(pillar.log_discount);
        if (!std::isfinite(discount) || discount <= 0.0) {
            throw std::invalid_argument(
                "PiecewiseLogLinearCurve: discount factors must be finite and positive");
        }
        nodes.push_back(pillar);
        previous = pillar.date;
    }
    return nodes;
}

std::vector<double> make_times(const QuantLib::Date& reference, const std::vector<CurveNode>& nodes,
                               const QuantLib::DayCounter& day_counter) {
    std::vector<double> times;
    times.reserve(nodes.size());
    for (const CurveNode& node : nodes) {
        times.push_back(day_counter.yearFraction(reference, node.date));
    }
    return times;
}

std::vector<double> make_log_discounts(const std::vector<CurveNode>& nodes) {
    std::vector<double> values;
    values.reserve(nodes.size());
    for (const CurveNode& node : nodes) {
        values.push_back(node.log_discount);
    }
    return values;
}

}  // namespace

PiecewiseLogLinearCurve::PiecewiseLogLinearCurve(QuantLib::Date reference,
                                                 std::vector<CurveNode> pillars,
                                                 QuantLib::DayCounter day_counter)
    : reference_(reference),
      nodes_(make_nodes(reference, pillars, day_counter)),
      day_counter_(std::move(day_counter)),
      interpolator_(make_times(reference_, nodes_, day_counter_), make_log_discounts(nodes_)) {}

double PiecewiseLogLinearCurve::discount(const QuantLib::Date& d) const {
    if (d == QuantLib::Date()) {
        throw std::invalid_argument("PiecewiseLogLinearCurve::discount: date is null");
    }
    if (d < reference_) {
        throw std::invalid_argument("PiecewiseLogLinearCurve::discount: date is before reference");
    }
    if (d > nodes_.back().date) {
        throw std::invalid_argument(
            "PiecewiseLogLinearCurve::discount: date is beyond last pillar");
    }
    const double time = day_counter_.yearFraction(reference_, d);
    const std::array<double, 1> query{time};
    const std::vector<double> log_discounts = interpolator_.evaluate(query);
    return std::exp(log_discounts.front());
}

QuantLib::Date PiecewiseLogLinearCurve::reference_date() const {
    return reference_;
}

const QuantLib::DayCounter& PiecewiseLogLinearCurve::day_counter() const {
    return day_counter_;
}

std::span<const CurveNode> PiecewiseLogLinearCurve::nodes() const {
    return nodes_;
}

PiecewiseLogLinearCurve bump_node(const PiecewiseLogLinearCurve& curve, std::size_t node_index,
                                  double delta) {
    if (node_index == 0 || node_index >= curve.nodes().size()) {
        throw std::invalid_argument("bump_node: anchor or out-of-range node index");
    }
    if (!std::isfinite(delta)) {
        throw std::invalid_argument("bump_node: delta must be finite");
    }
    const std::span<const CurveNode> nodes = curve.nodes();
    std::vector<CurveNode> pillars(nodes.begin() + 1, nodes.end());
    const std::size_t pillar_index = node_index - 1;
    const double bumped_log_discount = pillars[pillar_index].log_discount + delta;
    if (!std::isfinite(bumped_log_discount)) {
        throw std::invalid_argument("bump_node: bumped log discount must be finite");
    }
    pillars[pillar_index].log_discount = bumped_log_discount;
    return PiecewiseLogLinearCurve(curve.reference_date(), std::move(pillars), curve.day_counter());
}

}  // namespace irc
