#include "mesh/Mesh.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Error.hpp"

namespace saturn {
namespace {

struct VolumeElement {
  int type = 0;
  int physical = 0;
  std::vector<Index> nodes;
};

struct SurfaceElement {
  int type = 0;
  int physical = 0;
  std::vector<Index> nodes;
};

using FaceKey = std::array<Index, 4>;

FaceKey faceKey(const std::vector<Index>& nodes) {
  FaceKey key{-1, -1, -1, -1};
  for (std::size_t i = 0; i < nodes.size(); ++i)
    key[i] = nodes[i];
  std::sort(key.begin(), key.end());
  return key;
}

void addFace(Mesh& mesh, std::map<FaceKey, int>& faceMap, Index owner,
             const std::vector<Index>& nodes) {
  const FaceKey key = faceKey(nodes);
  auto it = faceMap.find(key);
  if (it == faceMap.end()) {
    Face f;
    f.nNodes = static_cast<int>(nodes.size());
    for (int i = 0; i < f.nNodes; ++i)
      f.nodes[i] = nodes[i];
    f.owner = owner;
    const int id = static_cast<int>(mesh.faces.size());
    mesh.faces.push_back(f);
    faceMap.emplace(key, id);
    return;
  }

  Face& f = mesh.faces[it->second];
  if (f.neighbour != -1)
    throw MeshError("non-manifold face: more than two cells share a face");
  f.neighbour = owner;
}

std::vector<std::vector<Index>> cellFaceNodes(const Cell& cell) {
  switch (cell.type) {
  case 5:  // hexahedron
    return {{cell.nodes[0], cell.nodes[1], cell.nodes[2], cell.nodes[3]},
            {cell.nodes[4], cell.nodes[5], cell.nodes[6], cell.nodes[7]},
            {cell.nodes[0], cell.nodes[1], cell.nodes[5], cell.nodes[4]},
            {cell.nodes[1], cell.nodes[2], cell.nodes[6], cell.nodes[5]},
            {cell.nodes[2], cell.nodes[3], cell.nodes[7], cell.nodes[6]},
            {cell.nodes[3], cell.nodes[0], cell.nodes[4], cell.nodes[7]}};
  case 4:  // tetrahedron
    return {{cell.nodes[0], cell.nodes[2], cell.nodes[1]},
            {cell.nodes[0], cell.nodes[1], cell.nodes[3]},
            {cell.nodes[1], cell.nodes[2], cell.nodes[3]},
            {cell.nodes[0], cell.nodes[3], cell.nodes[2]}};
  case 6:  // prism
    return {{cell.nodes[0], cell.nodes[1], cell.nodes[2]},
            {cell.nodes[3], cell.nodes[4], cell.nodes[5]},
            {cell.nodes[0], cell.nodes[1], cell.nodes[4], cell.nodes[3]},
            {cell.nodes[1], cell.nodes[2], cell.nodes[5], cell.nodes[4]},
            {cell.nodes[2], cell.nodes[0], cell.nodes[3], cell.nodes[5]}};
  case 7:  // pyramid
    return {{cell.nodes[0], cell.nodes[1], cell.nodes[2], cell.nodes[3]},
            {cell.nodes[0], cell.nodes[1], cell.nodes[4]},
            {cell.nodes[1], cell.nodes[2], cell.nodes[4]},
            {cell.nodes[2], cell.nodes[3], cell.nodes[4]},
            {cell.nodes[3], cell.nodes[0], cell.nodes[4]}};
  default:
    return {};
  }
}

std::string patchName(const std::map<int, std::string>& names, int tag) {
  auto it = names.find(tag);
  if (it != names.end())
    return it->second;
  if (tag == 0)
    return "boundary";
  return "physical" + std::to_string(tag);
}

int getPatch(Mesh& mesh, const std::map<int, std::string>& names, int tag) {
  const std::string name = patchName(names, tag);
  for (int i = 0; i < static_cast<int>(mesh.patches.size()); ++i)
    if (mesh.patches[i].tag == tag)
      return i;
  Patch p;
  p.name = name;
  p.tag = tag;
  mesh.patches.push_back(p);
  return static_cast<int>(mesh.patches.size()) - 1;
}

}  // namespace

Mesh readGmsh(const std::string& path) {
  std::ifstream in(path);
  if (!in)
    throw MeshError("cannot open mesh file '" + path + "'");

  Mesh mesh;
  std::map<int, std::string> physicalNames;
  std::vector<VolumeElement> volumeElements;
  std::vector<SurfaceElement> surfaceElements;
  std::vector<std::pair<Index, Vec3>> rawNodes;

  std::string line;
  bool sawFormat = false;

  while (std::getline(in, line)) {
    if (line == "$MeshFormat") {
      std::string version;
      int binary = 0;
      int wordSize = 0;
      in >> version >> binary >> wordSize;
      std::getline(in, line);  // consume newline
      if (version != "2.2")
        throw MeshError("unsupported Gmsh version '" + version +
                        "'; export with 'gmsh -format msh2'");
      if (binary != 0)
        throw MeshError(
            "binary Gmsh files are not supported in v1.0; export ASCII msh2");
      sawFormat = true;
      continue;
    }

    if (line == "$PhysicalNames") {
      int count = 0;
      in >> count;
      std::getline(in, line);
      for (int i = 0; i < count; ++i) {
        int dim = 0;
        int tag = 0;
        std::string name;
        in >> dim >> tag >> std::quoted(name);
        std::getline(in, line);
        physicalNames[tag] = name;
      }
      continue;
    }

    if (line == "$Nodes") {
      int count = 0;
      in >> count;
      std::getline(in, line);
      rawNodes.reserve(count);
      for (int i = 0; i < count; ++i) {
        Index id = 0;
        Real x = 0, y = 0, z = 0;
        in >> id >> x >> y >> z;
        std::getline(in, line);
        rawNodes.emplace_back(id, Vec3{x, y, z});
      }
      continue;
    }

    if (line == "$Elements") {
      int count = 0;
      in >> count;
      std::getline(in, line);
      for (int i = 0; i < count; ++i) {
        Index id = 0;
        int type = 0;
        int ntags = 0;
        in >> id >> type >> ntags;
        std::vector<int> tags(static_cast<std::size_t>(ntags));
        for (int t = 0; t < ntags; ++t)
          in >> tags[t];
        const int physical = ntags > 0 ? tags[0] : 0;

        int nNodes = 0;
        if (type == 2)
          nNodes = 3;
        else if (type == 3)
          nNodes = 4;
        else if (type == 4)
          nNodes = 4;
        else if (type == 5)
          nNodes = 8;
        else if (type == 6)
          nNodes = 6;
        else if (type == 7)
          nNodes = 5;

        std::vector<Index> nodes;
        nodes.reserve(nNodes);
        for (int n = 0; n < nNodes; ++n) {
          Index node = 0;
          in >> node;
          nodes.push_back(node);
        }
        std::getline(in, line);

        if (nNodes == 0)
          continue;
        if (type == 2 || type == 3) {
          surfaceElements.push_back({type, physical, std::move(nodes)});
        } else if (type >= 4 && type <= 7) {
          volumeElements.push_back({type, physical, std::move(nodes)});
        }
      }
      continue;
    }
  }

  if (!sawFormat)
    throw MeshError("'" + path + "' has no $MeshFormat block");
  if (volumeElements.empty())
    throw MeshError("'" + path + "' has no volume cells");

  // Map Gmsh node ids to dense 0-based ids.
  std::unordered_map<Index, Index> nodeMap;
  nodeMap.reserve(rawNodes.size() * 2);
  mesh.points.reserve(rawNodes.size());
  for (const auto& [id, x] : rawNodes) {
    if (nodeMap.count(id))
      throw MeshError("duplicate node id " + std::to_string(id));
    const Index index = static_cast<Index>(mesh.points.size());
    nodeMap[id] = index;
    mesh.points.push_back(x);
  }

  auto lookup = [&](Index id) -> Index {
    auto it = nodeMap.find(id);
    if (it == nodeMap.end())
      throw MeshError("element references missing node id " +
                      std::to_string(id));
    return it->second;
  };

  for (auto& e : volumeElements) {
    Cell cell;
    cell.type = e.type;
    for (Index id : e.nodes)
      cell.nodes.push_back(lookup(id));
    mesh.cells.push_back(std::move(cell));
  }

  std::map<FaceKey, int> faceMap;
  for (int c = 0; c < static_cast<int>(mesh.cells.size()); ++c) {
    const auto faces = cellFaceNodes(mesh.cells[c]);
    if (faces.empty())
      throw MeshError("unsupported volume element type " +
                      std::to_string(mesh.cells[c].type));
    for (const auto& nodes : faces)
      addFace(mesh, faceMap, c, nodes);
  }

  // Assign boundary faces from Gmsh surface elements.
  for (const auto& e : surfaceElements) {
    std::vector<Index> nodes;
    for (Index id : e.nodes)
      nodes.push_back(lookup(id));
    const FaceKey key = faceKey(nodes);
    auto it = faceMap.find(key);
    if (it == faceMap.end())
      throw MeshError(
          "boundary surface element does not match any volume face");
    Face& f = mesh.faces[it->second];
    if (f.neighbour != -1)
      throw MeshError("boundary surface element lies on an internal face");
    const int patch = getPatch(mesh, physicalNames, e.physical);
    if (f.patch != -1 && f.patch != patch)
      throw MeshError("face assigned to more than one boundary patch");
    f.patch = patch;
    mesh.patches[patch].faces.push_back(it->second);
  }

  // If the mesh has no physical surface groups, create one default patch for
  // all remaining boundary faces. Otherwise leave unassigned faces at -1 so
  // checkMesh reports them.
  if (surfaceElements.empty()) {
    Patch p;
    p.name = "boundary";
    mesh.patches.push_back(p);
    for (int i = 0; i < static_cast<int>(mesh.faces.size()); ++i) {
      Face& f = mesh.faces[i];
      if (f.neighbour == -1) {
        f.patch = 0;
        mesh.patches[0].faces.push_back(i);
      }
    }
  }

  return mesh;
}

}  // namespace saturn
