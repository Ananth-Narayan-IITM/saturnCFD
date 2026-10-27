#include "solver/CavitySolver.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "linalg/Poisson.hpp"
#include "solver/Limiters.hpp"

namespace saturn {

CavitySolver::CavitySolver(const Geometry& geom, const CavityParams& prm)
    : mGeometry(geom),
      mCells(geom.nCells(0)),
      mSpacing(geom.dx),
      mViscosity(prm.lidVelocity / (prm.reynolds * geom.length(0))),
      mParams(prm) {
  if (NDIM != 2)
    throw std::runtime_error(
        "CavitySolver: staggered implementation is 2D-only for now");
  if (mGeometry.domain.lo[0] != 0 || mGeometry.domain.lo[1] != 0)
    throw std::runtime_error("CavitySolver: domain must start at index 0");

  Idx patch{};
  patch[0] = mCells;
  patch[1] = mCells;
  const BoxArray ba = BoxArray::tileDomain(mGeometry.domain, patch);
  if (ba.size() != 1)
    throw std::runtime_error("CavitySolver: expected a single box");
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

  // Two ghost layers: the limited-reconstruction stencil reaches two points
  // upwind/downwind of a face, one further than the plain upwind stencil.
  mVelocity = MultiFab(ba, dm, 2, 2, 0, "u");
  mVelocityStar = MultiFab(ba, dm, 2, 2, 0, "ut");
  mPressure = MultiFab(ba, dm, 1, 1, 0, "p");
  mDivergence = MultiFab(ba, dm, 1, 1, 0, "div");
  mRhs = MultiFab(ba, dm, 1, 1, 0, "rhs");

  mHalo = HaloExchange(ba, dm, mGeometry, 2, 0, 1);
  mHalo.build();
}

void CavitySolver::initialize() {
  mVelocity.setVal(Real(0));
  mVelocityStar.setVal(Real(0));
  mPressure.setVal(Real(0));
  applyVelocityBC();
  mSteps = 0;
  mTime = 0;
}

// ---------------------------------------------------------------------------
// Boundary conditions
// ---------------------------------------------------------------------------
//   normal (on the wall face) : u(0,.)=u(n,.)=0 ,  v(.,0)=v(.,n)=0
//   tangential, no-slip       : odd reflection about the wall
//   tangential, moving lid    : u(i,n) = 2*Ulid - u(i,n-1)
//
// Two layers are filled because the limiter stencil looks two points beyond the
// face it is reconstructing.
void CavitySolver::applyVelocityBC() {
  const int n = mCells;
  const Real lid = mParams.lidVelocity;
  const Array4<Real> U = mVelocity.array(0);

  // Normal velocities on the four walls.
  Kokkos::parallel_for(
      "cavity::wallNormal", Kokkos::RangePolicy<>(0, n), KOKKOS_LAMBDA(int k) {
        U(0, k, 0, 0) = Real(0);
        U(n, k, 0, 0) = Real(0);
        U(k, 0, 0, 1) = Real(0);
        U(k, n, 0, 1) = Real(0);
      });

  // u: ghost layers in y (no-slip bottom, moving lid top)
  Kokkos::parallel_for(
      "cavity::uTangY", Kokkos::RangePolicy<>(0, n - 1),
      KOKKOS_LAMBDA(int idx) {
        const int i = 1 + idx;
        U(i, -1, 0, 0) = -U(i, 0, 0, 0);
        U(i, -2, 0, 0) = -U(i, 1, 0, 0);
        U(i, n, 0, 0) = Real(2) * lid - U(i, n - 1, 0, 0);
        U(i, n + 1, 0, 0) = Real(2) * lid - U(i, n - 2, 0, 0);
      });

  // u: ghost layers in x (no-slip side walls)
  Kokkos::parallel_for(
      "cavity::uTangX", Kokkos::RangePolicy<>(0, n), KOKKOS_LAMBDA(int j) {
        U(-1, j, 0, 0) = -U(1, j, 0, 0);
        U(-2, j, 0, 0) = -U(2, j, 0, 0);
        U(n + 1, j, 0, 0) = -U(n - 1, j, 0, 0);
      });

  // v: ghost layers in x
  Kokkos::parallel_for(
      "cavity::vTangX", Kokkos::RangePolicy<>(0, n + 1), KOKKOS_LAMBDA(int j) {
        U(-1, j, 0, 1) = -U(0, j, 0, 1);
        U(-2, j, 0, 1) = -U(1, j, 0, 1);
        U(n, j, 0, 1) = -U(n - 1, j, 0, 1);
        U(n + 1, j, 0, 1) = -U(n - 2, j, 0, 1);
      });

  // v: ghost layers in y (the limiter reaches two points beyond the face)
  Kokkos::parallel_for(
      "cavity::vTangY", Kokkos::RangePolicy<>(0, n), KOKKOS_LAMBDA(int i) {
        U(i, -1, 0, 1) = -U(i, 1, 0, 1);
        U(i, -2, 0, 1) = -U(i, 2, 0, 1);
        U(i, n + 1, 0, 1) = -U(i, n - 1, 0, 1);
      });
}

// ---------------------------------------------------------------------------
// Momentum predictor
// ---------------------------------------------------------------------------
// Each momentum control volume is half a cell wide in its own normal direction
// and a full cell wide in the other. That is why the advecting velocity on a
// face is the AVERAGE of the two neighbouring face values: the face sits at the
// midpoint between them.
void CavitySolver::predict(Real dt) {
  const int n = mCells;
  const Real h = mSpacing;
  const Real h2 = h * h;
  const Real nu = mViscosity;
  const int s = mParams.scheme;

  Kokkos::deep_copy(mVelocityStar.data(),
                    mVelocity.data());  // walls carried over

  const Array4<Real> U = mVelocity.array(0);
  const Array4<Real> Ut = mVelocityStar.array(0);

  // ---- u-momentum: CV centred on the x-face (i,j) ----------------------
  Kokkos::parallel_for(
      "cavity::uMomentum", Kokkos::RangePolicy<>(0, (n - 1) * n),
      KOKKOS_LAMBDA(int idx) {
        const int i = 1 + idx % (n - 1);
        const int j = idx / (n - 1);

        const Real uc = U(i, j, 0, 0);

        // advecting velocities on the four faces of this control volume
        const Real uWf = Real(0.5) * (U(i - 1, j, 0, 0) + U(i, j, 0, 0));
        const Real uEf = Real(0.5) * (U(i, j, 0, 0) + U(i + 1, j, 0, 0));
        const Real vSf = Real(0.5) * (U(i - 1, j, 0, 1) + U(i, j, 0, 1));
        const Real vNf =
            Real(0.5) * (U(i - 1, j + 1, 0, 1) + U(i, j + 1, 0, 1));

        // limited reconstructions of the convected u onto those faces
        const Real uE = limitedFace(s, U(i - 1, j, 0, 0), U(i, j, 0, 0),
                                    U(i + 1, j, 0, 0), U(i + 2, j, 0, 0), uEf);
        const Real uW = limitedFace(s, U(i - 2, j, 0, 0), U(i - 1, j, 0, 0),
                                    U(i, j, 0, 0), U(i + 1, j, 0, 0), uWf);
        const Real uN = limitedFace(s, U(i, j - 1, 0, 0), U(i, j, 0, 0),
                                    U(i, j + 1, 0, 0), U(i, j + 2, 0, 0), vNf);
        const Real uS = limitedFace(s, U(i, j - 2, 0, 0), U(i, j - 1, 0, 0),
                                    U(i, j, 0, 0), U(i, j + 1, 0, 0), vSf);

        const Real conv = (uEf * uE - uWf * uW) / h + (vNf * uN - vSf * uS) / h;
        const Real lap = (U(i + 1, j, 0, 0) - 2 * uc + U(i - 1, j, 0, 0)) / h2 +
                         (U(i, j + 1, 0, 0) - 2 * uc + U(i, j - 1, 0, 0)) / h2;

        Ut(i, j, 0, 0) = uc + dt * (-conv + nu * lap);
      });

  // ---- v-momentum: CV centred on the y-face (i,j) ----------------------
  Kokkos::parallel_for(
      "cavity::vMomentum", Kokkos::RangePolicy<>(0, n * (n - 1)),
      KOKKOS_LAMBDA(int idx) {
        const int i = idx % n;
        const int j = 1 + idx / n;

        const Real vc = U(i, j, 0, 1);

        const Real vSf = Real(0.5) * (U(i, j - 1, 0, 1) + U(i, j, 0, 1));
        const Real vNf = Real(0.5) * (U(i, j, 0, 1) + U(i, j + 1, 0, 1));
        const Real uWf = Real(0.5) * (U(i, j - 1, 0, 0) + U(i, j, 0, 0));
        const Real uEf =
            Real(0.5) * (U(i + 1, j - 1, 0, 0) + U(i + 1, j, 0, 0));

        const Real vN = limitedFace(s, U(i, j - 1, 0, 1), U(i, j, 0, 1),
                                    U(i, j + 1, 0, 1), U(i, j + 2, 0, 1), vNf);
        const Real vS = limitedFace(s, U(i, j - 2, 0, 1), U(i, j - 1, 0, 1),
                                    U(i, j, 0, 1), U(i, j + 1, 0, 1), vSf);
        const Real vE = limitedFace(s, U(i - 1, j, 0, 1), U(i, j, 0, 1),
                                    U(i + 1, j, 0, 1), U(i + 2, j, 0, 1), uEf);
        const Real vW = limitedFace(s, U(i - 2, j, 0, 1), U(i - 1, j, 0, 1),
                                    U(i, j, 0, 1), U(i + 1, j, 0, 1), uWf);

        const Real conv = (uEf * vE - uWf * vW) / h + (vNf * vN - vSf * vS) / h;
        const Real lap = (U(i + 1, j, 0, 1) - 2 * vc + U(i - 1, j, 0, 1)) / h2 +
                         (U(i, j + 1, 0, 1) - 2 * vc + U(i, j - 1, 0, 1)) / h2;

        Ut(i, j, 0, 1) = vc + dt * (-conv + nu * lap);
      });
}

void CavitySolver::computeDivergence(const MultiFab& vel, MultiFab& div) const {
  const Real h = mSpacing;
  const Array4<Real> U = vel.array(0);

  div.forEachCellFlat(KOKKOS_LAMBDA(int, Array4<Real> D, int i, int j, int k) {
    (void)k;
    D(i, j, 0, 0) = (U(i + 1, j, 0, 0) - U(i, j, 0, 0)) / h +
                    (U(i, j + 1, 0, 1) - U(i, j, 0, 1)) / h;
  });
}

void CavitySolver::correct(Real dt) {
  const int n = mCells;
  const Real h = mSpacing;
  const Array4<Real> Ut = mVelocityStar.array(0);
  const Array4<Real> U = mVelocity.array(0);
  const Array4<Real> P = mPressure.array(0);

  // u-faces: i in [0,n]
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
  const int n = mCells;
  const Array4<Real> U = mVelocity.array(0);

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
        if (a > mx)
          mx = a;
      },
      red);
  return red.reference();
}

Real CavitySolver::maxDivergence() const {
  MultiFab d(mVelocity.boxArray(), mVelocity.distributionMap(), 1, 0, 0, "d");
  computeDivergence(mVelocity, d);
  Real m0 = Real(0);
  Kokkos::Max<Real> red(m0);
  const Array4<Real> D = d.array(0);
  Kokkos::parallel_reduce(
      "cavity::maxDiv", Kokkos::RangePolicy<>(0, mCells * mCells),
      KOKKOS_LAMBDA(int idx, Real& mx) {
        const int i = idx % mCells;
        const int j = idx / mCells;
        const Real a = Kokkos::abs(D(i, j, 0, 0));
        if (a > mx)
          mx = a;
      },
      red);
  return red.reference();
}

Real CavitySolver::stableDt() const {
  const Real umax = maxSpeed();
  const Real dtConv = (umax > 0) ? mParams.cfl * mSpacing / umax : Real(1e30);
  const Real dtVisc =
      mParams.diffusionFactor * mSpacing * mSpacing / mViscosity;
  return Kokkos::min(dtConv, dtVisc);
}

void CavitySolver::step(Real dt) {
  predict(dt);
  computeDivergence(mVelocityStar, mDivergence);  // divergence of the PREDICTOR

  const Real invdt = Real(1) / dt;
  const Array4<Real> D = mDivergence.array(0);
  mRhs.forEachCellFlat(KOKKOS_LAMBDA(int, Array4<Real> R, int i, int j, int k) {
    (void)k;
    R(i, j, 0, 0) = -D(i, j, 0, 0) * invdt;
  });

  PoissonParams pp;
  pp.bc = BCType::Neumann;
  pp.maxIter = mParams.poissonMaxIter;
  pp.relTol = mParams.poissonTol;
  pp.warmStart = true;
  solvePoisson(mPressure, mRhs, mGeometry, mHalo, pp);

  correct(dt);
  applyVelocityBC();

  mSteps++;
  mTime += dt;
}

SteadyReport CavitySolver::runToSteady(
    Real tEnd, int maxSteps, Real tol, int checkEvery, int outputEvery,
    const std::function<void(int, Real)>& onOutput) {
  SteadyReport rep;
  MultiFab prev(mVelocity.boxArray(), mVelocity.distributionMap(), 2, 2, 0,
                "prev");

  if (onOutput && outputEvery > 0)
    onOutput(0, 0.0);

  while (mTime < tEnd && rep.steps < maxSteps) {
    const Real dt = stableDt();
    step(dt);
    rep.steps++;

    if (onOutput && outputEvery > 0 && rep.steps % outputEvery == 0)
      onOutput(rep.steps, mTime);

    if (rep.steps % checkEvery == 0) {
      const Array4<Real> U = mVelocity.array(0);
      const Array4<Real> V = prev.array(0);
      Real m0 = Real(0);
      Kokkos::Max<Real> red(m0);
      Kokkos::parallel_reduce(
          "cavity::delta", Kokkos::RangePolicy<>(0, (mCells + 1) * mCells),
          KOKKOS_LAMBDA(int idx, Real& mx) {
            const int i = idx % (mCells + 1);
            const int j = idx / (mCells + 1);
            const Real d = Kokkos::abs(U(i, j, 0, 0) - V(i, j, 0, 0));
            if (d > mx)
              mx = d;
          },
          red);
      rep.maxDelta = red.reference();
      Kokkos::deep_copy(prev.data(), mVelocity.data());
      rep.converged = (rep.maxDelta < tol);
      if (rep.converged)
        break;
    }
  }
  rep.time = mTime;
  return rep;
}

void CavitySolver::cellCentredVelocity(MultiFab& out) const {
  const Array4<Real> U = mVelocity.array(0);
  out.forEachCellFlat(KOKKOS_LAMBDA(int, Array4<Real> C, int i, int j, int k) {
    (void)k;
    C(i, j, 0, 0) = Real(0.5) * (U(i, j, 0, 0) + U(i + 1, j, 0, 0));
    C(i, j, 0, 1) = Real(0.5) * (U(i, j, 0, 1) + U(i, j + 1, 0, 1));
  });
  (void)out;
}

void CavitySolver::writeCentreLines(const std::string& path) const {
  const int n = mCells;
  const int ng = mVelocity.nGrow();
  auto h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(),
                                               mVelocity.data());
  const int nx = n + 2 * ng;
  auto at = [&](int comp, int i, int j) -> Real {
    const int ii = i + ng, jj = j + ng;
    return h[static_cast<std::size_t>(comp) * nx * nx + jj * nx + ii];
  };

  {
    std::ofstream os(path + "_u.csv");
    os << "# lid-driven cavity, u along the vertical centreline x=0.5\n";
    os << "# y,u\n";
    for (int j = 0; j < n; ++j)
      os << (j + 0.5) * mSpacing << "," << at(0, n / 2, j) << "\n";
  }
  {
    std::ofstream os(path + "_v.csv");
    os << "# lid-driven cavity, v along the horizontal centreline y=0.5\n";
    os << "# x,v\n";
    for (int i = 0; i < n; ++i)
      os << (i + 0.5) * mSpacing << "," << at(1, i, n / 2) << "\n";
  }
}

}  // namespace saturn
