#pragma once

#include <vector>

#include "comm/Partition.hpp"
#include "core/Types.hpp"

namespace saturn {

/// Host-staged halo exchange for a cell-centred field with `nGrow` ghosts.
/// Field layout is local ghosted: index = c*sc + kk*sz + jj*sy + ii, with
/// ii = i - (origin[0] - nGrow), etc.
class HaloExchange {
 public:
  HaloExchange(const MeshPartition& part, int nGrow, int ncomp);

  HaloExchange(const HaloExchange&) = delete;
  HaloExchange& operator=(const HaloExchange&) = delete;

  /// Fill ghosts of `data` from neighbouring ranks and local periodic images.
  /// Physical boundaries are not filled here.
  void execute(Real* data);

  /// One halo message: the peer, direction, global box and MPI tag.
  /// The host path owns `buffer`; device-field adapters use `box`/`tag`.
  struct Op {
    int peer = kProcNull;
    int dx = 0;
    int dy = 0;
    int dz = 0;
    IndexBox box;
    int tag = 0;
    std::vector<Real> buffer;
  };

  const std::vector<Op>& sendOps() const { return mSendOps; }
  const std::vector<Op>& recvOps() const { return mRecvOps; }
  const MeshPartition& partition() const { return mPart; }

  int numSendOps() const { return static_cast<int>(mSendOps.size()); }
  int numRecvOps() const { return static_cast<int>(mRecvOps.size()); }
  int nGrow() const { return mNGrow; }

 private:
  const MeshPartition& mPart;
  int mNGrow = 0;
  int mNcomp = 1;
  std::vector<Op> mSendOps;
  std::vector<Op> mRecvOps;

  void build();
  int localIndex(int global, int d) const;
  void pack(const IndexBox& box, const Real* data,
            std::vector<Real>& out) const;
  void unpack(const IndexBox& box, const std::vector<Real>& in,
              Real* data) const;
  void copyBox(const IndexBox& src, const IndexBox& dst, Real* data) const;
};

}  // namespace saturn
