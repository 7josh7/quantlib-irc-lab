#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>
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

class RootNotBracketedError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
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

    double f_lower = std::invoke(residual, lower);
    double f_upper = std::invoke(residual, upper);
    const auto make_bracket_message = [&](std::string_view reason) {
        std::ostringstream message;
        message << std::setprecision(std::numeric_limits<double>::max_digits10)
                << "bracketed_bisection: " << reason << ": lower=" << lower
                << ", f(lower)=" << f_lower << ", upper=" << upper << ", f(upper)=" << f_upper;
        return message.str();
    };

    if (!std::isfinite(f_lower) || !std::isfinite(f_upper)) {
        throw std::runtime_error(make_bracket_message("non-finite endpoint residual"));
    }
    if (std::abs(f_lower) <= options.residual_tolerance) {
        return BisectionResult{lower, f_lower, 0};
    }
    if (std::abs(f_upper) <= options.residual_tolerance) {
        return BisectionResult{upper, f_upper, 0};
    }
    if (std::signbit(f_lower) == std::signbit(f_upper)) {
        throw RootNotBracketedError(make_bracket_message("interval does not bracket a root"));
    }
    for (std::size_t index = 0; index < options.max_iterations; ++index) {
        const std::size_t iteration = index + 1;
        const double mid = std::midpoint(lower, upper);
        const double f_mid = std::invoke(residual, mid);
        if (!std::isfinite(f_mid)) {
            std::ostringstream reason;
            reason << std::setprecision(std::numeric_limits<double>::max_digits10)
                   << "non-finite midpoint residual at iteration=" << iteration
                   << ", midpoint=" << mid << ", residual=" << f_mid;
            throw std::runtime_error(make_bracket_message(reason.str()));
        }
        if (std::abs(f_mid) <= options.residual_tolerance) {
            return BisectionResult{mid, f_mid, iteration};
        }
        if (std::signbit(f_lower) != std::signbit(f_mid)) {
            upper = mid;
            f_upper = f_mid;
        } else {
            lower = mid;
            f_lower = f_mid;
        }
    }
    std::ostringstream reason;
    reason << std::setprecision(std::numeric_limits<double>::max_digits10)
           << "maximum iterations exhausted after " << options.max_iterations
           << " midpoint evaluations, residual_tolerance=" << options.residual_tolerance;
    throw std::runtime_error(make_bracket_message(reason.str()));
}

}  // namespace irc
