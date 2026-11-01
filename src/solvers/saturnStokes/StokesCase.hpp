#pragma once

#include <memory>

#include "config/Config.hpp"
#include "config/ConfigSchema.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

class SolverRegistry;

ConfigSchema stokesSchema();

std::unique_ptr<Solver> makeSaturnStokes(const Config& cfg);

void registerSaturnStokes(SolverRegistry& registry);

}  // namespace saturn
