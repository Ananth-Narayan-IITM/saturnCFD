/**
 * @file MultiFab.hpp
 *
 * saturnCFD — MultiFab: the distributed field container.
 *
 * One MultiFab holds `ncomp` cell-centred components on ONE AMR level, with
 * `nghost` ghost layers, distributed over boxes according to a
 * DistributionMapping.
 *
 * Storage is a single flat Kokkos::View<Real*> covering only the boxes this
 * rank owns, plus a per-box offset table. Kernels are launched over boxes,
 * which is the canonical GPU-AMR pattern: regular inner loops, no indirect
 * addressing beyond one box lookup per team.
 *
 * Access inside kernels goes through Array4, which takes global indices.
 */
#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include "amr/Array4.hpp"
#include "amr/BoxArray.hpp"
#include "amr/DistributionMapping.hpp"
#include "core/Types.hpp"

namespace saturn {

/**
 * @brief Distributed field: `ncomp` components on one level, with ghosts.
 *
 * Storage is a single flat Kokkos::View per rank covering only the boxes that
 * rank owns, plus a per-box offset table. Kernels launch over boxes.
 */
class MultiFab {
 public:
  MultiFab() = default;

  MultiFab(const BoxArray& ba, const DistributionMapping& dm, int ncomp,
           int nghost, int myrank = 0, const std::string& name = "fab");

  // ------------------------------------------------------------------ info --
  int numComp() const { return mNumComp; }
  int nGrow() const { return mNumGhost; }
  int numLocalBoxes() const { return static_cast<int>(mLocalBoxes.size()); }
  int numGlobalBoxes() const { return mBoxArray.size(); }
  int myRank() const { return mMyRank; }
  BigIndex numLocalPts() const { return mLocalCells; }
  BigIndex numGlobalPts() const { return mGlobalCells; }

  /// Global index of the i'th local box.
  int localToGlobal(int i) const { return mLocalBoxes[i]; }

  const BoxArray& boxArray() const { return mBoxArray; }
  const DistributionMapping& distributionMap() const {
    return mDistributionMap;
  }
  const std::string& name() const { return mName; }

  // ---------------------------------------------------------------- access --
  /// Accessor for a LOCAL box index (0..numLocalBoxes-1).
  Array4<Real> array(int localBox) const;

  /// Accessor for a local box, and a component range starting at comp.
  Array4<Real> array(int localBox, int comp) const;

  /// Accessor addressed by GLOBAL box index. Throws if the box is not local.
  Array4<Real> arrayByGlobal(int globalBox) const;

  /// Global box index -> local index, or -1 if not owned by this rank.
  int globalToLocal(int globalBox) const;

  View<Real>& data() { return mData; }
  const View<Real>& data() const { return mData; }

  // ---------------------------------------------------------------- kernels --
  /// Launch `f(int localBox, Array4<Real>, i, j, k)` over every local cell.
  /// One kernel launch, one Kokkos team per patch. Good for GPU (long-running
  /// teams, high occupancy) and for few patches.
  template <class F>
  void forEachCell(F&& f) const;
  template <class F>
  void forEachCell(F&& f);

  /// Same visitor, but flattened: ONE kernel over all local cells, the owning
  /// patch found by binary search on a cumulative-count table. This is the
  /// AMReX approach, and it is the right one when a level has many small
  /// patches, because it removes per-patch launch overhead entirely.
  template <class F>
  void forEachCellFlat(F&& f) const;

  /// Reducing variant of forEachCellFlat, for norms and dot products.
  template <class F, class Reducer>
  void forEachCellFlatReduce(F&& f, Reducer& r) const;

  /// Device view of per-local-patch accessors, so a kernel can reach a second
  /// MultiFab field with the same layout: `other(b)`.
  View<Array4<Real>> boxArrays() const;

  /// Dispatch to whichever of the two paths is better for this patch count.
  /// Measured on OpenMP/x86 (see docs/Benchmarks.md): the flat kernel wins for
  /// 1-4 patches because a single-team TeamPolicy cannot fill the machine; the
  /// team kernel wins from ~16 patches up because it avoids the per-cell binary
  /// search. The threshold is backend dependent and must be re-tuned on GPUs.
  static constexpr int kFlatKernelMaxPatches = 8;

  // NOTE  F is forwarded here, so at the call site it may deduce to an lvalue
  // reference type. The kernel functors therefore store std::decay_t<F> by
  // value: storing `F` directly would create a reference member pointing at a
  // local that dies when this function returns, and the device would then read
  // freed host stack memory.
  template <class F>
  void forEachCellAuto(F&& f) const {
    if (numLocalBoxes() <= kFlatKernelMaxPatches)
      forEachCellFlat(f);
    else
      forEachCell(f);
  }

  /// Skip the inner parallelisation: one thread per box. Useful for tiny boxes.
  template <class F>
  void forEachBoxSerial(F&& f) const;

  // ------------------------------------------------------------ operations --
  void setVal(Real v);
  void setVal(Real v, int comp);
  void copyFrom(const MultiFab& src);

  /// Sum over all components/cells on this rank. (No MPI reduction here.)
  Accum localSum(int comp = 0) const;

  /// Global sum across ranks (requires MPI build; no-op reduction otherwise).
  Accum globalSum(int comp = 0) const;

  /// Min / max over the local data.
  Real localMin(int comp = 0) const;
  Real localMax(int comp = 0) const;

 private:
  void allocate();

  BoxArray mBoxArray;
  DistributionMapping mDistributionMap;
  int mNumComp = 1;
  int mNumGhost = 0;
  int mMyRank = 0;
  std::string mName;

  std::vector<int> mLocalBoxes;   ///< local index -> global box index
  View<Box> mDeviceBoxes;         ///< device copy of local boxes
  View<BigIndex> mDeviceOffsets;  ///< offset of each local box in mData
  View<BigIndex> mDeviceCounts;   ///< cumulative cell counts, size nb+1
  View<Real> mData;

  BigIndex mLocalCells = 0;       ///< storage size incl. ghosts
  BigIndex mLocalValidCells = 0;  ///< live cells, excluding ghosts
  BigIndex mGlobalCells = 0;
};

// ---------------------------------------------------------------------------
// Kernel plumbing
// ---------------------------------------------------------------------------
//
// The visitor is passed as a template argument and stored in a FUNCTOR, not
// captured by a KOKKOS_LAMBDA. That is deliberate: nvcc forbids an extended
// (__host__ __device__) lambda from capturing another lambda closure type, so
// `KOKKOS_LAMBDA(...) { f(...); }` fails to compile on CUDA. A plain functor
// has no such restriction and is also easier to reason about.

namespace detail {

/// Everything a kernel needs to locate a patch's storage on the device.
struct FabMeta {
  const Box* boxes = nullptr;
  const BigIndex* offsets = nullptr;
  const BigIndex* counts = nullptr;
  Real* data = nullptr;
  int nBoxes = 0;
  int nGhost = 0;
  int nComp = 0;

  KOKKOS_INLINE_FUNCTION
  Array4<Real> fab(int b) const {
    const Box bx = boxes[b];
    Array4<Real> a;
    a.p = data + offsets[b];
    for (int d = 0; d < NDIM; ++d) {
      a.begin[d] = bx.lo[d] - nGhost;
      a.n[d] = bx.length(d) + 2 * nGhost;
    }
    if (NDIM == 2) {
      a.begin[2] = 0;
      a.n[2] = 1;
    }
    a.ncomp = nComp;
    return a;
  }
};

template <class F>
struct TeamKernel {
  std::decay_t<F> f;  // BY VALUE: see the note in forEachCellAuto
  FabMeta m;

  KOKKOS_INLINE_FUNCTION
  void operator()(
      const typename Kokkos::TeamPolicy<>::member_type& team) const {
    const int b = team.league_rank();
    const Box bx = m.boxes[b];
    const Array4<Real> a = m.fab(b);

    const int nx = bx.length(0);
    const int ny = bx.length(1);
    const int nz = (NDIM == 3) ? bx.length(2) : 1;
    const BigIndex np = static_cast<BigIndex>(nx) * ny * nz;

    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, np), [&](BigIndex idx) {
      const int i = static_cast<int>(idx % nx);
      const int r = static_cast<int>(idx / nx);
      const int j = r % ny;
      const int k = (NDIM == 3) ? (r / ny) : 0;
      f(b, a, bx.lo[0] + i, bx.lo[1] + j, bx.lo[2] + k);
    });
  }
};

/// Locate the patch owning flat index `t` by binary search on the count table.
KOKKOS_INLINE_FUNCTION
int findBox(const BigIndex* counts, int nBoxes, BigIndex t) {
  int lo = 0, hi = nBoxes - 1;
  while (lo < hi) {
    const int mid = (lo + hi) >> 1;
    if (counts[mid + 1] <= t)
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}

template <class F>
struct FlatKernel {
  std::decay_t<F> f;  // BY VALUE: see the note in forEachCellAuto
  FabMeta m;

  KOKKOS_INLINE_FUNCTION
  void operator()(BigIndex t) const {
    const int b = findBox(m.counts, m.nBoxes, t);
    const Box bx = m.boxes[b];
    const Array4<Real> a = m.fab(b);
    const BigIndex idx = t - m.counts[b];

    const int nx = bx.length(0);
    const int ny = bx.length(1);
    const int i = static_cast<int>(idx % nx);
    const int r = static_cast<int>(idx / nx);
    const int j = r % ny;
    const int k = (NDIM == 3) ? (r / ny) : 0;

    f(b, a, bx.lo[0] + i, bx.lo[1] + j, bx.lo[2] + k);
  }
};

template <class F, class Value>
struct FlatReduceKernel {
  using value_type = Value;
  std::decay_t<F> f;  // BY VALUE: see the note in forEachCellAuto
  FabMeta m;

  KOKKOS_INLINE_FUNCTION
  void operator()(BigIndex t, Value& acc) const {
    const int b = findBox(m.counts, m.nBoxes, t);
    const Box bx = m.boxes[b];
    const Array4<Real> a = m.fab(b);
    const BigIndex idx = t - m.counts[b];

    const int nx = bx.length(0);
    const int ny = bx.length(1);
    const int i = static_cast<int>(idx % nx);
    const int r = static_cast<int>(idx / nx);
    const int j = r % ny;
    const int k = (NDIM == 3) ? (r / ny) : 0;

    f(b, a, bx.lo[0] + i, bx.lo[1] + j, bx.lo[2] + k, acc);
  }
};

}  // namespace detail

// ---------------------------------------------------------------------------
// Template implementations
// ---------------------------------------------------------------------------

template <class F>
void MultiFab::forEachCell(F&& f) const {
  const int nb = numLocalBoxes();
  if (nb == 0)
    return;

  detail::FabMeta meta{mDeviceBoxes.data(),
                       mDeviceOffsets.data(),
                       mDeviceCounts.data(),
                       mData.data(),
                       nb,
                       mNumGhost,
                       mNumComp};

  Kokkos::parallel_for("saturn::MultiFab::forEachCell",
                       Kokkos::TeamPolicy<>(nb, Kokkos::AUTO),
                       detail::TeamKernel<F>{f, meta});
}

template <class F>
void MultiFab::forEachCell(F&& f) {
  static_cast<const MultiFab&>(*this).forEachCell(f);
}

template <class F>
void MultiFab::forEachCellFlat(F&& f) const {
  const int nb = numLocalBoxes();
  if (nb == 0)
    return;

  detail::FabMeta meta{mDeviceBoxes.data(),
                       mDeviceOffsets.data(),
                       mDeviceCounts.data(),
                       mData.data(),
                       nb,
                       mNumGhost,
                       mNumComp};
  const BigIndex total = mLocalValidCells;

  Kokkos::parallel_for("saturn::MultiFab::forEachCellFlat",
                       Kokkos::RangePolicy<>(0, total),
                       detail::FlatKernel<F>{f, meta});
}

template <class F, class Reducer>
void MultiFab::forEachCellFlatReduce(F&& f, Reducer& red) const {
  const int nb = numLocalBoxes();
  if (nb == 0)
    return;

  detail::FabMeta meta{mDeviceBoxes.data(),
                       mDeviceOffsets.data(),
                       mDeviceCounts.data(),
                       mData.data(),
                       nb,
                       mNumGhost,
                       mNumComp};
  const BigIndex total = mLocalValidCells;
  using Value = std::remove_reference_t<decltype(red.reference())>;

  Kokkos::parallel_reduce("saturn::MultiFab::forEachCellFlatReduce",
                          Kokkos::RangePolicy<>(0, total),
                          detail::FlatReduceKernel<F, Value>{f, meta}, red);
}

template <class F>
void MultiFab::forEachBoxSerial(F&& f) const {
  const int nb = numLocalBoxes();
  for (int b = 0; b < nb; ++b)
    f(b, array(b));
}

}  // namespace saturn
