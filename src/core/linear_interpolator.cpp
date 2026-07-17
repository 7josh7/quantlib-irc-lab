#include "core/linear_interpolator.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace irc {

LinearFlatInterpolator::LinearFlatInterpolator(std::vector<double> xs, std::vector<double> ys)
    : xs_(std::move(xs)), ys_(std::move(ys)), m_(0) {
    if (xs_.empty()) {
        throw std::invalid_argument("LinearFlatInterpolator: xs must not be empty");
    }
    for (std::size_t i = 0; i < xs_.size(); ++i) {
        if (!std::isfinite(xs_[i])) {
            throw std::invalid_argument("LinearFlatInterpolator: xs must be finite");
        }
        if (i > 0 && xs_[i] <= xs_[i - 1]) {
            throw std::invalid_argument("LinearFlatInterpolator: xs must be strictly increasing");
        }
    }
    if (ys_.empty() || ys_.size() % xs_.size() != 0) {
        throw std::invalid_argument(
            "LinearFlatInterpolator: ys size must be a positive multiple of xs size");
    }
    for (const double y : ys_) {
        if (!std::isfinite(y)) {
            throw std::invalid_argument("LinearFlatInterpolator: ys must be finite");
        }
    }
    m_ = ys_.size() / xs_.size();
}

std::vector<double> LinearFlatInterpolator::evaluate(std::span<const double> queries) const {
    for (const double query : queries) {
        if (!std::isfinite(query)) {
            throw std::invalid_argument("LinearFlatInterpolator: queries must be finite");
        }
    }
    throw std::logic_error("LinearFlatInterpolator::evaluate: not implemented (Phase 2 step 4)");
}

std::size_t LinearFlatInterpolator::outputs_per_knot() const {
    return m_;
}

}  // namespace irc
