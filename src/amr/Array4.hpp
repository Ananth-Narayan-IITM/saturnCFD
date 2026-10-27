/**
 * @file Array4.hpp
 *
 * saturnCFD — 4-index field accessor over a cell-centred block.
 *
 * Layout is z-slowest, x-fastest (Kokkos LayoutRight) so that a team sweeping
 * in x reads contiguous memory.
 *
 * Indices passed in are *global* (level) indices. The accessor subtracts the
 * block's ghost-inclusive origin, so callers never think about offsets.
 */
#pragma once

#include "core/Types.hpp"

namespace saturn {

/**
 * @brief 4-index accessor (i, j, k, component) over one cell block.
 *
 * Takes GLOBAL indices; the block origin is subtracted internally, so callers
 * never handle offsets.
 */
template <class T>
struct Array4 {
  T* p = nullptr;
  Index begin[NDIM3] = {0, 0, 0};  // global index of element (0,0,0)
  int n[NDIM3] = {0, 0, 0};        // extent including ghosts
  int ncomp = 0;

  KOKKOS_INLINE_FUNCTION
  T& operator()(int i, int j, int k, int c) const {
    const int ii = i - begin[0];
    const int jj = j - begin[1];
    const int kk = (NDIM == 3) ? (k - begin[2]) : 0;
    const BigIndex sx = 1;
    const BigIndex sy = n[0];
    const BigIndex sz = static_cast<BigIndex>(n[0]) * n[1];
    const BigIndex sc = sz * ((NDIM == 3) ? n[2] : 1);
    return p[static_cast<BigIndex>(c) * sc + kk * sz + jj * sy + ii * sx];
  }

  /// Convenience: 2D / single-component access.
  KOKKOS_INLINE_FUNCTION
  T& operator()(int i, int j) const { return operator()(i, j, 0, 0); }
};

}  // namespace saturn
