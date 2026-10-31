#pragma once

namespace saturn {

class SolverRegistry;

/// Add the 1D scalar Burgers solver to a registry.
void registerSaturnBurger(SolverRegistry& registry);

}  // namespace saturn
