#include "mesh/Mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace saturn {
namespace {

constexpr Real kPi = 3.14159265358979323846;

Real tetVolume(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) {
  return dot(b - a, cross(c - a, d - a)) / 6.0;
}

void cellGeometry(const Mesh& mesh, const Cell& cell, Real& volume,
                  Vec3& centroid) {
  volume = 0;
  centroid = Vec3{};
  std::vector<std::array<int, 4>> tets;
  switch (cell.type) {
  case 4:
    tets = {{{0, 1, 2, 3}}};
    break;
  case 5:
    tets = {{{0, 1, 2, 6}}, {{0, 2, 3, 6}}, {{0, 3, 7, 6}},
            {{0, 7, 4, 6}}, {{0, 4, 5, 6}}, {{0, 5, 1, 6}}};
    break;
  case 6:
    tets = {{{0, 1, 2, 3}}, {{1, 2, 3, 4}}, {{2, 3, 4, 5}}};
    break;
  case 7:
    tets = {{{0, 1, 2, 4}}, {{0, 2, 3, 4}}};
    break;
  default:
    break;
  }

  if (tets.empty()) {
    for (Index n : cell.nodes)
      centroid += mesh.points[n];
    if (!cell.nodes.empty())
      centroid = centroid / Real(cell.nodes.size());
    return;
  }

  Vec3 weighted{};
  for (const auto& t : tets) {
    const Vec3& a = mesh.points[cell.nodes[t[0]]];
    const Vec3& b = mesh.points[cell.nodes[t[1]]];
    const Vec3& c = mesh.points[cell.nodes[t[2]]];
    const Vec3& d = mesh.points[cell.nodes[t[3]]];
    const Real v = std::abs(tetVolume(a, b, c, d));
    volume += v;
    weighted += (a + b + c + d) * (0.25 * v);
  }
  if (volume > 0)
    centroid = weighted / volume;
}

Vec3 faceAreaVector(const Mesh& mesh, const Face& face) {
  const Vec3& p0 = mesh.points[face.nodes[0]];
  const Vec3& p1 = mesh.points[face.nodes[1]];
  const Vec3& p2 = mesh.points[face.nodes[2]];
  if (face.nNodes == 3)
    return cross(p1 - p0, p2 - p0) * 0.5;
  const Vec3& p3 = mesh.points[face.nodes[3]];
  return cross(p1 - p0, p2 - p0) * 0.5 + cross(p2 - p0, p3 - p0) * 0.5;
}

Vec3 faceCentroid(const Mesh& mesh, const Face& face) {
  Vec3 c{};
  for (int i = 0; i < face.nNodes; ++i)
    c += mesh.points[face.nodes[i]];
  return c / Real(face.nNodes);
}

Real clampUnit(Real v) {
  return std::max(Real(-1), std::min(Real(1), v));
}

}  // namespace

void computeGeometry(Mesh& mesh) {
  const int nCells = static_cast<int>(mesh.cells.size());
  const int nFaces = static_cast<int>(mesh.faces.size());

  mesh.cellVolume.assign(nCells, 0);
  mesh.cellCentroid.assign(nCells, Vec3{});
  mesh.cellNonOrthogonality.assign(nCells, 0);
  mesh.cellSkewness.assign(nCells, 0);
  mesh.cellAspectRatio.assign(nCells, 0);

  if (!mesh.points.empty()) {
    mesh.bboxMin = mesh.bboxMax = mesh.points[0];
    for (const Vec3& p : mesh.points) {
      mesh.bboxMin.x = std::min(mesh.bboxMin.x, p.x);
      mesh.bboxMin.y = std::min(mesh.bboxMin.y, p.y);
      mesh.bboxMin.z = std::min(mesh.bboxMin.z, p.z);
      mesh.bboxMax.x = std::max(mesh.bboxMax.x, p.x);
      mesh.bboxMax.y = std::max(mesh.bboxMax.y, p.y);
      mesh.bboxMax.z = std::max(mesh.bboxMax.z, p.z);
    }
  }

  for (int c = 0; c < nCells; ++c) {
    cellGeometry(mesh, mesh.cells[c], mesh.cellVolume[c], mesh.cellCentroid[c]);

    Vec3 lo = mesh.points[mesh.cells[c].nodes[0]];
    Vec3 hi = lo;
    for (Index n : mesh.cells[c].nodes) {
      const Vec3& p = mesh.points[n];
      lo.x = std::min(lo.x, p.x);
      lo.y = std::min(lo.y, p.y);
      lo.z = std::min(lo.z, p.z);
      hi.x = std::max(hi.x, p.x);
      hi.y = std::max(hi.y, p.y);
      hi.z = std::max(hi.z, p.z);
    }
    const Real dx = hi.x - lo.x, dy = hi.y - lo.y, dz = hi.z - lo.z;
    const Real minExtent = std::min({dx, dy, dz});
    const Real maxExtent = std::max({dx, dy, dz});
    mesh.cellAspectRatio[c] = minExtent > 0 ? maxExtent / minExtent
                                            : std::numeric_limits<Real>::max();
  }

  for (int f = 0; f < nFaces; ++f) {
    Face& face = mesh.faces[f];
    face.centroid = faceCentroid(mesh, face);
    const Vec3 av = faceAreaVector(mesh, face);
    face.area = norm(av);
    face.normal = face.area > 0 ? av / face.area : Vec3{};

    if (face.owner < 0)
      continue;
    if (face.neighbour >= 0) {
      const Vec3 d =
          mesh.cellCentroid[face.neighbour] - mesh.cellCentroid[face.owner];
      if (dot(face.normal, d) < 0)
        face.normal = -face.normal;
    } else {
      const Vec3 d = face.centroid - mesh.cellCentroid[face.owner];
      if (dot(face.normal, d) < 0)
        face.normal = -face.normal;
    }
  }

  for (int f = 0; f < nFaces; ++f) {
    const Face& face = mesh.faces[f];
    if (face.owner < 0 || face.neighbour < 0)
      continue;
    const int a = face.owner;
    const int b = face.neighbour;
    const Vec3 d = mesh.cellCentroid[b] - mesh.cellCentroid[a];
    const Real len = norm(d);
    if (len <= 0)
      continue;

    const Real cosTheta = std::abs(dot(face.normal, d)) / len;
    const Real angle = std::acos(clampUnit(cosTheta)) * 180.0 / kPi;
    mesh.cellNonOrthogonality[a] =
        std::max(mesh.cellNonOrthogonality[a], angle);
    mesh.cellNonOrthogonality[b] =
        std::max(mesh.cellNonOrthogonality[b], angle);

    const Vec3 da = face.centroid - mesh.cellCentroid[a];
    const Real t = dot(da, d) / (len * len);
    const Vec3 closest = mesh.cellCentroid[a] + d * t;
    const Real skew = norm(face.centroid - closest) / len;
    mesh.cellSkewness[a] = std::max(mesh.cellSkewness[a], skew);
    mesh.cellSkewness[b] = std::max(mesh.cellSkewness[b], skew);
  }
}

bool detectStructured(Mesh& mesh) {
  mesh.structured = false;
  mesh.cellIndex.clear();
  mesh.indexToCell.clear();
  mesh.nx = mesh.ny = mesh.nz = 0;

  const int n = static_cast<int>(mesh.cells.size());
  if (n == 0)
    return false;

  std::vector<std::vector<int>> cellFaces(n);
  for (const Cell& c : mesh.cells) {
    if (c.type != 5 || c.nodes.size() != 8)
      return false;
  }
  for (int f = 0; f < static_cast<int>(mesh.faces.size()); ++f) {
    const Face& face = mesh.faces[f];
    if (face.owner >= 0)
      cellFaces[face.owner].push_back(f);
    if (face.neighbour >= 0)
      cellFaces[face.neighbour].push_back(f);
  }
  for (const auto& cf : cellFaces)
    if (cf.size() != 6)
      return false;

  std::array<int, 3> plus{-1, -1, -1};
  std::array<int, 3> minus{-1, -1, -1};
  for (int f : cellFaces[0]) {
    const Vec3& nn = mesh.faces[f].normal;
    int axis = 0;
    if (std::abs(nn.y) > std::abs(nn[axis]))
      axis = 1;
    if (std::abs(nn.z) > std::abs(nn[axis]))
      axis = 2;
    if (nn[axis] > 0) {
      if (plus[axis] != -1)
        return false;
      plus[axis] = f;
    } else {
      if (minus[axis] != -1)
        return false;
      minus[axis] = f;
    }
  }
  for (int a = 0; a < 3; ++a)
    if (plus[a] == -1 || minus[a] == -1)
      return false;

  std::array<Vec3, 3> axisDir;
  for (int a = 0; a < 3; ++a)
    axisDir[a] = normalized(mesh.faces[plus[a]].normal);

  constexpr int kUnset = std::numeric_limits<int>::min();
  std::vector<std::array<int, 3>> idx(n, {kUnset, kUnset, kUnset});
  std::queue<int> q;
  idx[0] = {0, 0, 0};
  q.push(0);

  while (!q.empty()) {
    const int c = q.front();
    q.pop();
    for (int f : cellFaces[c]) {
      const Face& face = mesh.faces[f];
      const int other = face.owner == c ? face.neighbour : face.owner;
      if (other < 0)
        continue;

      int axis = 0;
      Real best = std::abs(dot(face.normal, axisDir[0]));
      for (int a = 1; a < 3; ++a) {
        const Real d = std::abs(dot(face.normal, axisDir[a]));
        if (d > best) {
          best = d;
          axis = a;
        }
      }
      if (best < 0.8)
        return false;
      Real s = dot(face.normal, axisDir[axis]);
      if (face.neighbour == c)
        s = -s;
      const int sign = s > 0 ? 1 : -1;
      auto next = idx[c];
      next[axis] += sign;

      if (idx[other][0] == kUnset) {
        idx[other] = next;
        q.push(other);
      } else if (idx[other] != next) {
        return false;
      }
    }
  }

  int lo[3] = {std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
               std::numeric_limits<int>::max()};
  int hi[3] = {std::numeric_limits<int>::min(), std::numeric_limits<int>::min(),
               std::numeric_limits<int>::min()};
  for (int c = 0; c < n; ++c) {
    if (idx[c][0] == kUnset)
      return false;
    for (int a = 0; a < 3; ++a) {
      lo[a] = std::min(lo[a], idx[c][a]);
      hi[a] = std::max(hi[a], idx[c][a]);
    }
  }

  const int nx = hi[0] - lo[0] + 1;
  const int ny = hi[1] - lo[1] + 1;
  const int nz = hi[2] - lo[2] + 1;
  if (static_cast<BigIndex>(nx) * ny * nz != n)
    return false;

  std::vector<std::array<int, 3>> normalized(n);
  std::vector<Index> indexToCell(static_cast<std::size_t>(nx) * ny * nz, -1);
  for (int c = 0; c < n; ++c) {
    normalized[c] = {idx[c][0] - lo[0], idx[c][1] - lo[1], idx[c][2] - lo[2]};
    const int i = normalized[c][0], j = normalized[c][1], k = normalized[c][2];
    if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz)
      return false;
    const int lin = i + nx * (j + ny * k);
    if (indexToCell[lin] != -1)
      return false;
    indexToCell[lin] = c;
  }

  for (int f = 0; f < static_cast<int>(mesh.faces.size()); ++f) {
    const Face& face = mesh.faces[f];
    if (face.owner < 0 || face.neighbour < 0)
      continue;
    int diff = 0;
    int nonzero = 0;
    for (int a = 0; a < 3; ++a) {
      const int d = normalized[face.neighbour][a] - normalized[face.owner][a];
      if (d != 0) {
        diff = d;
        ++nonzero;
      }
    }
    if (nonzero != 1 || std::abs(diff) != 1)
      return false;
  }

  mesh.structured = true;
  mesh.nx = nx;
  mesh.ny = ny;
  mesh.nz = nz;
  mesh.cellIndex = std::move(normalized);
  mesh.indexToCell = std::move(indexToCell);
  const int active = (nx > 1 ? 1 : 0) + (ny > 1 ? 1 : 0) + (nz > 1 ? 1 : 0);
  mesh.dimension = std::max(1, active);
  return true;
}

}  // namespace saturn
