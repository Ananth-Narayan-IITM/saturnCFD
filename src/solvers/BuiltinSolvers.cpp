#include "solvers/BuiltinSolvers.hpp"

#include "simulation/Registry.hpp"
#include "solvers/saturnBurger/BurgersCase.hpp"
#include "solvers/saturnHeat/HeatCase.hpp"
#ifdef SATURN_USE_PETSC
#include "solvers/saturnStokes/StokesCase.hpp"
#endif

namespace saturn {

void registerBuiltinSolvers() {
  SolverRegistry& registry = solverRegistry();
  if (!registry.contains("saturnBurger"))
    registerSaturnBurger(registry);
  if (!registry.contains("saturnHeat"))
    registerSaturnHeat(registry);
#ifdef SATURN_USE_PETSC
  if (!registry.contains("saturnStokes"))
    registerSaturnStokes(registry);
#endif
}

}  // namespace saturn
