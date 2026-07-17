#include "risk/dv01.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace irc {
namespace {

void validate_half_width(double half_width, const char* function_name) {
    if (!std::isfinite(half_width) || half_width <= 0.0) {
        throw std::invalid_argument(std::string(function_name) +
                                    ": half-width must be finite and positive");
    }
}

void validate_pricer(const CurvePricer& pv, const char* function_name) {
    if (!pv) {
        throw std::invalid_argument(std::string(function_name) + ": pricer is empty");
    }
}

}  // namespace

std::vector<double> curve_node_sensitivities(const PiecewiseLogLinearCurve& curve,
                                             const CurvePricer& pv, double node_half_width) {
    validate_half_width(node_half_width, "curve_node_sensitivities");
    validate_pricer(pv, "curve_node_sensitivities");
    (void)curve;
    throw std::logic_error("curve_node_sensitivities: not implemented (Phase 2 step 4)");
}

std::vector<double> quote_dv01_direct(const SofrMarketData& market,
                                      const SofrCurveBootstrapper& bootstrapper,
                                      const CurvePricer& pv, double quote_half_width) {
    validate_half_width(quote_half_width, "quote_dv01_direct");
    validate_pricer(pv, "quote_dv01_direct");
    (void)market;
    (void)bootstrapper;
    throw std::logic_error("quote_dv01_direct: not implemented (Phase 2 step 4)");
}

std::vector<std::vector<double>> calibration_jacobian(const SofrMarketData& market,
                                                      const SofrCurveBootstrapper& bootstrapper,
                                                      const PiecewiseLogLinearCurve& curve,
                                                      double node_half_width) {
    validate_half_width(node_half_width, "calibration_jacobian");
    (void)market;
    (void)bootstrapper;
    (void)curve;
    throw std::logic_error("calibration_jacobian: not implemented (Phase 2 step 4)");
}

std::vector<double> quote_dv01_via_jacobian(const std::vector<std::vector<double>>& jacobian,
                                            const std::vector<double>& curve_sensitivities) {
    const std::size_t size = jacobian.size();
    if (size == 0) {
        throw std::invalid_argument("quote_dv01_via_jacobian: matrix is empty");
    }
    if (curve_sensitivities.size() != size) {
        throw std::invalid_argument(
            "quote_dv01_via_jacobian: sensitivity dimension does not match matrix");
    }

    for (std::size_t row = 0; row < size; ++row) {
        if (jacobian[row].size() != size) {
            throw std::invalid_argument("quote_dv01_via_jacobian: row " + std::to_string(row) +
                                        " is not square");
        }
        if (!std::isfinite(curve_sensitivities[row])) {
            throw std::invalid_argument("quote_dv01_via_jacobian: sensitivity " +
                                        std::to_string(row) + " is not finite");
        }
        for (std::size_t column = 0; column < size; ++column) {
            const double value = jacobian[row][column];
            if (!std::isfinite(value)) {
                throw std::invalid_argument("quote_dv01_via_jacobian: entry (" +
                                            std::to_string(row) + "," + std::to_string(column) +
                                            ") is not finite");
            }
            if (column > row && std::abs(value) > 1e-10) {
                throw std::invalid_argument("quote_dv01_via_jacobian: strict-upper entry (" +
                                            std::to_string(row) + "," + std::to_string(column) +
                                            ") violates lower-triangular tolerance");
            }
        }
    }

    for (std::size_t column = 0; column < size; ++column) {
        double scale = 1.0;
        for (std::size_t row = column; row < size; ++row) {
            scale = std::max(scale, std::abs(jacobian[row][column]));
        }
        const double pivot_floor = 100.0 * std::numeric_limits<double>::epsilon() * scale;
        if (std::abs(jacobian[column][column]) <= pivot_floor) {
            throw std::invalid_argument("quote_dv01_via_jacobian: diagonal pivot " +
                                        std::to_string(column) + " is singular");
        }
    }

    throw std::logic_error("quote_dv01_via_jacobian: not implemented (Phase 2 step 4)");
}

}  // namespace irc
