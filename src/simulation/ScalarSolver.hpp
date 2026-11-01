#pragma once

#include <functional>
#include <vector>

#include "core/Types.hpp"
#include "mesh/Mesh.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

/// One cell-centre sample of a scalar field and its analytic reference.
struct ScalarProfilePoint {
  Index cell = -1;
  Real x = 0;
  Real y = 0;
  Real z = 0;
  Real numerical = 0;
  Real exact = 0;
  Real error = 0;
};

/// Global L1/L2/Linf error against an analytic scalar solution.
struct ScalarError {
  Real l1 = 0;
  Real l2 = 0;
  Real linf = 0;
  BigIndex samples = 0;
};

/// Optional interface for scalar solvers that support CSV/VTK/checkpoint and
/// analytic-error output. `Simulation` still sees only `Solver`.
class ScalarSolver : public Solver {
 public:
  virtual bool hasExact() const = 0;
  virtual const Mesh& mesh() const = 0;

  /// Gather the owned profile on every rank, sorted by x/y/z.
  virtual std::vector<ScalarProfilePoint> profile(
      Real time, bool withExact = true) const = 0;

  virtual ScalarError exactError(Real time) const = 0;

  /// Global field in mesh cell order, for checkpoint/VTK output.
  virtual std::vector<Real> globalState() const = 0;
  virtual void setGlobalState(const std::vector<Real>& values) = 0;
};

}  // namespace saturn
