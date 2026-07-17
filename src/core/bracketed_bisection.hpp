#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace irc {

struct BisectionOptions {
    double residual_tolerance = 1e-12;
    std::size_t max_iterations = 200;
};

struct BisectionResult {
    double root;
    double residual;
    std::size_t iterations;
};

template <typename F>
concept ScalarResidual =
    std::invocable<F&, double> && std::convertible_to<std::invoke_result_t<F&, double>, double>;

// Solves residual(x) = 0 on an already-bracketed finite interval.
// The callable may capture auxiliary state; it is invoked as an lvalue and is
// neither copied into persistent storage nor retained after this call.
template <ScalarResidual F>
BisectionResult bracketed_bisection(F&& residual, double lower, double upper,
                                    BisectionOptions options = {}) {
    if (!std::isfinite(lower) || !std::isfinite(upper) || lower >= upper) {
        throw std::invalid_argument(
            "bracketed_bisection: bounds must be finite with lower < upper");
    }
    if (!std::isfinite(options.residual_tolerance) || options.residual_tolerance <= 0.0) {
        throw std::invalid_argument(
            "bracketed_bisection: residual tolerance must be finite and positive");
    }
    if (options.max_iterations == 0) {
        throw std::invalid_argument("bracketed_bisection: max_iterations must be positive");
    }

    (void)residual;
    throw std::logic_error("bracketed_bisection: not implemented (Phase 2 step 4)");
}

}  // namespace irc
