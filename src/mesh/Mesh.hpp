#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/Types.hpp"

namespace saturn {

/// A volume cell. v1.0 accepts hexahedra; other types are kept so that the
/// reader can report a precise rejection instead of failing to parse.
struct Cell {
  int type = 0;  ///< Gmsh element type: 4=tet, 5=hex, 6=prism, 7=pyramid
  std::vector<Index> nodes;
};

/// A face of a volume cell. Triangles use nNodes=3, quads nNodes=4.
struct Face {
  std::array<Index, 4> nodes{};
  int nNodes = 0;
  Index owner = -1;
  Index neighbour = -1;
  int patch = -1;
  Vec3 normal;
  Vec3 centroid;
  Real area = 0;
};

/// A named boundary patch from a Gmsh physical group.
struct Patch {
  std::string name;
  int tag = 0;
  std::vector<Index> faces;
};

/// General mesh produced by the reader. `detectStructured()` fills the
/// (i,j,k) view; solvers use that view, not the raw face list.
struct Mesh {
  std::vector<Vec3> points;
  std::vector<Cell> cells;
  std::vector<Face> faces;
  std::vector<Patch> patches;

  std::vector<Real> cellVolume;
  std::vector<Vec3> cellCentroid;
  std::vector<Real> cellNonOrthogonality;  ///< max face angle, degrees
  std::vector<Real> cellSkewness;          ///< max face skewness
  std::vector<Real> cellAspectRatio;

  bool structured = false;
  int dimension = 3;
  int nx = 0;
  int ny = 0;
  int nz = 0;

  /// (i,j,k) for each cell when structured; empty otherwise.
  std::vector<std::array<int, 3>> cellIndex;
  /// Linear index -> cell id, row-major in (i,j,k).
  std::vector<Index> indexToCell;

  Vec3 bboxMin;
  Vec3 bboxMax;

  Index cellAt(int i, int j, int k) const {
    if (!structured || i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz)
      return -1;
    return indexToCell[static_cast<std::size_t>(i) +
                       static_cast<std::size_t>(nx) *
                           (static_cast<std::size_t>(j) +
                            static_cast<std::size_t>(ny) *
                                static_cast<std::size_t>(k))];
  }
};

/// Read a Gmsh 2.2 ASCII mesh and build unique faces and patches.
/// Throws MeshError for malformed input or an unmatched boundary element.
Mesh readGmsh(const std::string& path);

/// Compute volumes, centroids, face areas/normals and per-cell quality.
void computeGeometry(Mesh& mesh);

/// Try to assign a contiguous (i,j,k) to every cell. On success fills
/// `structured`, `nx/ny/nz`, `cellIndex`, `indexToCell` and `dimension`.
bool detectStructured(Mesh& mesh);

}  // namespace saturn
