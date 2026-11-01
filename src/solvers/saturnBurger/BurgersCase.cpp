#include "solvers/saturnBurger/BurgersCase.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "config/CommonSchema.hpp"
#include "core/Error.hpp"
#include "fv/Convection.hpp"
#include "mesh/MeshFactory.hpp"
#include "simulation/Registry.hpp"
#include "solvers/saturnBurger/SaturnBurger.hpp"

namespace saturn {
namespace {

constexpr Real kPi = Real(3.14159265358979323846264338327950288);

/// Exact viscous traveling-wave solution of scalar Burgers.
///
/// 1D:  u_t + (u^2/2)_x = nu u_xx
///      u = U/2 [1 - tanh(U (x - x0 - U t/2)/(4 nu))]
///
/// 2D:  u_t + (u^2/2)_x + (u^2/2)_y = nu (u_xx + u_yy)
///      u = U/2 [1 - tanh(U (x+y - x0-y0 - U t)/(4 nu))]
///
/// 3D:  u_t + (u^2/2)_x + (u^2/2)_y + (u^2/2)_z
///          = nu (u_xx + u_yy + u_zz)
///      u = U/2 [1 - tanh(U (x+y+z - x0-y0-z0 - 3 U t/2)/(4 nu))]
///
/// This is a physical case preset, not a test helper: the YAML option
/// `burgers.initial: travelingWave` uses it for the initial state, the
/// time-dependent Dirichlet data and the `output.exact` reference.
Real travelingWave(const Vec3& p, Real t, int dim, Real speed, Real nu, Real x0,
                   Real y0, Real z0) {
  Real coordinate = p.x - x0;
  Real waveSpeed = Real(0.5) * speed;
  if (dim >= 2) {
    coordinate += p.y - y0;
    waveSpeed = speed;
  }
  if (dim >= 3) {
    coordinate += p.z - z0;
    waveSpeed = Real(1.5) * speed;
  }
  return Real(0.5) * speed *
         (Real(1) -
          std::tanh(speed * (coordinate - waveSpeed * t) / (Real(4) * nu)));
}

}  // namespace

ConfigSchema burgersSchema() {
  ConfigSchema s = commonSchema();
  s.addDouble("burgers.nu", 0.02, "kinematic viscosity [m2/s]",
              Dimension::KinematicViscosity, 0.0, 0.0, false);
  s.addDouble("burgers.velocity", 1.0, "traveling-wave speed [m/s]",
              Dimension::Velocity);
  s.addDouble("burgers.x0", 0.0, "traveling-wave x centre at t=0 [m]",
              Dimension::Length);
  s.addDouble("burgers.y0", 0.0, "traveling-wave y centre at t=0 [m]",
              Dimension::Length);
  s.addDouble("burgers.z0", 0.0, "traveling-wave z centre at t=0 [m]",
              Dimension::Length);
  s.addDouble("burgers.source", 0.0, "constant source [m/s2]");
  s.addDouble("burgers.value", 1.0, "constant initial value [m/s]",
              Dimension::Velocity);
  s.addBool("burgers.periodic", false, "periodic x boundaries");
  s.addEnum("burgers.initial", "travelingWave",
            {"travelingWave", "sine", "constant"}, "initial and boundary data");
  s.addEnum("numerics.convection", "muscl", {"upwind", "rusanov", "muscl"},
            "Burgers flux reconstruction");
  s.addEnum("numerics.limiter", "vanLeer", {"minmod", "vanLeer", "superbee"},
            "MUSCL slope limiter");
  return s;
}

std::unique_ptr<Solver> makeSaturnBurger(const Config& cfg) {
  Mesh mesh = makeMeshFromConfig(cfg);
  if (mesh.dimension < 1 || mesh.dimension > 3)
    throw ConfigError("saturnBurger supports structured 1D, 2D and 3D meshes");
  if (mesh.dimension == 2 && mesh.nz != 1)
    throw ConfigError("saturnBurger 2D requires nz = 1");

  BurgersOptions options;
  options.nu = cfg.getDouble("burgers.nu", 0.02);
  options.source = cfg.getDouble("burgers.source", 0.0);
  options.periodic = cfg.getBool("burgers.periodic", false);
  options.convection =
      convectiveSchemeFromString(cfg.getString("numerics.convection", "muscl"));
  options.limiter =
      limiterFromString(cfg.getString("numerics.limiter", "vanLeer"));

  const std::string initial = cfg.getString("burgers.initial", "travelingWave");
  const Real speed = cfg.getDouble("burgers.velocity", 1.0);
  const Real x0 = cfg.getDouble("burgers.x0", 0.0);
  const Real y0 = cfg.getDouble("burgers.y0", 0.0);
  const Real z0 = cfg.getDouble("burgers.z0", 0.0);
  const int dim = mesh.dimension;

  if (initial == "travelingWave") {
    if (!(options.nu > 0))
      throw ConfigError(
          "burgers.initial: travelingWave requires burgers.nu > 0");
    const Real nu = options.nu;
    options.initial = [dim, speed, nu, x0, y0, z0](const Vec3& x) {
      return travelingWave(x, 0, dim, speed, nu, x0, y0, z0);
    };
    options.dirichlet = [dim, speed, nu, x0, y0, z0](const Vec3& x, Real t,
                                                     int) {
      return travelingWave(x, t, dim, speed, nu, x0, y0, z0);
    };
    options.exact = [dim, speed, nu, x0, y0, z0](const Vec3& x, Real t) {
      return travelingWave(x, t, dim, speed, nu, x0, y0, z0);
    };
  } else if (initial == "sine") {
    if (!options.periodic)
      throw ConfigError(
          "burgers.initial: sine requires burgers.periodic: true");
    if (dim == 1) {
      options.initial = [](const Vec3& x) { return -std::sin(kPi * x.x); };
    } else if (dim == 2) {
      options.initial = [](const Vec3& x) {
        return -std::sin(kPi * x.x) * std::sin(kPi * x.y);
      };
    } else {
      options.initial = [](const Vec3& x) {
        return -std::sin(kPi * x.x) * std::sin(kPi * x.y) * std::sin(kPi * x.z);
      };
    }
  } else if (initial == "constant") {
    const Real value = cfg.getDouble("burgers.value", 1.0);
    options.initial = [value](const Vec3&) { return value; };
    if (!options.periodic)
      options.dirichlet = [value](const Vec3&, Real, int) { return value; };
  } else {
    throw ConfigError("unsupported burgers.initial '" + initial + "'");
  }

  return std::make_unique<SaturnBurger>(std::move(mesh), std::move(options));
}

void registerSaturnBurger(SolverRegistry& registry) {
  registry.addCase(
      "saturnBurger", [](const Config& cfg) { return makeSaturnBurger(cfg); },
      [] { return burgersSchema(); });
}

}  // namespace saturn
