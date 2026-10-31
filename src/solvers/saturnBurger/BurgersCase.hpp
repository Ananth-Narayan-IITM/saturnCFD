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
///   the analytic solution is available for validation output. In 2D this is
///   the diagonal wave u(x,y,t) = U/2 [1 - tanh(U(x+y-x0-y0-Ut)/(4 nu))].
/// - `sine`: 1D `-sin(pi x)` or 2D `-sin(pi x) sin(pi y)`; periodic only;
/// - `constant`: `u0 = burgers.value`; Dirichlet data hold that value.
std::unique_ptr<Solver> makeSaturnBurger(const Config& cfg);

/// Add the 1D scalar Burgers solver to a registry.
void registerSaturnBurger(SolverRegistry& registry);

}  // namespace saturn
