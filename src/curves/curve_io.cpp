#include "curves/curve_io.hpp"

#include <ql/time/daycounters/actual365fixed.hpp>

#include <stdexcept>

namespace irc {
namespace {

void validate_output_curve(const PiecewiseLogLinearCurve& curve) {
    if (curve.day_counter() != QuantLib::Actual365Fixed()) {
        throw std::invalid_argument(
            "serialize_curve_csv: curve day counter must be Actual365Fixed");
    }
}

}  // namespace

std::string serialize_curve_csv(const PiecewiseLogLinearCurve& curve) {
    validate_output_curve(curve);
    throw std::logic_error("serialize_curve_csv: not implemented (Phase 2 step 4)");
}

void write_curve_csv(const PiecewiseLogLinearCurve& curve, const std::filesystem::path& path) {
    (void)path;
    (void)serialize_curve_csv(curve);
    throw std::logic_error("write_curve_csv: not implemented (Phase 2 step 4)");
}

}  // namespace irc
