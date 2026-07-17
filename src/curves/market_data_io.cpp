#include "curves/market_data_io.hpp"

#include <stdexcept>

namespace irc {

SofrMarketData load_sofr_market_data(const std::filesystem::path& quotes_csv,
                                     const std::filesystem::path& fixings_csv) {
    (void)quotes_csv;
    (void)fixings_csv;
    throw std::logic_error("load_sofr_market_data: not implemented (Phase 2 step 4)");
}

}  // namespace irc
