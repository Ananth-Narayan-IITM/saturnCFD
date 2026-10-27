/**
 * @file HaloExchange.hpp
 *
 * saturnCFD — ghost-cell exchange for one AMR level.
 *
 * The exchange is built in two phases:
 *
 * 1. build()   — pure host-side combinatorics. For every locally owned box,
 * walk its grown region (including edges and corners), find
 * which boxes cover that region (with periodic images), and
 * record CopyOps. Cost is O(nbox^2) which is fine for the
 * patch counts we target; a neighbour list replaces it later.
 *
 * 2. execute() — one kernel launch for all local copies, then pack / MPI /
 * unpack for the remote ones, grouped per peer rank.
 *
 * All ranks build an identical global op list, so the per-peer ordering is
 * deterministic and sender/receiver buffers agree without any negotiation.
 *
 * NOTE  Physical boundaries are NOT filled here. Ghost cells that fall outside
 * the domain when the dimension is not periodic must be filled by the
 * boundary-condition layer.
 */
#pragma once

#include <string>
#include <vector>

#include "amr/BoxArray.hpp"
#include "amr/DistributionMapping.hpp"
#include "amr/MultiFab.hpp"
#include "core/Types.hpp"
#include "mesh/Geometry.hpp"

namespace saturn {

/// One rectangular copy from a source box to a destination box.
struct CopyOp {
  Box src;          ///< source region, level index space
  Box dst;          ///< destination region, same extents as src
  int srcBox = -1;  ///< global box index of the source
  int dstBox = -1;  ///< global box index of the destination
  int srcRank = 0;
  int dstRank = 0;
  int ncomp = 1;
};

/**
 * @brief Ghost-cell exchange for one AMR level.
 *
 * build() resolves every patch-to-patch dependency on the host; execute()
 * launches one kernel for local copies and one packed exchange per MPI peer.
 */
class HaloExchange {
 public:
  HaloExchange() = default;

  HaloExchange(const BoxArray& ba, const DistributionMapping& dm,
               const Geometry& geom, int nghost, int myrank, int nranks);

  /// Build the copy-op list. Called once after construction / after regridding.
  void build();

  /// Fill ghost cells of `mf` from its valid cells (and from neighbours).
  /// `mf` must have been built from the same BoxArray / DistributionMapping.
  void execute(MultiFab& mf);

  // ------------------------------------------------------------------ info --
  int numLocalOps() const { return mNumLocalOps; }
  int numRemoteOps() const { return mNumRemoteOps; }
  double bytesSentLastExchange() const { return mBytesSent; }
  const std::string& name() const { return mName; }
  void setName(std::string n) { mName = std::move(n); }

 private:
  BoxArray mBoxArray;
  DistributionMapping mDistributionMap;
  Geometry mGeometry;
  int mNumGhost = 0;
  int mMyRank = 0;
  int mNumRanks = 1;
  std::string mName = "halo";

  std::vector<CopyOp> mLocalOps;
  std::vector<CopyOp> mRemoteOps;
  int mNumLocalOps = 0;
  int mNumRemoteOps = 0;
  double mBytesSent = 0.0;
};

}  // namespace saturn
