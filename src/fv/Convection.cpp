#include "fv/Convection.hpp"

#include <stdexcept>

#include "fv/Operators.hpp"

namespace saturn {
namespace {

void requireOwnedLayout(const CellField& field, const LocalMesh& mesh) {
  if (field.nGrow() != mesh.nGrow())
    throw std::invalid_argument("convection: CellField ghost width mismatch");
  for (int d = 0; d < 3; ++d)
    if (field.origin(d) != mesh.origin(d) || field.extent(d) != mesh.extent(d))
      throw std::invalid_argument(
          "convection: CellField layout does not match LocalMesh");
}

void requireOwnedLayout(const FaceField& field, const LocalMesh& mesh) {
  if (field.nGrow() != mesh.nGrow())
    throw std::invalid_argument("convection: FaceField ghost width mismatch");
  for (int d = 0; d < 3; ++d)
    if (field.origin(d) != mesh.origin(d) || field.extent(d) != mesh.extent(d))
      throw std::invalid_argument(
          "convection: FaceField layout does not match LocalMesh");
}

}  // namespace

void faceNormalVelocity(const CellField& velocity, FaceField& velocityFaces,
                        FaceField& normalVelocity, const LocalMesh& mesh) {
  requireOwnedLayout(velocity, mesh);
  requireOwnedLayout(velocityFaces, mesh);
  requireOwnedLayout(normalVelocity, mesh);
  if (velocity.ncomp() != 3)
    throw std::invalid_argument(
        "faceNormalVelocity: velocity must have three components");
  if (velocityFaces.ncomp() != 3 || normalVelocity.ncomp() != 1)
    throw std::invalid_argument(
        "faceNormalVelocity: workspace component counts are wrong");

  interpolate(velocity, velocityFaces, mesh);

  const LocalMesh::DeviceData m = mesh.device();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  FaceField uf = velocityFaces;
  FaceField un = normalVelocity;

  Kokkos::parallel_for(
      "saturn::face_normal_velocity",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          const Vec3 n = m.faceNormal(i, j, k, dir);
          const Vec3 u = {uf(i, j, k, dir, 0), uf(i, j, k, dir, 1),
                          uf(i, j, k, dir, 2)};
          un(i, j, k, dir, 0) = dot(u, n);
        }
      });
}

void convectiveFlux(const FaceField& normalVelocity, const CellField& phi,
                    FaceField& flux, const LocalMesh& mesh,
                    ConvectiveScheme scheme) {
  requireOwnedLayout(normalVelocity, mesh);
  requireOwnedLayout(phi, mesh);
  requireOwnedLayout(flux, mesh);
  if (normalVelocity.ncomp() != 1 || phi.ncomp() != 1 || flux.ncomp() != 1)
    throw std::invalid_argument(
        "convectiveFlux: all fields must have one component");

  const LocalMesh::DeviceData m = mesh.device();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  FaceField un = normalVelocity;
  CellField p = phi;
  FaceField f = flux;

  Kokkos::parallel_for(
      "saturn::convective_flux",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          int di = 0, dj = 0, dk = 0;
          faceDirectionOffset(dir, di, dj, dk);
          const Real velocity = un(i, j, k, dir, 0);
          const Real phiP = p(i, j, k, 0);
          const Real phiN = p(i + di, j + dj, k + dk, 0);
          if (scheme == ConvectiveScheme::Upwind) {
            f(i, j, k, dir, 0) =
                (velocity >= 0) ? velocity * phiP : velocity * phiN;
          } else {
            f(i, j, k, dir, 0) =
                Real(0.5) * velocity * (phiP + phiN) -
                Real(0.5) * Kokkos::abs(velocity) * (phiN - phiP);
          }
        }
      });
}

}  // namespace saturn
