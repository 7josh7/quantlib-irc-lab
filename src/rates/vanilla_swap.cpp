#include "rates/vanilla_swap.hpp"

#include <stdexcept>
#include <utility>

namespace irc {

VanillaSwap::VanillaSwap(SwapSide side, FixedLeg fixed_leg, FloatingLeg floating_leg)
    : side_(side),
      fixed_leg_(std::move(fixed_leg)),
      floating_leg_(std::move(floating_leg)) {}

double VanillaSwap::npv(const YieldCurve& curve) const {
    // TODO(step 4): receiver = fixed_pv - float_pv; payer = the negative.
    // Math note §4 (V_rec).
    (void)curve;
    throw std::logic_error("VanillaSwap::npv: not implemented (Phase 1 step 4)");
}

double VanillaSwap::fair_rate(const YieldCurve& curve) const {
    // TODO(step 4): floating_leg PV / fixed_leg annuity.  Math note §5.
    (void)curve;
    throw std::logic_error("VanillaSwap::fair_rate: not implemented (Phase 1 step 4)");
}

}  // namespace irc
