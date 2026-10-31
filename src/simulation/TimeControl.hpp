#pragma once

#include <algorithm>
#include <limits>
#include <string>

#include "core/Types.hpp"

namespace saturn {

enum class TimeScheme {
  ForwardEuler,
  SSPRK2,
  SSPRK3,
  BackwardEuler,
  CrankNicolson,
  BDF2
};

inline bool isExplicit(TimeScheme scheme) {
  return scheme == TimeScheme::ForwardEuler || scheme == TimeScheme::SSPRK2 ||
         scheme == TimeScheme::SSPRK3;
}

inline const char* timeSchemeName(TimeScheme scheme) {
  switch (scheme) {
  case TimeScheme::ForwardEuler:
    return "forwardEuler";
  case TimeScheme::SSPRK2:
    return "ssprk2";
  case TimeScheme::SSPRK3:
    return "ssprk3";
  case TimeScheme::BackwardEuler:
    return "backwardEuler";
  case TimeScheme::CrankNicolson:
    return "crankNicolson";
  case TimeScheme::BDF2:
    return "bdf2";
  }
  return "unknown";
}

/// Time-step controls and the explicit stability limit.
struct TimeControl {
  Real t0 = 0;
  Real tEnd = 1;
  Real dtMax = 1e-2;
  Real dtMin = 1e-12;
  Real cfl = 0.4;
  Real diffusionSafety = 0.5;
  int maxSteps = 1000000;
  bool adaptive = true;

  /// Explicit convective and diffusive limits. `dim` is the spatial dimension
  /// (1, 2 or 3), not the number of components.
  Real stableDt(Real maxWaveSpeed, Real maxDiffusivity, Real minCellSize,
                int dim) const {
    Real dt = dtMax;
    if (maxWaveSpeed > 0)
      dt = std::min(dt, cfl * minCellSize / maxWaveSpeed);
    if (maxDiffusivity > 0 && dim > 0) {
      const Real diffLimit = diffusionSafety * minCellSize * minCellSize /
                             (Real(2 * dim) * maxDiffusivity);
      dt = std::min(dt, diffLimit);
    }
    return dt;
  }
};

}  // namespace saturn
