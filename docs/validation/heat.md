# Heat / convection–diffusion validation

Solver: `saturnHeat` (scalar `ScalarSolver`).

## 1. Case

Periodic box $[0,1]^d$, prescribed uniform velocity $\mathbf{a}$, thermal
diffusivity $\alpha$, no source. The exact Fourier mode is

$$
T(\mathbf{x},t)
= A\sin\!\left(\mathbf{k}\cdot\mathbf{x}-(\mathbf{a}\cdot\mathbf{k})t\right)
\exp\!\left(-\alpha|\mathbf{k}|^2t\right).
$$

| case | domain | $\mathbf{a}$ | $\alpha$ | $\mathbf{k}$ | $A$ | end time |
|---|---|---|---|---|---|---|
| 2D | $[0,1]^2$ | $(1,\;0.5,\;0)$ | 0.01 | $2\pi(1,1,0)$ | 1 | 0.2 s |
| 3D | $[0,1]^3$ | $(1,\;0.5,\;0.25)$ | 0.01 | $2\pi(1,1,1)$ | 1 | 0.1 s |

Boundary conditions are periodic. The initial condition is the exact field at
$t=0$.

## 2. Numerics

- Cell-centred finite volume, structured box mesh.
- Convective flux: MUSCL reconstruction with the van Leer limiter.
- Diffusion: corrected face-normal Laplacian from `finiteVolume.md`.
- Time: SSPRK3 with adaptive `dt`, CFL 0.3, `dtMax` from the case.
- Stopping criterion: the stability-limited step reaches `time.end`.

## 3. Reference

The Fourier mode above satisfies the continuous equation exactly, so the
reference is analytic. It tests the phase (convection) and the amplitude
(diffusion) independently.

## 4. Results

3D, $t=0.1$ s, `dtMax = 0.005` (the regression YAML):

| $N^3$ | $L^1$ | $L^2$ | $L^\infty$ | order |
|---:|---:|---:|---:|---:|
| 16 | 1.581e-2 | 1.943e-2 | 3.420e-2 | — |
| 32 | 3.125e-3 | 3.991e-3 | 9.468e-3 | 2.28 |
| 48 | 1.184e-3 | 1.479e-3 | 4.083e-3 | 2.44 (48/32) |

The unit test runs the same mode with `dtMax = 0.01`; its 16^3/32^3/64^3
$L^2$ values are 1.932e-2/3.973e-3/7.365e-4 (orders 2.28/2.43).

2D, $t=0.2$ s:

| $N^2$ | $L^2$ | order |
|---:|---:|---:|
| 32 | 5.733e-3 | — |
| 64 | 1.112e-3 | 2.37 |
| 128 | 2.425e-4 | 2.20 |

The 48$^3$ CPU example and the CUDA build on the local Quadro P620 agree to
the printed precision ($L^2=1.478989\times10^{-3}$). The 3D YAML case gives
the same norms on 1, 2 and 4 ranks.

## 5. Observations

- The error field is smooth and oscillatory at the truncation-error scale; no
  limiter clipping or odd–even decoupling is visible.
- Advection controls the phase error and diffusion controls the amplitude;
  refining the mesh reduces both at second order.
- The periodic halo and rank-0 local-geometry distribution are exercised by
  the MPI targets without a change in the norms.
- The explicit stability limit is
  $\Delta t\le\min(\mathrm{CFL}\,h/|\mathbf{a}|,\;
  \mathrm{safety}\,h^2/(2\alpha d))$, and SSPRK3 keeps the diffusion term
  stable at the default safety factor.
- Checkpoint/restart through the generic `ScalarSolver` path reproduces the
  uninterrupted run to roundoff.

## 6. Reproduction

```bash
# example with figures
cd examples/saturnHeat/saturnHeat3D
saturnCFD saturnHeat3D.yaml
python3 compareAnalytic.py

# regression slice (2D + 3D + 1/2/4 ranks + restart)
ctest --preset heat
ctest --preset checkpoint
```

Stable cases:

```text
tests/regression/heat2d/fourier64.yaml
tests/regression/heat3d/fourier32.yaml
tests/regression/heat3d/restart*.yaml
```

Figures: `figures/heat2DValidation.png`, `figures/heat3DValidation.png`.

See also [`../theory/heat.md`](../theory/heat.md).
