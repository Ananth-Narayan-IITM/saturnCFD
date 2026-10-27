#include <cmath>
#include <limits>

#include "TestHarness.hpp"
#include "amr/BoxArray.hpp"
#include "amr/DistributionMapping.hpp"
#include "amr/MultiFab.hpp"
#include "comm/HaloExchange.hpp"
#include "linalg/Poisson.hpp"
#include "linalg/VecOps.hpp"
#include "mesh/Geometry.hpp"

using namespace saturn;

namespace {

constexpr Real kPi = 3.14159265358979323846;

struct Setup {
  Geometry geom;
  BoxArray ba;
  DistributionMapping dm;
  HaloExchange halo;
};

Setup makeSetup(int N) {
  Setup s;
  Idx n{};
  n[0] = N;
  n[1] = N;
  if (NDIM == 3)
    n[2] = N;
  s.geom.domain = Box::fromExtent(n);
  s.geom.dx = Real(1) / Real(N);
  for (int d = 0; d < NDIM; ++d)
    s.geom.periodic[d] = false;

  Idx patch{};
  patch[0] = N;
  patch[1] = N;
  if (NDIM == 3)
    patch[2] = N;
  s.ba = BoxArray::tileDomain(s.geom.domain, patch);
  s.dm = DistributionMapping::roundRobin(s.ba, 1);
  s.halo = HaloExchange(s.ba, s.dm, s.geom, 1, 0, 1);
  s.halo.build();
  return s;
}

KOKKOS_INLINE_FUNCTION
Real trigAt(const Geometry& g, int i, int j, int k, bool cosine) {
  Real v = 1;
  const Real x = g.xc(0, i);
  const Real y = g.xc(1, j);
  v *= cosine ? Kokkos::cos(kPi * x) : Kokkos::sin(kPi * x);
  v *= cosine ? Kokkos::cos(kPi * y) : Kokkos::sin(kPi * y);
  if (NDIM == 3) {
    const Real z = g.xc(2, k);
    v *= cosine ? Kokkos::cos(kPi * z) : Kokkos::sin(kPi * z);
  }
  return v;
}

Real maxErrorAgainst(const MultiFab& p, const Geometry& g, bool cosine) {
  const Geometry gg = g;
  Real m0 = Real(0);
  Kokkos::Max<Real> red(m0);
  p.forEachCellFlatReduce(
      KOKKOS_LAMBDA(int, Array4<Real> A, int i, int j, int k, Real& mx) {
        const Real e = Kokkos::abs(A(i, j, k, 0) - trigAt(gg, i, j, k, cosine));
        if (e > mx)
          mx = e;
      },
      red);
  return red.reference();
}

}  // namespace

// Constant field is in the null space of the Neumann Laplacian.
TEST(poissonNeumannAnnihilatesConstants) {
  Setup s = makeSetup(16);
  MultiFab p(s.ba, s.dm, 1, 1, 0, "p");
  MultiFab Ap(s.ba, s.dm, 1, 1, 0, "Ap");

  p.setVal(Real(3.5));
  applyNegativeLaplacian(p, Ap, s.geom, s.halo, BCType::Neumann, s.geom.dx);

  CHECK(maxNorm(Ap) < Real(1e-10));
}

// Manufactured solution with p = 0 on the boundary: -lap(p) = NDIM*pi^2*p.
TEST(poissonDirichletSecondOrder) {
  auto run = [](int N) {
    Setup s = makeSetup(N);
    const Geometry g = s.geom;
    MultiFab p(s.ba, s.dm, 1, 1, 0, "p");
    MultiFab rhs(s.ba, s.dm, 1, 1, 0, "rhs");

    const Real k = Real(NDIM) * kPi * kPi;
    rhs.forEachCellFlat(
        KOKKOS_LAMBDA(int, Array4<Real> A, int i, int j, int kk) {
          (void)kk;
          A(i, j, kk, 0) = k * trigAt(g, i, j, kk, false);
        });

    PoissonParams prm;
    prm.bc = BCType::Dirichlet;
    prm.relTol = (sizeof(Real) == 8) ? Real(1e-12) : Real(1e-5);
    prm.maxIter = 20000;

    const PoissonResult res = solvePoisson(p, rhs, s.geom, s.halo, prm);
    return std::pair<Real, bool>(maxErrorAgainst(p, s.geom, false),
                                 res.converged);
  };

  const auto coarse = run(16);
  const auto fine = run(32);

  CHECK(coarse.second);
  CHECK(fine.second);
  // Second-order accuracy: halving h should cut the error by ~4.
  const Real ratio = coarse.first / fine.first;
  CHECK(ratio > Real(3.0));
  CHECK(ratio < Real(5.0));
  CHECK(fine.first < Real(5e-3));
}

// Neumann problem with a zero-mean manufactured solution:
// p = cos(pi x) cos(pi y) [cos(pi z)], dp/dn = 0 on every wall.
TEST(poissonNeumannManufactured) {
  Setup s = makeSetup(32);
  const Geometry g = s.geom;

  MultiFab p(s.ba, s.dm, 1, 1, 0, "p");
  MultiFab rhs(s.ba, s.dm, 1, 1, 0, "rhs");

  const Real k = Real(NDIM) * kPi * kPi;
  rhs.forEachCellFlat(KOKKOS_LAMBDA(int, Array4<Real> A, int i, int j, int kk) {
    A(i, j, kk, 0) = k * trigAt(g, i, j, kk, true);
  });

  PoissonParams prm;
  prm.bc = BCType::Neumann;
  prm.relTol = (sizeof(Real) == 8) ? Real(1e-11) : Real(1e-5);
  prm.maxIter = 20000;

  const PoissonResult res = solvePoisson(p, rhs, s.geom, s.halo, prm);
  CHECK(res.converged);

  // Unique only up to a constant; subtract the mean before comparing.
  const Accum mean = p.globalSum(0) / static_cast<Accum>(g.domainPts());
  addScalar(p, static_cast<Real>(-mean));

  const Real err = maxErrorAgainst(p, g, true);
  CHECK(err < Real(5e-3));
}
