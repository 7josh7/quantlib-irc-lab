#pragma once

#include "curves/piecewise_log_linear_curve.hpp"

#include <filesystem>
#include <string>

namespace irc {

std::string serialize_curve_csv(const PiecewiseLogLinearCurve& curve);

void write_curve_csv(const PiecewiseLogLinearCurve& curve, const std::filesystem::path& path);

}  // namespace irc
