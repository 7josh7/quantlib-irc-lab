#include "curves/curve_instruments.hpp"
#include "curves/market_data_io.hpp"

#include <ql/quantlib.hpp>

#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace {

using QuantLibCurve = QuantLib::PiecewiseYieldCurve<QuantLib::Discount, QuantLib::LogLinear>;

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: 02_quantlib_sofr_curve_bootstrap <quotes.csv> <fixings.csv>\n";
        return 2;
    }

    try {
        const QuantLib::SavedSettings settings_guard;
        const irc::SofrMarketData market = irc::load_sofr_market_data(
            std::filesystem::path(argv[1]), std::filesystem::path(argv[2]));

        QuantLib::Settings::instance().evaluationDate() = market.as_of.valuation_date;
        QuantLib::IndexManager::instance().clearHistories();

        const auto sofr = QuantLib::ext::make_shared<QuantLib::Sofr>();
        for (const irc::SofrFixing& fixing : market.fixings) {
            sofr->addFixing(fixing.rate_date, fixing.rate, true);
        }

        std::vector<QuantLib::ext::shared_ptr<QuantLib::RateHelper>> helpers;
        helpers.reserve(market.futures.size() + market.ois.size());
        for (const irc::SofrFutureQuote& future : market.futures) {
            helpers.push_back(QuantLib::ext::make_shared<QuantLib::SofrFutureRateHelper>(
                future.price, future.reference_start.month(), future.reference_start.year(),
                QuantLib::Quarterly, 0.0, QuantLib::Pillar::LastRelevantDate));
        }

        const QuantLib::Calendar usny{QuantLib::UnitedStates(QuantLib::UnitedStates::Settlement)};
        for (const irc::OisQuote& ois : market.ois) {
            helpers.push_back(QuantLib::ext::make_shared<QuantLib::OISRateHelper>(
                2, ois.tenor, ois.par_rate, sofr, QuantLib::Handle<QuantLib::YieldTermStructure>(),
                false, 2, QuantLib::ModifiedFollowing, QuantLib::Annual, usny, 0 * QuantLib::Days,
                0.0, QuantLib::Pillar::LastRelevantDate, QuantLib::Date(),
                QuantLib::RateAveraging::Compound, QuantLib::ext::nullopt, QuantLib::Annual, usny,
                QuantLib::Null<QuantLib::Natural>(), 0, false,
                QuantLib::ext::shared_ptr<QuantLib::FloatingRateCouponPricer>(),
                QuantLib::DateGeneration::Backward, usny, QuantLib::ModifiedFollowing));
        }

        const auto curve = QuantLib::ext::make_shared<QuantLibCurve>(
            market.as_of.valuation_date, helpers, QuantLib::Actual365Fixed(), 1e-12);
        (void)curve->discount(curve->maxDate());

        std::cout << std::setprecision(std::numeric_limits<double>::max_digits10);
        std::cout << "instrument_id,pillar_date,market_quote,model_quote,residual,discount\n";
        for (std::size_t i = 0; i < helpers.size(); ++i) {
            const bool is_future = i < market.futures.size();
            const double market_quote =
                is_future ? irc::sofr_future_rate_from_price(market.futures[i].price)
                          : market.ois[i - market.futures.size()].par_rate;
            const double implied_native_quote = helpers[i]->impliedQuote();
            const double model_quote = is_future
                                           ? irc::sofr_future_rate_from_price(implied_native_quote)
                                           : implied_native_quote;
            const std::string& instrument_id =
                is_future ? market.futures[i].id : market.ois[i - market.futures.size()].id;
            const QuantLib::Date pillar_date = helpers[i]->pillarDate();

            std::cout << instrument_id << ',' << QuantLib::io::iso_date(pillar_date) << ','
                      << market_quote << ',' << model_quote << ',' << model_quote - market_quote
                      << ',' << curve->discount(pillar_date) << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
