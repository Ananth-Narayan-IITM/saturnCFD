#pragma once

#include <stdexcept>
#include <string>

#include <petsc.h>

#include "core/Types.hpp"

namespace saturn {

class PetscError : public std::runtime_error {
 public:
  explicit PetscError(const std::string& message)
      : std::runtime_error(message) {}
};

void petscCheck(PetscErrorCode code, const char* expression, const char* file,
                int line);

#define SATURN_PETSC_CHECK(expression) \
  ::saturn::petscCheck((expression), #expression, __FILE__, __LINE__)

/// Initialize PETSc once and finalize it on scope exit if this scope owns it.
/// MPI must already be initialized by the caller; PETSc detects that and uses
/// the existing communicator.
class PetscScope {
 public:
  PetscScope(int& argc, char**& argv);
  ~PetscScope();

  PetscScope(const PetscScope&) = delete;
  PetscScope& operator=(const PetscScope&) = delete;

 private:
  bool mOwns = false;
};

/// Minimal RAII wrapper around a PETSc KSP solver.
class KspSolver {
 public:
  KspSolver(MPI_Comm comm, const std::string& kspType = "cg",
            const std::string& pcType = "gamg");
  ~KspSolver();

  KspSolver(const KspSolver&) = delete;
  KspSolver& operator=(const KspSolver&) = delete;

  void solve(Mat A, Vec b, Vec x);

  int iterations() const { return mIterations; }
  Real residualNorm() const { return mResidualNorm; }
  KSP handle() const { return mKsp; }

 private:
  KSP mKsp = nullptr;
  int mIterations = 0;
  Real mResidualNorm = 0;
};

}  // namespace saturn
