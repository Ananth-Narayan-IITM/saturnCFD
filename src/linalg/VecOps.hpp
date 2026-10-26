// saturnCFD — elementary vector operations on MultiFab.
//
// A MultiFab is a distributed array, so these are the primitive global
// operations every Krylov solver needs: inner products, axpy, norms. They are
// header-only because they are templated on the Kokkos lambda they build.
//
// Every reduction here accumulates in `Accum` (double), even when the field
// precision is float. A float accumulation over 10^6 cells is meaningless.
#pragma once

#include <cmath>

#include "amr/MultiFab.hpp"
#include "core/Types.hpp"

namespace saturn {

/// sum over cells of a* b (single component each).
inline Accum dot(const MultiFab& a, const MultiFab& b, int ca = 0, int cb = 0) {
  const auto bf = b.boxArrays();
  Accum s0 = 0;
  Kokkos::Sum<Accum> red(s0);
  a.forEachCellFlatReduce(
      KOKKOS_LAMBDA(int box, Array4<Real> A, int i, int j, int k, Accum& acc) {
        Array4<Real> B = bf(box);
        acc += static_cast<Accum>(A(i, j, k, ca)) *
               static_cast<Accum>(B(i, j, k, cb));
      },
      red);
  return red.reference();
}

/// sqrt(sum a^2)
inline Accum norm2(const MultiFab& a, int c = 0) {
  return std::sqrt(dot(a, a, c, c));
}

/// max |a| over all local cells
inline Real maxNorm(const MultiFab& a, int c = 0) {
  Real m0 = Real(0);
  Kokkos::Max<Real> red(m0);
  a.forEachCellFlatReduce(
      KOKKOS_LAMBDA(int, Array4<Real> A, int i, int j, int k, Real& mx) {
        const Real v = Kokkos::abs(A(i, j, k, c));
        if (v > mx) mx = v;
      },
      red);
  return red.reference();
}

/// y += alpha * x  (matching box structure, single component)
inline void axpy(Real alpha, const MultiFab& x, MultiFab& y, int cx = 0,
                 int cy = 0) {
  const auto xf = x.boxArrays();
  y.forEachCellFlat(
      KOKKOS_LAMBDA(int box, Array4<Real> Y, int i, int j, int k) {
        Array4<Real> X = xf(box);
        Y(i, j, k, cy) += alpha * X(i, j, k, cx);
      });
}

/// y = alpha * x
inline void axpby(Real alpha, const MultiFab& x, Real beta, MultiFab& y,
                  int cx = 0, int cy = 0) {
  const auto xf = x.boxArrays();
  y.forEachCellFlat(
      KOKKOS_LAMBDA(int box, Array4<Real> Y, int i, int j, int k) {
        Array4<Real> X = xf(box);
        Y(i, j, k, cy) = alpha * X(i, j, k, cx) + beta * Y(i, j, k, cy);
      });
}

/// y = x
inline void copyField(const MultiFab& x, MultiFab& y, int cx = 0, int cy = 0) {
  const auto xf = x.boxArrays();
  y.forEachCellFlat(
      KOKKOS_LAMBDA(int box, Array4<Real> Y, int i, int j, int k) {
        Array4<Real> X = xf(box);
        Y(i, j, k, cy) = X(i, j, k, cx);
      });
}

inline void setZero(MultiFab& x, int c = 0) {
  x.forEachCellFlat(KOKKOS_LAMBDA(int, Array4<Real> A, int i, int j, int k) {
    A(i, j, k, c) = Real(0);
  });
}

/// x += s  (uniform shift; used to remove the null space of a Neumann problem)
inline void addScalar(MultiFab& x, Real s, int c = 0) {
  x.forEachCellFlat(KOKKOS_LAMBDA(int, Array4<Real> A, int i, int j, int k) {
    A(i, j, k, c) += s;
  });
}

/// x *= s
inline void scaleField(MultiFab& x, Real s, int c = 0) {
  x.forEachCellFlat(KOKKOS_LAMBDA(int, Array4<Real> A, int i, int j, int k) {
    A(i, j, k, c) *= s;
  });
}

}  // namespace saturn
