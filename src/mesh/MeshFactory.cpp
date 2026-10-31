#include "mesh/MeshFactory.hpp"

#include <string>

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

}  // namespace saturn
