#include "solvers/BuiltinSolvers.hpp"

#include "simulation/Registry.hpp"
#include "solvers/saturnBurger/BurgersCase.hpp"
#include "solvers/saturnHeat/HeatCase.hpp"

namespace saturn {

void registerBuiltinSolvers() {
  SolverRegistry& registry = solverRegistry();
  if (!registry.contains("saturnBurger"))
    registerSaturnBurger(registry);
  if (!registry.contains("saturnHeat"))
    registerSaturnHeat(registry);
}

}  // namespace saturn
