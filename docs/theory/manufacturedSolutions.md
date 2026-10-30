# Method of manufactured solutions

**Status:** required verification method for the FVM layer.

## 1. What it is

The method of manufactured solutions (MMS) starts by choosing an exact,
smooth solution $\phi(x,y,z,t)$. It is substituted into the continuous PDE
to obtain a source term:

$$
s = \mathcal{L}(\phi).
$$

The discrete solver is then run with that source and the boundary conditions
that $\phi$ satisfies. The numerical solution $\phi_h$ is compared with the
exact $\phi$ at cell centroids. Refining the mesh gives the observed order of
accuracy:

$$
p \approx \frac{\log(e_{\mathrm{coarse}}/e_{\mathrm{fine}})}{\log 2}.
$$

MMS does not require an exact solution of the *discrete* system. It tests the
implementation of the discrete operator against the continuous one.

## 2. Why we use it

A unit test checks one function in isolation. A regression test checks that an
accepted result did not change. Neither answers this question:

> Does the full discrete operator converge to the PDE at the expected order?

MMS does. It catches:

- a wrong sign or coefficient in a flux;
- a missing face or a double-counted partition face;
- a wrong ghost-cell fill or boundary condition;
- a wrong cell volume or face area;
- an interpolation that is only first-order where it should be second-order;
- a halo bug that is invisible on one rank.

It is also the only practical way to verify a new PDE/boundary-condition
combination before a physical benchmark exists.

## 3. Diffusion/Laplacian MMS

For

$$
\nabla^2\phi = s,
$$

choose a periodic solution on the unit cube:

$$
\phi(x,y,z)=\sin(2\pi x)\sin(2\pi y)\sin(2\pi z).
$$

Then

$$
s = -(2\pi)^2\,d\,\phi,
$$

where $d$ is the number of active dimensions (1, 2 or 3). The same field is
periodic in every direction, so the halo/periodic path is exercised without a
boundary-condition error.

For a Dirichlet test, choose a non-periodic $\phi$ and set its exact boundary
values. For a Neumann test, choose $\phi$ whose normal derivative vanishes on
the boundary:

$$
\phi(x,y,z)=\cos(\pi x)\cos(\pi y)\cos(\pi z).
$$

Then $s=-(d\pi^2)\phi$ and $\partial_n\phi=0$ on every wall.

## 4. Advection–diffusion MMS

For

$$
\frac{\partial \phi}{\partial t}
+ \nabla\cdot(\mathbf{a}\phi)
= \nu\nabla^2\phi + s,
$$

choose a smooth $\phi(\mathbf{x},t)$ and a known velocity $\mathbf{a}$, then

$$
s = \frac{\partial \phi}{\partial t}
+ \nabla\cdot(\mathbf{a}\phi)
- \nu\nabla^2\phi.
$$

Use two tests:

1. **Steady MMS** — no time term, solve to a steady state or solve the linear
   system directly.
2. **One-step MMS** — start from $\phi(\mathbf{x},0)$, take one step and
   compare with $\phi(\mathbf{x},\Delta t)$; refine $\Delta t$ and $h$.

The second separates spatial error from time-integration error. If both are
refined together, the observed order is the minimum of the two.

## 5. What to measure

For each refinement level $h, h/2, h/4$:

$$
L^1 = \frac{1}{N}\sum_P |\phi_P-\phi(\mathbf{x}_P)|,
\quad
L^2 = \sqrt{\frac{1}{N}\sum_P (\phi_P-\phi(\mathbf{x}_P))^2},
\quad
L^\infty = \max_P |\phi_P-\phi(\mathbf{x}_P)|.
$$

Report:

- the three norms;
- observed order $p$ for each norm;
- the refinement ratio;
- whether the asymptotic range was reached;
- CPU/GPU and 1/2-rank parity.

Expected order for the first implementation:

| operator | expected order |
|---|---|
| central Laplacian on a uniform hex mesh | 2 |
| linear face interpolation | 2 |
| Rusanov/upwind convection | 1 |
| MUSCL/TVD on a smooth, non-extremum solution | 2 (local reduction at extrema is expected) |
| backward Euler in time | 1 |

A TVD limiter can reduce the observed order at smooth extrema. That is a
property of the scheme, not necessarily a bug; document it and use a smooth
test that avoids the extremum if the goal is the spatial order.

### First measured results

CPU OpenMP build, two ghost layers, 1/2/4 ranks. The observed orders below are
the two refinement pairs; the values agree across rank counts because each rank
assembles the same operator.

| case | levels | observed order | reference |
|---|---|---|---|
| 3D periodic Laplacian | $8^3,16^3,32^3$ | 1.978, 1.994 | $L^2(32^3)=0.13435580757778084$ |
| 2D periodic Laplacian | $16^2,32^2,64^2$ | 1.994, 1.999 | — |
| 3D periodic gradient (LocalMesh) | $8^3,16^3,32^3$ | 1.967, 1.992 | — |
| 3D periodic divergence (LocalMesh) | $8^3,16^3,32^3$ | 1.967, 1.992 | — |

The 32$^3$ value is stored in `tests/TestFv.cpp` and checked to `1e-10` on
every run. The CUDA build carries the same reference with a `1e-9` window for
device reduction ordering; that build is written but not yet executed here
because the CUDA toolkit is absent.

The LocalMesh gradient/divergence cases use the Gauss face sum and linear
interpolation. Their observed order is slightly below 2 at the coarsest pair,
as expected for a centred face interpolation on a smooth trigonometric field.
The linear-field tests separately show exactness with Dirichlet/Neumann ghosts.

## 6. Test matrix

Implemented now: the 2D/3D periodic Laplacian and 3D periodic
gradient/divergence on 1/2/4 CPU ranks, plus linear-field Dirichlet/Neumann
ghost tests. Pending: Dirichlet/Neumann Laplacian order, advection–diffusion
and execution on a CUDA host.

| case | ranks | backend | BC |
|---|---|---|---|
| 2D periodic Laplacian | 1, 2, 4 | CPU | periodic |
| 3D periodic Laplacian | 1, 2, 4 | CPU | periodic |
| 3D periodic gradient/divergence | 1, 2, 4 | CPU | periodic |
| linear gradient/divergence | 1, 2, 4 | CPU | Dirichlet |
| linear gradient | 1, 2, 4 | CPU | Neumann |
| 3D Dirichlet Laplacian | 1, 2 | CPU | Dirichlet |
| 3D Neumann Laplacian | 1, 2 | CPU | Neumann |
| 2D/3D periodic Laplacian | 1 | GPU | periodic |
| advection–diffusion steady | 1, 2 | CPU | periodic/Dirichlet |
| advection–diffusion one-step | 1, 2 | CPU | periodic |

GPU parity is a separate check: same case, CPU vs GPU, within the precision
floor.

## 7. Why periodic first

Periodic MMS removes the boundary-condition implementation from the error. If
the periodic case is second order, the interior operator and halo exchange are
correct. Then Dirichlet and Neumann isolate the boundary ghosts. This is the
fastest way to localize a bug.

## 8. MMS versus other tests

| test | question |
|---|---|
| unit test | does one function do what it claims? |
| MMS | does the assembled operator converge at the expected order? |
| regression | did an accepted result change? |
| physical benchmark | does the model match reality/reference data? |

All four are needed. MMS is not a substitute for a benchmark, and a benchmark
is not a substitute for MMS.

## 9. Common pitfalls

- **Inconsistent source.** The source must be evaluated at cell centroids with
  the same discrete operator; a continuous source sampled at the wrong point
  produces a first-order error.
- **Boundary values.** Dirichlet/Neumann MMS needs the exact boundary data, not
  zero.
- **Ghost fill order.** Halo exchange before physical BC, then MMS comparison
  after a complete operator application.
- **Mixed refinement.** Refining both $h$ and $\Delta t$ gives the minimum
  order; refine them separately when possible.
- **Limiter order.** A TVD limiter can clip a smooth extremum; do not call that
  a convergence bug without checking the limiter branch.
- **Too few refinements.** Two refinements can be in the pre-asymptotic range;
  use three where possible.
- **Roundoff.** Float builds have a higher error floor; compare within the
  precision-dependent tolerance.

## 10. Acceptance criteria

- All three norms decrease under refinement.
- Observed order is within about 0.2 of the expected order in the asymptotic
  range.
- The same order is observed on 1, 2 and 4 ranks.
- CPU/GPU parity is within the precision floor.
- A deliberately broken sign, coefficient or halo test fails MMS.

## References

- Salari & Knupp (2000), *Code Verification by the Method of Manufactured
  Solutions*, Sandia Report SAND2000-1444.
- Roache (2002), *Code Verification by the Method of Manufactured Solutions*,
  J. Fluids Eng. 124, 4–10.
- Oberkampf & Roy (2010), *Verification and Validation in Scientific
  Computing*, Cambridge University Press.
