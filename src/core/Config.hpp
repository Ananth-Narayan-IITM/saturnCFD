// saturnCFD — YAML-subset configuration with self-documenting defaults.
//
// Design goal: at any time a user can run `saturnCFD --dump-config` and get a
// YAML file listing every option the code actually reads, with its default and
// a one-line description. That is achieved by *registering* options as they are
// queried, so the template can never drift from the code.
//
// Supported YAML subset: nested maps by indentation, scalar values, inline
// lists [a, b, c], and comments. No anchors, no multi-line strings.
#pragma once

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace saturn {

class Config {
 public:
  Config() = default;

  static Config fromFile(const std::string& path);
  static Config fromString(const std::string& text);

  bool has(const std::string& key) const;

  std::string getString(const std::string& key, const std::string& def,
                        const std::string& desc = "") const;
  int getInt(const std::string& key, int def, const std::string& desc = "") const;
  double getDouble(const std::string& key, double def,
                   const std::string& desc = "") const;
  bool getBool(const std::string& key, bool def,
               const std::string& desc = "") const;
  std::vector<double> getDoubleList(const std::string& key,
                                    const std::vector<double>& def,
                                    const std::string& desc = "") const;
  std::vector<int> getIntList(const std::string& key,
                              const std::vector<int>& def,
                              const std::string& desc = "") const;

  /// Write a commented YAML template for every option queried so far.
  void dumpTemplate(std::ostream& os) const;

  /// Every key present in the file.
  const std::map<std::string, std::string>& rawValues() const {
    return m_values;
  }

 private:
  struct Reg {
    std::string def;
    std::string desc;
    std::string type;
  };

  void registerOpt(const std::string& key, const std::string& def,
                   const std::string& desc, const std::string& type) const;

  std::map<std::string, std::string> m_values;
  mutable std::map<std::string, Reg> m_registry;
};

}  // namespace saturn
