# saturnCFD {#mainpage}

Structured finite-volume CFD for scalar transport, Stokes flow and
incompressible Navier–Stokes, driven by YAML and validated against analytic
solutions.

**Status:** planning. No solver code is written yet.

## Where to start

- [Design summary](Design-v1.md)
- [Repository layout](plan/repositoryLayout.md)
- [Roadmap](plan/v1-roadmap.md)
- [ADR log](decisions/README.md)
- [Protocols](protocols/README.md)
- [Theory pages](theory/README.md)
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
mkdir -p build
doxygen docs/Doxyfile
# HTML: build/doxygen/html/index.html
# PDF:  make -C build/doxygen/latex
```

The GitHub Pages site is published from the `docs.yml` workflow.
