#include "io/Checkpoint.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

#include "core/Error.hpp"

namespace saturn {
namespace {

Checkpoint readRoot(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    throw SaturnError("cannot read checkpoint '" + path + "'");

  std::string line;
  if (!std::getline(in, line) || line.rfind("SATURNCHK", 0) != 0)
    throw SaturnError("checkpoint '" + path + "' has no SATURNCHK header");

  Checkpoint checkpoint;
  int cells = 0;
  bool haveData = false;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::string key;
    ls >> key;
    if (key == "solver")
      ls >> checkpoint.solver;
    else if (key == "dimension")
      ls >> checkpoint.dimension;
    else if (key == "grid")
      ls >> checkpoint.nx >> checkpoint.ny >> checkpoint.nz;
    else if (key == "step")
      ls >> checkpoint.step;
    else if (key == "time")
      ls >> checkpoint.time;
    else if (key == "cells")
      ls >> cells;
    else if (key == "DATA") {
      haveData = true;
      break;
    } else if (!key.empty()) {
      throw SaturnError("unknown checkpoint key '" + key + "' in '" + path +
                        "'");
    }
  }
  if (!haveData || cells <= 0 || checkpoint.solver.empty() ||
      checkpoint.dimension < 1)
    throw SaturnError("checkpoint '" + path + "' is incomplete");
  if (cells > 1000000000)
    throw SaturnError("checkpoint '" + path + "' field is too large");

  checkpoint.values.resize(static_cast<std::size_t>(cells));
  in.read(
      reinterpret_cast<char*>(checkpoint.values.data()),
      static_cast<std::streamsize>(checkpoint.values.size() * sizeof(Real)));
  if (!in)
    throw SaturnError("checkpoint '" + path + "' has a truncated field");
  return checkpoint;
}

}  // namespace

void writeCheckpoint(const std::string& path, const Checkpoint& checkpoint) {
  const std::filesystem::path base(path);
  if (base.has_parent_path())
    std::filesystem::create_directories(base.parent_path());

  std::ofstream os(path, std::ios::binary);
  if (!os)
    throw SaturnError("cannot write checkpoint '" + path + "'");

  os << std::setprecision(17);
  os << "SATURNCHK 1\n";
  os << "solver " << checkpoint.solver << "\n";
  os << "dimension " << checkpoint.dimension << "\n";
  os << "grid " << checkpoint.nx << ' ' << checkpoint.ny << ' ' << checkpoint.nz
     << "\n";
  os << "step " << checkpoint.step << "\n";
  os << "time " << checkpoint.time << "\n";
  os << "cells " << checkpoint.values.size() << "\n";
  os << "DATA\n";
  os.write(
      reinterpret_cast<const char*>(checkpoint.values.data()),
      static_cast<std::streamsize>(checkpoint.values.size() * sizeof(Real)));
  if (!os)
    throw SaturnError("failed while writing checkpoint '" + path + "'");
}

Checkpoint readCheckpoint(const std::string& path) {
  Checkpoint checkpoint;
#ifdef SATURN_USE_MPI
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0)
    checkpoint = readRoot(path);

  int meta[5] = {checkpoint.dimension, checkpoint.nx, checkpoint.ny,
                 checkpoint.nz, checkpoint.step};
  MPI_Bcast(meta, 5, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&checkpoint.time, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  char solver[64]{};
  if (rank == 0)
    std::strncpy(solver, checkpoint.solver.c_str(), sizeof(solver) - 1);
  MPI_Bcast(solver, sizeof(solver), MPI_CHAR, 0, MPI_COMM_WORLD);

  int count = static_cast<int>(checkpoint.values.size());
  MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (count <= 0)
    throw SaturnError("checkpoint '" + path + "' has an empty field");
  checkpoint.values.resize(static_cast<std::size_t>(count));
  MPI_Bcast(checkpoint.values.data(), count, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  checkpoint.dimension = meta[0];
  checkpoint.nx = meta[1];
  checkpoint.ny = meta[2];
  checkpoint.nz = meta[3];
  checkpoint.step = meta[4];
  checkpoint.solver = solver;
#else
  checkpoint = readRoot(path);
#endif
  return checkpoint;
}

}  // namespace saturn
