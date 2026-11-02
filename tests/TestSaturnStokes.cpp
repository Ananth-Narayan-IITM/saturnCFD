#include <doctest/doctest.h>

#include <cmath>
#include <utility>

#include "simulation/SteadySolver.hpp"
#include "solvers/saturnStokes/SaturnStokes.hpp"
#include "solvers/saturnStokes/StokesMms.hpp"

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

StokesCase mms(int nx, int ny, int nz, MmsVariant variant) {
  StokesCase c;
  c.mesh = test::makeUniformBoxMesh(nx, ny, nz, {0, 0, 0}, {1, 1, 1});
  c.options.mu = 1.0;
  c.options.periodicX = true;
  c.options.periodicZ = true;
  c.options.periodicY = (variant == MmsVariant::Periodic);
  c.options.direct = true;
  const int dim = (nz == 1) ? 2 : 3;
  const Real hx = Real(1) / static_cast<Real>(nx);
  const Real hy = Real(1) / static_cast<Real>(ny);
  const Real hz = (dim == 3) ? Real(1) / static_cast<Real>(nz) : hy;
  const Real hMin = std::min(hx, std::min(hy, hz));
  const Real beta = c.options.stabilization * hMin * hMin / c.options.mu;
  const Real mu = c.options.mu;
  c.options.bodyForce = [dim, variant, mu](const Vec3& x) {
    return mmsForce(x, dim, variant, mu);
  };
  c.options.continuitySource = [dim, variant, beta](const Vec3& x) {
    return -beta * mmsPressureLaplacian(x, dim, variant);
  };
  c.options.exactVelocity = [dim, variant](const Vec3& x) {
    return mmsVelocity(x, dim, variant);
  };
  c.options.exactPressure = [dim, variant](const Vec3& x) {
    return mmsPressure(x, dim, variant);
  };
  return c;
}

VectorError solve(StokesCase c) {
  SaturnStokes solver(std::move(c.mesh), std::move(c.options));
  const SteadyResult result = solver.solveSteady();
  REQUIRE(result.converged);
  return solver.exactError();
}

/// Fully periodic Stokes is defined only up to a constant velocity, so the
/// comparison removes the mean velocity error before taking the norms.
VectorError solvePeriodic(StokesCase c) {
  SaturnStokes solver(std::move(c.mesh), std::move(c.options));
  const SteadyResult result = solver.solveSteady();
  REQUIRE(result.converged);
  const std::vector<VectorProfilePoint> points = solver.profile(true);
  VectorError out;
  out.samples = static_cast<BigIndex>(points.size());
  if (points.empty())
    return out;
  double mu = 0, mv = 0, mw = 0;
  for (const VectorProfilePoint& p : points) {
    mu += static_cast<double>(p.u - p.uExact);
    mv += static_cast<double>(p.v - p.vExact);
    mw += static_cast<double>(p.w - p.wExact);
  }
  const double inv = 1.0 / static_cast<double>(points.size());
  mu *= inv;
  mv *= inv;
  mw *= inv;
  double l1 = 0, l2 = 0, linf = 0, pMean = 0;
  for (const VectorProfilePoint& p : points) {
    const double du = static_cast<double>(p.u - p.uExact) - mu;
    const double dv = static_cast<double>(p.v - p.vExact) - mv;
    const double dw = static_cast<double>(p.w - p.wExact) - mw;
    const double e = std::sqrt(du * du + dv * dv + dw * dw);
    l1 += e;
    l2 += e * e;
    linf = std::max(linf, e);
    pMean += static_cast<double>(p.p - p.pExact);
  }
  pMean *= inv;
  double pL2 = 0, pLinf = 0;
  for (const VectorProfilePoint& p : points) {
    const double e = static_cast<double>(p.p - p.pExact) - pMean;
    pL2 += e * e;
    pLinf = std::max(pLinf, std::abs(e));
  }
  out.l1 = static_cast<Real>(l1 * inv);
  out.l2 = static_cast<Real>(std::sqrt(l2 * inv));
  out.linf = static_cast<Real>(linf);
  out.pL2 = static_cast<Real>(std::sqrt(pL2 * inv));
  out.pLinf = static_cast<Real>(pLinf);
  return out;
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

TEST_CASE("saturnStokes: fieldsplit matches direct") {
  const VectorError d2 = solve(poiseuille(32, 32, 1, 1.0, 1.0));
  StokesCase f2 = poiseuille(32, 32, 1, 1.0, 1.0);
  f2.options.direct = false;
  const VectorError s2 = solve(std::move(f2));
  MESSAGE("Stokes 2D fieldsplit L2: " << s2.l2 << " direct " << d2.l2);
  CHECK(std::abs(s2.l2 - d2.l2) < 1e-8);
  CHECK(s2.l2 < 1.05 * d2.l2);

  StokesCase f3 = poiseuille(16, 16, 4, 1.0, 1.0);
  f3.options.direct = false;
  const VectorError s3 = solve(std::move(f3));
  MESSAGE("Stokes 3D fieldsplit L2: " << s3.l2);
  CHECK(s3.l2 < 1e-3);
}

TEST_CASE("saturnStokes: periodic MMS is second order") {
  const VectorError e8 = solvePeriodic(mms(8, 8, 8, MmsVariant::Periodic));
  const VectorError e16 = solvePeriodic(mms(16, 16, 16, MmsVariant::Periodic));
  const Real orderU = std::log2(e8.l2 / e16.l2);
  const Real orderP = std::log2(e8.pL2 / e16.pL2);
  MESSAGE("Stokes periodic MMS 3D: L2 " << e8.l2 << " " << e16.l2 << " order "
                                        << orderU << " pL2 order " << orderP);
  CHECK(orderU > 1.8);
  // The pressure gauge pin and the stabilisation limit the asymptotic
  // pressure order on these coarse meshes; velocity is the primary check.
  CHECK(orderP > 1.5);

  const VectorError e2d16 = solvePeriodic(mms(16, 16, 1, MmsVariant::Periodic));
  const VectorError e2d32 = solvePeriodic(mms(32, 32, 1, MmsVariant::Periodic));
  const Real order2dU = std::log2(e2d16.l2 / e2d32.l2);
  const Real order2dP = std::log2(e2d16.pL2 / e2d32.pL2);
  MESSAGE("Stokes periodic MMS 2D: L2 " << e2d16.l2 << " " << e2d32.l2
                                        << " order " << order2dU
                                        << " pL2 order " << order2dP);
  CHECK(order2dU > 1.8);
  CHECK(order2dP > 1.5);
}

TEST_CASE("saturnStokes: MMS sources satisfy the PDE by finite differences") {
  const Real eps = 1e-5;
  const Real mu = 1.7;
  const Vec3 x0{0.31, 0.27, 0.19};
  const auto comp = [](const Vec3& v, int c) {
    return c == 0 ? v.x : (c == 1 ? v.y : v.z);
  };
  for (MmsVariant variant : {MmsVariant::Periodic, MmsVariant::Wall}) {
    for (int dim : {2, 3}) {
      const auto velocity = [&](const Vec3& x) {
        return mmsVelocity(x, dim, variant);
      };
      const auto pressure = [&](const Vec3& x) {
        return mmsPressure(x, dim, variant);
      };
      Vec3 lap{0, 0, 0};
      Vec3 grad{0, 0, 0};
      for (int dir = 0; dir < dim; ++dir) {
        Vec3 xp = x0, xm = x0;
        Real* pp = dir == 0 ? &xp.x : (dir == 1 ? &xp.y : &xp.z);
        Real* pm = dir == 0 ? &xm.x : (dir == 1 ? &xm.y : &xm.z);
        *pp += eps;
        *pm -= eps;
        const Vec3 up = velocity(xp), um = velocity(xm);
        const Vec3 u0 = velocity(x0);
        for (int c = 0; c < dim; ++c) {
          const Real second =
              (comp(up, c) - 2 * comp(u0, c) + comp(um, c)) / (eps * eps);
          if (c == 0)
            lap.x += second;
          else if (c == 1)
            lap.y += second;
          else
            lap.z += second;
        }
        const Real gradVal = (pressure(xp) - pressure(xm)) / (2 * eps);
        if (dir == 0)
          grad.x = gradVal;
        else if (dir == 1)
          grad.y = gradVal;
        else
          grad.z = gradVal;
      }
      const Vec3 f = mmsForce(x0, dim, variant, mu);
      const Real tol = 2e-3 * (1 + std::abs(f.x) + std::abs(f.y));
      CHECK(std::abs(f.x - (-mu * lap.x + grad.x)) < tol);
      CHECK(std::abs(f.y - (-mu * lap.y + grad.y)) < tol);
      if (dim == 3)
        CHECK(std::abs(f.z - (-mu * lap.z + grad.z)) < tol);

      // The manufactured velocity is divergence-free.
      Real div = 0;
      for (int dir = 0; dir < dim; ++dir) {
        Vec3 xp = x0, xm = x0;
        Real* pp = dir == 0 ? &xp.x : (dir == 1 ? &xp.y : &xp.z);
        Real* pm = dir == 0 ? &xm.x : (dir == 1 ? &xm.y : &xm.z);
        *pp += eps;
        *pm -= eps;
        const Vec3 up = velocity(xp), um = velocity(xm);
        div += (comp(up, dir) - comp(um, dir)) / (2 * eps);
      }
      CHECK(std::abs(div) < 1e-4);
    }
  }
}

TEST_CASE("saturnStokes: wall MMS is second order in 2D") {
  const VectorError e16 = solve(mms(16, 16, 1, MmsVariant::Wall));
  const VectorError e32 = solve(mms(32, 32, 1, MmsVariant::Wall));
  const Real orderU = std::log2(e16.l2 / e32.l2);
  const Real orderP = std::log2(e16.pL2 / e32.pL2);
  MESSAGE("Stokes wall MMS 2D: L2 " << e16.l2 << " " << e32.l2 << " order "
                                    << orderU << " pL2 order " << orderP);
  CHECK(orderU > 1.8);
  CHECK(orderP > 1.5);
  // Absolute bounds catch the top-wall sign error even when the coarse pair
  // happens to look superconvergent.
  CHECK(e16.l2 < 0.1);
  CHECK(e32.l2 < 0.02);
  CHECK(e16.pL2 < 0.3);
  CHECK(e32.pL2 < 0.06);
}

TEST_CASE("saturnStokes: periodic MMS fieldsplit matches direct") {
  StokesCase f2 = mms(16, 16, 1, MmsVariant::Periodic);
  f2.options.direct = false;
  const VectorError e2 = solvePeriodic(std::move(f2));
  MESSAGE("Stokes periodic MMS 2D fieldsplit L2: " << e2.l2 << " pL2 "
                                                   << e2.pL2);
  CHECK(e2.l2 < 0.02);
  CHECK(e2.pL2 < 0.02);

  StokesCase f3 = mms(8, 8, 8, MmsVariant::Periodic);
  f3.options.direct = false;
  const VectorError e3 = solvePeriodic(std::move(f3));
  MESSAGE("Stokes periodic MMS 3D fieldsplit L2: " << e3.l2 << " pL2 "
                                                   << e3.pL2);
  CHECK(e3.l2 < 0.05);
  CHECK(e3.pL2 < 0.05);
}
