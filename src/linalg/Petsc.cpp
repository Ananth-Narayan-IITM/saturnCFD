#include "linalg/Petsc.hpp"

#include <sstream>

namespace saturn {

void petscCheck(PetscErrorCode code, const char* expression, const char* file,
                int line) {
  if (code == PETSC_SUCCESS)
    return;
  std::ostringstream message;
  message << "PETSc error " << code << " in " << expression << " at " << file
          << ":" << line;
  throw PetscError(message.str());
}

PetscScope::PetscScope(int& argc, char**& argv) {
  if (!PetscInitializeCalled) {
    SATURN_PETSC_CHECK(PetscInitialize(&argc, &argv, nullptr, nullptr));
    mOwns = true;
  }
}

PetscScope::~PetscScope() {
  if (mOwns && !PetscFinalizeCalled)
    PetscFinalize();
}

KspSolver::KspSolver(MPI_Comm comm, const std::string& kspType,
                     const std::string& pcType) {
  SATURN_PETSC_CHECK(KSPCreate(comm, &mKsp));
  SATURN_PETSC_CHECK(KSPSetType(mKsp, kspType.c_str()));
  PC pc = nullptr;
  SATURN_PETSC_CHECK(KSPGetPC(mKsp, &pc));
  SATURN_PETSC_CHECK(PCSetType(pc, pcType.c_str()));
  SATURN_PETSC_CHECK(KSPSetFromOptions(mKsp));
}

KspSolver::~KspSolver() {
  if (mKsp != nullptr)
    KSPDestroy(&mKsp);
}

void KspSolver::solve(Mat A, Vec b, Vec x) {
  SATURN_PETSC_CHECK(KSPSetOperators(mKsp, A, A));
  SATURN_PETSC_CHECK(KSPSolve(mKsp, b, x));

  KSPConvergedReason reason;
  SATURN_PETSC_CHECK(KSPGetConvergedReason(mKsp, &reason));
  if (reason < 0) {
    std::ostringstream message;
    message << "KSP did not converge, reason=" << static_cast<int>(reason);
    throw PetscError(message.str());
  }

  PetscInt iterations = 0;
  PetscReal residual = 0;
  SATURN_PETSC_CHECK(KSPGetIterationNumber(mKsp, &iterations));
  SATURN_PETSC_CHECK(KSPGetResidualNorm(mKsp, &residual));
  mIterations = static_cast<int>(iterations);
  mResidualNorm = static_cast<Real>(residual);
}

}  // namespace saturn
