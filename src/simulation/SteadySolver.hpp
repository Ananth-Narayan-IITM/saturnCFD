#pragma once

#include <vector>

#include "core/Types.hpp"
#include "mesh/Mesh.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

/// One cell-centre velocity/pressure sample and its analytic reference.
struct VectorProfilePoint {
  Index cell = -1;
  Real x = 0;
  Real y = 0;
  Real z = 0;
  Real u = 0;
  Real v = 0;
  Real w = 0;
  Real p = 0;
  Real uExact = 0;
  Real vExact = 0;
  Real wExact = 0;
  Real pExact = 0;
  Real error = 0;
};

/// Global error norms for a velocity-pressure solution.
struct VectorError {
  Real l1 = 0;
  Real l2 = 0;
  Real linf = 0;
  Real pL2 = 0;
  Real pLinf = 0;
  BigIndex samples = 0;
};

/// Result of a steady linear/nonlinear solve.
struct SteadyResult {
  bool converged = false;
  int iterations = 0;
  Real residual = 0;
};

/// Interface for steady solvers driven by the CLI instead of `Simulation`.
///
/// The solver still derives from `Solver` so the existing registry can create
/// it; the steady path calls `solveSteady()` and skips the time loop.
class SteadySolver : public Solver {
 public:
  virtual bool hasExact() const = 0;
  virtual const Mesh& mesh() const = 0;
  virtual SteadyResult solveSteady() = 0;

  /// Owned profile gathered on every rank, sorted by x/y/z.
  virtual std::vector<VectorProfilePoint> profile(
      bool withExact = true) const = 0;

  /// Velocity magnitude and mean-removed pressure error norms.
  virtual VectorError exactError() const = 0;
};

}  // namespace saturn
