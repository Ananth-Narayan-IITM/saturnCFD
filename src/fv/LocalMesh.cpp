#include "fv/LocalMesh.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/Error.hpp"

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {

LocalMesh::LocalMesh(const Mesh& mesh, const MeshPartition& part, int nGrow)
    : LocalMesh(mesh, part.box(), nGrow) {}

LocalMesh::LocalMesh(const Mesh& mesh, const PartitionBox& box, int nGrow) {
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
    mGlobal[d] = box.global[d];
    mDevice.global[d] = box.global[d];
    mHost.global[d] = box.global[d];
    mDevice.origin[d] = box.origin[d];
    mHost.origin[d] = box.origin[d];
    mDevice.extent[d] = box.extent[d];
    mHost.extent[d] = box.extent[d];
    mDevice.periodic[d] = box.periodic[d];
    mHost.periodic[d] = box.periodic[d];
  }
  mGlobalCellCount =
      static_cast<BigIndex>(mGlobal[0]) * mGlobal[1] * mGlobal[2];
  mDevice.dimension = mesh.dimension;
  mHost.dimension = mesh.dimension;

  allocate(nGrow, mesh.dimension);
  build(mesh);

  Kokkos::deep_copy(mDevice.mVolume, mHost.mVolume);
  Kokkos::deep_copy(mDevice.mCentroid, mHost.mCentroid);
  Kokkos::deep_copy(mDevice.mFaceArea, mHost.mFaceArea);
  Kokkos::deep_copy(mDevice.mFaceNormal, mHost.mFaceNormal);
  Kokkos::deep_copy(mDevice.mFaceCentroid, mHost.mFaceCentroid);
  Kokkos::deep_copy(mDevice.mFaceDistance, mHost.mFaceDistance);
  Kokkos::deep_copy(mDevice.mFacePatch, mHost.mFacePatch);
  Kokkos::deep_copy(mDevice.mCellId, mHost.mCellId);
}

void LocalMesh::allocate(int nGrow, int dimension) {
  mDevice.nGrow = nGrow;
  mHost.nGrow = nGrow;
  mDevice.dimension = dimension;
  mHost.dimension = dimension;

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
  mDevice.mCellId = DeviceData::int3("saturn::lm_cell_id", nxg, nyg, nzg);

  mHost.mVolume = Kokkos::create_mirror_view(mDevice.mVolume);
  mHost.mCentroid = Kokkos::create_mirror_view(mDevice.mCentroid);
  mHost.mFaceArea = Kokkos::create_mirror_view(mDevice.mFaceArea);
  mHost.mFaceNormal = Kokkos::create_mirror_view(mDevice.mFaceNormal);
  mHost.mFaceCentroid = Kokkos::create_mirror_view(mDevice.mFaceCentroid);
  mHost.mFaceDistance = Kokkos::create_mirror_view(mDevice.mFaceDistance);
  mHost.mFacePatch = Kokkos::create_mirror_view(mDevice.mFacePatch);
  mHost.mCellId = Kokkos::create_mirror_view(mDevice.mCellId);
  Kokkos::deep_copy(mHost.mVolume, Real(0));
  Kokkos::deep_copy(mHost.mCentroid, Real(0));
  Kokkos::deep_copy(mHost.mFaceArea, Real(0));
  Kokkos::deep_copy(mHost.mFaceNormal, Real(0));
  Kokkos::deep_copy(mHost.mFaceCentroid, Real(0));
  Kokkos::deep_copy(mHost.mFaceDistance, Real(0));
  Kokkos::deep_copy(mHost.mFacePatch, -1);
  Kokkos::deep_copy(mHost.mCellId, -1);
}

void LocalMesh::build(const Mesh& mesh) {
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
        mHost.mCellId(ii, jj, kk) = cell;
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

LocalMesh::Serialized LocalMesh::pack() const {
  const int nxg = mHost.extent[0] + 2 * mHost.nGrow;
  const int nyg = mHost.extent[1] + 2 * mHost.nGrow;
  const int nzg = mHost.extent[2] + 2 * mHost.nGrow;
  const int cells = nxg * nyg * nzg;
  Serialized out;
  out.reals.reserve(static_cast<std::size_t>(cells) * 52);
  out.ints.reserve(static_cast<std::size_t>(cells) * 7);
  for (int k = 0; k < nzg; ++k) {
    for (int j = 0; j < nyg; ++j) {
      for (int i = 0; i < nxg; ++i) {
        out.reals.push_back(mHost.mVolume(i, j, k));
        for (int c = 0; c < 3; ++c)
          out.reals.push_back(mHost.mCentroid(i, j, k, c));
        for (int c = 0; c < 6; ++c)
          out.reals.push_back(mHost.mFaceArea(i, j, k, c));
        for (int c = 0; c < 18; ++c)
          out.reals.push_back(mHost.mFaceNormal(i, j, k, c));
        for (int c = 0; c < 18; ++c)
          out.reals.push_back(mHost.mFaceCentroid(i, j, k, c));
        for (int c = 0; c < 6; ++c)
          out.reals.push_back(mHost.mFaceDistance(i, j, k, c));
        for (int c = 0; c < 6; ++c)
          out.ints.push_back(mHost.mFacePatch(i, j, k, c));
        out.ints.push_back(mHost.mCellId(i, j, k));
      }
    }
  }
  out.patches = mPatchNames;
  return out;
}

LocalMesh LocalMesh::unpack(const PartitionBox& box, int nGrow,
                            const Serialized& data) {
  LocalMesh local;
  const int dimension = (box.global[2] > 1) ? 3 : ((box.global[1] > 1) ? 2 : 1);
  for (int d = 0; d < 3; ++d) {
    local.mGlobal[d] = box.global[d];
    local.mDevice.global[d] = box.global[d];
    local.mHost.global[d] = box.global[d];
    local.mDevice.origin[d] = box.origin[d];
    local.mHost.origin[d] = box.origin[d];
    local.mDevice.extent[d] = box.extent[d];
    local.mHost.extent[d] = box.extent[d];
    local.mDevice.periodic[d] = box.periodic[d];
    local.mHost.periodic[d] = box.periodic[d];
  }
  local.mGlobalCellCount =
      static_cast<BigIndex>(box.global[0]) * box.global[1] * box.global[2];
  local.allocate(nGrow, dimension);
  local.mPatchNames = data.patches;

  const std::size_t expectedReals =
      static_cast<std::size_t>(local.mHost.extent[0] + 2 * nGrow) *
      (local.mHost.extent[1] + 2 * nGrow) *
      (local.mHost.extent[2] + 2 * nGrow) * 52;
  const std::size_t expectedInts =
      static_cast<std::size_t>(local.mHost.extent[0] + 2 * nGrow) *
      (local.mHost.extent[1] + 2 * nGrow) *
      (local.mHost.extent[2] + 2 * nGrow) * 7;
  if (data.reals.size() != expectedReals || data.ints.size() != expectedInts)
    throw MeshError("LocalMesh::unpack: local geometry size mismatch");

  std::size_t r = 0;
  std::size_t q = 0;
  const int nxg = local.mHost.extent[0] + 2 * nGrow;
  const int nyg = local.mHost.extent[1] + 2 * nGrow;
  const int nzg = local.mHost.extent[2] + 2 * nGrow;
  for (int k = 0; k < nzg; ++k) {
    for (int j = 0; j < nyg; ++j) {
      for (int i = 0; i < nxg; ++i) {
        local.mHost.mVolume(i, j, k) = data.reals[r++];
        for (int c = 0; c < 3; ++c)
          local.mHost.mCentroid(i, j, k, c) = data.reals[r++];
        for (int c = 0; c < 6; ++c)
          local.mHost.mFaceArea(i, j, k, c) = data.reals[r++];
        for (int c = 0; c < 18; ++c)
          local.mHost.mFaceNormal(i, j, k, c) = data.reals[r++];
        for (int c = 0; c < 18; ++c)
          local.mHost.mFaceCentroid(i, j, k, c) = data.reals[r++];
        for (int c = 0; c < 6; ++c)
          local.mHost.mFaceDistance(i, j, k, c) = data.reals[r++];
        for (int c = 0; c < 6; ++c)
          local.mHost.mFacePatch(i, j, k, c) = data.ints[q++];
        local.mHost.mCellId(i, j, k) = data.ints[q++];
      }
    }
  }

  Kokkos::deep_copy(local.mDevice.mVolume, local.mHost.mVolume);
  Kokkos::deep_copy(local.mDevice.mCentroid, local.mHost.mCentroid);
  Kokkos::deep_copy(local.mDevice.mFaceArea, local.mHost.mFaceArea);
  Kokkos::deep_copy(local.mDevice.mFaceNormal, local.mHost.mFaceNormal);
  Kokkos::deep_copy(local.mDevice.mFaceCentroid, local.mHost.mFaceCentroid);
  Kokkos::deep_copy(local.mDevice.mFaceDistance, local.mHost.mFaceDistance);
  Kokkos::deep_copy(local.mDevice.mFacePatch, local.mHost.mFacePatch);
  Kokkos::deep_copy(local.mDevice.mCellId, local.mHost.mCellId);
  return local;
}

LocalMesh LocalMesh::distributed(const Mesh& mesh, const MeshPartition& part,
                                 int nGrow) {
#ifndef SATURN_USE_MPI
  return LocalMesh(mesh, part, nGrow);
#else
  int rank = 0;
  int size = 1;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (size == 1)
    return LocalMesh(mesh, part.box(), nGrow);

  if (rank == 0) {
    LocalMesh local(mesh, part.box(), nGrow);
    for (int target = 1; target < size; ++target) {
      PartitionBox box = part.boxForRank(target);
      LocalMesh temp(mesh, box, nGrow);
      Serialized data = temp.pack();

      std::string blob;
      for (const std::string& name : data.patches) {
        blob += name;
        blob.push_back('\0');
      }
      int sizes[3] = {static_cast<int>(data.reals.size()),
                      static_cast<int>(data.ints.size()),
                      static_cast<int>(blob.size())};
      MPI_Send(sizes, 3, MPI_INT, target, 700, MPI_COMM_WORLD);
      if (!data.reals.empty())
        MPI_Send(data.reals.data(), sizes[0], MPI_DOUBLE, target, 701,
                 MPI_COMM_WORLD);
      if (!data.ints.empty())
        MPI_Send(data.ints.data(), sizes[1], MPI_INT, target, 702,
                 MPI_COMM_WORLD);
      if (!blob.empty())
        MPI_Send(blob.data(), sizes[2], MPI_CHAR, target, 703, MPI_COMM_WORLD);
    }
    return local;
  }

  int sizes[3] = {0, 0, 0};
  MPI_Recv(sizes, 3, MPI_INT, 0, 700, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  Serialized data;
  data.reals.resize(static_cast<std::size_t>(sizes[0]));
  data.ints.resize(static_cast<std::size_t>(sizes[1]));
  std::string blob(static_cast<std::size_t>(sizes[2]), '\0');
  if (sizes[0] > 0)
    MPI_Recv(data.reals.data(), sizes[0], MPI_DOUBLE, 0, 701, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
  if (sizes[1] > 0)
    MPI_Recv(data.ints.data(), sizes[1], MPI_INT, 0, 702, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);
  if (sizes[2] > 0)
    MPI_Recv(blob.data(), sizes[2], MPI_CHAR, 0, 703, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

  std::size_t start = 0;
  while (start < blob.size()) {
    const std::size_t end = blob.find('\0', start);
    if (end == std::string::npos)
      break;
    data.patches.emplace_back(blob.substr(start, end - start));
    start = end + 1;
  }
  return unpack(part.box(), nGrow, data);
#endif
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
