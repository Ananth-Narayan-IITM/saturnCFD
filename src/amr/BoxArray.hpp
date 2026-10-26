// saturnCFD — a collection of disjoint Boxes describing one AMR level.
//
// Levels are built by: (1) tiling the domain into patches, (2) clustering
// tagged cells into new patches during regridding. Both produce a BoxArray.
#pragma once

#include <algorithm>
#include <vector>

#include "amr/Box.hpp"
#include "core/Types.hpp"

namespace saturn {

class BoxArray {
 public:
  BoxArray() = default;
  explicit BoxArray(const std::vector<Box>& boxes) : m_boxes(boxes) {}

  /// Tile `domain` into patches of at most `maxSize` cells per dimension.
  /// The last patch in each dimension absorbs the remainder (may be larger
  /// than maxSize, never smaller than one cell).
  static BoxArray tileDomain(const Box& domain, const Idx& maxSize,
                             int nghost = 0);

  void push_back(const Box& b) { m_boxes.push_back(b); }
  void clear() { m_boxes.clear(); }

  int size() const { return static_cast<int>(m_boxes.size()); }
  bool empty() const { return m_boxes.empty(); }

  const Box& operator[](int i) const { return m_boxes[i]; }
  Box& operator[](int i) { return m_boxes[i]; }

  const std::vector<Box>& boxes() const { return m_boxes; }

  /// Smallest box containing every box in the array. Empty if array empty.
  Box boundingBox() const;

  /// Total number of cells (excluding ghosts) across all boxes.
  BigIndex numPts() const;

  /// True if no two boxes overlap.
  bool disjoint() const;

 private:
  std::vector<Box> m_boxes;
};

}  // namespace saturn
