#pragma once

#include "fv/CellField.hpp"
#include "fv/FaceField.hpp"
#include "fv/LocalMesh.hpp"

namespace saturn {

/// Linear cell-to-face interpolation for every face of every owned cell.
///
/// Internal faces whose two cells are both owned use centroid distances; faces
/// touching a ghost use equal weights. Equal weights are exact on a uniform
/// mesh and for the mirror ghosts used by Dirichlet/Neumann fill. A general
/// non-uniform partition-boundary weighting is a later refinement.
void interpolate(const CellField& phi, FaceField& face, const LocalMesh& mesh);

/// Gauss gradient of a scalar cell field: (1/V) sum_f phi_f A_f n_f.
/// `grad` must have three components. `face` is a caller-owned workspace with
/// one component; it is overwritten.
void gradient(const CellField& phi, FaceField& face, CellField& grad,
              const LocalMesh& mesh);

/// Divergence of a face field: (1/V) sum_f F_f A_f.
///
/// A three-component `face` is a vector, and the flux is `u_f . n_f`. A
/// one-component `face` is already the outward normal flux. `div` has one
/// component. This lets the scalar output of `convectiveFlux` feed directly
/// into the divergence.
void divergence(const FaceField& face, CellField& div, const LocalMesh& mesh);

/// Interpolate a three-component cell vector to `face`, then take divergence.
/// `face` is a caller-owned three-component workspace.
void divergence(const CellField& u, FaceField& face, CellField& div,
                const LocalMesh& mesh);

}  // namespace saturn
