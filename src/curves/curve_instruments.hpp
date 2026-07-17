#pragma once

#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace irc {

struct MarketAsOf {
    QuantLib::Date valuation_date;
    std::chrono::sys_seconds as_of_utc;

    bool operator==(const MarketAsOf&) const = default;
};

struct SofrFutureQuote {
    std::string id;
    QuantLib::Date reference_start;
    QuantLib::Date reference_end;
    double price;
};

struct OisQuote {
    std::string id;
    QuantLib::Period tenor;
    double par_rate;
};

struct SofrFixing {
    QuantLib::Date rate_date;
    double rate;
};

double sofr_future_rate_from_price(double price);
double sofr_future_price_from_rate(double rate);

double realized_accumulation(const std::vector<SofrFixing>& fixings, const QuantLib::Date& start,
                             const QuantLib::Date& end_exclusive,
                             const QuantLib::Calendar& fixing_calendar);

struct SofrMarketData {
    MarketAsOf as_of;
    std::vector<SofrFutureQuote> futures;
    std::vector<OisQuote> ois;
    std::vector<SofrFixing> fixings;
};

}  // namespace irc
