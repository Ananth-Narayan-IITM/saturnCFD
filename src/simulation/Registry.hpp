#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/ConfigSchema.hpp"
#include "simulation/Solver.hpp"

namespace saturn {

/// Static solver registry. A solver registers a factory by name; the CLI and
/// tests create it without a runtime plugin.
///
/// Two factory kinds are supported:
///
/// - `Factory` builds a solver with no case data (unit tests);
/// - `CaseFactory` builds a solver from a validated YAML case (CLI runs).
///
/// A solver registered for the CLI also supplies its `ConfigSchema`, so
/// `-dumpConfig`, `-checkConfig` and `saturnCFD case.yaml` use exactly the same
/// option list.
class SolverRegistry {
 public:
  using Factory = std::function<std::unique_ptr<Solver>()>;
  using CaseFactory = std::function<std::unique_ptr<Solver>(const Config&)>;
  using SchemaFactory = std::function<ConfigSchema()>;

  void add(const std::string& name, Factory factory) {
    requireNew(name, factory);
    mFactories.emplace(name, std::move(factory));
  }

  void addCase(const std::string& name, CaseFactory factory,
               SchemaFactory schema) {
    requireNew(name, factory);
    if (!schema)
      throw std::invalid_argument("SolverRegistry: empty schema factory");
    mCases.emplace(name, CaseEntry{std::move(factory), std::move(schema)});
  }

  bool contains(const std::string& name) const {
    return mFactories.find(name) != mFactories.end() ||
           mCases.find(name) != mCases.end();
  }

  bool hasCase(const std::string& name) const {
    return mCases.find(name) != mCases.end();
  }

  std::unique_ptr<Solver> create(const std::string& name) const {
    auto it = mFactories.find(name);
    if (it != mFactories.end())
      return it->second();
    throw std::invalid_argument("SolverRegistry: solver '" + name +
                                "' needs a case; use create(name, config)");
  }

  std::unique_ptr<Solver> create(const std::string& name,
                                 const Config& cfg) const {
    auto it = mCases.find(name);
    if (it != mCases.end())
      return it->second.factory(cfg);
    throw std::invalid_argument("SolverRegistry: solver '" + name +
                                "' is not runnable from a case");
  }

  /// Option list for a case-driven solver.
  ConfigSchema schema(const std::string& name) const {
    auto it = mCases.find(name);
    if (it == mCases.end())
      throw std::invalid_argument("SolverRegistry: no schema for solver '" +
                                  name + "'");
    return it->second.schema();
  }

  std::vector<std::string> names() const {
    std::vector<std::string> out;
    out.reserve(mFactories.size() + mCases.size());
    for (const auto& entry : mFactories)
      out.push_back(entry.first);
    for (const auto& entry : mCases)
      out.push_back(entry.first);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }

 private:
  struct CaseEntry {
    CaseFactory factory;
    SchemaFactory schema;
  };

  template <typename F>
  void requireNew(const std::string& name, const F& factory) {
    if (name.empty() || !factory)
      throw std::invalid_argument("SolverRegistry: empty name or factory");
    if (contains(name))
      throw std::invalid_argument("SolverRegistry: duplicate solver '" + name +
                                  "'");
  }

  std::unordered_map<std::string, Factory> mFactories;
  std::unordered_map<std::string, CaseEntry> mCases;
};

inline SolverRegistry& solverRegistry() {
  static SolverRegistry registry;
  return registry;
}

}  // namespace saturn
