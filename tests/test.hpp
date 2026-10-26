// Minimal test harness — no external dependency, no magic.
#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace t {

struct Case {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}

inline int& failures() {
  static int f = 0;
  return f;
}

inline int& checks() {
  static int c = 0;
  return c;
}

inline int add(const std::string& name, std::function<void()> fn) {
  registry().push_back(Case{name, std::move(fn)});
  return 0;
}

inline void report(const char* file, int line, const std::string& what,
                   bool ok) {
  ++checks();
  if (!ok) {
    ++failures();
    std::cerr << "FAIL " << file << ":" << line << "  " << what << "\n";
  }
}

inline int runAll() {
  for (auto& c : registry()) {
    const int before = failures();
    c.fn();
    std::cout << (failures() == before ? "  ok   " : "  FAIL ") << c.name
              << "\n";
  }
  std::cout << "\n" << (checks() - failures()) << "/" << checks()
            << " checks passed\n";
  return failures() == 0 ? 0 : 1;
}

}  // namespace t

#define CHECK(cond) \
  ::t::report(__FILE__, __LINE__, std::string("CHECK(") + #cond + ")", (cond))

#define CHECK_NEAR(a, b, tol)                                             \
  ::t::report(__FILE__, __LINE__,                                          \
              std::string("CHECK_NEAR(") + #a + ", " + #b + ", " + #tol + \
                  ")  got " + std::to_string(a) + " vs " + std::to_string(b), \
              std::abs((a) - (b)) <= (tol))

#define CHECK_EQ(a, b)                                            \
  ::t::report(__FILE__, __LINE__,                                 \
              std::string("CHECK_EQ(") + #a + ", " + #b + ")",    \
              (a) == (b))

#define TEST(name)                                    \
  static void name();                                 \
  static int name##_reg = ::t::add(#name, name);      \
  static void name()
