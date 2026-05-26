#include <ql/quantlib.hpp>

#include <exception>
#include <iomanip>
#include <iostream>

int main() try {
    using namespace QuantLib;

    const Date valuation_date(23, May, 2026);
    Settings::instance().evaluationDate() = valuation_date;

    const Calendar calendar = UnitedStates(UnitedStates::GovernmentBond);
    const Date settlement_date = calendar.advance(valuation_date, 2, Days);
    const Date maturity_date = calendar.advance(settlement_date, 5, Years);

    const auto flat_rate = 0.04;
    const DayCounter curve_day_counter = Actual365Fixed();
    Handle<YieldTermStructure> discount_curve(ext::make_shared<FlatForward>(
        settlement_date, flat_rate, curve_day_counter, Compounded, Annual));

    const Schedule fixed_schedule(
        settlement_date,
        maturity_date,
        Period(Annual),
        calendar,
        ModifiedFollowing,
        ModifiedFollowing,
        DateGeneration::Forward,
        false);

    const Schedule floating_schedule(
        settlement_date,
        maturity_date,
        Period(Quarterly),
        calendar,
        ModifiedFollowing,
        ModifiedFollowing,
        DateGeneration::Forward,
        false);

    const auto libor_3m = ext::make_shared<USDLibor>(Period(3, Months), discount_curve);
    const Date first_fixing_date = libor_3m->fixingDate(floating_schedule[0]);
    if (first_fixing_date <= valuation_date) {
        libor_3m->addFixing(first_fixing_date, flat_rate);
    }

    VanillaSwap swap(
        VanillaSwap::Payer,
        1'000'000.0,
        fixed_schedule,
        0.04,
        Thirty360(Thirty360::BondBasis),
        floating_schedule,
        libor_3m,
        0.0,
        Actual360());

    swap.setPricingEngine(ext::make_shared<DiscountingSwapEngine>(discount_curve));

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Valuation date = " << valuation_date << std::endl;
    std::cout << "Settlement date = " << settlement_date << std::endl;
    std::cout << "Maturity date = " << maturity_date << std::endl;
    std::cout << "NPV of vanilla IRS = " << swap.NPV() << std::endl;
    std::cout << "Fair fixed rate = " << swap.fairRate() << std::endl;

    return 0;
} catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
}
