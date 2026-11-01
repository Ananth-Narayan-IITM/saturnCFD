#include <doctest/doctest.h>

#include <cmath>
#include <utility>

#include "simulation/Simulation.hpp"
#include "solvers/saturnHeat/SaturnHeat.hpp"

#include "UniformMesh.hpp"

using namespace saturn;

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;

double exactFourier(const Vec3& x, double t, double alpha, const Vec3& v,
                    const Vec3& k, double amplitude) {
  const double omega = v.x * k.x + v.y * k.y + v.z * k.z;
  const double decay = alpha * (k.x * k.x + k.y * k.y + k.z * k.z);
  return amplitude * std::sin(k.x * x.x + k.y * x.y + k.z * x.z - omega * t) *
         std::exp(-decay * t);
}

HeatOptions fourierOptions() {
  HeatOptions options;
  options.alpha = 0.01;
  options.velocity = {1.0, 0.5, 0.25};
  options.periodic = true;
  options.convection = ConvectiveScheme::Muscl;
  options.limiter = Limiter::VanLeer;
  const Vec3 k{2 * kPi, 2 * kPi, 2 * kPi};
  const double amplitude = 1.0;
  options.initial = [k, amplitude](const Vec3& x) {
    return exactFourier(x, 0.0, 0.01, {1.0, 0.5, 0.25}, k, amplitude);
  };
  options.exact = [k, amplitude](const Vec3& x, Real t) {
    return exactFourier(x, t, 0.01, {1.0, 0.5, 0.25}, k, amplitude);
  };
  return options;
}

double fourierError3D(int n) {
  Mesh mesh = test::makeUniformBoxMesh(n, n, n, {0, 0, 0}, {1, 1, 1});
  SaturnHeat solver(std::move(mesh), fourierOptions());
  REQUIRE(solver.dimension() == 3);

  TimeControl control;
  control.tEnd = 0.1;
  control.dtMax = 0.01;
  control.cfl = 0.3;
  control.diffusionSafety = 0.5;

  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);
  return solver.exactError(control.tEnd).l2;
}

}  // namespace

TEST_CASE("saturnHeat: 3D Fourier mode converges") {
  const double e16 = fourierError3D(16);
  const double e32 = fourierError3D(32);
  const double e64 = fourierError3D(64);
  const double o16 = std::log(e16 / e32) / std::log(2.0);
  const double o32 = std::log(e32 / e64) / std::log(2.0);
  MESSAGE("Heat 3D Fourier L2: " << e16 << " " << e32 << " " << e64
                                 << " orders=" << o16 << " " << o32);
  CHECK(e32 < e16);
  CHECK(e64 < e32);
  CHECK(o16 > 1.5);
  CHECK(o32 > 1.7);
  CHECK(e64 < 1e-3);
}

TEST_CASE("saturnHeat: constant field is preserved") {
  Mesh mesh = test::makeUniformBoxMesh(16, 16, 16, {0, 0, 0}, {1, 1, 1});
  HeatOptions options;
  options.alpha = 0.01;
  options.velocity = {1.0, 0.0, 0.0};
  options.periodic = true;
  options.initial = [](const Vec3&) { return 2.0; };
  SaturnHeat solver(std::move(mesh), options);

  TimeControl control;
  control.tEnd = 0.05;
  control.dtMax = 0.005;
  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);
  CHECK(result.final.linf == doctest::Approx(2.0).epsilon(1e-12));
}
