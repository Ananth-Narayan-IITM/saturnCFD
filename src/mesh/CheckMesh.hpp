#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/Types.hpp"
#include "mesh/Mesh.hpp"

namespace saturn {

struct MeshReport {
  std::string file;
  std::string format = "Gmsh 2.2 ASCII";

  int nCells = 0;
  int nPoints = 0;
  int nFaces = 0;
  int nInternalFaces = 0;
  int nBoundaryFaces = 0;
  std::vector<std::pair<std::string, int>> patches;

  Vec3 bboxMin;
  Vec3 bboxMax;
  Real volumeMin = 0;
  Real volumeMax = 0;
  Real volumeTotal = 0;
  Real areaMin = 0;
  Real areaMax = 0;

  Real nonOrthoMax = 0;
  Real nonOrthoAvg = 0;
  Real skewnessMax = 0;
  Real skewnessAvg = 0;
  Real aspectRatioMax = 0;
  Real aspectRatioAvg = 0;

  bool structured = false;
  int dimension = 3;
  int nx = 0;
  int ny = 0;
  int nz = 0;

  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  bool pass() const { return errors.empty(); }
  std::string toText() const;
  std::string toJson() const;
};

/// Compute geometry, detect structuredness and produce the report.
/// `file` is only used for display.
MeshReport checkMesh(Mesh& mesh, const std::string& file = "");

}  // namespace saturn
