#include <sstream>

#include "TestHarness.hpp"
#include "core/Config.hpp"

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

TEST(configReadsNestedValues) {
  const Config cfg = Config::fromString(kYaml);
  CHECKEQ(cfg.getInt("domain.nx", 0), 128);
  CHECKEQ(cfg.getInt("domain.ny", 0), 64);
  CHECKNEAR(cfg.getDouble("domain.lx", 0.0), 2.5, 1e-12);
  CHECK(cfg.getBool("boundary.periodic", false));
  CHECKEQ(cfg.getString("wall0", cfg.getString("boundary.walls", "none")),
          "[left, right]");
}

TEST(configDefaultsWhenMissing) {
  const Config cfg = Config::fromString(kYaml);
  CHECKEQ(cfg.getInt("domain.nz", 7), 7);
  CHECKNEAR(cfg.getDouble("time.dt", 0.25), 0.25, 1e-12);
  CHECK(!cfg.has("time.dt"));
  CHECK(cfg.has("domain.nx"));
}

TEST(configListsParse) {
  const Config cfg = Config::fromString(kYaml);
  const auto l = cfg.getDoubleList("physics.limits", {1.0, 2.0, 3.0});
  CHECKEQ(static_cast<int>(l.size()), 3);
  CHECKNEAR(l[2], 3.0, 1e-12);
}

TEST(configDumpListsQueriedOptions) {
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
