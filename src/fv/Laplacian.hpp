#pragma once

#include "fv/CartesianGeometry.hpp"
#include "fv/CellField.hpp"

namespace saturn {

/// Orthogonal (seven-point) Laplacian on a uniform Cartesian block.
///
/// For each owned cell P:
///
///   (L phi)_P = sum_d (phi_{P+e_d} - 2 phi_P + phi_{P-e_d}) / h_d^2
///
/// Ghost values must already be filled (periodic halo or physical BC).
/// Only owned cells are written. `out` must be a distinct field: the kernel
/// reads neighbours while it writes, so in-place application is not valid.
void laplacian(const CellField& phi, CellField& out,
               const CartesianGeometry& geom);

}  // namespace saturn
