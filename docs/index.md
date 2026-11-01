# saturnCFD {#mainpage}

<img src="saturnCFD.svg" alt="saturnCFD logo" width="140" align="right">

Structured finite-volume CFD for scalar transport, Stokes flow and
incompressible Navier–Stokes, driven by YAML and validated against analytic
solutions.

**Status:** Phase 1 in progress. Mesh, `checkMesh`, SI-only validation,
yaml-cpp schema validation, CLI help, VTK mesh export, MPI partition/halo and
the CPU Kokkos FVM slice (`CellField`, `FaceField`, `LocalMesh`, interpolation,
gradient, divergence, non-orthogonal Laplacian, boundary ghosts,
upwind/Rusanov/MUSCL convection, MMS and a simplified Rhie–Chow flux), the
Phase 0 PETSc `saturnLinAlg` layer, the explicit solver/time loop, the
1D/2D/3D Burgers solver and the `saturnHeat` scalar convection–diffusion
solver are in place. Both scalars are registered in the CLI and validated
against analytic solutions, with VTK output and checkpoint/restart.
`saturnHeat` is checked against a periodic Fourier mode in 2D and 3D, with
the rank-0 mesh load and local-geometry distribution used by both solvers.
Implicit time schemes and the Stokes solver follow.

## Where to start

- [Design summary](Design-v1.md)
- [Repository layout](plan/repositoryLayout.md)
- [Roadmap](plan/v1-roadmap.md)
- [ADR log](decisions/README.md)
- [Protocols](protocols/README.md)
- [Theory pages](theory/README.md)
- [Host requirements](guides/requirements.md)
- [PETSc setup and saturnLinAlg](guides/petsc.md)
- [CPU/GPU builds, threads and benchmarking](guides/gpu.md)
- [Solver implementation sequence](plan/solverSequence.md)
- [Poisson matrix and nullspace](theory/poisson.md)
- [Time integration](theory/timeIntegration.md)
- [Writing a solver](guides/writingASolver.md)
- [Validation record](validation/README.md)

## What v1 is

| area | decision |
|---|---|
| mesh | structured hexahedral Gmsh `.msh`; 2D is one cell thick |
| state | collocated cell-centred velocity vector and dynamic pressure |
| coupling | Rhie–Chow face flux; SIMPLE/PISO |
| linear algebra | PETSc |
| YAML | yaml-cpp 0.8, generated templates, strict validation |
| units | SI only in v1.0 |
| solvers | `saturnBurger` (1D), then heat, Stokes, incompressible |
| extension | copy a solver under `src/solvers/saturn<Name>`, add `addSolver(<name>)` |
| docs | this Doxygen site, built to HTML and PDF |

## Building the docs

```bash
mkdir -p buildCPU
doxygen docs/Doxyfile
# HTML: buildCPU/doxygen/html/index.html
# PDF:  make -C buildCPU/doxygen/latex
```

The GitHub Pages site is published from the `docs.yml` workflow.
