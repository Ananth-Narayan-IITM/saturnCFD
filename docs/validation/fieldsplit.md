# Fieldsplit versus direct LU

`saturnStokes` can solve the coupled $u,v,w,p$ saddle-point system in two
ways:

| `stokes.linearSolver` | method | memory | use |
|---|---|---|---|
| `direct` | PETSc `preonly` + LU (SuperLU_DIST when available) | $O(N^{4/3})$ in 3D | validation reference; small meshes; high-memory runs |
| `fieldsplit` | FGMRES + Schur-complement fieldsplit | $O(N)$ | default; production meshes |

## Block structure

The steady Stokes system is

$$
\left[\begin{array}{cc}A & -B^{T}\\ B & C\end{array}\right]
\left[\begin{array}{c}u\\p\end{array}\right]
=
\left[\begin{array}{c}f\\g\end{array}\right],
$$

with $A$ the vector Laplacian, $B$ the divergence, $C$ the
Brezzi–Pitkäranta pressure Laplacian and $g$ the MMS continuity source
(zero for physical cases). The Schur complement is

$$
S=C+B\,A^{-1}B^{T}.
$$

The fieldsplit preconditioner applies an approximate velocity solve with
$A$, an approximate Schur solve with the pressure block $C$
(`selfp`), and the coupling back-substitution of the full factorisation.

## Production configuration

The schema defaults are

```yaml
stokes:
  linearSolver: fieldsplit
  velocityKsp: gmres
  velocityPc: gamg
  pressureKsp: preonly
  pressurePc: gamg
```

plus an outer FGMRES at `rtol = 1e-12` and a velocity inner tolerance of
`1e-10`. GAMG on both blocks keeps memory $O(N)$ and gives mesh-independent
inner solves.

Two implementation details matter:

1. **The PC options prefix must be set.** `KSPSetOptionsPrefix(ksp, "stokes_")`
   alone does not prefix the PC. Without `PCSetOptionsPrefix(pc, "stokes_")`,
   the options `-fieldsplit_*` are ignored and PETSc silently falls back to
   GMRES/bjacobi. This was the original cause of the erratic fieldsplit
   behaviour.
2. **Do not factorise the local pressure block.** The pressure block is a
   Neumann Laplacian; on a subdomain without the reference row it is singular.
   Parallel ILU/bjacobi can therefore produce a zero pivot. A global GAMG
   pressure solve (or Jacobi) avoids this. This was the 2D four-rank failure.

## Fully periodic domains

A fully periodic Stokes problem has both a constant-pressure nullspace and a
constant-velocity nullspace. For **fully periodic domains only**, the assembly
adds two tiny diagonal gauges:

- `pressureGauge = 1e-6` to every continuity row, which fixes the pressure
  constant without removing a continuity equation;
- `velocityGauge = 1e-6` to the velocity rows, which fixes the velocity mean.

Wall-bounded cases keep the pressure reference row and no velocity gauge, so
`p = 0` remains exact there. The gauges perturb a fully periodic nonzero
solution by about `1e-6`, which is far below the discretisation error. Both
the direct and fieldsplit paths now solve fully periodic cases; the periodic
MMS regression test covers fieldsplit on 1/2/4/8 ranks.

## Known-good sub-solver configurations

| configuration | velocity | pressure | use |
|---|---|---|---|
| production | GMRES + GAMG | preonly + GAMG | parallel and larger meshes |
| serial-tuned | GMRES + ILU | preonly + Jacobi | serial comparisons; low setup cost |

The serial-tuned configuration is selected through the schema, not a code
change:

```yaml
stokes:
  linearSolver: fieldsplit
  velocityKsp: gmres
  velocityPc: ilu
  pressureKsp: preonly
  pressurePc: jacobi
```

## Measured comparison

`examples/saturnStokes/validate.py` runs every example case with both solvers
and reports wall time, peak process-tree RSS, analytic error and the
difference between the two numerical solutions. The serial-tuned fieldsplit
configuration is used for these runs; the table below is the Poiseuille
slice of `results/summary.txt`.

2D, $128^2$, 1 rank:

| solver | iterations | wall [s] | peak RSS [MB] | $L^2$ | pL2 |
|---|---:|---:|---:|---:|---:|
| direct | 1 | 4.92 | 600 | 7.63e-6 | 3.8e-15 |
| fieldsplit | 3 | 8.32 | 175 | 7.63e-6 | 3.9e-13 |

3D, $24^3$, 1 rank:

| solver | iterations | wall [s] | peak RSS [MB] | $L^2$ | pL2 |
|---|---:|---:|---:|---:|---:|
| direct | 1 | 94.8 | 1486 | 2.17e-4 | 7.5e-16 |
| fieldsplit | 2 | 1.73 | 166 | 2.17e-4 | 5.3e-13 |

The fieldsplit and direct velocities differ by $3.1\times10^{-14}$
(2D) and $5.5\times10^{-14}$ (3D) in RMS; the pressures differ by
$3.9\times10^{-13}$ and $5.3\times10^{-13}$ after mean removal.

Interpretation:

- 2D: fieldsplit is slower at this small size (ILU setup dominates) but uses
  $3.4\times$ less memory.
- 3D: fieldsplit is $56\times$ faster and uses $9\times$ less memory; the
  direct factor has already grown to 1.5 GB.
- The accuracy is the same: the difference is at the linear-solver
  tolerance, far below the discretisation error.

## Reproduction

```bash
cd examples/saturnStokes
python3 validate.py --dim 2d --cli saturnCPU
python3 validate.py --dim 3d --cli saturnCPU
```

The script writes `results/summary.txt` and, per dimension,
`results/<dim>/figures/<case>_{validation,solvers}.png` plus
`summary_solvers.png`. For `--ranks > 1` it switches to the production
GAMG/GAMG configuration and reports the summed RSS of the launcher and its MPI
children, sampled from `/proc`.

## Limitations

- The wall-bounded MMS is gated in 2D (velocity order 1.96, pressure order
  1.64 on 16^2 -> 32^2). In 3D the velocity is second order (1.97 on
  16^3 -> 24^3) and the pressure converges more slowly (1.53 on
  24^3 -> 32^3), so it is a measured but not yet gated order.
