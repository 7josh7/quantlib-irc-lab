#include "curves/curve_instruments.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace irc {

double sofr_future_rate_from_price(double price) {
    if (!std::isfinite(price)) {
        throw std::invalid_argument("sofr_future_rate_from_price: price must be finite");
    }
    return (100.0 - price) / 100.0;
}

double sofr_future_price_from_rate(double rate) {
    if (!std::isfinite(rate)) {
        throw std::invalid_argument("sofr_future_price_from_rate: rate must be finite");
    }
    return 100.0 - 100.0 * rate;
}

double realized_accumulation(const std::vector<SofrFixing>& fixings, const QuantLib::Date& start,
                             const QuantLib::Date& end_exclusive,
                             const QuantLib::Calendar& fixing_calendar) {
    if (fixing_calendar.empty()) {
        throw std::invalid_argument("realized_accumulation: fixing calendar is empty");
    }
    if (start == QuantLib::Date() || end_exclusive == QuantLib::Date() || end_exclusive <= start) {
        throw std::invalid_argument(
            "realized_accumulation: require non-null start < end_exclusive");
    }
    if (!fixing_calendar.isBusinessDay(start)) {
        throw std::invalid_argument("realized_accumulation: start must be a fixing business day");
    }

    std::vector<QuantLib::Date> expected_dates;
    for (QuantLib::Date date = start; date < end_exclusive;
         date = fixing_calendar.advance(date, 1, QuantLib::Days)) {
        expected_dates.push_back(date);
    }
    if (fixings.size() != expected_dates.size()) {
        throw std::invalid_argument("realized_accumulation: missing or out-of-window fixing");
    }
    for (std::size_t i = 0; i < fixings.size(); ++i) {
        const SofrFixing& fixing = fixings[i];
        if (fixing.rate_date == QuantLib::Date() ||
            !fixing_calendar.isBusinessDay(fixing.rate_date)) {
            throw std::invalid_argument(
                "realized_accumulation: fixing date is null or not a business day");
        }
        if (fixing.rate_date != expected_dates[i]) {
            throw std::invalid_argument(
                "realized_accumulation: duplicate, missing, unordered, or out-of-window fixing");
        }
        if (!std::isfinite(fixing.rate)) {
            throw std::invalid_argument("realized_accumulation: fixing rate must be finite");
        }
    }

    double accumulation{1.0};
    for (const SofrFixing& fixing : fixings) {
        const QuantLib::Date next_rate_date =
            fixing_calendar.advance(fixing.rate_date, 1, QuantLib::Days);
        const QuantLib::Date coverage_end = std::min(next_rate_date, end_exclusive);
        const double delta = static_cast<double>(coverage_end - fixing.rate_date) / 360.0;
        accumulation *= 1.0 + fixing.rate * delta;
    }
    if (!std::isfinite(accumulation)) {
        throw std::runtime_error("realized_accumulation: accumulation became non-finite");
    }
    return accumulation;
}

}  // namespace irc
