#include "solvers/BuiltinSolvers.hpp"

#include "simulation/Registry.hpp"
#include "solvers/saturnBurger/Register.hpp"

namespace saturn {

void registerBuiltinSolvers() {
  SolverRegistry& registry = solverRegistry();
  if (!registry.contains("saturnBurger"))
    registerSaturnBurger(registry);
}

}  // namespace saturn
