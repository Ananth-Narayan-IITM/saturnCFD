#include "solver/CavitySolver.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "linalg/Poisson.hpp"

namespace saturn {

CavitySolver::CavitySolver(const Geometry& geom, const CavityParams& prm)
    : m_geom(geom),
      m_n(geom.nCells(0)),
      m_h(geom.dx),
      m_nu(prm.lidVelocity / (prm.reynolds * geom.length(0))),
      m_prm(prm) {
  if (NDIM != 2)
    throw std::runtime_error(
        "CavitySolver: staggered implementation is 2D-only for now");
  if (m_geom.domain.lo[0] != 0 || m_geom.domain.lo[1] != 0)
    throw std::runtime_error("CavitySolver: domain must start at index 0");

  Idx patch{};
  patch[0] = m_n;
  patch[1] = m_n;
  const BoxArray ba = BoxArray::tileDomain(m_geom.domain, patch);
  if (ba.size() != 1)
    throw std::runtime_error("CavitySolver: expected a single box");
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

  m_u = MultiFab(ba, dm, 2, 1, 0, "u");
  m_ut = MultiFab(ba, dm, 2, 1, 0, "ut");
  m_p = MultiFab(ba, dm, 1, 1, 0, "p");
  m_div = MultiFab(ba, dm, 1, 1, 0, "div");
  m_rhs = MultiFab(ba, dm, 1, 1, 0, "rhs");

  m_halo = HaloExchange(ba, dm, m_geom, 1, 0, 1);
  m_halo.build();
}

void CavitySolver::initialize() {
  m_u.setVal(Real(0));
  m_ut.setVal(Real(0));
  m_p.setVal(Real(0));
  applyVelocityBC();
  m_steps = 0;
  m_time = 0;
}

// ---------------------------------------------------------------------------
// Boundary conditions
// ---------------------------------------------------------------------------
// Normal velocities are prescribed on the walls; tangential ones are imposed
// through ghost cells so that the discrete shear at the wall is correct.
//
//   bottom (j = 0)   : u ghost at j = -1  =  -u(j=0)          (no-slip)
//   top lid (j = n)  : u ghost at j = n   =  2*Ulid - u(n-1)  (moving wall)
//   sides (i = 0,n)  : v ghost mirror      =  -v
//
void CavitySolver::applyVelocityBC() {
  const int n = m_n;
  const Real lid = m_prm.lidVelocity;
  const Array4<Real> U = m_u.array(0);

  // Normal velocity on all four walls.
  Kokkos::parallel_for(
      "cavity::wallNormal", Kokkos::RangePolicy<>(0, n),
      KOKKOS_LAMBDA(int k) {
        U(0, k, 0, 0) = Real(0);  // west wall
        U(n, k, 0, 0) = Real(0);  // east wall
        U(k, 0, 0, 1) = Real(0);  // south wall
        U(k, n, 0, 1) = Real(0);  // north wall
      });

  // Tangential ghost cells (interior columns/rows only; the corners are never
  // touched by the stencil).
  Kokkos::parallel_for(
      "cavity::wallTangential", Kokkos::RangePolicy<>(0, n + 1),
      KOKKOS_LAMBDA(int i) {
        if (i >= 1 && i <= n - 1) {
          U(i, -1, 0, 0) = -U(i, 0, 0, 0);              // bottom, no-slip
          U(i, n, 0, 0) = 2.0 * lid - U(i, n - 1, 0, 0); // lid, moving
        }
        if (i >= 0 && i <= n - 1) {
          U(-1, i, 0, 1) = -U(0, i, 0, 1);   // west, no-slip
          U(n, i, 0, 1) = -U(n - 1, i, 0, 1);  // east, no-slip
        }
      });
}

// ---------------------------------------------------------------------------
// Momentum predictor
// ---------------------------------------------------------------------------
void CavitySolver::predict(Real dt) {
  const int n = m_n;
  const Real h = m_h;
  const Real h2 = h * h;
  const Real nu = m_nu;

  Kokkos::deep_copy(m_ut.data(), m_u.data());  // walls carried over

  const Array4<Real> U = m_u.array(0);
  const Array4<Real> Ut = m_ut.array(0);

  // ---- u-momentum, on interior x-faces --------------------------------
  Kokkos::parallel_for(
      "cavity::uMomentum", Kokkos::RangePolicy<>(0, (n - 1) * n),
      KOKKOS_LAMBDA(int idx) {
        const int i = 1 + idx % (n - 1);
        const int j = idx / (n - 1);

        const Real uc = U(i, j, 0, 0);

        // x-direction fluxes at faces i and i+1
        const Real uEast = U(i + 1, j, 0, 0);
        const Real fluxE =
            (i + 1 >= n)
                ? Real(0)
                : uEast * ((uEast > 0) ? uc : U(i + 2, j, 0, 0));
        const Real fluxW = uc * ((uc > 0) ? U(i - 1, j, 0, 0) : uEast);

        // y-direction fluxes: v interpolated onto this x-face
        const Real vS = 0.5 * (U(i - 1, j, 0, 1) + U(i, j, 0, 1));
        const Real vN = 0.5 * (U(i - 1, j + 1, 0, 1) + U(i, j + 1, 0, 1));
        const Real fluxN = vN * ((vN > 0) ? U(i, j, 0, 0) : U(i, j + 1, 0, 0));
        const Real fluxS = vS * ((vS > 0) ? U(i, j - 1, 0, 0) : U(i, j, 0, 0));

        const Real conv = (fluxE - fluxW) / h + (fluxN - fluxS) / h;
        const Real lap = (U(i + 1, j, 0, 0) - 2 * uc + U(i - 1, j, 0, 0)) / h2 +
                         (U(i, j + 1, 0, 0) - 2 * uc + U(i, j - 1, 0, 0)) / h2;

        Ut(i, j, 0, 0) = uc + dt * (-conv + nu * lap);
      });

  // ---- v-momentum, on interior y-faces --------------------------------
  Kokkos::parallel_for(
      "cavity::vMomentum", Kokkos::RangePolicy<>(0, n * (n - 1)),
      KOKKOS_LAMBDA(int idx) {
        const int i = idx % n;
        const int j = 1 + idx / n;

        const Real vc = U(i, j, 0, 1);

        // y-direction fluxes at faces j and j+1
        const Real vNorth = U(i, j + 1, 0, 1);
        const Real fluxN =
            (j + 1 >= n)
                ? Real(0)
                : vNorth * ((vNorth > 0) ? vc : U(i, j + 2, 0, 1));
        const Real fluxS = vc * ((vc > 0) ? U(i, j - 1, 0, 1) : vNorth);

        // x-direction fluxes: u interpolated onto this y-face
        const Real uW = 0.5 * (U(i, j - 1, 0, 0) + U(i, j, 0, 0));
        const Real uE = 0.5 * (U(i + 1, j - 1, 0, 0) + U(i + 1, j, 0, 0));
        const Real fluxE =
            (i + 1 >= n)
                ? Real(0)
                : uE * ((uE > 0) ? U(i, j, 0, 1) : U(i + 1, j, 0, 1));
        const Real fluxW = uW * ((uW > 0) ? U(i - 1, j, 0, 1) : U(i, j, 0, 1));

        const Real conv = (fluxE - fluxW) / h + (fluxN - fluxS) / h;
        const Real lap = (U(i + 1, j, 0, 1) - 2 * vc + U(i - 1, j, 0, 1)) / h2 +
                         (U(i, j + 1, 0, 1) - 2 * vc + U(i, j - 1, 0, 1)) / h2;

        Ut(i, j, 0, 1) = vc + dt * (-conv + nu * lap);
      });
}

void CavitySolver::computeDivergence(const MultiFab& vel,
                                     MultiFab& div) const {
  const Real h = m_h;
  const Array4<Real> U = vel.array(0);

  div.forEachCellFlat(
      KOKKOS_LAMBDA(int, Array4<Real> D, int i, int j, int k) {
        (void)k;
        D(i, j, 0, 0) = (U(i + 1, j, 0, 0) - U(i, j, 0, 0)) / h +
                        (U(i, j + 1, 0, 1) - U(i, j, 0, 1)) / h;
      });
}

void CavitySolver::correct(Real dt) {
  const int n = m_n;
  const Real h = m_h;
  const Array4<Real> Ut = m_ut.array(0);
  const Array4<Real> U = m_u.array(0);
  const Array4<Real> P = m_p.array(0);

  // u-faces: i in [0,n]. At i=0 and i=n the pressure ghosts are the interior
  // values (homogeneous Neumann), so the wall velocity is untouched.
  Kokkos::parallel_for(
      "cavity::correctU", Kokkos::RangePolicy<>(0, (n + 1) * n),
      KOKKOS_LAMBDA(int idx) {
        const int i = idx % (n + 1);
        const int j = idx / (n + 1);
        const Real pe = (i < n) ? P(i, j, 0, 0) : P(n - 1, j, 0, 0);
        const Real pw = (i > 0) ? P(i - 1, j, 0, 0) : P(0, j, 0, 0);
        U(i, j, 0, 0) = Ut(i, j, 0, 0) - dt * (pe - pw) / h;
      });

  // v-faces: j in [0,n]
  Kokkos::parallel_for(
      "cavity::correctV", Kokkos::RangePolicy<>(0, n * (n + 1)),
      KOKKOS_LAMBDA(int idx) {
        const int i = idx % n;
        const int j = idx / n;
        const Real pn = (j < n) ? P(i, j, 0, 0) : P(i, n - 1, 0, 0);
        const Real ps = (j > 0) ? P(i, j - 1, 0, 0) : P(i, 0, 0, 0);
        U(i, j, 0, 1) = Ut(i, j, 0, 1) - dt * (pn - ps) / h;
      });
}

Real CavitySolver::maxSpeed() const {
  const int n = m_n;
  const Array4<Real> U = m_u.array(0);

  Real m0 = Real(0);
  Kokkos::Max<Real> red(m0);
  Kokkos::parallel_reduce(
      "cavity::maxSpeed", Kokkos::RangePolicy<>(0, (n + 1) * n + n * (n + 1)),
      KOKKOS_LAMBDA(int idx, Real& mx) {
        Real a = Real(0);
        if (idx < (n + 1) * n) {
          const int i = idx % (n + 1);
          const int j = idx / (n + 1);
          a = Kokkos::abs(U(i, j, 0, 0));
        } else {
          const int t = idx - (n + 1) * n;
          const int i = t % n;
          const int j = t / n;
          a = Kokkos::abs(U(i, j, 0, 1));
        }
        if (a > mx) mx = a;
      },
      red);
  return red.reference();
}

Real CavitySolver::maxDivergence() const {
  MultiFab d(m_u.boxArray(), m_u.distributionMap(), 1, 0, 0, "d");
  computeDivergence(m_u, d);
  Real m0 = Real(0);
  Kokkos::Max<Real> red(m0);
  const Array4<Real> D = d.array(0);
  Kokkos::parallel_reduce(
      "cavity::maxDiv", Kokkos::RangePolicy<>(0, m_n * m_n),
      KOKKOS_LAMBDA(int idx, Real& mx) {
        const int i = idx % m_n;
        const int j = idx / m_n;
        const Real a = Kokkos::abs(D(i, j, 0, 0));
        if (a > mx) mx = a;
      },
      red);
  return red.reference();
}

Real CavitySolver::stableDt() const {
  const Real umax = maxSpeed();
  const Real dtConv = (umax > 0) ? m_prm.cfl * m_h / umax : Real(1e30);
  const Real dtVisc = m_prm.diffusionFactor * m_h * m_h / m_nu;
  return Kokkos::min(dtConv, dtVisc);
}

void CavitySolver::step(Real dt) {
  predict(dt);
  computeDivergence(m_ut, m_div);  // divergence of the PREDICTOR, not of u^n

  // rhs = -div(u*)/dt ; then  -lap(phi) = rhs
  const Real invdt = Real(1) / dt;
  const Array4<Real> D = m_div.array(0);
  m_rhs.forEachCellFlat(
      KOKKOS_LAMBDA(int, Array4<Real> R, int i, int j, int k) {
        (void)k;
        R(i, j, 0, 0) = -D(i, j, 0, 0) * invdt;
      });

  PoissonParams pp;
  pp.bc = BCType::Neumann;
  pp.maxIter = m_prm.poissonMaxIter;
  pp.relTol = m_prm.poissonTol;
  pp.warmStart = true;  // pressure changes little between steps
  solvePoisson(m_p, m_rhs, m_geom, m_halo, pp);

  correct(dt);
  applyVelocityBC();

  m_steps++;
  m_time += dt;
}

SteadyReport CavitySolver::runToSteady(Real tEnd, int maxSteps, Real tol,
                                       int checkEvery) {
  SteadyReport rep;

  MultiFab prev(m_u.boxArray(), m_u.distributionMap(), 2, 1, 0, "prev");

  while (m_time < tEnd && rep.steps < maxSteps) {
    const Real dt = stableDt();
    step(dt);
    rep.steps++;

    if (rep.steps % checkEvery == 0) {
      // max |u_new - u_prev|
      const Array4<Real> U = m_u.array(0);
      const Array4<Real> V = prev.array(0);
      Real m0 = Real(0);
      Kokkos::Max<Real> red(m0);
      Kokkos::parallel_reduce(
          "cavity::delta", Kokkos::RangePolicy<>(0, (m_n + 1) * m_n),
          KOKKOS_LAMBDA(int idx, Real& mx) {
            const int i = idx % (m_n + 1);
            const int j = idx / (m_n + 1);
            const Real d = Kokkos::abs(U(i, j, 0, 0) - V(i, j, 0, 0));
            if (d > mx) mx = d;
          },
          red);
      rep.maxDelta = red.reference();
      Kokkos::deep_copy(prev.data(), m_u.data());
      rep.converged = (rep.maxDelta < tol);
      if (rep.converged) break;
    }
  }
  rep.time = m_time;
  Kokkos::deep_copy(prev.data(), m_u.data());
  return rep;
}

void CavitySolver::writeCentreLines(const std::string& path) const {
  const int n = m_n;
  // Copy to host with the same flat layout as Array4.
  auto h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), m_u.data());
  const int nx = n + 2;  // including one ghost layer each side
  auto at = [&](int comp, int i, int j) -> Real {
    const int ii = i + 1, jj = j + 1;
    return h[static_cast<std::size_t>(comp) * nx * nx + jj * nx + ii];
  };

  {
    std::ofstream os(path + "_u.csv");
    os << "# lid-driven cavity, u along the vertical centreline x=0.5\n";
    os << "# y,u\n";
    for (int j = 0; j < n; ++j)
      os << (j + 0.5) * m_h << "," << at(0, n / 2, j) << "\n";
  }
  {
    std::ofstream os(path + "_v.csv");
    os << "# lid-driven cavity, v along the horizontal centreline y=0.5\n";
    os << "# x,v\n";
    for (int i = 0; i < n; ++i)
      os << (i + 0.5) * m_h << "," << at(1, i, n / 2) << "\n";
  }
}

}  // namespace saturn
