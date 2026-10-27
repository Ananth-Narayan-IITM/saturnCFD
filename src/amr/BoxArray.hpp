/**
 * @file BoxArray.hpp
 *
 * saturnCFD — a collection of disjoint Boxes describing one AMR level.
 *
 * Levels are built by: (1) tiling the domain into patches, (2) clustering
 * tagged cells into new patches during regridding. Both produce a BoxArray.
 */
#pragma once

#include <algorithm>
#include <vector>

#include "amr/Box.hpp"
#include "core/Types.hpp"

namespace saturn {

/** @brief A set of disjoint boxes describing one AMR level. */
class BoxArray {
 public:
  BoxArray() = default;
  explicit BoxArray(const std::vector<Box>& boxes) : mBoxes(boxes) {}

  /// Tile `domain` into patches of at most `maxSize` cells per dimension.
  /// The last patch in each dimension absorbs the remainder (may be larger
  /// than maxSize, never smaller than one cell).
  static BoxArray tileDomain(const Box& domain, const Idx& maxSize,
                             int nghost = 0);

  void push_back(const Box& b) { mBoxes.push_back(b); }
  void clear() { mBoxes.clear(); }

  int size() const { return static_cast<int>(mBoxes.size()); }
  bool empty() const { return mBoxes.empty(); }

  const Box& operator[](int i) const { return mBoxes[i]; }
  Box& operator[](int i) { return mBoxes[i]; }

  const std::vector<Box>& boxes() const { return mBoxes; }

  /// Smallest box containing every box in the array. Empty if array empty.
  Box boundingBox() const;

  /// Total number of cells (excluding ghosts) across all boxes.
  BigIndex numPts() const;

  /// True if no two boxes overlap.
  bool disjoint() const;

 private:
  std::vector<Box> mBoxes;
};

}  // namespace saturn
