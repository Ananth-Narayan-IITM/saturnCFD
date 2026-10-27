/**
 * @file Poisson.hpp
 *
 * saturnCFD — variable-free Poisson solve on a single AMR level.
 *
 * In the projection method the pressure equation is the only global elliptic
 * problem, and it dominates both cost and scalability. It is therefore isolated
 * here as its own testable component.
 *
 * We solve  -laplacian(p) = rhs  with homogeneous boundary conditions.
 *
 * The sign convention is deliberate: the 5/7-point Laplacian is negative
 * semi-definite, so we negate it to get a symmetric positive semi-definite
 * operator, which is what conjugate gradients requires.
 *
 * NOTE  CG is used here because it is simple, robust and easy to verify. It is
 * NOT the right production solver: CG iteration count grows as O(1/h) for
 * Poisson, so wall-clock time grows as O(N^1.5) in 2D. Geometric
 * multigrid (which makes the count grid-independent) is the next
 * milestone and is required before any lead-time claim against Basilisk.
 */
#pragma once

#include "amr/MultiFab.hpp"
#include "comm/HaloExchange.hpp"
#include "core/Types.hpp"
#include "mesh/Geometry.hpp"

namespace saturn {

enum class BCType {
  Neumann,   ///< dp/dn = 0  (impermeable wall, natural for pressure)
  Dirichlet  ///< p = 0      (open boundary / immersed solid potential)
};

/** @brief Solver controls for solvePoisson(). */
struct PoissonParams {
  BCType bc = BCType::Neumann;
  int maxIter = 5000;
  Real relTol = Real(1e-10);
  bool verbose = false;
  /// Reuse the incoming `p` as the initial guess (essential when the pressure
  /// is solved every timestep: it cuts CG iterations by an order of magnitude).
  bool warmStart = false;
};

struct PoissonResult {
  int iters = 0;
  Real relResidual = Real(0);
  bool converged = false;
};

/// Fill ghost cells lying outside the physical domain.
/// Neumann mirrors with + , Dirichlet mirrors with - (odd reflection).
void fillBoundaryGhosts(MultiFab& p, const Geometry& geom, BCType bc,
                        int comp = 0);

/// Ap = -laplacian(p), i.e. (2*NDIM*p - sum(neighbours)) / dx^2.
/// `p` must have at least one ghost layer; its halo and physical ghosts are
/// refreshed here, so the caller does not have to.
void applyNegativeLaplacian(MultiFab& p, MultiFab& Ap, const Geometry& geom,
                            HaloExchange& halo, BCType bc, Real dx);

/// Solve -laplacian(p) = rhs with homogeneous BCs, by conjugate gradients.
/// `p` is used as the initial guess and overwritten with the solution.
PoissonResult solvePoisson(MultiFab& p, const MultiFab& rhs,
                           const Geometry& geom, HaloExchange& halo,
                           const PoissonParams& prm);

}  // namespace saturn
