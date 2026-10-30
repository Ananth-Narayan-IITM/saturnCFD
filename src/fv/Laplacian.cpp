#include "fv/Laplacian.hpp"

#include <stdexcept>

namespace saturn {

void laplacian(const CellField& phi, CellField& out,
               const CartesianGeometry& geom) {
  if (phi.ncomp() != out.ncomp())
    throw std::invalid_argument("laplacian: component count mismatch");
  for (int d = 0; d < 3; ++d) {
    if (phi.origin(d) != out.origin(d) || phi.extent(d) != out.extent(d))
      throw std::invalid_argument("laplacian: field layout mismatch");
  }

  const int i0 = phi.origin(0);
  const int j0 = phi.origin(1);
  const int k0 = phi.origin(2);
  const int nx = phi.extent(0);
  const int ny = phi.extent(1);
  const int nz = phi.extent(2);
  const int nc = phi.ncomp();
  const int dim = geom.dim;
  const CartesianGeometry g = geom;
  CellField p = phi;
  CellField o = out;

  Kokkos::parallel_for(
      "saturn::laplacian",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int c = 0; c < nc; ++c) {
          Real sum = 0;
          for (int d = 0; d < 3; ++d) {
            if (d >= dim)
              break;
            const int di = (d == 0) ? 1 : 0;
            const int dj = (d == 1) ? 1 : 0;
            const int dk = (d == 2) ? 1 : 0;
            sum += g.weight(d) *
                   (p(i + di, j + dj, k + dk, c) +
                    p(i - di, j - dj, k - dk, c) - Real(2) * p(i, j, k, c));
          }
          o(i, j, k, c) = sum;
        }
      });
}

}  // namespace saturn
