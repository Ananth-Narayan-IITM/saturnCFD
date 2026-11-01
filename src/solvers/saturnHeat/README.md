# saturnHeat — 1D/2D/3D scalar convection–diffusion solver

**Target:** `saturnHeat`

**Class:** `saturn::SaturnHeat`

**Registered name:** `saturnHeat`

**Equation:** scalar convection–diffusion (heat)

$$
\frac{\partial T}{\partial t}
+ \nabla\cdot(\mathbf{a}T)
= \alpha\nabla^2T + S,
$$

with a prescribed uniform velocity $\mathbf{a}$, diffusivity $\alpha$ and
constant source $S$. For constant $\mathbf{a}$ this is the heat/advection–
diffusion equation.

The solver supports structured 1D, 2D and 3D blocks, periodic or Dirichlet
boundaries, upwind/Rusanov/MUSCL convection and the generic scalar
CSV/VTK/checkpoint/exact-error output. It is registered in the CLI:
`saturnCFD -listSolvers` prints `saturnHeat`, and a case file runs with
`saturnCFD case.yaml`.

## Files

```text
src/solvers/saturnHeat/
  CMakeLists.txt       target saturnHeat
  SaturnHeat.hpp       options, solver class, profile/error diagnostics
  SaturnHeat.cpp       FVM assembly, boundary fill, RHS, profile gathering
  HeatCase.hpp         YAML schema, case factory and registration
  HeatCase.cpp         mesh, Fourier/constant data, registry entry
  README.md            this page
```

## Why the files are split

- `SaturnHeat.hpp/.cpp` is the **physics and numerics**: partition, fields,
  prescribed face velocity, convection, Laplacian, boundary refresh, RHS and
  time state. It does not include `Config`, YAML or the registry.
- `HeatCase.hpp/.cpp` is the **case layer**: the YAML schema, mesh choice,
  `heat.initial` presets (periodic Fourier mode or constant), the exact helper
  and the registry entry.
- `README.md` documents both.

The solver reuses the Burgers finite-volume operators, so the scalar transport
is tested independently of the Stokes pressure–velocity coupling.

## Boundary conditions

| `heat.periodic` | behaviour |
|---|---|
| `true` | periodic halo in every non-singleton direction |
| `false` | Dirichlet ghost values from `heat.dirichlet` (the Fourier exact field, or a constant) |

For `heat.initial: fourier` the initial condition, the Dirichlet data and the
exact field are all built from

$$
T(\mathbf{x},t)
= A\sin\!\left(\mathbf{k}\cdot\mathbf{x}-(\mathbf{a}\cdot\mathbf{k})t\right)
\exp\!\left(-\alpha|\mathbf{k}|^2t\right).
$$

## Running a YAML case

```bash
cmake --preset default
cmake --build buildCPU -j
./buildCPU/saturnCFD -listSolvers                 # saturnBurger, saturnHeat
./buildCPU/saturnCFD -dumpConfig saturnHeat
./buildCPU/saturnCFD examples/saturnHeat/saturnHeat3D/saturnHeat3D.yaml
```

The validated examples are:

```text
examples/saturnHeat/saturnHeat2D/   128^2 periodic Fourier mode
examples/saturnHeat/saturnHeat3D/   48^3  periodic Fourier mode
```

Each has a `compareAnalytic.py` that recomputes the exact solution from the
YAML and plots the numerical field, analytic field and error.

## Tests

- `tests/TestSaturnHeat.cpp` — 3D Fourier convergence and constant-field
  preservation, on 1/2/4 ranks through the CTest MPI targets.
- `tests/regression/heat2d/fourier64.yaml`,
  `tests/regression/heat3d/fourier32.yaml`, checked by
  `tests/checkHeat.cmake`.
- `tests/checkHeatCheckpoint.py` checks the generic scalar
  checkpoint/restart path.
- `ctest --preset heat`, and `ctest --preset checkpoint` for restart.

See [`docs/theory/heat.md`](../../../docs/theory/heat.md) for the discretisation
and the measured convergence tables.
