# saturnCFD {#mainpage}

Structured finite-volume CFD for scalar transport, Stokes flow and
incompressible Navier–Stokes, driven by YAML and validated against analytic
solutions.

**Status:** Phase 1 in progress. Mesh, `checkMesh`, SI-only validation,
yaml-cpp schema validation, CLI help, VTK mesh export, MPI partition/halo and
the CPU Kokkos FVM slice (`CellField`, `FaceField`, `LocalMesh`, interpolation,
gradient, divergence, non-orthogonal Laplacian, boundary ghosts,
upwind/Rusanov/MUSCL convection, MMS and a simplified Rhie–Chow flux) and the
Phase 0 PETSc `saturnLinAlg` layer are in place. Burgers/heat solvers follow.
Solver physics starts in Phase 2.

## Where to start

- [Design summary](Design-v1.md)
- [Repository layout](plan/repositoryLayout.md)
- [Roadmap](plan/v1-roadmap.md)
- [ADR log](decisions/README.md)
- [Protocols](protocols/README.md)
- [Theory pages](theory/README.md)
- [PETSc setup and saturnLinAlg](guides/petsc.md)
- [Solver implementation sequence](plan/solverSequence.md)
- [Poisson matrix and nullspace](theory/poisson.md)
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
| solvers | Burgers, heat, Stokes, incompressible |
| extension | copy a solver under `src/solvers`, add `addSolver(name)` |
| docs | this Doxygen site, built to HTML and PDF |

## Building the docs

```bash
mkdir -p buildCPU
doxygen docs/Doxyfile
# HTML: buildCPU/doxygen/html/index.html
# PDF:  make -C buildCPU/doxygen/latex
```

The GitHub Pages site is published from the `docs.yml` workflow.
