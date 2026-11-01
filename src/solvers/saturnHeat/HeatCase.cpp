#include "solvers/saturnHeat/HeatCase.hpp"

#include <cmath>
#include <utility>

#include "config/CommonSchema.hpp"
#include "core/Error.hpp"
#include "fv/Convection.hpp"
#include "mesh/MeshFactory.hpp"
#include "simulation/Registry.hpp"
#include "solvers/saturnHeat/SaturnHeat.hpp"

namespace saturn {

ConfigSchema heatSchema() {
  ConfigSchema s = commonSchema();
  s.addDouble("heat.alpha", 0.01, "thermal diffusivity [m2/s]",
              Dimension::ThermalDiffusivity, 0.0, 0.0, false);
  s.addDouble("heat.velocityX", 1.0, "prescribed x velocity [m/s]",
              Dimension::Velocity);
  s.addDouble("heat.velocityY", 0.0, "prescribed y velocity [m/s]",
              Dimension::Velocity);
  s.addDouble("heat.velocityZ", 0.0, "prescribed z velocity [m/s]",
              Dimension::Velocity);
  s.addDouble("heat.source", 0.0, "constant source [K/s]");
  s.addBool("heat.periodic", false, "periodic boundaries");
  s.addEnum("heat.initial", "fourier", {"fourier", "constant"},
            "initial and exact data");
  s.addDouble("heat.amplitude", 1.0, "Fourier amplitude [K]",
              Dimension::Temperature);
  s.addDouble("heat.value", 1.0, "constant initial value [K]",
              Dimension::Temperature);
  s.addDouble("heat.kx", 6.283185307179586, "x wave number [1/m]");
  s.addDouble("heat.ky", 6.283185307179586, "y wave number [1/m]");
  s.addDouble("heat.kz", 6.283185307179586, "z wave number [1/m]");
  s.addEnum("numerics.convection", "muscl", {"upwind", "rusanov", "muscl"},
            "convective flux reconstruction");
  s.addEnum("numerics.limiter", "vanLeer", {"minmod", "vanLeer", "superbee"},
            "MUSCL slope limiter");
  return s;
}

std::unique_ptr<Solver> makeSaturnHeat(const Config& cfg) {
  Mesh mesh = makeMeshForSolver(cfg);
  if (mesh.dimension < 1 || mesh.dimension > 3)
    throw ConfigError("saturnHeat supports structured 1D, 2D and 3D meshes");

  HeatOptions options;
  options.alpha = cfg.getDouble("heat.alpha", 0.01);
  options.velocity = {cfg.getDouble("heat.velocityX", 1.0),
                      cfg.getDouble("heat.velocityY", 0.0),
                      cfg.getDouble("heat.velocityZ", 0.0)};
  options.source = cfg.getDouble("heat.source", 0.0);
  options.periodic = cfg.getBool("heat.periodic", false);
  options.convection =
      convectiveSchemeFromString(cfg.getString("numerics.convection", "muscl"));
  options.limiter =
      limiterFromString(cfg.getString("numerics.limiter", "vanLeer"));

  const std::string initial = cfg.getString("heat.initial", "fourier");
  if (initial == "fourier") {
    if (options.source != 0)
      throw ConfigError("saturnHeat: Fourier data requires heat.source = 0");
    const Real amplitude = cfg.getDouble("heat.amplitude", 1.0);
    const Real kx = cfg.getDouble("heat.kx", 6.283185307179586);
    const Real ky = cfg.getDouble("heat.ky", 6.283185307179586);
    const Real kz = cfg.getDouble("heat.kz", 6.283185307179586);
    if (mesh.dimension < 2 && (ky != 0 || kz != 0))
      throw ConfigError(
          "saturnHeat: 1D Fourier data requires heat.ky = heat.kz = 0");
    if (mesh.dimension < 3 && kz != 0)
      throw ConfigError("saturnHeat: 2D Fourier data requires heat.kz = 0");
    const Real k2 = kx * kx + ky * ky + kz * kz;
    if (!(k2 > 0))
      throw ConfigError("saturnHeat: Fourier wave vector must be nonzero");
    const Real omega = options.velocity.x * kx + options.velocity.y * ky +
                       options.velocity.z * kz;
    const Real decay = options.alpha * k2;
    auto exact = [amplitude, kx, ky, kz, omega, decay](const Vec3& x, Real t) {
      return amplitude * std::sin(kx * x.x + ky * x.y + kz * x.z - omega * t) *
             std::exp(-decay * t);
    };
    options.initial = [exact](const Vec3& x) { return exact(x, 0); };
    options.dirichlet = [exact](const Vec3& x, Real t, int) {
      return exact(x, t);
    };
    options.exact = exact;
  } else if (initial == "constant") {
    const Real value = cfg.getDouble("heat.value", 1.0);
    options.initial = [value](const Vec3&) { return value; };
    if (!options.periodic)
      options.dirichlet = [value](const Vec3&, Real, int) { return value; };
    if (!options.periodic)
      options.exact = [value](const Vec3&, Real) { return value; };
  } else {
    throw ConfigError("unsupported heat.initial '" + initial + "'");
  }

  return std::make_unique<SaturnHeat>(std::move(mesh), std::move(options));
}

void registerSaturnHeat(SolverRegistry& registry) {
  registry.addCase(
      "saturnHeat", [](const Config& cfg) { return makeSaturnHeat(cfg); },
      [] { return heatSchema(); });
}

}  // namespace saturn
