#pragma once

#include <memory>

#include "config/Config.hpp"
#include "config/ConfigSchema.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

class SolverRegistry;

/// Complete option list for `saturnBurger`: common keys plus the Burgers block.
ConfigSchema burgersSchema();

/// Build the scalar Burgers solver described by a validated case.
///
/// Supported `burgers.initial` values:
///
/// - `travelingWave`: exact viscous wave; Dirichlet data follow the wave and
///   the analytic solution is available for validation output. 1D uses
///   U(x-x0-Ut/2), 2D uses x+y-x0-y0-Ut, and 3D uses
///   x+y+z-x0-y0-z0-3Ut/2 inside the tanh argument.
/// - `sine`: product of sines in 1D/2D/3D; periodic only;
/// - `constant`: `u0 = burgers.value`; Dirichlet data hold that value.
std::unique_ptr<Solver> makeSaturnBurger(const Config& cfg);

/// Add the 1D scalar Burgers solver to a registry.
void registerSaturnBurger(SolverRegistry& registry);

}  // namespace saturn
