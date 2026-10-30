#include "mesh/VtuWriter.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Error.hpp"

namespace saturn {
namespace {

std::string safeName(const std::string& name) {
  std::string out;
  for (unsigned char c : name) {
    if (std::isalnum(c))
      out += static_cast<char>(c);
  }
  if (out.empty())
    out = "patch";
  if (std::isdigit(static_cast<unsigned char>(out.front())))
    out = "patch" + out;
  return out;
}

int vtkCellType(int gmshType) {
  switch (gmshType) {
  case 4:
    return 10;  // tetra
  case 5:
    return 12;  // hexahedron
  case 6:
    return 13;  // wedge
  case 7:
    return 14;  // pyramid
  default:
    return 12;
  }
}

void openOrThrow(std::ofstream& os, const std::string& path) {
  os.open(path);
  if (!os)
    throw MeshError("cannot write VTK file '" + path + "'");
}

}  // namespace

void writeMeshVtk(const Mesh& mesh, const std::string& prefix,
                  bool withPatches) {
  const std::filesystem::path base(prefix);
  if (base.has_parent_path())
    std::filesystem::create_directories(base.parent_path());

  const std::string vtu = prefix + ".vtu";
  {
    std::ofstream os;
    openOrThrow(os, vtu);
    os << std::setprecision(15);
    os << "<?xml version=\"1.0\"?>\n";
    os << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" "
          "byte_order=\"LittleEndian\">\n";
    os << "  <UnstructuredGrid>\n";
    os << "    <Piece NumberOfPoints=\"" << mesh.points.size()
       << "\" NumberOfCells=\"" << mesh.cells.size() << "\">\n";
    os << "      <Points>\n";
    os << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" "
          "format=\"ascii\">\n";
    for (const Vec3& p : mesh.points)
      os << p.x << " " << p.y << " " << p.z << "\n";
    os << "        </DataArray>\n";
    os << "      </Points>\n";
    os << "      <Cells>\n";
    os << "        <DataArray type=\"Int32\" Name=\"connectivity\" "
          "format=\"ascii\">\n";
    for (const Cell& c : mesh.cells) {
      for (std::size_t i = 0; i < c.nodes.size(); ++i) {
        if (i)
          os << " ";
        os << c.nodes[i];
      }
      os << "\n";
    }
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"Int32\" Name=\"offsets\" "
          "format=\"ascii\">\n";
    int offset = 0;
    for (const Cell& c : mesh.cells) {
      offset += static_cast<int>(c.nodes.size());
      os << offset << "\n";
    }
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"UInt8\" Name=\"types\" "
          "format=\"ascii\">\n";
    for (const Cell& c : mesh.cells)
      os << vtkCellType(c.type) << "\n";
    os << "        </DataArray>\n";
    os << "      </Cells>\n";
    os << "      <CellData>\n";
    os << "        <DataArray type=\"Int32\" Name=\"cellId\" "
          "format=\"ascii\">\n";
    for (int i = 0; i < static_cast<int>(mesh.cells.size()); ++i)
      os << i << "\n";
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"Float64\" Name=\"volume\" "
          "format=\"ascii\">\n";
    for (Real v : mesh.cellVolume)
      os << v << "\n";
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"Float64\" Name=\"nonOrthogonality\" "
          "format=\"ascii\">\n";
    for (Real v : mesh.cellNonOrthogonality)
      os << v << "\n";
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"Float64\" Name=\"skewness\" "
          "format=\"ascii\">\n";
    for (Real v : mesh.cellSkewness)
      os << v << "\n";
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"Float64\" Name=\"aspectRatio\" "
          "format=\"ascii\">\n";
    for (Real v : mesh.cellAspectRatio)
      os << v << "\n";
    os << "        </DataArray>\n";
    os << "      </CellData>\n";
    os << "    </Piece>\n";
    os << "  </UnstructuredGrid>\n";
    os << "</VTKFile>\n";
  }

  if (!withPatches)
    return;

  std::vector<std::string> patchFiles;
  for (int p = 0; p < static_cast<int>(mesh.patches.size()); ++p) {
    const Patch& patch = mesh.patches[p];
    if (patch.faces.empty())
      continue;

    std::vector<Index> pointMap(mesh.points.size(), -1);
    std::vector<Index> localPoints;
    std::vector<std::array<Index, 4>> polygons;
    std::vector<int> polygonSizes;

    for (Index fi : patch.faces) {
      const Face& face = mesh.faces[fi];
      std::array<Index, 4> poly{-1, -1, -1, -1};
      for (int i = 0; i < face.nNodes; ++i) {
        const Index gid = face.nodes[i];
        if (pointMap[gid] < 0) {
          pointMap[gid] = static_cast<Index>(localPoints.size());
          localPoints.push_back(gid);
        }
        poly[i] = pointMap[gid];
      }
      polygons.push_back(poly);
      polygonSizes.push_back(face.nNodes);
    }

    const std::string patchFile = prefix + "_" + safeName(patch.name) + ".vtp";
    patchFiles.push_back(patchFile);

    std::ofstream os;
    openOrThrow(os, patchFile);
    os << std::setprecision(15);
    os << "<?xml version=\"1.0\"?>\n";
    os << "<VTKFile type=\"PolyData\" version=\"0.1\" "
          "byte_order=\"LittleEndian\">\n";
    os << "  <PolyData>\n";
    os << "    <Piece NumberOfPoints=\"" << localPoints.size()
       << "\" NumberOfVerts=\"0\" NumberOfLines=\"0\" NumberOfStrips=\"0\" "
          "NumberOfPolys=\""
       << polygons.size() << "\">\n";
    os << "      <Points>\n";
    os << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" "
          "format=\"ascii\">\n";
    for (Index gid : localPoints) {
      const Vec3& q = mesh.points[gid];
      os << q.x << " " << q.y << " " << q.z << "\n";
    }
    os << "        </DataArray>\n";
    os << "      </Points>\n";
    os << "      <Polys>\n";
    os << "        <DataArray type=\"Int32\" Name=\"connectivity\" "
          "format=\"ascii\">\n";
    for (const auto& poly : polygons) {
      const int n = polygonSizes[&poly - polygons.data()];
      for (int i = 0; i < n; ++i) {
        if (i)
          os << " ";
        os << poly[i];
      }
      os << "\n";
    }
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"Int32\" Name=\"offsets\" "
          "format=\"ascii\">\n";
    int off = 0;
    for (int n : polygonSizes) {
      off += n;
      os << off << "\n";
    }
    os << "        </DataArray>\n";
    os << "        <DataArray type=\"UInt8\" Name=\"types\" "
          "format=\"ascii\">\n";
    for (int n : polygonSizes)
      os << (n == 3 ? 5 : 9) << "\n";
    os << "        </DataArray>\n";
    os << "      </Polys>\n";
    os << "      <CellData>\n";
    os << "        <DataArray type=\"Int32\" Name=\"patchId\" "
          "format=\"ascii\">\n";
    for (std::size_t i = 0; i < polygons.size(); ++i)
      os << p << "\n";
    os << "        </DataArray>\n";
    os << "      </CellData>\n";
    os << "    </Piece>\n";
    os << "  </PolyData>\n";
    os << "</VTKFile>\n";
  }

  {
    std::ofstream os;
    openOrThrow(os, prefix + ".pvd");
    os << "<?xml version=\"1.0\"?>\n";
    os << "<VTKFile type=\"Collection\" version=\"0.1\" "
          "byte_order=\"LittleEndian\">\n";
    os << "  <Collection>\n";
    os << "    <DataSet timestep=\"0\" group=\"\" part=\"0\" file=\""
       << std::filesystem::path(vtu).filename().string() << "\"/>\n";
    for (std::size_t i = 0; i < patchFiles.size(); ++i) {
      os << "    <DataSet timestep=\"0\" group=\"\" part=\"" << (i + 1)
         << "\" file=\""
         << std::filesystem::path(patchFiles[i]).filename().string()
         << "\"/>\n";
    }
    os << "  </Collection>\n";
    os << "</VTKFile>\n";
  }
}

}  // namespace saturn
