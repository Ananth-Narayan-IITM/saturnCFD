#pragma once

#include "fv/CartesianGeometry.hpp"
#include "fv/CellField.hpp"
#include "fv/FaceField.hpp"
#include "fv/LocalMesh.hpp"

namespace saturn {

/// Orthogonal (seven-point) Laplacian on a uniform Cartesian block.
///
/// For each owned cell P:
///
///   (L phi)_P = sum_d (phi_{P+e_d} - 2 phi_P + phi_{P-e_d}) / h_d^2
///
/// Ghost values must already be filled (periodic halo or physical BC).
/// Only owned cells are written. `out` must be a distinct field.
void laplacian(const CellField& phi, CellField& out,
               const CartesianGeometry& geom);

/// Non-orthogonal corrected Laplacian on a structured LocalMesh.
///
/// The face-normal gradient is
///
///   grad(phi)_f . n = [(phi_N - phi_P) - d . grad_tang(phi)_f] / (d . n)
///
/// where d = x_N - x_P and grad_tang is the tangential part of the
/// interpolated cell gradient. On an orthogonal mesh the correction vanishes.
///
/// `face` (one component) and `grad` (three components) are caller-owned
/// workspaces and are overwritten. `out` must be a distinct field.
///
/// Internal owned-owned faces use the interpolated cell gradient. Faces that
/// touch a partition or periodic ghost use the owner gradient; the ghost
/// centroid is available in LocalMesh. Physical-boundary faces use the mirror
/// ghost at 2d and no correction.
void laplacian(const CellField& phi, FaceField& face, CellField& grad,
               CellField& out, const LocalMesh& mesh);

}  // namespace saturn
