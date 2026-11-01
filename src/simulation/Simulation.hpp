#pragma once

#include <functional>
#include <utility>
#include <vector>

#include "simulation/Solver.hpp"
#include "simulation/TimeControl.hpp"

namespace saturn {

struct SimulationResult {
  Diagnostics final;
  std::vector<Diagnostics> history;
  bool reachedEnd = false;
};

/// Generic explicit time loop. The solver owns the state and `rhs`; this class
/// owns the stage combination, stability limit and diagnostics.
class Simulation {
 public:
  Simulation(Solver& solver, TimeControl control, TimeScheme scheme);

  SimulationResult run();

  /// Called once at the initial state (step 0) and after every accepted step.
  using StepCallback = std::function<void(int step, Real time)>;
  void setStepCallback(StepCallback callback) {
    mStepCallback = std::move(callback);
  }

  /// Resume at `step` and `time`. `restore` must fill the solver state from a
  /// checkpoint; the time loop refreshes the boundary ghosts afterwards.
  void setRestart(int step, Real time, std::function<void()> restore) {
    mRestart = true;
    mRestartStep = step;
    mRestartTime = time;
    mRestore = std::move(restore);
  }

  // Public only because nvcc rejects an extended __host__ __device__ lambda
  // inside a private member function. They are internal to run().
  void rkUpdate(CellField& out, const CellField& base, const CellField& stage,
                const CellField& rate, Real dt, Real aBase, Real aStage,
                Real aRate);
  Diagnostics makeDiagnostics() const;

 private:
  Solver& mSolver;
  TimeControl mControl;
  TimeScheme mScheme;
  int mStep = 0;
  Real mTime = 0;
  Real mLastDt = 0;

  CellField mOriginal;
  CellField mStage1;
  CellField mStage2;
  CellField mRate;
  StepCallback mStepCallback;
  bool mRestart = false;
  int mRestartStep = 0;
  Real mRestartTime = 0;
  std::function<void()> mRestore;

  void allocateWorkspaces();
  void explicitStep(Real dt);
};

}  // namespace saturn
