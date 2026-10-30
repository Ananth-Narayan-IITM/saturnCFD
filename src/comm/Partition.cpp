#include "comm/Partition.hpp"

#include <algorithm>
#include <stdexcept>

namespace saturn {

MeshPartition::MeshPartition(int nx, int ny, int nz, bool periodicX,
                             bool periodicY, bool periodicZ) {
  mGlobal[0] = nx;
  mGlobal[1] = ny;
  mGlobal[2] = nz;
  mPeriodic[0] = periodicX;
  mPeriodic[1] = periodicY;
  mPeriodic[2] = periodicZ;

  if (nx < 1 || ny < 1 || nz < 1)
    throw std::invalid_argument("MeshPartition: empty global grid");

#ifdef SATURN_USE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &mRank);
  MPI_Comm_size(MPI_COMM_WORLD, &mNranks);

  int dims[3] = {0, 0, 0};
  // Keep degenerate directions un-decomposed: 2D has pz=1, 1D has py=pz=1.
  if (nz == 1)
    dims[2] = 1;
  if (ny == 1) {
    dims[1] = 1;
    dims[2] = 1;
  }
  MPI_Dims_create(mNranks, 3, dims);
  for (int d = 0; d < 3; ++d)
    mDims[d] = dims[d];

  int periods[3] = {periodicX ? 1 : 0, periodicY ? 1 : 0, periodicZ ? 1 : 0};
  MPI_Cart_create(MPI_COMM_WORLD, 3, dims, periods, 0, &mCart);
  MPI_Cart_coords(mCart, mRank, 3, mCoords);
#else
  mRank = 0;
  mNranks = 1;
  for (int d = 0; d < 3; ++d) {
    mDims[d] = 1;
    mCoords[d] = 0;
  }
#endif

  for (int d = 0; d < 3; ++d) {
    const int base = mGlobal[d] / mDims[d];
    const int rem = mGlobal[d] % mDims[d];
    mLocal[d] = base + (mCoords[d] < rem ? 1 : 0);
    mOrigin[d] = mCoords[d] * base + std::min(mCoords[d], rem);
    if (mLocal[d] < 1)
      throw std::invalid_argument(
          "MeshPartition: more ranks than cells in a direction");
  }
}

MeshPartition::~MeshPartition() {
#ifdef SATURN_USE_MPI
  if (mCart != MPI_COMM_NULL)
    MPI_Comm_free(&mCart);
#endif
}

int MeshPartition::neighbor(int dx, int dy, int dz) const {
  const int dir[3] = {dx, dy, dz};
  int target[3] = {0, 0, 0};
  for (int d = 0; d < 3; ++d) {
    int c = mCoords[d] + dir[d];
    if (c < 0 || c >= mDims[d]) {
      if (!mPeriodic[d])
        return kProcNull;
      c = (c % mDims[d] + mDims[d]) % mDims[d];
    }
    target[d] = c;
  }
  if (dx == 0 && dy == 0 && dz == 0)
    return mRank;
#ifdef SATURN_USE_MPI
  int r = kProcNull;
  MPI_Cart_rank(mCart, target, &r);
  return r;
#else
  (void)target;
  return 0;
#endif
}

IndexBox MeshPartition::ghostBox(int nGrow, int dx, int dy, int dz) const {
  const int dir[3] = {dx, dy, dz};
  IndexBox b;
  for (int d = 0; d < 3; ++d) {
    if (dir[d] > 0) {
      b.lo[d] = mOrigin[d] + mLocal[d];
      b.hi[d] = mOrigin[d] + mLocal[d] + nGrow - 1;
    } else if (dir[d] < 0) {
      b.lo[d] = mOrigin[d] - nGrow;
      b.hi[d] = mOrigin[d] - 1;
    } else {
      b.lo[d] = mOrigin[d];
      b.hi[d] = mOrigin[d] + mLocal[d] - 1;
    }
  }
  return b;
}

IndexBox MeshPartition::validSlab(int nGrow, int dx, int dy, int dz) const {
  const int dir[3] = {dx, dy, dz};
  IndexBox b;
  for (int d = 0; d < 3; ++d) {
    if (dir[d] > 0) {
      b.lo[d] = mOrigin[d] + mLocal[d] - nGrow;
      b.hi[d] = mOrigin[d] + mLocal[d] - 1;
    } else if (dir[d] < 0) {
      b.lo[d] = mOrigin[d];
      b.hi[d] = mOrigin[d] + nGrow - 1;
    } else {
      b.lo[d] = mOrigin[d];
      b.hi[d] = mOrigin[d] + mLocal[d] - 1;
    }
  }
  return b;
}

}  // namespace saturn
