#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

#include "mesh/Mesh.hpp"

namespace saturn::test {

/// Build a uniform structured hex box for tests. The mesh is not read from
/// disk: this keeps convergence tests independent of fixture files.
inline Mesh makeUniformBoxMesh(int nx, int ny, int nz, Vec3 lo = {0, 0, 0},
                               Vec3 hi = {1, 1, 1}) {
  if (nx < 1 || ny < 1 || nz < 1)
    throw std::invalid_argument("makeUniformBoxMesh: extents must be positive");

  Mesh mesh;
  const int pnx = nx + 1;
  const int pny = ny + 1;
  const int pnz = nz + 1;
  mesh.points.resize(static_cast<std::size_t>(pnx) * pny * pnz);

  auto pointId = [&](int i, int j, int k) { return i + pnx * (j + pny * k); };
  for (int k = 0; k < pnz; ++k)
    for (int j = 0; j < pny; ++j)
      for (int i = 0; i < pnx; ++i)
        mesh.points[pointId(i, j, k)] = {
            lo.x + (hi.x - lo.x) * static_cast<Real>(i) / nx,
            lo.y + (hi.y - lo.y) * static_cast<Real>(j) / ny,
            lo.z + (hi.z - lo.z) * static_cast<Real>(k) / nz};

  mesh.cells.resize(static_cast<std::size_t>(nx) * ny * nz);
  auto cellId = [&](int i, int j, int k) { return i + nx * (j + ny * k); };
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        Cell& cell = mesh.cells[cellId(i, j, k)];
        cell.type = 5;
        cell.nodes = {pointId(i, j, k),
                      pointId(i + 1, j, k),
                      pointId(i + 1, j + 1, k),
                      pointId(i, j + 1, k),
                      pointId(i, j, k + 1),
                      pointId(i + 1, j, k + 1),
                      pointId(i + 1, j + 1, k + 1),
                      pointId(i, j + 1, k + 1)};
      }
    }
  }

  std::map<std::array<int, 4>, int> faceMap;
  auto addFace = [&](int a, int b, int c, int d, int owner) {
    std::array<int, 4> key{a, b, c, d};
    std::sort(key.begin(), key.end());
    auto it = faceMap.find(key);
    if (it == faceMap.end()) {
      Face face;
      face.nodes = {a, b, c, d};
      face.nNodes = 4;
      face.owner = owner;
      face.neighbour = -1;
      mesh.faces.push_back(face);
      faceMap.emplace(key, static_cast<int>(mesh.faces.size()) - 1);
    } else {
      Face& face = mesh.faces[it->second];
      if (face.neighbour != -1)
        throw std::runtime_error("makeUniformBoxMesh: non-manifold face");
      face.neighbour = owner;
    }
  };

  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int owner = cellId(i, j, k);
        const int n0 = pointId(i, j, k);
        const int n1 = pointId(i + 1, j, k);
        const int n2 = pointId(i + 1, j + 1, k);
        const int n3 = pointId(i, j + 1, k);
        const int n4 = pointId(i, j, k + 1);
        const int n5 = pointId(i + 1, j, k + 1);
        const int n6 = pointId(i + 1, j + 1, k + 1);
        const int n7 = pointId(i, j + 1, k + 1);

        addFace(n0, n4, n7, n3, owner);  // x-
        addFace(n1, n2, n6, n5, owner);  // x+
        addFace(n0, n1, n5, n4, owner);  // y-
        addFace(n3, n7, n6, n2, owner);  // y+
        addFace(n0, n3, n2, n1, owner);  // z-
        addFace(n4, n5, n6, n7, owner);  // z+
      }
    }
  }

  computeGeometry(mesh);

  auto patchIndex = [&](const std::string& name) {
    for (int i = 0; i < static_cast<int>(mesh.patches.size()); ++i)
      if (mesh.patches[i].name == name)
        return i;
    Patch patch;
    patch.name = name;
    patch.tag = static_cast<int>(mesh.patches.size()) + 1;
    mesh.patches.push_back(patch);
    return static_cast<int>(mesh.patches.size()) - 1;
  };

  for (int f = 0; f < static_cast<int>(mesh.faces.size()); ++f) {
    Face& face = mesh.faces[f];
    if (face.neighbour >= 0)
      continue;
    int axis = 0;
    Real best = std::abs(face.normal[0]);
    for (int a = 1; a < 3; ++a) {
      const Real candidate = std::abs(face.normal[a]);
      if (candidate > best) {
        best = candidate;
        axis = a;
      }
    }
    const bool positive = face.normal[axis] > 0;
    std::string name;
    if (axis == 0)
      name = positive ? "xmax" : "xmin";
    else if (axis == 1)
      name = positive ? "ymax" : "ymin";
    else
      name = positive ? "zmax" : "zmin";
    const int patch = patchIndex(name);
    face.patch = patch;
    mesh.patches[patch].faces.push_back(f);
  }

  if (!detectStructured(mesh))
    throw std::runtime_error("makeUniformBoxMesh: structured detection failed");
  return mesh;
}

}  // namespace saturn::test
