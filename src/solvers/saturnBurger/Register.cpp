#include "solvers/saturnBurger/Register.hpp"

#include "config/Config.hpp"
#include "simulation/Registry.hpp"
#include "solvers/saturnBurger/BurgersCase.hpp"

namespace saturn {

void registerSaturnBurger(SolverRegistry& registry) {
  registry.addCase(
      "saturnBurger", [](const Config& cfg) { return makeSaturnBurger(cfg); },
      [] { return burgersSchema(); });
}

}  // namespace saturn
