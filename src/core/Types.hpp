#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace saturn {

using Real = double;
using Index = int;
using BigIndex = std::int64_t;

struct Vec3 {
  Real x = 0;
  Real y = 0;
  Real z = 0;

  constexpr Vec3() = default;
  constexpr Vec3(Real a, Real b, Real c) : x(a), y(b), z(c) {}

  constexpr Real& operator[](int d) { return d == 0 ? x : (d == 1 ? y : z); }
  constexpr const Real& operator[](int d) const {
    return d == 0 ? x : (d == 1 ? y : z);
  }
};

constexpr Vec3 operator+(const Vec3& a, const Vec3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
constexpr Vec3 operator-(const Vec3& a, const Vec3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
constexpr Vec3 operator*(const Vec3& a, Real s) {
  return {a.x * s, a.y * s, a.z * s};
}
constexpr Vec3 operator*(Real s, const Vec3& a) {
  return a * s;
}
constexpr Vec3 operator/(const Vec3& a, Real s) {
  return {a.x / s, a.y / s, a.z / s};
}
constexpr Vec3 operator-(const Vec3& a) {
  return {-a.x, -a.y, -a.z};
}

constexpr Vec3& operator+=(Vec3& a, const Vec3& b) {
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

constexpr Real dot(const Vec3& a, const Vec3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Real norm(const Vec3& a) {
  return std::sqrt(dot(a, a));
}

inline Vec3 normalized(const Vec3& a) {
  const Real n = norm(a);
  return n > 0 ? a / n : Vec3{};
}

}  // namespace saturn
