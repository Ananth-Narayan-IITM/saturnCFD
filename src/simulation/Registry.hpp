#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "simulation/Solver.hpp"

namespace saturn {

/// Static solver registry. A solver registers a factory by name; the CLI and
/// tests create it without a runtime plugin.
class SolverRegistry {
 public:
  using Factory = std::function<std::unique_ptr<Solver>()>;

  void add(const std::string& name, Factory factory) {
    if (name.empty() || !factory)
      throw std::invalid_argument("SolverRegistry: empty name or factory");
    if (mFactories.count(name))
      throw std::invalid_argument("SolverRegistry: duplicate solver '" + name +
                                  "'");
    mFactories.emplace(name, std::move(factory));
  }

  bool contains(const std::string& name) const {
    return mFactories.find(name) != mFactories.end();
  }

  std::unique_ptr<Solver> create(const std::string& name) const {
    auto it = mFactories.find(name);
    if (it == mFactories.end())
      throw std::invalid_argument("SolverRegistry: unknown solver '" + name +
                                  "'");
    return it->second();
  }

  std::vector<std::string> names() const {
    std::vector<std::string> out;
    out.reserve(mFactories.size());
    for (const auto& entry : mFactories)
      out.push_back(entry.first);
    std::sort(out.begin(), out.end());
    return out;
  }

 private:
  std::unordered_map<std::string, Factory> mFactories;
};

inline SolverRegistry& solverRegistry() {
  static SolverRegistry registry;
  return registry;
}

}  // namespace saturn
