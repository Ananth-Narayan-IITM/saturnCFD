#include <doctest/doctest.h>

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "config/CommonSchema.hpp"
#include "config/Config.hpp"
#include "core/Error.hpp"
#include "mesh/CheckMesh.hpp"
#include "mesh/Mesh.hpp"

using namespace saturn;

namespace {

using Expected = std::map<std::string, std::string>;

Expected loadExpected(const std::string& path) {
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error("cannot open expected file: " + path);
  Expected e;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    const auto pos = line.find('=');
    if (pos == std::string::npos)
      continue;
    e[line.substr(0, pos)] = line.substr(pos + 1);
  }
  return e;
}

int asInt(const Expected& e, const std::string& key) {
  const auto it = e.find(key);
  if (it == e.end())
    throw std::runtime_error("missing expected key: " + key);
  return std::stoi(it->second);
}

double asDouble(const Expected& e, const std::string& key) {
  const auto it = e.find(key);
  if (it == e.end())
    throw std::runtime_error("missing expected key: " + key);
  return std::stod(it->second);
}

std::string asString(const Expected& e, const std::string& key) {
  const auto it = e.find(key);
  if (it == e.end())
    throw std::runtime_error("missing expected key: " + key);
  return it->second;
}

void checkAgainst(const MeshReport& r, const std::string& expectedPath) {
  const Expected e = loadExpected(expectedPath);

  CHECK(r.nCells == asInt(e, "cells"));
  CHECK(r.nPoints == asInt(e, "points"));
  CHECK(r.nFaces == asInt(e, "faces"));
  CHECK(r.nInternalFaces == asInt(e, "internalFaces"));
  CHECK(r.nBoundaryFaces == asInt(e, "boundaryFaces"));
  CHECK(static_cast<int>(r.patches.size()) == asInt(e, "patches"));
  CHECK(r.structured == (asString(e, "structured") == "yes"));
  CHECK(r.dimension == asInt(e, "dimension"));
  if (e.count("nx"))
    CHECK(r.nx == asInt(e, "nx"));
  if (e.count("ny"))
    CHECK(r.ny == asInt(e, "ny"));
  if (e.count("nz"))
    CHECK(r.nz == asInt(e, "nz"));
  if (e.count("volumeTotal"))
    CHECK(r.volumeTotal == doctest::Approx(asDouble(e, "volumeTotal")));
  CHECK(r.pass() == (asString(e, "status") == "PASS"));

  if (e.count("errorsContain")) {
    bool found = false;
    for (const auto& err : r.errors)
      if (err.find(e.at("errorsContain")) != std::string::npos)
        found = true;
    CHECK(found);
  }
}

std::string meshPath(const std::string& name) {
  return std::string(SATURN_TEST_DATA_DIR) + "/" + name;
}

std::string expectedPath(const std::string& name) {
  return std::string(SATURN_REGRESSION_DIR) + "/mesh/" + name + ".expected";
}

std::string configPath(const std::string& name) {
  return std::string(SATURN_REGRESSION_DIR) + "/config/" + name;
}

void checkConfigCase(const std::string& name) {
  const Expected e = loadExpected(configPath(name + ".expected"));
  bool passed = true;
  std::string error;
  try {
    const Config cfg = Config::fromFile(configPath(name + ".yaml"));
    commonSchema().validate(cfg);
  } catch (const ConfigError& ex) {
    passed = false;
    error = ex.what();
  }
  CHECK(passed == (asString(e, "status") == "PASS"));
  if (e.count("errorContains"))
    CHECK(error.find(e.at("errorContains")) != std::string::npos);
}

}  // namespace

TEST_CASE("regression: box2x2x2 [regression][mesh]") {
  Mesh mesh = readGmsh(meshPath("box2x2x2.msh"));
  const MeshReport r = checkMesh(mesh, "box2x2x2.msh");
  checkAgainst(r, expectedPath("box2x2x2"));
}

TEST_CASE("regression: thin2d [regression][mesh]") {
  Mesh mesh = readGmsh(meshPath("thin2d.msh"));
  const MeshReport r = checkMesh(mesh, "thin2d.msh");
  checkAgainst(r, expectedPath("thin2d"));
}

TEST_CASE("regression: tetRejected [regression][mesh]") {
  Mesh mesh = readGmsh(meshPath("tet.msh"));
  const MeshReport r = checkMesh(mesh, "tet.msh");
  checkAgainst(r, expectedPath("tetRejected"));
}

TEST_CASE("regression: config siOnly [regression][config]") {
  checkConfigCase("siOnly");
}

TEST_CASE("regression: config cgsRejected [regression][config]") {
  checkConfigCase("cgsRejected");
}

TEST_CASE("regression: config unknownKey [regression][config]") {
  checkConfigCase("unknownKey");
}

TEST_CASE("regression: config templateRoundTrip [regression][config]") {
  const ConfigSchema schema = commonSchema();
  std::ostringstream os;
  schema.dump(os, "common");
  const Config cfg = Config::fromString(os.str(), "<template>");
  CHECK_NOTHROW(schema.validate(cfg));
}
