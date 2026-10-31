#include "fv/LocalMesh.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Error.hpp"

namespace saturn {

LocalMesh::LocalMesh(const Mesh& mesh, const MeshPartition& part, int nGrow) {
  if (!mesh.structured)
    throw MeshError("LocalMesh: the global mesh is not structured");
  if (nGrow < 0)
    throw MeshError("LocalMesh: nGrow < 0");
  if (mesh.cellVolume.size() != mesh.cells.size() ||
      mesh.cellCentroid.size() != mesh.cells.size())
    throw MeshError(
        "LocalMesh: computeGeometry() must be called before extraction");
  if (mesh.faces.empty())
    throw MeshError("LocalMesh: the global mesh has no faces");

  for (int d = 0; d < 3; ++d) {
    const int global = (d == 0) ? mesh.nx : ((d == 1) ? mesh.ny : mesh.nz);
    if (part.globalExtent(d) != global)
      throw MeshError(
          "LocalMesh: MeshPartition global extent does not match the mesh");
    mGlobal[d] = global;
    mDevice.global[d] = global;
    mHost.global[d] = global;
    mDevice.origin[d] = part.localOrigin(d);
    mHost.origin[d] = part.localOrigin(d);
    mDevice.extent[d] = part.localExtent(d);
    mHost.extent[d] = part.localExtent(d);
    mDevice.periodic[d] = part.periodic(d);
    mHost.periodic[d] = part.periodic(d);
  }
  mDevice.nGrow = nGrow;
  mHost.nGrow = nGrow;
  mDevice.dimension = mesh.dimension;
  mHost.dimension = mesh.dimension;

  const int nxg = mDevice.extent[0] + 2 * nGrow;
  const int nyg = mDevice.extent[1] + 2 * nGrow;
  const int nzg = mDevice.extent[2] + 2 * nGrow;

  mDevice.mVolume = DeviceData::real3("saturn::lm_volume", nxg, nyg, nzg);
  mDevice.mCentroid =
      DeviceData::real4("saturn::lm_centroid", nxg, nyg, nzg, 3);
  mDevice.mFaceArea =
      DeviceData::real4("saturn::lm_face_area", nxg, nyg, nzg, 6);
  mDevice.mFaceNormal =
      DeviceData::real4("saturn::lm_face_normal", nxg, nyg, nzg, 18);
  mDevice.mFaceCentroid =
      DeviceData::real4("saturn::lm_face_centroid", nxg, nyg, nzg, 18);
  mDevice.mFaceDistance =
      DeviceData::real4("saturn::lm_face_distance", nxg, nyg, nzg, 6);
  mDevice.mFacePatch =
      DeviceData::int4("saturn::lm_face_patch", nxg, nyg, nzg, 6);

  mHost.mVolume = Kokkos::create_mirror_view(mDevice.mVolume);
  mHost.mCentroid = Kokkos::create_mirror_view(mDevice.mCentroid);
  mHost.mFaceArea = Kokkos::create_mirror_view(mDevice.mFaceArea);
  mHost.mFaceNormal = Kokkos::create_mirror_view(mDevice.mFaceNormal);
  mHost.mFaceCentroid = Kokkos::create_mirror_view(mDevice.mFaceCentroid);
  mHost.mFaceDistance = Kokkos::create_mirror_view(mDevice.mFaceDistance);
  mHost.mFacePatch = Kokkos::create_mirror_view(mDevice.mFacePatch);

  build(mesh, part);

  Kokkos::deep_copy(mDevice.mVolume, mHost.mVolume);
  Kokkos::deep_copy(mDevice.mCentroid, mHost.mCentroid);
  Kokkos::deep_copy(mDevice.mFaceArea, mHost.mFaceArea);
  Kokkos::deep_copy(mDevice.mFaceNormal, mHost.mFaceNormal);
  Kokkos::deep_copy(mDevice.mFaceCentroid, mHost.mFaceCentroid);
  Kokkos::deep_copy(mDevice.mFaceDistance, mHost.mFaceDistance);
  Kokkos::deep_copy(mDevice.mFacePatch, mHost.mFacePatch);
}

void LocalMesh::build(const Mesh& mesh, const MeshPartition& part) {
  (void)part;
  const int nCells = static_cast<int>(mesh.cells.size());
  std::vector<std::vector<int>> cellFaces(nCells);
  for (int f = 0; f < static_cast<int>(mesh.faces.size()); ++f) {
    const Face& face = mesh.faces[f];
    if (face.owner >= 0)
      cellFaces[face.owner].push_back(f);
    if (face.neighbour >= 0)
      cellFaces[face.neighbour].push_back(f);
  }

  mPatchNames.clear();
  for (const Patch& patch : mesh.patches)
    mPatchNames.push_back(patch.name);

  for (int k = mHost.origin[2]; k < mHost.origin[2] + mHost.extent[2]; ++k) {
    for (int j = mHost.origin[1]; j < mHost.origin[1] + mHost.extent[1]; ++j) {
      for (int i = mHost.origin[0]; i < mHost.origin[0] + mHost.extent[0];
           ++i) {
        const int cell = mesh.cellAt(i, j, k);
        if (cell < 0)
          throw MeshError("LocalMesh: owned cell missing from the mesh");
        const int ii = mHost.localIndex(i, 0);
        const int jj = mHost.localIndex(j, 1);
        const int kk = mHost.localIndex(k, 2);

        mHost.mVolume(ii, jj, kk) = mesh.cellVolume[cell];
        const Vec3& centre = mesh.cellCentroid[cell];
        mHost.mCentroid(ii, jj, kk, 0) = centre.x;
        mHost.mCentroid(ii, jj, kk, 1) = centre.y;
        mHost.mCentroid(ii, jj, kk, 2) = centre.z;

        std::array<bool, 6> seen{};
        for (int fi : cellFaces[cell]) {
          const Face& face = mesh.faces[fi];
          const bool isOwner = face.owner == cell;
          if (!isOwner && face.neighbour != cell)
            throw MeshError("LocalMesh: face is not adjacent to its cell");
          if (face.neighbour < 0 && face.patch < 0)
            throw MeshError("LocalMesh: boundary face has no patch");

          const Vec3 outward = isOwner ? face.normal : -face.normal;
          int axis = 0;
          Real best = std::abs(outward[0]);
          for (int a = 1; a < 3; ++a) {
            const Real candidate = std::abs(outward[a]);
            if (candidate > best) {
              best = candidate;
              axis = a;
            }
          }
          if (best <= 0)
            throw MeshError("LocalMesh: degenerate face normal");
          const int dir = 2 * axis + (outward[axis] > 0 ? 1 : 0);
          if (seen[dir])
            throw MeshError("LocalMesh: duplicate face direction");
          seen[dir] = true;

          mHost.mFaceArea(ii, jj, kk, dir) = face.area;
          mHost.mFaceNormal(ii, jj, kk, dir * 3 + 0) = outward.x;
          mHost.mFaceNormal(ii, jj, kk, dir * 3 + 1) = outward.y;
          mHost.mFaceNormal(ii, jj, kk, dir * 3 + 2) = outward.z;
          mHost.mFaceCentroid(ii, jj, kk, dir * 3 + 0) = face.centroid.x;
          mHost.mFaceCentroid(ii, jj, kk, dir * 3 + 1) = face.centroid.y;
          mHost.mFaceCentroid(ii, jj, kk, dir * 3 + 2) = face.centroid.z;

          const Real distance = dot(face.centroid - centre, outward);
          if (!(distance > 0))
            throw MeshError("LocalMesh: non-positive face distance");
          mHost.mFaceDistance(ii, jj, kk, dir) = distance;
          mHost.mFacePatch(ii, jj, kk, dir) =
              (face.neighbour >= 0) ? -1 : face.patch;
        }
        for (int dir = 0; dir < 6; ++dir)
          if (!seen[dir])
            throw MeshError("LocalMesh: cell does not have six distinct faces");
      }
    }
  }

  // Ghost-cell centroids are needed by the non-orthogonal Laplacian on faces
  // that touch a partition or periodic ghost. A physical-boundary ghost is
  // handled by the mirror formula and does not need a centroid.
  Vec3 period[3]{};
  for (int d = 0; d < 3; ++d) {
    if (!mHost.periodic[d] || mGlobal[d] <= 1)
      continue;
    int i1 = 0, j1 = 0, k1 = 0;
    if (d == 0)
      i1 = 1;
    else if (d == 1)
      j1 = 1;
    else
      k1 = 1;
    const int c0 = mesh.cellAt(0, 0, 0);
    const int c1 = mesh.cellAt(i1, j1, k1);
    if (c0 < 0 || c1 < 0)
      throw MeshError("LocalMesh: cannot determine the periodic lattice");
    period[d] =
        (mesh.cellCentroid[c1] - mesh.cellCentroid[c0]) * Real(mGlobal[d]);
  }

  const int ng = mHost.nGrow;
  for (int k = mHost.origin[2] - ng; k < mHost.origin[2] + mHost.extent[2] + ng;
       ++k) {
    for (int j = mHost.origin[1] - ng;
         j < mHost.origin[1] + mHost.extent[1] + ng; ++j) {
      for (int i = mHost.origin[0] - ng;
           i < mHost.origin[0] + mHost.extent[0] + ng; ++i) {
        if (mHost.owned(i, j, k))
          continue;
        int wrapped[3] = {i, j, k};
        Vec3 shift{};
        bool valid = true;
        for (int d = 0; d < 3; ++d) {
          const int index = (d == 0) ? i : ((d == 1) ? j : k);
          const int n = mGlobal[d];
          if (index >= 0 && index < n)
            continue;
          if (!mHost.periodic[d]) {
            valid = false;
            break;
          }
          int q = index / n;
          if (index < 0 && index % n != 0)
            --q;
          wrapped[d] = index - q * n;
          shift += period[d] * Real(q);
        }
        if (!valid)
          continue;
        const int cell = mesh.cellAt(wrapped[0], wrapped[1], wrapped[2]);
        if (cell < 0)
          throw MeshError("LocalMesh: wrapped ghost cell is not in the mesh");
        const Vec3 x = mesh.cellCentroid[cell] + shift;
        const int ii = mHost.localIndex(i, 0);
        const int jj = mHost.localIndex(j, 1);
        const int kk = mHost.localIndex(k, 2);
        mHost.mCentroid(ii, jj, kk, 0) = x.x;
        mHost.mCentroid(ii, jj, kk, 1) = x.y;
        mHost.mCentroid(ii, jj, kk, 2) = x.z;
      }
    }
  }
}

int LocalMesh::patchIndex(const std::string& name) const {
  for (int i = 0; i < static_cast<int>(mPatchNames.size()); ++i)
    if (mPatchNames[i] == name)
      return i;
  throw MeshError("LocalMesh: unknown patch '" + name + "'");
}

const std::string& LocalMesh::patchName(int index) const {
  if (index < 0 || index >= static_cast<int>(mPatchNames.size()))
    throw MeshError("LocalMesh: patch index out of range");
  return mPatchNames[index];
}

}  // namespace saturn
