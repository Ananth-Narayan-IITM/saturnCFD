#include "amr/DistributionMapping.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace saturn {

DistributionMapping DistributionMapping::roundRobin(const BoxArray& ba,
                                                    int nranks) {
  if (nranks < 1)
    throw std::invalid_argument("DistributionMapping: nranks < 1");

  DistributionMapping dm;
  dm.mNumRanks = nranks;
  dm.mRank.assign(ba.size(), 0);

  // Heaviest-first ordering; ties broken by index for determinism.
  std::vector<int> order(ba.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    return ba[a].numPts() > ba[b].numPts();
  });

  std::vector<BigIndex> load(nranks, 0);
  for (int ibox : order) {
    const int r = static_cast<int>(std::min_element(load.begin(), load.end()) -
                                   load.begin());
    dm.mRank[ibox] = r;
    load[r] += ba[ibox].numPts();
  }
  return dm;
}

std::vector<int> DistributionMapping::boxesOnRank(int rank) const {
  std::vector<int> out;
  for (int i = 0; i < size(); ++i)
    if (mRank[i] == rank)
      out.push_back(i);
  return out;
}

int DistributionMapping::countOnRank(int rank) const {
  int n = 0;
  for (int r : mRank)
    if (r == rank)
      ++n;
  return n;
}

}  // namespace saturn
