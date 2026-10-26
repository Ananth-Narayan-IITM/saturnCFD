// saturnCFD — flux limiters for higher-order upwind convection.
//
// WHY LIMITERS
// ------------
// First-order upwind is stable but adds numerical diffusion proportional to
// |u|*h, which at moderate grids acts like an extra viscosity and lowers the
// effective Reynolds number. Central differencing is second order but
// unbounded: near a maximum it overshoots, and at high Re those overshoots
// grow until the solution breaks down.
//
// A TVD (total-variation-diminishing) limiter is the standard compromise. The
// face value is reconstructed as
//
//     u_face = u_upwind + 0.5 * psi(r) * (u_downwind - u_upwind)
//     r      = (upwind gradient) / (gradient across the face)
//
// psi(r) = 0 recovers first-order upwind (maximum damping).
// psi(r) = 1 is central differencing (second order, unbounded).
// Each limiter below interpolates between those two according to how smooth the
// local solution is: where r is close to 1 the data is smooth and psi -> 1;
// where r is far from 1 (an extremum or a jump) psi -> 0 and the scheme falls
// back to upwind. That is why the scheme stays stable at high Re.
//
// All functions are KOKKOS_INLINE_FUNCTION so they inline into device kernels.
#pragma once

#include <stdexcept>
#include <string>

#include "core/Types.hpp"

namespace saturn {

enum class ConvScheme : int {
  Upwind = 0,   ///< 1st order, maximum diffusion (reference)
  Minmod = 1,   ///< 2nd order, most diffusive TVD limiter
  VanLeer = 2,  ///< 2nd order, smooth, good general default
  Superbee = 3, ///< 2nd order, least diffusive TVD, compressive
  MC = 4,       ///< 2nd order, monotonicised central: best accuracy/stability
  Quick = 5,    ///< 3rd order, NOT TVD: can oscillate, use for comparison
  Central = 6   ///< 2nd order, unbounded: unstable at high Re, reference only
};

/// psi(r) for the limiter named by `s`.
KOKKOS_INLINE_FUNCTION
Real limiterPsi(int s, Real r) {
  switch (s) {
    case 0:  // upwind
      return Real(0);
    case 1:  // minmod: max(0, min(1, r))
      if (r <= Real(0)) return Real(0);
      return (r < Real(1)) ? r : Real(1);
    case 2: {  // van Leer: (r + |r|) / (1 + |r|)
      const Real ar = Kokkos::abs(r);
      return (r + ar) / (Real(1) + ar);
    }
    case 3: {  // superbee: max(0, min(2r,1), min(r,2))
      if (r <= Real(0)) return Real(0);
      const Real a = (Real(2) * r < Real(1)) ? Real(2) * r : Real(1);
      const Real b = (r < Real(2)) ? r : Real(2);
      return (a > b) ? a : b;
    }
    case 4: {  // MC: max(0, min(2r, (1+r)/2, 2))
      if (r <= Real(0)) return Real(0);
      Real m = Real(2) * r;
      const Real mid = Real(0.5) * (Real(1) + r);
      if (mid < m) m = mid;
      if (Real(2) < m) m = Real(2);
      return m;
    }
    case 5:  // QUICK (3rd order, not TVD)
      return Real(0.25) * (Real(3) + r);
    default:  // central
      return Real(1);
  }
}

/// Reconstructed convected value at the face between `u0` and `up1`.
/// `um1` is one point further upwind of `u0`, `up2` one further downwind of
/// `up1`; the limiter only reaches for them in the branch it needs.
/// `adv` is the advecting (face-normal) velocity, whose sign picks the branch.
KOKKOS_INLINE_FUNCTION
Real limitedFace(int s, Real um1, Real u0, Real up1, Real up2, Real adv) {
  constexpr Real tiny = Real(1e-30);
  if (adv > Real(0)) {
    const Real d = up1 - u0;
    if (Kokkos::abs(d) < tiny) return u0;
    const Real r = (u0 - um1) / d;
    return u0 + Real(0.5) * limiterPsi(s, r) * d;
  }
  const Real d = u0 - up1;
  if (Kokkos::abs(d) < tiny) return up1;
  const Real r = (up1 - up2) / d;
  return up1 + Real(0.5) * limiterPsi(s, r) * d;
}

/// Parse a scheme name from the YAML file. Throws on an unknown name so a typo
/// in a case file fails loudly instead of silently running first order.
inline int parseScheme(const std::string& name) {
  static const char* kNames[] = {"upwind", "minmod",  "vanleer", "superbee",
                                 "mc",     "quick",   "central"};
  for (int i = 0; i < 7; ++i)
    if (name == kNames[i]) return i;
  throw std::runtime_error(
      "unknown convection scheme '" + name +
      "' (expected upwind|minmod|vanleer|superbee|mc|quick|central)");
}

/// Human-readable name, for logs and the CLI.
inline const char* schemeName(int s) {
  switch (s) {
    case 0: return "upwind";
    case 1: return "minmod";
    case 2: return "vanleer";
    case 3: return "superbee";
    case 4: return "mc";
    case 5: return "quick";
    default: return "central";
  }
}

}  // namespace saturn
