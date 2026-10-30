#pragma once

#include <string>
#include <string_view>

namespace saturn {

/// v1.0 accepts SI only. Other systems are rejected, not converted.
enum class UnitSystem { SI };

/// Physical dimension of a scalar option. Used by the config schema to catch
/// obvious mistakes even while only SI is accepted.
enum class Dimension {
  Dimensionless,
  Length,
  Time,
  Mass,
  Temperature,
  Velocity,
  Acceleration,
  Pressure,
  Density,
  DynamicViscosity,
  KinematicViscosity,
  ThermalDiffusivity,
  ThermalConductivity,
  SpecificHeat,
  BodyForce
};

/// Parse a unit-system name. Only "SI" (case-insensitive) is accepted in v1.0.
/// Throws ConfigError with a clear message for anything else.
UnitSystem parseUnitSystem(std::string_view name);

std::string_view unitSystemName(UnitSystem system) noexcept;
std::string_view dimensionName(Dimension dim) noexcept;

/// True only for the v1.0 supported system. Kept explicit so call sites read
/// like a policy check.
bool isSupportedUnitSystem(UnitSystem system) noexcept;

}  // namespace saturn
