#include <doctest/doctest.h>

#include <cmath>
#include <utility>

#include "simulation/Simulation.hpp"
#include "solvers/saturnBurger/SaturnBurger.hpp"

#include "UniformMesh.hpp"

using namespace saturn;

namespace {

double exact3D(double x, double y, double z, double t, double speed, double nu,
               double x0, double y0, double z0) {
  return 0.5 * speed *
         (1.0 - std::tanh(speed * (x + y + z - x0 - y0 - z0 - 1.5 * speed * t) /
                          (4.0 * nu)));
}

BurgersOptions options3D(double nu) {
  BurgersOptions options;
  options.nu = nu;
  options.periodic = false;
  options.convection = ConvectiveScheme::Muscl;
  options.limiter = Limiter::VanLeer;
  options.initial = [nu](const Vec3& x) {
    return exact3D(x.x, x.y, x.z, 0.0, 1.0, nu, 0.0, 0.0, 0.0);
  };
  options.dirichlet = [nu](const Vec3& x, Real t, int) {
    return exact3D(x.x, x.y, x.z, t, 1.0, nu, 0.0, 0.0, 0.0);
  };
  options.exact = [nu](const Vec3& x, Real t) {
    return exact3D(x.x, x.y, x.z, t, 1.0, nu, 0.0, 0.0, 0.0);
  };
  return options;
}

double travelingError3D(int n) {
  Mesh mesh = test::makeUniformBoxMesh(n, n, n, {-1, -1, -1}, {1, 1, 1});
  SaturnBurger solver(std::move(mesh), options3D(0.2));
  REQUIRE(solver.dimension() == 3);

  TimeControl control;
  control.tEnd = 0.25;
  control.dtMax = 0.01;
  control.cfl = 0.3;
  control.diffusionSafety = 0.5;

  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);
  return solver.exactError(control.tEnd).l2;
}

double exactPlaneX(double x, double t, double speed, double nu, double x0) {
  return 0.5 * speed *
         (1.0 - std::tanh(speed * (x - x0 - 0.5 * speed * t) / (4.0 * nu)));
}

BurgersOptions optionsPlaneX(double nu) {
  BurgersOptions options;
  options.nu = nu;
  options.periodic = false;
  options.convection = ConvectiveScheme::Muscl;
  options.limiter = Limiter::VanLeer;
  options.initial = [nu](const Vec3& x) {
    return exactPlaneX(x.x, 0.0, 1.0, nu, 0.0);
  };
  options.dirichlet = [nu](const Vec3& x, Real t, int) {
    return exactPlaneX(x.x, t, 1.0, nu, 0.0);
  };
  options.exact = [nu](const Vec3& x, Real t) {
    return exactPlaneX(x.x, t, 1.0, nu, 0.0);
  };
  return options;
}

double planeError3D(int n) {
  Mesh mesh = test::makeUniformBoxMesh(n, n, n, {-1, -1, -1}, {1, 1, 1});
  SaturnBurger solver(std::move(mesh), optionsPlaneX(0.2));
  REQUIRE(solver.dimension() == 3);

  TimeControl control;
  control.tEnd = 0.25;
  control.dtMax = 0.01;
  control.cfl = 0.3;
  control.diffusionSafety = 0.5;

  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);
  return solver.exactError(control.tEnd).l2;
}
}  // namespace

TEST_CASE("saturnBurger: 3D plane wave converges") {
  const double e8 = planeError3D(8);
  const double e16 = planeError3D(16);
  const double order = std::log(e8 / e16) / std::log(2.0);
  MESSAGE("Burgers 3D x-plane wave L2: " << e8 << " " << e16
                                         << " order=" << order);
  CHECK(e16 < e8);
  CHECK(order > 1.3);
  CHECK(e16 < 2e-3);
}

TEST_CASE("saturnBurger: 3D diagonal wave decreases under refinement") {
  const double e8 = travelingError3D(8);
  const double e16 = travelingError3D(16);
  MESSAGE("Burgers 3D diagonal wave L2: " << e8 << " " << e16);
  CHECK(e16 < e8);
  CHECK(e16 < 3e-3);
}
