#pragma once

#include <stdexcept>
#include <string>

#include <Kokkos_Core.hpp>

#include "comm/Partition.hpp"
#include "core/Types.hpp"

namespace saturn {

/// A cell-centred field with a local ghost layer, backed by Kokkos.
///
/// Storage is local and ghosted:
///
///   nxg = nx + 2*nGrow, nyg = ny + 2*nGrow, nzg = nz + 2*nGrow
///   index(i,j,k,c) = c*nxg*nyg*nzg + kk*nxg*nyg + jj*nxg + ii
///
/// where ii = i - (origin[0] - nGrow), and similarly for j/k. The global
/// (i,j,k) index is kept in the accessor so kernels and halo operations share
/// one convention with MeshPartition/HaloExchange.
class CellField {
 public:
  using memory_space = typename Kokkos::DefaultExecutionSpace::memory_space;
  using view_type = Kokkos::View<Real****, Kokkos::LayoutLeft, memory_space>;
  using host_view_type =
      Kokkos::View<Real****, Kokkos::LayoutLeft, Kokkos::HostSpace>;

  CellField() = default;

  CellField(const MeshPartition& part, int nGrow, int ncomp = 1,
            const std::string& label = "saturn::CellField")
      : mNGrow(nGrow), mNcomp(ncomp) {
    if (nGrow < 0)
      throw std::invalid_argument("CellField: nGrow < 0");
    if (ncomp < 1)
      throw std::invalid_argument("CellField: ncomp < 1");
    for (int d = 0; d < 3; ++d) {
      mOrigin[d] = part.localOrigin(d);
      mExtent[d] = part.localExtent(d);
    }
    mData = view_type(label, mExtent[0] + 2 * nGrow, mExtent[1] + 2 * nGrow,
                      mExtent[2] + 2 * nGrow, ncomp);
  }

  /// Global-index access to the local ghosted storage. Valid on host and
  /// device.
  KOKKOS_INLINE_FUNCTION
  Real& operator()(int i, int j, int k, int c = 0) const {
    return mData(i - mOrigin[0] + mNGrow, j - mOrigin[1] + mNGrow,
                 k - mOrigin[2] + mNGrow, c);
  }

  int nGrow() const { return mNGrow; }
  int ncomp() const { return mNcomp; }
  int origin(int d) const { return mOrigin[d]; }
  int extent(int d) const { return mExtent[d]; }
  int ghostExtent(int d) const { return mExtent[d] + 2 * mNGrow; }

  BigIndex numOwned() const {
    return static_cast<BigIndex>(mExtent[0]) * mExtent[1] * mExtent[2];
  }

  view_type view() const { return mData; }

  /// Copy the whole ghosted field to host memory (synchronous).
  host_view_type hostView() const {
    host_view_type host = Kokkos::create_mirror_view(mData);
    Kokkos::deep_copy(host, mData);
    return host;
  }

  /// Copy a host image back into the field (synchronous).
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
