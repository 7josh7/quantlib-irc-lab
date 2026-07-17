#pragma once

#include "curves/curve_instruments.hpp"
#include "curves/piecewise_log_linear_curve.hpp"
#include "curves/sofr_bootstrapper.hpp"

#include <functional>
#include <vector>

namespace irc {

using CurvePricer = std::function<double(const YieldCurve&)>;

std::vector<double> curve_node_sensitivities(const PiecewiseLogLinearCurve& curve,
                                             const CurvePricer& pv, double node_half_width = 1e-6);

std::vector<double> quote_dv01_direct(const SofrMarketData& market,
                                      const SofrCurveBootstrapper& bootstrapper,
                                      const CurvePricer& pv, double quote_half_width = 1e-4);

std::vector<std::vector<double>> calibration_jacobian(const SofrMarketData& market,
                                                      const SofrCurveBootstrapper& bootstrapper,
                                                      const PiecewiseLogLinearCurve& curve,
                                                      double node_half_width = 1e-6);

std::vector<double> quote_dv01_via_jacobian(const std::vector<std::vector<double>>& jacobian,
                                            const std::vector<double>& curve_sensitivities);

}  // namespace irc
