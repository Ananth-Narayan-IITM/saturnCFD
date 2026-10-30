#pragma once

#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "core/Types.hpp"
#include "core/Units.hpp"

namespace saturn {

/// A parsed YAML case. Access is dotted-key based and type-checked.
class Config {
 public:
  static Config fromFile(const std::string& path);
  static Config fromString(const std::string& text,
                           const std::string& source = "<string>");

  bool has(const std::string& key) const;
  YAML::Node at(const std::string& key) const;
  const YAML::Node& root() const { return mRoot; }
  const std::string& source() const { return mSource; }

  std::string getString(const std::string& key,
                        const std::string& fallback) const;
  int getInt(const std::string& key, int fallback) const;
  Real getDouble(const std::string& key, Real fallback) const;
  bool getBool(const std::string& key, bool fallback) const;

  /// Validate the `units:` block and return the system. v1.0 accepts SI only;
  /// any other value throws ConfigError. An absent block means SI.
  UnitSystem unitSystem() const;

  /// Dotted paths of every leaf in the YAML tree. Used to reject unknown keys.
  std::vector<std::string> leafKeys() const;

 private:
  YAML::Node mRoot;
  std::string mSource = "<memory>";
};

}  // namespace saturn
