# Stokes validation

**Status:** implemented for 2D on 1/2/4/8 MPI ranks and 3D on 1/2/4. Results below are measured.

Solver: `saturnStokes`, coupled pressure–velocity saddle-point formulation.
`fieldsplit`/Schur is the default linear solve; `direct` is the validation
reference. Theory:
[`../theory/stokes.md`](../theory/stokes.md) and
[`../theory/coupledSaddlePoint.md`](../theory/coupledSaddlePoint.md).

## 1. Cases

| case | domain | boundary conditions | reference |
|---|---|---|---|
| Poiseuille 2D | $[0,L]\times[0,H]$ | no-slip walls, body force $f_x=G$ or pressure drop | analytic parabola |
| Poiseuille 3D | $[0,L]\times[0,H]\times[0,W]$ | no-slip walls, streamwise force | analytic parabola in $y$ |
| Couette 2D | $[0,L]\times[0,H]$ | lower wall at rest, upper wall $U$ | analytic linear profile |
| Couette 3D | $[0,L]\times[0,H]\times[0,W]$ | lower wall at rest, upper wall $U$ | analytic linear profile |
| MMS 2D | periodic or Dirichlet box | manufactured divergence-free velocity | second-order velocity/pressure |
| MMS 3D | periodic box | manufactured divergence-free velocity | second-order velocity/pressure |
| exit boundary | 3D box/pipe with an outflow face | no-slip walls, prescribed inlet, exit condition | Sankhla & Das (2025) |

The Poiseuille and Couette cases are the primary regression tests. The MMS
cases verify the formal order. The exit-boundary case is the bridge to the
3D/BEM comparison and is scheduled after the first two tests pass.

## 2. Numerics

- Cell-centred finite volume on a structured hexahedral box.
- Coupled $u,v,w,p$ saddle-point matrix; Brezzi–Pitkäranta pressure stabilisation.
- PETSc `MPIAIJ`, GMRES/FGMRES, fieldsplit/Schur preconditioner.
- Velocity block: GAMG/ILU; pressure block: GAMG/HYPRE.
- Pressure nullspace for all-Neumann cases.
- Tolerances: outer `rtol = 1e-10`, `atol = 1e-12`; failure is an error.

## 3. Error norms

For velocity and pressure separately:

$$
L^2(\phi)=\sqrt{\frac{1}{N}\sum_P(\phi_P-\phi_P^{\mathrm{ref}})^2},
\qquad
L^\infty(\phi)=\max_P|\phi_P-\phi_P^{\mathrm{ref}}|.
$$

The pressure error is computed after removing the mean pressure. The discrete
divergence norm is

$$
\|D\|_2=\sqrt{\frac{1}{N}\sum_P\left(\nabla_h\cdot\mathbf{u}_h\right)_P^2}.
$$

## 4. Acceptance targets

| test | target |
|---|---|
| Poiseuille velocity | $L^2$ decreases at second order; profile matches the parabola |
| Couette velocity | linear profile to discretisation error |
| MMS velocity | observed order $p\ge 1.8$ on the finest pair |
| MMS pressure | observed order $p\ge 1.8$ on the finest pair |
| continuity | $\|D\|_2$ below the linear-solve tolerance scale |
| nullspace | mean pressure stable; no drift with iterations |
| rank parity | 2D and 3D norms agree on 1/2/4 ranks (2D also on 8) |
| fieldsplit | FGMRES + GAMG converges without manual per-case tuning |

## 4a. Measured results

| case | mesh | $L^2$ | $L^\infty$ |
|---|---|---|---|
| Poiseuille 2D | 16$^2$ | 4.88e-4 | 4.88e-4 |
| Poiseuille 2D | 32$^2$ | 1.22e-4 | 1.22e-4 |
| Poiseuille 2D | 64$^2$ | 3.05e-5 | 3.05e-5 |
| Couette 2D | 64$^2$ | 7.5e-13 | 1.1e-12 |
| Poiseuille 3D | 24$^2\times8$ | 2.17e-4 | 2.17e-4 |
| Couette 3D | 24$^2\times8$ | 5.7e-12 | 6.6e-11 |

Multi-rank parity after the DMDA assembly fix:

| case | 1 rank | 2 ranks | 4 ranks | 8 ranks |
|---|---:|---:|---:|---:|
| Poiseuille 2D $64^2$ | 3.0518e-5 | 3.0518e-5 | 3.0518e-5 | 3.0518e-5 |
| Couette 2D $64^2$ | 5.04e-13 | 5.06e-13 | 5.06e-13 | 5.42e-13 |
| Poiseuille 3D $24^2\times8$ | 2.1701e-4 | 2.1701e-4 | 2.1701e-4 | — |
| Couette 3D $24^2\times8$ | 3.19e-13 | 4.85e-13 | 2.29e-14 | — |

These are direct-LU results (the round-off floor).

### Fieldsplit default

The examples now use the default fieldsplit: outer FGMRES (rtol $10^{-12}$),
`selfp` Schur, GMRES + GAMG velocity (rtol $10^{-10}$), and preonly + GAMG
pressure. Measured on the example meshes:

| case | ranks | outer iterations | $L^2$ | pL2 |
|---|---|---:|---:|---:|
| Poiseuille 2D $64^2$ | 1 / 2 / 4 / 8 | 18 / 20 / 20 / 20 | 3.05e-5 | 2–4e-12 |
| Couette 2D $64^2$ | 1 / 2 / 4 / 8 | 11 / 11 / 11 / 11 | 4.0e-11 | 2e-9 |
| Poiseuille 3D $24^2\times8$ | 1 / 2 / 4 | 13 / 12 / 13 | 2.17e-4 | 5–10e-12 |
| Couette 3D $24^2\times8$ | 1 / 2 / 4 | 3 / 5 / 6 | 1.2e-11 | 4e-10 |

The Couette values are the linear-solve error, not the discretisation error:
the pressure pL2 stays below the example tolerance of $10^{-8}$.

The `z`-dependent body-force sampling and the per-volume row scaling were also
fixed: the old momentum rows were volume-integrated while the continuity rows
were per-volume, which scaled the solved pressure by the cell volume and made
the Schur complement `O(1/(mu h^3))`. MMS and the Sankhla & Das exit-boundary
cases remain planned.

## 5. Test/CTest plan

Planned cases (names reserved for the implementation):

```text
stokes_np1 / stokes_np2 / stokes_np4   # testSaturnStokes
cli_checkConfig_example_stokes2d
cli_checkConfig_example_stokes3d
```

The `testSaturnStokes` binary covers Poiseuille and Couette in 2D/3D on
1/2/4/8 ranks (2D) and 1/2/4 ranks (3D), and checks fieldsplit against
direct. Each example writes `u`, `v`, `w`, `p`, the analytic comparison
and the PETSc iteration diagnostics. The CTest label is `solver`.

## 6. Reproduction

```bash
cd examples/saturnStokes/saturnStokes2D
saturnCFD poiseuille.yaml
saturnCFD couette.yaml
python3 compareAnalytic.py       # validates both, writes summary.txt

cd ../saturnStokes3D
saturnCFD poiseuille.yaml
saturnCFD couette.yaml
python3 compareAnalytic.py       # 3D profile surfaces + summary

ctest --preset solver
```

With `output.vtkEvery: 1`, each case writes one `.vtu` containing `u`, `v`,
`w` and `p` for ParaView. The comparison script plots the numerical field,
analytic field, error and averaged profile; the 3D version uses colored
velocity-profile surfaces for the numerical solution, the analytic solution
and their error.

## 7. Observations

To be filled from the first runs:

- fieldsplit iteration counts versus mesh, viscosity and the pressure
  stabilisation coefficient;
- velocity/pressure orders for MMS;
- pressure mean and nullspace behaviour;
- divergence after the coupled solve;
- comparison with the BEM exit-boundary data.

## References

- Sankhla, K. & Das, S. K. (2025), *Solution of three-dimensional steady state
  Stokes flow problems with exit boundary conditions by the boundary element
  method*, Computers & Mathematics with Applications 195, 1–13.
- Benzi, Golub & Liesen (2005).
- Elman, Silvester & Wathen (2014).
- Rhie & Chow (1983).
- Balay et al., *PETSc/TAO Users Manual*.
