#pragma once

#include "core/Types.hpp"

namespace saturn {

/// Manufactured Stokes solutions used by the `mms` case and the order tests.
///
/// Both variants are divergence-free. `Periodic` is periodic in x, y and z;
/// `Wall` has no-slip walls at y = 0 and y = 1 and is periodic in x and z.
enum class MmsVariant { Periodic, Wall };

Vec3 mmsVelocity(const Vec3& x, int dim, MmsVariant variant);

Real mmsPressure(const Vec3& x, int dim, MmsVariant variant);

/// Momentum source f = -mu lap(u) + grad(p).
Vec3 mmsForce(const Vec3& x, int dim, MmsVariant variant, Real mu);

/// Laplacian of the manufactured pressure, used for the continuity source.
///
/// The discrete continuity equation is div(u) - beta lap(p) = g with
/// beta = stabilization * h_min^2 / mu. Because div(u) = 0, the source is
/// g = -beta lap(p); see `StokesCase.cpp`.
Real mmsPressureLaplacian(const Vec3& x, int dim, MmsVariant variant);

}  // namespace saturn
