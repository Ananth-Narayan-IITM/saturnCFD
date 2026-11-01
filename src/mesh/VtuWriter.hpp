#pragma once

#include <string>
#include <utility>
#include <vector>

#include "mesh/CheckMesh.hpp"
#include "mesh/Mesh.hpp"

namespace saturn {

/// Write the volume `.vtu`. When `withPatches` is true, also write one `.vtp`
/// per boundary patch and a `.pvd` collection. `prefix` is a path without
/// extension. The JSON report is written by the caller.
void writeMeshVtk(const Mesh& mesh, const std::string& prefix,
                  bool withPatches);

/// Write a single `.vtu` with one cell-centred scalar field.
///
/// `cellValues` must have one entry per `mesh.cells` entry. `path` is the full
/// output file name, normally ending in `.vtu`.
void writeCellFieldVtu(const Mesh& mesh, const std::vector<Real>& cellValues,
                       const std::string& path,
                       const std::string& fieldName = "u");

/// Write a ParaView `.pvd` collection for a sequence of `.vtu` files.
/// The frame `.second` values are file names, not full paths.
void writePvd(const std::string& path,
              const std::vector<std::pair<Real, std::string>>& frames);

}  // namespace saturn
