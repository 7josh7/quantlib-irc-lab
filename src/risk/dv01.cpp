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
    std::vector<double> result;
    const std::span<const CurveNode> nodes = curve.nodes();
    result.reserve(nodes.size() - 1);
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        double s_i =
            (pv(bump_node(curve, i, node_half_width)) - pv(bump_node(curve, i, -node_half_width))) /
            (2.0 * node_half_width);
        result.push_back(s_i);
    }
    return result;
}
std::vector<double> quote_dv01_direct(const SofrMarketData& market,
                                      const SofrCurveBootstrapper& bootstrapper,
                                      const CurvePricer& pv, double quote_half_width) {
    validate_half_width(quote_half_width, "quote_dv01_direct");
    validate_pricer(pv, "quote_dv01_direct");
    SofrMarketData bumped = market; 
    std::vector<double> result;
    result.reserve(bumped.futures.size() + bumped.ois.size());    
    for (std::size_t i = 0; i < bumped.futures.size(); ++i) {
        const double price = bumped.futures[i].price;
        bumped.futures[i].price = price - 100.0 * quote_half_width;
        const BootstrapResult up = bootstrapper.bootstrap(bumped);
        bumped.futures[i].price = price + 100.0 * quote_half_width;
        const BootstrapResult down = bootstrapper.bootstrap(bumped);
        bumped.futures[i].price = price;
        result.push_back((pv(up.curve) - pv(down.curve)) / (2.0 * quote_half_width) * 1e-4);
    }
    for (std::size_t i = 0; i < bumped.ois.size(); ++i) {
        const double rate = bumped.ois[i].par_rate;
        bumped.ois[i].par_rate = rate + quote_half_width;
        BootstrapResult res_u = bootstrapper.bootstrap(bumped);
        bumped.ois[i].par_rate = rate - quote_half_width;
        BootstrapResult res_d = bootstrapper.bootstrap(bumped);
        const double s_i = (pv(res_u.curve) - pv(res_d.curve)) / (2.0 * quote_half_width) * 1e-4;
        result.push_back(s_i);
        bumped.ois[i].par_rate = rate;
    }
    return result;
}

std::vector<std::vector<double>> calibration_jacobian(const SofrMarketData& market,
                                                      const SofrCurveBootstrapper& bootstrapper,
                                                      const PiecewiseLogLinearCurve& curve,
                                                      double node_half_width) {
    validate_half_width(node_half_width, "calibration_jacobian");
    // The curve constructor rejects empty pillars and always prepends the anchor,
    // so nodes().size() >= 2 and m >= 1 — this subtraction cannot underflow.
    const std::size_t m = curve.nodes().size() - 1;
    std::vector<std::vector<double>> jacobian(m, std::vector<double>(m, 0.0));

    for (std::size_t n = 1; n <= m; ++n) {
        const std::vector<double> up =
            bootstrapper.model_quotes(market, bump_node(curve, n, node_half_width));
        const std::vector<double> down =
            bootstrapper.model_quotes(market, bump_node(curve, n, -node_half_width));
        for (std::size_t row = 0; row < m; ++row) {
            jacobian[row][n - 1] = (up[row] - down[row]) / (2.0 * node_half_width);
        }
    }
    return jacobian;
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
    std::vector<double> result(size);
    std::vector<double> quote_sensitivities(size, 0.0);
    for (std::size_t n = size; n-- > 0;) {  // size-1 down to 0
        double sum = curve_sensitivities[n];
        for (std::size_t m = n + 1; m < size; ++m) {
            sum -= jacobian[m][n] * quote_sensitivities[m];
        }
        quote_sensitivities[n] = sum / jacobian[n][n];
        if (!std::isfinite(quote_sensitivities[n])) {
            throw std::runtime_error("quote_dv01_via_jacobian: non-finite intermediate at index " +
                                     std::to_string(n));
        }
        result[n] = quote_sensitivities[n] * 1e-4;
    }
    return result;
}

}  // namespace irc
