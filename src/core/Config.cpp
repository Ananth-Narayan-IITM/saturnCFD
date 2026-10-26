#include "core/Config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace saturn {
namespace {

std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

/// Strip a trailing '#' comment that is not inside quotes or brackets.
std::string stripComment(const std::string& s) {
  bool in_s = false, in_d = false;
  for (std::size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (c == '\'' && !in_d) in_s = !in_s;
    else if (c == '"' && !in_s) in_d = !in_d;
    else if (c == '#' && !in_s && !in_d) return s.substr(0, i);
  }
  return s;
}

std::string unquote(const std::string& s) {
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                        (s.front() == '\'' && s.back() == '\'')))
    return s.substr(1, s.size() - 2);
  return s;
}

std::vector<std::string> splitList(const std::string& v) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : v) {
    if (c == ',') {
      out.push_back(trim(cur));
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!trim(cur).empty()) out.push_back(trim(cur));
  return out;
}

}  // namespace

Config Config::fromString(const std::string& text) {
  Config cfg;
  std::istringstream in(text);
  std::string line;

  // Stack of (indent, key) for nested maps.
  std::vector<std::pair<int, std::string>> stack;

  while (std::getline(in, line)) {
    const std::string cleaned = trim(stripComment(line));
    if (cleaned.empty()) continue;

    const int indent = static_cast<int>(line.find_first_not_of(' '));

    // Pop levels that this line is not nested under.
    while (!stack.empty() && indent <= stack.back().first) stack.pop_back();

    std::string prefix;
    for (const auto& kv : stack) prefix += kv.second + ".";
    if (!prefix.empty()) prefix.pop_back();
    if (!prefix.empty()) prefix += ".";

    if (cleaned.rfind("- ", 0) == 0 || cleaned == "-") {
      // List item: attach to the enclosing key as "<key>[]".
      if (stack.empty())
        throw std::runtime_error("Config: list item without a parent key");
      const std::string full = prefix + "[]";
      const std::string item = unquote(trim(cleaned.substr(1)));
      auto it = cfg.m_values.find(full);
      if (it == cfg.m_values.end())
        cfg.m_values[full] = item;
      else
        it->second += "," + item;
      continue;
    }

    const auto colon = cleaned.find(':');
    if (colon == std::string::npos)
      throw std::runtime_error("Config: malformed line: '" + cleaned + "'");

    const std::string key = trim(cleaned.substr(0, colon));
    std::string value = trim(cleaned.substr(colon + 1));

    if (value.empty()) {
      stack.emplace_back(indent, key);
      continue;
    }
    cfg.m_values[prefix + key] = unquote(value);
  }
  return cfg;
}

Config Config::fromFile(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("Config: cannot open '" + path + "'");
  std::stringstream ss;
  ss << f.rdbuf();
  return fromString(ss.str());
}

bool Config::has(const std::string& key) const {
  return m_values.find(key) != m_values.end();
}

void Config::registerOpt(const std::string& key, const std::string& def,
                         const std::string& desc,
                         const std::string& type) const {
  auto it = m_registry.find(key);
  if (it == m_registry.end())
    m_registry[key] = Reg{def, desc, type};
  else if (!desc.empty())
    it->second.desc = desc;
}

std::string Config::getString(const std::string& key, const std::string& def,
                              const std::string& desc) const {
  registerOpt(key, def, desc, "string");
  auto it = m_values.find(key);
  return (it == m_values.end()) ? def : it->second;
}

int Config::getInt(const std::string& key, int def,
                   const std::string& desc) const {
  registerOpt(key, std::to_string(def), desc, "int");
  auto it = m_values.find(key);
  if (it == m_values.end()) return def;
  try {
    return std::stoi(it->second);
  } catch (const std::exception&) {
    throw std::runtime_error("Config: key '" + key + "' is not an integer: '" +
                             it->second + "'");
  }
}

double Config::getDouble(const std::string& key, double def,
                         const std::string& desc) const {
  std::ostringstream os;
  os << std::setprecision(17) << def;
  registerOpt(key, os.str(), desc, "double");
  auto it = m_values.find(key);
  if (it == m_values.end()) return def;
  try {
    return std::stod(it->second);
  } catch (const std::exception&) {
    throw std::runtime_error("Config: key '" + key + "' is not a number: '" +
                             it->second + "'");
  }
}

bool Config::getBool(const std::string& key, bool def,
                     const std::string& desc) const {
  registerOpt(key, def ? "true" : "false", desc, "bool");
  auto it = m_values.find(key);
  if (it == m_values.end()) return def;
  const std::string v = lower(it->second);
  if (v == "true" || v == "yes" || v == "on" || v == "1") return true;
  if (v == "false" || v == "no" || v == "off" || v == "0") return false;
  throw std::runtime_error("Config: key '" + key + "' is not a boolean: '" +
                           it->second + "'");
}

std::vector<double> Config::getDoubleList(const std::string& key,
                                          const std::vector<double>& def,
                                          const std::string& desc) const {
  std::ostringstream os;
  os << "[";
  for (std::size_t i = 0; i < def.size(); ++i) {
    if (i) os << ", ";
    os << std::setprecision(17) << def[i];
  }
  os << "]";
  registerOpt(key, os.str(), desc, "list<double>");

  auto it = m_values.find(key);
  if (it == m_values.end()) return def;

  std::string v = trim(it->second);
  if (!v.empty() && v.front() == '[') v = v.substr(1);
  if (!v.empty() && v.back() == ']') v.pop_back();

  std::vector<double> out;
  for (const auto& tok : splitList(v)) out.push_back(std::stod(tok));
  return out;
}

std::vector<int> Config::getIntList(const std::string& key,
                                    const std::vector<int>& def,
                                    const std::string& desc) const {
  std::ostringstream os;
  os << "[";
  for (std::size_t i = 0; i < def.size(); ++i) {
    if (i) os << ", ";
    os << def[i];
  }
  os << "]";
  registerOpt(key, os.str(), desc, "list<int>");

  auto it = m_values.find(key);
  if (it == m_values.end()) return def;

  std::string v = trim(it->second);
  if (!v.empty() && v.front() == '[') v = v.substr(1);
  if (!v.empty() && v.back() == ']') v.pop_back();

  std::vector<int> out;
  for (const auto& tok : splitList(v)) out.push_back(std::stoi(tok));
  return out;
}

int Config::reportUnknown(std::ostream& os) const {
  int n = 0;
  for (const auto& kv : m_values) {
    if (m_registry.find(kv.first) == m_registry.end()) {
      os << "warning: key '" << kv.first << "' is not read by this solver"
         << " (value: \"" << kv.second << "\")\n";
      ++n;
    }
  }
  return n;
}

void Config::dumpTemplate(std::ostream& os) const {
  os << "# saturnCFD configuration template\n";
  os << "# Generated by --dump-config. Every entry below is read by the solver.\n\n";

  std::string prevPrefix;
  for (const auto& kv : m_registry) {
    const std::string& key = kv.first;
    const Reg& r = kv.second;

    // Split "a.b.c" into group prefix and leaf.
    const auto pos = key.rfind('.');
    const std::string group = (pos == std::string::npos) ? "" : key.substr(0, pos);
    const std::string leaf = (pos == std::string::npos) ? key : key.substr(pos + 1);

    if (group != prevPrefix) {
      if (!prevPrefix.empty()) os << "\n";
      os << group << ":\n";
      prevPrefix = group;
    }
    os << "  " << leaf << ": " << r.def;
    if (!r.desc.empty()) os << "   # " << r.desc;
    os << "  [" << r.type << "]\n";
  }
}

}  // namespace saturn
