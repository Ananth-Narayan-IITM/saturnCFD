#pragma once

#include <functional>
#include <string>
#include <vector>

#include <petsc.h>

#include "comm/Partition.hpp"
#include "core/Types.hpp"
#include "fv/CellField.hpp"
#include "mesh/Mesh.hpp"
#include "simulation/SteadySolver.hpp"

namespace saturn {

/// Options for the steady Stokes solver.
///
/// The collocated system is
///
///   -mu lap(u) + grad(p) = f,
///   div(u) - beta lap(p) = 0,
///
/// where the pressure term is the stabilisation that replaces the
/// checkerboard-prone raw collocated divergence. `stabilization` scales
/// beta = stabilization * h^2 / mu.
struct StokesOptions {
  Real mu = 1.0;
  Vec3 force{0, 0, 0};
  bool periodicX = true;
  bool periodicY = false;
  bool periodicZ = true;
  Real wallBottomU = 0;
  Real wallTopU = 0;
  Real stabilization = 1.0;
  bool direct = true;

  std::function<Vec3(const Vec3&)> bodyForce;
  std::function<Vec3(const Vec3&)> exactVelocity;
  std::function<Real(const Vec3&)> exactPressure;
};

/// Steady 3D Stokes solver on a structured box.
///
/// The current core uses a PETSc DMDA with four cell-centred degrees of
/// freedom (u,v,w,p). A 2D case uses the same core with nz = 1; the w rows are
/// identity rows and w = 0. The coupled saddle-point matrix is solved with
/// GMRES/FGMRES and a fieldsplit/Schur preconditioner.
class SaturnStokes : public SteadySolver {
 public:
  SaturnStokes(Mesh mesh, StokesOptions options);
  ~SaturnStokes() override;

  std::string name() const override { return "saturnStokes"; }

  void initialize() override;
  void applyInitialConditions() override;
  void applyBoundaryConditions() override {}
  void rhs(const CellField&, CellField&) override;
  void setTime(Real) override {}

  Real maxWaveSpeed() const override { return 0; }
  Real maxDiffusivity() const override { return mOptions.mu; }
  Real minCellSize() const override;
  int dimension() const override { return mDim; }

  CellField& state() override { return mState; }
  const CellField& state() const override { return mState; }

  bool hasExact() const override {
    return static_cast<bool>(mOptions.exactVelocity) &&
           static_cast<bool>(mOptions.exactPressure);
  }
  const Mesh& mesh() const override { return mMesh; }

  SteadyResult solveSteady() override;
  std::vector<VectorProfilePoint> profile(bool withExact = true) const override;
  VectorError exactError() const override;

  const StokesOptions& options() const { return mOptions; }

 private:
  Mesh mMesh;
  StokesOptions mOptions;
  MeshPartition mPart;
  CellField mState;
  Real mTime = 0;

  int mDim = 3;
  PetscInt mNx = 0;
  PetscInt mNy = 0;
  PetscInt mNz = 0;
  Real mHx = 1;
  Real mHy = 1;
  Real mHz = 1;
  Real mXmin = 0;
  Real mYmin = 0;
  Real mZmin = 0;

  DM mDa = nullptr;
  Mat mA = nullptr;
  Vec mB = nullptr;
  Vec mX = nullptr;
  KSP mKsp = nullptr;
  MatNullSpace mNullspace = nullptr;
  IS mVelocityIS = nullptr;
  IS mPressureIS = nullptr;
  bool mAssembled = false;
  bool mSolved = false;
  int mIterations = 0;
  Real mResidual = 0;

  void createDmda();
  void assemble();
  void solve();
  void createSolver();
  void destroyPetsc();
};

}  // namespace saturn
