#pragma once

#include "config/Config.hpp"
#include "mesh/Mesh.hpp"

namespace saturn {

/// Build the mesh described by a case file.
///
/// `mesh.type` selects the source:
///
/// - `auto` (default): Gmsh when `mesh.file` is set, otherwise a box;
/// - `gmsh`: read and check `mesh.file`;
/// - `box`: structured box from `mesh.nx/ny/nz` and `mesh.box.*`.
///
/// The returned mesh always has geometry computed and a structured index when
/// the topology permits it.
Mesh makeMeshFromConfig(const Config& cfg);

/// Load/build the mesh once on rank 0 and give every other rank metadata only.
/// The solver's LocalMesh distributor then sends each rank its local geometry.
Mesh makeMeshForSolver(const Config& cfg);

}  // namespace saturn
