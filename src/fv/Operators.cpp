#include "fv/Operators.hpp"

#include <stdexcept>

namespace saturn {
namespace {

void requireOwnedLayout(const CellField& field, const LocalMesh& mesh) {
  if (field.nGrow() != mesh.nGrow())
    throw std::invalid_argument("operators: CellField ghost width mismatch");
  for (int d = 0; d < 3; ++d)
    if (field.origin(d) != mesh.origin(d) || field.extent(d) != mesh.extent(d))
      throw std::invalid_argument(
          "operators: CellField layout does not match LocalMesh");
}

void requireOwnedLayout(const FaceField& field, const LocalMesh& mesh) {
  if (field.nGrow() != mesh.nGrow())
    throw std::invalid_argument("operators: FaceField ghost width mismatch");
  for (int d = 0; d < 3; ++d)
    if (field.origin(d) != mesh.origin(d) || field.extent(d) != mesh.extent(d))
      throw std::invalid_argument(
          "operators: FaceField layout does not match LocalMesh");
}

}  // namespace

void interpolate(const CellField& phi, FaceField& face, const LocalMesh& mesh) {
  requireOwnedLayout(phi, mesh);
  requireOwnedLayout(face, mesh);
  if (phi.ncomp() != face.ncomp())
    throw std::invalid_argument("interpolate: component count mismatch");

  const LocalMesh::DeviceData m = mesh.device();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  const int nc = phi.ncomp();
  CellField p = phi;
  FaceField f = face;

  Kokkos::parallel_for(
      "saturn::interpolate",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          int di = 0, dj = 0, dk = 0;
          faceDirectionOffset(dir, di, dj, dk);
          const int ni = i + di;
          const int nj = j + dj;
          const int nk = k + dk;

          Real wP = Real(0.5);
          Real wN = Real(0.5);
          if (m.boundaryPatch(i, j, k, dir) < 0 && m.owned(ni, nj, nk)) {
            const Vec3 xf = m.faceCentroid(i, j, k, dir);
            const Vec3 xP = m.cellCentroid(i, j, k);
            const Vec3 xN = m.cellCentroid(ni, nj, nk);
            const Vec3 dPv = xf - xP;
            const Vec3 dNv = xf - xN;
            const Real dP = Kokkos::sqrt(dot(dPv, dPv));
            const Real dN = Kokkos::sqrt(dot(dNv, dNv));
            const Real total = dP + dN;
            if (total > 0) {
              wP = dN / total;
              wN = dP / total;
            }
          }

          for (int c = 0; c < nc; ++c)
            f(i, j, k, dir, c) = wP * p(i, j, k, c) + wN * p(ni, nj, nk, c);
        }
      });
}

void gradient(const CellField& phi, FaceField& face, CellField& grad,
              const LocalMesh& mesh) {
  requireOwnedLayout(phi, mesh);
  requireOwnedLayout(face, mesh);
  requireOwnedLayout(grad, mesh);
  if (phi.ncomp() != 1)
    throw std::invalid_argument("gradient: input must have one component");
  if (face.ncomp() != 1)
    throw std::invalid_argument("gradient: workspace must have one component");
  if (grad.ncomp() != 3)
    throw std::invalid_argument("gradient: output must have three components");

  interpolate(phi, face, mesh);

  const LocalMesh::DeviceData m = mesh.device();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  FaceField f = face;
  CellField g = grad;

  Kokkos::parallel_for(
      "saturn::gradient",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        Vec3 sum{};
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          const Vec3 n = m.faceNormal(i, j, k, dir);
          const Real area = m.faceArea(i, j, k, dir);
          sum += n * (area * f(i, j, k, dir, 0));
        }
        const Real invVolume = Real(1) / m.cellVolume(i, j, k);
        g(i, j, k, 0) = sum.x * invVolume;
        g(i, j, k, 1) = sum.y * invVolume;
        g(i, j, k, 2) = sum.z * invVolume;
      });
}

void divergence(const FaceField& face, CellField& div, const LocalMesh& mesh) {
  requireOwnedLayout(face, mesh);
  requireOwnedLayout(div, mesh);
  if (face.ncomp() != 3)
    throw std::invalid_argument(
        "divergence: face field must have 3 components");
  if (div.ncomp() != 1)
    throw std::invalid_argument("divergence: output must have one component");

  const LocalMesh::DeviceData m = mesh.device();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  FaceField f = face;
  CellField d = div;

  Kokkos::parallel_for(
      "saturn::divergence",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        Real sum = 0;
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          const Vec3 n = m.faceNormal(i, j, k, dir);
          const Real area = m.faceArea(i, j, k, dir);
          const Vec3 u = {f(i, j, k, dir, 0), f(i, j, k, dir, 1),
                          f(i, j, k, dir, 2)};
          sum += area * dot(u, n);
        }
        d(i, j, k, 0) = sum / m.cellVolume(i, j, k);
      });
}

void divergence(const CellField& u, FaceField& face, CellField& div,
                const LocalMesh& mesh) {
  requireOwnedLayout(u, mesh);
  requireOwnedLayout(face, mesh);
  requireOwnedLayout(div, mesh);
  if (u.ncomp() != 3)
    throw std::invalid_argument(
        "divergence: cell field must have 3 components");
  if (face.ncomp() != 3)
    throw std::invalid_argument("divergence: workspace must have 3 components");
  interpolate(u, face, mesh);
  divergence(face, div, mesh);
}

}  // namespace saturn
