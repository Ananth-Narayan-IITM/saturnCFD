#include "solvers/saturnStokes/SaturnStokes.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "linalg/Petsc.hpp"

#ifdef SATURN_USE_MPI
#include <mpi.h>
#endif

namespace saturn {
namespace {

constexpr PetscInt kDof = 4;  // u, v, w, p

MatStencil stencil(PetscInt i, PetscInt j, PetscInt k, PetscInt c) {
  MatStencil s{};
  s.i = i;
  s.j = j;
  s.k = k;
  s.c = c;
  return s;
}

}  // namespace

SaturnStokes::SaturnStokes(Mesh mesh, StokesOptions options)
    : mMesh(std::move(mesh)),
      mOptions(std::move(options)),
      mPart(mMesh.nx, mMesh.ny, mMesh.dimension == 3 ? mMesh.nz : 1,
            mOptions.periodicX && mMesh.nx > 1,
            mOptions.periodicY && mMesh.ny > 1,
            mOptions.periodicZ && mMesh.dimension == 3 && mMesh.nz > 1),
      mState(mPart, 0, 1, "saturnStokes::dummy") {
  if (!mMesh.structured || (mMesh.dimension != 2 && mMesh.dimension != 3))
    throw std::invalid_argument(
        "saturnStokes: expected a structured 2D or 3D box mesh");
  if (mMesh.nx < 1 || mMesh.ny < 1 || (mMesh.dimension == 3 && mMesh.nz < 1))
    throw std::invalid_argument("saturnStokes: empty mesh");
  if (!(mOptions.mu > 0))
    throw std::invalid_argument("saturnStokes: mu must be positive");
  if (mOptions.stabilization < 0)
    throw std::invalid_argument("saturnStokes: negative stabilization");
  if (!mOptions.periodicX || (mMesh.dimension == 3 && !mOptions.periodicZ))
    throw std::invalid_argument(
        "saturnStokes: the current core requires periodic x and z boundaries; "
        "y may be walls or periodic");

  mDim = mMesh.dimension;
  mNx = mMesh.nx;
  mNy = mMesh.ny;
  mNz = (mDim == 3) ? mMesh.nz : 1;
  mXmin = mMesh.bboxMin.x;
  mYmin = mMesh.bboxMin.y;
  mZmin = mMesh.bboxMin.z;
  mHx = (mMesh.bboxMax.x - mMesh.bboxMin.x) / static_cast<Real>(mNx);
  mHy = (mMesh.bboxMax.y - mMesh.bboxMin.y) / static_cast<Real>(mNy);
  mHz = (mDim == 3)
            ? (mMesh.bboxMax.z - mMesh.bboxMin.z) / static_cast<Real>(mNz)
            : 1.0;
  if (!(mHx > 0) || !(mHy > 0) || !(mHz > 0))
    throw std::invalid_argument("saturnStokes: non-positive cell size");
}

SaturnStokes::~SaturnStokes() {
  destroyPetsc();
}

void SaturnStokes::destroyPetsc() {
  if (mKsp != nullptr)
    KSPDestroy(&mKsp);
  if (mVelocityIS != nullptr)
    ISDestroy(&mVelocityIS);
  if (mPressureIS != nullptr)
    ISDestroy(&mPressureIS);
  if (mNullspace != nullptr)
    MatNullSpaceDestroy(&mNullspace);
  if (mX != nullptr)
    VecDestroy(&mX);
  if (mB != nullptr)
    VecDestroy(&mB);
  if (mA != nullptr)
    MatDestroy(&mA);
  if (mDa != nullptr)
    DMDestroy(&mDa);
}

void SaturnStokes::initialize() {
  if (mDa != nullptr)
    return;
  createDmda();
  createSolver();
  VecSet(mX, 0.0);
  mAssembled = false;
  mSolved = false;
}

void SaturnStokes::applyInitialConditions() {
  if (mX != nullptr)
    VecSet(mX, 0.0);
}

void SaturnStokes::createDmda() {
  const DMBoundaryType bx =
      (mOptions.periodicX && mNx > 1) ? DM_BOUNDARY_PERIODIC : DM_BOUNDARY_NONE;
  const DMBoundaryType by =
      (mOptions.periodicY && mNy > 1) ? DM_BOUNDARY_PERIODIC : DM_BOUNDARY_NONE;
  const DMBoundaryType bz = (mDim == 3 && mOptions.periodicZ && mNz > 1)
                                ? DM_BOUNDARY_PERIODIC
                                : DM_BOUNDARY_NONE;

  SATURN_PETSC_CHECK(DMDACreate3d(PETSC_COMM_WORLD, bx, by, bz,
                                  DMDA_STENCIL_STAR, mNx, mNy, mNz,
                                  PETSC_DECIDE, PETSC_DECIDE, PETSC_DECIDE,
                                  kDof, 1, nullptr, nullptr, nullptr, &mDa));
  SATURN_PETSC_CHECK(DMSetUp(mDa));
  {
    PetscInt dim = 0, M = 0, N = 0, P = 0, m = 0, n = 0, p = 0, dof = 0, sw = 0;
    DMBoundaryType bbx = DM_BOUNDARY_NONE, bby = DM_BOUNDARY_NONE,
                   bbz = DM_BOUNDARY_NONE;
    DMDAStencilType st = DMDA_STENCIL_STAR;
    SATURN_PETSC_CHECK(DMDAGetInfo(mDa, &dim, &M, &N, &P, &m, &n, &p, &dof, &sw,
                                   &bbx, &bby, &bbz, &st));
    int rank = 0;
    MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
    PetscSynchronizedPrintf(PETSC_COMM_WORLD,
                            "rank %d DMDA global %d %d %d local %d %d %d "
                            "dof %d sw %d\n",
                            rank, (int)M, (int)N, (int)P, (int)m, (int)n,
                            (int)p, (int)dof, (int)sw);
    PetscSynchronizedFlush(PETSC_COMM_WORLD, PETSC_STDOUT);
  }
  SATURN_PETSC_CHECK(DMDASetUniformCoordinates(
      mDa, mXmin, mMesh.bboxMax.x, mYmin, mMesh.bboxMax.y, mZmin, mZmin + mHz));

  SATURN_PETSC_CHECK(DMCreateMatrix(mDa, &mA));
  SATURN_PETSC_CHECK(DMCreateGlobalVector(mDa, &mB));
  SATURN_PETSC_CHECK(VecDuplicate(mB, &mX));
  SATURN_PETSC_CHECK(
      MatSetOption(mA, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_FALSE));
}

void SaturnStokes::createSolver() {
  PetscInt xs = 0, ys = 0, zs = 0, xm = 0, ym = 0, zm = 0;
  SATURN_PETSC_CHECK(DMDAGetCorners(mDa, &xs, &ys, &zs, &xm, &ym, &zm));
  const PetscInt nodes = xm * ym * zm;
  // DMCreateMatrix uses a block size of kDof and assigns each rank a
  // contiguous row range, so local node q maps to global node
  // firstNode + q. Building ids from the natural i + nx*(j + ny*k)
  // formula is wrong on y/z-split layouts.
  PetscInt rowStart = 0, rowEnd = 0, blockSize = 0;
  SATURN_PETSC_CHECK(MatGetOwnershipRange(mA, &rowStart, &rowEnd));
  SATURN_PETSC_CHECK(MatGetBlockSize(mA, &blockSize));
  if (blockSize != kDof || (rowEnd - rowStart) / kDof != nodes)
    throw PetscError("saturnStokes: unexpected DMDA matrix ownership layout");
  const PetscInt firstNode = rowStart / kDof;
  std::vector<PetscInt> velocity;
  std::vector<PetscInt> pressure;
  velocity.reserve(static_cast<std::size_t>(nodes) * 3);
  pressure.reserve(static_cast<std::size_t>(nodes));
  for (PetscInt k = zs; k < zs + zm; ++k) {
    for (PetscInt j = ys; j < ys + ym; ++j) {
      for (PetscInt i = xs; i < xs + xm; ++i) {
        const PetscInt q = (i - xs) + xm * ((j - ys) + ym * (k - zs));
        const PetscInt node = firstNode + q;
        velocity.push_back(kDof * node + 0);
        velocity.push_back(kDof * node + 1);
        velocity.push_back(kDof * node + 2);
        pressure.push_back(kDof * node + 3);
      }
    }
  }
  SATURN_PETSC_CHECK(
      ISCreateGeneral(PETSC_COMM_WORLD, static_cast<PetscInt>(velocity.size()),
                      velocity.data(), PETSC_COPY_VALUES, &mVelocityIS));
  SATURN_PETSC_CHECK(
      ISCreateGeneral(PETSC_COMM_WORLD, static_cast<PetscInt>(pressure.size()),
                      pressure.data(), PETSC_COPY_VALUES, &mPressureIS));

  SATURN_PETSC_CHECK(KSPCreate(PETSC_COMM_WORLD, &mKsp));
  if (mOptions.direct) {
    SATURN_PETSC_CHECK(KSPSetType(mKsp, KSPPREONLY));
    PC directPc = nullptr;
    SATURN_PETSC_CHECK(KSPGetPC(mKsp, &directPc));
    SATURN_PETSC_CHECK(PCSetType(directPc, PCLU));
    // SuperLU_DIST is more robust than MUMPS on the small, unsymmetric,
    // multi-periodic saddle-point matrices that appear in the refinement
    // tests; MUMPS can report a false zero pivot there. Fall back to the
    // PETSc default when SuperLU_DIST is not part of the build.
#if defined(PETSC_HAVE_SUPERLU_DIST)
    SATURN_PETSC_CHECK(
        PCFactorSetMatSolverType(directPc, MATSOLVERSUPERLU_DIST));
#endif
    SATURN_PETSC_CHECK(KSPSetOperators(mKsp, mA, mA));
    SATURN_PETSC_CHECK(KSPSetTolerances(mKsp, 1e-12, 1e-14, PETSC_DEFAULT, 50));
    return;
  }
  SATURN_PETSC_CHECK(KSPSetType(mKsp, KSPFGMRES));
  PC pc = nullptr;
  SATURN_PETSC_CHECK(KSPGetPC(mKsp, &pc));
  SATURN_PETSC_CHECK(PCSetType(pc, PCFIELDSPLIT));
  SATURN_PETSC_CHECK(PCFieldSplitSetType(pc, PC_COMPOSITE_SCHUR));
  SATURN_PETSC_CHECK(
      PCFieldSplitSetSchurFactType(pc, PC_FIELDSPLIT_SCHUR_FACT_FULL));
  SATURN_PETSC_CHECK(PCFieldSplitSetIS(pc, "velocity", mVelocityIS));
  SATURN_PETSC_CHECK(PCFieldSplitSetIS(pc, "pressure", mPressureIS));

  SATURN_PETSC_CHECK(KSPSetOptionsPrefix(mKsp, "stokes_"));
  // The velocity block is 3x3 per cell and the pressure block is 1x1, so the
  // a11 Schur preconditioner has the wrong size. The pressure stabilisation
  // gives a genuine pressure Laplacian; use it (selfp) instead.
  PetscOptionsSetValue(nullptr, "-stokes_pc_fieldsplit_schur_precondition",
                       "selfp");
  PetscOptionsSetValue(nullptr, "-stokes_fieldsplit_velocity_ksp_type",
                       "preonly");
  PetscOptionsSetValue(nullptr, "-stokes_fieldsplit_velocity_pc_type", "ilu");
  PetscOptionsSetValue(nullptr, "-stokes_fieldsplit_pressure_ksp_type",
                       "preonly");
  PetscOptionsSetValue(nullptr, "-stokes_fieldsplit_pressure_pc_type", "ilu");
  SATURN_PETSC_CHECK(KSPSetTolerances(mKsp, 1e-10, 1e-12, PETSC_DEFAULT, 500));
  SATURN_PETSC_CHECK(KSPSetFromOptions(mKsp));

  SATURN_PETSC_CHECK(KSPSetOperators(mKsp, mA, mA));
}

void SaturnStokes::assemble() {
  if (mDa == nullptr)
    initialize();
  SATURN_PETSC_CHECK(MatZeroEntries(mA));
  SATURN_PETSC_CHECK(VecSet(mB, 0.0));
  SATURN_PETSC_CHECK(KSPSetOperators(mKsp, mA, mA));

  PetscInt xs = 0, ys = 0, zs = 0, xm = 0, ym = 0, zm = 0;
  SATURN_PETSC_CHECK(DMDAGetCorners(mDa, &xs, &ys, &zs, &xm, &ym, &zm));

  PetscScalar**** bv = nullptr;
  SATURN_PETSC_CHECK(DMDAVecGetArrayDOF(mDa, mB, &bv));

  // Every equation is assembled per unit volume. The momentum diffusion
  // coefficient is mu/h^2, the pressure gradient is centred on 0.5/h, and the
  // divergence is centred on 0.5/h, so the discrete gradient is the negative
  // transpose of the discrete divergence and the pressure is physical.
  const Real cx = mOptions.mu / (mHx * mHx);
  const Real cy = mOptions.mu / (mHy * mHy);
  const Real cz = (mDim == 3) ? mOptions.mu / (mHz * mHz) : Real(0);
  // beta = stabilization * h^2 / mu is the physical Brezzi-Pitkaranta
  // coefficient; per unit volume the discrete Laplacian uses beta/h^2.
  const Real hMin =
      (mDim == 3) ? std::min(mHx, std::min(mHy, mHz)) : std::min(mHx, mHy);
  const Real beta = mOptions.stabilization * hMin * hMin / mOptions.mu;
  const Real betaX = beta / (mHx * mHx);
  const Real betaY = beta / (mHy * mHy);
  const Real betaZ = (mDim == 3) ? beta / (mHz * mHz) : Real(0);
  const bool periodicX = mOptions.periodicX && mNx > 1;
  const bool periodicY = mOptions.periodicY && mNy > 1;
  const bool periodicZ = (mDim == 3) && mOptions.periodicZ && mNz > 1;

  // MatSetValuesStencil applies the DMDA periodic wrap itself, so keep the
  // logical neighbour index unwrapped (i-1 and i+1 may fall outside [0,M)).
  auto px = [&](int i) { return i; };
  auto py = [&](int j) { return j; };
  auto pz = [&](int k) { return k; };

  for (PetscInt k = zs; k < zs + zm; ++k) {
    for (PetscInt j = ys; j < ys + ym; ++j) {
      for (PetscInt i = xs; i < xs + xm; ++i) {
        const Real xc = mXmin + (static_cast<Real>(i) + Real(0.5)) * mHx;
        const Real yc = mYmin + (static_cast<Real>(j) + Real(0.5)) * mHy;
        const Real zc = mZmin + (static_cast<Real>(k) + Real(0.5)) * mHz;
        const Vec3 f = mOptions.bodyForce ? mOptions.bodyForce({xc, yc, zc})
                                          : mOptions.force;
        const bool bottom = (!periodicY && j == 0);
        const bool top = (!periodicY && j == mNy - 1);

        auto addMomentum = [&](PetscInt c, const std::vector<MatStencil>& cols,
                               const std::vector<PetscScalar>& vals) {
          if (cols.empty())
            return;
          // MatSetValuesStencil maps the global logical (i,j,k,c) indices
          // through the DMDA local-to-global map. Hand-building PETSc global
          // row ids from i,j,k is only correct when the DA is distributed
          // along x, which is why it broke on y-split 3D layouts.
          MatStencil row = stencil(i, j, k, c);
          SATURN_PETSC_CHECK(MatSetValuesStencil(
              mA, 1, &row, static_cast<PetscInt>(cols.size()), cols.data(),
              vals.data(), ADD_VALUES));
        };

        // ---------------- u momentum ----------------
        {
          std::vector<MatStencil> cols;
          std::vector<PetscScalar> vals;
          Real diag = 0;
          auto add = [&](int ii, int jj, int kk, int c, Real v) {
            cols.push_back(stencil(ii, jj, kk, c));
            vals.push_back(v);
          };
          // x diffusion (periodic or internal)
          if (periodicX || i > 0) {
            add(px(i - 1), j, k, 0, -cx);
            diag += cx;
          }
          if (periodicX || i + 1 < mNx) {
            add(px(i + 1), j, k, 0, -cx);
            diag += cx;
          }
          // y diffusion / walls
          if (bottom) {
            diag += 2 * cy;
            bv[k][j][i][0] += 2 * cy * mOptions.wallBottomU;
            if (j + 1 < mNy) {
              add(i, j + 1, k, 0, -cy);
              diag += cy;
            }
          } else if (top) {
            diag += 2 * cy;
            bv[k][j][i][0] += 2 * cy * mOptions.wallTopU;
            if (j - 1 >= 0) {
              add(i, j - 1, k, 0, -cy);
              diag += cy;
            }
          } else {
            add(i, py(j - 1), k, 0, -cy);
            add(i, py(j + 1), k, 0, -cy);
            diag += 2 * cy;
          }
          // z diffusion
          if (mDim == 3) {
            if (periodicZ || k > 0) {
              add(i, j, pz(k - 1), 0, -cz);
              diag += cz;
            }
            if (periodicZ || k + 1 < mNz) {
              add(i, j, pz(k + 1), 0, -cz);
              diag += cz;
            }
          }
          // pressure gradient x
          if (periodicX || i > 0)
            add(px(i - 1), j, k, 3, -Real(0.5) / mHx);
          if (periodicX || i + 1 < mNx)
            add(px(i + 1), j, k, 3, Real(0.5) / mHx);
          add(i, j, k, 0, diag);
          bv[k][j][i][0] += f.x;
          addMomentum(0, cols, vals);
        }

        // ---------------- v momentum ----------------
        {
          std::vector<MatStencil> cols;
          std::vector<PetscScalar> vals;
          Real diag = 0;
          auto add = [&](int ii, int jj, int kk, int c, Real v) {
            cols.push_back(stencil(ii, jj, kk, c));
            vals.push_back(v);
          };
          if (periodicX || i > 0) {
            add(px(i - 1), j, k, 1, -cx);
            diag += cx;
          }
          if (periodicX || i + 1 < mNx) {
            add(px(i + 1), j, k, 1, -cx);
            diag += cx;
          }
          if (bottom) {
            diag += 2 * cy;
            if (j + 1 < mNy) {
              add(i, j + 1, k, 1, -cy);
              diag += cy;
            }
          } else if (top) {
            diag += 2 * cy;
            if (j - 1 >= 0) {
              add(i, j - 1, k, 1, -cy);
              diag += cy;
            }
          } else {
            add(i, py(j - 1), k, 1, -cy);
            add(i, py(j + 1), k, 1, -cy);
            diag += 2 * cy;
          }
          if (mDim == 3) {
            if (periodicZ || k > 0) {
              add(i, j, pz(k - 1), 1, -cz);
              diag += cz;
            }
            if (periodicZ || k + 1 < mNz) {
              add(i, j, pz(k + 1), 1, -cz);
              diag += cz;
            }
          }
          // pressure gradient y
          if (bottom) {
            add(i, j, k, 3, -Real(0.5) / mHy);
            add(i, j + 1, k, 3, Real(0.5) / mHy);
          } else if (top) {
            add(i, j - 1, k, 3, -Real(0.5) / mHy);
            add(i, j, k, 3, Real(0.5) / mHy);
          } else {
            add(i, py(j - 1), k, 3, -Real(0.5) / mHy);
            add(i, py(j + 1), k, 3, Real(0.5) / mHy);
          }
          add(i, j, k, 1, diag);
          bv[k][j][i][1] += f.y;
          addMomentum(1, cols, vals);
        }

        // ---------------- w momentum ----------------
        if (mDim == 3) {
          std::vector<MatStencil> cols;
          std::vector<PetscScalar> vals;
          Real diag = 0;
          auto add = [&](int ii, int jj, int kk, int c, Real v) {
            cols.push_back(stencil(ii, jj, kk, c));
            vals.push_back(v);
          };
          if (periodicX || i > 0) {
            add(px(i - 1), j, k, 2, -cx);
            diag += cx;
          }
          if (periodicX || i + 1 < mNx) {
            add(px(i + 1), j, k, 2, -cx);
            diag += cx;
          }
          if (bottom) {
            diag += 2 * cy;
            if (j + 1 < mNy) {
              add(i, j + 1, k, 2, -cy);
              diag += cy;
            }
          } else if (top) {
            diag += 2 * cy;
            if (j - 1 >= 0) {
              add(i, j - 1, k, 2, -cy);
              diag += cy;
            }
          } else {
            add(i, py(j - 1), k, 2, -cy);
            add(i, py(j + 1), k, 2, -cy);
            diag += 2 * cy;
          }
          if (periodicZ || k > 0) {
            add(i, j, pz(k - 1), 2, -cz);
            diag += cz;
          }
          if (periodicZ || k + 1 < mNz) {
            add(i, j, pz(k + 1), 2, -cz);
            diag += cz;
          }
          // pressure gradient z
          if (periodicZ || k > 0)
            add(i, j, pz(k - 1), 3, -Real(0.5) / mHz);
          if (periodicZ || k + 1 < mNz)
            add(i, j, pz(k + 1), 3, Real(0.5) / mHz);
          add(i, j, k, 2, diag);
          bv[k][j][i][2] += f.z;
          addMomentum(2, cols, vals);
        } else {
          std::vector<MatStencil> cols{stencil(i, j, k, 2)};
          std::vector<PetscScalar> vals{1.0};
          addMomentum(2, cols, vals);
        }

        // ---------------- continuity ----------------
        if (i == 0 && j == 0 && k == 0) {
          std::vector<MatStencil> cols{stencil(i, j, k, 3)};
          std::vector<PetscScalar> vals{1.0};
          addMomentum(3, cols, vals);
        } else {
          std::vector<MatStencil> cols;
          std::vector<PetscScalar> vals;
          auto add = [&](int ii, int jj, int kk, int c, Real v) {
            cols.push_back(stencil(ii, jj, kk, c));
            vals.push_back(v);
          };
          // div(u) in x
          add(px(i - 1), j, k, 0, -Real(0.5) / mHx);
          add(px(i + 1), j, k, 0, Real(0.5) / mHx);
          // div(u) in y, one-sided at walls
          if (bottom) {
            add(i, j, k, 1, Real(0.5) / mHy);
            add(i, j + 1, k, 1, Real(0.5) / mHy);
          } else if (top) {
            add(i, j - 1, k, 1, Real(0.5) / mHy);
            add(i, j, k, 1, Real(0.5) / mHy);
          } else {
            add(i, py(j - 1), k, 1, -Real(0.5) / mHy);
            add(i, py(j + 1), k, 1, Real(0.5) / mHy);
          }
          // div(u) in z
          if (mDim == 3) {
            add(i, j, pz(k - 1), 2, -Real(0.5) / mHz);
            add(i, j, pz(k + 1), 2, Real(0.5) / mHz);
          }
          // pressure stabilisation -beta lap(p), Neumann at y walls
          Real diag = 0;
          if (periodicX || i > 0) {
            add(px(i - 1), j, k, 3, -betaX);
            diag += betaX;
          }
          if (periodicX || i + 1 < mNx) {
            add(px(i + 1), j, k, 3, -betaX);
            diag += betaX;
          }
          if (!bottom && !top) {
            add(i, py(j - 1), k, 3, -betaY);
            add(i, py(j + 1), k, 3, -betaY);
            diag += 2 * betaY;
          } else if (periodicY) {
            add(i, py(j - 1), k, 3, -betaY);
            add(i, py(j + 1), k, 3, -betaY);
            diag += 2 * betaY;
          }
          if (mDim == 3) {
            if (periodicZ || k > 0) {
              add(i, j, pz(k - 1), 3, -betaZ);
              diag += betaZ;
            }
            if (periodicZ || k + 1 < mNz) {
              add(i, j, pz(k + 1), 3, -betaZ);
              diag += betaZ;
            }
          }
          add(i, j, k, 3, diag);
          addMomentum(3, cols, vals);
        }
      }
    }
  }

  SATURN_PETSC_CHECK(DMDAVecRestoreArrayDOF(mDa, mB, &bv));
  SATURN_PETSC_CHECK(MatAssemblyBegin(mA, MAT_FINAL_ASSEMBLY));
  SATURN_PETSC_CHECK(MatAssemblyEnd(mA, MAT_FINAL_ASSEMBLY));
  mAssembled = true;
}

void SaturnStokes::solve() {
  if (!mAssembled)
    assemble();
  SATURN_PETSC_CHECK(KSPSolve(mKsp, mB, mX));
  KSPConvergedReason reason;
  SATURN_PETSC_CHECK(KSPGetConvergedReason(mKsp, &reason));
  PetscInt its = 0;
  PetscReal rnorm = 0;
  SATURN_PETSC_CHECK(KSPGetIterationNumber(mKsp, &its));
  SATURN_PETSC_CHECK(KSPGetResidualNorm(mKsp, &rnorm));
  mIterations = static_cast<int>(its);
  mResidual = static_cast<Real>(rnorm);
  if (reason < 0)
    throw PetscError("saturnStokes: coupled KSP did not converge, reason=" +
                     std::to_string(static_cast<int>(reason)));
  mSolved = true;
}

SteadyResult SaturnStokes::solveSteady() {
  initialize();
  assemble();
  solve();
  SteadyResult out;
  out.converged = mSolved;
  out.iterations = mIterations;
  out.residual = mResidual;
  return out;
}

Real SaturnStokes::minCellSize() const {
  return std::min(mHx, std::min(mHy, mDim == 3 ? mHz : mHy));
}

void SaturnStokes::rhs(const CellField&, CellField&) {
  throw std::logic_error(
      "saturnStokes is a steady solver; call solveSteady() instead of rhs()");
}

std::vector<VectorProfilePoint> SaturnStokes::profile(bool withExact) const {
  if (withExact && !hasExact())
    throw std::runtime_error(
        "saturnStokes: profile comparison needs exact data");
  if (mX == nullptr)
    throw std::runtime_error("saturnStokes: solve before profile()");

  PetscInt xs = 0, ys = 0, zs = 0, xm = 0, ym = 0, zm = 0;
  SATURN_PETSC_CHECK(DMDAGetCorners(mDa, &xs, &ys, &zs, &xm, &ym, &zm));
  const PetscScalar**** xv = nullptr;
  SATURN_PETSC_CHECK(DMDAVecGetArrayDOFRead(mDa, mX, &xv));

  const int localCount = static_cast<int>(xm * ym * zm);
  std::vector<double> local;
  local.reserve(static_cast<std::size_t>(localCount) * 13);
  for (PetscInt k = zs; k < zs + zm; ++k) {
    for (PetscInt j = ys; j < ys + ym; ++j) {
      for (PetscInt i = xs; i < xs + xm; ++i) {
        const Real x = mXmin + (static_cast<Real>(i) + Real(0.5)) * mHx;
        const Real y = mYmin + (static_cast<Real>(j) + Real(0.5)) * mHy;
        const Real z =
            mZmin + ((mDim == 3) ? (static_cast<Real>(k) + Real(0.5)) * mHz
                                 : Real(0.5) * mHz);
        const Real u = xv[k][j][i][0];
        const Real v = xv[k][j][i][1];
        const Real w = (mDim == 3) ? xv[k][j][i][2] : Real(0);
        const Real p = xv[k][j][i][3];
        Real ue = 0, ve = 0, we = 0, pe = 0;
        if (withExact) {
          const Vec3 ev = mOptions.exactVelocity({x, y, z});
          ue = ev.x;
          ve = ev.y;
          we = ev.z;
          pe = mOptions.exactPressure({x, y, z});
        }
        local.push_back(x);
        local.push_back(y);
        local.push_back(z);
        local.push_back(u);
        local.push_back(v);
        local.push_back(w);
        local.push_back(p);
        local.push_back(ue);
        local.push_back(ve);
        local.push_back(we);
        local.push_back(pe);
        local.push_back(static_cast<double>(i + mNx * (j + mNy * k)));
        local.push_back(0.0);  // reserved for divergence
      }
    }
  }
  SATURN_PETSC_CHECK(DMDAVecRestoreArrayDOFRead(mDa, mX, &xv));

  std::vector<double> global;
#ifdef SATURN_USE_MPI
  int worldSize = 1;
  MPI_Comm_size(PETSC_COMM_WORLD, &worldSize);
  std::vector<int> counts(static_cast<std::size_t>(worldSize));
  std::vector<int> offsets(static_cast<std::size_t>(worldSize));
  MPI_Allgather(&localCount, 1, MPI_INT, counts.data(), 1, MPI_INT,
                MPI_COMM_WORLD);
  offsets[0] = 0;
  for (int r = 1; r < worldSize; ++r)
    offsets[static_cast<std::size_t>(r)] =
        offsets[static_cast<std::size_t>(r - 1)] +
        counts[static_cast<std::size_t>(r - 1)];
  const int total = offsets.back() + counts.back();
  std::vector<int> dcounts(static_cast<std::size_t>(worldSize));
  std::vector<int> doffsets(static_cast<std::size_t>(worldSize));
  for (int r = 0; r < worldSize; ++r) {
    dcounts[static_cast<std::size_t>(r)] =
        13 * counts[static_cast<std::size_t>(r)];
    doffsets[static_cast<std::size_t>(r)] =
        13 * offsets[static_cast<std::size_t>(r)];
  }
  global.resize(static_cast<std::size_t>(13) * total);
  MPI_Allgatherv(local.data(), 13 * localCount, MPI_DOUBLE, global.data(),
                 dcounts.data(), doffsets.data(), MPI_DOUBLE, MPI_COMM_WORLD);
#else
  global = std::move(local);
#endif

  const std::size_t pointCount = global.size() / 13;
  std::vector<VectorProfilePoint> points(pointCount);
  for (std::size_t q = 0; q < pointCount; ++q) {
    const std::size_t b = 13 * q;
    VectorProfilePoint point;
    point.x = global[b + 0];
    point.y = global[b + 1];
    point.z = global[b + 2];
    point.u = global[b + 3];
    point.v = global[b + 4];
    point.w = global[b + 5];
    point.p = global[b + 6];
    point.uExact = global[b + 7];
    point.vExact = global[b + 8];
    point.wExact = global[b + 9];
    point.pExact = global[b + 10];
    point.cell = static_cast<Index>(global[b + 11]);
    point.error =
        std::sqrt((point.u - point.uExact) * (point.u - point.uExact) +
                  (point.v - point.vExact) * (point.v - point.vExact) +
                  (point.w - point.wExact) * (point.w - point.wExact));
    points[q] = point;
  }
  std::sort(points.begin(), points.end(),
            [](const VectorProfilePoint& a, const VectorProfilePoint& b) {
              if (a.x != b.x)
                return a.x < b.x;
              if (a.y != b.y)
                return a.y < b.y;
              return a.z < b.z;
            });
  return points;
}

VectorError SaturnStokes::exactError() const {
  const std::vector<VectorProfilePoint> points = profile(true);
  VectorError out;
  out.samples = static_cast<BigIndex>(points.size());
  if (points.empty())
    return out;

  double l1 = 0, l2 = 0, linf = 0;
  double pMean = 0;
  for (const VectorProfilePoint& p : points) {
    l1 += std::abs(p.error);
    l2 += p.error * p.error;
    linf = std::max(linf, static_cast<double>(std::abs(p.error)));
    pMean += static_cast<double>(p.p - p.pExact);
  }
  pMean /= static_cast<double>(points.size());

  double pL2 = 0, pLinf = 0;
  for (const VectorProfilePoint& p : points) {
    const double e = static_cast<double>(p.p - p.pExact) - pMean;
    pL2 += e * e;
    pLinf = std::max(pLinf, std::abs(e));
  }
  out.l1 = static_cast<Real>(l1 / static_cast<double>(points.size()));
  out.l2 =
      static_cast<Real>(std::sqrt(l2 / static_cast<double>(points.size())));
  out.linf = static_cast<Real>(linf);
  out.pL2 =
      static_cast<Real>(std::sqrt(pL2 / static_cast<double>(points.size())));
  out.pLinf = static_cast<Real>(pLinf);
  return out;
}

}  // namespace saturn
