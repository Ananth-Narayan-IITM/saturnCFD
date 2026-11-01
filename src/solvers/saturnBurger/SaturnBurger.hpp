#pragma once

#include <functional>
#include <string>
#include <vector>

#include "comm/HaloExchange.hpp"
#include "comm/Partition.hpp"
#include "core/Types.hpp"
#include "fv/BoundaryFill.hpp"
#include "fv/CellField.hpp"
#include "fv/Convection.hpp"
#include "fv/FaceField.hpp"
#include "fv/FieldHalo.hpp"
#include "fv/LocalMesh.hpp"
#include "mesh/Mesh.hpp"
#include "simulation/ScalarSolver.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

/// Options for the scalar Burgers solver (1D, 2D or 3D).
///
/// The solver advances
///
///   du/dt + d/dx(u^2/2) = nu d2u/dx2 + source
///
/// on a structured 1D/2D/3D block. The initial and Dirichlet callbacks receive
/// the physical point; Dirichlet also receives time so a moving exact solution
/// can be imposed.
struct BurgersOptions {
  Real nu = 1e-2;
  Real source = 0;
  bool periodic = false;
  ConvectiveScheme convection = ConvectiveScheme::Muscl;
  Limiter limiter = Limiter::VanLeer;

  std::function<Real(const Vec3&)> initial;
  std::function<Real(const Vec3&, Real, int)> dirichlet;

  /// Optional analytic solution `u_exact(x,t)`. When set, the CLI can compare
  /// the numerical profile with it and write a validation CSV.
  std::function<Real(const Vec3&, Real)> exact;
};

using ProfilePoint = ScalarProfilePoint;
using ExactError = ScalarError;

/// Scalar Burgers solver for 1D, 2D and 3D structured blocks.
///
/// This is the first production solver. The spatial operators are the shared
/// FVM layer; the time loop is `Simulation`. The multi-D case uses the
/// conservative flux vector F(u) = (u^2/2, u^2/2, u^2/2).
class SaturnBurger : public ScalarSolver {
 public:
  SaturnBurger(Mesh mesh, BurgersOptions options);

  std::string name() const override { return "saturnBurger"; }

  void initialize() override;
  void applyInitialConditions() override;
  void applyBoundaryConditions() override;
  void rhs(const CellField& state, CellField& rate) override;
  void setTime(Real time) override { mTime = time; }

  Real maxWaveSpeed() const override;
  Real maxDiffusivity() const override { return mOptions.nu; }
  Real minCellSize() const override { return mMinCellSize; }
  int dimension() const override { return mMesh.dimension; }

  CellField& state() override { return mU; }
  const CellField& state() const override { return mU; }

  const Mesh& mesh() const override { return mMesh; }
  const BurgersOptions& options() const { return mOptions; }
  bool hasExact() const override { return static_cast<bool>(mOptions.exact); }

  /// Gather the owned cell-centre profile on every rank, sorted by x/y/z.
  /// `withExact` requires `BurgersOptions::exact`; when false, only the
  /// numerical field and cell ids are filled. This is used by VTK output.
  std::vector<ScalarProfilePoint> profile(Real time,
                                          bool withExact = true) const override;

  /// Global error norms against `BurgersOptions::exact`.
  ScalarError exactError(Real time) const override;

  /// Global field in mesh cell order. Used by checkpoint output.
  std::vector<Real> globalState() const override;

  /// Replace the owned state from a global field in mesh cell order.
  /// The caller is responsible for refreshing ghosts afterwards.
  void setGlobalState(const std::vector<Real>& values) override;

 private:
  Mesh mMesh;
  BurgersOptions mOptions;
  MeshPartition mPart;
  LocalMesh mLocal;
  HaloExchange mHalo;
  FieldHalo mFieldHalo;
  CellField mU;
  FaceField mFlux;
  FaceField mLapFace;
  CellField mDiv;
  CellField mLap;
  CellField mGrad;
  Real mTime = 0;
  Real mMinCellSize = 1;
};

}  // namespace saturn
