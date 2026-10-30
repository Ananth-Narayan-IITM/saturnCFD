#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace saturn {

class SaturnError : public std::runtime_error {
 public:
  explicit SaturnError(std::string message, int exitCode = 1)
      : std::runtime_error(std::move(message)), mExitCode(exitCode) {}

  int exitCode() const noexcept { return mExitCode; }

 private:
  int mExitCode;
};

class ConfigError : public SaturnError {
 public:
  explicit ConfigError(std::string message)
      : SaturnError(std::move(message), 1) {}
};

class MeshError : public SaturnError {
 public:
  explicit MeshError(std::string message)
      : SaturnError(std::move(message), 1) {}
};

class UsageError : public SaturnError {
 public:
  explicit UsageError(std::string message)
      : SaturnError(std::move(message), 2) {}
};

}  // namespace saturn
