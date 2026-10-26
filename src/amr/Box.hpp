// saturnCFD — axis-aligned index-space box. The atom of the AMR decomposition.
//
// A Box is inclusive on both ends: lo[d] and hi[d] are valid indices.
// An *empty* box has lo[d] > hi[d] for at least one d. All queries are
// KOKKOS_INLINE_FUNCTION so they work inside kernels.
#pragma once

#include <Kokkos_Macros.hpp>

#include <type_traits>

#include "core/Types.hpp"

namespace saturn {

struct Box {
  Idx lo;
  Idx hi;

  KOKKOS_INLINE_FUNCTION
  Box() {
    for (int d = 0; d < 3; ++d) {
      lo[d] = 0;
      hi[d] = -1;  // empty
    }
  }

  KOKKOS_INLINE_FUNCTION
  Box(const Idx& a_lo, const Idx& a_hi) : lo(a_lo), hi(a_hi) {}

  /// A box covering [0, n) in every dimension.
  KOKKOS_INLINE_FUNCTION
  static Box fromExtent(const Idx& n) {
    Box b;
    for (int d = 0; d < NDIM; ++d) {
      b.lo[d] = 0;
      b.hi[d] = n[d] - 1;
    }
    return b;
  }

  /// A single cell.
  KOKKOS_INLINE_FUNCTION
  static Box unit(const Idx& p) { return Box{p, p}; }

  KOKKOS_INLINE_FUNCTION
  bool ok() const {
    for (int d = 0; d < NDIM; ++d)
      if (hi[d] < lo[d]) return false;
    return true;
  }

  KOKKOS_INLINE_FUNCTION
  Index length(int d) const { return ok() ? (hi[d] - lo[d] + 1) : 0; }

  KOKKOS_INLINE_FUNCTION
  BigIndex numPts() const {
    if (!ok()) return 0;
    BigIndex n = 1;
    for (int d = 0; d < NDIM; ++d) n *= (hi[d] - lo[d] + 1);
    return n;
  }

  KOKKOS_INLINE_FUNCTION
  bool contains(const Idx& p) const {
    for (int d = 0; d < NDIM; ++d)
      if (p[d] < lo[d] || p[d] > hi[d]) return false;
    return true;
  }

  /// True if b is entirely inside *this.
  KOKKOS_INLINE_FUNCTION
  bool contains(const Box& b) const {
    if (!b.ok()) return true;
    for (int d = 0; d < NDIM; ++d)
      if (b.lo[d] < lo[d] || b.hi[d] > hi[d]) return false;
    return true;
  }

  KOKKOS_INLINE_FUNCTION
  bool intersects(const Box& b) const { return intersect(b).ok(); }

  KOKKOS_INLINE_FUNCTION
  Box intersect(const Box& b) const {
    Box r;
    for (int d = 0; d < NDIM; ++d) {
      r.lo[d] = lo[d] > b.lo[d] ? lo[d] : b.lo[d];
      r.hi[d] = hi[d] < b.hi[d] ? hi[d] : b.hi[d];
    }
    return r;
  }

  /// Grow by n cells on every side.
  KOKKOS_INLINE_FUNCTION
  Box grow(int n) const {
    Box r = *this;
    if (!ok()) return r;
    for (int d = 0; d < NDIM; ++d) {
      r.lo[d] -= n;
      r.hi[d] += n;
    }
    return r;
  }

  /// Grow by n cells in dimension d only.
  KOKKOS_INLINE_FUNCTION
  Box grow(int d, int n) const {
    Box r = *this;
    if (!ok()) return r;
    r.lo[d] -= n;
    r.hi[d] += n;
    return r;
  }

  KOKKOS_INLINE_FUNCTION
  Box shift(const Idx& v) const {
    Box r = *this;
    for (int d = 0; d < NDIM; ++d) {
      r.lo[d] += v[d];
      r.hi[d] += v[d];
    }
    return r;
  }

  /// Shrink by n on every side (may become empty).
  KOKKOS_INLINE_FUNCTION
  Box coarsen(int n) const {
    Box r = *this;
    if (!ok()) return r;
    for (int d = 0; d < NDIM; ++d) {
      r.lo[d] += n;
      r.hi[d] -= n;
    }
    return r;
  }

  KOKKOS_INLINE_FUNCTION
  bool operator==(const Box& b) const {
    for (int d = 0; d < NDIM; ++d)
      if (lo[d] != b.lo[d] || hi[d] != b.hi[d]) return false;
    return true;
  }

  KOKKOS_INLINE_FUNCTION
  bool operator!=(const Box& b) const { return !(*this == b); }
};

}  // namespace saturn

// Required by Kokkos to detect trivially copyable kernel arguments.
static_assert(std::is_trivially_copyable<saturn::Box>::value,
              "Box must be trivially copyable to live in a Kokkos::View");
