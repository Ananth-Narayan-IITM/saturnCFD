// saturnCFD — lid-driven cavity, single level, 2D.
//
// WHY A STAGGERED (MAC) GRID
// --------------------------
// In a finite-volume incompressible solver the pressure has no evolution
// equation; it is a Lagrange multiplier that makes the velocity field
// divergence-free. Two layouts are possible:
//
//   Collocated : u, v, p all at cell centres.
//   Staggered  : u on the east/west faces, v on the north/south faces,
//                p at cell centres (Patankar's arrangement).
//
// On a collocated grid, differentiating p across centres and then taking the
// divergence of that gradient produces a wide (2h) stencil that does not see the
// odd/even "checkerboard" pressure mode. The discrete system is then singular in
// a way that has nothing to do with the physics, and the pressure field comes
// out jagged. Curing that requires Rhie-Chow momentum interpolation.
//
// On a staggered grid the mass flux through each face IS a stored unknown, so
// the discrete continuity equation is exactly the flux balance of the control
// volume and checkerboarding is impossible by construction. AMReX's
// incompressible solver uses the same arrangement, so this stays compatible
// with the block-structured AMR hierarchy.
//
// INDEX CONVENTIONS (cell i,j spans [i*h,(i+1)*h] x [j*h,(j+1)*h])
//   u(i,j) is the x-velocity at ( i*h , (j+0.5)*h )      i in [0,n], j in [0,n-1]
//   v(i,j) is the y-velocity at ( (i+0.5)*h , j*h )      i in [0,n-1], j in [0,n]
//
// The extra faces (i = 0 and i = n for u) sit exactly on the walls. They are
// stored in the ghost layer of the MultiFab, which is why one ghost layer is
// enough.
//
// DISCRETISATION
//   momentum : du/dt = -(u u)_x - (u v)_y - p_x + nu lap(u)     [first-order
//              upwind convection, centred diffusion, explicit in time]
//   projection: u* -> solve  -lap(phi) = -div(u*)/dt  -> u = u* - dt grad(phi)
//
// This is Chorin's projection method. It is first order in time, which is fine
// for the steady cavity solutions we validate against, and it will be replaced
// by a second-order Bell-Colella-Glaz scheme later.
#pragma once

#include <functional>
#include <string>

#include "amr/MultiFab.hpp"
#include "comm/HaloExchange.hpp"
#include "core/Types.hpp"
#include "mesh/Geometry.hpp"

namespace saturn {

struct CavityParams {
  Real reynolds = 100;
  Real lidVelocity = 1;
  Real cfl = 0.4;              ///< convective dt safety factor
  Real diffusionFactor = 0.25; ///< viscous dt safety factor
  int poissonMaxIter = 5000;
  Real poissonTol = Real(1e-9);
  /// Convection scheme, see solver/Limiters.hpp (4 = MC, a good default).
  int scheme = 4;
};

struct SteadyReport {
  int steps = 0;
  Real time = 0;
  Real maxDelta = 0;   ///< max |u_new - u_old| at the last check
  bool converged = false;
};

class CavitySolver {
 public:
  /// `geom` must describe a single square box (n x n cells, dx = 1/n).
  CavitySolver(const Geometry& geom, const CavityParams& prm);

  void initialize();

  /// Largest stable explicit step for the current velocity field.
  Real stableDt() const;

  /// One projection step.
  void step(Real dt);

  /// March until `tEnd`, or until max|du| < tol between checks.
  /// `onOutput(step, time)` is called after initialisation and then every
  /// `outputEvery` steps, so the driver can write VTK without the solver
  /// knowing anything about file formats.
  SteadyReport runToSteady(Real tEnd, int maxSteps, Real tol, int checkEvery,
                           int outputEvery = 0,
                           const std::function<void(int, Real)>& onOutput = {});

  /// Interpolate the staggered velocity onto cell centres (components u, v).
  /// This is what gets written to VTK: ParaView expects cell- or node-centred
  /// data, not face-centred.
  void cellCentredVelocity(MultiFab& out) const;

  /// u along the vertical centreline x = 0.5, as (y, u) pairs.
  /// v along the horizontal centreline y = 0.5, as (x, v) pairs.
  void writeCentreLines(const std::string& path) const;

  const MultiFab& velocity() const { return m_u; }
  const MultiFab& pressure() const { return m_p; }
  int steps() const { return m_steps; }
  Real time() const { return m_time; }
  int cells() const { return m_n; }
  Real maxSpeed() const;

  /// Discrete divergence of the current velocity, max over cells. Should be
  /// ~1e-12 after the projection; a big number means a bug.
  Real maxDivergence() const;

 private:
  void applyVelocityBC();
  void computeDivergence(const MultiFab& vel, MultiFab& div) const;
  void predict(Real dt);
  void correct(Real dt);

  Geometry m_geom;
  int m_n;
  Real m_h;
  Real m_nu;
  CavityParams m_prm;

  MultiFab m_u;      ///< comp 0 = u (x-faces), comp 1 = v (y-faces)
  MultiFab m_ut;     ///< predictor
  MultiFab m_p;      ///< cell-centred pressure (kinematic: p/rho)
  MultiFab m_div;
  MultiFab m_rhs;
  HaloExchange m_halo;

  int m_steps = 0;
  Real m_time = 0;
};

}  // namespace saturn
