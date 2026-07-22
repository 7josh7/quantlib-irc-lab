#include "curves/curve_io.hpp"

#include <ql/time/daycounters/actual365fixed.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <span>
#include <stdexcept>
#include <system_error>

namespace irc {
namespace {

void validate_output_curve(const PiecewiseLogLinearCurve& curve) {
    if (curve.day_counter() != QuantLib::Actual365Fixed()) {
        throw std::invalid_argument(
            "serialize_curve_csv: curve day counter must be Actual365Fixed");
    }
}

// std::to_chars is locale-independent, unlike printf or an ostream: the
// output must be byte-identical regardless of the caller's global locale.
// Precision 16 emits 17 significant digits. The CurveIo tests verify exact
// round-trip behavior on the target MSVC charconv implementation; this is not
// asserted as a guarantee across all standard-library implementations.
std::string format_scientific(double value) {
    std::array<char, 32> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                            std::chars_format::scientific, 16);
    if (error != std::errc{}) {
        throw std::runtime_error("serialize_curve_csv: value cannot be formatted");
    }
    return std::string(buffer.data(), end);
}

// YYYY-MM-DD, built digit by digit for the same locale-independence reason.
std::string format_iso_date(const QuantLib::Date& date) {
    const int year = date.year();
    const int month = static_cast<int>(date.month());
    const int day = date.dayOfMonth();
    std::string text;
    text.reserve(10);
    text.push_back(static_cast<char>('0' + (year / 1000) % 10));
    text.push_back(static_cast<char>('0' + (year / 100) % 10));
    text.push_back(static_cast<char>('0' + (year / 10) % 10));
    text.push_back(static_cast<char>('0' + year % 10));
    text.push_back('-');
    text.push_back(static_cast<char>('0' + (month / 10) % 10));
    text.push_back(static_cast<char>('0' + month % 10));
    text.push_back('-');
    text.push_back(static_cast<char>('0' + (day / 10) % 10));
    text.push_back(static_cast<char>('0' + day % 10));
    return text;
}

}  // namespace

std::string serialize_curve_csv(const PiecewiseLogLinearCurve& curve) {
    validate_output_curve(curve);

    const std::span<const CurveNode> nodes = curve.nodes();
    const QuantLib::Date reference = curve.reference_date();
    const QuantLib::DayCounter& day_counter = curve.day_counter();

    std::string csv = "date,t_act365f,discount,zero_cc,fwd_section\n";
    csv.reserve(csv.size() + nodes.size() * 96);

    double previous_time = 0.0;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const CurveNode& node = nodes[i];
        const double time = day_counter.yearFraction(reference, node.date);

        csv += format_iso_date(node.date);
        csv += ',';
        csv += format_scientific(time);
        csv += ',';
        csv += format_scientific(std::exp(node.log_discount));
        csv += ',';
        // The anchor sits at t = 0, where -log P / t is undefined; the column
        // is left empty rather than filled with a placeholder.
        if (time > 0.0) {
            csv += format_scientific(-node.log_discount / time);
        }
        csv += ',';
        // Constant instantaneous forward rate of the segment *ending* at this
        // node, so the anchor row has no segment behind it.
        if (i > 0) {
            const double segment = time - previous_time;
            csv += format_scientific(-(node.log_discount - nodes[i - 1].log_discount) / segment);
        }
        csv += '\n';

        previous_time = time;
    }
    return csv;
}

void write_curve_csv(const PiecewiseLogLinearCurve& curve, const std::filesystem::path& path) {
    const std::string csv = serialize_curve_csv(curve);

    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        throw std::runtime_error("write_curve_csv: parent directory does not exist: " +
                                 parent.string());
    }

    // Binary mode: text mode would translate '\n' to "\r\n" on Windows and
    // break the byte-for-byte round trip the serializer guarantees.
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("write_curve_csv: cannot open file for writing: " + path.string());
    }
    output.write(csv.data(), static_cast<std::streamsize>(csv.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("write_curve_csv: failed to write file: " + path.string());
    }
}

}  // namespace irc
