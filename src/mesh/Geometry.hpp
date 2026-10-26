// saturnCFD — problem geometry. Index space is integer; physical space is
// obtained with a single uniform dx per level.
#pragma once

#include "amr/Box.hpp"
#include "core/Types.hpp"

namespace saturn {

struct Geometry {
  Box domain;          ///< level-0 index space, cell-centred
  Real dx = 1;         ///< cell size on THIS level
  Real origin[NDIM3] = {0, 0, 0};          ///< physical coord of cell-centre of lo[0..]
  bool periodic[NDIM3] = {false, false, false};

  int nCells(int d) const { return domain.hi[d] - domain.lo[d] + 1; }

  BigIndex domainPts() const { return domain.numPts(); }

  /// Physical extent of the domain.
  Real length(int d) const { return static_cast<Real>(nCells(d)) * dx; }

  /// Cell-centre coordinate of a global index.
  KOKKOS_INLINE_FUNCTION
  Real xc(int d, Index i) const {
    return origin[d] + (static_cast<Real>(i - domain.lo[d]) + Real(0.5)) * dx;
  }

  /// The same geometry refined by 2^level (AMR level descriptor).
  Geometry refined(int level) const {
    Geometry g = *this;
    g.dx = dx / static_cast<Real>(1 << level);
    for (int d = 0; d < NDIM; ++d) {
      g.domain.lo[d] = domain.lo[d] << level;
      g.domain.hi[d] = ((domain.hi[d] + 1) << level) - 1;
    }
    return g;
  }
};

}  // namespace saturn
