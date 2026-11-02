#include "solvers/saturnStokes/StokesMms.hpp"

#include <cmath>

namespace saturn {
namespace {

constexpr Real kPi = 3.14159265358979323846264338327950288;

}  // namespace

Vec3 mmsVelocity(const Vec3& x, int dim, MmsVariant variant) {
  const Real k = 2 * kPi;
  if (variant == MmsVariant::Periodic) {
    const Real zc = (dim == 3) ? std::cos(k * x.z) : Real(1);
    return {std::sin(k * x.x) * std::cos(k * x.y) * zc,
            -std::cos(k * x.x) * std::sin(k * x.y) * zc, 0};
  }
  const Real zs = (dim == 3) ? std::sin(k * x.z) : Real(1);
  return {kPi * std::sin(k * x.y) * std::sin(k * x.x) * zs,
          -2 * kPi * std::sin(kPi * x.y) * std::sin(kPi * x.y) *
              std::cos(k * x.x) * zs,
          0};
}

Real mmsPressure(const Vec3& x, int dim, MmsVariant variant) {
  const Real k = 2 * kPi;
  if (variant == MmsVariant::Periodic)
    return std::sin(k * x.x) * std::sin(k * x.y) *
           ((dim == 3) ? std::sin(k * x.z) : Real(1));
  return std::cos(k * x.x) * std::cos(kPi * x.y) *
         ((dim == 3) ? std::cos(k * x.z) : Real(1));
}

Vec3 mmsForce(const Vec3& x, int dim, MmsVariant variant, Real mu) {
  const Real k = 2 * kPi;
  const Real k2 = k * k;
  const Vec3 u = mmsVelocity(x, dim, variant);
  if (variant == MmsVariant::Periodic) {
    const Real lapFactor = (dim == 3) ? Real(3) : Real(2);
    Vec3 f;
    f.x = lapFactor * mu * k2 * u.x +
          k * std::cos(k * x.x) * std::sin(k * x.y) *
              ((dim == 3) ? std::sin(k * x.z) : Real(1));
    f.y = lapFactor * mu * k2 * u.y +
          k * std::sin(k * x.x) * std::cos(k * x.y) *
              ((dim == 3) ? std::sin(k * x.z) : Real(1));
    f.z = (dim == 3)
              ? k * std::sin(k * x.x) * std::sin(k * x.y) * std::cos(k * x.z)
              : Real(0);
    return f;
  }

  // Wall variant: u is a product of sines in every direction, so
  // lap(u) = -dim k^2 u. The v component is a product of sin^2(pi y),
  // cos(k x) and sin(k z); its Laplacian is worked out analytically below.
  const Real zs = (dim == 3) ? std::sin(k * x.z) : Real(1);
  const Real zc = (dim == 3) ? std::cos(k * x.z) : Real(1);
  const Real cx = std::cos(k * x.x);
  const Real sx = std::sin(k * x.x);
  const Real spy = std::sin(kPi * x.y);
  const Real cy = std::cos(k * x.y);

  const Real lapU = -((dim == 3) ? Real(3) : Real(2)) * k2 * u.x;
  const Real lapV = (dim == 3) ? kPi * k2 * (2 - 3 * cy) * cx * zs
                               : kPi * k2 * (1 - 2 * cy) * cx;

  Vec3 f;
  f.x = -mu * lapU - k * sx * std::cos(kPi * x.y) * zc;
  f.y = -mu * lapV - kPi * cx * spy * zc;
  f.z =
      (dim == 3) ? -k * cx * std::cos(kPi * x.y) * std::sin(k * x.z) : Real(0);
  return f;
}

Real mmsPressureLaplacian(const Vec3& x, int dim, MmsVariant variant) {
  const Real k = 2 * kPi;
  const Real p = mmsPressure(x, dim, variant);
  if (variant == MmsVariant::Periodic)
    return -((dim == 3) ? Real(3) : Real(2)) * k * k * p;
  const Real factor = 5 * kPi * kPi + ((dim == 3) ? k * k : Real(0));
  return -factor * p;
}

}  // namespace saturn
