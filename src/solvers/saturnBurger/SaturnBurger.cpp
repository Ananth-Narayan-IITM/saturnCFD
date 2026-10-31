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
      mPart(mMesh.nx, mMesh.ny, mMesh.nz, mOptions.periodic, false, false),
      mLocal(mMesh, mPart, 2),
      mHalo(mPart, 2, 1),
      mFieldHalo(mHalo, 1),
      mU(mPart, 2, 1, "saturnBurger::u"),
      mFlux(mPart, 2, 1, "saturnBurger::flux"),
      mLapFace(mPart, 2, 1, "saturnBurger::lap_face"),
      mDiv(mPart, 2, 1, "saturnBurger::div"),
      mLap(mPart, 2, 1, "saturnBurger::lap"),
      mGrad(mPart, 2, 3, "saturnBurger::grad") {
  if (!mMesh.structured || mMesh.dimension != 1 || mMesh.ny != 1 ||
      mMesh.nz != 1)
    throw std::invalid_argument(
        "saturnBurger: expected a structured 1D mesh (ny = nz = 1)");
  if (mMesh.nx < 1)
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
  for (Real volume : mMesh.cellVolume)
    minH = std::min(minH, volume);
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

  const int i0 = mPart.localOrigin(0);
  const int j0 = mPart.localOrigin(1);
  const int k0 = mPart.localOrigin(2);
  const int n = mPart.localExtent(0);

  std::vector<double> x(static_cast<std::size_t>(n));
  std::vector<double> numerical(static_cast<std::size_t>(n));
  std::vector<double> exact(static_cast<std::size_t>(n));

  auto host = mU.hostView();
  const int gi0 = mU.origin(0) - mU.nGrow();
  const int gj0 = mU.origin(1) - mU.nGrow();
  const int gk0 = mU.origin(2) - mU.nGrow();
  for (int a = 0; a < n; ++a) {
    const int i = i0 + a;
    const int cell = mMesh.cellAt(i, j0, k0);
    if (cell < 0)
      throw std::runtime_error("saturnBurger: missing owned cell");
    const Vec3& centre = mMesh.cellCentroid[cell];
    x[static_cast<std::size_t>(a)] = centre.x;
    numerical[static_cast<std::size_t>(a)] =
        host(i - gi0, j0 - gj0, k0 - gk0, 0);
    exact[static_cast<std::size_t>(a)] = mOptions.exact(centre, time);
  }

  std::vector<double> globalX;
  std::vector<double> globalNumerical;
  std::vector<double> globalExact;
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
  globalX.resize(static_cast<std::size_t>(total));
  globalNumerical.resize(static_cast<std::size_t>(total));
  globalExact.resize(static_cast<std::size_t>(total));
  MPI_Allgatherv(x.data(), localCount, MPI_DOUBLE, globalX.data(),
                 counts.data(), offsets.data(), MPI_DOUBLE, MPI_COMM_WORLD);
  MPI_Allgatherv(numerical.data(), localCount, MPI_DOUBLE,
                 globalNumerical.data(), counts.data(), offsets.data(),
                 MPI_DOUBLE, MPI_COMM_WORLD);
  MPI_Allgatherv(exact.data(), localCount, MPI_DOUBLE, globalExact.data(),
                 counts.data(), offsets.data(), MPI_DOUBLE, MPI_COMM_WORLD);
#else
  globalX = std::move(x);
  globalNumerical = std::move(numerical);
  globalExact = std::move(exact);
#endif

  std::vector<ProfilePoint> points(globalX.size());
  for (std::size_t a = 0; a < globalX.size(); ++a) {
    points[a].x = globalX[a];
    points[a].numerical = globalNumerical[a];
    points[a].exact = globalExact[a];
    points[a].error = points[a].numerical - points[a].exact;
  }
  std::sort(
      points.begin(), points.end(),
      [](const ProfilePoint& a, const ProfilePoint& b) { return a.x < b.x; });
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
