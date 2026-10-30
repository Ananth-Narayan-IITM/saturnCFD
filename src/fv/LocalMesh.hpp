#pragma once

#include <array>
#include <string>
#include <vector>

#include <Kokkos_Core.hpp>

#include "comm/Partition.hpp"
#include "core/Types.hpp"
#include "mesh/Mesh.hpp"

namespace saturn {

/// Direction order used for the six cell faces:
/// 0 = -x, 1 = +x, 2 = -y, 3 = +y, 4 = -z, 5 = +z.
KOKKOS_INLINE_FUNCTION
void faceDirectionOffset(int dir, int& di, int& dj, int& dk) {
  di = (dir == 0) ? -1 : ((dir == 1) ? 1 : 0);
  dj = (dir == 2) ? -1 : ((dir == 3) ? 1 : 0);
  dk = (dir == 4) ? -1 : ((dir == 5) ? 1 : 0);
}

/// Device-copyable geometry for one local structured block.
///
/// Views are indexed by global (i,j,k) through the local ghost origin, exactly
/// like CellField. Geometry is filled for owned cells; ghost cells are left
/// untouched and must not be read.
template <class MemorySpace>
struct LocalMeshData {
  using real3 = Kokkos::View<Real***, Kokkos::LayoutLeft, MemorySpace>;
  using real4 = Kokkos::View<Real****, Kokkos::LayoutLeft, MemorySpace>;
  using int4 = Kokkos::View<int****, Kokkos::LayoutLeft, MemorySpace>;

  real3 mVolume;        ///< cell volume
  real4 mCentroid;      ///< (i,j,k,3) cell centroid
  real4 mFaceArea;      ///< (i,j,k,6) outward face area
  real4 mFaceNormal;    ///< (i,j,k,dir*3+axis) outward unit normal
  real4 mFaceCentroid;  ///< (i,j,k,dir*3+axis) face centroid
  real4 mFaceDistance;  ///< (i,j,k,6) centroid-to-face normal distance
  int4 mFacePatch;      ///< (i,j,k,6) patch index, or -1 for an internal face

  int origin[3] = {0, 0, 0};
  int extent[3] = {0, 0, 0};
  int nGrow = 0;
  int dimension = 3;
  bool periodic[3] = {false, false, false};

  KOKKOS_INLINE_FUNCTION
  int localIndex(int global, int d) const {
    return global - (origin[d] - nGrow);
  }

  KOKKOS_INLINE_FUNCTION
  bool owned(int i, int j, int k) const {
    return i >= origin[0] && i < origin[0] + extent[0] && j >= origin[1] &&
           j < origin[1] + extent[1] && k >= origin[2] &&
           k < origin[2] + extent[2];
  }

  KOKKOS_INLINE_FUNCTION
  Real cellVolume(int i, int j, int k) const {
    return mVolume(localIndex(i, 0), localIndex(j, 1), localIndex(k, 2));
  }

  KOKKOS_INLINE_FUNCTION
  Vec3 cellCentroid(int i, int j, int k) const {
    const int ii = localIndex(i, 0);
    const int jj = localIndex(j, 1);
    const int kk = localIndex(k, 2);
    return {mCentroid(ii, jj, kk, 0), mCentroid(ii, jj, kk, 1),
            mCentroid(ii, jj, kk, 2)};
  }

  KOKKOS_INLINE_FUNCTION
  Real faceArea(int i, int j, int k, int dir) const {
    return mFaceArea(localIndex(i, 0), localIndex(j, 1), localIndex(k, 2), dir);
  }

  KOKKOS_INLINE_FUNCTION
  Vec3 faceNormal(int i, int j, int k, int dir) const {
    const int ii = localIndex(i, 0);
    const int jj = localIndex(j, 1);
    const int kk = localIndex(k, 2);
    const int c = dir * 3;
    return {mFaceNormal(ii, jj, kk, c), mFaceNormal(ii, jj, kk, c + 1),
            mFaceNormal(ii, jj, kk, c + 2)};
  }

  KOKKOS_INLINE_FUNCTION
  Vec3 faceCentroid(int i, int j, int k, int dir) const {
    const int ii = localIndex(i, 0);
    const int jj = localIndex(j, 1);
    const int kk = localIndex(k, 2);
    const int c = dir * 3;
    return {mFaceCentroid(ii, jj, kk, c), mFaceCentroid(ii, jj, kk, c + 1),
            mFaceCentroid(ii, jj, kk, c + 2)};
  }

  KOKKOS_INLINE_FUNCTION
  Real faceDistance(int i, int j, int k, int dir) const {
    return mFaceDistance(localIndex(i, 0), localIndex(j, 1), localIndex(k, 2),
                         dir);
  }

  KOKKOS_INLINE_FUNCTION
  int boundaryPatch(int i, int j, int k, int dir) const {
    return mFacePatch(localIndex(i, 0), localIndex(j, 1), localIndex(k, 2),
                      dir);
  }
};

/// Structured local geometry extracted from a global hex mesh.
///
/// The class owns device views plus host mirrors. On a host Kokkos build the
/// mirrors alias the device views; on a device build they are separate copies.
/// Operators capture `device()` by value; host-side boundary fill uses the
/// host accessors.
class LocalMesh {
 public:
  using memory_space = typename Kokkos::DefaultExecutionSpace::memory_space;
  using DeviceData = LocalMeshData<memory_space>;
  using HostData = LocalMeshData<Kokkos::HostSpace>;

  LocalMesh() = default;
  LocalMesh(const Mesh& mesh, const MeshPartition& part, int nGrow = 2);

  const DeviceData& device() const { return mDevice; }
  const HostData& host() const { return mHost; }

  int nGrow() const { return mDevice.nGrow; }
  int origin(int d) const { return mDevice.origin[d]; }
  int extent(int d) const { return mDevice.extent[d]; }
  int globalExtent(int d) const { return mGlobal[d]; }
  int ghostExtent(int d) const { return mDevice.extent[d] + 2 * nGrow(); }
  int dimension() const { return mDevice.dimension; }
  bool periodic(int d) const { return mDevice.periodic[d]; }

  int numPatches() const { return static_cast<int>(mPatchNames.size()); }
  int patchIndex(const std::string& name) const;
  const std::string& patchName(int index) const;

  Real cellVolume(int i, int j, int k) const {
    return mHost.cellVolume(i, j, k);
  }
  Vec3 cellCentroid(int i, int j, int k) const {
    return mHost.cellCentroid(i, j, k);
  }
  Real faceArea(int i, int j, int k, int dir) const {
    return mHost.faceArea(i, j, k, dir);
  }
  Vec3 faceNormal(int i, int j, int k, int dir) const {
    return mHost.faceNormal(i, j, k, dir);
  }
  Vec3 faceCentroid(int i, int j, int k, int dir) const {
    return mHost.faceCentroid(i, j, k, dir);
  }
  Real faceDistance(int i, int j, int k, int dir) const {
    return mHost.faceDistance(i, j, k, dir);
  }
  int facePatch(int i, int j, int k, int dir) const {
    return mHost.boundaryPatch(i, j, k, dir);
  }

 private:
  DeviceData mDevice;
  HostData mHost;
  std::vector<std::string> mPatchNames;
  int mGlobal[3] = {0, 0, 0};

  void build(const Mesh& mesh, const MeshPartition& part);
};

}  // namespace saturn
