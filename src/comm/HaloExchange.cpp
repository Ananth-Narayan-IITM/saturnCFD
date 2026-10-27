#include "comm/HaloExchange.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {
namespace {

/// Device-side copy instruction. Fabs are resolved to raw accessors on the host
/// before the kernel is launched, so the kernel does no lookup at all.
struct DevOp {
  Array4<Real> srcFab;
  Array4<Real> dstFab;
  Box src;
  Box dst;
  int srcComp = 0;
  int dstComp = 0;
  int ncomp = 1;
  BigIndex off = 0;
};

KOKKOS_INLINE_FUNCTION
void decode(BigIndex idx, int nx, int ny, int& i, int& j, int& k) {
  i = static_cast<int>(idx % nx);
  const BigIndex r = idx / nx;
  j = static_cast<int>(r % ny);
  k = (NDIM == 3) ? static_cast<int>(r / ny) : 0;
}

}  // namespace

HaloExchange::HaloExchange(const BoxArray& ba, const DistributionMapping& dm,
                           const Geometry& geom, int nghost, int myrank,
                           int nranks)
    : mBoxArray(ba),
      mDistributionMap(dm),
      mGeometry(geom),
      mNumGhost(nghost),
      mMyRank(myrank),
      mNumRanks(nranks) {}

void HaloExchange::build() {
  mLocalOps.clear();
  mRemoteOps.clear();

  if (mNumGhost == 0)
    return;

  // Periodic image shifts: 3^NDIM combinations, only for periodic dimensions.
  std::vector<Idx> shifts;
  shifts.push_back(Idx{});
  for (int d = 0; d < NDIM; ++d) {
    if (!mGeometry.periodic[d])
      continue;
    const int L = mGeometry.nCells(d);
    const int n = static_cast<int>(shifts.size());
    for (int s = 0; s < n; ++s) {
      Idx up = shifts[s];
      up[d] += L;
      shifts.push_back(up);
      Idx dn = shifts[s];
      dn[d] -= L;
      shifts.push_back(dn);
    }
  }

  const int nbox = mBoxArray.size();
  for (int b = 0; b < nbox; ++b) {
    if (mDistributionMap[b] != mMyRank)
      continue;  // only fill ghosts of boxes I own

    const Box gb = mBoxArray[b].grow(mNumGhost);

    for (const Idx& s : shifts) {
      const bool isIdentity = (s[0] == 0) && (s[1] == 0) && (s[2] == 0);
      const Box gs = gb.shift(s);

      for (int c = 0; c < nbox; ++c) {
        if (c == b && isIdentity)
          continue;

        const Box inter = gs.intersect(mBoxArray[c]);
        if (!inter.ok())
          continue;

        CopyOp op;
        op.src = inter;
        op.dst = inter.shift(Idx{-s[0], -s[1], -s[2]});
        op.srcBox = c;
        op.dstBox = b;
        op.srcRank = mDistributionMap[c];
        op.dstRank = mDistributionMap[b];
        op.ncomp = 1;
        if (op.srcRank == op.dstRank)
          mLocalOps.push_back(op);
        else
          mRemoteOps.push_back(op);
      }
    }
  }

  mNumLocalOps = static_cast<int>(mLocalOps.size());
  mNumRemoteOps = static_cast<int>(mRemoteOps.size());
}

void HaloExchange::execute(MultiFab& mf) {
  const int ncomp = mf.numComp();
  mBytesSent = 0.0;

  // ------------------------------------------------------------------ local
  if (!mLocalOps.empty()) {
    const int nops = static_cast<int>(mLocalOps.size());
    View<DevOp> dOps("halo::localOps", nops);
    auto hOps = Kokkos::create_mirror_view(dOps);
    for (int i = 0; i < nops; ++i) {
      const CopyOp& op = mLocalOps[i];
      DevOp d;
      d.srcFab = mf.arrayByGlobal(op.srcBox);
      d.dstFab = mf.arrayByGlobal(op.dstBox);
      d.src = op.src;
      d.dst = op.dst;
      d.ncomp = ncomp;
      hOps(i) = d;
    }
    Kokkos::deep_copy(dOps, hOps);

    Kokkos::parallel_for(
        "saturn::haloLocal", Kokkos::TeamPolicy<>(nops, Kokkos::AUTO),
        KOKKOS_LAMBDA(const typename Kokkos::TeamPolicy<>::member_type& team) {
          const DevOp op = dOps(team.league_rank());
          const int nx = op.src.length(0);
          const int ny = op.src.length(1);
          const BigIndex np = op.src.numPts();
          Kokkos::parallel_for(
              Kokkos::TeamThreadRange(team, np), [&](BigIndex idx) {
                int i, j, k;
                decode(idx, nx, ny, i, j, k);
                for (int c = 0; c < op.ncomp; ++c)
                  op.dstFab(op.dst.lo[0] + i, op.dst.lo[1] + j,
                            op.dst.lo[2] + k, c) =
                      op.srcFab(op.src.lo[0] + i, op.src.lo[1] + j,
                                op.src.lo[2] + k, c);
              });
        });
  }

#ifndef SATURN_USE_MPI
  mNumRemoteOps = 0;
  return;
#else
  if (mRemoteOps.empty() || mNumRanks == 1)
    return;

  // ------------------------------------------------------- group by peer ---
  std::map<int, std::vector<int>> sendMap, recvMap;
  for (int i = 0; i < static_cast<int>(mRemoteOps.size()); ++i) {
    const CopyOp& op = mRemoteOps[i];
    if (op.srcRank == mMyRank)
      sendMap[op.dstRank].push_back(i);
    if (op.dstRank == mMyRank)
      recvMap[op.srcRank].push_back(i);
  }

  auto bufferSize = [&](const std::vector<int>& ids) {
    BigIndex n = 0;
    for (int i : ids)
      n += mRemoteOps[i].src.numPts() * ncomp;
    return n;
  };

  struct Peer {
    int rank;
    std::vector<int> ops;
    BigIndex size;
    View<Real> dev;
    HostView<Real> host;
  };

  std::vector<Peer> sends, recvs;
  for (auto& kv : sendMap) {
    Peer p;
    p.rank = kv.first;
    p.ops = kv.second;
    p.size = bufferSize(p.ops);
    p.dev = View<Real>("halo::send", p.size);
    p.host = Kokkos::create_mirror_view(p.dev);
    sends.push_back(std::move(p));
  }
  for (auto& kv : recvMap) {
    Peer p;
    p.rank = kv.first;
    p.ops = kv.second;
    p.size = bufferSize(p.ops);
    p.dev = View<Real>("halo::recv", p.size);
    p.host = Kokkos::create_mirror_view(p.dev);
    recvs.push_back(std::move(p));
  }

  // -------------------------------------------------------------- packing --
  for (auto& p : sends) {
    const int nops = static_cast<int>(p.ops.size());
    View<DevOp> dOps("halo::packOps", nops);
    auto hOps = Kokkos::create_mirror_view(dOps);
    BigIndex off = 0;
    for (int t = 0; t < nops; ++t) {
      const CopyOp& op = mRemoteOps[p.ops[t]];
      DevOp d;
      d.srcFab = mf.arrayByGlobal(op.srcBox);
      d.src = op.src;
      d.ncomp = ncomp;
      d.off = off;
      hOps(t) = d;
      off += op.src.numPts() * ncomp;
    }
    Kokkos::deep_copy(dOps, hOps);

    Real* buf = p.dev.data();
    Kokkos::parallel_for(
        "saturn::haloPack", Kokkos::TeamPolicy<>(nops, Kokkos::AUTO),
        KOKKOS_LAMBDA(const typename Kokkos::TeamPolicy<>::member_type& team) {
          const DevOp op = dOps(team.league_rank());
          const int nx = op.src.length(0);
          const int ny = op.src.length(1);
          const BigIndex np = op.src.numPts();
          Kokkos::parallel_for(
              Kokkos::TeamThreadRange(team, np), [&](BigIndex idx) {
                int i, j, k;
                decode(idx, nx, ny, i, j, k);
                for (int c = 0; c < op.ncomp; ++c)
                  buf[op.off + static_cast<BigIndex>(c) * np + idx] = op.srcFab(
                      op.src.lo[0] + i, op.src.lo[1] + j, op.src.lo[2] + k, c);
              });
        });
  }

  // -------------------------------------------------------- communication --
  const int tag = 401;
  std::vector<MPI_Request> reqs(sends.size() + recvs.size(), MPI_REQUEST_NULL);

  int r = 0;
  for (auto& p : sends) {
    Kokkos::deep_copy(p.host, p.dev);
    const int count =
        static_cast<int>(p.size * static_cast<BigIndex>(sizeof(Real)));
    MPI_Isend(p.host.data(), count, MPI_BYTE, p.rank, tag, MPI_COMM_WORLD,
              &reqs[r++]);
    mBytesSent += static_cast<double>(count);
  }
  for (auto& p : recvs) {
    const int count =
        static_cast<int>(p.size * static_cast<BigIndex>(sizeof(Real)));
    MPI_Irecv(p.host.data(), count, MPI_BYTE, p.rank, tag, MPI_COMM_WORLD,
              &reqs[r++]);
  }
  MPI_Waitall(static_cast<int>(reqs.size()), reqs.data(), MPI_STATUSES_IGNORE);

  // ------------------------------------------------------------ unpacking --
  for (auto& p : recvs) {
    Kokkos::deep_copy(p.dev, p.host);

    const int nops = static_cast<int>(p.ops.size());
    View<DevOp> dOps("halo::unpackOps", nops);
    auto hOps = Kokkos::create_mirror_view(dOps);
    BigIndex off = 0;
    for (int t = 0; t < nops; ++t) {
      const CopyOp& op = mRemoteOps[p.ops[t]];
      DevOp d;
      d.dstFab = mf.arrayByGlobal(op.dstBox);
      d.dst = op.dst;
      d.ncomp = ncomp;
      d.off = off;
      hOps(t) = d;
      off += op.src.numPts() * ncomp;
    }
    Kokkos::deep_copy(dOps, hOps);

    const Real* buf = p.dev.data();
    Kokkos::parallel_for(
        "saturn::haloUnpack", Kokkos::TeamPolicy<>(nops, Kokkos::AUTO),
        KOKKOS_LAMBDA(const typename Kokkos::TeamPolicy<>::member_type& team) {
          const DevOp op = dOps(team.league_rank());
          const int nx = op.src.length(0);
          const int ny = op.src.length(1);
          const BigIndex np = op.src.numPts();
          Kokkos::parallel_for(
              Kokkos::TeamThreadRange(team, np), [&](BigIndex idx) {
                int i, j, k;
                decode(idx, nx, ny, i, j, k);
                for (int c = 0; c < op.ncomp; ++c)
                  op.dstFab(op.dst.lo[0] + i, op.dst.lo[1] + j,
                            op.dst.lo[2] + k, c) =
                      buf[op.off + static_cast<BigIndex>(c) * np + idx];
              });
        });
  }
#endif
}

}  // namespace saturn
