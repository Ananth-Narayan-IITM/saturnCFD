#include <doctest/doctest.h>

#include <cmath>
#include <iomanip>
#include <string>

#include <Kokkos_Core.hpp>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

#include "comm/HaloExchange.hpp"
#include "comm/Partition.hpp"
#include "fv/CartesianGeometry.hpp"
#include "fv/CellField.hpp"
#include "fv/FieldHalo.hpp"
#include "fv/Laplacian.hpp"

using namespace saturn;

namespace {

KOKKOS_INLINE_FUNCTION constexpr double pi() {
  return 3.14159265358979323846;
}

/// Periodic manufactured solution on the unit box. `dim` selects 2D or 3D.
KOKKOS_INLINE_FUNCTION double mms(double x, double y, double z, int dim) {
  double v = Kokkos::sin(2 * pi() * x);
  if (dim >= 2)
    v *= Kokkos::sin(2 * pi() * y);
  if (dim >= 3)
    v *= Kokkos::sin(2 * pi() * z);
  return v;
}

/// Source s = Laplacian(phi) for the manufactured field above.
KOKKOS_INLINE_FUNCTION double mmsSource(double x, double y, double z, int dim) {
  const double k = 2 * pi();
  return -static_cast<double>(dim) * k * k * mms(x, y, z, dim);
}

double globalSum(double value) {
#ifdef SATURN_USE_MPI
  double out = 0;
  MPI_Allreduce(&value, &out, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  return out;
#else
  return value;
#endif
}

double globalMax(double value) {
#ifdef SATURN_USE_MPI
  double out = 0;
  MPI_Allreduce(&value, &out, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  return out;
#else
  return value;
#endif
}

using Policy3 = Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>;

Policy3 ownedPolicy(const CellField& field) {
  return Policy3(
      {field.origin(0), field.origin(1), field.origin(2)},
      {field.origin(0) + field.extent(0), field.origin(1) + field.extent(1),
       field.origin(2) + field.extent(2)});
}

void fillOwned(CellField& field, const CartesianGeometry& geom) {
  const int nc = field.ncomp();
  const CartesianGeometry g = geom;
  CellField f = field;
  Kokkos::parallel_for(
      "fv::fill_mms", ownedPolicy(field), KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = g.cellCentre(i, j, k);
        for (int c = 0; c < nc; ++c)
          f(i, j, k, c) = mms(x.x, x.y, x.z, g.dim);
      });
}

double sumOwned(const CellField& field) {
  const int nc = field.ncomp();
  CellField f = field;
  double local = 0;
  Kokkos::parallel_reduce(
      "fv::sum_owned", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int c = 0; c < nc; ++c)
          acc += f(i, j, k, c);
      },
      local);
  return globalSum(local);
}

/// Max error between the ghost cells and the periodic manufactured field.
/// Physical-boundary ghosts are skipped by construction (periodic test only).
double ghostError(const CellField& field, const CartesianGeometry& geom, int nx,
                  int ny, int nz, int comp = 0, double offset = 0.0) {
  const int ng = field.nGrow();
  const int i0 = field.origin(0);
  const int j0 = field.origin(1);
  const int k0 = field.origin(2);
  const int lx = field.extent(0);
  const int ly = field.extent(1);
  const int lz = field.extent(2);
  const int dim = geom.dim;
  const CartesianGeometry g = geom;
  CellField f = field;

  double local = 0;
  Kokkos::parallel_reduce(
      "fv::ghost_error",
      Policy3({i0 - ng, j0 - ng, k0 - ng},
              {i0 + lx + ng, j0 + ly + ng, k0 + lz + ng}),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const bool owned = i >= i0 && i < i0 + lx && j >= j0 && j < j0 + ly &&
                           k >= k0 && k < k0 + lz;
        if (owned)
          return;
        // Inactive directions have no halo (thin 2D mesh).
        if (dim < 3 && (k < k0 || k >= k0 + lz))
          return;
        if (dim < 2 && (j < j0 || j >= j0 + ly))
          return;

        int ii = i % nx;
        if (ii < 0)
          ii += nx;
        int jj = j % ny;
        if (jj < 0)
          jj += ny;
        int kk = k % nz;
        if (kk < 0)
          kk += nz;

        const Vec3 x = g.cellCentre(ii, jj, kk);
        acc = Kokkos::max(acc, Kokkos::abs(f(i, j, k, comp) -
                                           (mms(x.x, x.y, x.z, dim) + offset)));
      },
      Kokkos::Max<double>(local));
  return globalMax(local);
}

struct Norms {
  double l1 = 0;
  double l2 = 0;
  double linf = 0;
};

Norms mmsLaplacianNorms(int n, int dim, int nGrow) {
  const int nz = (dim == 3) ? n : 1;
  const double h = 1.0 / static_cast<double>(n);
  MeshPartition part(n, n, nz, true, true, dim == 3);
  CartesianGeometry geom({0, 0, 0}, {h, h, h}, dim);

  CellField phi(part, nGrow, 1, "saturn::mms_phi");
  CellField lap(part, nGrow, 1, "saturn::mms_lap");
  fillOwned(phi, geom);

  HaloExchange halo(part, nGrow, 1);
  FieldHalo fieldHalo(halo, 1);
  fieldHalo.execute(phi);
  laplacian(phi, lap, geom);

  const int d = dim;
  const CartesianGeometry g = geom;
  CellField l = lap;

  double localL1 = 0;
  Kokkos::parallel_reduce(
      "fv::mms_l1", ownedPolicy(lap),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 x = g.cellCentre(i, j, k);
        acc += Kokkos::abs(l(i, j, k) - mmsSource(x.x, x.y, x.z, d));
      },
      localL1);

  double localL2 = 0;
  Kokkos::parallel_reduce(
      "fv::mms_l2", ownedPolicy(lap),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 x = g.cellCentre(i, j, k);
        const double e = l(i, j, k) - mmsSource(x.x, x.y, x.z, d);
        acc += e * e;
      },
      localL2);

  double localLinf = 0;
  Kokkos::parallel_reduce(
      "fv::mms_linf", ownedPolicy(lap),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        const Vec3 x = g.cellCentre(i, j, k);
        acc = Kokkos::max(
            acc, Kokkos::abs(l(i, j, k) - mmsSource(x.x, x.y, x.z, d)));
      },
      Kokkos::Max<double>(localLinf));

  const double count = static_cast<double>(n) * n * nz;
  Norms out;
  out.l1 = globalSum(localL1) / count;
  out.l2 = std::sqrt(globalSum(localL2) / count);
  out.linf = globalMax(localLinf);
  return out;
}

double observedOrder(double coarse, double fine) {
  return std::log(coarse / fine) / std::log(2.0);
}

// Measured on the CPU OpenMP build with 32^3 cells, two ghost layers.
// The GPU (double) build must reproduce this to within reduction roundoff.
constexpr double kReferenceL2 = 0.13435580757778084;

}  // namespace

TEST_CASE("fv: CellField fill and owned-cell reduction") {
  MeshPartition part(16, 16, 16, false, false, false);
  CellField field(part, 1, 1, "saturn::fill_test");
  field.fill(2.0);
  CHECK(sumOwned(field) == doctest::Approx(2.0 * 16 * 16 * 16));
}

TEST_CASE("fv: Kokkos halo fills two periodic ghost layers") {
  const int n = 16;
  const int ng = 2;
  const double h = 1.0 / n;
  MeshPartition part(n, n, n, true, true, true);
  CartesianGeometry geom({0, 0, 0}, {h, h, h}, 3);
  CellField field(part, ng, 1, "saturn::halo_test");
  fillOwned(field, geom);

  HaloExchange halo(part, ng, 1);
  FieldHalo fieldHalo(halo, 1);
  CHECK(fieldHalo.numSendOps() == halo.numSendOps());
  CHECK(fieldHalo.numRecvOps() == halo.numRecvOps());

  fieldHalo.execute(field);
  CHECK(ghostError(field, geom, n, n, n) < 1e-12);
}

TEST_CASE("fv: Kokkos halo exchanges two components") {
  const int n = 8;
  const int ng = 1;
  const double h = 1.0 / n;
  MeshPartition part(n, n, n, true, true, true);
  CartesianGeometry geom({0, 0, 0}, {h, h, h}, 3);
  CellField field(part, ng, 2, "saturn::halo_components");
  const CartesianGeometry g = geom;
  CellField f = field;
  Kokkos::parallel_for(
      "fv::fill_components", ownedPolicy(field),
      KOKKOS_LAMBDA(int i, int j, int k) {
        const Vec3 x = g.cellCentre(i, j, k);
        f(i, j, k, 0) = mms(x.x, x.y, x.z, 3);
        f(i, j, k, 1) = mms(x.x, x.y, x.z, 3) + 1.0;
      });

  HaloExchange halo(part, ng, 2);
  FieldHalo fieldHalo(halo, 2);
  fieldHalo.execute(field);
  CHECK(ghostError(field, geom, n, n, n, 0, 0.0) < 1e-12);
  CHECK(ghostError(field, geom, n, n, n, 1, 1.0) < 1e-12);
}

TEST_CASE("fv: 3D periodic Laplacian MMS is second order") {
  const int n0 = 8;
  const Norms e0 = mmsLaplacianNorms(n0, 3, 2);
  const Norms e1 = mmsLaplacianNorms(2 * n0, 3, 2);
  const Norms e2 = mmsLaplacianNorms(4 * n0, 3, 2);

  const double p1 = observedOrder(e0.l2, e1.l2);
  const double p2 = observedOrder(e1.l2, e2.l2);
  MESSAGE("3D MMS L2: " << std::setprecision(17) << e0.l2 << " " << e1.l2 << " "
                        << e2.l2);
  MESSAGE("3D MMS orders: " << p1 << " " << p2);

  CHECK(e2.l2 < e1.l2);
  CHECK(e1.l2 < e0.l2);
  CHECK(std::abs(p1 - 2.0) < 0.2);
  CHECK(std::abs(p2 - 2.0) < 0.2);
}

TEST_CASE("fv: 2D periodic Laplacian MMS is second order") {
  const int n0 = 16;
  const Norms e0 = mmsLaplacianNorms(n0, 2, 2);
  const Norms e1 = mmsLaplacianNorms(2 * n0, 2, 2);
  const Norms e2 = mmsLaplacianNorms(4 * n0, 2, 2);

  const double p1 = observedOrder(e0.l2, e1.l2);
  const double p2 = observedOrder(e1.l2, e2.l2);
  MESSAGE("2D MMS L2: " << e0.l2 << " " << e1.l2 << " " << e2.l2);
  MESSAGE("2D MMS orders: " << p1 << " " << p2);

  CHECK(e2.l2 < e1.l2);
  CHECK(e1.l2 < e0.l2);
  CHECK(std::abs(p1 - 2.0) < 0.2);
  CHECK(std::abs(p2 - 2.0) < 0.2);
}

TEST_CASE("fv: manufactured Laplacian reference norm") {
  const Norms e = mmsLaplacianNorms(32, 3, 2);
  MESSAGE("32^3 MMS L2 = " << std::setprecision(17) << e.l2 << " (reference "
                           << kReferenceL2 << ")");
  CHECK(std::abs(e.l2 - kReferenceL2) < 1e-10);
}

#ifdef SATURN_BACKEND_CUDA
TEST_CASE("fv gpu: CUDA execution space is active") {
  const std::string name = Kokkos::DefaultExecutionSpace::name();
  CHECK(name.find("Cuda") != std::string::npos);
  CHECK(Kokkos::DefaultExecutionSpace::concurrency() > 1);
}

TEST_CASE("fv gpu: 32^3 MMS matches the CPU reference on device") {
  // Same double-precision operator as the CPU build. Reduction ordering can
  // differ on the device, so allow a roundoff-sized window around the CPU
  // reference; a wrong stencil or halo is far larger than this.
  const Norms coarse = mmsLaplacianNorms(16, 3, 2);
  const Norms fine = mmsLaplacianNorms(32, 3, 2);
  MESSAGE("GPU 32^3 MMS L2 = " << std::setprecision(17) << fine.l2
                               << " (CPU reference " << kReferenceL2 << ")");
  CHECK(std::abs(fine.l2 - kReferenceL2) < 1e-9);
  CHECK(std::abs(observedOrder(coarse.l2, fine.l2) - 2.0) < 0.2);
}

TEST_CASE("fv gpu: periodic halo ghost error stays at roundoff") {
  const int n = 16;
  const int ng = 2;
  const double h = 1.0 / n;
  MeshPartition part(n, n, n, true, true, true);
  CartesianGeometry geom({0, 0, 0}, {h, h, h}, 3);
  CellField field(part, ng, 1, "saturn::gpu_halo_test");
  fillOwned(field, geom);

  HaloExchange halo(part, ng, 1);
  FieldHalo fieldHalo(halo, 1);
  fieldHalo.execute(field);
  CHECK(ghostError(field, geom, n, n, n) < 1e-12);
}
#endif
