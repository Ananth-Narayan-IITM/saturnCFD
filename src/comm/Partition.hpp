#pragma once

#include <array>
#include <stdexcept>

#include "core/Types.hpp"

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {

constexpr int kProcNull = -1;

struct IndexBox {
  std::array<int, 3> lo{0, 0, 0};
  std::array<int, 3> hi{-1, -1, -1};

  bool ok() const {
    for (int d = 0; d < 3; ++d)
      if (hi[d] < lo[d])
        return false;
    return true;
  }
  int length(int d) const { return ok() ? hi[d] - lo[d] + 1 : 0; }
  BigIndex numPts() const {
    if (!ok())
      return 0;
    return static_cast<BigIndex>(length(0)) * length(1) * length(2);
  }
};

/// Cartesian decomposition of a structured (i,j,k) grid over MPI ranks.
/// Cells are never shared; partition neighbours exchange ghost layers.
class MeshPartition {
 public:
  MeshPartition(int nx, int ny, int nz, bool periodicX = false,
                bool periodicY = false, bool periodicZ = false);
  ~MeshPartition();

  MeshPartition(const MeshPartition&) = delete;
  MeshPartition& operator=(const MeshPartition&) = delete;

  int rank() const { return mRank; }
  int nranks() const { return mNranks; }
  int globalExtent(int d) const { return mGlobal[d]; }
  int localExtent(int d) const { return mLocal[d]; }
  int localOrigin(int d) const { return mOrigin[d]; }
  int dims(int d) const { return mDims[d]; }
  int coord(int d) const { return mCoords[d]; }
  bool periodic(int d) const { return mPeriodic[d]; }

  /// Rank owning the block in direction (dx,dy,dz), or kProcNull.
  int neighbor(int dx, int dy, int dz) const;

  /// Global ghost region in direction (dx,dy,dz); may lie outside the domain.
  IndexBox ghostBox(int nGrow, int dx, int dy, int dz) const;

  /// Global valid slab on this rank facing direction (dx,dy,dz).
  IndexBox validSlab(int nGrow, int dx, int dy, int dz) const;

 private:
  int mGlobal[3] = {0, 0, 0};
  int mLocal[3] = {0, 0, 0};
  int mOrigin[3] = {0, 0, 0};
  int mDims[3] = {1, 1, 1};
  int mCoords[3] = {0, 0, 0};
  bool mPeriodic[3] = {false, false, false};
  int mRank = 0;
  int mNranks = 1;
#ifdef SATURN_USE_MPI
  MPI_Comm mCart = MPI_COMM_NULL;
#endif
};

}  // namespace saturn
