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
