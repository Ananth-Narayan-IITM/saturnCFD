#include "comm/HaloExchange.hpp"

#include <algorithm>
#include <climits>
#include <stdexcept>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {
namespace {

int dirIndex(int dx, int dy, int dz) {
  return (dx + 1) + 3 * (dy + 1) + 9 * (dz + 1);
}

}  // namespace

HaloExchange::HaloExchange(const MeshPartition& part, int nGrow, int ncomp)
    : mPart(part), mNGrow(nGrow), mNcomp(ncomp) {
  if (nGrow < 0)
    throw std::invalid_argument("HaloExchange: nGrow < 0");
  if (ncomp < 1)
    throw std::invalid_argument("HaloExchange: ncomp < 1");
  for (int d = 0; d < 3; ++d) {
    if (mPart.globalExtent(d) > 1 && mPart.localExtent(d) < nGrow)
      throw std::invalid_argument(
          "HaloExchange: local block is smaller than the ghost width");
  }
  build();
}

void HaloExchange::build() {
  for (int dz = -1; dz <= 1; ++dz) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0 && dz == 0)
          continue;
        const int dir[3] = {dx, dy, dz};
        bool touchesEmpty = false;
        for (int d = 0; d < 3; ++d)
          if (dir[d] != 0 && mPart.globalExtent(d) == 1)
            touchesEmpty = true;
        if (touchesEmpty)
          continue;

        const int sendPeer = mPart.neighbor(dx, dy, dz);
        if (sendPeer != kProcNull) {
          Op op;
          op.peer = sendPeer;
          op.dx = dx;
          op.dy = dy;
          op.dz = dz;
          op.box = mPart.validSlab(mNGrow, dx, dy, dz);
          op.tag = 1000 + dirIndex(dx, dy, dz);
          mSendOps.push_back(std::move(op));
        }

        const int recvPeer = mPart.neighbor(dx, dy, dz);
        if (recvPeer != kProcNull) {
          Op op;
          op.peer = recvPeer;
          op.dx = dx;
          op.dy = dy;
          op.dz = dz;
          op.box = mPart.ghostBox(mNGrow, dx, dy, dz);
          op.tag = 1000 + dirIndex(-dx, -dy, -dz);
          mRecvOps.push_back(std::move(op));
        }
      }
    }
  }
}

int HaloExchange::localIndex(int global, int d) const {
  return global - (mPart.localOrigin(d) - mNGrow);
}

void HaloExchange::pack(const IndexBox& box, const Real* data,
                        std::vector<Real>& out) const {
  const int nxg = mPart.localExtent(0) + 2 * mNGrow;
  const int nyg = mPart.localExtent(1) + 2 * mNGrow;
  const int nzg = mPart.localExtent(2) + 2 * mNGrow;
  const BigIndex sy = nxg;
  const BigIndex sz = static_cast<BigIndex>(nxg) * nyg;
  const BigIndex sc = sz * nzg;
  const BigIndex np = box.numPts();
  out.resize(static_cast<std::size_t>(np * mNcomp));
  BigIndex t = 0;
  for (int k = box.lo[2]; k <= box.hi[2]; ++k) {
    for (int j = box.lo[1]; j <= box.hi[1]; ++j) {
      for (int i = box.lo[0]; i <= box.hi[0]; ++i) {
        const int ii = localIndex(i, 0);
        const int jj = localIndex(j, 1);
        const int kk = localIndex(k, 2);
        for (int c = 0; c < mNcomp; ++c)
          out[static_cast<std::size_t>(t * mNcomp + c)] =
              data[c * sc + kk * sz + jj * sy + ii];
        ++t;
      }
    }
  }
}

void HaloExchange::unpack(const IndexBox& box, const std::vector<Real>& in,
                          Real* data) const {
  const int nxg = mPart.localExtent(0) + 2 * mNGrow;
  const int nyg = mPart.localExtent(1) + 2 * mNGrow;
  const int nzg = mPart.localExtent(2) + 2 * mNGrow;
  const BigIndex sy = nxg;
  const BigIndex sz = static_cast<BigIndex>(nxg) * nyg;
  const BigIndex sc = sz * nzg;
  BigIndex t = 0;
  for (int k = box.lo[2]; k <= box.hi[2]; ++k) {
    for (int j = box.lo[1]; j <= box.hi[1]; ++j) {
      for (int i = box.lo[0]; i <= box.hi[0]; ++i) {
        const int ii = localIndex(i, 0);
        const int jj = localIndex(j, 1);
        const int kk = localIndex(k, 2);
        for (int c = 0; c < mNcomp; ++c)
          data[c * sc + kk * sz + jj * sy + ii] =
              in[static_cast<std::size_t>(t * mNcomp + c)];
        ++t;
      }
    }
  }
}

void HaloExchange::copyBox(const IndexBox& src, const IndexBox& dst,
                           Real* data) const {
  const int nxg = mPart.localExtent(0) + 2 * mNGrow;
  const int nyg = mPart.localExtent(1) + 2 * mNGrow;
  const int nzg = mPart.localExtent(2) + 2 * mNGrow;
  const BigIndex sy = nxg;
  const BigIndex sz = static_cast<BigIndex>(nxg) * nyg;
  const BigIndex sc = sz * nzg;
  const int nx = src.length(0);
  const int ny = src.length(1);
  const int nz = src.length(2);
  for (int k = 0; k < nz; ++k) {
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int si = localIndex(src.lo[0] + i, 0);
        const int sj = localIndex(src.lo[1] + j, 1);
        const int sk = localIndex(src.lo[2] + k, 2);
        const int di = localIndex(dst.lo[0] + i, 0);
        const int dj = localIndex(dst.lo[1] + j, 1);
        const int dk = localIndex(dst.lo[2] + k, 2);
        for (int c = 0; c < mNcomp; ++c)
          data[c * sc + dk * sz + dj * sy + di] =
              data[c * sc + sk * sz + sj * sy + si];
      }
    }
  }
}

void HaloExchange::execute(Real* data) {
  // Same-rank periodic copies (and small process grids where a neighbour is
  // the local rank) first.
  for (const Op& r : mRecvOps) {
    if (r.peer != mPart.rank())
      continue;
    const IndexBox src = mPart.validSlab(mNGrow, -r.dx, -r.dy, -r.dz);
    copyBox(src, r.box, data);
  }

#ifdef SATURN_USE_MPI
  std::vector<MPI_Request> requests;
  requests.reserve(mSendOps.size() + mRecvOps.size());

  for (Op& r : mRecvOps) {
    if (r.peer == mPart.rank() || r.peer == kProcNull)
      continue;
    const BigIndex n = r.box.numPts() * mNcomp;
    if (n > INT_MAX)
      throw std::runtime_error("HaloExchange: message exceeds INT_MAX");
    r.buffer.resize(static_cast<std::size_t>(n));
    MPI_Request req = MPI_REQUEST_NULL;
    MPI_Irecv(r.buffer.data(), static_cast<int>(n), MPI_DOUBLE, r.peer, r.tag,
              MPI_COMM_WORLD, &req);
    requests.push_back(req);
  }

  for (Op& s : mSendOps) {
    if (s.peer == mPart.rank() || s.peer == kProcNull)
      continue;
    pack(s.box, data, s.buffer);
    const BigIndex n = s.box.numPts() * mNcomp;
    if (n > INT_MAX)
      throw std::runtime_error("HaloExchange: message exceeds INT_MAX");
    MPI_Request req = MPI_REQUEST_NULL;
    MPI_Isend(s.buffer.data(), static_cast<int>(n), MPI_DOUBLE, s.peer, s.tag,
              MPI_COMM_WORLD, &req);
    requests.push_back(req);
  }

  if (!requests.empty())
    MPI_Waitall(static_cast<int>(requests.size()), requests.data(),
                MPI_STATUSES_IGNORE);

  for (const Op& r : mRecvOps) {
    if (r.peer == mPart.rank() || r.peer == kProcNull)
      continue;
    unpack(r.box, r.buffer, data);
  }
#else
  (void)data;
#endif
}

}  // namespace saturn
