#include <doctest/doctest.h>

#include <sstream>

#include "config/Config.hpp"
#include "config/ConfigSchema.hpp"
#include "core/Error.hpp"
#include "core/Units.hpp"

using namespace saturn;

namespace {

ConfigSchema testSchema() {
  ConfigSchema s;
  s.addString("solver", "", "solver name");
  s.addEnum("units.system", "SI", {"SI"}, "unit system");
  s.addString("mesh.file", "", "mesh file");
  s.addInt("time.maxSteps", 10000, "step limit", 1, 1000000);
  s.addDouble("time.cfl", 0.4, "CFL", Dimension::Dimensionless, 0.0, 1.0, true);
  s.require("solver");
  return s;
}

}  // namespace

TEST_CASE("config: SI and absent units are accepted") {
  const ConfigSchema schema = testSchema();

  const Config none = Config::fromString("solver: test\n");
  CHECK(none.unitSystem() == UnitSystem::SI);
  CHECK_NOTHROW(schema.validate(none));

  const Config si = Config::fromString("solver: test\nunits:\n  system: SI\n");
  CHECK(si.unitSystem() == UnitSystem::SI);
  CHECK_NOTHROW(schema.validate(si));
}

TEST_CASE("config: non-SI units are rejected") {
  const ConfigSchema schema = testSchema();

  const Config cgs =
      Config::fromString("solver: test\nunits:\n  system: CGS\n");
  CHECK_THROWS_AS(schema.validate(cgs), ConfigError);

  const Config mm = Config::fromString("solver: test\nunits:\n  length: mm\n");
  CHECK_THROWS_AS(schema.validate(mm), ConfigError);
}

TEST_CASE("config: unknown keys and ranges are rejected") {
  const ConfigSchema schema = testSchema();

  const Config unknown = Config::fromString("solver: test\ntypo: 1\n");
  CHECK_THROWS_AS(schema.validate(unknown), ConfigError);

  const Config cfl = Config::fromString("solver: test\ntime:\n  cfl: 2.0\n");
  CHECK_THROWS_AS(schema.validate(cfl), ConfigError);

  const Config bad = Config::fromString("solver: test\ntime:\n  maxSteps: 0\n");
  CHECK_THROWS_AS(schema.validate(bad), ConfigError);
}

TEST_CASE("config: missing required key is rejected") {
  const ConfigSchema schema = testSchema();
  const Config cfg = Config::fromString("time:\n  cfl: 0.2\n");
  CHECK_THROWS_AS(schema.validate(cfg), ConfigError);
}

TEST_CASE("config: template contains registered keys") {
  const ConfigSchema schema = testSchema();
  std::ostringstream os;
  schema.dump(os, "test");
  const std::string text = os.str();
  CHECK(text.find("solver: test") != std::string::npos);
  CHECK(text.find("maxSteps") != std::string::npos);
  CHECK(text.find("system: SI") != std::string::npos);
}
