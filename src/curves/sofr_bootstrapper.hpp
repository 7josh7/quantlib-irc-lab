#pragma once

#include "curves/curve_instruments.hpp"
#include "curves/piecewise_log_linear_curve.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace irc {

struct CalibrationDiagnostic {
    std::string instrument_id;
    double market_quote;
    double model_quote;
    double residual;
    std::size_t solver_iterations;
    bool used_expanded_bracket;
};

struct BootstrapResult {
    MarketAsOf as_of;
    PiecewiseLogLinearCurve curve;
    std::vector<CalibrationDiagnostic> diagnostics;
};

class SofrCurveBootstrapper {
public:
    BootstrapResult bootstrap(const SofrMarketData& market) const;

    std::vector<double> model_quotes(const SofrMarketData& market,
                                     const PiecewiseLogLinearCurve& curve) const;
};

}  // namespace irc
