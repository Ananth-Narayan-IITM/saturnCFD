#pragma once

#include "fv/CellField.hpp"
#include "fv/FaceField.hpp"
#include "fv/LocalMesh.hpp"

namespace saturn {

enum class ConvectiveScheme { Upwind, Rusanov, Muscl };

/// TVD slope limiters for the MUSCL reconstruction.
enum class Limiter { Minmod, VanLeer, Superbee };

/// Interpolate a three-component cell velocity to faces and take its normal
/// component. `velocityFaces` is a three-component workspace; `normalVelocity`
/// receives one component. Both are caller-owned.
void faceNormalVelocity(const CellField& velocity, FaceField& velocityFaces,
                        FaceField& normalVelocity, const LocalMesh& mesh);

/// Convective flux per unit area through every face of every owned cell.
///
/// `normalVelocity` is positive along the outward face normal. `phi` is the
/// transported scalar; `flux` receives `u_n * phi_face`.
///
/// - `Upwind`: first-order upwind face value.
/// - `Rusanov`: scalar Rusanov flux. For one wave speed this is algebraically
///   the upwind flux; it is kept for the system extension.
/// - `Muscl`: limited linear reconstruction of the upwind cell value. Use
///   `limiter` to select minmod, van Leer or superbee.
///
/// Physical-boundary faces use the first-order upwind value; the reconstruction
/// is applied on internal and periodic faces. MUSCL requires two ghost layers.
void convectiveFlux(const FaceField& normalVelocity, const CellField& phi,
                    FaceField& flux, const LocalMesh& mesh,
                    ConvectiveScheme scheme, Limiter limiter = Limiter::Minmod);

/// Simplified Rhie--Chow face-normal velocity.
///
/// A momentum solve supplies the coefficient `V/a_P` in the full method. Until
/// that layer exists, `coefficient` is a caller-supplied scalar:
///
///   u_n,f = average(u)_f . n
///         + coefficient * [ (p_N-p_P)/d_perp - grad(p)_f . n ].
///
/// The bracket vanishes for a linear pressure field and suppresses a
/// checkerboard pressure mode. `velocityFaces` (3 components), `pressureFaces`
/// (1) and `pressureGrad` (3) are caller-owned workspaces.
void rhieChowFlux(const CellField& velocity, const CellField& pressure,
                  FaceField& velocityFaces, FaceField& pressureFaces,
                  CellField& pressureGrad, FaceField& normalVelocity,
                  const LocalMesh& mesh, Real coefficient = 1.0);

}  // namespace saturn
