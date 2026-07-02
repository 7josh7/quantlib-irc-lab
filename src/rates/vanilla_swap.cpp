#include "rates/vanilla_swap.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

VanillaSwap::VanillaSwap(SwapSide side, FixedLeg fixed_leg, FloatingLeg floating_leg)
    : side_(side), fixed_leg_(std::move(fixed_leg)), floating_leg_(std::move(floating_leg)) {}

double VanillaSwap::npv(const YieldCurve& curve) const {
    // Receiver value = fixed_pv - float_pv; payer is the negative. Note §4.
    const double receiver_value =
        fixed_leg_.present_value(curve) - floating_leg_.present_value(curve);
    return side_ == SwapSide::Receiver ? receiver_value : -receiver_value;
}

double VanillaSwap::fair_rate(const YieldCurve& curve) const {
    // fair rate = floating-leg PV / annuity (both include the notional, so it
    // cancels). Math note §5. Side-independent by construction.
    const double annuity = fixed_leg_.annuity(curve);
    if (annuity == 0.0) {
        throw std::runtime_error("VanillaSwap::fair_rate: annuity is zero");
    }
    return floating_leg_.present_value(curve) / annuity;
}

}  // namespace irc
