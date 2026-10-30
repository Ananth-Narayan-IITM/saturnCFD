#pragma once

#include <stdexcept>

#include <Kokkos_Macros.hpp>

#include "core/Types.hpp"

namespace saturn {

/// Geometry of a uniform Cartesian block.
///
/// This is the first device geometry. It is deliberately uniform: the
/// manufactured-solution tests on a uniform hex grid can then verify the halo
/// and the discrete Laplacian independently of mesh-quality corrections. A
/// general `LocalMesh` extracted from `Mesh` replaces it for non-uniform grids.
struct CartesianGeometry {
  Vec3 origin{};
  Vec3 spacing{1, 1, 1};
  int dim = 3;

  CartesianGeometry() = default;

  CartesianGeometry(Vec3 o, Vec3 s, int d) : origin(o), spacing(s), dim(d) {
    if (d < 1 || d > 3)
      throw std::invalid_argument("CartesianGeometry: dim must be 1, 2 or 3");
    for (int axis = 0; axis < 3; ++axis)
      if (!(spacing[axis] > 0))
        throw std::invalid_argument(
            "CartesianGeometry: spacing must be positive");
  }

  /// Coefficient of the three-point stencil in direction `d`: 1/h_d^2.
  KOKKOS_INLINE_FUNCTION Real weight(int d) const {
    return Real(1) / (spacing[d] * spacing[d]);
  }

  KOKKOS_INLINE_FUNCTION Vec3 cellCentre(int i, int j, int k) const {
    return {origin.x + (Real(i) + Real(0.5)) * spacing.x,
            origin.y + (Real(j) + Real(0.5)) * spacing.y,
            origin.z + (Real(k) + Real(0.5)) * spacing.z};
  }

  KOKKOS_INLINE_FUNCTION Real cellVolume() const {
    return spacing.x * spacing.y * spacing.z;
  }
};

}  // namespace saturn
