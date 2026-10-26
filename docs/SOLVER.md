# How the solver works, in control-volume terms

This document exists because the code is easier to trust when you can point at
each piece and say "that's the flux through that face". Every section maps a
CFD concept onto a specific function.

---

## 1. The picture

The domain is cut into square **cells** (control volumes). Each cell has four
**faces**. In finite-volume form, every equation is a statement about what
crosses those faces plus what accumulates inside:

```
  d/dt (cell average) = sum over faces of (flux in) - sum over faces of (flux out) + source
```

For incompressible flow we solve two things:

| Equation | Physical meaning | Where it lives in the code |
|---|---|---|
| Momentum | Newton's second law on each control volume | `CavitySolver::predict` |
| Continuity | mass in = mass out, exactly | `CavitySolver::correct` + `solvePoisson` |

The awkward part of incompressible flow is that **pressure is not a
thermodynamic variable** — there is no equation that advances it in time. It is
a Lagrange multiplier whose only job is to make the velocity field satisfy
continuity. That is why the algorithm is split into two stages (a *projection*)
rather than simply integrating both equations forward.

---

## 2. Why the velocities sit on faces, not cell centres

Two possible layouts:

- **Collocated** — `u`, `v`, `p` all at cell centres.
- **Staggered / MAC** — `u` on the east–west faces, `v` on the north–south
  faces, `p` at cell centres. This is Patankar's arrangement.

We use staggered. The reason is concrete, not aesthetic.

Suppose pressure is at cell centres and you compute its gradient with a central
difference across cell centres. The gradient at cell `i` uses `p(i+1)` and
`p(i-1)` — it never sees `p(i)`. Now take the divergence of that gradient: you
get a Laplacian on a stencil of width `2h`, which also skips every other cell.
The result is that the **odd and even cells are decoupled**. A pressure field
that alternates `+1, -1, +1, -1` produces *zero* gradient at every cell, so the
discrete equations cannot see it. This is the **checkerboard mode**, and it is a
property of the discretisation, not of the flow.

On a staggered grid this cannot happen: the mass flux through each face is
itself a stored unknown, so the continuity equation for a cell is literally

```
(u_east - u_west)/h + (v_north - v_south)/h = 0
```

and the pressure gradient that drives `u_east` is `(p(i+1) - p(i))/h` — the two
cells that share that face. The stencil is compact and the odd/even mode
produces a non-zero gradient, so the solver kills it.

Collocated grids are used all the time (OpenFOAM is collocated) but they need
Rhie–Chow momentum interpolation to suppress checkerboarding. Staggering is
simpler and is what AMReX's incompressible solver does, so it stays compatible
with block-structured AMR.

### Index conventions

```
cell (i,j) spans  [i*h,(i+1)*h] x [j*h,(j+1)*h]

u(i,j)  is the x-velocity at ( i*h , (j+0.5)*h )     i in [0,n],   j in [0,n-1]
v(i,j)  is the y-velocity at ( (i+0.5)*h , j*h )     i in [0,n-1], j in [0,n]
```

Note `i` runs to `n` for `u`: those are the faces lying **on** the walls. They
are stored in the ghost layer, which is why one ghost layer is enough.

---

## 3. Step 1 — momentum predictor (`predict`)

For each `u`-face we advance

```
u* = u + dt * ( - d(uu)/dx - d(uv)/dy + nu * lap(u) )
```

Three pieces:

- **Convection** `d(uu)/dx`: at face `i` the mass flux is `u(i)`, and the value
  being carried is taken from *upwind* — `u(i-1)` if the flow is positive,
  `u(i+1)` if negative. First-order upwind. This is the term that currently
  costs us accuracy (see §7).
- **Cross convection** `d(uv)/dy`: `v` lives on `y`-faces, so it is averaged
  onto the `x`-face first (`vS`, `vN` in the code).
- **Diffusion** `nu*lap(u)`: plain centred second difference. At the wall rows
  it reaches into the ghost cells, which is how the no-slip and moving-lid
  conditions enter the shear.

Same construction for `v`.

Diffusion is explicit here. That is fine at Re = 100–1000 on these grids but it
puts a `dt < h^2/(4 nu)` limit on the timestep, which becomes ruinous at high
Re on fine grids. Implicit diffusion (ADI or a Krylov solve) is a later change.

## 4. Step 2 — the pressure equation (`solvePoisson`)

We want the corrected field `u = u* - dt * grad(phi)` to be divergence-free.
Taking the divergence:

```
div(u*) - dt * lap(phi) = 0      =>      -lap(phi) = -div(u*)/dt
```

So we solve a Poisson equation for `phi` (pressure divided by density). The
right-hand side is built in `CavitySolver::step` from `computeDivergence(m_ut,
...)` — **note it is the divergence of the predictor `u*`, not of the old
velocity**; using the old one silently does nothing (this was a real bug during
development and it is the first thing to check if the solution misbehaves).

Walls are impermeable, so `dp/dn = 0` there — a **homogeneous Neumann**
condition. A pure-Neumann Poisson problem is singular (adding a constant to `p`
changes nothing), so the solver removes the mean of the right-hand side before
iterating and re-centres the answer afterwards.

## 5. Step 3 — the correction (`correct`)

```
u(i,j) = u*(i,j) - dt * ( p(i,j) - p(i-1,j) ) / h
```

At the wall faces the Neumann condition makes `p` mirror, so the bracket is
zero and the wall velocity stays exactly on its boundary value. That is why the
impermeability survives the correction without special-casing.

After the correction the discrete divergence is ~1e-8 — not zero, because the
Poisson solve is iterative, but small enough to be irrelevant.

---

## 6. The Poisson solver, and why it is temporary

`linalg/Poisson.cpp` implements conjugate gradients on the 5-point Laplacian,
with three tests against manufactured solutions (second-order convergence
verified, both BC types).

It is **not** the production solver. CG's iteration count grows like `O(1/h)`
for Poisson, so total work grows like `O(N^1.5)` in 2D. On a 64² cavity the
pressure solve already dominates the runtime (~5 ms/step for 4096 cells).
**Geometric multigrid is required** — it makes the iteration count independent
of grid size — and it must exist before any claim about time-to-solution
against Basilisk.

Also note: Jacobi preconditioning is deliberately *not* used. The diagonal of
this operator is the constant `2*NDIM/h^2`, so Jacobi is exactly a uniform
scaling of the system and cannot change CG's convergence at all. Adding it would
look like optimisation while doing nothing.

---

## 7. Validation status

Lid-driven cavity, Re = 100, 64 x 64, run to steady state
(`max|du| < 1e-6`), first-order upwind:

| y | Ghia et al. (1982) u | saturnCFD u | deviation |
|---:|---:|---:|---:|
| 0.9766 | 0.8412 | 0.8361 | 0.6 % |
| 0.7344 | 0.0033 | ≈ -0.016 | large *absolute* error in a near-zero region |
| 0.5000 | -0.2058 | -0.187 | 9 % |
| 0.1719 | -0.1015 | -0.1058 | 4 % |

Solution is qualitatively correct and converges to steady state, but the
mid-height error is ~9 %. **This is the expected signature of first-order upwind
numerical diffusion**: the scheme adds an effective viscosity proportional to
`|u|*h`, which at 64² corresponds to a noticeable drop in effective Reynolds
number.

Two things must improve before this is a DNS-quality comparison:

1. **Second-order convection** (linear-upwind / QUICK, with a limiter). Expected
   to cut the mid-height error by roughly a factor of 4 at fixed grid.
2. **Grid refinement study** at 128² and 256² to demonstrate the order of
   accuracy, rather than a single-point comparison.

`max|u| = 1.11` (slightly above the lid speed) occurs at the lid corners, where
the boundary condition is genuinely singular. This is well documented for MAC
schemes and is not an instability.

---

## 8. What is deliberately not here yet

| Missing | Consequence |
|---|---|
| Multigrid pressure solve | 50–100x slower than necessary; blocks scaling claims |
| Second-order convection | ~9 % error at Re=100 on 64² |
| Implicit diffusion | `dt ~ h^2/nu` restriction |
| AMR (tagging, regrid, reflux) | single level only |
| Multiphase | later milestone |
