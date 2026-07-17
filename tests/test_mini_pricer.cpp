// Phase 1 mini-pricer tests — originally written before the implementations
// under AGENTS.md workflow step 3. These are now the green regression suite.
//
// Trade and conventions: docs/impl_notes/01_mini_pricer.md §1.
// Math:                  docs/math_notes/01_sofr_swap.md.
//
// Deterministic throughout; no randomness, no market data.

#include "core/flat_curve.hpp"
#include "rates/fixed_leg.hpp"
#include "rates/floating_leg.hpp"
#include "rates/rate_accrual.hpp"
#include "rates/vanilla_swap.hpp"

#include <gtest/gtest.h>
#include <ql/quantlib.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace {

namespace ql = QuantLib;

class MiniPricerTest : public ::testing::Test {
protected:
    // Conventions pinned in impl note §1. Valuation date matches the
    // Phase 0 example.
    const ql::Date valuation_{23, ql::May, 2026};
    const ql::Calendar calendar_{ql::UnitedStates(ql::UnitedStates::SOFR)};
    const ql::Date settlement_{calendar_.advance(valuation_, 2, ql::Days)};
    const ql::Date maturity_{calendar_.advance(settlement_, 5, ql::Years)};

    const double flat_rate_ = 0.04;
    const double notional_ = 1'000'000.0;
    const double fixed_rate_ = 0.04;  // deliberately non-par

    const ql::DayCounter curve_dc_ = ql::Actual365Fixed();  // curve accrual
    const ql::DayCounter leg_dc_ = ql::Actual360();         // leg accrual

    const irc::FlatCurve curve_{settlement_, flat_rate_, curve_dc_};

    ql::Schedule annual_schedule() const {
        return ql::Schedule(settlement_, maturity_, ql::Period(ql::Annual), calendar_,
                            ql::ModifiedFollowing, ql::ModifiedFollowing,
                            ql::DateGeneration::Forward, false);
    }

    irc::FixedLeg make_fixed_leg(double rate) const {
        return {annual_schedule(), leg_dc_, notional_, rate};
    }

    irc::FloatingLeg make_floating_leg(std::shared_ptr<const irc::RateAccrual> accrual) const {
        return {annual_schedule(), leg_dc_, notional_, std::move(accrual)};
    }

    irc::VanillaSwap make_swap(irc::SwapSide side, double rate) const {
        return {side, make_fixed_leg(rate),
                make_floating_leg(std::make_shared<irc::CompoundedOvernightRate>(calendar_))};
    }
};

// --- 1. FlatCurve basics --------------------------------------------------

TEST_F(MiniPricerTest, FlatCurveDiscountAtReferenceIsOne) {
    EXPECT_DOUBLE_EQ(curve_.discount(settlement_), 1.0);
}

TEST_F(MiniPricerTest, FlatCurveMatchesClosedForm) {
    // P(t,T) = exp(-r * tau_365F(t,T)) — impl note §1 curve convention.
    const double tau = curve_dc_.yearFraction(settlement_, maturity_);
    EXPECT_NEAR(curve_.discount(maturity_), std::exp(-flat_rate_ * tau), 1e-14);
}

TEST_F(MiniPricerTest, FlatCurveIsStrictlyDecreasing) {
    const auto schedule = annual_schedule();
    double previous = curve_.discount(schedule.dates().front());
    for (std::size_t i = 1; i < schedule.size(); ++i) {
        const double current = curve_.discount(schedule.dates()[i]);
        EXPECT_LT(current, previous) << "not decreasing at index " << i;
        previous = current;
    }
}

TEST_F(MiniPricerTest, FlatCurveThrowsBeforeReference) {
    EXPECT_THROW(curve_.discount(settlement_ - 1), std::invalid_argument);
}

// --- 2. Strategies agree on a single curve --------------------------------

// Single-curve telescoping (math note §4): daily compounding of curve
// forwards collapses to P(start)/P(end), so both strategies must produce
// the same floating-leg PV. Tolerance is 1e-7 absolute (not the plan's
// 1e-10): ~1250 daily factors each carry O(1e-16) rounding.
TEST_F(MiniPricerTest, SimpleAndCompoundedStrategiesAgree) {
    const auto simple = make_floating_leg(std::make_shared<irc::SimpleForwardRate>());
    const auto compounded =
        make_floating_leg(std::make_shared<irc::CompoundedOvernightRate>(calendar_));

    EXPECT_NEAR(simple.present_value(curve_), compounded.present_value(curve_), 1e-7);
}

// Analytic anchor: PV_flt = N * (P(T0) - P(Tn))  (math note §4).
TEST_F(MiniPricerTest, FloatingLegTelescopesAnalytically) {
    const auto leg = make_floating_leg(std::make_shared<irc::SimpleForwardRate>());
    const auto schedule = annual_schedule();
    const double expected = notional_ * (curve_.discount(schedule.dates().front()) -
                                         curve_.discount(schedule.dates().back()));
    EXPECT_NEAR(leg.present_value(curve_), expected, 1e-8);
}

// --- 3. Annuity -----------------------------------------------------------

TEST_F(MiniPricerTest, AnnuityMatchesManualSum) {
    const auto leg = make_fixed_leg(fixed_rate_);
    const auto schedule = annual_schedule();

    double expected = 0.0;  // N * sum_i tau_i P(t,T_i) — math note §3
    for (std::size_t i = 1; i < schedule.size(); ++i) {
        const double tau = leg_dc_.yearFraction(schedule.dates()[i - 1], schedule.dates()[i]);
        expected += notional_ * tau * curve_.discount(schedule.dates()[i]);
    }
    EXPECT_NEAR(leg.annuity(curve_), expected, 1e-10);

    // present_value = K * annuity (math note §3).
    EXPECT_NEAR(leg.present_value(curve_), fixed_rate_ * leg.annuity(curve_), 1e-10);
}

// --- 4. Par swap has zero NPV ----------------------------------------------

TEST_F(MiniPricerTest, ParSwapHasZeroNpv) {
    const auto probe = make_swap(irc::SwapSide::Receiver, fixed_rate_);
    const double fair = probe.fair_rate(curve_);
    const auto par_swap = make_swap(irc::SwapSide::Receiver, fair);
    EXPECT_NEAR(par_swap.npv(curve_), 0.0, 1e-8);
}

// --- 5. Payer / receiver sign flip ------------------------------------------

TEST_F(MiniPricerTest, PayerIsNegativeOfReceiver) {
    const auto payer = make_swap(irc::SwapSide::Payer, fixed_rate_);
    const auto receiver = make_swap(irc::SwapSide::Receiver, fixed_rate_);
    EXPECT_NEAR(payer.npv(curve_), -receiver.npv(curve_), 1e-10);
}

// --- 6. Fair-rate sanity -----------------------------------------------------

// Flat 4% continuous Act/365F curve, quoted on an Act/360 annual leg:
// roughly r * 365/360 adjusted by compounding — expect ~4.0-4.1%, and
// certainly within [3.5%, 4.5%].
TEST_F(MiniPricerTest, FairRateIsPlausible) {
    const auto swap = make_swap(irc::SwapSide::Receiver, fixed_rate_);
    const double fair = swap.fair_rate(curve_);
    EXPECT_GT(fair, 0.035);
    EXPECT_LT(fair, 0.045);
}

// --- 7. QuantLib oracle ------------------------------------------------------

// Identical trade priced by QuantLib's OvernightIndexedSwap on the same
// flat curve. Roadmap milestone gate: |NPV diff| < 1e-6.
// If this fails, walk AGENTS.md "Numerical debugging protocol" in order
// (df direction -> day count -> calendar) before touching tolerances.
TEST_F(MiniPricerTest, MatchesQuantLibOvernightIndexedSwap) {
    const ql::SavedSettings settings_guard;
    ql::Settings::instance().evaluationDate() = valuation_;

    const ql::Handle<ql::YieldTermStructure> flat(
        ql::ext::make_shared<ql::FlatForward>(settlement_, flat_rate_, curve_dc_, ql::Continuous));
    const auto sofr = ql::ext::make_shared<ql::Sofr>(flat);

    ql::OvernightIndexedSwap ql_swap(ql::OvernightIndexedSwap::Receiver, notional_,
                                     annual_schedule(), fixed_rate_, leg_dc_, sofr);
    ql_swap.setPricingEngine(ql::ext::make_shared<ql::DiscountingSwapEngine>(flat));

    const auto ours = make_swap(irc::SwapSide::Receiver, fixed_rate_);

    EXPECT_NEAR(ours.npv(curve_), ql_swap.NPV(), 1e-6);
    EXPECT_NEAR(ours.fair_rate(curve_), ql_swap.fairRate(), 1e-8);
}

// --- Phase 2 prerequisite: finite-input hardening ---------------------------

TEST_F(MiniPricerTest, RejectsNonFiniteFlatCurveRate) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    EXPECT_THROW((void)irc::FlatCurve(settlement_, nan, curve_dc_), std::invalid_argument);
    EXPECT_THROW((void)irc::FlatCurve(settlement_, inf, curve_dc_), std::invalid_argument);
    EXPECT_THROW((void)irc::FlatCurve(settlement_, -inf, curve_dc_), std::invalid_argument);
}

TEST_F(MiniPricerTest, RejectsNonFiniteLegInputs) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const auto schedule = annual_schedule();

    EXPECT_THROW((void)irc::FixedLeg(schedule, leg_dc_, nan, fixed_rate_), std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg(schedule, leg_dc_, inf, fixed_rate_), std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg(schedule, leg_dc_, notional_, nan), std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg(schedule, leg_dc_, notional_, inf), std::invalid_argument);

    const auto accrual = std::make_shared<irc::SimpleForwardRate>();
    EXPECT_THROW((void)irc::FloatingLeg(schedule, leg_dc_, nan, accrual), std::invalid_argument);
    EXPECT_THROW((void)irc::FloatingLeg(schedule, leg_dc_, inf, accrual), std::invalid_argument);
    EXPECT_THROW((void)irc::FloatingLeg(schedule, leg_dc_, notional_, accrual, nan),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FloatingLeg(schedule, leg_dc_, notional_, accrual, inf),
                 std::invalid_argument);
}

TEST_F(MiniPricerTest, RejectsNonFiniteCallerSuppliedYearFractions) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const irc::SimpleForwardRate simple;
    const irc::CompoundedOvernightRate compounded(calendar_);

    EXPECT_THROW((void)simple.forward_rate(curve_, settlement_, settlement_ + 1, nan),
                 std::invalid_argument);
    EXPECT_THROW((void)simple.forward_rate(curve_, settlement_, settlement_ + 1, inf),
                 std::invalid_argument);
    EXPECT_THROW((void)compounded.forward_rate(curve_, settlement_, settlement_ + 1, nan),
                 std::invalid_argument);
    EXPECT_THROW((void)compounded.forward_rate(curve_, settlement_, settlement_ + 1, inf),
                 std::invalid_argument);
}

// --- Phase 2 rates layer: CouponPeriod and payment lag ----------------------

TEST_F(MiniPricerTest, CouponPeriodsApplyBusinessDayPaymentLag) {
    const ql::Calendar payment_calendar{ql::UnitedStates(ql::UnitedStates::Settlement)};
    const ql::Schedule schedule(
        std::vector<ql::Date>{ql::Date(15, ql::January, 2026), ql::Date(16, ql::January, 2026)});

    const auto lagged = irc::make_coupon_periods(schedule, leg_dc_, payment_calendar, 2);
    ASSERT_EQ(lagged.size(), 1);
    EXPECT_EQ(lagged[0].accrual_end, ql::Date(16, ql::January, 2026));
    EXPECT_EQ(lagged[0].payment_date, ql::Date(21, ql::January, 2026));
    EXPECT_DOUBLE_EQ(lagged[0].year_fraction,
                     leg_dc_.yearFraction(lagged[0].accrual_start, lagged[0].accrual_end));

    const auto zero_lag = irc::make_coupon_periods(schedule, leg_dc_, payment_calendar, 0);
    ASSERT_EQ(zero_lag.size(), 1);
    EXPECT_EQ(zero_lag[0].payment_date, zero_lag[0].accrual_end);
}

TEST_F(MiniPricerTest, CouponPeriodAndPeriodLegValidationRejectBadInputs) {
    const ql::Calendar payment_calendar{ql::UnitedStates(ql::UnitedStates::Settlement)};
    const ql::Schedule one_date(std::vector<ql::Date>{ql::Date(16, ql::January, 2026)});
    EXPECT_THROW((void)irc::make_coupon_periods(one_date, leg_dc_, payment_calendar, 2),
                 std::invalid_argument);
    EXPECT_THROW(
        (void)irc::make_coupon_periods(annual_schedule(), ql::DayCounter(), payment_calendar, 2),
        std::invalid_argument);
    EXPECT_THROW((void)irc::make_coupon_periods(annual_schedule(), leg_dc_, ql::Calendar(), 2),
                 std::invalid_argument);

    const ql::Date start(15, ql::January, 2026);
    const ql::Date end(16, ql::January, 2026);
    const double tau = leg_dc_.yearFraction(start, end);
    const auto accrual = std::make_shared<irc::SimpleForwardRate>();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    EXPECT_THROW((void)irc::FixedLeg(std::vector<irc::CouponPeriod>{}, notional_, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{{}, end, end, tau}}, notional_, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{end, start, end, tau}}, notional_, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{start, end, start, tau}}, notional_, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{start, end, end, nan}}, notional_, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{start, end, end, 0.0}}, notional_, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{start, end, end, tau}, {start, end + 1, end + 1, tau}},
                                     notional_, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{start, end, end, tau}}, inf, fixed_rate_),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FixedLeg({{start, end, end, tau}}, notional_, nan),
                 std::invalid_argument);

    EXPECT_THROW((void)irc::FloatingLeg({{start, end, end, tau}}, notional_, accrual, inf),
                 std::invalid_argument);
    EXPECT_THROW((void)irc::FloatingLeg({{start, end, end, tau}}, notional_, nullptr),
                 std::invalid_argument);
}

TEST_F(MiniPricerTest, ZeroLagPeriodLegsMatchLegacyAndLaggedPaymentsDiscountMore) {
    const auto schedule = annual_schedule();
    std::vector<irc::CouponPeriod> zero_lag_periods;
    std::vector<irc::CouponPeriod> lagged_periods;
    const ql::Calendar payment_calendar{ql::UnitedStates(ql::UnitedStates::Settlement)};
    for (std::size_t i = 1; i < schedule.size(); ++i) {
        const ql::Date start = schedule.dates()[i - 1];
        const ql::Date end = schedule.dates()[i];
        const double tau = leg_dc_.yearFraction(start, end);
        zero_lag_periods.push_back({start, end, end, tau});
        lagged_periods.push_back({start, end, payment_calendar.advance(end, 2, ql::Days), tau});
    }

    const irc::FixedLeg legacy_fixed(schedule, leg_dc_, notional_, fixed_rate_);
    const irc::FixedLeg period_fixed(zero_lag_periods, notional_, fixed_rate_);
    const irc::FixedLeg lagged_fixed(lagged_periods, notional_, fixed_rate_);
    EXPECT_NEAR(period_fixed.annuity(curve_), legacy_fixed.annuity(curve_), 1e-12);
    EXPECT_NEAR(period_fixed.present_value(curve_), legacy_fixed.present_value(curve_), 1e-12);
    EXPECT_LT(lagged_fixed.present_value(curve_), period_fixed.present_value(curve_));

    const auto accrual = std::make_shared<irc::SimpleForwardRate>();
    const irc::FloatingLeg legacy_float(schedule, leg_dc_, notional_, accrual);
    const irc::FloatingLeg period_float(zero_lag_periods, notional_, accrual);
    EXPECT_NEAR(period_float.present_value(curve_), legacy_float.present_value(curve_), 1e-12);
}

}  // namespace
