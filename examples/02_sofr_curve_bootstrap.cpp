#include "curves/curve_io.hpp"
#include "curves/market_data_io.hpp"
#include "curves/sofr_bootstrapper.hpp"
#include "rates/coupon_period.hpp"
#include "rates/fixed_leg.hpp"
#include "rates/floating_leg.hpp"
#include "rates/rate_accrual.hpp"
#include "rates/vanilla_swap.hpp"
#include "risk/dv01.hpp"

#include <ql/quantlib.hpp>

#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kNotional = 1'000'000.0;
constexpr double kFixedRate = 0.035;

std::vector<irc::CouponPeriod> ten_year_periods(const QuantLib::Date& valuation_date) {
    const QuantLib::Calendar usny{QuantLib::UnitedStates(QuantLib::UnitedStates::Settlement)};
    const QuantLib::Date spot = usny.advance(valuation_date, 2, QuantLib::Days);
    const QuantLib::Date maturity =
        usny.advance(spot, 10, QuantLib::Years, QuantLib::ModifiedFollowing);
    const QuantLib::Schedule schedule(spot, maturity, QuantLib::Period(QuantLib::Annual), usny,
                                      QuantLib::ModifiedFollowing, QuantLib::ModifiedFollowing,
                                      QuantLib::DateGeneration::Forward, false);
    return irc::make_coupon_periods(schedule, QuantLib::Actual360(), usny, 2);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: 02_sofr_curve_bootstrap <quotes.csv> <fixings.csv> <output.csv>\n";
        return 2;
    }

    try {
        const std::filesystem::path quotes_path = argv[1];
        const std::filesystem::path fixings_path = argv[2];
        const std::filesystem::path output_path = argv[3];

        const irc::SofrMarketData market = irc::load_sofr_market_data(quotes_path, fixings_path);
        const irc::SofrCurveBootstrapper bootstrapper;
        const irc::BootstrapResult result = bootstrapper.bootstrap(market);
        irc::write_curve_csv(result.curve, output_path);

        const std::vector<irc::CouponPeriod> periods =
            ten_year_periods(market.as_of.valuation_date);
        const auto accrual = std::make_shared<const irc::CompoundedOvernightRate>(
            QuantLib::UnitedStates(QuantLib::UnitedStates::SOFR));
        const irc::VanillaSwap payer(irc::SwapSide::Payer,
                                     irc::FixedLeg(periods, kNotional, kFixedRate),
                                     irc::FloatingLeg(periods, kNotional, accrual));
        const irc::CurvePricer pv = [&payer](const irc::YieldCurve& curve) {
            return payer.npv(curve);
        };
        const std::vector<double> dv01 = irc::quote_dv01_direct(market, bootstrapper, pv);
        if (dv01.size() != result.diagnostics.size()) {
            throw std::runtime_error("example: DV01 and diagnostic counts do not match");
        }

        std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
        std::cout << "instrument_id,market_quote,model_quote,residual,solver_iterations,"
                     "expanded_bracket,dv01\n";
        for (std::size_t i = 0; i < result.diagnostics.size(); ++i) {
            const irc::CalibrationDiagnostic& diagnostic = result.diagnostics[i];
            std::cout << diagnostic.instrument_id << ',' << diagnostic.market_quote << ','
                      << diagnostic.model_quote << ',' << diagnostic.residual << ','
                      << diagnostic.solver_iterations << ','
                      << (diagnostic.used_expanded_bracket ? "true" : "false") << ',' << dv01[i]
                      << '\n';
        }

        std::cout << "curve_output=" << output_path.string() << '\n';
        std::cout << "payer_10y_npv=" << payer.npv(result.curve) << '\n';
        std::cout << "payer_10y_total_dv01=" << std::accumulate(dv01.begin(), dv01.end(), 0.0)
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
