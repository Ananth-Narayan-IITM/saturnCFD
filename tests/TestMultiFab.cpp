#include "TestHarness.hpp"
#include "amr/BoxArray.hpp"
#include "amr/DistributionMapping.hpp"
#include "amr/MultiFab.hpp"
#include "mesh/Geometry.hpp"

using namespace saturn;

namespace {

Geometry makeGeom(int n) {
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

}  // namespace

TEST(tileDomainCoversAndIsDisjoint) {
  const Geometry g = makeGeom(10);
  Idx patch{};
  patch[0] = 4;
  patch[1] = 4;
  if (NDIM == 3)
    patch[2] = 4;

  const BoxArray ba = BoxArray::tileDomain(g.domain, patch);
  CHECK(ba.size() > 1);
  CHECK(ba.disjoint());

  BigIndex total = 0;
  for (int i = 0; i < ba.size(); ++i)
    total += ba[i].numPts();
  CHECKEQ(total, g.domainPts());

  const Box bb = ba.boundingBox();
  CHECK(bb == g.domain);
}

TEST(distributionBalancesBoxes) {
  const Geometry g = makeGeom(16);
  Idx patch{};
  patch[0] = 4;
  patch[1] = 4;
  if (NDIM == 3)
    patch[2] = 4;
  const BoxArray ba = BoxArray::tileDomain(g.domain, patch);

  const int nranks = 3;
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, nranks);

  int assigned = 0;
  for (int r = 0; r < nranks; ++r) {
    const int c = dm.countOnRank(r);
    CHECK(c > 0);
    assigned += c;
  }
  CHECKEQ(assigned, ba.size());
}

TEST(multifabFillAndReduce) {
  const Geometry g = makeGeom(8);
  Idx patch{};
  patch[0] = 4;
  patch[1] = 4;
  if (NDIM == 3)
    patch[2] = 4;
  const BoxArray ba = BoxArray::tileDomain(g.domain, patch);
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

  MultiFab mf(ba, dm, 1, 0, 0, "mf");
  mf.setVal(0.0);

  CHECKEQ(mf.numLocalBoxes(), ba.size());
  CHECKEQ(mf.numLocalPts(), g.domainPts());

  mf.forEachCell([](int, Array4<Real> a, int i, int j, int k) {
    a(i, j, k, 0) = Real(2);
  });

  CHECKNEAR(mf.localSum(0), static_cast<double>(g.domainPts()) * 2.0, 1e-12);
  CHECKNEAR(mf.localMin(0), 2.0, 1e-12);
  CHECKNEAR(mf.localMax(0), 2.0, 1e-12);
}

TEST(multifabGlobalBoxLookup) {
  const Geometry g = makeGeom(8);
  Idx patch{};
  patch[0] = 4;
  patch[1] = 4;
  if (NDIM == 3)
    patch[2] = 4;
  const BoxArray ba = BoxArray::tileDomain(g.domain, patch);
  const DistributionMapping dm = DistributionMapping::roundRobin(ba, 1);

  MultiFab mf(ba, dm, 2, 1, 0, "mf");

  for (int gb = 0; gb < ba.size(); ++gb) {
    CHECKEQ(mf.globalToLocal(gb), gb);
    const Array4<Real> a = mf.arrayByGlobal(gb);
    CHECK(a.p != nullptr);
  }

  // Component addressing must not collide.
  mf.setVal(0.0);
  for (int lb = 0; lb < mf.numLocalBoxes(); ++lb) {
    const Box bx = mf.boxArray()[mf.localToGlobal(lb)];
    const Array4<Real> a = mf.array(lb);
    a(bx.lo[0], bx.lo[1], bx.lo[2], 0) = Real(1);
    a(bx.lo[0], bx.lo[1], bx.lo[2], 1) = Real(5);
  }
  CHECKNEAR(mf.localSum(0), static_cast<double>(ba.size()), 1e-12);
  CHECKNEAR(mf.localSum(1), static_cast<double>(ba.size()) * 5.0, 1e-12);
}
