#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <utility>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

#include "simulation/Simulation.hpp"
#include "solvers/saturnBurger/SaturnBurger.hpp"

#include "UniformMesh.hpp"

using namespace saturn;

namespace {

constexpr double kPi = 3.14159265358979323846;

double travelingWave(double x, double t, double speed, double nu) {
  return 0.5 * speed *
         (1.0 - std::tanh(speed * (x - 0.5 * speed * t) / (4.0 * nu)));
}

double l2Error(const SaturnBurger& solver, double time, double speed,
               double nu) {
  const CellField& state = solver.state();
  const Mesh& mesh = solver.mesh();
  auto host = state.hostView();
  double local = 0;
  for (int k = state.origin(2); k < state.origin(2) + state.extent(2); ++k) {
    for (int j = state.origin(1); j < state.origin(1) + state.extent(1); ++j) {
      for (int i = state.origin(0); i < state.origin(0) + state.extent(0);
           ++i) {
        const int cell = mesh.cellAt(i, j, k);
        const int ii = i - (state.origin(0) - state.nGrow());
        const int jj = j - (state.origin(1) - state.nGrow());
        const int kk = k - (state.origin(2) - state.nGrow());
        const double error =
            host(ii, jj, kk, 0) -
            travelingWave(mesh.cellCentroid[cell].x, time, speed, nu);
        local += error * error;
      }
    }
  }
  double global = local;
#ifdef SATURN_USE_MPI
  MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
  const double count = static_cast<double>(state.numOwned());
  return std::sqrt(global / count);
}

double integral(const SaturnBurger& solver) {
  const CellField& state = solver.state();
  const Mesh& mesh = solver.mesh();
  auto host = state.hostView();
  double local = 0;
  for (int k = state.origin(2); k < state.origin(2) + state.extent(2); ++k) {
    for (int j = state.origin(1); j < state.origin(1) + state.extent(1); ++j) {
      for (int i = state.origin(0); i < state.origin(0) + state.extent(0);
           ++i) {
        const int cell = mesh.cellAt(i, j, k);
        const int ii = i - (state.origin(0) - state.nGrow());
        const int jj = j - (state.origin(1) - state.nGrow());
        const int kk = k - (state.origin(2) - state.nGrow());
        local += host(ii, jj, kk, 0) * mesh.cellVolume[cell];
      }
    }
  }
  double global = local;
#ifdef SATURN_USE_MPI
  MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
#endif
  return global;
}

struct Bounds {
  double minimum;
  double maximum;
};

Bounds bounds(const SaturnBurger& solver) {
  const CellField& state = solver.state();
  auto host = state.hostView();
  double minimum = 1e300;
  double maximum = -1e300;
  for (int k = state.origin(2); k < state.origin(2) + state.extent(2); ++k) {
    for (int j = state.origin(1); j < state.origin(1) + state.extent(1); ++j) {
      for (int i = state.origin(0); i < state.origin(0) + state.extent(0);
           ++i) {
        const int ii = i - (state.origin(0) - state.nGrow());
        const int jj = j - (state.origin(1) - state.nGrow());
        const int kk = k - (state.origin(2) - state.nGrow());
        minimum = std::min(minimum, host(ii, jj, kk, 0));
        maximum = std::max(maximum, host(ii, jj, kk, 0));
      }
    }
  }
#ifdef SATURN_USE_MPI
  double globalMin = 0;
  double globalMax = 0;
  MPI_Allreduce(&minimum, &globalMin, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  MPI_Allreduce(&maximum, &globalMax, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  return {globalMin, globalMax};
#else
  return {minimum, maximum};
#endif
}

BurgersOptions travelingOptions(double nu) {
  const double speed = 1.0;
  BurgersOptions options;
  options.nu = nu;
  options.periodic = false;
  options.convection = ConvectiveScheme::Muscl;
  options.limiter = Limiter::VanLeer;
  options.initial = [speed, nu](const Vec3& x) {
    return travelingWave(x.x, 0, speed, nu);
  };
  options.dirichlet = [speed, nu](const Vec3& x, Real time, int) {
    return travelingWave(x.x, time, speed, nu);
  };
  return options;
}

double travelingError(int nx, double nu) {
  Mesh mesh = test::makeUniformBoxMesh(nx, 1, 1, {-1, 0, 0}, {1, 1, 1});
  SaturnBurger solver(std::move(mesh), travelingOptions(nu));
  TimeControl control;
  control.tEnd = 0.5;
  control.dtMax = 0.01;
  control.cfl = 0.3;
  control.diffusionSafety = 0.5;
  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);
  return l2Error(solver, control.tEnd, 1.0, nu);
}

}  // namespace

TEST_CASE("saturnBurger: viscous traveling wave converges under refinement") {
  const double e64 = travelingError(64, 0.02);
  const double e128 = travelingError(128, 0.02);
  const double order = std::log(e64 / e128) / std::log(2.0);
  MESSAGE("Burgers traveling wave L2: " << e64 << " " << e128
                                        << " order=" << order);
  CHECK(e128 < e64);
  CHECK(order > 1.5);
  CHECK(e128 < 2e-2);
}

TEST_CASE("saturnBurger: constant field is preserved") {
  const int nx = 32;
  Mesh mesh = test::makeUniformBoxMesh(nx, 1, 1, {-1, 0, 0}, {1, 1, 1});
  BurgersOptions options;
  options.nu = 0;
  options.periodic = true;
  options.initial = [](const Vec3&) { return 1.0; };
  SaturnBurger solver(std::move(mesh), options);

  TimeControl control;
  control.tEnd = 0.1;
  control.dtMax = 0.01;
  control.cfl = 0.3;
  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);
  MESSAGE("constant Burgers linf=" << result.final.linf);
  CHECK(result.final.linf == doctest::Approx(1.0).epsilon(1e-10));
}

TEST_CASE("saturnBurger: periodic inviscid shock is conservative and bounded") {
  const int nx = 128;
  Mesh mesh = test::makeUniformBoxMesh(nx, 1, 1, {-1, 0, 0}, {1, 1, 1});
  BurgersOptions options;
  options.nu = 0;
  options.periodic = true;
  options.convection = ConvectiveScheme::Muscl;
  options.limiter = Limiter::VanLeer;
  options.initial = [](const Vec3& x) { return -std::sin(kPi * x.x); };
  SaturnBurger solver(std::move(mesh), options);

  TimeControl control;
  control.tEnd = 0.5;
  control.dtMax = 0.01;
  control.cfl = 0.3;
  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  REQUIRE(result.reachedEnd);

  const Bounds range = bounds(solver);
  MESSAGE("Burgers shock bounds: " << range.minimum << " " << range.maximum);
  MESSAGE("Burgers shock integral: " << integral(solver));
  CHECK(range.minimum >= -1.01);
  CHECK(range.maximum <= 1.01);
  CHECK(std::abs(integral(solver)) < 1e-8);
}
