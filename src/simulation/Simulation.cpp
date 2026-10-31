#include "simulation/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include <Kokkos_Core.hpp>

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {
namespace {

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

}  // namespace

Simulation::Simulation(Solver& solver, TimeControl control, TimeScheme scheme)
    : mSolver(solver), mControl(control), mScheme(scheme) {}

void Simulation::allocateWorkspaces() {
  mOriginal = mSolver.state().clone();
  mStage1 = mSolver.state().clone();
  mStage2 = mSolver.state().clone();
  mRate = mSolver.state().clone();
}

void Simulation::rkUpdate(CellField& out, const CellField& base,
                          const CellField& stage, const CellField& rate,
                          Real dt, Real aBase, Real aStage, Real aRate) {
  const int nc = out.ncomp();
  CellField o = out;
  CellField b = base;
  CellField s = stage;
  CellField r = rate;
  Kokkos::parallel_for(
      "simulation::rk_update", ownedPolicy(out),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int c = 0; c < nc; ++c)
          o(i, j, k, c) = aBase * b(i, j, k, c) + aStage * s(i, j, k, c) +
                          aRate * dt * r(i, j, k, c);
      });
}

void Simulation::explicitStep(Real dt) {
  CellField& u = mSolver.state();
  mOriginal.copyFrom(u);

  if (mScheme == TimeScheme::ForwardEuler) {
    mSolver.rhs(u, mRate);
    rkUpdate(u, mOriginal, mOriginal, mRate, dt, 1, 0, 1);
    mSolver.applyBoundaryConditions();
    return;
  }

  // Stage 1: u1 = u0 + dt L(u0)
  mSolver.rhs(u, mRate);
  rkUpdate(mStage1, mOriginal, mOriginal, mRate, dt, 1, 0, 1);
  u.copyFrom(mStage1);
  mSolver.applyBoundaryConditions();

  // Stage 2: u2 = 3/4 u0 + 1/4 (u1 + dt L(u1))
  mSolver.rhs(u, mRate);
  if (mScheme == TimeScheme::SSPRK2) {
    // For SSPRK2 the second update is the final one:
    // u^{n+1} = 1/2 u0 + 1/2 (u1 + dt L(u1)).
    rkUpdate(u, mOriginal, mStage1, mRate, dt, Real(0.5), Real(0.5), Real(0.5));
    mSolver.applyBoundaryConditions();
    return;
  }

  rkUpdate(mStage2, mOriginal, mStage1, mRate, dt, Real(0.75), Real(0.25),
           Real(0.25));
  u.copyFrom(mStage2);
  mSolver.applyBoundaryConditions();

  // Stage 3: u^{n+1} = 1/3 u0 + 2/3 (u2 + dt L(u2))
  mSolver.rhs(u, mRate);
  rkUpdate(u, mOriginal, mStage2, mRate, dt, Real(1) / Real(3),
           Real(2) / Real(3), Real(2) / Real(3));
  mSolver.applyBoundaryConditions();
}

Diagnostics Simulation::makeDiagnostics() const {
  const CellField u = mSolver.state();
  const int nc = u.ncomp();
  double localL1 = 0;
  double localL2 = 0;
  double localLinf = 0;

  Kokkos::parallel_reduce(
      "simulation::l1", ownedPolicy(u),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int c = 0; c < nc; ++c)
          acc += Kokkos::abs(u(i, j, k, c));
      },
      localL1);
  Kokkos::parallel_reduce(
      "simulation::l2", ownedPolicy(u),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int c = 0; c < nc; ++c) {
          const Real value = u(i, j, k, c);
          acc += value * value;
        }
      },
      localL2);
  Kokkos::parallel_reduce(
      "simulation::linf", ownedPolicy(u),
      KOKKOS_LAMBDA(int i, int j, int k, double& acc) {
        for (int c = 0; c < nc; ++c)
          acc = Kokkos::max(acc, Kokkos::abs(u(i, j, k, c)));
      },
      Kokkos::Max<double>(localLinf));

  const double localCount =
      static_cast<double>(u.numOwned()) * static_cast<double>(nc);
  const double count = globalSum(localCount);

  Diagnostics out;
  out.step = mStep;
  out.time = mTime;
  out.dt = mLastDt;
  out.l1 = globalSum(localL1) / count;
  out.l2 = std::sqrt(globalSum(localL2) / count);
  out.linf = globalMax(localLinf);
  const Real h = mSolver.minCellSize();
  out.cfl = h > 0 ? mSolver.maxWaveSpeed() * mLastDt / h : 0;
  out.diffusive = h > 0 ? mSolver.maxDiffusivity() * mLastDt / (h * h) : 0;
  const Real tolerance = 1e-12 * std::max(Real(1), std::abs(mControl.tEnd));
  out.converged = std::abs(mTime - mControl.tEnd) <= tolerance;
  return out;
}

SimulationResult Simulation::run() {
  if (!isExplicit(mScheme))
    throw std::invalid_argument(
        "Simulation: implicit time scheme requires an implicit solver; "
        "use forwardEuler, ssprk2 or ssprk3 for now");

  mSolver.initialize();
  mSolver.applyInitialConditions();
  mSolver.applyBoundaryConditions();
  allocateWorkspaces();

  mTime = mControl.t0;
  mStep = 0;
  mLastDt = 0;

  SimulationResult result;
  result.history.reserve(
      static_cast<std::size_t>(std::min(mControl.maxSteps, 100000)));
  result.history.push_back(makeDiagnostics());

  const Real endTolerance = 1e-14 * std::max(Real(1), std::abs(mControl.tEnd));
  while (mTime < mControl.tEnd - endTolerance) {
    if (mStep >= mControl.maxSteps)
      break;

    Real dt = mControl.adaptive
                  ? mControl.stableDt(
                        mSolver.maxWaveSpeed(), mSolver.maxDiffusivity(),
                        mSolver.minCellSize(), mSolver.dimension())
                  : mControl.dtMax;
    dt = std::min(dt, mControl.tEnd - mTime);
    if (dt <= 0)
      break;
    if (dt < mControl.dtMin)
      throw std::runtime_error("Simulation: stable time step is below dtMin");

    mLastDt = dt;
    explicitStep(dt);
    mTime += dt;
    ++mStep;
    result.history.push_back(makeDiagnostics());
  }

  result.final = result.history.back();
  result.reachedEnd = std::abs(mTime - mControl.tEnd) <=
                      1e-12 * std::max(Real(1), std::abs(mControl.tEnd));
  result.final.converged = result.reachedEnd;
  return result;
}

}  // namespace saturn
