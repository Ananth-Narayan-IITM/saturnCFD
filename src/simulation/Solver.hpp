#pragma once

#include <string>

#include "core/Types.hpp"
#include "fv/CellField.hpp"

namespace saturn {

/// Snapshot written by the time loop at one step.
struct Diagnostics {
  int step = 0;
  Real time = 0;
  Real dt = 0;
  Real l1 = 0;
  Real l2 = 0;
  Real linf = 0;
  Real cfl = 0;
  Real diffusive = 0;
  bool converged = false;
};

/// A scalar/vector PDE solver driven by the generic time loop.
///
/// The solver owns its state and exposes the explicit right-hand side
/// `du/dt = rhs(u)`. The time loop handles the stage combination and the
/// boundary refresh. Implicit schemes are represented in `TimeScheme` but are
/// enabled only when a solver provides an implicit diffusion solve.
class Solver {
 public:
  virtual ~Solver() = default;

  virtual std::string name() const = 0;

  /// Allocate fields and geometry. Called once by Simulation before the loop.
  virtual void initialize() = 0;

  /// Fill owned cells with the initial state and refresh ghosts.
  virtual void applyInitialConditions() = 0;

  /// Tell the solver the current stage time for time-dependent boundary data.
  virtual void setTime(Real) {}

  /// Refresh periodic, partition and physical ghosts after a stage update.
  virtual void applyBoundaryConditions() = 0;

  /// Explicit rate `du/dt` from the current state. Ghost values are current.
  virtual void rhs(const CellField& state, CellField& rate) = 0;

  virtual Real maxWaveSpeed() const = 0;
  virtual Real maxDiffusivity() const = 0;
  virtual Real minCellSize() const = 0;
  virtual int dimension() const = 0;

  virtual CellField& state() = 0;
  virtual const CellField& state() const = 0;
};

}  // namespace saturn
