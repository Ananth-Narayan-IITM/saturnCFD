#pragma once

#include "fv/CellField.hpp"
#include "fv/FaceField.hpp"
#include "fv/LocalMesh.hpp"

namespace saturn {

enum class ConvectiveScheme { Upwind, Rusanov };

/// Interpolate a three-component cell velocity to faces and take its normal
/// component. `velocityFaces` is a three-component workspace; `normalVelocity`
/// receives one component. Both are caller-owned.
void faceNormalVelocity(const CellField& velocity, FaceField& velocityFaces,
                        FaceField& normalVelocity, const LocalMesh& mesh);

/// Convective flux per unit area through every face of every owned cell.
///
/// `normalVelocity` is positive along the outward face normal. `phi` is the
/// transported scalar; `flux` receives `u_n * phi_upwind` (upwind) or the
/// Rusanov flux. The caller then applies `divergence(flux, ...)`.
///
/// Upwind is first order. Rusanov adds a diffusive term proportional to
/// |u_n| (phi_N - phi_P), also first order for a smooth field.
void convectiveFlux(const FaceField& normalVelocity, const CellField& phi,
                    FaceField& flux, const LocalMesh& mesh,
                    ConvectiveScheme scheme);

}  // namespace saturn
