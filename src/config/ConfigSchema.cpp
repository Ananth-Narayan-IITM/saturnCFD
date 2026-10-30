#include "config/ConfigSchema.hpp"

#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>

#include "core/Error.hpp"

namespace saturn {
namespace {

std::string formatDouble(Real v) {
  std::ostringstream os;
  os << std::setprecision(17) << v;
  return os.str();
}

std::string quoted(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\')
      out += '\\';
    out += c;
  }
  out += '"';
  return out;
}

std::string formatDefault(const Option& o) {
  if (o.type == OptionType::String)
    return quoted(o.defaultValue);
  return o.defaultValue;
}

std::string typeName(OptionType t) {
  switch (t) {
  case OptionType::String:
    return "string";
  case OptionType::Int:
    return "int";
  case OptionType::Double:
    return "double";
  case OptionType::Bool:
    return "bool";
  case OptionType::Enum:
    return "enum";
  }
  return "unknown";
}

bool inEnum(const std::string& v, const std::vector<std::string>& values) {
  for (const auto& x : values)
    if (x == v)
      return true;
  return false;
}

}  // namespace

void ConfigSchema::addString(const std::string& key, const std::string& def,
                             const std::string& desc) {
  Option o;
  o.type = OptionType::String;
  o.defaultValue = def;
  o.description = desc;
  mOptions[key] = o;
}

void ConfigSchema::addInt(const std::string& key, int def,
                          const std::string& desc, int minValue, int maxValue) {
  Option o;
  o.type = OptionType::Int;
  o.defaultValue = std::to_string(def);
  o.description = desc;
  o.hasRange = true;
  o.minValue = static_cast<double>(minValue);
  o.maxValue = static_cast<double>(maxValue);
  mOptions[key] = o;
}

void ConfigSchema::addDouble(const std::string& key, Real def,
                             const std::string& desc, Dimension dim,
                             Real minValue, Real maxValue, bool hasRange) {
  Option o;
  o.type = OptionType::Double;
  o.defaultValue = formatDouble(def);
  o.description = desc;
  o.dimension = dim;
  o.hasRange = hasRange;
  o.minValue = minValue;
  o.maxValue = maxValue;
  mOptions[key] = o;
}

void ConfigSchema::addBool(const std::string& key, bool def,
                           const std::string& desc) {
  Option o;
  o.type = OptionType::Bool;
  o.defaultValue = def ? "true" : "false";
  o.description = desc;
  mOptions[key] = o;
}

void ConfigSchema::addEnum(const std::string& key, const std::string& def,
                           const std::vector<std::string>& values,
                           const std::string& desc) {
  Option o;
  o.type = OptionType::Enum;
  o.defaultValue = def;
  o.description = desc;
  o.enumValues = values;
  mOptions[key] = o;
}

void ConfigSchema::require(const std::string& key) {
  auto it = mOptions.find(key);
  if (it == mOptions.end())
    throw std::logic_error("ConfigSchema::require: key not registered: " + key);
  it->second.required = true;
}

void ConfigSchema::validate(const Config& cfg) const {
  // Units first: non-SI is rejected before any value is interpreted.
  (void)cfg.unitSystem();

  for (const auto& key : cfg.leafKeys()) {
    if (key == "units.system") {
      if (mOptions.find(key) == mOptions.end())
        throw ConfigError("units.system is not declared by this schema");
      continue;
    }
    if (mOptions.find(key) == mOptions.end())
      throw ConfigError("unknown key '" + key + "' in '" + cfg.source() + "'");
  }

  for (const auto& [key, opt] : mOptions) {
    if (key == "units.system")
      continue;
    if (!cfg.has(key)) {
      if (opt.required)
        throw ConfigError("missing required key '" + key + "' in '" +
                          cfg.source() + "'");
      continue;
    }

    switch (opt.type) {
    case OptionType::String:
      (void)cfg.getString(key, "");
      break;
    case OptionType::Bool:
      (void)cfg.getBool(key, false);
      break;
    case OptionType::Int: {
      const int v = cfg.getInt(key, 0);
      if (opt.hasRange && (v < opt.minValue || v > opt.maxValue))
        throw ConfigError("key '" + key + "' is out of range");
      break;
    }
    case OptionType::Double: {
      const Real v = cfg.getDouble(key, 0);
      if (opt.hasRange && (v < opt.minValue || v > opt.maxValue))
        throw ConfigError("key '" + key + "' is out of range");
      break;
    }
    case OptionType::Enum: {
      const std::string v = cfg.getString(key, "");
      if (!inEnum(v, opt.enumValues)) {
        std::string allowed;
        for (std::size_t i = 0; i < opt.enumValues.size(); ++i) {
          if (i)
            allowed += " | ";
          allowed += opt.enumValues[i];
        }
        throw ConfigError("key '" + key + "' has invalid value '" + v +
                          "' (expected " + allowed + ")");
      }
      break;
    }
    }
  }
}

void ConfigSchema::dump(std::ostream& os, const std::string& solverName) const {
  os << "solver: " << solverName << "\n\n";

  std::string previousGroup;
  for (const auto& [key, opt] : mOptions) {
    if (key == "solver")
      continue;
    const auto pos = key.rfind('.');
    const std::string group =
        pos == std::string::npos ? "" : key.substr(0, pos);
    const std::string leaf =
        pos == std::string::npos ? key : key.substr(pos + 1);

    if (group != previousGroup) {
      if (!previousGroup.empty())
        os << "\n";
      if (!group.empty())
        os << group << ":\n";
      previousGroup = group;
    }

    const std::string indent = group.empty() ? "" : "  ";
    os << indent << leaf << ": " << formatDefault(opt);
    if (!opt.description.empty())
      os << "   # " << opt.description;
    if (opt.type == OptionType::Enum) {
      os << "  [";
      for (std::size_t i = 0; i < opt.enumValues.size(); ++i) {
        if (i)
          os << "|";
        os << opt.enumValues[i];
      }
      os << "]";
    } else {
      os << "  [" << typeName(opt.type) << "]";
    }
    os << "\n";
  }
}

}  // namespace saturn
