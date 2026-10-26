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
    : m_ba(ba),
      m_dm(dm),
      m_geom(geom),
      m_nghost(nghost),
      m_myrank(myrank),
      m_nranks(nranks) {}

void HaloExchange::build() {
  m_localOps.clear();
  m_remoteOps.clear();

  if (m_nghost == 0) return;

  // Periodic image shifts: 3^NDIM combinations, only for periodic dimensions.
  std::vector<Idx> shifts;
  shifts.push_back(Idx{});
  for (int d = 0; d < NDIM; ++d) {
    if (!m_geom.periodic[d]) continue;
    const int L = m_geom.nCells(d);
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

  const int nbox = m_ba.size();
  for (int b = 0; b < nbox; ++b) {
    if (m_dm[b] != m_myrank) continue;  // only fill ghosts of boxes I own

    const Box gb = m_ba[b].grow(m_nghost);

    for (const Idx& s : shifts) {
      const bool is_identity = (s[0] == 0) && (s[1] == 0) && (s[2] == 0);
      const Box gs = gb.shift(s);

      for (int c = 0; c < nbox; ++c) {
        if (c == b && is_identity) continue;

        const Box inter = gs.intersect(m_ba[c]);
        if (!inter.ok()) continue;

        CopyOp op;
        op.src = inter;
        op.dst = inter.shift(Idx{-s[0], -s[1], -s[2]});
        op.srcBox = c;
        op.dstBox = b;
        op.srcRank = m_dm[c];
        op.dstRank = m_dm[b];
        op.ncomp = 1;
        if (op.srcRank == op.dstRank)
          m_localOps.push_back(op);
        else
          m_remoteOps.push_back(op);
      }
    }
  }

  m_num_local = static_cast<int>(m_localOps.size());
  m_num_remote = static_cast<int>(m_remoteOps.size());
}

void HaloExchange::execute(MultiFab& mf) {
  const int ncomp = mf.numComp();
  m_bytes_sent = 0.0;

  // ------------------------------------------------------------------ local
  if (!m_localOps.empty()) {
    const int nops = static_cast<int>(m_localOps.size());
    View<DevOp> d_ops("halo::localOps", nops);
    auto h_ops = Kokkos::create_mirror_view(d_ops);
    for (int i = 0; i < nops; ++i) {
      const CopyOp& op = m_localOps[i];
      DevOp d;
      d.srcFab = mf.arrayByGlobal(op.srcBox);
      d.dstFab = mf.arrayByGlobal(op.dstBox);
      d.src = op.src;
      d.dst = op.dst;
      d.ncomp = ncomp;
      h_ops(i) = d;
    }
    Kokkos::deep_copy(d_ops, h_ops);

    Kokkos::parallel_for(
        "saturn::haloLocal", Kokkos::TeamPolicy<>(nops, Kokkos::AUTO),
        KOKKOS_LAMBDA(const typename Kokkos::TeamPolicy<>::member_type& team) {
          const DevOp op = d_ops(team.league_rank());
          const int nx = op.src.length(0);
          const int ny = op.src.length(1);
          const BigIndex np = op.src.numPts();
          Kokkos::parallel_for(
              Kokkos::TeamThreadRange(team, np), [&](BigIndex idx) {
                int i, j, k;
                decode(idx, nx, ny, i, j, k);
                for (int c = 0; c < op.ncomp; ++c)
                  op.dstFab(op.dst.lo[0] + i, op.dst.lo[1] + j, op.dst.lo[2] + k, c) =
                      op.srcFab(op.src.lo[0] + i, op.src.lo[1] + j,
                                op.src.lo[2] + k, c);
              });
        });
  }

#ifndef SATURN_USE_MPI
  m_num_remote = 0;
  return;
#else
  if (m_remoteOps.empty() || m_nranks == 1) return;

  // ------------------------------------------------------- group by peer ---
  std::map<int, std::vector<int>> sendMap, recvMap;
  for (int i = 0; i < static_cast<int>(m_remoteOps.size()); ++i) {
    const CopyOp& op = m_remoteOps[i];
    if (op.srcRank == m_myrank) sendMap[op.dstRank].push_back(i);
    if (op.dstRank == m_myrank) recvMap[op.srcRank].push_back(i);
  }

  auto bufferSize = [&](const std::vector<int>& ids) {
    BigIndex n = 0;
    for (int i : ids) n += m_remoteOps[i].src.numPts() * ncomp;
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
    View<DevOp> d_ops("halo::packOps", nops);
    auto h_ops = Kokkos::create_mirror_view(d_ops);
    BigIndex off = 0;
    for (int t = 0; t < nops; ++t) {
      const CopyOp& op = m_remoteOps[p.ops[t]];
      DevOp d;
      d.srcFab = mf.arrayByGlobal(op.srcBox);
      d.src = op.src;
      d.ncomp = ncomp;
      d.off = off;
      h_ops(t) = d;
      off += op.src.numPts() * ncomp;
    }
    Kokkos::deep_copy(d_ops, h_ops);

    Real* buf = p.dev.data();
    Kokkos::parallel_for(
        "saturn::haloPack", Kokkos::TeamPolicy<>(nops, Kokkos::AUTO),
        KOKKOS_LAMBDA(const typename Kokkos::TeamPolicy<>::member_type& team) {
          const DevOp op = d_ops(team.league_rank());
          const int nx = op.src.length(0);
          const int ny = op.src.length(1);
          const BigIndex np = op.src.numPts();
          Kokkos::parallel_for(
              Kokkos::TeamThreadRange(team, np), [&](BigIndex idx) {
                int i, j, k;
                decode(idx, nx, ny, i, j, k);
                for (int c = 0; c < op.ncomp; ++c)
                  buf[op.off + static_cast<BigIndex>(c) * np + idx] =
                      op.srcFab(op.src.lo[0] + i, op.src.lo[1] + j,
                                op.src.lo[2] + k, c);
              });
        });
  }

  // -------------------------------------------------------- communication --
  const int tag = 401;
  std::vector<MPI_Request> reqs(sends.size() + recvs.size(), MPI_REQUEST_NULL);

  int r = 0;
  for (auto& p : sends) {
    Kokkos::deep_copy(p.host, p.dev);
    const int count = static_cast<int>(p.size * static_cast<BigIndex>(sizeof(Real)));
    MPI_Isend(p.host.data(), count, MPI_BYTE, p.rank, tag, MPI_COMM_WORLD, &reqs[r++]);
    m_bytes_sent += static_cast<double>(count);
  }
  for (auto& p : recvs) {
    const int count = static_cast<int>(p.size * static_cast<BigIndex>(sizeof(Real)));
    MPI_Irecv(p.host.data(), count, MPI_BYTE, p.rank, tag, MPI_COMM_WORLD, &reqs[r++]);
  }
  MPI_Waitall(static_cast<int>(reqs.size()), reqs.data(), MPI_STATUSES_IGNORE);

  // ------------------------------------------------------------ unpacking --
  for (auto& p : recvs) {
    Kokkos::deep_copy(p.dev, p.host);

    const int nops = static_cast<int>(p.ops.size());
    View<DevOp> d_ops("halo::unpackOps", nops);
    auto h_ops = Kokkos::create_mirror_view(d_ops);
    BigIndex off = 0;
    for (int t = 0; t < nops; ++t) {
      const CopyOp& op = m_remoteOps[p.ops[t]];
      DevOp d;
      d.dstFab = mf.arrayByGlobal(op.dstBox);
      d.dst = op.dst;
      d.ncomp = ncomp;
      d.off = off;
      h_ops(t) = d;
      off += op.src.numPts() * ncomp;
    }
    Kokkos::deep_copy(d_ops, h_ops);

    const Real* buf = p.dev.data();
    Kokkos::parallel_for(
        "saturn::haloUnpack", Kokkos::TeamPolicy<>(nops, Kokkos::AUTO),
        KOKKOS_LAMBDA(const typename Kokkos::TeamPolicy<>::member_type& team) {
          const DevOp op = d_ops(team.league_rank());
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
