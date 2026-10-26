#include "amr/MultiFab.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {

MultiFab::MultiFab(const BoxArray& ba, const DistributionMapping& dm, int ncomp,
                   int nghost, int myrank, const std::string& name)
    : m_ba(ba),
      m_dm(dm),
      m_ncomp(ncomp),
      m_nghost(nghost),
      m_myrank(myrank),
      m_name(name) {
  if (ncomp < 1) throw std::invalid_argument("MultiFab: ncomp < 1");
  if (nghost < 0) throw std::invalid_argument("MultiFab: nghost < 0");
  if (dm.size() != ba.size())
    throw std::invalid_argument("MultiFab: BoxArray / DistributionMapping size mismatch");
  m_local = dm.boxesOnRank(myrank);
  allocate();
}

void MultiFab::allocate() {
  const int nb = static_cast<int>(m_local.size());

  m_d_boxes = View<Box>("saturn::boxes", nb);
  m_d_off = View<BigIndex>("saturn::offsets", nb);
  m_d_counts = View<BigIndex>("saturn::counts", nb + 1);
  auto h_boxes = Kokkos::create_mirror_view(m_d_boxes);
  auto h_off = Kokkos::create_mirror_view(m_d_off);
  auto h_counts = Kokkos::create_mirror_view(m_d_counts);

  BigIndex total = 0;
  h_counts(0) = 0;
  const BigIndex ng = m_nghost;
  for (int b = 0; b < nb; ++b) {
    const Box bx = m_ba[m_local[b]];
    h_boxes(b) = bx;
    h_off(b) = total;
    BigIndex with_ghost = 1;
    for (int d = 0; d < NDIM; ++d) with_ghost *= (bx.length(d) + 2 * ng);
    total += with_ghost * m_ncomp;
    h_counts(b + 1) = h_counts(b) + bx.numPts();
  }
  m_local_pts = total;

  Kokkos::deep_copy(m_d_boxes, h_boxes);
  Kokkos::deep_copy(m_d_off, h_off);
  Kokkos::deep_copy(m_d_counts, h_counts);

  m_data = View<Real>(m_name + "::data", total);
  Kokkos::deep_copy(m_data, Real(0));

  // Global point count needs a reduction over the DistributionMapping.
  m_global_pts = 0;
  for (int i = 0; i < m_ba.size(); ++i) m_global_pts += m_ba[i].numPts();
}

Array4<Real> MultiFab::array(int localBox) const {
  Array4<Real> a;
  if (localBox < 0 || localBox >= numLocalBoxes())
    throw std::out_of_range("MultiFab::array: local box out of range");

  const int g = m_local[localBox];
  const Box bx = m_ba[g];

  BigIndex base = 0;
  for (int b = 0; b < localBox; ++b) {
    const Box pb = m_ba[m_local[b]];
    BigIndex n = 1;
    for (int d = 0; d < NDIM; ++d) n *= (pb.length(d) + 2 * m_nghost);
    base += n * m_ncomp;
  }

  a.p = m_data.data() + base;
  for (int d = 0; d < NDIM; ++d) {
    a.begin[d] = bx.lo[d] - m_nghost;
    a.n[d] = bx.length(d) + 2 * m_nghost;
  }
  if (NDIM == 2) {
    a.begin[2] = 0;
    a.n[2] = 1;
  }
  a.ncomp = m_ncomp;
  return a;
}

Array4<Real> MultiFab::array(int localBox, int comp) const {
  Array4<Real> a = array(localBox);
  a.p += static_cast<BigIndex>(comp) * (a.n[0] * a.n[1] * a.n[2]);
  return a;
}

int MultiFab::globalToLocal(int globalBox) const {
  for (int i = 0; i < numLocalBoxes(); ++i)
    if (m_local[i] == globalBox) return i;
  return -1;
}

View<Array4<Real>> MultiFab::boxArrays() const {
  const int nb = numLocalBoxes();
  View<Array4<Real>> v("saturn::boxArrays", nb);
  auto h = Kokkos::create_mirror_view(v);
  for (int b = 0; b < nb; ++b) h(b) = array(b);
  Kokkos::deep_copy(v, h);
  return v;
}

Array4<Real> MultiFab::arrayByGlobal(int globalBox) const {
  const int loc = globalToLocal(globalBox);
  if (loc < 0)
    throw std::out_of_range("MultiFab::arrayByGlobal: box " +
                            std::to_string(globalBox) + " is not local");
  return array(loc);
}

void MultiFab::setVal(Real v) {
  Kokkos::deep_copy(m_data, v);
}

void MultiFab::setVal(Real v, int comp) {
  const int loc = numLocalBoxes();
  for (int b = 0; b < loc; ++b) {
    Array4<Real> a = array(b);
    const int stride = a.n[0] * a.n[1] * a.n[2];
    Real* ptr = a.p + static_cast<BigIndex>(comp) * stride;
    Kokkos::parallel_for(
        "saturn::setVal", Kokkos::RangePolicy<>(0, stride),
        KOKKOS_LAMBDA(int i) { ptr[i] = v; });
  }
}

void MultiFab::copyFrom(const MultiFab& src) {
  if (src.numLocalBoxes() != numLocalBoxes() || src.numComp() != numComp())
    throw std::invalid_argument("MultiFab::copyFrom: layout mismatch");
  Kokkos::deep_copy(m_data, src.data());
}

Accum MultiFab::localSum(int comp) const {
  const int nb = numLocalBoxes();
  auto d_boxes = m_d_boxes;
  auto d_off = m_d_off;
  auto d_data = m_data;
  const int ng = m_nghost;
  Accum s0 = 0;
  Kokkos::Sum<Accum> r(s0);
  Kokkos::parallel_reduce(
      "saturn::localSum", Kokkos::RangePolicy<>(0, nb),
      KOKKOS_LAMBDA(int b, Accum& acc) {
        const Box bx = d_boxes(b);
        Array4<Real> a;
        a.p = d_data.data() + d_off(b);
        for (int d = 0; d < NDIM; ++d) {
          a.begin[d] = bx.lo[d] - ng;
          a.n[d] = bx.length(d) + 2 * ng;
        }
        if (NDIM == 2) {
          a.begin[2] = 0;
          a.n[2] = 1;
        }
        a.ncomp = 1;
        for (int k = 0; k < ((NDIM == 3) ? bx.length(2) : 1); ++k)
          for (int j = 0; j < bx.length(1); ++j)
            for (int i = 0; i < bx.length(0); ++i)
              acc += static_cast<Accum>(a(bx.lo[0] + i, bx.lo[1] + j,
                                          bx.lo[2] + k, comp));
      },
      r);
  return r.reference();
}

Accum MultiFab::globalSum(int comp) const {
  Accum s = localSum(comp);
#ifdef SATURN_USE_MPI
  Accum g = 0;
  MPI_Allreduce(&s, &g, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  return g;
#else
  return s;
#endif
}

Real MultiFab::localMin(int comp) const {
  using Min = Kokkos::Min<Real>;
  Real m0 = std::numeric_limits<Real>::max();
  Min r(m0);
  const int nb = numLocalBoxes();
  const MultiFab& self = *this;
  for (int b = 0; b < nb; ++b) {
    Array4<Real> a = self.array(b);
    const int n0 = a.n[0], n1 = a.n[1], n2 = a.n[2];
    Real* ptr = a.p + static_cast<BigIndex>(comp) * (n0 * n1 * n2);
    Kokkos::parallel_reduce(
        "saturn::localMin", Kokkos::RangePolicy<>(0, n0 * n1 * n2),
        KOKKOS_LAMBDA(int i, Real& m) {
          const Real v = ptr[i];
          if (v < m) m = v;
        },
        r);
  }
  return r.reference();
}

Real MultiFab::localMax(int comp) const {
  using Max = Kokkos::Max<Real>;
  Real m1 = std::numeric_limits<Real>::lowest();
  Max r(m1);
  const int nb = numLocalBoxes();
  const MultiFab& self = *this;
  for (int b = 0; b < nb; ++b) {
    Array4<Real> a = self.array(b);
    const int n0 = a.n[0], n1 = a.n[1], n2 = a.n[2];
    Real* ptr = a.p + static_cast<BigIndex>(comp) * (n0 * n1 * n2);
    Kokkos::parallel_reduce(
        "saturn::localMax", Kokkos::RangePolicy<>(0, n0 * n1 * n2),
        KOKKOS_LAMBDA(int i, Real& m) {
          const Real v = ptr[i];
          if (v > m) m = v;
        },
        r);
  }
  return r.reference();
}

}  // namespace saturn
