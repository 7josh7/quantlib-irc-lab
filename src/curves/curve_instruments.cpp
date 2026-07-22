#include "curves/curve_instruments.hpp"

#include <algorithm>
#include <cmath>
#include <ql/time/daycounters/actual360.hpp>
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

double sofr_future_log_forward_discount(const SofrFutureQuote& quote) {
    if (quote.reference_start == QuantLib::Date() || quote.reference_end == QuantLib::Date()) {
        throw std::invalid_argument("sofr_future_log_forward_discount: future '" + quote.id +
                                    "' reference dates must not be null");
    }
    const QuantLib::DayCounter accrual_day_counter = QuantLib::Actual360();
    const double accrual =
        accrual_day_counter.yearFraction(quote.reference_start, quote.reference_end);
    const double growth = accrual * sofr_future_rate_from_price(quote.price);
    const double accumulation_factor = 1.0 + growth;
    if (!(accrual > 0.0) || !std::isfinite(accrual) || !std::isfinite(accumulation_factor) ||
        !(accumulation_factor > 0.0)) {
        throw std::invalid_argument("sofr_future_log_forward_discount: future '" + quote.id +
                                    "' requires positive accrual and finite 1 + tau*R > 0");
    }
    return -std::log1p(growth);
}

double sofr_future_model_rate(const SofrFutureQuote& quote, double start_value,
                              double end_discount) {
    if (quote.reference_start == QuantLib::Date() || quote.reference_end == QuantLib::Date()) {
        throw std::invalid_argument("sofr_future_model_rate: future '" + quote.id +
                                    "' reference dates must not be null");
    }
    const QuantLib::DayCounter accrual_day_counter = QuantLib::Actual360();
    const double accrual =
        accrual_day_counter.yearFraction(quote.reference_start, quote.reference_end);
    // !(x > 0.0) rather than x <= 0.0: every comparison against NaN is false,
    // so the second spelling would accept a NaN and divide by it below.
    if (!(accrual > 0.0)) {
        throw std::invalid_argument("sofr_future_model_rate: future '" + quote.id +
                                    "' must have a positive Act/360 accrual");
    }
    if (!std::isfinite(start_value) || !(start_value > 0.0)) {
        throw std::invalid_argument("sofr_future_model_rate: future '" + quote.id +
                                    "' start value must be finite and strictly positive");
    }
    if (!std::isfinite(end_discount) || !(end_discount > 0.0)) {
        throw std::invalid_argument("sofr_future_model_rate: future '" + quote.id +
                                    "' end discount must be finite and strictly positive");
    }
    const double rate = (start_value / end_discount - 1.0) / accrual;
    if (!std::isfinite(rate)) {
        throw std::invalid_argument("sofr_future_model_rate: future '" + quote.id +
                                    "' implied rate must be finite");
    }
    return rate;
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
