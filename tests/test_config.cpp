#include <sstream>

#include "core/Config.hpp"
#include "test.hpp"

using namespace saturn;

namespace {
const char* kYaml = R"(
# a comment
domain:
  nx: 128      # trailing comment
  ny: 64
  lx: 2.5
boundary:
  periodic: true
  walls: [left, right]
solver:
  order: 2
)";
}

TEST(config_reads_nested_values) {
  const Config cfg = Config::fromString(kYaml);
  CHECK_EQ(cfg.getInt("domain.nx", 0), 128);
  CHECK_EQ(cfg.getInt("domain.ny", 0), 64);
  CHECK_NEAR(cfg.getDouble("domain.lx", 0.0), 2.5, 1e-12);
  CHECK(cfg.getBool("boundary.periodic", false));
  CHECK_EQ(cfg.getString("wall0", cfg.getString("boundary.walls", "none")), "[left, right]");
}

TEST(config_defaults_when_missing) {
  const Config cfg = Config::fromString(kYaml);
  CHECK_EQ(cfg.getInt("domain.nz", 7), 7);
  CHECK_NEAR(cfg.getDouble("time.dt", 0.25), 0.25, 1e-12);
  CHECK(!cfg.has("time.dt"));
  CHECK(cfg.has("domain.nx"));
}

TEST(config_lists_parse) {
  const Config cfg = Config::fromString(kYaml);
  const auto l = cfg.getDoubleList("physics.limits", {1.0, 2.0, 3.0});
  CHECK_EQ(static_cast<int>(l.size()), 3);
  CHECK_NEAR(l[2], 3.0, 1e-12);
}

TEST(config_dump_lists_queried_options) {
  Config cfg = Config::fromString(kYaml);
  (void)cfg.getInt("domain.nx", 0, "cells in x");
  (void)cfg.getBool("boundary.periodic", false, "periodic everywhere");

  std::ostringstream os;
  cfg.dumpTemplate(os);
  const std::string s = os.str();

  CHECK(s.find("domain:") != std::string::npos);
  CHECK(s.find("cells in x") != std::string::npos);
  CHECK(s.find("periodic") != std::string::npos);
}
