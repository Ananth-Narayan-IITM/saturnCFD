#include "solvers/saturnHeat/SaturnHeat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <Kokkos_Core.hpp>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

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

}  // namespace

SaturnHeat::SaturnHeat(Mesh mesh, HeatOptions options)
    : mMesh(std::move(mesh)),
      mOptions(std::move(options)),
      mPart(mMesh.nx, mMesh.ny, mMesh.nz, mOptions.periodic && mMesh.nx > 1,
            mOptions.periodic && mMesh.dimension >= 2 && mMesh.ny > 1,
            mOptions.periodic && mMesh.dimension >= 3 && mMesh.nz > 1),
      mLocal(LocalMesh::distributed(mMesh, mPart, 2)),
      mHalo(mPart, 2, 1),
      mFieldHalo(mHalo, 1),
      mT(mPart, 2, 1, "saturnHeat::T"),
      mFlux(mPart, 2, 1, "saturnHeat::flux"),
      mNormalVelocity(mPart, 2, 1, "saturnHeat::normal_velocity"),
      mLapFace(mPart, 2, 1, "saturnHeat::lap_face"),
      mDiv(mPart, 2, 1, "saturnHeat::div"),
      mLap(mPart, 2, 1, "saturnHeat::lap"),
      mGrad(mPart, 2, 3, "saturnHeat::grad") {
  if (!mMesh.structured || mMesh.dimension < 1 || mMesh.dimension > 3)
    throw std::invalid_argument(
        "saturnHeat: expected a structured 1D, 2D or 3D mesh");
  if (mMesh.dimension == 1 && (mMesh.ny != 1 || mMesh.nz != 1))
    throw std::invalid_argument("saturnHeat: 1D requires ny = nz = 1");
  if (mMesh.dimension == 2 && mMesh.nz != 1)
    throw std::invalid_argument("saturnHeat: 2D requires nz = 1");
  if (mMesh.nx < 1 || mMesh.ny < 1 || mMesh.nz < 1)
    throw std::invalid_argument("saturnHeat: empty mesh");
  if (mOptions.alpha < 0)
    throw std::invalid_argument("saturnHeat: negative diffusivity");
  if (!mOptions.initial)
    throw std::invalid_argument("saturnHeat: initial condition is missing");
  if (!mOptions.periodic && !mOptions.dirichlet)
    throw std::invalid_argument(
        "saturnHeat: non-periodic boundaries need a Dirichlet callback");

  Real minH = std::numeric_limits<Real>::max();
  const Real exponent = Real(1) / static_cast<Real>(mMesh.dimension);
  for (int k = mPart.localOrigin(2);
       k < mPart.localOrigin(2) + mPart.localExtent(2); ++k) {
    for (int j = mPart.localOrigin(1);
         j < mPart.localOrigin(1) + mPart.localExtent(1); ++j) {
      for (int i = mPart.localOrigin(0);
           i < mPart.localOrigin(0) + mPart.localExtent(0); ++i)
        minH = std::min(minH, std::pow(mLocal.cellVolume(i, j, k), exponent));
    }
  }
  if (!(minH > 0))
    throw std::invalid_argument("saturnHeat: non-positive cell size");
  mMinCellSize = minH;
}

void SaturnHeat::initialize() {
  FaceField un = mNormalVelocity;
  const LocalMesh::DeviceData mesh = mLocal.device();
  const Vec3 velocity = mOptions.velocity;
  const int i0 = mesh.origin[0];
  const int j0 = mesh.origin[1];
  const int k0 = mesh.origin[2];
  Kokkos::parallel_for(
      "saturnHeat::velocity_faces",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0},
          {i0 + mesh.extent[0], j0 + mesh.extent[1], k0 + mesh.extent[2]}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int dir = 0; dir < 2 * mesh.dimension; ++dir)
          un(i, j, k, dir, 0) = dot(velocity, mesh.faceNormal(i, j, k, dir));
      });
}

void SaturnHeat::applyInitialConditions() {
  auto host = mT.hostView();
  for (int k = mPart.localOrigin(2);
       k < mPart.localOrigin(2) + mPart.localExtent(2); ++k) {
    for (int j = mPart.localOrigin(1);
         j < mPart.localOrigin(1) + mPart.localExtent(1); ++j) {
      for (int i = mPart.localOrigin(0);
           i < mPart.localOrigin(0) + mPart.localExtent(0); ++i) {
        const int ii = i - (mT.origin(0) - mT.nGrow());
        const int jj = j - (mT.origin(1) - mT.nGrow());
        const int kk = k - (mT.origin(2) - mT.nGrow());
        host(ii, jj, kk, 0) = mOptions.initial(mLocal.cellCentroid(i, j, k));
      }
    }
  }
  mT.syncFromHost(host);
  applyBoundaryConditions();
}

void SaturnHeat::applyBoundaryConditions() {
  mFieldHalo.execute(mT);
  if (mOptions.periodic)
    return;

  std::vector<BoundaryCondition> conditions(mLocal.numPatches());
  const Real time = mTime;
  for (std::size_t p = 0; p < conditions.size(); ++p) {
    conditions[p].type = BCType::Dirichlet;
    conditions[p].value = [this, time](const Vec3& x, const Vec3&, int c) {
      return mOptions.dirichlet(x, time, c);
    };
  }
  fillBoundaryGhosts(mT, mLocal, conditions);
}

void SaturnHeat::rhs(const CellField& state, CellField& rate) {
  if (state.ncomp() != 1)
    throw std::invalid_argument("saturnHeat: expected a scalar state");
  convectiveFlux(mNormalVelocity, state, mFlux, mLocal, mOptions.convection,
                 mOptions.limiter);
  divergence(mFlux, mDiv, mLocal);
  laplacian(state, mLapFace, mGrad, mLap, mLocal);

  CellField r = rate;
  CellField div = mDiv;
  CellField lap = mLap;
  const Real alpha = mOptions.alpha;
  const Real source = mOptions.source;
  Kokkos::parallel_for(
      "saturnHeat::rhs", ownedPolicy(state),
      KOKKOS_LAMBDA(int i, int j, int k) {
        r(i, j, k, 0) = -div(i, j, k, 0) + alpha * lap(i, j, k, 0) + source;
      });
}

Real SaturnHeat::maxWaveSpeed() const {
  const Vec3 v = mOptions.velocity;
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

namespace {

/// Shared packed profile for scalar solvers.
template <class FieldAccess>
std::vector<ScalarProfilePoint> gatherProfile(
    const LocalMesh& local, const MeshPartition& part, int i0, int j0, int k0,
    int nx, int ny, int nz, FieldAccess value, Real time,
    const std::function<Real(const Vec3&, Real)>& exact, bool withExact,
    BigIndex globalCellCount) {
  const int n = nx * ny * nz;
  std::vector<double> packed(static_cast<std::size_t>(6) * n);
  int a = 0;
  for (int k = k0; k < k0 + nz; ++k) {
    for (int j = j0; j < j0 + ny; ++j) {
      for (int i = i0; i < i0 + nx; ++i) {
        const int cell = local.cellId(i, j, k);
        if (cell < 0)
          throw std::runtime_error("saturnHeat: missing owned cell");
        const Vec3 centre = local.cellCentroid(i, j, k);
        const std::size_t base = static_cast<std::size_t>(6) * a;
        packed[base + 0] = centre.x;
        packed[base + 1] = centre.y;
        packed[base + 2] = centre.z;
        packed[base + 3] = value(i, j, k);
        packed[base + 4] = withExact ? exact(centre, time) : 0.0;
        packed[base + 5] = static_cast<double>(cell);
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
  std::vector<int> doubleCounts(static_cast<std::size_t>(worldSize));
  std::vector<int> doubleOffsets(static_cast<std::size_t>(worldSize));
  for (int r = 0; r < worldSize; ++r) {
    doubleCounts[static_cast<std::size_t>(r)] =
        6 * counts[static_cast<std::size_t>(r)];
    doubleOffsets[static_cast<std::size_t>(r)] =
        6 * offsets[static_cast<std::size_t>(r)];
  }
  global.resize(static_cast<std::size_t>(6) * total);
  MPI_Allgatherv(packed.data(), 6 * n, MPI_DOUBLE, global.data(),
                 doubleCounts.data(), doubleOffsets.data(), MPI_DOUBLE,
                 MPI_COMM_WORLD);
#else
  global = std::move(packed);
#endif

  const std::size_t pointCount = global.size() / 6;
  std::vector<ScalarProfilePoint> points(pointCount);
  for (std::size_t p = 0; p < pointCount; ++p) {
    const std::size_t base = 6 * p;
    points[p].cell = static_cast<Index>(global[base + 5]);
    points[p].x = global[base + 0];
    points[p].y = global[base + 1];
    points[p].z = global[base + 2];
    points[p].numerical = global[base + 3];
    points[p].exact = global[base + 4];
    points[p].error = points[p].numerical - points[p].exact;
  }
  std::sort(points.begin(), points.end(),
            [](const ScalarProfilePoint& a, const ScalarProfilePoint& b) {
              if (a.x != b.x)
                return a.x < b.x;
              if (a.y != b.y)
                return a.y < b.y;
              return a.z < b.z;
            });
  (void)part;
  (void)globalCellCount;
  return points;
}

}  // namespace

std::vector<ScalarProfilePoint> SaturnHeat::profile(Real time,
                                                    bool withExact) const {
  if (withExact && !mOptions.exact)
    throw std::runtime_error(
        "saturnHeat: profile comparison needs an exact solution");
  auto host = mT.hostView();
  const int gi0 = mT.origin(0) - mT.nGrow();
  const int gj0 = mT.origin(1) - mT.nGrow();
  const int gk0 = mT.origin(2) - mT.nGrow();
  auto value = [&](int i, int j, int k) {
    return host(i - gi0, j - gj0, k - gk0, 0);
  };
  return gatherProfile(mLocal, mPart, mPart.localOrigin(0),
                       mPart.localOrigin(1), mPart.localOrigin(2),
                       mPart.localExtent(0), mPart.localExtent(1),
                       mPart.localExtent(2), value, time, mOptions.exact,
                       withExact, mLocal.globalCellCount());
}

ScalarError SaturnHeat::exactError(Real time) const {
  const std::vector<ScalarProfilePoint> points = profile(time);
  ScalarError out;
  out.samples = static_cast<BigIndex>(points.size());
  if (points.empty())
    return out;
  double l1 = 0;
  double l2 = 0;
  double linf = 0;
  for (const ScalarProfilePoint& p : points) {
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

std::vector<Real> SaturnHeat::globalState() const {
  std::vector<Real> values(static_cast<std::size_t>(mLocal.globalCellCount()),
                           0);
  for (const ScalarProfilePoint& point : profile(0, false)) {
    if (point.cell >= 0 && static_cast<std::size_t>(point.cell) < values.size())
      values[static_cast<std::size_t>(point.cell)] = point.numerical;
  }
  return values;
}

void SaturnHeat::setGlobalState(const std::vector<Real>& values) {
  if (values.size() != static_cast<std::size_t>(mLocal.globalCellCount()))
    throw std::invalid_argument(
        "saturnHeat: checkpoint field size does not match the mesh");
  auto host = mT.hostView();
  const int gi0 = mT.origin(0) - mT.nGrow();
  const int gj0 = mT.origin(1) - mT.nGrow();
  const int gk0 = mT.origin(2) - mT.nGrow();
  for (int k = mPart.localOrigin(2);
       k < mPart.localOrigin(2) + mPart.localExtent(2); ++k) {
    for (int j = mPart.localOrigin(1);
         j < mPart.localOrigin(1) + mPart.localExtent(1); ++j) {
      for (int i = mPart.localOrigin(0);
           i < mPart.localOrigin(0) + mPart.localExtent(0); ++i) {
        const int cell = mLocal.cellId(i, j, k);
        if (cell < 0)
          throw std::runtime_error("saturnHeat: missing owned cell");
        host(i - gi0, j - gj0, k - gk0, 0) =
            values[static_cast<std::size_t>(cell)];
      }
    }
  }
  mT.syncFromHost(host);
}

}  // namespace saturn
