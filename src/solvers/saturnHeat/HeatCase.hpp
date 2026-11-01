#pragma once

#include <memory>

#include "config/Config.hpp"
#include "config/ConfigSchema.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

class SolverRegistry;

/// Complete option list for `saturnHeat`: common keys plus the heat block.
ConfigSchema heatSchema();

/// Build the scalar convection–diffusion solver described by a validated case.
///
/// `heat.initial: fourier` uses
///
///   T(x,t) = A sin(k.x - (a.k)t) exp(-alpha |k|^2 t)
///
/// with uniform velocity `a` and prescribed wave vector `k`.
/// `heat.initial: constant` uses `heat.value`.
std::unique_ptr<Solver> makeSaturnHeat(const Config& cfg);

void registerSaturnHeat(SolverRegistry& registry);

}  // namespace saturn
