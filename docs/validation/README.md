# Validation record

Each solver gets a page here once it has run. The page is a lab notebook: it
records what was measured, not only what matched.

Regression tests are separate from validation reports: see
[`regressionTests.md`](regressionTests.md) for what they protect and how the
expected files are maintained.

## Planned pages

| page | solver | status |
|---|---|---|
| [`burgers.md`](burgers.md) | 1D scalar `saturnBurger` | implemented; order 2.06, rank independent |
| `heat.md` | heat / convection–diffusion | waiting for Phase 3 |
| `stokes.md` | Stokes | waiting for Phase 4 |
| `incompressible.md` | incompressible Navier–Stokes | waiting for Phase 5 |
| `mesh.md` | `checkMesh` | waiting for Phase 1 |
| `config.md` | YAML validation | waiting for Phase 1 |

Mesh validation is reproducible with:

```bash
saturnCFD -checkMesh mesh.msh -report results/mesh
saturnCFD -checkMesh mesh.msh -report results/mesh -writeVTK results/mesh
```

The first command writes `results/mesh.vtu` and `results/mesh.json`. The second
adds `.vtp` patches and `results/mesh.pvd`; open the `.pvd` in ParaView.

## Required content

Every page states:

1. **Case** — geometry, mesh, BCs, initial conditions, physical parameters.
2. **Numerics** — scheme, limiter, time integrator, linear solver,
   preconditioner, tolerances.
3. **Reference** — analytic solution or published benchmark with citation.
4. **Results** — table of computed vs reference, error norms, observed order.
5. **Observations** — artefacts, failures, sensitivity, cost.
6. **Reproduction** — the exact YAML file and command.

## Error norms

For a scalar field $\phi$:

$$
L^1 = \frac{1}{N}\sum_P |\phi_P-\phi_P^{\mathrm{ref}}|,\qquad
L^2 = \sqrt{\frac{1}{N}\sum_P (\phi_P-\phi_P^{\mathrm{ref}})^2},\qquad
L^\infty = \max_P |\phi_P-\phi_P^{\mathrm{ref}}|.
$$

For a vector field, report each component and the magnitude.

## Observed order

For a refinement ratio $r$,

$$
p \approx \frac{\log(e_{\mathrm{coarse}}/e_{\mathrm{fine}})}{\log r}.
$$

Report the order for each norm and state whether the asymptotic range was
reached.

## Tolerance policy

- double precision: $10^{-10}$ for linear solver residuals, $10^{-8}$ for
  analytic comparisons;
- single precision (later GPU): $10^{-5}$ to $10^{-4}$, documented.
- Never hide a failed case by loosening a tolerance without recording why.
