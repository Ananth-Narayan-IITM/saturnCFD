#pragma once

#include <cmath>

#include "core/Types.hpp"

namespace saturn {

/// Exact viscous traveling-wave solution of the 1D Burgers equation
///
///   u_t + (u^2/2)_x = nu u_xx
///
/// on the whole line:
///
///   u(x,t) = U/2 [ 1 - tanh( U (x - x0 - U t / 2) / (4 nu) ) ].
///
/// It is used as the initial condition, the Dirichlet boundary data and the
/// analytic reference for validation.
struct TravelingWave {
  Real speed = 1;
  Real nu = 0.02;
  Real x0 = 0;

  Real value(Real x, Real t) const {
    return Real(0.5) * speed *
           (Real(1) - std::tanh(speed * (x - x0 - Real(0.5) * speed * t) /
                                (Real(4) * nu)));
  }

  Real operator()(Real x, Real t) const { return value(x, t); }
};

}  // namespace saturn
