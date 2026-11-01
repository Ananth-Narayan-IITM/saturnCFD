#include <doctest/doctest.h>

#include <cmath>
#include <utility>

#include "simulation/SteadySolver.hpp"
#include "solvers/saturnStokes/SaturnStokes.hpp"

#include "UniformMesh.hpp"

using namespace saturn;

namespace {

struct StokesCase {
  Mesh mesh;
  StokesOptions options;
};

StokesCase poiseuille(int nx, int ny, int nz, Real mu, Real G) {
  StokesCase c;
  c.mesh = test::makeUniformBoxMesh(nx, ny, nz, {0, 0, 0}, {1, 1, 1});
  c.options.mu = mu;
  c.options.force = {G, 0, 0};
  c.options.periodicX = true;
  c.options.periodicY = false;
  c.options.periodicZ = true;
  c.options.direct = true;
  c.options.exactVelocity = [mu, G](const Vec3& x) {
    return Vec3{G / (2 * mu) * x.y * (1 - x.y), 0, 0};
  };
  c.options.exactPressure = [](const Vec3&) { return Real(0); };
  return c;
}

StokesCase couette(int nx, int ny, int nz, Real mu, Real U) {
  StokesCase c;
  c.mesh = test::makeUniformBoxMesh(nx, ny, nz, {0, 0, 0}, {1, 1, 1});
  c.options.mu = mu;
  c.options.periodicX = true;
  c.options.periodicY = false;
  c.options.periodicZ = true;
  c.options.wallTopU = U;
  c.options.direct = true;
  c.options.exactVelocity = [U](const Vec3& x) { return Vec3{U * x.y, 0, 0}; };
  c.options.exactPressure = [](const Vec3&) { return Real(0); };
  return c;
}

VectorError solve(StokesCase c) {
  SaturnStokes solver(std::move(c.mesh), std::move(c.options));
  const SteadyResult result = solver.solveSteady();
  REQUIRE(result.converged);
  return solver.exactError();
}

}  // namespace

TEST_CASE("saturnStokes: 2D Poiseuille is second order") {
  const VectorError e16 = solve(poiseuille(16, 16, 1, 1.0, 1.0));
  const VectorError e32 = solve(poiseuille(32, 32, 1, 1.0, 1.0));
  MESSAGE("Stokes 2D Poiseuille L2: " << e16.l2 << " " << e32.l2);
  CHECK(e32.l2 < e16.l2);
  CHECK(e32.l2 < e16.l2 / 3.5);
  CHECK(e16.pL2 < 1e-10);
  CHECK(e32.pL2 < 1e-10);
}

TEST_CASE("saturnStokes: 2D Couette is exact") {
  const VectorError e = solve(couette(24, 24, 1, 1.0, 1.0));
  MESSAGE("Stokes 2D Couette L2: " << e.l2 << " pL2 " << e.pL2);
  CHECK(e.l2 < 1e-10);
  CHECK(e.pL2 < 1e-10);
}

TEST_CASE("saturnStokes: 3D Poiseuille on a periodic extrusion") {
  const VectorError e16 = solve(poiseuille(16, 16, 4, 1.0, 1.0));
  const VectorError e24 = solve(poiseuille(24, 24, 4, 1.0, 1.0));
  MESSAGE("Stokes 3D Poiseuille L2: " << e16.l2 << " " << e24.l2);
  CHECK(e24.l2 < e16.l2);
  CHECK(e16.l2 < 1e-3);
  CHECK(e24.l2 < 5e-4);
  CHECK(e16.pL2 < 1e-10);
  CHECK(e24.pL2 < 1e-10);
}

TEST_CASE("saturnStokes: 3D Couette is exact") {
  const VectorError e = solve(couette(12, 12, 4, 1.0, 1.0));
  MESSAGE("Stokes 3D Couette L2: " << e.l2 << " pL2 " << e.pL2);
  CHECK(e.l2 < 1e-10);
  CHECK(e.pL2 < 1e-10);
}
