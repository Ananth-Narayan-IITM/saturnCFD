#include "mesh/MeshFactory.hpp"

#include <string>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

#include "core/Error.hpp"
#include "mesh/BoxMesh.hpp"
#include "mesh/CheckMesh.hpp"

namespace saturn {

Mesh makeMeshFromConfig(const Config& cfg) {
  std::string type = cfg.getString("mesh.type", "auto");
  const std::string file = cfg.getString("mesh.file", "");
  if (type == "auto")
    type = file.empty() ? "box" : "gmsh";

  if (type == "gmsh") {
    if (file.empty())
      throw ConfigError("mesh.file is required when mesh.type is gmsh");
    Mesh mesh = readGmsh(file);
    const MeshReport report = checkMesh(mesh, file);
    if (!report.pass()) {
      std::string message = "mesh '" + file + "' failed validation:";
      for (const auto& error : report.errors)
        message += "\n  " + error;
      throw ConfigError(message);
    }
    return mesh;
  }

  if (type != "box")
    throw ConfigError("mesh.type must be auto, gmsh or box");

  const int nx = cfg.getInt("mesh.nx", 1);
  const int ny = cfg.getInt("mesh.ny", 1);
  const int nz = cfg.getInt("mesh.nz", 1);
  if (nx < 1 || ny < 1 || nz < 1)
    throw ConfigError("mesh.nx, mesh.ny and mesh.nz must be positive");

  const Vec3 lo{cfg.getDouble("mesh.box.xmin", 0.0),
                cfg.getDouble("mesh.box.ymin", 0.0),
                cfg.getDouble("mesh.box.zmin", 0.0)};
  const Vec3 hi{cfg.getDouble("mesh.box.xmax", 1.0),
                cfg.getDouble("mesh.box.ymax", 1.0),
                cfg.getDouble("mesh.box.zmax", 1.0)};
  if (!(hi.x > lo.x) || !(hi.y > lo.y) || !(hi.z > lo.z))
    throw ConfigError("mesh.box.max must be greater than mesh.box.min");

  return makeBoxMesh(nx, ny, nz, lo, hi);
}

Mesh makeMeshForSolver(const Config& cfg) {
#ifndef SATURN_USE_MPI
  return makeMeshFromConfig(cfg);
#else
  int rank = 0;
  int size = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (size == 1)
    return makeMeshFromConfig(cfg);

  Mesh mesh;
  if (rank == 0)
    mesh = makeMeshFromConfig(cfg);

  int meta[5] = {mesh.structured ? 1 : 0, mesh.dimension, mesh.nx, mesh.ny,
                 mesh.nz};
  MPI_Bcast(meta, 5, MPI_INT, 0, MPI_COMM_WORLD);
  double bbox[6] = {mesh.bboxMin.x, mesh.bboxMin.y, mesh.bboxMin.z,
                    mesh.bboxMax.x, mesh.bboxMax.y, mesh.bboxMax.z};
  MPI_Bcast(bbox, 6, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  int patchCount = static_cast<int>(mesh.patches.size());
  MPI_Bcast(&patchCount, 1, MPI_INT, 0, MPI_COMM_WORLD);
  std::vector<int> tags(static_cast<std::size_t>(patchCount));
  if (rank == 0)
    for (int p = 0; p < patchCount; ++p)
      tags[static_cast<std::size_t>(p)] = mesh.patches[p].tag;
  MPI_Bcast(tags.data(), patchCount, MPI_INT, 0, MPI_COMM_WORLD);

  std::string blob;
  if (rank == 0) {
    for (const Patch& patch : mesh.patches) {
      blob += patch.name;
      blob.push_back('\0');
    }
  }
  int blobSize = static_cast<int>(blob.size());
  MPI_Bcast(&blobSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
  blob.resize(static_cast<std::size_t>(blobSize));
  MPI_Bcast(blob.data(), blobSize, MPI_CHAR, 0, MPI_COMM_WORLD);

  if (rank != 0) {
    mesh.structured = meta[0] != 0;
    mesh.dimension = meta[1];
    mesh.nx = meta[2];
    mesh.ny = meta[3];
    mesh.nz = meta[4];
    mesh.bboxMin = {bbox[0], bbox[1], bbox[2]};
    mesh.bboxMax = {bbox[3], bbox[4], bbox[5]};
    std::size_t start = 0;
    for (int p = 0; p < patchCount; ++p) {
      const std::size_t end = blob.find('\0', start);
      Patch patch;
      patch.tag = tags[static_cast<std::size_t>(p)];
      patch.name = (end == std::string::npos) ? blob.substr(start)
                                              : blob.substr(start, end - start);
      mesh.patches.push_back(std::move(patch));
      start = (end == std::string::npos) ? blob.size() : end + 1;
    }
  }
  return mesh;
#endif
}

}  // namespace saturn
