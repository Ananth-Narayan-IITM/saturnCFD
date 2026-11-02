#pragma once

#include <Kokkos_Core.hpp>

namespace saturn {

/// The 3D cell-range policy used by every field kernel.
///
/// `CellField` and `FaceField` store `(i, j, k, c)` as `LayoutLeft`, so the
/// first index is contiguous. Kokkos' default MDRange iteration is the
/// opposite ("right", last index fastest), which walks the fields with a
/// stride of `ny*nz` doubles and wastes most of each cache line. On one core a
/// 64^3 7-point stencil runs at 14.1 ns/cell with the default order and
/// 5.1 ns/cell with `Iterate::Left`; on the GPU the same setting gives
/// coalesced warp loads.
///
/// Only the traversal order changes, never the arithmetic or the result.
using Policy3d = Kokkos::MDRangePolicy<
    Kokkos::Rank<3, Kokkos::Iterate::Default, Kokkos::Iterate::Left>,
    Kokkos::IndexType<int>>;

}  // namespace saturn
