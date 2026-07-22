// Phase 2 curve-bootstrap tests are intentionally red until the owner
// implements the approved interfaces in docs/impl_notes/02_curve_bootstrap.md.

#include "core/bracketed_bisection.hpp"
#include "core/linear_interpolator.hpp"
#include "curves/curve_instruments.hpp"
#include "curves/curve_io.hpp"
#include "curves/market_data_io.hpp"
#include "curves/piecewise_log_linear_curve.hpp"
#include "curves/sofr_bootstrapper.hpp"
#include "rates/coupon_period.hpp"
#include "rates/fixed_leg.hpp"
#include "rates/floating_leg.hpp"
#include "rates/rate_accrual.hpp"
#include "rates/vanilla_swap.hpp"
#include "risk/dv01.hpp"

#include <gtest/gtest.h>
#include <ql/quantlib.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace ql = QuantLib;

constexpr double kNotional = 1'000'000.0;
constexpr double kTargetFixedRate = 0.035;

std::filesystem::path fixture_path(const char* filename) {
    return std::filesystem::path(IRC_TEST_DATA_DIR) / filename;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("test fixture cannot be opened: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::filesystem::path write_test_file(const std::string& filename, const std::string& contents) {
    const std::filesystem::path path = std::filesystem::path(::testing::TempDir()) / filename;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << contents;
    output.close();
    return path;
}

std::string replace_once(std::string text, const std::string& from, const std::string& to) {
    const std::size_t position = text.find(from);
    if (position == std::string::npos) {
        throw std::logic_error("test replacement source not found: " + from);
    }
    text.replace(position, from.size(), to);
    return text;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = line.find(',', start);
        const std::size_t end = comma == std::string::npos ? line.size() : comma;
        fields.push_back(line.substr(start, end - start));
        if (comma == std::string::npos) {
            return fields;
        }
        start = comma + 1;
    }
}

// Two pillars one and two years out, with log-discounts chosen so that every
// zero rate and segment forward is exactly 0.04.
irc::PiecewiseLogLinearCurve two_pillar_curve() {
    return {ql::Date(15, ql::January, 2026),
            {{ql::Date(15, ql::January, 2027), -0.04}, {ql::Date(15, ql::January, 2028), -0.08}},
            ql::Actual365Fixed()};
}

irc::PiecewiseLogLinearCurve known_curve() {
    const ql::Date reference(15, ql::January, 2026);
    return {reference,
            {{ql::Date(15, ql::January, 2027), std::log(0.96)},
             {ql::Date(15, ql::January, 2028), std::log(0.92)},
             {ql::Date(15, ql::January, 2029), std::log(0.88)}},
            ql::Actual365Fixed()};
}

irc::SofrMarketData load_pinned_market() {
    return irc::load_sofr_market_data(fixture_path("sofr_quotes_2026-01-15.csv"),
                                      fixture_path("sofr_fixings_2025-12-17_2026-01-14.csv"));
}

using QuantLibCurve = ql::PiecewiseYieldCurve<ql::Discount, ql::LogLinear>;

ql::ext::shared_ptr<QuantLibCurve> build_quantlib_curve(const irc::SofrMarketData& market) {
    ql::Settings::instance().evaluationDate() = market.as_of.valuation_date;
    ql::IndexManager::instance().clearHistories();

    const auto sofr = ql::ext::make_shared<ql::Sofr>();
    for (const irc::SofrFixing& fixing : market.fixings) {
        sofr->addFixing(fixing.rate_date, fixing.rate, true);
    }

    std::vector<ql::ext::shared_ptr<ql::RateHelper>> helpers;
    helpers.reserve(market.futures.size() + market.ois.size());
    for (const irc::SofrFutureQuote& future : market.futures) {
        helpers.push_back(ql::ext::make_shared<ql::SofrFutureRateHelper>(
            future.price, future.reference_start.month(), future.reference_start.year(),
            ql::Quarterly, 0.0, ql::Pillar::LastRelevantDate));
    }

    const ql::Calendar usny{ql::UnitedStates(ql::UnitedStates::Settlement)};
    for (const irc::OisQuote& ois : market.ois) {
        helpers.push_back(ql::ext::make_shared<ql::OISRateHelper>(
            2, ois.tenor, ois.par_rate, sofr, ql::Handle<ql::YieldTermStructure>(), false, 2,
            ql::ModifiedFollowing, ql::Annual, usny, 0 * ql::Days, 0.0,
            ql::Pillar::LastRelevantDate, ql::Date(), ql::RateAveraging::Compound, ql::ext::nullopt,
            ql::Annual, usny, ql::Null<ql::Natural>(), 0, false,
            ql::ext::shared_ptr<ql::FloatingRateCouponPricer>(), ql::DateGeneration::Backward, usny,
            ql::ModifiedFollowing));
    }

    auto curve = ql::ext::make_shared<QuantLibCurve>(market.as_of.valuation_date, helpers,
                                                     ql::Actual365Fixed(), 1e-12);
    (void)curve->discount(curve->maxDate());
    return curve;
}

ql::Schedule target_schedule(const ql::Date& valuation_date) {
    const ql::Calendar usny{ql::UnitedStates(ql::UnitedStates::Settlement)};
    const ql::Date spot = usny.advance(valuation_date, 2, ql::Days);
    const ql::Date maturity = usny.advance(spot, 10, ql::Years, ql::ModifiedFollowing);
    return {spot,
            maturity,
            ql::Period(ql::Annual),
            usny,
            ql::ModifiedFollowing,
            ql::ModifiedFollowing,
            ql::DateGeneration::Forward,
            false};
}

std::vector<irc::CouponPeriod> target_periods(const ql::Date& valuation_date) {
    return irc::make_coupon_periods(target_schedule(valuation_date), ql::Actual360(),
                                    ql::UnitedStates(ql::UnitedStates::Settlement), 2);
}

irc::CurvePricer target_pricer(const ql::Date& valuation_date, irc::SwapSide side) {
    const auto periods = target_periods(valuation_date);
    const auto accrual =
        std::make_shared<irc::CompoundedOvernightRate>(ql::UnitedStates(ql::UnitedStates::SOFR));
    return [periods, accrual, side](const irc::YieldCurve& curve) {
        const irc::FixedLeg fixed(periods, kNotional, kTargetFixedRate);
        const irc::FloatingLeg floating(periods, kNotional, accrual);
        return irc::VanillaSwap(side, fixed, floating).npv(curve);
    };
}

double quantlib_target_pv(const irc::SofrMarketData& market, ql::Swap::Type side) {
    const auto curve = build_quantlib_curve(market);
    const ql::Handle<ql::YieldTermStructure> handle(curve);
    const auto sofr = ql::ext::make_shared<ql::Sofr>(handle);
    const ql::Schedule schedule = target_schedule(market.as_of.valuation_date);
    ql::OvernightIndexedSwap swap(side, kNotional, schedule, kTargetFixedRate, ql::Actual360(),
                                  sofr, 0.0, 2, ql::ModifiedFollowing,
                                  ql::UnitedStates(ql::UnitedStates::Settlement));
    swap.setPricingEngine(ql::ext::make_shared<ql::DiscountingSwapEngine>(handle));
    return swap.NPV();
}

// --- A. Finance-free numerics -----------------------------------------------

TEST(LinearFlatInterpolatorTest, VectorizedRowMajorInterpolation) {
    const irc::LinearFlatInterpolator interpolator({1.0, 2.0, 3.0},
                                                   {10.0, 100.0, 20.0, 200.0, 30.0, 300.0});
    EXPECT_EQ(interpolator.outputs_per_knot(), 2);
    EXPECT_EQ(interpolator.evaluate(std::vector<double>{1.5, 2.5}),
              (std::vector<double>{15.0, 150.0, 25.0, 250.0}));
}

TEST(LinearFlatInterpolatorTest, FlatExtrapolation) {
    const irc::LinearFlatInterpolator interpolator({1.0, 2.0, 3.0},
                                                   {10.0, 100.0, 20.0, 200.0, 30.0, 300.0});
    EXPECT_EQ(interpolator.evaluate(std::vector<double>{0.5, 9.0}),
              (std::vector<double>{10.0, 100.0, 30.0, 300.0}));
}

TEST(LinearFlatInterpolatorTest, UnsortedQueriesPreserveInputOrder) {
    const irc::LinearFlatInterpolator interpolator({1.0, 2.0, 3.0}, {10.0, 20.0, 30.0});
    EXPECT_EQ(interpolator.evaluate(std::vector<double>{2.5, 1.5}),
              (std::vector<double>{25.0, 15.0}));
}

TEST(LinearFlatInterpolatorTest, RejectsMalformedOrNonFiniteInputs) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW((void)irc::LinearFlatInterpolator({}, {}), std::invalid_argument);
    EXPECT_THROW((void)irc::LinearFlatInterpolator({1.0, 1.0}, {1.0, 2.0}), std::invalid_argument);
    EXPECT_THROW((void)irc::LinearFlatInterpolator({2.0, 1.0}, {1.0, 2.0}), std::invalid_argument);
    EXPECT_THROW((void)irc::LinearFlatInterpolator({1.0, 2.0}, {1.0, 2.0, 3.0}),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::LinearFlatInterpolator({1.0, nan}, {1.0, 2.0}), std::invalid_argument);
    EXPECT_THROW((void)irc::LinearFlatInterpolator({1.0, 2.0}, {1.0, nan}), std::invalid_argument);

    const irc::LinearFlatInterpolator valid({1.0, 2.0}, {1.0, 2.0});
    EXPECT_THROW((void)valid.evaluate(std::vector<double>{nan}), std::invalid_argument);
}

TEST(LinearFlatInterpolatorTest, ScalarOutputsRoundTripKnots) {
    const irc::LinearFlatInterpolator interpolator({1.0, 2.0, 3.0}, {10.0, 20.0, 30.0});
    EXPECT_EQ(interpolator.evaluate(std::vector<double>{1.0, 2.0, 3.0}),
              (std::vector<double>{10.0, 20.0, 30.0}));
}

TEST(BracketedBisectionTest, CapturingLambdaFindsRootAndEndpointRoot) {
    const double target = 2.0;
    auto residual = [target](double x) { return x * x - target; };
    const irc::BisectionResult result = irc::bracketed_bisection(residual, 0.0, 2.0);
    EXPECT_NEAR(result.root, std::sqrt(target), 1e-12);
    EXPECT_LE(std::abs(result.residual), 1e-12);
    EXPECT_GT(result.iterations, 0);
    EXPECT_LE(result.iterations, 200);

    const irc::BisectionResult endpoint =
        irc::bracketed_bisection([](double x) { return x - 1.0; }, 1.0, 2.0);
    EXPECT_DOUBLE_EQ(endpoint.root, 1.0);
    EXPECT_EQ(endpoint.iterations, 0);
}

TEST(BracketedBisectionTest, EnforcesValidationAndFailureContract) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    auto identity = [](double x) { return x; };
    EXPECT_THROW((void)irc::bracketed_bisection(identity, nan, 1.0), std::invalid_argument);
    EXPECT_THROW((void)irc::bracketed_bisection(identity, 1.0, 1.0), std::invalid_argument);
    EXPECT_THROW((void)irc::bracketed_bisection(identity, -1.0, 1.0, {.residual_tolerance = 0.0}),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::bracketed_bisection(identity, -1.0, 1.0, {.residual_tolerance = inf}),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::bracketed_bisection(identity, -1.0, 1.0, {.max_iterations = 0}),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::bracketed_bisection([](double x) { return x * x + 1.0; }, -1.0, 1.0),
                 std::runtime_error);
    EXPECT_THROW((void)irc::bracketed_bisection([nan](double) { return nan; }, -1.0, 1.0),
                 std::runtime_error);
    EXPECT_THROW((void)irc::bracketed_bisection([](double x) { return x - 0.3; }, 0.0, 1.0,
                                                {.residual_tolerance = 1e-30, .max_iterations = 1}),
                 std::runtime_error);
}

// --- B. Curve from known nodes ----------------------------------------------

TEST(PiecewiseLogLinearCurveTest, PrependsOneAnchorAndReproducesNodeDiscounts) {
    const auto curve = known_curve();
    ASSERT_EQ(curve.nodes().size(), 4);
    EXPECT_EQ(curve.nodes()[0].date, curve.reference_date());
    EXPECT_DOUBLE_EQ(curve.nodes()[0].log_discount, 0.0);
    EXPECT_DOUBLE_EQ(curve.discount(curve.reference_date()), 1.0);
    EXPECT_NEAR(curve.discount(curve.nodes()[1].date), 0.96, 1e-15);
    EXPECT_THROW((void)irc::PiecewiseLogLinearCurve(
                     curve.reference_date(), {{curve.reference_date(), 0.0}}, ql::Actual365Fixed()),
                 std::invalid_argument);
}

TEST(PiecewiseLogLinearCurveTest, OffNodeDiscountUsesLogLinearFormula) {
    const auto curve = known_curve();
    const ql::Date midpoint(16, ql::July, 2027);
    const double t0 =
        curve.day_counter().yearFraction(curve.reference_date(), curve.nodes()[1].date);
    const double t1 =
        curve.day_counter().yearFraction(curve.reference_date(), curve.nodes()[2].date);
    const double t = curve.day_counter().yearFraction(curve.reference_date(), midpoint);
    const double weight = (t - t0) / (t1 - t0);
    const double expected = std::exp((1.0 - weight) * curve.nodes()[1].log_discount +
                                     weight * curve.nodes()[2].log_discount);
    EXPECT_NEAR(curve.discount(midpoint), expected, 1e-14);
}

TEST(PiecewiseLogLinearCurveTest, DiscountsRemainPositiveAndDecrease) {
    const auto curve = known_curve();
    double previous = curve.discount(curve.reference_date());
    for (ql::Date date = curve.reference_date() + 30; date <= curve.nodes().back().date;
         date += 30) {
        const double current = curve.discount(date);
        EXPECT_TRUE(std::isfinite(current));
        EXPECT_GT(current, 0.0);
        EXPECT_LT(current, previous);
        previous = current;
    }
}

TEST(PiecewiseLogLinearCurveTest, RejectsQueriesOutsideCalibratedDomain) {
    const auto curve = known_curve();
    EXPECT_THROW((void)curve.discount(curve.reference_date() - 1), std::invalid_argument);
    EXPECT_THROW((void)curve.discount(curve.nodes().back().date + 1), std::invalid_argument);
}

TEST(PiecewiseLogLinearCurveTest, BumpLeavesAnchorAndNonAdjacentRegionUnchanged) {
    const auto curve = known_curve();
    EXPECT_THROW((void)irc::bump_node(curve, 0, 1e-6), std::invalid_argument);
    const auto bumped = irc::bump_node(curve, 1, 1e-6);
    EXPECT_DOUBLE_EQ(bumped.nodes()[0].log_discount, curve.nodes()[0].log_discount);
    EXPECT_NE(bumped.nodes()[1].log_discount, curve.nodes()[1].log_discount);
    EXPECT_DOUBLE_EQ(bumped.nodes()[2].log_discount, curve.nodes()[2].log_discount);
    EXPECT_NE(bumped.discount(curve.reference_date() + 100),
              curve.discount(curve.reference_date() + 100));
    EXPECT_DOUBLE_EQ(bumped.discount(curve.nodes()[2].date), curve.discount(curve.nodes()[2].date));
}

// --- C. Fixings and realized accumulation ----------------------------------

TEST(RealizedAccumulationTest, MatchesCalendarDayWeightedGolden) {
    const ql::Calendar calendar{ql::UnitedStates(ql::UnitedStates::SOFR)};
    const ql::Date thursday(8, ql::January, 2026);
    const ql::Date friday(9, ql::January, 2026);
    const ql::Date monday(12, ql::January, 2026);
    const std::vector<irc::SofrFixing> fixings{{thursday, 0.0400}, {friday, 0.0410}};
    const double expected = (1.0 + 0.0400 / 360.0) * (1.0 + 3.0 * 0.0410 / 360.0);
    EXPECT_NEAR(irc::realized_accumulation(fixings, thursday, monday, calendar), expected, 1e-12);
}

TEST(RealizedAccumulationTest, RejectsInvalidCoverageAndFixings) {
    const ql::Calendar calendar{ql::UnitedStates(ql::UnitedStates::SOFR)};
    const ql::Date thursday(8, ql::January, 2026);
    const ql::Date friday(9, ql::January, 2026);
    const ql::Date monday(12, ql::January, 2026);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<irc::SofrFixing> valid{{thursday, 0.0400}, {friday, 0.0410}};

    EXPECT_EQ(monday - friday, 3);
    EXPECT_THROW((void)irc::realized_accumulation({{thursday, 0.0400}}, thursday, monday, calendar),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::realized_accumulation({{thursday, 0.0400}, {thursday, 0.0410}},
                                                  thursday, monday, calendar),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::realized_accumulation({{thursday - 1, 0.0400}, {friday, 0.0410}},
                                                  thursday, monday, calendar),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::realized_accumulation({{thursday, 0.0400}, {friday + 1, 0.0410}},
                                                  thursday, monday, calendar),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::realized_accumulation({{thursday, nan}, {friday, 0.0410}}, thursday,
                                                  monday, calendar),
                 std::invalid_argument);
    (void)valid;
}

TEST(RealizedAccumulationTest, PinnedSr3Z25BoundaryUsesTwentyNineCalendarDays) {
    const auto market = load_pinned_market();
    ASSERT_FALSE(market.futures.empty());
    EXPECT_EQ(market.futures.front().reference_start, ql::Date(17, ql::December, 2025));
    EXPECT_EQ(market.futures.front().reference_end, ql::Date(18, ql::March, 2026));
    EXPECT_EQ(market.futures.front().reference_end - market.futures.front().reference_start, 91);
    EXPECT_EQ(market.as_of.valuation_date - market.futures.front().reference_start, 29);
    ASSERT_EQ(market.fixings.size(), 19);
    EXPECT_EQ(market.fixings.back().rate_date, ql::Date(14, ql::January, 2026));
    EXPECT_TRUE(std::none_of(market.fixings.begin(), market.fixings.end(),
                             [&](const irc::SofrFixing& fixing) {
                                 return fixing.rate_date == market.as_of.valuation_date;
                             }));

    const ql::Calendar calendar{ql::UnitedStates(ql::UnitedStates::SOFR)};
    int total_days = 0;
    for (const auto& fixing : market.fixings) {
        total_days +=
            std::min(calendar.advance(fixing.rate_date, 1, ql::Days), market.as_of.valuation_date) -
            fixing.rate_date;
    }
    EXPECT_EQ(total_days, 29);
}

// --- D. Market data and bootstrap ------------------------------------------

TEST(CurveInstrumentTest, FuturesPriceRateConversionRoundTrips) {
    EXPECT_DOUBLE_EQ(irc::sofr_future_rate_from_price(95.75), 0.0425);
    EXPECT_DOUBLE_EQ(irc::sofr_future_price_from_rate(0.0425), 95.75);
    const double bumped_price = irc::sofr_future_price_from_rate(0.0426);
    EXPECT_NEAR(bumped_price - 95.75, -0.01, 1e-14);
}

TEST(MarketDataIoTest, LoadsPinnedSnapshotAndRejectsMalformedCopies) {
    const auto market = load_pinned_market();
    EXPECT_EQ(market.futures.size(), 13);
    EXPECT_EQ(market.ois.size(), 9);
    EXPECT_EQ(market.fixings.size(), 19);
    EXPECT_EQ(market.as_of.valuation_date, ql::Date(15, ql::January, 2026));
    const auto expected_timestamp =
        std::chrono::sys_days(std::chrono::year(2026) / std::chrono::January / 15) +
        std::chrono::hours(20);
    EXPECT_EQ(market.as_of.as_of_utc, expected_timestamp);

    const std::string source = read_text(fixture_path("sofr_quotes_2026-01-15.csv"));
    const auto fixings = fixture_path("sofr_fixings_2025-12-17_2026-01-14.csv");
    const std::vector<std::string> malformed{
        replace_once(source, "valuation,,2026-01-15,2026-01-15T20:00:00Z,,,,",
                     "valuation,,2026-01-15,,,,,"),
        replace_once(source, "valuation,,2026-01-15,2026-01-15T20:00:00Z,,,,",
                     "valuation,,2026-01-15,2026-01-15T20:00:00,,,,"),
        replace_once(source, "future,SR3Z25", "unknown,SR3Z25"),
        replace_once(source, "95.75,imm_price", "95.75,decimal_rate"),
        replace_once(source, "future,SR3Z25,,,", "future,SR3Z25,2026-01-15,,"),
        replace_once(source, "95.75,imm_price", "NaN,imm_price"),
        replace_once(source, "future,SR3H26", "future,SR3Z25"),
        replace_once(source, "future,SR3H26,,,2026-03-18,2026-06-17",
                     "future,SR3H26,,,2026-03-17,2026-06-17"),
        source + "ois,04Y,,,,,0.0375,decimal_rate\n"};
    for (std::size_t i = 0; i < malformed.size(); ++i) {
        const auto path = write_test_file("bad_quotes_" + std::to_string(i) + ".csv", malformed[i]);
        EXPECT_THROW((void)irc::load_sofr_market_data(path, fixings), std::runtime_error)
            << "malformed fixture index " << i;
    }

    const auto negative_ois = write_test_file(
        "negative_ois.csv", replace_once(source, "0.0375,decimal_rate", "-0.0010,decimal_rate"));
    EXPECT_NO_THROW((void)irc::load_sofr_market_data(negative_ois, fixings));

    EXPECT_THROW((void)irc::load_sofr_market_data(fixture_path("sofr_quotes_2026-01-15.csv"),
                                                  std::filesystem::path{}),
                 std::runtime_error);
    const auto fully_forward =
        write_test_file("fully_forward_quotes.csv",
                        replace_once(source, "valuation,,2026-01-15,2026-01-15T20:00:00Z,,,,",
                                     "valuation,,2025-12-16,2025-12-16T20:00:00Z,,,,"));
    const auto fully_forward_market =
        irc::load_sofr_market_data(fully_forward, std::filesystem::path{});
    EXPECT_TRUE(fully_forward_market.fixings.empty());

    const ql::Calendar usny{ql::UnitedStates(ql::UnitedStates::Settlement)};
    const ql::Calendar usgs{ql::UnitedStates(ql::UnitedStates::SOFR)};
    const ql::Date good_friday(3, ql::April, 2026);
    EXPECT_TRUE(usny.isBusinessDay(good_friday));
    EXPECT_FALSE(usgs.isBusinessDay(good_friday));
}

// The malformed-copy test proves the loader rejects bad input. This one
// proves it puts good input in the right slots, which nothing else in the
// suite can: the QuantLib benchmark builds its rate helpers from the same
// SofrMarketData, so a transposed field or a dropped row would corrupt both
// sides of that comparison identically and still agree.
TEST(MarketDataIoTest, ParsesPinnedSnapshotIntoTheRightFields) {
    const auto market = load_pinned_market();

    ASSERT_EQ(market.futures.size(), 13);
    EXPECT_EQ(market.futures.front().id, "SR3Z25");
    EXPECT_EQ(market.futures.front().reference_start, ql::Date(17, ql::December, 2025));
    EXPECT_EQ(market.futures.front().reference_end, ql::Date(18, ql::March, 2026));
    EXPECT_DOUBLE_EQ(market.futures.front().price, 95.75);
    EXPECT_EQ(market.futures[6].id, "SR3M27");
    EXPECT_DOUBLE_EQ(market.futures[6].price, 96.03);
    EXPECT_EQ(market.futures.back().id, "SR3Z28");
    EXPECT_EQ(market.futures.back().reference_end, ql::Date(21, ql::March, 2029));
    EXPECT_DOUBLE_EQ(market.futures.back().price, 96.20);

    ASSERT_EQ(market.ois.size(), 9);
    EXPECT_EQ(market.ois.front().id, "4Y");
    EXPECT_EQ(market.ois.front().tenor, ql::Period(4, ql::Years));
    EXPECT_DOUBLE_EQ(market.ois.front().par_rate, 0.0375);
    EXPECT_EQ(market.ois[4].tenor, ql::Period(10, ql::Years));
    EXPECT_DOUBLE_EQ(market.ois[4].par_rate, 0.0350);
    EXPECT_EQ(market.ois.back().tenor, ql::Period(30, ql::Years));
    EXPECT_DOUBLE_EQ(market.ois.back().par_rate, 0.0330);

    ASSERT_EQ(market.fixings.size(), 19);
    EXPECT_EQ(market.fixings.front().rate_date, ql::Date(17, ql::December, 2025));
    EXPECT_DOUBLE_EQ(market.fixings.front().rate, 0.0369);
    EXPECT_EQ(market.fixings.back().rate_date, ql::Date(14, ql::January, 2026));
    EXPECT_DOUBLE_EQ(market.fixings.back().rate, 0.0364);

    // Christmas and New Year are absent from the published series. Pinning the
    // rows on either side of each gap catches a dropped or reordered fixing,
    // which a size check alone cannot: losing one row and gaining another
    // leaves the count at 19.
    EXPECT_EQ(market.fixings[5].rate_date, ql::Date(24, ql::December, 2025));
    EXPECT_EQ(market.fixings[6].rate_date, ql::Date(26, ql::December, 2025));
    EXPECT_DOUBLE_EQ(market.fixings[6].rate, 0.0376);
    EXPECT_EQ(market.fixings[9].rate_date, ql::Date(31, ql::December, 2025));
    EXPECT_EQ(market.fixings[10].rate_date, ql::Date(2, ql::January, 2026));
    EXPECT_DOUBLE_EQ(market.fixings[10].rate, 0.0375);
}

TEST(SofrBootstrapperTest, FirstPartiallyAccruedPillarMatchesGoldenAndInverse) {
    auto market = load_pinned_market();
    market.ois.clear();
    market.futures.resize(1);
    market.futures[0].price = 95.70;
    for (irc::SofrFixing& fixing : market.fixings) {
        fixing.rate = 0.0;
        if (fixing.rate_date == ql::Date(8, ql::January, 2026)) {
            fixing.rate = 0.0400;
        } else if (fixing.rate_date == ql::Date(9, ql::January, 2026)) {
            fixing.rate = 0.0410;
        }
    }

    const irc::BootstrapResult result = irc::SofrCurveBootstrapper().bootstrap(market);
    ASSERT_EQ(result.curve.nodes().size(), 2);
    constexpr double accumulation = 1.00045281574074;
    constexpr double tau = 91.0 / 360.0;
    constexpr double rate = 0.0430;
    constexpr double expected_discount = accumulation / (1.0 + tau * rate);
    EXPECT_NEAR(std::exp(result.curve.nodes()[1].log_discount), expected_discount, 1e-12);
    const auto model = irc::SofrCurveBootstrapper().model_quotes(market, result.curve);
    ASSERT_EQ(model.size(), 1);
    EXPECT_NEAR(model[0], rate, 1e-12);
}

TEST(SofrBootstrapperTest, FuturesSubcurveMatchesTelescopedHandCalculation) {
    auto market = load_pinned_market();
    market.ois.clear();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto result = bootstrapper.bootstrap(market);
    ASSERT_EQ(result.curve.nodes().size(), market.futures.size() + 1);

    const ql::Calendar fixing_calendar{ql::UnitedStates(ql::UnitedStates::SOFR)};
    const double accumulation =
        irc::realized_accumulation(market.fixings, market.futures.front().reference_start,
                                   market.as_of.valuation_date, fixing_calendar);
    double expected_discount =
        accumulation / (1.0 + ql::Actual360().yearFraction(market.futures[0].reference_start,
                                                           market.futures[0].reference_end) *
                                  irc::sofr_future_rate_from_price(market.futures[0].price));
    EXPECT_NEAR(std::exp(result.curve.nodes()[1].log_discount), expected_discount, 1e-12);
    for (std::size_t i = 1; i < market.futures.size(); ++i) {
        const double tau = ql::Actual360().yearFraction(market.futures[i].reference_start,
                                                        market.futures[i].reference_end);
        expected_discount /= 1.0 + tau * irc::sofr_future_rate_from_price(market.futures[i].price);
        EXPECT_NEAR(std::exp(result.curve.nodes()[i + 1].log_discount), expected_discount, 1e-12);
    }
}

TEST(SofrBootstrapperTest, FullCurveRepricesAllInputsAndPreservesSnapshot) {
    const auto market = load_pinned_market();
    const auto result = irc::SofrCurveBootstrapper().bootstrap(market);
    EXPECT_EQ(result.as_of, market.as_of);
    ASSERT_EQ(result.diagnostics.size(), 22);
    ASSERT_EQ(result.curve.nodes().size(), 23);
    for (std::size_t i = 0; i < result.diagnostics.size(); ++i) {
        const std::string& expected_id = i < market.futures.size()
                                             ? market.futures[i].id
                                             : market.ois[i - market.futures.size()].id;
        EXPECT_EQ(result.diagnostics[i].instrument_id, expected_id);
        EXPECT_LE(std::abs(result.diagnostics[i].residual), 1e-10);
    }
    for (std::size_t i = 1; i < result.curve.nodes().size(); ++i) {
        EXPECT_LT(result.curve.nodes()[i - 1].date, result.curve.nodes()[i].date);
        EXPECT_TRUE(std::isfinite(result.curve.nodes()[i].log_discount));
        EXPECT_GT(std::exp(result.curve.nodes()[i].log_discount), 0.0);
    }
}

TEST(SofrBootstrapperTest, RejectsInvalidMarketsWithContext) {
    const auto pinned = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;

    auto empty = pinned;
    empty.futures.clear();
    EXPECT_THROW((void)bootstrapper.bootstrap(empty), std::invalid_argument);

    auto bad_date = pinned;
    bad_date.as_of.valuation_date = ql::Date(17, ql::January, 2026);
    EXPECT_THROW((void)bootstrapper.bootstrap(bad_date), std::invalid_argument);

    auto non_imm = pinned;
    non_imm.futures[1].reference_start += 1;
    EXPECT_THROW((void)bootstrapper.bootstrap(non_imm), std::invalid_argument);

    auto missing_fixing = pinned;
    missing_fixing.fixings.erase(missing_fixing.fixings.begin() + 2);
    EXPECT_THROW((void)bootstrapper.bootstrap(missing_fixing), std::invalid_argument);

    auto settled = pinned;
    settled.as_of.valuation_date = settled.futures.front().reference_end;
    EXPECT_THROW((void)bootstrapper.bootstrap(settled), std::invalid_argument);

    auto gap = pinned;
    gap.futures.erase(gap.futures.begin() + 1);
    EXPECT_THROW((void)bootstrapper.bootstrap(gap), std::invalid_argument);

    auto non_finite = pinned;
    non_finite.ois.front().par_rate = std::numeric_limits<double>::infinity();
    EXPECT_THROW((void)bootstrapper.bootstrap(non_finite), std::invalid_argument);

    auto impossible_future = pinned;
    impossible_future.futures.front().price = 300.0;
    EXPECT_THROW((void)bootstrapper.bootstrap(impossible_future), std::invalid_argument);

    auto pathological = pinned;
    pathological.ois.back().par_rate = 5.0;
    EXPECT_THROW((void)bootstrapper.bootstrap(pathological), std::runtime_error);
}

// --- E. Deterministic output ------------------------------------------------

// The serializer is tested in two halves, because its output mixes values
// that IEEE-754 pins exactly with values that it does not.
//
// The format — column names and order, precision, the empty-column rule, line
// endings, date shape — is fully determined, so it is compared as bytes.
//
// The discount column is exp(x). std::exp is not required by the C++ standard
// or IEEE-754 to be correctly rounded, so two conforming implementations may
// return doubles one ulp apart and print different 17th digits. Byte-comparing
// that column would pin the test to one platform's libm rather than to the
// serializer, so the numbers are checked against their mathematical values
// with a tolerance instead. (QuantLib's own curve tests do the same: see
// test-suite/piecewiseyieldcurve.cpp, `Real tolerance = 1.0e-9`.)

TEST(CurveIoTest, SerializerFormatMatchesGoldenBytes) {
    const std::string csv = irc::serialize_curve_csv(two_pillar_curve());

    // Byte-exact through the anchor row: t = 0 and exp(0) = 1 are both exact
    // in IEEE-754, so no libm result reaches these bytes.
    const std::string expected_prefix =
        "date,t_act365f,discount,zero_cc,fwd_section\n"
        "2026-01-15,0.0000000000000000e+00,1.0000000000000000e+00,,\n";
    EXPECT_EQ(csv.substr(0, std::min(csv.size(), expected_prefix.size())), expected_prefix);
    EXPECT_TRUE(csv.ends_with("\n"));

    const std::vector<std::string> lines = split_lines(csv);
    ASSERT_EQ(lines.size(), 4);
    EXPECT_EQ(lines[0], "date,t_act365f,discount,zero_cc,fwd_section");

    for (std::size_t row = 1; row < lines.size(); ++row) {
        const std::vector<std::string> fields = split_fields(lines[row]);
        ASSERT_EQ(fields.size(), 5) << "row " << row;
        EXPECT_EQ(fields[0].size(), 10) << "row " << row;
        EXPECT_EQ(fields[0][4], '-') << "row " << row;
        EXPECT_EQ(fields[0][7], '-') << "row " << row;
        for (std::size_t field = 1; field < fields.size(); ++field) {
            if (fields[field].empty()) {
                continue;
            }
            // d.dddddddddddddddde+dd — one mantissa digit, 16 fraction digits
            // (max_digits10 - 1), and a two-digit exponent. All values in this
            // fixture are non-negative, so there is no sign character.
            EXPECT_EQ(fields[field].size(), 22) << "row " << row << " field " << field;
            EXPECT_EQ(fields[field].find('e'), 18U) << "row " << row << " field " << field;
        }
    }

    // Only the anchor row leaves zero_cc and fwd_section empty: -log(P)/t is
    // undefined at t = 0, and no segment ends at the first node.
    EXPECT_EQ(split_fields(lines[1])[0], "2026-01-15");
    EXPECT_TRUE(split_fields(lines[1])[3].empty());
    EXPECT_TRUE(split_fields(lines[1])[4].empty());
    EXPECT_EQ(split_fields(lines[2])[0], "2027-01-15");
    EXPECT_FALSE(split_fields(lines[2])[3].empty());
    EXPECT_FALSE(split_fields(lines[2])[4].empty());
    EXPECT_EQ(split_fields(lines[3])[0], "2028-01-15");

    const irc::PiecewiseLogLinearCurve wrong_dc(ql::Date(15, ql::January, 2026),
                                                {{ql::Date(15, ql::January, 2027), -0.04}},
                                                ql::Actual360());
    EXPECT_THROW((void)irc::serialize_curve_csv(wrong_dc), std::invalid_argument);
}

TEST(CurveIoTest, SerializedValuesMatchTheClosedForm) {
    const std::vector<std::string> lines =
        split_lines(irc::serialize_curve_csv(two_pillar_curve()));
    ASSERT_EQ(lines.size(), 4);

    // Expected discounts are the mathematical values of exp(-0.04) and
    // exp(-0.08) to 21 digits, computed independently of this code. The
    // tolerance is ~10 ulp at this magnitude: far tighter than any real
    // formula error, far looser than the last-bit freedom std::exp has.
    struct ExpectedRow {
        double t;
        double discount;
        double zero_cc;
        double fwd_section;
    };
    const std::vector<ExpectedRow> expected{
        {0.0, 1.0, 0.0, 0.0},
        {1.0, 0.960789439152323209439, 0.04, 0.04},
        {2.0, 0.923116346386635782911, 0.04, 0.04},
    };
    constexpr double kTolerance = 1e-15;

    for (std::size_t row = 0; row < expected.size(); ++row) {
        const std::vector<std::string> fields = split_fields(lines[row + 1]);
        ASSERT_EQ(fields.size(), 5) << "row " << row;
        EXPECT_NEAR(std::stod(fields[1]), expected[row].t, kTolerance) << "t, row " << row;
        EXPECT_NEAR(std::stod(fields[2]), expected[row].discount, kTolerance)
            << "discount, row " << row;
        if (row == 0) {
            continue;  // the anchor reports neither a zero rate nor a segment
        }
        EXPECT_NEAR(std::stod(fields[3]), expected[row].zero_cc, kTolerance)
            << "zero_cc, row " << row;
        EXPECT_NEAR(std::stod(fields[4]), expected[row].fwd_section, kTolerance)
            << "fwd_section, row " << row;
    }
}

TEST(CurveIoTest, WriterRoundTripsBytesAndRejectsMissingParent) {
    const auto curve = known_curve();
    const std::string expected = irc::serialize_curve_csv(curve);
    const auto path = std::filesystem::path(::testing::TempDir()) / "phase2_curve.csv";
    irc::write_curve_csv(curve, path);
    EXPECT_EQ(read_text(path), expected);

    const auto missing_parent =
        std::filesystem::path(::testing::TempDir()) / "missing_parent" / "curve.csv";
    EXPECT_THROW(irc::write_curve_csv(curve, missing_parent), std::runtime_error);
}

// --- F. QuantLib oracle -----------------------------------------------------

TEST(QuantLibCurveOracleTest, PillarsAndOffPillarDiscountsAgree) {
    const ql::SavedSettings settings_guard;
    const auto market = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto ours = bootstrapper.bootstrap(market);
    const auto quantlib = build_quantlib_curve(market);

    const auto model_quotes = bootstrapper.model_quotes(market, ours.curve);
    ASSERT_EQ(model_quotes.size(), market.futures.size() + market.ois.size());
    EXPECT_NEAR(model_quotes.front(), irc::sofr_future_rate_from_price(market.futures[0].price),
                1e-10);
    for (std::size_t i = 1; i < ours.curve.nodes().size(); ++i) {
        const ql::Date date = ours.curve.nodes()[i].date;
        EXPECT_NEAR(ours.curve.discount(date), quantlib->discount(date), 1e-8) << "pillar " << i;
    }
    for (const auto& future : market.futures) {
        EXPECT_NEAR(ours.curve.discount(future.reference_end),
                    quantlib->discount(future.reference_end), 1e-8);
    }
}

TEST(QuantLibCurveOracleTest, TenYearPaymentLagSwapPvAndParRateAgree) {
    const ql::SavedSettings settings_guard;
    const auto market = load_pinned_market();
    const auto ours = irc::SofrCurveBootstrapper().bootstrap(market);
    const auto periods = target_periods(market.as_of.valuation_date);
    const ql::Calendar usny{ql::UnitedStates(ql::UnitedStates::Settlement)};
    const auto schedule = target_schedule(market.as_of.valuation_date);
    ASSERT_EQ(periods.size() + 1, schedule.size());
    for (std::size_t i = 0; i < periods.size(); ++i) {
        EXPECT_EQ(periods[i].payment_date, usny.advance(periods[i].accrual_end, 2, ql::Days));
    }

    const auto accrual =
        std::make_shared<irc::CompoundedOvernightRate>(ql::UnitedStates(ql::UnitedStates::SOFR));
    const irc::VanillaSwap our_swap(irc::SwapSide::Payer,
                                    irc::FixedLeg(periods, kNotional, kTargetFixedRate),
                                    irc::FloatingLeg(periods, kNotional, accrual));

    const auto ql_curve = build_quantlib_curve(market);
    const ql::Handle<ql::YieldTermStructure> handle(ql_curve);
    const auto sofr = ql::ext::make_shared<ql::Sofr>(handle);
    ql::OvernightIndexedSwap ql_swap(ql::OvernightIndexedSwap::Payer, kNotional, schedule,
                                     kTargetFixedRate, ql::Actual360(), sofr, 0.0, 2,
                                     ql::ModifiedFollowing, usny);
    ql_swap.setPricingEngine(ql::ext::make_shared<ql::DiscountingSwapEngine>(handle));

    EXPECT_NEAR(our_swap.npv(ours.curve), ql_swap.NPV(), 1e-6);
    EXPECT_NEAR(our_swap.fair_rate(ours.curve), ql_swap.fairRate(), 1e-8);
}

// --- G. Required direct quote DV01 -----------------------------------------

TEST(QuoteDv01Test, PayerIsPositiveAndReceiverIsItsNegative) {
    const auto market = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto payer = irc::quote_dv01_direct(
        market, bootstrapper, target_pricer(market.as_of.valuation_date, irc::SwapSide::Payer));
    const auto receiver = irc::quote_dv01_direct(
        market, bootstrapper, target_pricer(market.as_of.valuation_date, irc::SwapSide::Receiver));
    ASSERT_EQ(payer.size(), 22);
    ASSERT_EQ(receiver.size(), payer.size());
    const double payer_total = std::accumulate(payer.begin(), payer.end(), 0.0);
    const double receiver_total = std::accumulate(receiver.begin(), receiver.end(), 0.0);
    EXPECT_GT(payer_total, 0.0);
    EXPECT_NEAR(receiver_total, -payer_total, 1e-8);
}

TEST(QuoteDv01Test, TotalMagnitudeIsConsistentWithAnnuity) {
    const auto market = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto result = bootstrapper.bootstrap(market);
    const auto periods = target_periods(market.as_of.valuation_date);
    const irc::FixedLeg fixed(periods, kNotional, kTargetFixedRate);
    const double annuity_dv01 = fixed.annuity(result.curve) * 1e-4;
    const auto dv01 = irc::quote_dv01_direct(
        market, bootstrapper, target_pricer(market.as_of.valuation_date, irc::SwapSide::Payer));
    const double total = std::accumulate(dv01.begin(), dv01.end(), 0.0);
    EXPECT_NEAR(total, annuity_dv01, 0.30 * annuity_dv01);
}

TEST(QuoteDv01Test, CentralHalfWidthsAgreeAndInvalidWidthsThrow) {
    const auto market = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto pv = target_pricer(market.as_of.valuation_date, irc::SwapSide::Payer);
    const auto wide = irc::quote_dv01_direct(market, bootstrapper, pv, 1e-4);
    const auto narrow = irc::quote_dv01_direct(market, bootstrapper, pv, 0.5e-4);
    const double wide_total = std::accumulate(wide.begin(), wide.end(), 0.0);
    const double narrow_total = std::accumulate(narrow.begin(), narrow.end(), 0.0);
    EXPECT_NEAR(wide_total, narrow_total, 1e-3 * std::abs(wide_total));

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_THROW((void)irc::quote_dv01_direct(market, bootstrapper, pv, 0.0),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_direct(market, bootstrapper, pv, -1e-4),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_direct(market, bootstrapper, pv, nan),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_direct(market, bootstrapper, pv, inf),
                 std::invalid_argument);
}

TEST(QuoteDv01Test, FuturesBumpUsesRateOrientationAndLeavesFixingsImmutable) {
    const auto market = load_pinned_market();
    const auto original_fixings = market.fixings;
    const auto original_as_of = market.as_of;
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto pv = target_pricer(market.as_of.valuation_date, irc::SwapSide::Payer);
    constexpr double h = 1e-4;

    auto up = market;
    auto down = market;
    up.futures[0].price -= 100.0 * h;
    down.futures[0].price += 100.0 * h;
    const double pv_up = pv(bootstrapper.bootstrap(up).curve);
    const double pv_down = pv(bootstrapper.bootstrap(down).curve);
    const double expected = (pv_up - pv_down) / (2.0 * h) * 1e-4;

    const auto direct = irc::quote_dv01_direct(market, bootstrapper, pv, h);
    ASSERT_FALSE(direct.empty());
    EXPECT_NEAR(direct[0], expected, 1e-9);
    EXPECT_EQ(market.as_of, original_as_of);
    ASSERT_EQ(market.fixings.size(), original_fixings.size());
    for (std::size_t i = 0; i < market.fixings.size(); ++i) {
        EXPECT_EQ(market.fixings[i].rate_date, original_fixings[i].rate_date);
        EXPECT_DOUBLE_EQ(market.fixings[i].rate, original_fixings[i].rate);
    }
}

TEST(QuoteDv01Test, TotalAgreesWithFullQuantLibRebuilds) {
    const ql::SavedSettings settings_guard;
    const auto market = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto ours = irc::quote_dv01_direct(
        market, bootstrapper, target_pricer(market.as_of.valuation_date, irc::SwapSide::Payer));
    constexpr double h = 1e-4;

    double ql_total = 0.0;
    for (std::size_t i = 0; i < market.futures.size() + market.ois.size(); ++i) {
        auto up = market;
        auto down = market;
        if (i < market.futures.size()) {
            up.futures[i].price -= 100.0 * h;
            down.futures[i].price += 100.0 * h;
        } else {
            const std::size_t ois_index = i - market.futures.size();
            up.ois[ois_index].par_rate += h;
            down.ois[ois_index].par_rate -= h;
        }
        ql_total += (quantlib_target_pv(up, ql::OvernightIndexedSwap::Payer) -
                     quantlib_target_pv(down, ql::OvernightIndexedSwap::Payer)) /
                    (2.0 * h) * 1e-4;
    }
    const double our_total = std::accumulate(ours.begin(), ours.end(), 0.0);
    EXPECT_NEAR(our_total, ql_total, std::max(1e-6 * kNotional, 1e-4 * std::abs(ql_total)));
}

// --- H. Stretch: finite-difference Jacobian --------------------------------

TEST(JacobianDv01Test, CalibrationJacobianIsLowerTriangularWithUsablePivots) {
    const auto market = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto curve = bootstrapper.bootstrap(market).curve;
    const auto jacobian = irc::calibration_jacobian(market, bootstrapper, curve);
    ASSERT_EQ(jacobian.size(), 22);
    for (std::size_t row = 0; row < jacobian.size(); ++row) {
        ASSERT_EQ(jacobian[row].size(), jacobian.size());
        for (std::size_t column = row + 1; column < jacobian.size(); ++column) {
            EXPECT_LE(std::abs(jacobian[row][column]), 1e-10);
        }
        EXPECT_GT(std::abs(jacobian[row][row]), 100.0 * std::numeric_limits<double>::epsilon());
    }
}

TEST(JacobianDv01Test, RejectsMalformedMatricesBeforeSolving) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({}, {}), std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({{1.0, 0.0}}, {1.0}), std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({{1.0}, {0.0, 1.0}}, {1.0, 2.0}),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({{1.0}}, {1.0, 2.0}), std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({{nan}}, {1.0}), std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({{1.0}}, {nan}), std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({{1.0, 1e-4}, {1.0, 1.0}}, {1.0, 2.0}),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::quote_dv01_via_jacobian({{0.0}}, {1.0}), std::invalid_argument);
}

TEST(JacobianDv01Test, JacobianAndDirectQuoteDv01AgreePerInstrument) {
    const auto market = load_pinned_market();
    const irc::SofrCurveBootstrapper bootstrapper;
    const auto curve = bootstrapper.bootstrap(market).curve;
    const auto pv = target_pricer(market.as_of.valuation_date, irc::SwapSide::Payer);
    const auto direct = irc::quote_dv01_direct(market, bootstrapper, pv);
    const auto node_sensitivities = irc::curve_node_sensitivities(curve, pv);
    const auto jacobian = irc::calibration_jacobian(market, bootstrapper, curve);
    const auto transformed = irc::quote_dv01_via_jacobian(jacobian, node_sensitivities);
    ASSERT_EQ(direct.size(), transformed.size());
    for (std::size_t i = 0; i < direct.size(); ++i) {
        const double tolerance = std::max(1e-10 * kNotional, 1e-6 * std::abs(direct[i]));
        EXPECT_NEAR(direct[i], transformed[i], tolerance) << "instrument " << i;
    }
}

}  // namespace
