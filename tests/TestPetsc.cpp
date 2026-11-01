#include <doctest/doctest.h>

#include <cmath>
#include <vector>

#include <mpi.h>
#include <petsc.h>

#include "linalg/Petsc.hpp"

using namespace saturn;

namespace {

constexpr double kPi = 3.14159265358979323846;

MatStencil stencil(int i, int j = 0, int k = 0) {
  MatStencil s{};
  s.i = i;
  s.j = j;
  s.k = k;
  s.c = 0;
  return s;
}

double poisson1dError(int n) {
  DM da = nullptr;
  SATURN_PETSC_CHECK(
      DMDACreate1d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, n, 1, 1, nullptr, &da));
  SATURN_PETSC_CHECK(DMSetFromOptions(da));
  SATURN_PETSC_CHECK(DMSetUp(da));

  Mat A = nullptr;
  Vec b = nullptr;
  Vec x = nullptr;
  SATURN_PETSC_CHECK(DMCreateMatrix(da, &A));
  SATURN_PETSC_CHECK(DMCreateGlobalVector(da, &b));
  SATURN_PETSC_CHECK(VecDuplicate(b, &x));

  PetscInt xs = 0, xm = 0;
  SATURN_PETSC_CHECK(
      DMDAGetCorners(da, &xs, nullptr, nullptr, &xm, nullptr, nullptr));
  const double h = 1.0 / (n - 1);
  for (PetscInt i = xs; i < xs + xm; ++i) {
    const MatStencil row = stencil(i);
    if (i == 0 || i == n - 1) {
      const MatStencil col = row;
      const PetscScalar value = 1.0;
      SATURN_PETSC_CHECK(
          MatSetValuesStencil(A, 1, &row, 1, &col, &value, INSERT_VALUES));
    } else {
      MatStencil cols[3] = {stencil(i - 1), stencil(i), stencil(i + 1)};
      PetscScalar values[3] = {-1.0, 2.0, -1.0};
      SATURN_PETSC_CHECK(
          MatSetValuesStencil(A, 1, &row, 3, cols, values, INSERT_VALUES));
    }
  }
  SATURN_PETSC_CHECK(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  SATURN_PETSC_CHECK(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));

  PetscScalar* bv = nullptr;
  SATURN_PETSC_CHECK(DMDAVecGetArray(da, b, &bv));
  for (PetscInt i = xs; i < xs + xm; ++i) {
    const double xi = i * h;
    const double exact = std::sin(kPi * xi);
    if (i == 0 || i == n - 1)
      bv[i] = exact;
    else
      bv[i] = h * h * kPi * kPi * exact;
  }
  SATURN_PETSC_CHECK(DMDAVecRestoreArray(da, b, &bv));
  SATURN_PETSC_CHECK(VecAssemblyBegin(b));
  SATURN_PETSC_CHECK(VecAssemblyEnd(b));

  KspSolver solver(PETSC_COMM_WORLD, "cg", "jacobi");
  solver.solve(A, b, x);

  const PetscScalar* xv = nullptr;
  SATURN_PETSC_CHECK(DMDAVecGetArrayRead(da, x, &xv));
  double local = 0;
  for (PetscInt i = xs; i < xs + xm; ++i) {
    const double xi = i * h;
    const double error = xv[i] - std::sin(kPi * xi);
    local += error * error;
  }
  SATURN_PETSC_CHECK(DMDAVecRestoreArrayRead(da, x, &xv));

  double global = 0;
  MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  SATURN_PETSC_CHECK(VecDestroy(&x));
  SATURN_PETSC_CHECK(VecDestroy(&b));
  SATURN_PETSC_CHECK(MatDestroy(&A));
  SATURN_PETSC_CHECK(DMDestroy(&da));
  return std::sqrt(global / n);
}

double poisson3dError(int n) {
  DM da = nullptr;
  SATURN_PETSC_CHECK(DMDACreate3d(
      PETSC_COMM_WORLD, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE, DM_BOUNDARY_NONE,
      DMDA_STENCIL_STAR, n, n, n, PETSC_DECIDE, PETSC_DECIDE, PETSC_DECIDE, 1,
      1, nullptr, nullptr, nullptr, &da));
  SATURN_PETSC_CHECK(DMSetFromOptions(da));
  SATURN_PETSC_CHECK(DMSetUp(da));

  Mat A = nullptr;
  Vec b = nullptr;
  Vec x = nullptr;
  SATURN_PETSC_CHECK(DMCreateMatrix(da, &A));
  SATURN_PETSC_CHECK(DMCreateGlobalVector(da, &b));
  SATURN_PETSC_CHECK(VecDuplicate(b, &x));

  PetscInt xs = 0, ys = 0, zs = 0, xm = 0, ym = 0, zm = 0;
  SATURN_PETSC_CHECK(DMDAGetCorners(da, &xs, &ys, &zs, &xm, &ym, &zm));
  const double h = 1.0 / (n - 1);
  for (PetscInt k = zs; k < zs + zm; ++k) {
    for (PetscInt j = ys; j < ys + ym; ++j) {
      for (PetscInt i = xs; i < xs + xm; ++i) {
        const MatStencil row = stencil(i, j, k);
        if (i == 0 || i == n - 1 || j == 0 || j == n - 1 || k == 0 ||
            k == n - 1) {
          const MatStencil col = row;
          const PetscScalar value = 1.0;
          SATURN_PETSC_CHECK(
              MatSetValuesStencil(A, 1, &row, 1, &col, &value, INSERT_VALUES));
        } else {
          MatStencil cols[7] = {stencil(i, j, k),     stencil(i - 1, j, k),
                                stencil(i + 1, j, k), stencil(i, j - 1, k),
                                stencil(i, j + 1, k), stencil(i, j, k - 1),
                                stencil(i, j, k + 1)};
          PetscScalar values[7] = {6.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0};
          SATURN_PETSC_CHECK(
              MatSetValuesStencil(A, 1, &row, 7, cols, values, INSERT_VALUES));
        }
      }
    }
  }
  SATURN_PETSC_CHECK(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  SATURN_PETSC_CHECK(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));

  PetscScalar*** bv = nullptr;
  SATURN_PETSC_CHECK(DMDAVecGetArray(da, b, &bv));
  for (PetscInt k = zs; k < zs + zm; ++k) {
    for (PetscInt j = ys; j < ys + ym; ++j) {
      for (PetscInt i = xs; i < xs + xm; ++i) {
        const double xc = i * h;
        const double yc = j * h;
        const double zc = k * h;
        const double exact =
            std::sin(kPi * xc) * std::sin(kPi * yc) * std::sin(kPi * zc);
        if (i == 0 || i == n - 1 || j == 0 || j == n - 1 || k == 0 ||
            k == n - 1)
          bv[k][j][i] = exact;
        else
          bv[k][j][i] = 3 * kPi * kPi * h * h * exact;
      }
    }
  }
  SATURN_PETSC_CHECK(DMDAVecRestoreArray(da, b, &bv));
  SATURN_PETSC_CHECK(VecAssemblyBegin(b));
  SATURN_PETSC_CHECK(VecAssemblyEnd(b));

  KspSolver solver(PETSC_COMM_WORLD, "cg", "gamg");
  solver.solve(A, b, x);

  const PetscScalar*** xv = nullptr;
  SATURN_PETSC_CHECK(DMDAVecGetArrayRead(da, x, &xv));
  double local = 0;
  for (PetscInt k = zs; k < zs + zm; ++k) {
    for (PetscInt j = ys; j < ys + ym; ++j) {
      for (PetscInt i = xs; i < xs + xm; ++i) {
        const double xc = i * h;
        const double yc = j * h;
        const double zc = k * h;
        const double exact =
            std::sin(kPi * xc) * std::sin(kPi * yc) * std::sin(kPi * zc);
        const double error = xv[k][j][i] - exact;
        local += error * error;
      }
    }
  }
  SATURN_PETSC_CHECK(DMDAVecRestoreArrayRead(da, x, &xv));

  double global = 0;
  MPI_Allreduce(&local, &global, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  SATURN_PETSC_CHECK(VecDestroy(&x));
  SATURN_PETSC_CHECK(VecDestroy(&b));
  SATURN_PETSC_CHECK(MatDestroy(&A));
  SATURN_PETSC_CHECK(DMDestroy(&da));
  const double count = static_cast<double>(n) * n * n;
  return std::sqrt(global / count);
}

}  // namespace

TEST_CASE("petsc: 1D Poisson is second order") {
  const double e0 = poisson1dError(17);
  const double e1 = poisson1dError(33);
  const double e2 = poisson1dError(65);
  const double p1 = std::log(e0 / e1) / std::log(2.0);
  const double p2 = std::log(e1 / e2) / std::log(2.0);
  MESSAGE("1D Poisson errors: " << e0 << " " << e1 << " " << e2);
  MESSAGE("1D Poisson orders: " << p1 << " " << p2);
  CHECK(e2 < e1);
  CHECK(e1 < e0);
  CHECK(std::abs(p1 - 2.0) < 0.15);
  CHECK(std::abs(p2 - 2.0) < 0.15);
}

TEST_CASE("petsc: 3D Poisson solves with CG and GAMG") {
  const double error = poisson3dError(12);
  MESSAGE("3D Poisson L2 error: " << error);
  CHECK(error < 5e-2);
}

TEST_CASE("petsc: all-Neumann nullspace is attached and accepted") {
  const int n = 17;
  DM da = nullptr;
  SATURN_PETSC_CHECK(
      DMDACreate1d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, n, 1, 1, nullptr, &da));
  SATURN_PETSC_CHECK(DMSetUp(da));
  Mat A = nullptr;
  Vec b = nullptr;
  Vec x = nullptr;
  SATURN_PETSC_CHECK(DMCreateMatrix(da, &A));
  SATURN_PETSC_CHECK(DMCreateGlobalVector(da, &b));
  SATURN_PETSC_CHECK(VecDuplicate(b, &x));

  PetscInt xs = 0, xm = 0;
  SATURN_PETSC_CHECK(
      DMDAGetCorners(da, &xs, nullptr, nullptr, &xm, nullptr, nullptr));
  for (PetscInt i = xs; i < xs + xm; ++i) {
    const MatStencil row = stencil(i);
    if (i == 0) {
      MatStencil cols[2] = {stencil(i), stencil(i + 1)};
      PetscScalar values[2] = {1.0, -1.0};
      SATURN_PETSC_CHECK(
          MatSetValuesStencil(A, 1, &row, 2, cols, values, INSERT_VALUES));
    } else if (i == n - 1) {
      MatStencil cols[2] = {stencil(i - 1), stencil(i)};
      PetscScalar values[2] = {-1.0, 1.0};
      SATURN_PETSC_CHECK(
          MatSetValuesStencil(A, 1, &row, 2, cols, values, INSERT_VALUES));
    } else {
      MatStencil cols[3] = {stencil(i - 1), stencil(i), stencil(i + 1)};
      PetscScalar values[3] = {-1.0, 2.0, -1.0};
      SATURN_PETSC_CHECK(
          MatSetValuesStencil(A, 1, &row, 3, cols, values, INSERT_VALUES));
    }
  }
  SATURN_PETSC_CHECK(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  SATURN_PETSC_CHECK(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));
  SATURN_PETSC_CHECK(VecSet(b, 0.0));

  MatNullSpace nullspace = nullptr;
  SATURN_PETSC_CHECK(
      MatNullSpaceCreate(PETSC_COMM_WORLD, PETSC_TRUE, 0, nullptr, &nullspace));
  SATURN_PETSC_CHECK(MatSetNullSpace(A, nullspace));
  PetscBool isNull = PETSC_FALSE;
  SATURN_PETSC_CHECK(MatNullSpaceTest(nullspace, A, &isNull));
  CHECK(isNull == PETSC_TRUE);

  SATURN_PETSC_CHECK(VecSet(x, 1.0));
  KspSolver solver(PETSC_COMM_WORLD, "cg", "gamg");
  solver.solve(A, b, x);
  CHECK(solver.residualNorm() < 1e-10);

  SATURN_PETSC_CHECK(MatNullSpaceDestroy(&nullspace));
  SATURN_PETSC_CHECK(VecDestroy(&x));
  SATURN_PETSC_CHECK(VecDestroy(&b));
  SATURN_PETSC_CHECK(MatDestroy(&A));
  SATURN_PETSC_CHECK(DMDestroy(&da));
}

TEST_CASE("petsc: coupled saddle-point fieldsplit solves a 2-field block") {
  const int n = 8;
  DM da = nullptr;
  SATURN_PETSC_CHECK(
      DMDACreate1d(PETSC_COMM_WORLD, DM_BOUNDARY_NONE, n, 2, 1, nullptr, &da));
  SATURN_PETSC_CHECK(DMSetUp(da));

  Mat A = nullptr;
  Vec b = nullptr;
  Vec x = nullptr;
  SATURN_PETSC_CHECK(DMCreateMatrix(da, &A));
  SATURN_PETSC_CHECK(DMCreateGlobalVector(da, &b));
  SATURN_PETSC_CHECK(VecDuplicate(b, &x));

  PetscInt xs = 0, xm = 0;
  SATURN_PETSC_CHECK(
      DMDAGetCorners(da, &xs, nullptr, nullptr, &xm, nullptr, nullptr));

  // Per cell: [1 1; 1 0] [u; p] = [2; 1], exact u = p = 1.
  for (PetscInt i = xs; i < xs + xm; ++i) {
    MatStencil row{};
    row.i = i;
    row.c = 0;
    MatStencil cols[2] = {row, row};
    cols[0].c = 0;
    cols[1].c = 1;
    PetscScalar values[2] = {1.0, 1.0};
    SATURN_PETSC_CHECK(
        MatSetValuesStencil(A, 1, &row, 2, cols, values, INSERT_VALUES));

    row.c = 1;
    const MatStencil col = cols[0];
    const PetscScalar one = 1.0;
    SATURN_PETSC_CHECK(
        MatSetValuesStencil(A, 1, &row, 1, &col, &one, INSERT_VALUES));
  }
  SATURN_PETSC_CHECK(MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY));
  SATURN_PETSC_CHECK(MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY));

  PetscScalar* bv = nullptr;
  SATURN_PETSC_CHECK(VecGetArray(b, &bv));
  for (PetscInt i = xs; i < xs + xm; ++i) {
    bv[2 * (i - xs) + 0] = 2.0;
    bv[2 * (i - xs) + 1] = 1.0;
  }
  SATURN_PETSC_CHECK(VecRestoreArray(b, &bv));

  // Build the velocity/pressure field index sets on the DMDA dof layout.
  std::vector<PetscInt> velocityIdx(static_cast<std::size_t>(xm));
  std::vector<PetscInt> pressureIdx(static_cast<std::size_t>(xm));
  for (PetscInt i = 0; i < xm; ++i) {
    velocityIdx[static_cast<std::size_t>(i)] = 2 * (xs + i);
    pressureIdx[static_cast<std::size_t>(i)] = 2 * (xs + i) + 1;
  }
  IS fields[2] = {nullptr, nullptr};
  SATURN_PETSC_CHECK(ISCreateGeneral(PETSC_COMM_WORLD, xm, velocityIdx.data(),
                                     PETSC_COPY_VALUES, &fields[0]));
  SATURN_PETSC_CHECK(ISCreateGeneral(PETSC_COMM_WORLD, xm, pressureIdx.data(),
                                     PETSC_COPY_VALUES, &fields[1]));

  KSP ksp = nullptr;
  SATURN_PETSC_CHECK(KSPCreate(PETSC_COMM_WORLD, &ksp));
  SATURN_PETSC_CHECK(KSPSetType(ksp, KSPGMRES));
  PC pc = nullptr;
  SATURN_PETSC_CHECK(KSPGetPC(ksp, &pc));
  SATURN_PETSC_CHECK(PCSetType(pc, PCFIELDSPLIT));
  SATURN_PETSC_CHECK(PCFieldSplitSetType(pc, PC_COMPOSITE_SCHUR));
  SATURN_PETSC_CHECK(PCFieldSplitSetIS(pc, "velocity", fields[0]));
  SATURN_PETSC_CHECK(PCFieldSplitSetIS(pc, "pressure", fields[1]));
  SATURN_PETSC_CHECK(KSPSetOptionsPrefix(ksp, "saddle_"));
  PetscOptionsSetValue(nullptr, "-saddle_pc_type", "fieldsplit");
  PetscOptionsSetValue(nullptr, "-saddle_pc_fieldsplit_type", "schur");
  PetscOptionsSetValue(nullptr, "-saddle_pc_fieldsplit_schur_fact_type",
                       "full");
  PetscOptionsSetValue(nullptr, "-saddle_pc_fieldsplit_schur_precondition",
                       "self");
  PetscOptionsSetValue(nullptr, "-saddle_fieldsplit_velocity_ksp_type",
                       "preonly");
  PetscOptionsSetValue(nullptr, "-saddle_fieldsplit_velocity_pc_type", "lu");
  PetscOptionsSetValue(nullptr, "-saddle_fieldsplit_pressure_ksp_type",
                       "preonly");
  PetscOptionsSetValue(nullptr, "-saddle_fieldsplit_pressure_pc_type", "lu");
  SATURN_PETSC_CHECK(KSPSetOperators(ksp, A, A));
  SATURN_PETSC_CHECK(KSPSetTolerances(ksp, 1e-12, 1e-14, PETSC_DEFAULT, 100));
  SATURN_PETSC_CHECK(KSPSetFromOptions(ksp));

  SATURN_PETSC_CHECK(KSPSolve(ksp, b, x));
  KSPConvergedReason reason;
  SATURN_PETSC_CHECK(KSPGetConvergedReason(ksp, &reason));
  PetscInt its = 0;
  PetscReal rnorm = 0;
  SATURN_PETSC_CHECK(KSPGetIterationNumber(ksp, &its));
  SATURN_PETSC_CHECK(KSPGetResidualNorm(ksp, &rnorm));
  CHECK(reason > 0);
  CHECK(its <= 5);
  CHECK(rnorm < 1e-10);

  const PetscScalar* xv = nullptr;
  SATURN_PETSC_CHECK(VecGetArrayRead(x, &xv));
  for (PetscInt i = xs; i < xs + xm; ++i) {
    CHECK(xv[2 * (i - xs) + 0] == doctest::Approx(1.0).epsilon(1e-10));
    CHECK(xv[2 * (i - xs) + 1] == doctest::Approx(1.0).epsilon(1e-10));
  }
  SATURN_PETSC_CHECK(VecRestoreArrayRead(x, &xv));

  SATURN_PETSC_CHECK(KSPDestroy(&ksp));
  SATURN_PETSC_CHECK(ISDestroy(&fields[0]));
  SATURN_PETSC_CHECK(ISDestroy(&fields[1]));
  SATURN_PETSC_CHECK(VecDestroy(&x));
  SATURN_PETSC_CHECK(VecDestroy(&b));
  SATURN_PETSC_CHECK(MatDestroy(&A));
  SATURN_PETSC_CHECK(DMDestroy(&da));
}
