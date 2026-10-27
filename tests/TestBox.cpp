#include "TestHarness.hpp"
#include "amr/Box.hpp"

using namespace saturn;

TEST(boxEmptyConstruction) {
  Box b;
  CHECK(!b.ok());
  CHECKEQ(b.numPts(), 0);
}

TEST(boxUnitAndExtent) {
  Idx n{};
  n[0] = 4;
  n[1] = 5;
  if (NDIM == 3)
    n[2] = 6;

  const Box b = Box::fromExtent(n);
  CHECK(b.ok());
  CHECKEQ(b.lo[0], 0);
  CHECKEQ(b.hi[0], 3);
  CHECKEQ(b.length(0), 4);
  CHECKEQ(b.length(1), 5);
  BigIndex expect = 20;
  if (NDIM == 3)
    expect *= 6;
  CHECKEQ(b.numPts(), expect);
}

TEST(boxGrowAndShift) {
  Idx lo{}, hi{};
  lo[0] = 2;
  hi[0] = 5;
  lo[1] = 3;
  hi[1] = 7;
  if (NDIM == 3) {
    lo[2] = 0;
    hi[2] = 1;
  }
  const Box b{lo, hi};

  const Box g = b.grow(2);
  CHECKEQ(g.lo[0], 0);
  CHECKEQ(g.hi[0], 7);
  CHECKEQ(g.lo[1], 1);
  CHECKEQ(g.hi[1], 9);

  const Box s = b.shift(Idx{1, -1, 0});
  CHECKEQ(s.lo[0], 3);
  CHECKEQ(s.lo[1], 2);
  CHECK(s.contains(b) == false);
  CHECK(g.contains(b));
}

TEST(boxIntersect) {
  Idx a0{}, a1{}, b0{}, b1{};
  a0[0] = 0;
  a1[0] = 4;
  a0[1] = 0;
  a1[1] = 4;
  b0[0] = 3;
  b1[0] = 8;
  b0[1] = 3;
  b1[1] = 8;
  if (NDIM == 3) {
    a0[2] = 0;
    a1[2] = 4;
    b0[2] = 0;
    b1[2] = 4;
  }
  const Box A{a0, a1}, B{b0, b1};

  const Box I = A.intersect(B);
  CHECK(I.ok());
  CHECKEQ(I.lo[0], 3);
  CHECKEQ(I.hi[0], 4);
  CHECKEQ(I.numPts(), static_cast<BigIndex>(NDIM == 2 ? 4 : 8));

  const Box C = A.shift(Idx{10, 0, 0});
  CHECK(!A.intersects(C));
  CHECK(A.intersect(C).numPts() == 0);
}

TEST(boxContainsCell) {
  Idx lo{}, hi{};
  lo[0] = 0;
  hi[0] = 2;
  lo[1] = 0;
  hi[1] = 2;
  if (NDIM == 3) {
    lo[2] = 0;
    hi[2] = 2;
  }
  const Box b{lo, hi};

  CHECK(b.contains(Idx{0, 0, 0}));
  CHECK(b.contains(Idx{2, 2, 2}));
  CHECK(!b.contains(Idx{3, 0, 0}));
  CHECK(!b.contains(Idx{-1, 0, 0}));
}
