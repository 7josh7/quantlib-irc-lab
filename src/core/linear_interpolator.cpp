#include "core/linear_interpolator.hpp"

#include <algorithm>
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
    std::vector<double> results;
    results.reserve(queries.size() * m_);
    for (const double query : queries) {
        // Branch order matters: the <= and >= below guarantee
        // xs_.front() < query < xs_.back() in the final branch, which is what makes
        // right >= 1 (so left = right - 1 cannot underflow to SIZE_MAX) and
        // right_it != end() (so *right_it is a safe dereference).
        if (query <= xs_.front()) {
            for (std::size_t j = 0; j < m_; ++j) {
                results.push_back(ys_[j]);
            }
        } else if (query >= xs_.back()) {
            const std::size_t offset = (xs_.size() - 1) * m_;
            for (std::size_t j = 0; j < m_; ++j) {
                results.push_back(ys_[offset + j]);
            }
        } else {
            const auto right_it = std::lower_bound(xs_.begin(), xs_.end(), query);
            const std::size_t right = static_cast<std::size_t>(right_it - xs_.begin());
            if (*right_it == query) {  // a fast path
                const std::size_t offset = right * m_;
                for (std::size_t j = 0; j < m_; ++j) {
                    results.push_back(ys_[offset + j]);
                }
                continue;
            }
            const std::size_t left = right - 1;
            const double weight = (query - xs_[left]) / (xs_[right] - xs_[left]);
            const std::size_t offset = left * m_;
            for (std::size_t j = 0; j < m_; ++j) {
                results.push_back(ys_[offset + j] * (1.0 - weight) +
                                  (ys_[offset + m_ + j] * weight));
            }
        }
    }
    return results;
}

std::size_t LinearFlatInterpolator::outputs_per_knot() const {
    return m_;
}

}  // namespace irc
