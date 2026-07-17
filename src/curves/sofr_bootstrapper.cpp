#include "curves/sofr_bootstrapper.hpp"

#include <stdexcept>

namespace irc {

BootstrapResult SofrCurveBootstrapper::bootstrap(const SofrMarketData& market) const {
    (void)market;
    throw std::logic_error("SofrCurveBootstrapper::bootstrap: not implemented (Phase 2 step 4)");
}

std::vector<double> SofrCurveBootstrapper::model_quotes(
    const SofrMarketData& market, const PiecewiseLogLinearCurve& curve) const {
    (void)market;
    (void)curve;
    throw std::logic_error("SofrCurveBootstrapper::model_quotes: not implemented (Phase 2 step 4)");
}

}  // namespace irc
