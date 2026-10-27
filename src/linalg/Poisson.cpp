#include "linalg/Poisson.hpp"

#include <cmath>
#include <stdexcept>

#include "linalg/VecOps.hpp"

namespace saturn {

void fillBoundaryGhosts(MultiFab& p, const Geometry& geom, BCType bc,
                        int comp) {
  const int ng = p.nGrow();
  if (ng == 0)
    return;

  const Box dom = geom.domain;
  const Real sgn = (bc == BCType::Neumann) ? Real(1) : Real(-1);

  for (int lb = 0; lb < p.numLocalBoxes(); ++lb) {
    const Box bx = p.boxArray()[p.localToGlobal(lb)];
    const Array4<Real> a = p.array(lb);
    const Box gb = bx.grow(ng);

    const int n0 = gb.length(0);
    const int n1 = gb.length(1);
    const int n2 = (NDIM == 3) ? gb.length(2) : 1;

    Kokkos::parallel_for(
        "saturn::fillBoundaryGhosts", Kokkos::RangePolicy<>(0, n0 * n1 * n2),
        KOKKOS_LAMBDA(int idx) {
          const int i = gb.lo[0] + idx % n0;
          const int r = idx / n0;
          const int j = gb.lo[1] + r % n1;
          const int k = (NDIM == 3) ? (gb.lo[2] + r / n1) : 0;

          Idx q{i, j, k};
          bool outside = false;
          for (int d = 0; d < NDIM; ++d) {
            if (q[d] < dom.lo[d]) {
              q[d] = 2 * dom.lo[d] - q[d] - 1;  // reflect across the low face
              outside = true;
            } else if (q[d] > dom.hi[d]) {
              q[d] = 2 * dom.hi[d] - q[d] + 1;  // reflect across the high face
              outside = true;
            }
          }
          if (!outside)
            return;  // interior ghost or valid cell: leave alone

          a(i, j, k, comp) = sgn * a(q[0], q[1], q[2], comp);
        });
  }
}

void applyNegativeLaplacian(MultiFab& p, MultiFab& Ap, const Geometry& geom,
                            HaloExchange& halo, BCType bc, Real dx) {
  if (p.nGrow() < 1)
    throw std::invalid_argument(
        "applyNegativeLaplacian: p needs at least one ghost layer");

  halo.execute(p);                  // periodic / inter-patch ghosts
  fillBoundaryGhosts(p, geom, bc);  // physical ghosts (overrides at the border)

  const auto pf = p.boxArrays();
  const Real invdx2 = Real(1) / (dx * dx);

  Ap.forEachCellFlat(
      KOKKOS_LAMBDA(int b, Array4<Real> out, int i, int j, int k) {
        Array4<Real> q = pf(b);
        Real s = 2.0 * Real(NDIM) * q(i, j, k, 0);
        s -= q(i + 1, j, k, 0) + q(i - 1, j, k, 0);
        s -= q(i, j + 1, k, 0) + q(i, j - 1, k, 0);
        if (NDIM == 3)
          s -= q(i, j, k + 1, 0) + q(i, j, k - 1, 0);
        out(i, j, k, 0) = s * invdx2;
      });
}

PoissonResult solvePoisson(MultiFab& p, const MultiFab& rhs,
                           const Geometry& geom, HaloExchange& halo,
                           const PoissonParams& prm) {
  PoissonResult res;
  const auto ba = p.boxArray();
  const auto dm = p.distributionMap();
  const int rank = p.myRank();

  MultiFab r(ba, dm, 1, 1, rank, "cg::r");
  MultiFab s(ba, dm, 1, 1, rank, "cg::s");
  MultiFab Ap(ba, dm, 1, 1, rank, "cg::Ap");

  copyField(rhs, r);

  // The Neumann problem is singular: the operator annihilates constants, so the
  // right-hand side must be orthogonal to them. Project it out.
  const Accum cellCount = static_cast<Accum>(geom.domainPts());
  if (prm.bc == BCType::Neumann) {
    const Accum mean = r.globalSum(0) / cellCount;
    addScalar(r, static_cast<Real>(-mean));
  }

  if (!prm.warmStart)
    setZero(p);

  // r = rhs - A p  (with p = 0 this is just rhs, but warm starts need the term)
  applyNegativeLaplacian(p, Ap, geom, halo, prm.bc, geom.dx);
  axpy(Real(-1), Ap, r);

  const Accum bnorm = norm2(r);
  if (bnorm == 0) {
    res.converged = true;
    return res;
  }

  copyField(r, s);  // s = r (no preconditioner; see note below)

  // NOTE on preconditioning: the diagonal of this operator is the constant
  // 2*NDIM/dx^2, so Jacobi preconditioning is exactly a uniform scaling of the
  // system and cannot change CG's convergence at all. Any real gain needs
  // multigrid or an incomplete factorisation; we deliberately do neither yet
  // rather than ship a preconditioner that does nothing.

  Accum rho = dot(r, r);

  for (int it = 0; it < prm.maxIter; ++it) {
    applyNegativeLaplacian(s, Ap, geom, halo, prm.bc, geom.dx);

    const Accum denom = dot(s, Ap);
    if (denom == 0)
      break;
    const Real alpha = static_cast<Real>(rho / denom);

    axpy(alpha, s, p);    // p += alpha * s
    axpy(-alpha, Ap, r);  // r -= alpha * Ap

    const Accum rnorm = norm2(r);
    res.iters = it + 1;
    res.relResidual = static_cast<Real>(rnorm / bnorm);
    if (prm.verbose && (it % 50 == 0))
      Kokkos::printf("  cg %4d  |r|/|b| = %.3e\n", it, res.relResidual);
    if (res.relResidual <= prm.relTol) {
      res.converged = true;
      break;
    }

    const Accum rhoNew = dot(r, r);
    const Real beta = static_cast<Real>(rhoNew / rho);
    axpby(Real(1), r, beta, s);  // s = r + beta * s
    rho = rhoNew;
  }

  // Renormalise the Neumann null space so the pressure has zero mean.
  if (prm.bc == BCType::Neumann) {
    const Accum mean = p.globalSum(0) / cellCount;
    addScalar(p, static_cast<Real>(-mean));
  }

  return res;
}

}  // namespace saturn
