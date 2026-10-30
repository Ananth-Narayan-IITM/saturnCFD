#include <doctest/doctest.h>

#include <cmath>
#include <limits>
#include <vector>

#include <mpi.h>

#include "comm/HaloExchange.hpp"
#include "comm/Partition.hpp"

using namespace saturn;

namespace {

constexpr double kPi = 3.14159265358979323846;

double globalMax(double local) {
  double out = 0;
  MPI_Allreduce(&local, &out, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  return out;
}

double analytic(int i, int j, int k, int nx, int ny, int nz) {
  double v = std::sin(2 * kPi * i / nx) + std::cos(2 * kPi * j / ny);
  if (nz > 1)
    v += std::sin(2 * kPi * k / nz);
  return v;
}

struct Field {
  const MeshPartition& p;
  int ng;
  int nxg, nyg, nzg;
  std::vector<double> data;

  Field(const MeshPartition& part, int ghost)
      : p(part),
        ng(ghost),
        nxg(p.localExtent(0) + 2 * ghost),
        nyg(p.localExtent(1) + 2 * ghost),
        nzg(p.localExtent(2) + 2 * ghost),
        data(static_cast<std::size_t>(nxg) * nyg * nzg,
             std::numeric_limits<double>::quiet_NaN()) {}

  double& at(int i, int j, int k) {
    const int ii = i - (p.localOrigin(0) - ng);
    const int jj = j - (p.localOrigin(1) - ng);
    const int kk = k - (p.localOrigin(2) - ng);
    return data[static_cast<std::size_t>(kk) * nxg * nyg +
                static_cast<std::size_t>(jj) * nxg + ii];
  }

  double at(int i, int j, int k) const {
    const int ii = i - (p.localOrigin(0) - ng);
    const int jj = j - (p.localOrigin(1) - ng);
    const int kk = k - (p.localOrigin(2) - ng);
    return data[static_cast<std::size_t>(kk) * nxg * nyg +
                static_cast<std::size_t>(jj) * nxg + ii];
  }
};

void fillValid(Field& f, int nx, int ny, int nz) {
  for (int k = f.p.localOrigin(2); k < f.p.localOrigin(2) + f.p.localExtent(2);
       ++k)
    for (int j = f.p.localOrigin(1);
         j < f.p.localOrigin(1) + f.p.localExtent(1); ++j)
      for (int i = f.p.localOrigin(0);
           i < f.p.localOrigin(0) + f.p.localExtent(0); ++i)
        f.at(i, j, k) = analytic(i, j, k, nx, ny, nz);
}

double ghostError(const Field& f, int nx, int ny, int nz) {
  double err = 0;
  for (int k = f.p.localOrigin(2) - f.ng;
       k < f.p.localOrigin(2) + f.p.localExtent(2) + f.ng; ++k) {
    for (int j = f.p.localOrigin(1) - f.ng;
         j < f.p.localOrigin(1) + f.p.localExtent(1) + f.ng; ++j) {
      for (int i = f.p.localOrigin(0) - f.ng;
           i < f.p.localOrigin(0) + f.p.localExtent(0) + f.ng; ++i) {
        const bool inside =
            i >= 0 && i < nx && j >= 0 && j < ny && k >= 0 && k < nz;
        if (!inside)
          continue;  // physical boundary: not filled by halo
        const double d =
            std::abs(f.at(i, j, k) - analytic(i, j, k, nx, ny, nz));
        if (d > err)
          err = d;
      }
    }
  }
  return globalMax(err);
}

}  // namespace

TEST_CASE("mpi: partition extents sum to global") {
  MeshPartition p(16, 12, 8, true, true, true);
  BigIndex local = static_cast<BigIndex>(p.localExtent(0)) * p.localExtent(1) *
                   p.localExtent(2);
  BigIndex global = 0;
  MPI_Allreduce(&local, &global, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
  CHECK(global == static_cast<BigIndex>(16) * 12 * 8);
  CHECK(p.localExtent(0) > 0);
  CHECK(p.localExtent(1) > 0);
  CHECK(p.localExtent(2) > 0);
  CHECK(p.localOrigin(0) >= 0);
  CHECK(p.localOrigin(0) + p.localExtent(0) <= 16);
}

TEST_CASE("mpi: degenerate 2D process grid") {
  MeshPartition p(16, 16, 1, true, true, false);
  CHECK(p.dims(2) == 1);
  CHECK(p.localExtent(2) == 1);
}

TEST_CASE("mpi: periodic halo one ghost") {
  const int nx = 16, ny = 16, nz = 16;
  MeshPartition p(nx, ny, nz, true, true, true);
  HaloExchange h(p, 1, 1);
  Field f(p, 1);
  fillValid(f, nx, ny, nz);
  h.execute(f.data.data());
  CHECK(ghostError(f, nx, ny, nz) < 1e-12);
}

TEST_CASE("mpi: periodic halo two ghosts") {
  const int nx = 16, ny = 16, nz = 16;
  MeshPartition p(nx, ny, nz, true, true, true);
  HaloExchange h(p, 2, 1);
  Field f(p, 2);
  fillValid(f, nx, ny, nz);
  h.execute(f.data.data());
  CHECK(ghostError(f, nx, ny, nz) < 1e-12);
}

TEST_CASE("mpi: thin 2D halo") {
  const int nx = 16, ny = 16, nz = 1;
  MeshPartition p(nx, ny, nz, true, true, false);
  HaloExchange h(p, 1, 1);
  Field f(p, 1);
  fillValid(f, nx, ny, nz);
  h.execute(f.data.data());
  CHECK(ghostError(f, nx, ny, nz) < 1e-12);
}

TEST_CASE("mpi: non-periodic internal halo") {
  const int nx = 16, ny = 16, nz = 16;
  MeshPartition p(nx, ny, nz, false, false, false);
  HaloExchange h(p, 2, 1);
  Field f(p, 2);
  fillValid(f, nx, ny, nz);
  h.execute(f.data.data());
  // ghostError skips physical boundaries; internal partition ghosts are
  // checked.
  CHECK(ghostError(f, nx, ny, nz) < 1e-12);
}

TEST_CASE("mpi: two components are exchanged") {
  const int nx = 8, ny = 8, nz = 8;
  MeshPartition p(nx, ny, nz, true, true, true);
  HaloExchange h(p, 1, 2);
  Field f(p, 1);
  // Reuse the scalar Field layout but allocate two components.
  f.data.assign(static_cast<std::size_t>(f.nxg) * f.nyg * f.nzg * 2,
                std::numeric_limits<double>::quiet_NaN());
  for (int comp = 0; comp < 2; ++comp)
    for (int k = p.localOrigin(2); k < p.localOrigin(2) + p.localExtent(2); ++k)
      for (int j = p.localOrigin(1); j < p.localOrigin(1) + p.localExtent(1);
           ++j)
        for (int i = p.localOrigin(0); i < p.localOrigin(0) + p.localExtent(0);
             ++i) {
          const int ii = i - (p.localOrigin(0) - 1);
          const int jj = j - (p.localOrigin(1) - 1);
          const int kk = k - (p.localOrigin(2) - 1);
          const std::size_t sc =
              static_cast<std::size_t>(f.nxg) * f.nyg * f.nzg;
          f.data[comp * sc + static_cast<std::size_t>(kk) * f.nxg * f.nyg +
                 static_cast<std::size_t>(jj) * f.nxg + ii] =
              analytic(i, j, k, nx, ny, nz) + comp;
        }
  h.execute(f.data.data());
  // Spot-check one ghost cell per component, if this rank has one.
  const int gi = p.localOrigin(0) - 1;
  const int gj = p.localOrigin(1);
  const int gk = p.localOrigin(2);
  if (gi >= 0) {
    const int ii = gi - (p.localOrigin(0) - 1);
    const int jj = gj - (p.localOrigin(1) - 1);
    const int kk = gk - (p.localOrigin(2) - 1);
    const std::size_t sc = static_cast<std::size_t>(f.nxg) * f.nyg * f.nzg;
    const std::size_t off = static_cast<std::size_t>(kk) * f.nxg * f.nyg +
                            static_cast<std::size_t>(jj) * f.nxg + ii;
    CHECK(std::abs(f.data[off] - analytic(gi, gj, gk, nx, ny, nz)) < 1e-12);
    CHECK(std::abs(f.data[sc + off] - (analytic(gi, gj, gk, nx, ny, nz) + 1)) <
          1e-12);
  }
}
