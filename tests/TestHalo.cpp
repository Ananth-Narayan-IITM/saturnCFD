#include <cmath>

#include "TestHarness.hpp"
#include "amr/BoxArray.hpp"
#include "amr/DistributionMapping.hpp"
#include "amr/MultiFab.hpp"
#include "comm/HaloExchange.hpp"

using namespace saturn;

namespace {

Geometry haloGeom(int n) {
  Geometry g;
  Idx e{};
  e[0] = n;
  e[1] = n;
  if (NDIM == 3)
    e[2] = n;
  g.domain = Box::fromExtent(e);
  g.dx = 1.0;
  for (int d = 0; d < NDIM; ++d)
    g.periodic[d] = true;
  return g;
}

KOKKOS_INLINE_FUNCTION
Real wave(const Geometry& g, int i, int j, int k) {
  const Real pi = 3.14159265358979323846;
  Real v = Kokkos::sin(2 * pi * g.xc(0, i) / g.length(0)) *
           Kokkos::cos(2 * pi * g.xc(1, j) / g.length(1));
  if (NDIM == 3)
    v += Kokkos::sin(2 * pi * g.xc(2, k) / g.length(2));
  return v;
}

/// Max error over cells that are ghosts (outside the owning box, inside
/// domain).
Real maxGhostError(const MultiFab& mf, const Geometry& geom) {
  Real mx = Real(0);
  Kokkos::Max<Real> err(mx);

  for (int lb = 0; lb < mf.numLocalBoxes(); ++lb) {
    const Box bx = mf.boxArray()[mf.localToGlobal(lb)];
    const Array4<Real> a = mf.array(lb);
    const int ng = mf.nGrow();
    if (ng == 0)
      continue;

    const Box gb = bx.grow(ng);
    const int n0 = gb.length(0), n1 = gb.length(1);
    const int n2 = (NDIM == 3) ? gb.length(2) : 1;
    const Box dom = geom.domain;

    Kokkos::parallel_reduce(
        "maxGhostError", Kokkos::RangePolicy<>(0, n0 * n1 * n2),
        KOKKOS_LAMBDA(int idx, Real& m) {
          const int i = gb.lo[0] + idx % n0;
          const int r = idx / n0;
          const int j = gb.lo[1] + r % n1;
          const int k = (NDIM == 3) ? gb.lo[2] + r / n1 : 0;

          Idx p{};
          p[0] = i;
          p[1] = j;
          if (NDIM == 3)
            p[2] = k;
          if (bx.contains(p))
            return;  // valid cell
          if (!dom.contains(p))
            return;  // outside the domain: physical BC

          const Real d = Kokkos::abs(a(i, j, k, 0) - wave(geom, i, j, k));
          if (d > m)
            m = d;
        },
        err);
  }
  return err.reference();
}

}  // namespace

TEST(haloSingleBoxPeriodicWrapsOntoItself) {
  const Geometry g = haloGeom(8);
  Idx patch{};
  patch[0] = 8;
  patch[1] = 8;
  if (NDIM == 3)
    patch[2] = 8;

  const BoxArray ba = BoxArray::tileDomain(g.domain, patch);
  CHECKEQ(ba.size(), 1);
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

  MultiFab mf(ba, dm, 1, 2, 0, "mf");
  mf.forEachCell([=](int, Array4<Real> a, int i, int j, int k) {
    a(i, j, k, 0) = wave(g, i, j, k);
  });

  HaloExchange hx(ba, dm, g, 2, 0, 1);
  hx.build();
  CHECK(hx.numLocalOps() > 0);
  hx.execute(mf);

  const Real err = maxGhostError(mf, g);
  CHECK(err < Real(1e-5));
}

TEST(haloMultiBoxPeriodic) {
  const Geometry g = haloGeom(16);
  Idx patch{};
  patch[0] = 4;
  patch[1] = 4;
  if (NDIM == 3)
    patch[2] = 4;

  const BoxArray ba = BoxArray::tileDomain(g.domain, patch);
  CHECK(ba.size() > 1);
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

  MultiFab mf(ba, dm, 1, 2, 0, "mf");
  mf.forEachCell([=](int, Array4<Real> a, int i, int j, int k) {
    a(i, j, k, 0) = wave(g, i, j, k);
  });

  HaloExchange hx(ba, dm, g, 2, 0, 1);
  hx.build();
  hx.execute(mf);

  const Real err = maxGhostError(mf, g);
  CHECK(err < Real(1e-5));

  // Valid cells must be untouched.
  Real minv = mf.localMin(0);
  CHECK(minv > Real(-10));
}

TEST(haloZeroGhostIsNoop) {
  const Geometry g = haloGeom(8);
  Idx patch{};
  patch[0] = 4;
  patch[1] = 4;
  if (NDIM == 3)
    patch[2] = 4;

  const BoxArray ba = BoxArray::tileDomain(g.domain, patch);
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

  MultiFab mf(ba, dm, 1, 0, 0, "mf");
  mf.setVal(Real(3));

  HaloExchange hx(ba, dm, g, 0, 0, 1);
  hx.build();
  CHECKEQ(hx.numLocalOps(), 0);
  hx.execute(mf);

  CHECKNEAR(mf.localMin(0), 3.0, 1e-12);
  CHECKNEAR(mf.localMax(0), 3.0, 1e-12);
}
