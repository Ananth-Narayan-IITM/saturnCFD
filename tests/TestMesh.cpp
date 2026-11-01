#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "mesh/CheckMesh.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/VtuWriter.hpp"

using namespace saturn;

namespace {
std::string meshPath(const std::string& name) {
  return std::string(SATURN_TEST_DATA_DIR) + "/" + name;
}
}  // namespace

TEST_CASE("mesh: box2x2x2 passes [mesh]") {
  Mesh mesh = readGmsh(meshPath("box2x2x2.msh"));
  const MeshReport r = checkMesh(mesh, "box2x2x2.msh");
  CHECK(r.pass());
  CHECK(r.nCells == 8);
  CHECK(r.nPoints == 27);
  CHECK(r.nFaces == 36);
  CHECK(r.structured);
  CHECK(r.nx == 2);
  CHECK(r.ny == 2);
  CHECK(r.nz == 2);
  CHECK(r.dimension == 3);
  CHECK(r.patches.size() == 6);
  CHECK(r.volumeTotal == doctest::Approx(1.0));
}

TEST_CASE("mesh: thin2d is reported as 2D [mesh]") {
  Mesh mesh = readGmsh(meshPath("thin2d.msh"));
  const MeshReport r = checkMesh(mesh, "thin2d.msh");
  CHECK(r.pass());
  CHECK(r.structured);
  CHECK(r.dimension == 2);
  CHECK(r.nz == 1);
  CHECK(r.nCells == 4);
}

TEST_CASE("mesh: tetrahedra are rejected [mesh]") {
  Mesh mesh = readGmsh(meshPath("tet.msh"));
  const MeshReport r = checkMesh(mesh, "tet.msh");
  CHECK(!r.pass());
  CHECK(!r.structured);
  bool found = false;
  for (const auto& e : r.errors)
    if (e.find("unsupported element type") != std::string::npos)
      found = true;
  CHECK(found);
}

TEST_CASE("mesh: default VTK writes only vtu [mesh]") {
  Mesh mesh = readGmsh(meshPath("box2x2x2.msh"));
  const auto dir =
      std::filesystem::temp_directory_path() / "saturncfd_vtk_default";
  std::filesystem::remove_all(dir);
  const std::string prefix = (dir / "mesh").string();
  writeMeshVtk(mesh, prefix, false);

  CHECK(std::filesystem::exists(prefix + ".vtu"));
  CHECK(!std::filesystem::exists(prefix + ".pvd"));
  CHECK(!std::filesystem::exists(prefix + "_xmin.vtp"));
  std::filesystem::remove_all(dir);
}

TEST_CASE("mesh: writeVTK adds patches and pvd [mesh]") {
  Mesh mesh = readGmsh(meshPath("box2x2x2.msh"));
  const auto dir =
      std::filesystem::temp_directory_path() / "saturncfd_vtk_patches";
  std::filesystem::remove_all(dir);
  const std::string prefix = (dir / "mesh").string();
  writeMeshVtk(mesh, prefix, true);

  CHECK(std::filesystem::exists(prefix + ".vtu"));
  CHECK(std::filesystem::exists(prefix + ".pvd"));
  CHECK(std::filesystem::exists(prefix + "_xmin.vtp"));
  CHECK(std::filesystem::exists(prefix + "_zmax.vtp"));
  std::filesystem::remove_all(dir);
}

TEST_CASE("mesh: multi-field vtu keeps all arrays in one file [mesh]") {
  Mesh mesh = readGmsh(meshPath("box2x2x2.msh"));
  const auto dir =
      std::filesystem::temp_directory_path() / "saturncfd_vtk_multifield";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string path = (dir / "fields.vtu").string();
  const std::vector<Real> values(mesh.cells.size(), Real(1));
  writeCellFieldsVtu(mesh,
                     {CellFieldVtu{"u", values}, CellFieldVtu{"v", values},
                      CellFieldVtu{"w", values}, CellFieldVtu{"p", values}},
                     path);

  std::ifstream in(path);
  REQUIRE(in.good());
  std::ostringstream text;
  text << in.rdbuf();
  const std::string body = text.str();
  CHECK(body.find("Name=\"u\"") != std::string::npos);
  CHECK(body.find("Name=\"v\"") != std::string::npos);
  CHECK(body.find("Name=\"w\"") != std::string::npos);
  CHECK(body.find("Name=\"p\"") != std::string::npos);
  std::filesystem::remove_all(dir);
}
