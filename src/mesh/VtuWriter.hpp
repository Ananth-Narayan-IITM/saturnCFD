#pragma once

#include <string>

#include "mesh/CheckMesh.hpp"
#include "mesh/Mesh.hpp"

namespace saturn {

/// Write a volume `.vtu`, one `.vtp` per patch, a `.pvd` collection and a
/// `.json` sidecar. `prefix` is a path without extension.
void writeMeshVtk(const Mesh& mesh, const MeshReport& report,
                  const std::string& prefix);

}  // namespace saturn
