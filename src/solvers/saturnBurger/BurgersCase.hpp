#pragma once

#include <memory>

#include "config/Config.hpp"
#include "config/ConfigSchema.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

/// Complete option list for `saturnBurger`: common keys plus the Burgers block.
ConfigSchema burgersSchema();

/// Build the 1D scalar Burgers solver described by a validated case.
///
/// Supported `burgers.initial` values:
///
/// - `travelingWave`: exact viscous wave; Dirichlet data follow the wave and
///   the analytic solution is available for validation output;
/// - `sine`: `u0 = -sin(pi x)`; requires periodic boundaries;
/// - `constant`: `u0 = burgers.value`; Dirichlet data hold that value.
std::unique_ptr<Solver> makeSaturnBurger(const Config& cfg);

}  // namespace saturn
