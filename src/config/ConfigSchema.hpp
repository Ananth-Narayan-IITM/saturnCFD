#pragma once

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "config/Config.hpp"
#include "core/Units.hpp"

namespace saturn {

enum class OptionType { String, Int, Double, Bool, Enum };

struct Option {
  OptionType type = OptionType::String;
  std::string defaultValue;
  std::string description;
  Dimension dimension = Dimension::Dimensionless;
  bool required = false;
  bool hasRange = false;
  double minValue = 0;
  double maxValue = 0;
  std::vector<std::string> enumValues;
};

/// Typed description of every key a solver or common module reads.
class ConfigSchema {
 public:
  void addString(const std::string& key, const std::string& def,
                 const std::string& desc);
  void addInt(const std::string& key, int def, const std::string& desc,
              int minValue, int maxValue);
  void addDouble(const std::string& key, Real def, const std::string& desc,
                 Dimension dim = Dimension::Dimensionless, Real minValue = 0,
                 Real maxValue = 0, bool hasRange = false);
  void addBool(const std::string& key, bool def, const std::string& desc);
  void addEnum(const std::string& key, const std::string& def,
               const std::vector<std::string>& values, const std::string& desc);
  void require(const std::string& key);

  /// Validate a parsed case. Throws ConfigError on the first problem.
  void validate(const Config& cfg) const;

  /// Print a commented YAML template. `solverName` is written as `solver:`.
  void dump(std::ostream& os, const std::string& solverName) const;

  const std::map<std::string, Option>& options() const { return mOptions; }

 private:
  std::map<std::string, Option> mOptions;
};

}  // namespace saturn
