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

KOKKOS_INLINE_FUNCTION
Real minmod(Real a, Real b) {
  if (a * b <= 0)
    return 0;
  return (Kokkos::abs(a) < Kokkos::abs(b)) ? a : b;
}

KOKKOS_INLINE_FUNCTION
Real maxmod(Real a, Real b) {
  if (a * b <= 0)
    return 0;
  return (Kokkos::abs(a) > Kokkos::abs(b)) ? a : b;
}

KOKKOS_INLINE_FUNCTION
Real limitedSlope(Real s1, Real s2, Limiter limiter) {
  switch (limiter) {
  case Limiter::Minmod:
    return minmod(s1, s2);
  case Limiter::VanLeer:
    if (s1 * s2 <= 0 || s1 + s2 == 0)
      return 0;
    return Real(2) * s1 * s2 / (s1 + s2);
  case Limiter::Superbee:
    return maxmod(minmod(Real(2) * s1, s2), minmod(s1, Real(2) * s2));
  }
  return 0;
}

/// Limited linear reconstruction of cell `(i,j,k)` at its `dir` face.
KOKKOS_INLINE_FUNCTION
Real musclFaceValue(const CellField& phi, const LocalMesh::DeviceData& m, int i,
                    int j, int k, int dir, const Vec3& xFace, Limiter limiter) {
  int di = 0, dj = 0, dk = 0;
  faceDirectionOffset(dir, di, dj, dk);
  const int bi = i - di;
  const int bj = j - dj;
  const int bk = k - dk;
  const int fi = i + di;
  const int fj = j + dj;
  const int fk = k + dk;

  const Real phiP = phi(i, j, k, 0);
  if (!m.hasCentroid(bi, bj, bk) || !m.hasCentroid(fi, fj, fk))
    return phiP;
  const Real phiB = phi(bi, bj, bk, 0);
  const Real phiF = phi(fi, fj, fk, 0);

  const Vec3 xP = m.cellCentroid(i, j, k);
  const Vec3 xB = m.cellCentroid(bi, bj, bk);
  const Vec3 xF = m.cellCentroid(fi, fj, fk);
  const Vec3 d1v = xP - xB;
  const Vec3 d2v = xF - xP;
  const Vec3 dfv = xFace - xP;
  const Real d1 = Kokkos::sqrt(dot(d1v, d1v));
  const Real d2 = Kokkos::sqrt(dot(d2v, d2v));
  const Real df = Kokkos::sqrt(dot(dfv, dfv));
  if (!(d1 > 0) || !(d2 > 0) || !(df > 0))
    return phiP;

  const Real s1 = (phiP - phiB) / d1;
  const Real s2 = (phiF - phiP) / d2;
  return phiP + limitedSlope(s1, s2, limiter) * df;
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
                    ConvectiveScheme scheme, Limiter limiter) {
  requireOwnedLayout(normalVelocity, mesh);
  requireOwnedLayout(phi, mesh);
  requireOwnedLayout(flux, mesh);
  if (normalVelocity.ncomp() != 1 || phi.ncomp() != 1 || flux.ncomp() != 1)
    throw std::invalid_argument(
        "convectiveFlux: all fields must have one component");
  if (scheme == ConvectiveScheme::Muscl && phi.nGrow() < 2)
    throw std::invalid_argument(
        "convectiveFlux: MUSCL needs at least two ghost layers");

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
          const int ni = i + di;
          const int nj = j + dj;
          const int nk = k + dk;
          const Real velocity = un(i, j, k, dir, 0);
          const Real phiP = p(i, j, k, 0);
          const Real phiN = p(ni, nj, nk, 0);

          if (scheme == ConvectiveScheme::Upwind) {
            f(i, j, k, dir, 0) =
                (velocity >= 0) ? velocity * phiP : velocity * phiN;
          } else if (scheme == ConvectiveScheme::Rusanov) {
            f(i, j, k, dir, 0) =
                Real(0.5) * velocity * (phiP + phiN) -
                Real(0.5) * Kokkos::abs(velocity) * (phiN - phiP);
          } else {
            const bool physicalBoundary =
                (m.boundaryPatch(i, j, k, dir) >= 0) && !m.periodic[dir / 2];
            Real phiFace;
            if (physicalBoundary) {
              phiFace = (velocity >= 0) ? phiP : phiN;
            } else {
              const Vec3 xFace = m.faceCentroid(i, j, k, dir);
              phiFace = (velocity >= 0)
                            ? musclFaceValue(p, m, i, j, k, dir, xFace, limiter)
                            : musclFaceValue(p, m, ni, nj, nk, dir ^ 1, xFace,
                                             limiter);
            }
            f(i, j, k, dir, 0) = velocity * phiFace;
          }
        }
      });
}

void rhieChowFlux(const CellField& velocity, const CellField& pressure,
                  FaceField& velocityFaces, FaceField& pressureFaces,
                  CellField& pressureGrad, FaceField& normalVelocity,
                  const LocalMesh& mesh, Real coefficient) {
  requireOwnedLayout(velocity, mesh);
  requireOwnedLayout(pressure, mesh);
  requireOwnedLayout(velocityFaces, mesh);
  requireOwnedLayout(pressureFaces, mesh);
  requireOwnedLayout(pressureGrad, mesh);
  requireOwnedLayout(normalVelocity, mesh);
  if (velocity.ncomp() != 3 || pressure.ncomp() != 1 ||
      velocityFaces.ncomp() != 3 || pressureFaces.ncomp() != 1 ||
      pressureGrad.ncomp() != 3 || normalVelocity.ncomp() != 1)
    throw std::invalid_argument("rhieChowFlux: component count mismatch");

  interpolate(velocity, velocityFaces, mesh);
  gradient(pressure, pressureFaces, pressureGrad, mesh);

  const LocalMesh::DeviceData m = mesh.device();
  const int i0 = mesh.origin(0);
  const int j0 = mesh.origin(1);
  const int k0 = mesh.origin(2);
  const int nx = mesh.extent(0);
  const int ny = mesh.extent(1);
  const int nz = mesh.extent(2);
  FaceField uf = velocityFaces;
  FaceField pf = pressureFaces;
  CellField gp = pressureGrad;
  CellField p = pressure;
  FaceField un = normalVelocity;

  Kokkos::parallel_for(
      "saturn::rhie_chow",
      Kokkos::MDRangePolicy<Kokkos::Rank<3>, Kokkos::IndexType<int>>(
          {i0, j0, k0}, {i0 + nx, j0 + ny, k0 + nz}),
      KOKKOS_LAMBDA(int i, int j, int k) {
        for (int dir = 0; dir < 2 * m.dimension; ++dir) {
          int di = 0, dj = 0, dk = 0;
          faceDirectionOffset(dir, di, dj, dk);
          const int ni = i + di;
          const int nj = j + dj;
          const int nk = k + dk;
          const Vec3 n = m.faceNormal(i, j, k, dir);
          const Vec3 u = {uf(i, j, k, dir, 0), uf(i, j, k, dir, 1),
                          uf(i, j, k, dir, 2)};
          const Real average = dot(u, n);
          const Real pP = p(i, j, k, 0);
          const Vec3 gradP = {gp(i, j, k, 0), gp(i, j, k, 1), gp(i, j, k, 2)};

          Real correction = 0;
          const bool physicalBoundary =
              (m.boundaryPatch(i, j, k, dir) >= 0) && !m.periodic[dir / 2];
          if (physicalBoundary) {
            const Real pF = pf(i, j, k, dir, 0);
            const Real d = m.faceDistance(i, j, k, dir);
            correction = coefficient * ((pF - pP) / d - dot(gradP, n));
          } else {
            const Vec3 xP = m.cellCentroid(i, j, k);
            const Vec3 xN = m.cellCentroid(ni, nj, nk);
            const Vec3 dvec = xN - xP;
            Real dPerp = dot(dvec, n);
            if (!(dPerp > 0))
              dPerp = m.faceDistance(i, j, k, dir);

            Vec3 gradF = gradP;
            if (m.owned(ni, nj, nk)) {
              const Vec3 xf = m.faceCentroid(i, j, k, dir);
              const Vec3 dPv = xf - xP;
              const Vec3 dNv = xf - xN;
              const Real dP = Kokkos::sqrt(dot(dPv, dPv));
              const Real dN = Kokkos::sqrt(dot(dNv, dNv));
              const Real total = dP + dN;
              const Real wP = (total > 0) ? dN / total : Real(0.5);
              const Real wN = (total > 0) ? dP / total : Real(0.5);
              gradF = {wP * gp(i, j, k, 0) + wN * gp(ni, nj, nk, 0),
                       wP * gp(i, j, k, 1) + wN * gp(ni, nj, nk, 1),
                       wP * gp(i, j, k, 2) + wN * gp(ni, nj, nk, 2)};
            }
            const Real pN = p(ni, nj, nk, 0);
            correction = coefficient * ((pN - pP) / dPerp - dot(gradF, n));
          }
          un(i, j, k, dir, 0) = average + correction;
        }
      });
}

}  // namespace saturn
