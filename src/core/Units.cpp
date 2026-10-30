#include "core/Units.hpp"

#include <algorithm>
#include <cctype>

#include "core/Error.hpp"

namespace saturn {
namespace {

std::string upper(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  return out;
}

}  // namespace

UnitSystem parseUnitSystem(std::string_view name) {
  if (upper(name) == "SI")
    return UnitSystem::SI;
  throw ConfigError("units.system: '" + std::string(name) +
                    "' is not supported in v1.0; only SI is implemented. "
                    "Non-SI conversion is a future extension.");
}

std::string_view unitSystemName(UnitSystem system) noexcept {
  switch (system) {
  case UnitSystem::SI:
    return "SI";
  }
  return "unknown";
}

std::string_view dimensionName(Dimension dim) noexcept {
  switch (dim) {
  case Dimension::Dimensionless:
    return "dimensionless";
  case Dimension::Length:
    return "length";
  case Dimension::Time:
    return "time";
  case Dimension::Mass:
    return "mass";
  case Dimension::Temperature:
    return "temperature";
  case Dimension::Velocity:
    return "velocity";
  case Dimension::Acceleration:
    return "acceleration";
  case Dimension::Pressure:
    return "pressure";
  case Dimension::Density:
    return "density";
  case Dimension::DynamicViscosity:
    return "dynamic viscosity";
  case Dimension::KinematicViscosity:
    return "kinematic viscosity";
  case Dimension::ThermalDiffusivity:
    return "thermal diffusivity";
  case Dimension::ThermalConductivity:
    return "thermal conductivity";
  case Dimension::SpecificHeat:
    return "specific heat";
  case Dimension::BodyForce:
    return "body force";
  }
  return "unknown";
}

bool isSupportedUnitSystem(UnitSystem system) noexcept {
  return system == UnitSystem::SI;
}

}  // namespace saturn
