/**
 * @file DistributionMapping.hpp
 *
 * saturnCFD — which MPI rank owns which box.
 *
 * v0.1 uses a greedy longest-processing-time assignment, which is within 4/3 of
 * optimal for identical boxes and is what you want for a uniform patch size.
 * A space-filling-curve mapping (Morton/Hilbert) replaces this once patch costs
 * become heterogeneous.
 */
#pragma once

#include <vector>

#include "amr/BoxArray.hpp"

namespace saturn {

/** @brief Which MPI rank owns which box. */
class DistributionMapping {
 public:
  DistributionMapping() = default;

  /// Assign boxes to ranks, heaviest-first to the currently lightest rank.
  static DistributionMapping roundRobin(const BoxArray& ba, int nranks);

  int size() const { return static_cast<int>(mRank.size()); }
  int nranks() const { return mNumRanks; }
  int operator[](int ibox) const { return mRank[ibox]; }

  /// Global box indices owned by `rank`, in ascending order.
  std::vector<int> boxesOnRank(int rank) const;

  /// Number of boxes owned by `rank`.
  int countOnRank(int rank) const;

 private:
  std::vector<int> mRank;
  int mNumRanks = 1;
};

}  // namespace saturn
