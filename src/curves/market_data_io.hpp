#pragma once

#include "curves/curve_instruments.hpp"

#include <filesystem>

namespace irc {

SofrMarketData load_sofr_market_data(const std::filesystem::path& quotes_csv,
                                     const std::filesystem::path& fixings_csv = {});

}  // namespace irc
