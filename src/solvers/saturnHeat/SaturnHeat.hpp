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

namespace saturn {

/// Options for the scalar convection–diffusion (heat) solver.
///
/// The solver advances
///
///   dT/dt + div(a T) = alpha lap(T) + source
///
/// on a structured 1D/2D/3D block with a prescribed constant velocity `a`.
struct HeatOptions {
  Real alpha = 1e-2;
  Vec3 velocity{1, 0, 0};
  Real source = 0;
  bool periodic = false;
  ConvectiveScheme convection = ConvectiveScheme::Muscl;
  Limiter limiter = Limiter::VanLeer;

  std::function<Real(const Vec3&)> initial;
  std::function<Real(const Vec3&, Real, int)> dirichlet;
  std::function<Real(const Vec3&, Real)> exact;
};

/// Scalar convection–diffusion solver for 1D/2D/3D structured blocks.
class SaturnHeat : public ScalarSolver {
 public:
  SaturnHeat(Mesh mesh, HeatOptions options);

  std::string name() const override { return "saturnHeat"; }

  void initialize() override;
  void applyInitialConditions() override;
  void applyBoundaryConditions() override;
  void rhs(const CellField& state, CellField& rate) override;
  void setTime(Real time) override { mTime = time; }

  Real maxWaveSpeed() const override;
  Real maxDiffusivity() const override { return mOptions.alpha; }
  Real minCellSize() const override { return mMinCellSize; }
  int dimension() const override { return mMesh.dimension; }

  CellField& state() override { return mT; }
  const CellField& state() const override { return mT; }

  const Mesh& mesh() const override { return mMesh; }
  const HeatOptions& options() const { return mOptions; }
  bool hasExact() const override { return static_cast<bool>(mOptions.exact); }

  std::vector<ScalarProfilePoint> profile(Real time,
                                          bool withExact = true) const override;
  ScalarError exactError(Real time) const override;
  std::vector<Real> globalState() const override;
  void setGlobalState(const std::vector<Real>& values) override;

 private:
  Mesh mMesh;
  HeatOptions mOptions;
  MeshPartition mPart;
  LocalMesh mLocal;
  HaloExchange mHalo;
  FieldHalo mFieldHalo;
  CellField mT;
  FaceField mFlux;
  FaceField mNormalVelocity;
  FaceField mLapFace;
  CellField mDiv;
  CellField mLap;
  CellField mGrad;
  Real mTime = 0;
  Real mMinCellSize = 1;
};

}  // namespace saturn
