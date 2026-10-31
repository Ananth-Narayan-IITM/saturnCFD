#include <doctest/doctest.h>

#include <cmath>
#include <utility>

#include "simulation/Simulation.hpp"
#include "solvers/saturnBurger/SaturnBurger.hpp"

#include "UniformMesh.hpp"

using namespace saturn;

namespace {

double exact2D(double x, double y, double t, double speed, double nu, double x0,
               double y0) {
  return 0.5 * speed *
         (1.0 - std::tanh(speed * (x + y - x0 - y0 - speed * t) / (4.0 * nu)));
}

BurgersOptions options2D(double nu) {
  BurgersOptions options;
  options.nu = nu;
  options.periodic = false;
  options.convection = ConvectiveScheme::Muscl;
  options.limiter = Limiter::VanLeer;
  options.initial = [nu](const Vec3& x) {
    return exact2D(x.x, x.y, 0.0, 1.0, nu, 0.0, 0.0);
  };
  options.dirichlet = [nu](const Vec3& x, Real t, int) {
    return exact2D(x.x, x.y, t, 1.0, nu, 0.0, 0.0);
  };
  options.exact = [nu](const Vec3& x, Real t) {
    return exact2D(x.x, x.y, t, 1.0, nu, 0.0, 0.0);
  };
  return options;
}

double travelingError2D(int n) {
  Mesh mesh = test::makeUniformBoxMesh(n, n, 1, {-1, -1, 0}, {1, 1, 1});
  SaturnBurger solver(std::move(mesh), options2D(0.02));
  REQUIRE(solver.dimension() == 2);

  TimeControl control;
  control.tEnd = 0.5;
  control.dtMax = 0.005;
  control.cfl = 0.3;
  control.diffusionSafety = 0.5;

  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);
  return solver.exactError(control.tEnd).l2;
}

}  // namespace

TEST_CASE("saturnBurger: 2D diagonal traveling wave converges") {
  const double e32 = travelingError2D(32);
  const double e64 = travelingError2D(64);
  const double order = std::log(e32 / e64) / std::log(2.0);
  MESSAGE("Burgers 2D diagonal wave L2: " << e32 << " " << e64
                                          << " order=" << order);
  CHECK(e64 < e32);
  CHECK(order > 1.7);
  CHECK(e64 < 3e-3);
}
