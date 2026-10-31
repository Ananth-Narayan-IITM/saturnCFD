#pragma once

#include <string>

#include "core/Types.hpp"
#include "mesh/Mesh.hpp"

namespace saturn {

/// Build a structured hexahedral box.
///
/// `nx`, `ny`, `nz` are the cell counts in each direction. `lo` and `hi` are
/// the opposite corners. `shear` shifts x by `shear*(y - lo.y)`, producing a
/// non-orthogonal parallelepiped; the default is an orthogonal box.
///
/// The returned mesh has geometry computed, boundary patches named
/// `xmin/xmax/ymin/ymax/zmin/zmax`, and a structured (i,j,k) index.
/// Throws `std::invalid_argument` for non-positive extents.
Mesh makeBoxMesh(int nx, int ny, int nz, Vec3 lo, Vec3 hi, Real shear = 0);

/// Orthogonal sugar for `makeBoxMesh(nx, ny, nz, lo, hi, 0)`.
Mesh makeUniformBoxMesh(int nx, int ny, int nz, Vec3 lo = {0, 0, 0},
                        Vec3 hi = {1, 1, 1});

/// Sheared sugar for `makeBoxMesh(nx, ny, nz, lo, hi, shear)`.
Mesh makeShearedBoxMesh(int nx, int ny, int nz, Real shear, Vec3 lo = {0, 0, 0},
                        Vec3 hi = {1, 1, 1});

}  // namespace saturn
