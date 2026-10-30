#pragma once

#include <functional>
#include <vector>

#include "fv/CellField.hpp"
#include "fv/LocalMesh.hpp"

namespace saturn {

enum class BCType { Dirichlet, Neumann };

/// One boundary condition per mesh patch.
///
/// `value(x, outwardNormal, component)` returns the Dirichlet value or the
/// outward normal derivative, depending on `type`. The callback is evaluated
/// at the face centroid.
struct BoundaryCondition {
  BCType type = BCType::Dirichlet;
  std::function<Real(const Vec3&, const Vec3&, int)> value;
};

/// Convenience for a spatially constant patch condition.
inline BoundaryCondition constantBoundary(BCType type, Real value) {
  BoundaryCondition bc;
  bc.type = type;
  bc.value = [value](const Vec3&, const Vec3&, int) { return value; };
  return bc;
}

/// Fill physical-boundary ghost cells after the periodic/partition halo.
///
/// - Periodic directions are skipped: FieldHalo already filled them.
/// - Inactive directions on a 2D mesh are skipped.
/// - Dirichlet: ghost = 2*value - owner, so a face value interpolated with
///   equal weights is exactly the boundary value.
/// - Neumann: ghost layer l = owner + 2*l*d*g, where d is the owner-centroid
///   to face-centroid normal distance and g is the outward normal derivative.
///   The first layer gives the usual mirrored ghost; higher layers are filled
///   for the two-layer MUSCL stencil.
///
/// The callback runs on the host in this CPU-first slice; the field is copied
/// through its host mirror, which is an alias on a host Kokkos build.
///
/// Corner ghosts (two or three boundary directions at once) are not filled;
/// the current gradient/divergence stencils only read face-neighbour ghosts.
/// A higher-order reconstruction that needs corner ghosts must extend this.
void fillBoundaryGhosts(CellField& field, const LocalMesh& mesh,
                        const std::vector<BoundaryCondition>& conditions);

}  // namespace saturn
