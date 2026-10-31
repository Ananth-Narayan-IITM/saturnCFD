#pragma once

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

  void allocateWorkspaces();
  void explicitStep(Real dt);
  void rkUpdate(CellField& out, const CellField& base, const CellField& stage,
                const CellField& rate, Real dt, Real aBase, Real aStage,
                Real aRate);
  Diagnostics makeDiagnostics() const;
};

}  // namespace saturn
