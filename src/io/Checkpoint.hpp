#pragma once

#include <string>
#include <vector>

#include "core/Types.hpp"

namespace saturn {

/// State needed to restart a structured scalar solver.
///
/// The field is stored in **global cell order**, independent of the MPI
/// partition. Mesh identity is the solver, dimension and grid extents.
struct Checkpoint {
  std::string solver;
  int dimension = 0;
  int nx = 0;
  int ny = 0;
  int nz = 0;
  int step = 0;
  Real time = 0;
  std::vector<Real> values;
};

/// Write a checkpoint. Call on rank 0 with the global field.
void writeCheckpoint(const std::string& path, const Checkpoint& checkpoint);

/// Read a checkpoint on every rank. Rank 0 reads the file and broadcasts the
/// metadata and field, so the shared filesystem is touched once.
Checkpoint readCheckpoint(const std::string& path);

}  // namespace saturn
