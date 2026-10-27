/**
 * @file Timer.hpp
 *
 * saturnCFD — wall-clock timers that understand asynchronous execution.
 *
 * Kokkos::Timer alone is a trap on GPUs: it measures the launch, not the work.
 * Every start/stop here fences, so the number means what you think it means.
 */
#pragma once

#include <Kokkos_Core.hpp>

#include <string>

#include "core/Types.hpp"

namespace saturn {

/** @brief Wall-clock timer that fences, so GPU timings mean what they say. */
class Timer {
 public:
  explicit Timer(std::string name = "") : mName(std::move(name)) {}

  void start() {
    ExecSpace().fence();
    mClock.reset();
  }

  void stop() {
    ExecSpace().fence();
    mSeconds += mClock.seconds();
  }

  /// Zero the accumulator without starting.
  void reset() { mSeconds = 0.0; }

  double seconds() const { return mSeconds; }
  const std::string& name() const { return mName; }

 private:
  std::string mName;
  Kokkos::Timer mClock;
  double mSeconds = 0.0;
};

/// RAII convenience: accumulates into an external counter.
class ScopedTimer {
 public:
  explicit ScopedTimer(Timer& t) : mTimer(t) { mTimer.start(); }
  ~ScopedTimer() { mTimer.stop(); }
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

 private:
  Timer& mTimer;
};

}  // namespace saturn
