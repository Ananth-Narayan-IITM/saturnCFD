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
#include "simulation/Solver.hpp"

namespace saturn {

/// Options for the 1D scalar Burgers solver.
///
/// The solver advances
///
///   du/dt + d/dx(u^2/2) = nu d2u/dx2 + source
///
/// on a structured 1D block. The initial and Dirichlet callbacks receive the
/// physical point; Dirichlet also receives time so a moving exact solution can
/// be imposed.
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

/// One cell-centre sample of the numerical and analytic profiles.
struct ProfilePoint {
  Real x = 0;
  Real numerical = 0;
  Real exact = 0;
  Real error = 0;
};

/// Global L1/L2/Linf error against the analytic solution.
struct ExactError {
  Real l1 = 0;
  Real l2 = 0;
  Real linf = 0;
  BigIndex samples = 0;
};

/// 1D scalar Burgers solver.
///
/// This is the first production solver. The spatial operators are the shared
/// FVM layer; the time loop is `Simulation`. The 2D/3D extension will keep the
/// same class name and add the transverse directions.
class SaturnBurger : public Solver {
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
  int dimension() const override { return 1; }

  CellField& state() override { return mU; }
  const CellField& state() const override { return mU; }

  const Mesh& mesh() const { return mMesh; }
  const BurgersOptions& options() const { return mOptions; }
  bool hasExact() const { return static_cast<bool>(mOptions.exact); }

  /// Gather the owned cell-centre profile on every rank, sorted by x.
  /// Requires `BurgersOptions::exact` when exact values are requested.
  std::vector<ProfilePoint> profile(Real time) const;

  /// Global error norms against `BurgersOptions::exact`.
  ExactError exactError(Real time) const;

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
