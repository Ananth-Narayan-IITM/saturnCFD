#include "config/Config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "core/Error.hpp"

namespace saturn {
namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::vector<std::string> split(const std::string& key) {
  std::vector<std::string> parts;
  std::string current;
  for (char c : key) {
    if (c == '.') {
      if (current.empty())
        throw ConfigError("invalid empty component in key '" + key + "'");
      parts.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  if (current.empty())
    throw ConfigError("invalid trailing dot in key '" + key + "'");
  parts.push_back(current);
  return parts;
}

void collectLeaves(const YAML::Node& node, const std::string& prefix,
                   std::vector<std::string>& out) {
  if (!node || !node.IsMap()) {
    if (!prefix.empty())
      out.push_back(prefix);
    return;
  }
  for (auto it = node.begin(); it != node.end(); ++it) {
    const std::string key = it->first.as<std::string>();
    const std::string path = prefix.empty() ? key : prefix + "." + key;
    if (it->second.IsMap()) {
      collectLeaves(it->second, path, out);
    } else {
      out.push_back(path);
    }
  }
}

}  // namespace

Config Config::fromFile(const std::string& path) {
  std::ifstream in(path);
  if (!in)
    throw ConfigError("cannot open case file '" + path + "'");
  std::stringstream ss;
  ss << in.rdbuf();
  return fromString(ss.str(), path);
}

Config Config::fromString(const std::string& text, const std::string& source) {
  Config cfg;
  cfg.mSource = source;
  try {
    cfg.mRoot = YAML::Load(text);
  } catch (const YAML::Exception& e) {
    throw ConfigError("YAML parse error in '" + source + "': " + e.what());
  }
  if (cfg.mRoot && !cfg.mRoot.IsMap() && !cfg.mRoot.IsNull())
    throw ConfigError("case file '" + source + "' must contain a YAML map");
  return cfg;
}

bool Config::has(const std::string& key) const {
  YAML::Node node = mRoot;
  try {
    for (const auto& part : split(key)) {
      if (!node || !node.IsMap() || !node[part])
        return false;
      node.reset(node[part]);
    }
    return static_cast<bool>(node);
  } catch (const ConfigError&) {
    return false;
  }
}

YAML::Node Config::at(const std::string& key) const {
  YAML::Node node = mRoot;
  for (const auto& part : split(key)) {
    if (!node || !node.IsMap() || !node[part])
      throw ConfigError("missing key '" + key + "' in '" + mSource + "'");
    node.reset(node[part]);
  }
  return node;
}

std::string Config::getString(const std::string& key,
                              const std::string& fallback) const {
  if (!has(key))
    return fallback;
  try {
    const YAML::Node n = at(key);
    if (!n.IsScalar())
      throw ConfigError("key '" + key + "' in '" + mSource +
                        "' must be a string");
    return n.as<std::string>();
  } catch (const ConfigError&) {
    throw;
  } catch (const YAML::Exception& e) {
    throw ConfigError("key '" + key + "' in '" + mSource +
                      "' is not a string: " + e.what());
  }
}

int Config::getInt(const std::string& key, int fallback) const {
  if (!has(key))
    return fallback;
  try {
    return at(key).as<int>();
  } catch (const YAML::Exception& e) {
    throw ConfigError("key '" + key + "' in '" + mSource +
                      "' is not an integer: " + e.what());
  }
}

Real Config::getDouble(const std::string& key, Real fallback) const {
  if (!has(key))
    return fallback;
  try {
    return at(key).as<Real>();
  } catch (const YAML::Exception& e) {
    throw ConfigError("key '" + key + "' in '" + mSource +
                      "' is not a number: " + e.what());
  }
}

bool Config::getBool(const std::string& key, bool fallback) const {
  if (!has(key))
    return fallback;
  const YAML::Node n = at(key);
  if (!n.IsScalar())
    throw ConfigError("key '" + key + "' in '" + mSource +
                      "' is not a boolean");
  const std::string v = lower(n.as<std::string>());
  if (v == "true" || v == "yes" || v == "on" || v == "1")
    return true;
  if (v == "false" || v == "no" || v == "off" || v == "0")
    return false;
  throw ConfigError("key '" + key + "' in '" + mSource +
                    "' is not a boolean: '" + v + "'");
}

UnitSystem Config::unitSystem() const {
  if (!has("units"))
    return UnitSystem::SI;
  const YAML::Node units = at("units");
  if (!units.IsMap())
    throw ConfigError("units in '" + mSource +
                      "' must be a map, e.g. units: { system: SI }");
  for (auto it = units.begin(); it != units.end(); ++it) {
    const std::string key = it->first.as<std::string>();
    if (key != "system")
      throw ConfigError(
          "units." + key +
          " is not supported in v1.0; only units.system: SI is accepted");
  }
  if (!units["system"])
    return UnitSystem::SI;
  try {
    return parseUnitSystem(units["system"].as<std::string>());
  } catch (const ConfigError&) {
    throw;
  } catch (const YAML::Exception& e) {
    throw ConfigError("units.system in '" + mSource +
                      "' is not a string: " + e.what());
  }
}

std::vector<std::string> Config::leafKeys() const {
  std::vector<std::string> out;
  collectLeaves(mRoot, "", out);
  return out;
}

}  // namespace saturn
