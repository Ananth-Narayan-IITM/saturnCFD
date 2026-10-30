#include <doctest/doctest.h>

#include "core/Error.hpp"
#include "core/Units.hpp"

using namespace saturn;

TEST_CASE("units: SI is accepted") {
  CHECK(parseUnitSystem("SI") == UnitSystem::SI);
  CHECK(parseUnitSystem("si") == UnitSystem::SI);
  CHECK(isSupportedUnitSystem(UnitSystem::SI));
}

TEST_CASE("units: non-SI is rejected") {
  CHECK_THROWS_AS(parseUnitSystem("CGS"), ConfigError);
  CHECK_THROWS_AS(parseUnitSystem("imperial"), ConfigError);
  CHECK_THROWS_AS(parseUnitSystem(""), ConfigError);
}

TEST_CASE("units: names are stable") {
  CHECK(unitSystemName(UnitSystem::SI) == "SI");
  CHECK(dimensionName(Dimension::Velocity) == "velocity");
  CHECK(dimensionName(Dimension::DynamicViscosity) == "dynamic viscosity");
}
