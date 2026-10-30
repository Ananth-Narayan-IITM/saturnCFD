#include "mesh/CheckMesh.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>

namespace saturn {
namespace {

Real average(const std::vector<Real>& v) {
  if (v.empty())
    return 0;
  const Real sum = std::accumulate(v.begin(), v.end(), Real(0));
  return sum / Real(v.size());
}

Real maximum(const std::vector<Real>& v) {
  if (v.empty())
    return 0;
  return *std::max_element(v.begin(), v.end());
}

std::string jsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
    }
  }
  return out;
}

std::string number(Real v) {
  std::ostringstream os;
  os << std::setprecision(10) << v;
  return os.str();
}

}  // namespace

MeshReport checkMesh(Mesh& mesh, const std::string& file) {
  MeshReport r;
  r.file = file;

  computeGeometry(mesh);
  detectStructured(mesh);

  r.nCells = static_cast<int>(mesh.cells.size());
  r.nPoints = static_cast<int>(mesh.points.size());
  r.nFaces = static_cast<int>(mesh.faces.size());
  for (const Face& f : mesh.faces) {
    if (f.neighbour >= 0)
      ++r.nInternalFaces;
    else
      ++r.nBoundaryFaces;
  }
  for (const Patch& p : mesh.patches)
    r.patches.emplace_back(p.name, static_cast<int>(p.faces.size()));

  r.bboxMin = mesh.bboxMin;
  r.bboxMax = mesh.bboxMax;
  r.structured = mesh.structured;
  r.dimension = mesh.dimension;
  r.nx = mesh.nx;
  r.ny = mesh.ny;
  r.nz = mesh.nz;

  if (!mesh.cellVolume.empty()) {
    r.volumeMin =
        *std::min_element(mesh.cellVolume.begin(), mesh.cellVolume.end());
    r.volumeMax =
        *std::max_element(mesh.cellVolume.begin(), mesh.cellVolume.end());
    r.volumeTotal = std::accumulate(mesh.cellVolume.begin(),
                                    mesh.cellVolume.end(), Real(0));
  }

  bool haveArea = false;
  for (const Face& f : mesh.faces) {
    if (f.area <= 0)
      continue;
    if (!haveArea) {
      r.areaMin = r.areaMax = f.area;
      haveArea = true;
    } else {
      r.areaMin = std::min(r.areaMin, f.area);
      r.areaMax = std::max(r.areaMax, f.area);
    }
  }

  r.nonOrthoMax = maximum(mesh.cellNonOrthogonality);
  r.nonOrthoAvg = average(mesh.cellNonOrthogonality);
  r.skewnessMax = maximum(mesh.cellSkewness);
  r.skewnessAvg = average(mesh.cellSkewness);
  r.aspectRatioMax = maximum(mesh.cellAspectRatio);
  r.aspectRatioAvg = average(mesh.cellAspectRatio);

  if (mesh.cells.empty())
    r.errors.push_back("mesh has no volume cells");
  for (const Cell& c : mesh.cells) {
    if (c.type != 5)
      r.errors.push_back("unsupported element type " + std::to_string(c.type) +
                         "; v1.0 accepts hexahedra only");
  }
  for (int i = 0; i < static_cast<int>(mesh.cellVolume.size()); ++i) {
    if (mesh.cellVolume[i] <= 0)
      r.errors.push_back("cell " + std::to_string(i) +
                         " has non-positive volume");
  }
  for (int i = 0; i < static_cast<int>(mesh.faces.size()); ++i) {
    const Face& f = mesh.faces[i];
    if (f.area <= 0)
      r.errors.push_back("face " + std::to_string(i) + " has zero area");
    if (f.neighbour < 0 && f.patch < 0)
      r.errors.push_back("boundary face " + std::to_string(i) +
                         " is not assigned to a patch");
  }
  if (!mesh.structured)
    r.errors.push_back(
        "mesh is not structured; only contiguous structured hexahedral "
        "meshes are supported in v1.0");

  if (r.nonOrthoMax > 60)
    r.warnings.push_back("maximum non-orthogonality exceeds 60 degrees");
  if (r.skewnessMax > 0.5)
    r.warnings.push_back("maximum skewness exceeds 0.5");
  if (r.aspectRatioMax > 100)
    r.warnings.push_back("maximum aspect ratio exceeds 100");
  if (r.volumeMin > 0 && r.volumeMax / r.volumeMin > 1.0e4)
    r.warnings.push_back("cell volume ratio exceeds 1e4");

  return r;
}

std::string MeshReport::toText() const {
  std::ostringstream os;
  os << "mesh              : " << (file.empty() ? "<memory>" : file) << "\n";
  os << "format            : " << format << "\n";
  os << "cells             : " << nCells << "\n";
  os << "points            : " << nPoints << "\n";
  os << "faces             : " << nFaces << " (internal " << nInternalFaces
     << ", boundary " << nBoundaryFaces << ")\n";
  os << "patches           : ";
  if (patches.empty()) {
    os << "(none)";
  } else {
    for (std::size_t i = 0; i < patches.size(); ++i) {
      if (i)
        os << ", ";
      os << patches[i].first << "(" << patches[i].second << ")";
    }
  }
  os << "\n";
  os << "bounding box      : [" << bboxMin.x << ", " << bboxMax.x << "] x ["
     << bboxMin.y << ", " << bboxMax.y << "] x [" << bboxMin.z << ", "
     << bboxMax.z << "]\n";
  os << std::setprecision(6);
  os << "volume            : min " << volumeMin << "  max " << volumeMax
     << "  total " << volumeTotal << "\n";
  os << "face area         : min " << areaMin << "  max " << areaMax << "\n";
  os << "non-orthogonality : max " << nonOrthoMax << " deg  average "
     << nonOrthoAvg << " deg\n";
  os << "skewness          : max " << skewnessMax << "  average " << skewnessAvg
     << "\n";
  os << "aspect ratio      : max " << aspectRatioMax << "  average "
     << aspectRatioAvg << "\n";
  os << "structured        : " << (structured ? "yes" : "no") << "\n";
  os << "dimension         : " << dimension << "D";
  if (structured)
    os << " (nx=" << nx << ", ny=" << ny << ", nz=" << nz << ")";
  os << "\n";
  os << "errors            : " << errors.size() << "\n";
  for (const auto& e : errors)
    os << "  - " << e << "\n";
  os << "warnings          : " << warnings.size() << "\n";
  for (const auto& w : warnings)
    os << "  - " << w << "\n";
  os << "status            : " << (pass() ? "PASS" : "FAIL") << "\n";
  return os.str();
}

std::string MeshReport::toJson() const {
  std::ostringstream os;
  os << "{\n";
  os << "  \"file\": \"" << jsonEscape(file) << "\",\n";
  os << "  \"format\": \"" << jsonEscape(format) << "\",\n";
  os << "  \"cells\": " << nCells << ",\n";
  os << "  \"points\": " << nPoints << ",\n";
  os << "  \"faces\": " << nFaces << ",\n";
  os << "  \"internalFaces\": " << nInternalFaces << ",\n";
  os << "  \"boundaryFaces\": " << nBoundaryFaces << ",\n";
  os << "  \"patches\": [";
  for (std::size_t i = 0; i < patches.size(); ++i) {
    if (i)
      os << ", ";
    os << "{\"name\": \"" << jsonEscape(patches[i].first)
       << "\", \"faces\": " << patches[i].second << "}";
  }
  os << "],\n";
  os << "  \"boundingBox\": {\"min\": [" << bboxMin.x << ", " << bboxMin.y
     << ", " << bboxMin.z << "], \"max\": [" << bboxMax.x << ", " << bboxMax.y
     << ", " << bboxMax.z << "]},\n";
  os << "  \"volume\": {\"min\": " << number(volumeMin)
     << ", \"max\": " << number(volumeMax)
     << ", \"total\": " << number(volumeTotal) << "},\n";
  os << "  \"faceArea\": {\"min\": " << number(areaMin)
     << ", \"max\": " << number(areaMax) << "},\n";
  os << "  \"nonOrthogonality\": {\"max\": " << number(nonOrthoMax)
     << ", \"average\": " << number(nonOrthoAvg) << "},\n";
  os << "  \"skewness\": {\"max\": " << number(skewnessMax)
     << ", \"average\": " << number(skewnessAvg) << "},\n";
  os << "  \"aspectRatio\": {\"max\": " << number(aspectRatioMax)
     << ", \"average\": " << number(aspectRatioAvg) << "},\n";
  os << "  \"structured\": " << (structured ? "true" : "false") << ",\n";
  os << "  \"dimension\": " << dimension << ",\n";
  os << "  \"index\": {\"nx\": " << nx << ", \"ny\": " << ny
     << ", \"nz\": " << nz << "},\n";
  os << "  \"errors\": [";
  for (std::size_t i = 0; i < errors.size(); ++i) {
    if (i)
      os << ", ";
    os << "\"" << jsonEscape(errors[i]) << "\"";
  }
  os << "],\n";
  os << "  \"warnings\": [";
  for (std::size_t i = 0; i < warnings.size(); ++i) {
    if (i)
      os << ", ";
    os << "\"" << jsonEscape(warnings[i]) << "\"";
  }
  os << "],\n";
  os << "  \"status\": \"" << (pass() ? "PASS" : "FAIL") << "\"\n";
  os << "}\n";
  return os.str();
}

}  // namespace saturn
