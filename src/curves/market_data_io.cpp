#include "curves/market_data_io.hpp"

#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/imm.hpp>
#include <ql/utilities/dataparsers.hpp>

#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct CsvRow {
    std::size_t line_number;
    std::vector<std::string> fields;
};

[[noreturn]] void fail(const std::filesystem::path& path, std::size_t line_number,
                       const std::string& reason) {
    std::string message = "load_sofr_market_data: " + path.string();
    if (line_number != 0) {
        message += ':' + std::to_string(line_number);
    }
    throw std::runtime_error(message + ": " + reason);
}

bool is_ascii_whitespace(char character) {
    return character == ' ' || character == '\t' || character == '\r' || character == '\n' ||
           character == '\f' || character == '\v';
}

std::string_view trim_ascii(std::string_view text) {
    while (!text.empty() && is_ascii_whitespace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_ascii_whitespace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

std::vector<std::string> split_csv_line(const std::filesystem::path& path, std::size_t line_number,
                                        std::string_view line) {
    if (line.find('"') != std::string_view::npos) {
        fail(path, line_number, "quoted CSV fields are not supported");
    }

    std::vector<std::string> fields;
    std::size_t field_start = 0;
    while (true) {
        const std::size_t comma = line.find(',', field_start);
        const std::size_t field_end = comma == std::string_view::npos ? line.size() : comma;
        fields.emplace_back(trim_ascii(line.substr(field_start, field_end - field_start)));
        if (comma == std::string_view::npos) {
            break;
        }
        field_start = comma + 1;
    }
    return fields;
}

std::vector<CsvRow> read_csv(const std::filesystem::path& path,
                             std::initializer_list<std::string_view> expected_header) {
    if (path.empty()) {
        fail(path, 0, "file path is empty");
    }

    std::ifstream input(path);
    if (!input) {
        fail(path, 0, "cannot open file");
    }

    const std::vector<std::string> header(expected_header.begin(), expected_header.end());
    std::vector<CsvRow> rows;
    bool header_seen = false;
    std::string physical_line;
    std::size_t line_number = 0;
    while (std::getline(input, physical_line)) {
        ++line_number;
        const std::string_view line = trim_ascii(physical_line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        std::vector<std::string> fields = split_csv_line(path, line_number, line);
        if (!header_seen) {
            if (fields != header) {
                fail(path, line_number, "header does not match the required schema");
            }
            header_seen = true;
            continue;
        }
        if (fields == header) {
            fail(path, line_number, "duplicate header");
        }
        if (fields.size() != header.size()) {
            fail(path, line_number,
                 "expected " + std::to_string(header.size()) + " fields, found " +
                     std::to_string(fields.size()));
        }
        rows.push_back({line_number, std::move(fields)});
    }

    if (input.bad()) {
        fail(path, line_number, "I/O error while reading file");
    }
    if (!header_seen) {
        fail(path, 0, "required header is missing");
    }
    return rows;
}

void require_empty(const std::filesystem::path& path, const CsvRow& row, std::size_t field_index,
                   std::string_view field_name) {
    if (!row.fields[field_index].empty()) {
        fail(path, row.line_number,
             "field '" + std::string(field_name) + "' must be empty for row type '" +
                 row.fields.front() + "'");
    }
}

void require_nonempty(const std::filesystem::path& path, const CsvRow& row, std::size_t field_index,
                      std::string_view field_name) {
    if (row.fields[field_index].empty()) {
        fail(path, row.line_number,
             "field '" + std::string(field_name) + "' must not be empty for row type '" +
                 row.fields.front() + "'");
    }
}

bool is_ascii_digit(char character) {
    return character >= '0' && character <= '9';
}

int parse_fixed_width_integer(std::string_view text, std::size_t start, std::size_t width) {
    int value = 0;
    for (std::size_t i = start; i < start + width; ++i) {
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

QuantLib::Date parse_date(const std::filesystem::path& path, const CsvRow& row,
                          std::size_t field_index, std::string_view field_name) {
    const std::string& text = row.fields[field_index];
    bool valid_shape = text.size() == 10 && text[4] == '-' && text[7] == '-';
    if (valid_shape) {
        for (std::size_t i = 0; i < text.size(); ++i) {
            const bool separator = i == 4 || i == 7;
            if (!separator && !is_ascii_digit(text[i])) {
                valid_shape = false;
                break;
            }
        }
    }
    if (!valid_shape) {
        fail(path, row.line_number, "field '" + std::string(field_name) + "' must use YYYY-MM-DD");
    }

    try {
        return QuantLib::DateParser::parseISO(text);
    } catch (const std::exception& error) {
        fail(path, row.line_number,
             "invalid field '" + std::string(field_name) + "': " + error.what());
    }
}

std::chrono::sys_seconds parse_utc_timestamp(const std::filesystem::path& path, const CsvRow& row,
                                             std::size_t field_index) {
    const std::string& text = row.fields[field_index];
    bool valid_shape = text.size() == 20 && text[4] == '-' && text[7] == '-' && text[10] == 'T' &&
                       text[13] == ':' && text[16] == ':' && text[19] == 'Z';
    if (valid_shape) {
        for (std::size_t i = 0; i < text.size(); ++i) {
            const bool separator = i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19;
            if (!separator && !is_ascii_digit(text[i])) {
                valid_shape = false;
                break;
            }
        }
    }
    if (!valid_shape) {
        fail(path, row.line_number,
             "field 'as_of_utc' must use second-precision UTC text YYYY-MM-DDTHH:MM:SSZ");
    }

    CsvRow date_row{row.line_number, {text.substr(0, 10)}};
    const QuantLib::Date date = parse_date(path, date_row, 0, "as_of_utc date");
    const int hour = parse_fixed_width_integer(text, 11, 2);
    const int minute = parse_fixed_width_integer(text, 14, 2);
    const int second = parse_fixed_width_integer(text, 17, 2);
    if (hour > 23 || minute > 59 || second > 59) {
        fail(path, row.line_number, "field 'as_of_utc' contains an invalid time of day");
    }

    const std::chrono::year_month_day calendar_date{
        std::chrono::year(date.year()), std::chrono::month(static_cast<unsigned>(date.month())),
        std::chrono::day(static_cast<unsigned>(date.dayOfMonth()))};
    return std::chrono::sys_days(calendar_date) + std::chrono::hours(hour) +
           std::chrono::minutes(minute) + std::chrono::seconds(second);
}

double parse_finite_double(const std::filesystem::path& path, const CsvRow& row,
                           std::size_t field_index, std::string_view field_name) {
    const std::string& text = row.fields[field_index];
    double value = 0.0;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto [position, error] = std::from_chars(begin, end, value, std::chars_format::general);
    if (error != std::errc{} || position != end || !std::isfinite(value)) {
        fail(path, row.line_number,
             "field '" + std::string(field_name) + "' must be a finite decimal number");
    }
    return value;
}

QuantLib::Period parse_positive_tenor(const std::filesystem::path& path, const CsvRow& row,
                                      std::size_t field_index) {
    QuantLib::Period tenor;
    try {
        tenor = QuantLib::PeriodParser::parse(row.fields[field_index]);
    } catch (const std::exception& error) {
        fail(path, row.line_number,
             "invalid OIS tenor '" + row.fields[field_index] + "': " + error.what());
    }
    if (tenor.length() <= 0) {
        fail(path, row.line_number, "OIS tenor must be positive");
    }
    return tenor;
}

std::vector<irc::SofrFixing> load_fixings(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }

    const std::vector<CsvRow> rows = read_csv(path, {"rate_date", "rate"});
    const QuantLib::Calendar fixing_calendar{QuantLib::UnitedStates(QuantLib::UnitedStates::SOFR)};
    std::vector<irc::SofrFixing> fixings;
    fixings.reserve(rows.size());
    std::optional<QuantLib::Date> previous_date;
    for (const CsvRow& row : rows) {
        require_nonempty(path, row, 0, "rate_date");
        require_nonempty(path, row, 1, "rate");
        const QuantLib::Date rate_date = parse_date(path, row, 0, "rate_date");
        const double rate = parse_finite_double(path, row, 1, "rate");
        if (!fixing_calendar.isBusinessDay(rate_date)) {
            fail(path, row.line_number,
                 "SOFR fixing date " + row.fields[0] + " is not a UnitedStates(SOFR) business day");
        }
        if (previous_date.has_value() && rate_date <= *previous_date) {
            fail(path, row.line_number, "SOFR fixing dates must be unique and strictly increasing");
        }
        fixings.push_back({rate_date, rate});
        previous_date = rate_date;
    }
    return fixings;
}

}  // namespace

namespace irc {

SofrMarketData load_sofr_market_data(const std::filesystem::path& quotes_csv,
                                     const std::filesystem::path& fixings_csv) {
    constexpr std::size_t kType = 0;
    constexpr std::size_t kId = 1;
    constexpr std::size_t kValuationDate = 2;
    constexpr std::size_t kAsOfUtc = 3;
    constexpr std::size_t kStart = 4;
    constexpr std::size_t kEnd = 5;
    constexpr std::size_t kQuote = 6;
    constexpr std::size_t kQuoteUnit = 7;

    const std::vector<CsvRow> rows = read_csv(
        quotes_csv,
        {"type", "id", "valuation_date", "as_of_utc", "start", "end", "quote", "quote_unit"});
    const QuantLib::Calendar settlement_calendar{
        QuantLib::UnitedStates(QuantLib::UnitedStates::Settlement)};

    SofrMarketData market;
    bool valuation_seen = false;
    std::unordered_set<std::string> instrument_ids;
    std::optional<QuantLib::Date> previous_future_end;
    std::optional<QuantLib::Period> previous_ois_tenor;

    for (const CsvRow& row : rows) {
        const std::string& type = row.fields[kType];
        if (type == "valuation") {
            if (valuation_seen) {
                fail(quotes_csv, row.line_number, "duplicate valuation row");
            }
            require_empty(quotes_csv, row, kId, "id");
            require_nonempty(quotes_csv, row, kValuationDate, "valuation_date");
            require_nonempty(quotes_csv, row, kAsOfUtc, "as_of_utc");
            require_empty(quotes_csv, row, kStart, "start");
            require_empty(quotes_csv, row, kEnd, "end");
            require_empty(quotes_csv, row, kQuote, "quote");
            require_empty(quotes_csv, row, kQuoteUnit, "quote_unit");

            market.as_of.valuation_date =
                parse_date(quotes_csv, row, kValuationDate, "valuation_date");
            market.as_of.as_of_utc = parse_utc_timestamp(quotes_csv, row, kAsOfUtc);
            if (!settlement_calendar.isBusinessDay(market.as_of.valuation_date)) {
                fail(quotes_csv, row.line_number,
                     "valuation_date must be a UnitedStates(Settlement) business day");
            }
            valuation_seen = true;
            continue;
        }

        if (type != "future" && type != "ois") {
            fail(quotes_csv, row.line_number, "unknown row type '" + type + "'");
        }
        require_nonempty(quotes_csv, row, kId, "id");
        if (!instrument_ids.insert(row.fields[kId]).second) {
            fail(quotes_csv, row.line_number, "duplicate instrument ID '" + row.fields[kId] + "'");
        }
        require_empty(quotes_csv, row, kValuationDate, "valuation_date");
        require_empty(quotes_csv, row, kAsOfUtc, "as_of_utc");
        require_nonempty(quotes_csv, row, kQuote, "quote");

        if (type == "future") {
            require_nonempty(quotes_csv, row, kStart, "start");
            require_nonempty(quotes_csv, row, kEnd, "end");
            if (row.fields[kQuoteUnit] != "imm_price") {
                fail(quotes_csv, row.line_number, "future quote_unit must be exactly 'imm_price'");
            }

            const QuantLib::Date start = parse_date(quotes_csv, row, kStart, "start");
            const QuantLib::Date end = parse_date(quotes_csv, row, kEnd, "end");
            const double price = parse_finite_double(quotes_csv, row, kQuote, "quote");
            if (!QuantLib::IMM::isIMMdate(start, true) || !QuantLib::IMM::isIMMdate(end, true)) {
                fail(quotes_csv, row.line_number,
                     "future start and end must be main-cycle IMM dates");
            }
            if (end <= start) {
                fail(quotes_csv, row.line_number, "future end must be after start");
            }
            if (previous_future_end.has_value() && start != *previous_future_end) {
                fail(quotes_csv, row.line_number,
                     "future reference periods must be contiguous and non-overlapping");
            }

            const double accrual = QuantLib::Actual360().yearFraction(start, end);
            const double rate = sofr_future_rate_from_price(price);
            const double denominator = 1.0 + accrual * rate;
            if (!(accrual > 0.0) || !std::isfinite(accrual) || !std::isfinite(denominator) ||
                denominator <= 0.0) {
                fail(quotes_csv, row.line_number,
                     "future quote violates the positive accrual-factor condition");
            }

            market.futures.push_back({row.fields[kId], start, end, price});
            previous_future_end = end;
            continue;
        }

        require_empty(quotes_csv, row, kStart, "start");
        require_empty(quotes_csv, row, kEnd, "end");
        if (row.fields[kQuoteUnit] != "decimal_rate") {
            fail(quotes_csv, row.line_number, "OIS quote_unit must be exactly 'decimal_rate'");
        }

        const QuantLib::Period tenor = parse_positive_tenor(quotes_csv, row, kId);
        const double par_rate = parse_finite_double(quotes_csv, row, kQuote, "quote");
        bool increasing = true;
        if (previous_ois_tenor.has_value()) {
            try {
                increasing = *previous_ois_tenor < tenor;
            } catch (const std::exception& error) {
                fail(quotes_csv, row.line_number,
                     "OIS tenor cannot be ordered after the preceding tenor: " +
                         std::string(error.what()));
            }
        }
        if (!increasing) {
            fail(quotes_csv, row.line_number, "OIS tenors must be unique and strictly increasing");
        }
        market.ois.push_back({row.fields[kId], tenor, par_rate});
        previous_ois_tenor = tenor;
    }

    if (!valuation_seen) {
        fail(quotes_csv, 0, "exactly one valuation row is required");
    }

    market.fixings = load_fixings(fixings_csv);
    const QuantLib::Calendar fixing_calendar{QuantLib::UnitedStates(QuantLib::UnitedStates::SOFR)};
    const SofrFutureQuote* partially_accrued = nullptr;
    for (const SofrFutureQuote& future : market.futures) {
        if (future.reference_start < market.as_of.valuation_date &&
            market.as_of.valuation_date < future.reference_end) {
            if (partially_accrued != nullptr) {
                fail(quotes_csv, 0, "more than one future straddles valuation_date");
            }
            partially_accrued = &future;
        }
    }

    if (partially_accrued != nullptr) {
        if (fixings_csv.empty()) {
            fail(quotes_csv, 0,
                 "future '" + partially_accrued->id +
                     "' is partially accrued, so an explicit fixings file is required");
        }
        try {
            (void)realized_accumulation(market.fixings, partially_accrued->reference_start,
                                        market.as_of.valuation_date, fixing_calendar);
        } catch (const std::exception& error) {
            fail(fixings_csv, 0,
                 "fixing coverage for partially accrued future '" + partially_accrued->id +
                     "' is invalid: " + error.what());
        }
    }

    return market;
}

}  // namespace irc
