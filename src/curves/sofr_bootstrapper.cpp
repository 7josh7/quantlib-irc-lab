#include "curves/sofr_bootstrapper.hpp"

#include "core/bracketed_bisection.hpp"
#include "rates/coupon_period.hpp"
#include "rates/rate_accrual.hpp"
#include "rates/vanilla_swap.hpp"

#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/imm.hpp>
#include <ql/time/schedule.hpp>

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace irc {
namespace {

// §5 test 20: every instrument must reprice to within this, in rate units.
// Deliberately looser than the solver's 1e-12 residual target — that is what
// bisection aims at per instrument, this is what the finished curve must
// satisfy once every node is in place.
constexpr double kCalibrationTolerance = 1e-10;

// The bootstrapper receives a struct, not a file, so it cannot assume the
// loader ran: the group D tests build markets by mutating a loaded snapshot.
// Every rejection here is std::invalid_argument per §5 test 21; only solver
// exhaustion (21g) raises std::runtime_error. Messages name the stable
// instrument ID so a rejected snapshot points at the offending quote.
void validate_market(const SofrMarketData& market) {
    const QuantLib::Calendar settlement_calendar{
        QuantLib::UnitedStates(QuantLib::UnitedStates::Settlement)};

    if (market.futures.empty()) {
        throw std::invalid_argument("SofrCurveBootstrapper: futures strip must not be empty");
    }
    if (market.as_of.valuation_date == QuantLib::Date() ||
        !settlement_calendar.isBusinessDay(market.as_of.valuation_date)) {
        throw std::invalid_argument(
            "SofrCurveBootstrapper: valuation_date must be a UnitedStates(Settlement) business "
            "day");
    }

    for (std::size_t i = 0; i < market.futures.size(); ++i) {
        const SofrFutureQuote& future = market.futures[i];
        if (!std::isfinite(future.price)) {
            throw std::invalid_argument("SofrCurveBootstrapper: non-finite price for future '" +
                                        future.id + "'");
        }
        if (future.reference_start == QuantLib::Date() ||
            future.reference_end == QuantLib::Date() ||
            !QuantLib::IMM::isIMMdate(future.reference_start, true) ||
            !QuantLib::IMM::isIMMdate(future.reference_end, true)) {
            throw std::invalid_argument("SofrCurveBootstrapper: future '" + future.id +
                                        "' reference dates must be main-cycle IMM dates");
        }
        if (future.reference_end <= future.reference_start) {
            throw std::invalid_argument("SofrCurveBootstrapper: future '" + future.id +
                                        "' must end after it starts");
        }
        if (i > 0 && future.reference_start != market.futures[i - 1].reference_end) {
            throw std::invalid_argument("SofrCurveBootstrapper: future '" + future.id +
                                        "' does not start where '" + market.futures[i - 1].id +
                                        "' ends; the strip must be contiguous");
        }
    }

    if (market.futures.front().reference_end <= market.as_of.valuation_date) {
        throw std::invalid_argument("SofrCurveBootstrapper: future '" + market.futures.front().id +
                                    "' has already settled on the valuation date");
    }

    for (const OisQuote& ois : market.ois) {
        if (!std::isfinite(ois.par_rate)) {
            throw std::invalid_argument("SofrCurveBootstrapper: non-finite par rate for OIS '" +
                                        ois.id + "'");
        }
    }
    for (const SofrFixing& fixing : market.fixings) {
        if (!std::isfinite(fixing.rate)) {
            std::ostringstream message;
            message << "SofrCurveBootstrapper: non-finite SOFR fixing on " << fixing.rate_date;
            throw std::invalid_argument(message.str());
        }
    }
}

// log M for the first pillar: P(0,T_e1) = M * P(0; T_s1, T_e1).
//   straddling contract -> M = A^SOFR over [T_s1, valuation_date)
//   fully forward       -> M = 1, so log M = 0
// The already-settled case is rejected in validate_market (test 21c), so the
// realized window here is always non-empty when it is computed at all.
double first_pillar_log_multiplier(const SofrMarketData& market) {
    const SofrFutureQuote& first = market.futures.front();
    if (first.reference_start >= market.as_of.valuation_date) {
        return 0.0;  // nothing realized; §3 allows fully-forward input with empty fixings
    }
    const QuantLib::Calendar fixing_calendar{QuantLib::UnitedStates(QuantLib::UnitedStates::SOFR)};
    return std::log(realized_accumulation(market.fixings, first.reference_start,
                                          market.as_of.valuation_date, fixing_calendar));
}
struct SofrOisConventions {
    QuantLib::Integer spot_lag_days = 2;
    QuantLib::Integer payment_lag_days = 2;
    QuantLib::Frequency fixed_frequency = QuantLib::Annual;
    QuantLib::BusinessDayConvention roll = QuantLib::ModifiedFollowing;
    QuantLib::DayCounter accrual_day_counter = QuantLib::Actual360();
    QuantLib::Calendar business_calendar{
        QuantLib::UnitedStates(QuantLib::UnitedStates::Settlement)};
    QuantLib::Calendar fixing_calendar{QuantLib::UnitedStates(QuantLib::UnitedStates::SOFR)};
};

// The instrument implied by one spot-starting OIS quote. Dates are derived,
// never stored: spot from the valuation date, maturity from the tenor, then an
// annual schedule. Both legs share these periods — a standard USD SOFR OIS
// pays annually on both sides — so one vector serves the fixed and floating
// constructors alike.
//
// This mirrors target_schedule()/target_periods() in
// tests/test_curve_bootstrap.cpp so the QuantLib oracle compares like for
// like; advance(spot, Period, convention) delegates to
// advance(spot, length, units, convention), which is the spelling used there.
//
// The caller takes the pillar from periods.back().payment_date — the last
// payment date including the lag, not the maturity.
std::vector<CouponPeriod> ois_coupon_periods(const OisQuote& quote, const MarketAsOf& as_of,
                                             const SofrOisConventions& conventions) {
    const QuantLib::Date spot = conventions.business_calendar.advance(
        as_of.valuation_date, conventions.spot_lag_days, QuantLib::Days);
    const QuantLib::Date maturity =
        conventions.business_calendar.advance(spot, quote.tenor, conventions.roll);

    const QuantLib::Schedule schedule(spot, maturity, QuantLib::Period(conventions.fixed_frequency),
                                      conventions.business_calendar, conventions.roll,
                                      conventions.roll, QuantLib::DateGeneration::Forward, false);

    return make_coupon_periods(schedule, conventions.accrual_day_counter,
                               conventions.business_calendar, conventions.payment_lag_days);
}

}  // namespace

BootstrapResult SofrCurveBootstrapper::bootstrap(const SofrMarketData& market) const {
    validate_market(market);

    const MarketAsOf& as_of = market.as_of;
    const QuantLib::DayCounter curve_day_counter = QuantLib::Actual365Fixed();
    const SofrOisConventions conventions;

    std::vector<CurveNode> pillars;
    pillars.reserve(market.futures.size() + market.ois.size());
    std::vector<CalibrationDiagnostic> diagnostics;
    diagnostics.reserve(market.futures.size() + market.ois.size());

    // Futures pillars are closed form: zero solver iterations, no widened
    // bracket. The three quote fields stay zero here and are filled by the
    // single model_quotes pass once the curve exists.
    const double first_log_discount =
        sofr_future_log_forward_discount(market.futures[0]) + first_pillar_log_multiplier(market);
    pillars.push_back(CurveNode(market.futures[0].reference_end, first_log_discount));
    diagnostics.push_back(CalibrationDiagnostic{market.futures[0].id, 0.0, 0.0, 0.0, 0, false});

    for (std::size_t i = 1; i < market.futures.size(); ++i) {
        pillars.push_back(CurveNode(
            market.futures[i].reference_end,
            pillars[i - 1].log_discount + sofr_future_log_forward_discount(market.futures[i])));
        diagnostics.push_back(CalibrationDiagnostic{market.futures[i].id, 0.0, 0.0, 0.0, 0, false});
    }

    const auto overnight_accrual =
        std::make_shared<const CompoundedOvernightRate>(conventions.fixing_calendar);

    for (const OisQuote& ois : market.ois) {
        const std::vector<CouponPeriod> periods = ois_coupon_periods(ois, as_of, conventions);
        const QuantLib::Date pillar_date = periods.back().payment_date;  // not maturity
        const VanillaSwap swap(SwapSide::Payer, FixedLeg(periods, 1.0, ois.par_rate),
                               FloatingLeg(periods, 1.0, overnight_accrual));

        const CurveNode& previous = pillars.back();
        const double segment = curve_day_counter.yearFraction(as_of.valuation_date, pillar_date) -
                               curve_day_counter.yearFraction(as_of.valuation_date, previous.date);

        auto residual = [&](double x) {
            std::vector<CurveNode> trial = pillars;
            trial.push_back(CurveNode(pillar_date, x));
            const PiecewiseLogLinearCurve candidate(as_of.valuation_date, trial, curve_day_counter);
            return swap.fair_rate(candidate) - ois.par_rate;
        };
        constexpr double kSolverResidualTolerance = 1e-12;
        const auto brackets_root = [](double f_lower, double f_upper) {
            if (!std::isfinite(f_lower) || !std::isfinite(f_upper)) {
                return false;
            }
            return std::abs(f_lower) <= kSolverResidualTolerance ||
                   std::abs(f_upper) <= kSolverResidualTolerance ||
                   std::signbit(f_lower) != std::signbit(f_upper);
        };

        const double initial_lower = previous.log_discount - 0.50 * segment;
        const double initial_upper = previous.log_discount + 0.10 * segment;
        const double expanded_lower = previous.log_discount - 1.00 * segment;
        const double expanded_upper = previous.log_discount + 0.50 * segment;

        BisectionResult solved{};
        bool used_expanded = false;
        try {
            const double initial_f_lower = residual(initial_lower);
            const double initial_f_upper = residual(initial_upper);

            if (brackets_root(initial_f_lower, initial_f_upper)) {
                solved = bracketed_bisection(residual, initial_lower, initial_upper);
            } else {
                const double expanded_f_lower = residual(expanded_lower);
                const double expanded_f_upper = residual(expanded_upper);

                if (brackets_root(expanded_f_lower, expanded_f_upper)) {
                    used_expanded = true;
                    solved = bracketed_bisection(residual, expanded_lower, expanded_upper);
                } else {
                    std::ostringstream message;
                    message << std::setprecision(std::numeric_limits<double>::max_digits10)
                            << "failed to bracket a calibration root: "
                            << "initial_forward_rate=[-0.10, 0.50], initial_node=[" << initial_lower
                            << ", " << initial_upper << "], initial_residuals=[" << initial_f_lower
                            << ", " << initial_f_upper << "], "
                            << "expanded_forward_rate=[-0.50, 1.00], expanded_node=["
                            << expanded_lower << ", " << expanded_upper << "], expanded_residuals=["
                            << expanded_f_lower << ", " << expanded_f_upper << "]";
                    throw std::runtime_error(message.str());
                }
            }
        } catch (const std::exception& error) {
            throw std::runtime_error("SofrCurveBootstrapper: OIS '" + ois.id +
                                     "' solve failed: " + error.what());
        }

        pillars.push_back(CurveNode(pillar_date, solved.root));
        diagnostics.push_back(
            CalibrationDiagnostic{ois.id, 0.0, 0.0, 0.0, solved.iterations, used_expanded});
    }

    PiecewiseLogLinearCurve curve(as_of.valuation_date, std::move(pillars), curve_day_counter);

    // Quote fields come from one model_quotes pass over the finished curve, not
    // from solver intermediates. The two coincide for a sequential bootstrap --
    // instrument m depends only on nodes 1..m, the math note's lower-triangular
    // property -- but recomputing asserts that the completed curve reprices its
    // own inputs through the same path a caller uses, rather than assuming the
    // theorem. It also keeps one implementation of g(theta).
    const std::vector<double> model = model_quotes(market, curve);
    if (model.size() != diagnostics.size()) {
        throw std::runtime_error(
            "SofrCurveBootstrapper: model quote count does not match the instrument count");
    }

    for (std::size_t m = 0; m < diagnostics.size(); ++m) {
        const double market_quote = m < market.futures.size()
                                        ? sofr_future_rate_from_price(market.futures[m].price)
                                        : market.ois[m - market.futures.size()].par_rate;

        diagnostics[m].market_quote = market_quote;
        diagnostics[m].model_quote = model[m];
        diagnostics[m].residual = model[m] - market_quote;

        if (!std::isfinite(diagnostics[m].residual) ||
            std::abs(diagnostics[m].residual) > kCalibrationTolerance) {
            std::ostringstream message;
            message << std::setprecision(std::numeric_limits<double>::max_digits10)
                    << "SofrCurveBootstrapper: '" << diagnostics[m].instrument_id
                    << "' does not reprice: market=" << market_quote << ", model=" << model[m]
                    << ", residual=" << diagnostics[m].residual
                    << ", tolerance=" << kCalibrationTolerance;
            throw std::runtime_error(message.str());
        }
    }

    return BootstrapResult{as_of, std::move(curve), std::move(diagnostics)};
}

std::vector<double> SofrCurveBootstrapper::model_quotes(
    const SofrMarketData& market, const PiecewiseLogLinearCurve& curve) const {
    std::vector<double> result;
    result.reserve(market.futures.size() + market.ois.size());
    for (int i = 0; i < market.futures.size(); ++i) {
        const double M = i == 0 ? std::exp(first_pillar_log_multiplier(market))
                                : curve.discount(market.futures[i].reference_start);
        ;
        result.push_back(sofr_future_model_rate(market.futures[i], M,
                                                curve.discount(market.futures[i].reference_end)));
    }
    SofrOisConventions conventions;
    const QuantLib::DayCounter curve_day_counter = QuantLib::Actual365Fixed();
    const auto overnight_accrual =
        std::make_shared<const CompoundedOvernightRate>(conventions.fixing_calendar);
    for (const OisQuote& ois : market.ois) {
        const std::vector<CouponPeriod> periods =
            ois_coupon_periods(ois, market.as_of, conventions);
        const QuantLib::Date pillar_date = periods.back().payment_date;  // not maturity
        const VanillaSwap swap(SwapSide::Payer, FixedLeg(periods, 1.0, ois.par_rate),
                               FloatingLeg(periods, 1.0, overnight_accrual));
        result.push_back(swap.fair_rate(curve));
    }
    return result;
}

}  // namespace irc
