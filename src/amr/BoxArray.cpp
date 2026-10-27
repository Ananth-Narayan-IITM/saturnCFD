#include "amr/BoxArray.hpp"

#include <stdexcept>

namespace saturn {

BoxArray BoxArray::tileDomain(const Box& domain, const Idx& maxSize,
                              int /*nghost*/) {
  BoxArray ba;
  if (!domain.ok())
    return ba;

  for (int d = 0; d < NDIM; ++d)
    if (maxSize[d] <= 0)
      throw std::invalid_argument("BoxArray::tileDomain: maxSize must be > 0");

  // Number of patches per dimension (ceil division).
  Idx npatch{};
  for (int d = 0; d < NDIM; ++d) {
    const int len = domain.hi[d] - domain.lo[d] + 1;
    npatch[d] = (len + maxSize[d] - 1) / maxSize[d];
  }

  // Walk the patch grid in row-major order.
  Idx pid{};
  bool done = false;
  while (!done) {
    Idx blo{}, bhi{};
    for (int d = 0; d < NDIM; ++d) {
      blo[d] = domain.lo[d] + pid[d] * maxSize[d];
      bhi[d] = std::min(blo[d] + maxSize[d] - 1, domain.hi[d]);
    }
    ba.push_back(Box{blo, bhi});

    // increment
    int d = NDIM - 1;
    while (d >= 0) {
      if (++pid[d] < npatch[d])
        break;
      pid[d] = 0;
      --d;
    }
    if (d < 0)
      done = true;
  }
  return ba;
}

Box BoxArray::boundingBox() const {
  if (mBoxes.empty())
    return Box{};
  Box r = mBoxes[0];
  for (std::size_t i = 1; i < mBoxes.size(); ++i) {
    for (int d = 0; d < NDIM; ++d) {
      r.lo[d] = std::min(r.lo[d], mBoxes[i].lo[d]);
      r.hi[d] = std::max(r.hi[d], mBoxes[i].hi[d]);
    }
  }
  return r;
}

BigIndex BoxArray::numPts() const {
  BigIndex n = 0;
  for (const auto& b : mBoxes)
    n += b.numPts();
  return n;
}

bool BoxArray::disjoint() const {
  for (std::size_t i = 0; i < mBoxes.size(); ++i)
    for (std::size_t j = i + 1; j < mBoxes.size(); ++j)
      if (mBoxes[i].intersects(mBoxes[j]))
        return false;
  return true;
}

}  // namespace saturn
