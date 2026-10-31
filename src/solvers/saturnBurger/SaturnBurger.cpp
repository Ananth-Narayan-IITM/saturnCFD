#include "solvers/saturnBurger/SaturnBurger.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <Kokkos_Core.hpp>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

#include "fv/BoundaryFill.hpp"
#include "fv/Convection.hpp"
#include "fv/Laplacian.hpp"
#include "fv/Operators.hpp"

namespace saturn {
namespace {

using Policy3 = Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>;

Policy3 ownedPolicy(const CellField& field) {
  return Policy3(
      {field.origin(0), field.origin(1), field.origin(2)},
      {field.origin(0) + field.extent(0), field.origin(1) + field.extent(1),
       field.origin(2) + field.extent(2)});
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

}  // namespace

SaturnBurger::SaturnBurger(Mesh mesh, BurgersOptions options)
    : mMesh(std::move(mesh)),
      mOptions(std::move(options)),
      mPart(mMesh.nx, mMesh.ny, mMesh.nz, mOptions.periodic && mMesh.nx > 1,
            mOptions.periodic && mMesh.dimension >= 2 && mMesh.ny > 1, false),
      mLocal(mMesh, mPart, 2),
      mHalo(mPart, 2, 1),
      mFieldHalo(mHalo, 1),
      mU(mPart, 2, 1, "saturnBurger::u"),
      mFlux(mPart, 2, 1, "saturnBurger::flux"),
      mLapFace(mPart, 2, 1, "saturnBurger::lap_face"),
      mDiv(mPart, 2, 1, "saturnBurger::div"),
      mLap(mPart, 2, 1, "saturnBurger::lap"),
      mGrad(mPart, 2, 3, "saturnBurger::grad") {
  if (!mMesh.structured || (mMesh.dimension != 1 && mMesh.dimension != 2) ||
      (mMesh.dimension == 1 && (mMesh.ny != 1 || mMesh.nz != 1)) ||
      (mMesh.dimension == 2 && mMesh.nz != 1))
    throw std::invalid_argument(
        "saturnBurger: expected a structured 1D or 2D mesh (nz = 1)");
  if (mMesh.nx < 1 || mMesh.ny < 1 || mMesh.nz < 1)
    throw std::invalid_argument("saturnBurger: empty mesh");
  if (mOptions.nu < 0)
    throw std::invalid_argument("saturnBurger: negative diffusivity");
  if (!mOptions.initial)
    throw std::invalid_argument("saturnBurger: initial condition is missing");
  if (!mOptions.periodic && !mOptions.dirichlet)
    throw std::invalid_argument(
        "saturnBurger: non-periodic boundaries need a Dirichlet callback");
  if (mMesh.cellVolume.size() != mMesh.cells.size() ||
      mMesh.cellCentroid.size() != mMesh.cells.size())
    throw std::invalid_argument(
        "saturnBurger: computeGeometry() must run before the solver");

  Real minH = std::numeric_limits<Real>::max();
  const Real exponent = Real(1) / static_cast<Real>(mMesh.dimension);
  for (Real volume : mMesh.cellVolume)
    minH = std::min(minH, std::pow(volume, exponent));
  if (!(minH > 0))
    throw std::invalid_argument("saturnBurger: non-positive cell size");
  mMinCellSize = minH;
}

void SaturnBurger::initialize() {
  // Fields are allocated in the constructor; nothing else to build.
}

void SaturnBurger::applyInitialConditions() {
  auto host = mU.hostView();
  for (int k = mPart.localOrigin(2);
       k < mPart.localOrigin(2) + mPart.localExtent(2); ++k) {
    for (int j = mPart.localOrigin(1);
         j < mPart.localOrigin(1) + mPart.localExtent(1); ++j) {
      for (int i = mPart.localOrigin(0);
           i < mPart.localOrigin(0) + mPart.localExtent(0); ++i) {
        const int cell = mMesh.cellAt(i, j, k);
        if (cell < 0)
          throw std::runtime_error("saturnBurger: missing owned cell");
        const int ii = i - (mU.origin(0) - mU.nGrow());
        const int jj = j - (mU.origin(1) - mU.nGrow());
        const int kk = k - (mU.origin(2) - mU.nGrow());
        host(ii, jj, kk, 0) = mOptions.initial(mMesh.cellCentroid[cell]);
      }
    }
  }
  mU.syncFromHost(host);
  applyBoundaryConditions();
}

void SaturnBurger::applyBoundaryConditions() {
  mFieldHalo.execute(mU);
  if (mOptions.periodic)
    return;

  std::vector<BoundaryCondition> conditions(mMesh.patches.size());
  const Real time = mTime;
  for (std::size_t p = 0; p < conditions.size(); ++p) {
    conditions[p].type = BCType::Dirichlet;
    conditions[p].value = [this, time](const Vec3& x, const Vec3&, int c) {
      return mOptions.dirichlet(x, time, c);
    };
  }
  fillBoundaryGhosts(mU, mLocal, conditions);
}

void SaturnBurger::rhs(const CellField& state, CellField& rate) {
  if (state.ncomp() != 1)
    throw std::invalid_argument("saturnBurger: expected a scalar state");
  burgersFlux(state, mFlux, mLocal, mOptions.convection, mOptions.limiter);
  divergence(mFlux, mDiv, mLocal);
  laplacian(state, mLapFace, mGrad, mLap, mLocal);

  CellField r = rate;
  CellField div = mDiv;
  CellField lap = mLap;
  const Real nu = mOptions.nu;
  const Real source = mOptions.source;
  Kokkos::parallel_for(
      "saturnBurger::rhs", ownedPolicy(state),
      KOKKOS_LAMBDA(int i, int j, int k) {
        r(i, j, k, 0) = -div(i, j, k, 0) + nu * lap(i, j, k, 0) + source;
      });
}

std::vector<ProfilePoint> SaturnBurger::profile(Real time) const {
  if (!mOptions.exact)
    throw std::runtime_error(
        "saturnBurger: profile comparison needs an exact solution");

  const int nx = mU.extent(0);
  const int ny = mU.extent(1);
  const int nz = mU.extent(2);
  const int i0 = mU.origin(0);
  const int j0 = mU.origin(1);
  const int k0 = mU.origin(2);
  const int n = nx * ny * nz;

  // Pack x,y,z,u_numerical,u_exact per cell into one flat buffer so the
  // global profile needs a single MPI_Allgatherv.
  std::vector<double> local(static_cast<std::size_t>(5) * n);
  auto host = mU.hostView();
  const int gi0 = mU.origin(0) - mU.nGrow();
  const int gj0 = mU.origin(1) - mU.nGrow();
  const int gk0 = mU.origin(2) - mU.nGrow();
  int a = 0;
  for (int k = k0; k < k0 + nz; ++k) {
    for (int j = j0; j < j0 + ny; ++j) {
      for (int i = i0; i < i0 + nx; ++i) {
        const int cell = mMesh.cellAt(i, j, k);
        if (cell < 0)
          throw std::runtime_error("saturnBurger: missing owned cell");
        const Vec3& centre = mMesh.cellCentroid[cell];
        const std::size_t base = static_cast<std::size_t>(5) * a;
        local[base + 0] = centre.x;
        local[base + 1] = centre.y;
        local[base + 2] = centre.z;
        local[base + 3] = host(i - gi0, j - gj0, k - gk0, 0);
        local[base + 4] = mOptions.exact(centre, time);
        ++a;
      }
    }
  }

  std::vector<double> global;
#ifdef SATURN_USE_MPI
  int localCount = n;
  int worldSize = 1;
  MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
  std::vector<int> counts(static_cast<std::size_t>(worldSize));
  std::vector<int> offsets(static_cast<std::size_t>(worldSize));
  MPI_Allgather(&localCount, 1, MPI_INT, counts.data(), 1, MPI_INT,
                MPI_COMM_WORLD);
  offsets[0] = 0;
  for (int r = 1; r < worldSize; ++r)
    offsets[static_cast<std::size_t>(r)] =
        offsets[static_cast<std::size_t>(r - 1)] +
        counts[static_cast<std::size_t>(r - 1)];
  const int total = offsets.back() + counts.back();
  const int localDoubles = 5 * n;
  std::vector<int> doubleCounts(static_cast<std::size_t>(worldSize));
  std::vector<int> doubleOffsets(static_cast<std::size_t>(worldSize));
  for (int r = 0; r < worldSize; ++r) {
    doubleCounts[static_cast<std::size_t>(r)] =
        5 * counts[static_cast<std::size_t>(r)];
    doubleOffsets[static_cast<std::size_t>(r)] =
        5 * offsets[static_cast<std::size_t>(r)];
  }
  global.resize(static_cast<std::size_t>(5) * total);
  MPI_Allgatherv(local.data(), localDoubles, MPI_DOUBLE, global.data(),
                 doubleCounts.data(), doubleOffsets.data(), MPI_DOUBLE,
                 MPI_COMM_WORLD);
#else
  global = std::move(local);
#endif

  const std::size_t pointCount = global.size() / 5;
  std::vector<ProfilePoint> points(pointCount);
  for (std::size_t p = 0; p < pointCount; ++p) {
    const std::size_t base = 5 * p;
    points[p].x = global[base + 0];
    points[p].y = global[base + 1];
    points[p].z = global[base + 2];
    points[p].numerical = global[base + 3];
    points[p].exact = global[base + 4];
    points[p].error = points[p].numerical - points[p].exact;
  }
  std::sort(points.begin(), points.end(),
            [](const ProfilePoint& a, const ProfilePoint& b) {
              if (a.x != b.x)
                return a.x < b.x;
              if (a.y != b.y)
                return a.y < b.y;
              return a.z < b.z;
            });
  return points;
}

ExactError SaturnBurger::exactError(Real time) const {
  const std::vector<ProfilePoint> points = profile(time);
  ExactError out;
  out.samples = static_cast<BigIndex>(points.size());
  if (points.empty())
    return out;
  double l1 = 0;
  double l2 = 0;
  double linf = 0;
  for (const ProfilePoint& p : points) {
    const double e = static_cast<double>(p.error);
    l1 += std::abs(e);
    l2 += e * e;
    linf = std::max(linf, std::abs(e));
  }
  out.l1 = static_cast<Real>(l1 / static_cast<double>(points.size()));
  out.l2 =
      static_cast<Real>(std::sqrt(l2 / static_cast<double>(points.size())));
  out.linf = static_cast<Real>(linf);
  return out;
}

Real SaturnBurger::maxWaveSpeed() const {
  CellField u = mU;
  double local = 0;
  Kokkos::parallel_reduce(
      "saturnBurger::max_wave_speed", ownedPolicy(u),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        acc = Kokkos::max(acc, Kokkos::abs(u(i, j, k, 0)));
      },
      Kokkos::Max<double>(local));
  return static_cast<Real>(globalMax(local));
}

}  // namespace saturn
