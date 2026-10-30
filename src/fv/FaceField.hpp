#pragma once

#include <stdexcept>
#include <string>

#include <Kokkos_Core.hpp>

#include "comm/Partition.hpp"
#include "core/Types.hpp"

namespace saturn {

/// Face values for the six faces of each owned cell.
///
/// This is a cell-local face representation: `(i,j,k,dir)` is the face of cell
/// `(i,j,k)` in direction `dir` (0 = -x, 1 = +x, ...). An internal face is
/// therefore stored twice, once from each side, with opposite outward normals.
/// That is exactly what cell-centred gradient/divergence loops need and avoids
/// a global face-ownership map in the first implementation.
class FaceField {
 public:
  using memory_space = typename Kokkos::DefaultExecutionSpace::memory_space;
  using view_type = Kokkos::View<Real*****, Kokkos::LayoutLeft, memory_space>;
  using host_view_type =
      Kokkos::View<Real*****, Kokkos::LayoutLeft, Kokkos::HostSpace>;

  FaceField() = default;

  FaceField(const MeshPartition& part, int nGrow, int ncomp = 1,
            const std::string& label = "saturn::FaceField")
      : mNGrow(nGrow), mNcomp(ncomp) {
    if (nGrow < 0)
      throw std::invalid_argument("FaceField: nGrow < 0");
    if (ncomp < 1)
      throw std::invalid_argument("FaceField: ncomp < 1");
    for (int d = 0; d < 3; ++d) {
      mOrigin[d] = part.localOrigin(d);
      mExtent[d] = part.localExtent(d);
    }
    mData = view_type(label, mExtent[0] + 2 * nGrow, mExtent[1] + 2 * nGrow,
                      mExtent[2] + 2 * nGrow, 6, ncomp);
  }

  KOKKOS_INLINE_FUNCTION
  Real& operator()(int i, int j, int k, int dir, int c = 0) const {
    return mData(i - mOrigin[0] + mNGrow, j - mOrigin[1] + mNGrow,
                 k - mOrigin[2] + mNGrow, dir, c);
  }

  int nGrow() const { return mNGrow; }
  int ncomp() const { return mNcomp; }
  int origin(int d) const { return mOrigin[d]; }
  int extent(int d) const { return mExtent[d]; }
  int ghostExtent(int d) const { return mExtent[d] + 2 * mNGrow; }

  view_type view() const { return mData; }

  host_view_type hostView() const {
    host_view_type host = Kokkos::create_mirror_view(mData);
    Kokkos::deep_copy(host, mData);
    return host;
  }

  void syncFromHost(const host_view_type& host) {
    Kokkos::deep_copy(mData, host);
  }

  void fill(Real value) { Kokkos::deep_copy(mData, value); }

 private:
  view_type mData;
  int mNGrow = 0;
  int mNcomp = 1;
  int mOrigin[3] = {0, 0, 0};
  int mExtent[3] = {0, 0, 0};
};

}  // namespace saturn
