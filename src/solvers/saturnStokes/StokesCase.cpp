#include "solvers/saturnStokes/StokesCase.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "config/CommonSchema.hpp"
#include "core/Error.hpp"
#include "mesh/MeshFactory.hpp"
#include "simulation/Registry.hpp"
#include "solvers/saturnStokes/SaturnStokes.hpp"
#include "solvers/saturnStokes/StokesMms.hpp"

namespace saturn {

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
  s.addBool("stokes.mmsWall", false,
            "MMS variant: wall-bounded (true) or fully periodic (false)");
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
    const MmsVariant variant = cfg.getBool("stokes.mmsWall", false)
                                   ? MmsVariant::Wall
                                   : MmsVariant::Periodic;
    options.periodicY = (variant == MmsVariant::Periodic);
    const int dim = mesh.dimension;
    const Real hx =
        (mesh.bboxMax.x - mesh.bboxMin.x) / static_cast<Real>(mesh.nx);
    const Real hy =
        (mesh.bboxMax.y - mesh.bboxMin.y) / static_cast<Real>(mesh.ny);
    const Real hz = (dim == 3) ? (mesh.bboxMax.z - mesh.bboxMin.z) /
                                     static_cast<Real>(mesh.nz)
                               : hy;
    const Real hMin = std::min(hx, std::min(hy, hz));
    const Real beta = options.stabilization * hMin * hMin / options.mu;
    // The pressure reference row pins p = 0 at the first cell centre. Shift
    // the manufactured pressure so that it is exactly zero there; a constant
    // shift does not change grad(p) or lap(p), hence not f or g either.
    const Vec3 pinPoint{mesh.bboxMin.x + Real(0.5) * hx,
                        mesh.bboxMin.y + Real(0.5) * hy,
                        (dim == 3 ? mesh.bboxMin.z + Real(0.5) * hz : Real(0))};
    const Real pPin = mmsPressure(pinPoint, dim, variant);
    options.bodyForce = [dim, variant, mu = options.mu](const Vec3& x) {
      return mmsForce(x, dim, variant, mu);
    };
    options.continuitySource = [dim, variant, beta](const Vec3& x) {
      return -beta * mmsPressureLaplacian(x, dim, variant);
    };
    options.exactVelocity = [dim, variant](const Vec3& x) {
      return mmsVelocity(x, dim, variant);
    };
    options.exactPressure = [dim, variant, pPin](const Vec3& x) {
      return mmsPressure(x, dim, variant) - pPin;
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
