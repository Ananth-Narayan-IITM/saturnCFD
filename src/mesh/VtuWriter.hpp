#pragma once

#include <string>

#include "mesh/CheckMesh.hpp"
#include "mesh/Mesh.hpp"

namespace saturn {

/// Write the volume `.vtu`. When `withPatches` is true, also write one `.vtp`
/// per boundary patch and a `.pvd` collection. `prefix` is a path without
/// extension. The JSON report is written by the caller.
void writeMeshVtk(const Mesh& mesh, const std::string& prefix,
                  bool withPatches);

}  // namespace saturn
