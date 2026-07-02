#pragma once
#include "rates/fixed_leg.hpp"
#include "rates/floating_leg.hpp"

namespace irc {

// Payer pays fixed and receives float; Receiver is the opposite.
// Matches the math note §1 sign convention (note takes the receiver view).
enum class SwapSide { Payer, Receiver };

// Vanilla fixed-vs-float swap. Math note §4:
//   V_rec = fixed_pv - float_pv,   V_pay = -V_rec.
class VanillaSwap {
public:
    VanillaSwap(SwapSide side, FixedLeg fixed_leg, FloatingLeg floating_leg);

    double npv(const YieldCurve& curve) const;

    // fair_rate = float_pv / annuity — side-independent (math note §5).
    double fair_rate(const YieldCurve& curve) const;

private:
    SwapSide side_;
    FixedLeg fixed_leg_;
    FloatingLeg floating_leg_;
};

}  // namespace irc
