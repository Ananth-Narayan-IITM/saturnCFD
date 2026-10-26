// saturnCFD — wall-clock timers that understand asynchronous execution.
//
// Kokkos::Timer alone is a trap on GPUs: it measures the launch, not the work.
// Every start/stop here fences, so the number means what you think it means.
#pragma once

#include <Kokkos_Core.hpp>

#include <string>

#include "core/Types.hpp"

namespace saturn {

class Timer {
 public:
  explicit Timer(std::string name = "") : m_name(std::move(name)) {}

  void start() {
    ExecSpace().fence();
    m_t.reset();
  }

  void stop() {
    ExecSpace().fence();
    m_seconds += m_t.seconds();
  }

  /// Zero the accumulator without starting.
  void reset() { m_seconds = 0.0; }

  double seconds() const { return m_seconds; }
  const std::string& name() const { return m_name; }

 private:
  std::string m_name;
  Kokkos::Timer m_t;
  double m_seconds = 0.0;
};

/// RAII convenience: accumulates into an external counter.
class ScopedTimer {
 public:
  explicit ScopedTimer(Timer& t) : m_timer(t) { m_timer.start(); }
  ~ScopedTimer() { m_timer.stop(); }
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

 private:
  Timer& m_timer;
};

}  // namespace saturn
