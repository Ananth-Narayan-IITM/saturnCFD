#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <stdexcept>

#include <Kokkos_Core.hpp>

#include "comm/HaloExchange.hpp"
#include "comm/Partition.hpp"
#include "fv/BoundaryFill.hpp"
#include "fv/CellField.hpp"
#include "fv/Convection.hpp"
#include "fv/FaceField.hpp"
#include "fv/FieldHalo.hpp"
#include "fv/Laplacian.hpp"
#include "fv/LocalMesh.hpp"
#include "fv/Operators.hpp"
#include "simulation/Registry.hpp"
#include "simulation/Simulation.hpp"
#include "simulation/Solver.hpp"
#include "simulation/TimeControl.hpp"

#include "UniformMesh.hpp"

using namespace saturn;

namespace {

constexpr double kPi = 3.14159265358979323846;

KOKKOS_INLINE_FUNCTION constexpr double piValue() {
  return 3.14159265358979323846;
}

using Policy3 = Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>;

Policy3 ownedPolicy(const CellField& field) {
  return Policy3(
      {field.origin(0), field.origin(1), field.origin(2)},
      {field.origin(0) + field.extent(0), field.origin(1) + field.extent(1),
       field.origin(2) + field.extent(2)});
}

enum class Initial { Constant, Sine, Cosine };

/// Periodic scalar advection-diffusion used to exercise the time loop:
///   du/dt + div(a u) = nu lap(u).
class PeriodicAdvectionDiffusion : public Solver {
 public:
  PeriodicAdvectionDiffusion(int n, Vec3 velocity, Real diffusivity,
                             Initial initial)
      : mN(n),
        mVelocity(velocity),
        mDiffusivity(diffusivity),
        mInitial(initial),
        mMesh(test::makeUniformBoxMesh(n, n, n)),
        mPart(n, n, n, true, true, true),
        mLocal(mMesh, mPart, 2),
        mHaloPhi(mPart, 2, 1),
        mFieldHaloPhi(mHaloPhi, 1),
        mPhi(mPart, 2, 1, "test::sim_phi"),
        mVel(mPart, 2, 3, "test::sim_vel"),
        mVelFaces(mPart, 2, 3, "test::sim_vel_face"),
        mUn(mPart, 2, 1, "test::sim_un"),
        mFlux(mPart, 2, 1, "test::sim_flux"),
        mLapFace(mPart, 2, 1, "test::sim_lap_face"),
        mConv(mPart, 2, 1, "test::sim_conv"),
        mLap(mPart, 2, 1, "test::sim_lap"),
        mGrad(mPart, 2, 3, "test::sim_grad") {}

  std::string name() const override { return "periodicAdvectionDiffusion"; }

  void initialize() override {
    CellField v = mVel;
    const Vec3 velocity = mVelocity;
    Kokkos::parallel_for(
        "test::sim_initialize", ownedPolicy(mVel),
        KOKKOS_LAMBDA(int i, int j, int k) {
          for (int c = 0; c < 3; ++c)
            v(i, j, k, c) = velocity[c];
        });
    HaloExchange halo(mPart, 2, 3);
    FieldHalo fieldHalo(halo, 3);
    fieldHalo.execute(mVel);
    faceNormalVelocity(mVel, mVelFaces, mUn, mLocal);
  }

  void applyInitialConditions() override {
    const LocalMesh::DeviceData mesh = mLocal.device();
    const Initial kind = mInitial;
    CellField p = mPhi;
    Kokkos::parallel_for(
        "test::sim_initial", ownedPolicy(mPhi),
        KOKKOS_LAMBDA(int i, int j, int k) {
          const Vec3 x = mesh.cellCentroid(i, j, k);
          if (kind == Initial::Constant) {
            p(i, j, k, 0) = 1;
          } else if (kind == Initial::Sine) {
            p(i, j, k, 0) = Kokkos::sin(2 * piValue() * x.x) *
                            Kokkos::sin(2 * piValue() * x.y) *
                            Kokkos::sin(2 * piValue() * x.z);
          } else {
            p(i, j, k, 0) = Kokkos::cos(2 * piValue() * x.x) *
                            Kokkos::cos(2 * piValue() * x.y) *
                            Kokkos::cos(2 * piValue() * x.z);
          }
        });
    applyBoundaryConditions();
  }

  void applyBoundaryConditions() override { mFieldHaloPhi.execute(mPhi); }

  void rhs(const CellField& state, CellField& rate) override {
    if (state.ncomp() != 1)
      throw std::invalid_argument("test solver expects a scalar field");
    convectiveFlux(mUn, state, mFlux, mLocal, ConvectiveScheme::Muscl,
                   Limiter::VanLeer);
    divergence(mFlux, mConv, mLocal);
    laplacian(state, mLapFace, mGrad, mLap, mLocal);

    CellField r = rate;
    CellField conv = mConv;
    CellField lap = mLap;
    const Real nu = mDiffusivity;
    Kokkos::parallel_for(
        "test::sim_rhs", ownedPolicy(state),
        KOKKOS_LAMBDA(int i, int j, int k) {
          r(i, j, k, 0) = -conv(i, j, k, 0) + nu * lap(i, j, k, 0);
        });
  }

  Real maxWaveSpeed() const override { return norm(mVelocity); }
  Real maxDiffusivity() const override { return mDiffusivity; }
  Real minCellSize() const override { return Real(1) / mN; }
  int dimension() const override { return mLocal.dimension(); }

  CellField& state() override { return mPhi; }
  const CellField& state() const override { return mPhi; }

 private:
  int mN;
  Vec3 mVelocity;
  Real mDiffusivity;
  Initial mInitial;
  Mesh mMesh;
  MeshPartition mPart;
  LocalMesh mLocal;
  HaloExchange mHaloPhi;
  FieldHalo mFieldHaloPhi;
  CellField mPhi;
  CellField mVel;
  FaceField mVelFaces;
  FaceField mUn;
  FaceField mFlux;
  FaceField mLapFace;
  CellField mConv;
  CellField mLap;
  CellField mGrad;
};

}  // namespace

TEST_CASE("simulation: explicit schemes preserve a constant field") {
  for (TimeScheme scheme :
       {TimeScheme::ForwardEuler, TimeScheme::SSPRK2, TimeScheme::SSPRK3}) {
    PeriodicAdvectionDiffusion solver(8, Vec3{1, 0.5, 0.25}, 0.05,
                                      Initial::Constant);
    TimeControl control;
    control.tEnd = 0.02;
    control.dtMax = 0.005;
    control.cfl = 0.4;
    Simulation simulation(solver, control, scheme);
    const SimulationResult result = simulation.run();
    CHECK(result.reachedEnd);
    CHECK(result.history.size() > 1);
    CHECK(result.final.linf == doctest::Approx(1.0).epsilon(1e-10));
    CHECK(result.final.l2 == doctest::Approx(1.0).epsilon(1e-10));
  }
}

TEST_CASE("simulation: SSPRK3 diffuses a cosine mode") {
  PeriodicAdvectionDiffusion solver(16, Vec3{0, 0, 0}, 0.05, Initial::Cosine);
  TimeControl control;
  control.tEnd = 0.05;
  control.dtMax = 0.01;
  control.cfl = 0.5;
  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  CHECK(result.reachedEnd);

  const double initialL2 = std::sqrt(1.0 / 8.0);
  const double exactL2 =
      initialL2 * std::exp(-0.05 * 3 * (2 * kPi) * (2 * kPi) * 0.05);
  MESSAGE("diffusion L2: computed=" << result.final.l2 << " exact=" << exactL2);
  CHECK(std::abs(result.final.l2 - exactL2) < 5e-3);
}

TEST_CASE("simulation: SSPRK3 advection of a sine stays bounded") {
  PeriodicAdvectionDiffusion solver(16, Vec3{1, 0, 0}, 0, Initial::Sine);
  TimeControl control;
  control.tEnd = 0.1;
  control.dtMax = 0.01;
  control.cfl = 0.4;
  Simulation simulation(solver, control, TimeScheme::SSPRK3);
  const SimulationResult result = simulation.run();
  CHECK(result.reachedEnd);
  CHECK(std::isfinite(result.final.l1));
  CHECK(result.final.linf < 1.01);
}

TEST_CASE("simulation: TimeControl selects the active stability limit") {
  TimeControl control;
  control.dtMax = 1;
  control.cfl = 0.5;
  control.diffusionSafety = 0.5;
  CHECK(control.stableDt(2, 0, 0.1, 3) ==
        doctest::Approx(0.025).epsilon(1e-12));
  CHECK(control.stableDt(0, 0.1, 0.1, 3) ==
        doctest::Approx(0.5 * 0.01 / 0.6).epsilon(1e-12));
}

TEST_CASE("simulation: solver registry stores and creates factories") {
  SolverRegistry registry;
  registry.add("test", [] {
    return std::make_unique<PeriodicAdvectionDiffusion>(4, Vec3{0, 0, 0}, 0,
                                                        Initial::Constant);
  });
  CHECK(registry.contains("test"));
  const std::vector<std::string> names = registry.names();
  REQUIRE(names.size() == 1);
  CHECK(names[0] == "test");
  const auto solver = registry.create("test");
  CHECK(solver->name() == "periodicAdvectionDiffusion");
  CHECK_THROWS_AS(registry.create("missing"), std::invalid_argument);
  CHECK_THROWS_AS(
      registry.add("test",
                   [] {
                     return std::make_unique<PeriodicAdvectionDiffusion>(
                         4, Vec3{0, 0, 0}, 0, Initial::Constant);
                   }),
      std::invalid_argument);
}

TEST_CASE("simulation: implicit schemes fail with a clear message") {
  PeriodicAdvectionDiffusion solver(8, Vec3{0, 0, 0}, 0, Initial::Constant);
  TimeControl control;
  control.tEnd = 0.1;
  Simulation simulation(solver, control, TimeScheme::BackwardEuler);
  CHECK_THROWS_AS(simulation.run(), std::invalid_argument);
}
