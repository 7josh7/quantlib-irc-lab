#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace irc {

// Vectorized piecewise-linear interpolator with flat extrapolation.
// Knots xs (strictly increasing) each carry m = ys.size()/xs.size() outputs,
// stored row-major: ys = {y(x0)[0..m), y(x1)[0..m), ...}.
// evaluate() returns queries.size()*m values, row-major per query.
// Queries need not be sorted. Outside [xs.front(), xs.back()] the end
// values are returned unchanged (flat extrapolation).
class LinearFlatInterpolator {
public:
    LinearFlatInterpolator(std::vector<double> xs, std::vector<double> ys);

    std::vector<double> evaluate(std::span<const double> queries) const;

    std::size_t outputs_per_knot() const;

private:
    std::vector<double> xs_;
    std::vector<double> ys_;
    std::size_t m_;
};

}  // namespace irc
