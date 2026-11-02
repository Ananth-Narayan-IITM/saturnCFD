#include "solvers/saturnStokes/StokesCase.hpp"

#include <cmath>
#include <utility>

#include "config/CommonSchema.hpp"
#include "core/Error.hpp"
#include "mesh/MeshFactory.hpp"
#include "simulation/Registry.hpp"
#include "solvers/saturnStokes/SaturnStokes.hpp"

namespace saturn {
namespace {

constexpr Real kPi = 3.14159265358979323846264338327950288;

Vec3 mmsVelocity(const Vec3& x, int dim) {
  const Real k = 2 * kPi;
  const Real u = std::sin(k * x.x) * std::cos(k * x.y) *
                 (dim == 3 ? std::cos(k * x.z) : Real(1));
  const Real v = -std::cos(k * x.x) * std::sin(k * x.y) *
                 (dim == 3 ? std::cos(k * x.z) : Real(1));
  return {u, v, 0};
}

Real mmsPressure(const Vec3& x, int dim) {
  const Real k = 2 * kPi;
  return std::sin(k * x.x) * std::sin(k * x.y) *
         (dim == 3 ? std::sin(k * x.z) : Real(1));
}

Vec3 mmsForce(const Vec3& x, int dim, Real mu) {
  const Real k = 2 * kPi;
  const Vec3 u = mmsVelocity(x, dim);
  const Real lapFactor = (dim == 3) ? Real(3) : Real(2);
  const Real k2 = k * k;
  Vec3 f;
  f.x =
      lapFactor * mu * k2 * u.x + k * std::cos(k * x.x) * std::sin(k * x.y) *
                                      (dim == 3 ? std::sin(k * x.z) : Real(1));
  f.y =
      lapFactor * mu * k2 * u.y + k * std::sin(k * x.x) * std::cos(k * x.y) *
                                      (dim == 3 ? std::sin(k * x.z) : Real(1));
  f.z = (dim == 3)
            ? k * std::sin(k * x.x) * std::sin(k * x.y) * std::cos(k * x.z)
            : Real(0);
  return f;
}

}  // namespace

ConfigSchema stokesSchema() {
  ConfigSchema s = commonSchema();
  s.addDouble("stokes.mu", 1.0, "dynamic viscosity [Pa.s]",
              Dimension::DynamicViscosity, 0.0, 0.0, false);
  s.addEnum("stokes.case", "poiseuille", {"poiseuille", "couette", "mms"},
            "analytic Stokes case");
  s.addDouble("stokes.forceX", 0.0, "x body force per volume [N/m3]",
              Dimension::BodyForce);
  s.addDouble("stokes.forceY", 0.0, "y body force per volume [N/m3]",
              Dimension::BodyForce);
  s.addDouble("stokes.forceZ", 0.0, "z body force per volume [N/m3]",
              Dimension::BodyForce);
  s.addDouble("stokes.wallVelocityX", 0.0, "x velocity of the top wall [m/s]",
              Dimension::Velocity);
  s.addBool("stokes.periodicY", false,
            "periodic y boundaries (MMS only); otherwise walls");
  s.addEnum("stokes.linearSolver", "fieldsplit", {"direct", "fieldsplit"},
            "coupled PETSc linear solve: fieldsplit/Schur by default "
            "(O(N) memory), direct LU for validation or very high memory");
  s.addDouble("stokes.stabilization", 1.0,
              "pressure-stabilisation factor (0 disables)");
  return s;
}

std::unique_ptr<Solver> makeSaturnStokes(const Config& cfg) {
  Mesh mesh = makeMeshForSolver(cfg);
  if (mesh.dimension != 2 && mesh.dimension != 3)
    throw ConfigError("saturnStokes supports structured 2D and 3D meshes");
  if (mesh.nz < 1 || (mesh.dimension == 2 && mesh.nz != 1))
    throw ConfigError("saturnStokes: 2D requires nz = 1");

  StokesOptions options;
  options.mu = cfg.getDouble("stokes.mu", 1.0);
  options.stabilization = cfg.getDouble("stokes.stabilization", 1.0);
  options.periodicX = true;
  options.periodicZ = true;
  options.periodicY = cfg.getBool("stokes.periodicY", false);
  options.direct =
      cfg.getString("stokes.linearSolver", "fieldsplit") == "direct";
  options.force = {cfg.getDouble("stokes.forceX", 0.0),
                   cfg.getDouble("stokes.forceY", 0.0),
                   cfg.getDouble("stokes.forceZ", 0.0)};

  const std::string test = cfg.getString("stokes.case", "poiseuille");
  const Real y0 = mesh.bboxMin.y;
  const Real H = mesh.bboxMax.y - y0;

  if (test == "poiseuille") {
    const Real G = cfg.getDouble("stokes.forceX", 1.0);
    const Real mu = options.mu;
    options.force = {G, 0, 0};
    options.periodicY = false;
    options.exactVelocity = [G, mu, y0, H](const Vec3& x) {
      const Real yy = x.y - y0;
      return Vec3{G / (2 * mu) * yy * (H - yy), 0, 0};
    };
    options.exactPressure = [](const Vec3&) { return Real(0); };
  } else if (test == "couette") {
    const Real U = cfg.getDouble("stokes.wallVelocityX", 1.0);
    options.force = {0, 0, 0};
    options.periodicY = false;
    options.wallTopU = U;
    options.exactVelocity = [U, y0, H](const Vec3& x) {
      return Vec3{U * (x.y - y0) / H, 0, 0};
    };
    options.exactPressure = [](const Vec3&) { return Real(0); };
  } else if (test == "mms") {
    options.force = {0, 0, 0};
    options.periodicY = true;
    const int dim = mesh.dimension;
    options.bodyForce = [dim, mu = options.mu](const Vec3& x) {
      return mmsForce(x, dim, mu);
    };
    options.exactVelocity = [dim](const Vec3& x) {
      return mmsVelocity(x, dim);
    };
    options.exactPressure = [dim](const Vec3& x) {
      return mmsPressure(x, dim);
    };
  } else {
    throw ConfigError("unsupported stokes.case '" + test + "'");
  }

  return std::make_unique<SaturnStokes>(std::move(mesh), std::move(options));
}

void registerSaturnStokes(SolverRegistry& registry) {
  registry.addCase(
      "saturnStokes", [](const Config& cfg) { return makeSaturnStokes(cfg); },
      [] { return stokesSchema(); });
}

}  // namespace saturn
