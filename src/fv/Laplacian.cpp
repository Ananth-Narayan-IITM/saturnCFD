#include "fv/Laplacian.hpp"

#include <stdexcept>

#include "fv/Operators.hpp"

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

void laplacian(const CellField& phi, FaceField& face, CellField& grad,
               CellField& out, const LocalMesh& mesh) {
  if (phi.ncomp() != 1)
    throw std::invalid_argument("laplacian: phi must have one component");
  if (face.ncomp() != 1)
    throw std::invalid_argument(
        "laplacian: face workspace must have one component");
  if (grad.ncomp() != 3)
    throw std::invalid_argument("laplacian: grad must have three components");
  if (out.ncomp() != 1)
    throw std::invalid_argument("laplacian: out must have one component");
  if (phi.nGrow() != mesh.nGrow() || face.nGrow() != mesh.nGrow() ||
      grad.nGrow() != mesh.nGrow() || out.nGrow() != mesh.nGrow())
    throw std::invalid_argument("laplacian: ghost width mismatch");
  for (int d = 0; d < 3; ++d) {
    if (phi.origin(d) != mesh.origin(d) || phi.extent(d) != mesh.extent(d) ||
        face.origin(d) != mesh.origin(d) || face.extent(d) != mesh.extent(d) ||
        grad.origin(d) != mesh.origin(d) || grad.extent(d) != mesh.extent(d) ||
        out.origin(d) != mesh.origin(d) || out.extent(d) != mesh.extent(d))
      throw std::invalid_argument(
          "laplacian: field layout does not match LocalMesh");
  }

  gradient(phi, face, grad, mesh);

  const LocalMesh::DeviceData m = mesh.device();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  CellField p = phi;
  CellField g = grad;
  CellField o = out;
  FaceField f = face;

  Kokkos::parallel_for(
      "saturn::laplacian_localmesh",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        Real sum = 0;
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          int di = 0, dj = 0, dk = 0;
          faceDirectionOffset(dir, di, dj, dk);
          const int ni = i + di;
          const int nj = j + dj;
          const int nk = k + dk;
          const Vec3 n = m.faceNormal(i, j, k, dir);
          const Real area = m.faceArea(i, j, k, dir);
          const Real phiP = p(i, j, k, 0);
          const Real phiN = p(ni, nj, nk, 0);

          Real flux = 0;
          const bool physicalBoundary =
              (m.boundaryPatch(i, j, k, dir) >= 0) && !m.periodic[dir / 2];
          if (physicalBoundary) {
            // Use the interpolated boundary-face value and correct for the
            // tangential offset between the owner centroid and the face
            // centroid. On an orthogonal mesh t is zero.
            const Vec3 xP = m.cellCentroid(i, j, k);
            const Vec3 xf = m.faceCentroid(i, j, k, dir);
            const Vec3 dvec = xf - xP;
            const Real dPerp = dot(dvec, n);
            const Vec3 t = dvec - n * dPerp;
            const Vec3 gradP = {g(i, j, k, 0), g(i, j, k, 1), g(i, j, k, 2)};
            const Real denom =
                (dPerp > 0) ? dPerp : m.faceDistance(i, j, k, dir);
            flux = area * ((f(i, j, k, dir, 0) - phiP) - dot(t, gradP)) / denom;
          } else {
            const Vec3 xP = m.cellCentroid(i, j, k);
            const Vec3 xN = m.cellCentroid(ni, nj, nk);
            const Vec3 dvec = xN - xP;
            const Real dPerp = dot(dvec, n);

            Vec3 gradF{};
            if (m.owned(ni, nj, nk)) {
              const Vec3 xf = m.faceCentroid(i, j, k, dir);
              const Vec3 dPv = xf - xP;
              const Vec3 dNv = xf - xN;
              const Real dP = Kokkos::sqrt(dot(dPv, dPv));
              const Real dN = Kokkos::sqrt(dot(dNv, dNv));
              const Real total = dP + dN;
              const Real wP = (total > 0) ? dN / total : Real(0.5);
              const Real wN = (total > 0) ? dP / total : Real(0.5);
              gradF = {wP * g(i, j, k, 0) + wN * g(ni, nj, nk, 0),
                       wP * g(i, j, k, 1) + wN * g(ni, nj, nk, 1),
                       wP * g(i, j, k, 2) + wN * g(ni, nj, nk, 2)};
            } else {
              // Partition/periodic ghost: the owner gradient is the best
              // available face gradient. It is exact for a linear field.
              gradF = {g(i, j, k, 0), g(i, j, k, 1), g(i, j, k, 2)};
            }

            const Vec3 gradTang = gradF - n * dot(gradF, n);
            const Real denom =
                (dPerp > 0) ? dPerp : Real(2) * m.faceDistance(i, j, k, dir);
            flux = area * ((phiN - phiP) - dot(dvec, gradTang)) / denom;
          }
          sum += flux;
        }
        o(i, j, k, 0) = sum / m.cellVolume(i, j, k);
      });
}

}  // namespace saturn
