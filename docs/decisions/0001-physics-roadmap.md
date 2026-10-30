# 0001 — Physics roadmap: scalar to vector to incompressible

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/README.md`
- **Tests:** `tests/solvers/...`

## Context

v1 is a coursework project that must be validated step by step. Jumping
straight to incompressible Navier–Stokes makes it difficult to separate a bug
in the mesh, the finite-volume operators, the boundary conditions, the linear
solver or the pressure–velocity coupling. Each earlier PDE is a controlled
test of one more ingredient.

The user also wants each PDE to live in its own solver module and to be driven
by a YAML case file, with the same mesh, finite-volume, linear-algebra and I/O
substrate underneath.

## Decision

The v1 solver sequence is:

1. **Burgers** — scalar first, then vector. Tests convection, shock capturing,
   limiters, diffusion and time integration.
2. **Heat / convection–diffusion** — tests scalar transport, boundary layers,
   Péclet-number effects and source terms.
3. **Stokes** — tests the vector elliptic operator, the saddle-point pressure
   coupling and the linear solver, without nonlinearity or time accuracy.
4. **Incompressible Navier–Stokes** — combines all of the above with the
   pressure–velocity coupling.

Each is a separate solver under `src/solvers/<name>/`:

```text
src/solvers/burgers/
src/solvers/heat/
src/solvers/stokes/
src/solvers/incompressible/
```

They share `src/mesh`, `src/fv`, `src/config`, `src/linalg` and `src/io`.
Compressible flow is out of v1.0.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Start with incompressible Navier–Stokes | too many failure modes at once; validation becomes ambiguous |
| Implement all PDEs in parallel | no stable substrate to share; duplicated debugging |
| Put all solvers in one source file | violates the requirement that each PDE is a separate solver |
| Start with compressible flow | adds thermodynamics and Riemann solvers before the incompressible path is trusted |

## Consequences

- **Code:** a shared finite-volume layer and a solver registry; no solver may
  own its own mesh, linear algebra or YAML parser.
- **Validation:** each solver has a gate before the next one starts.
- **Schedule:** Burgers is the first deliverable; incompressible NS is the last
  v1.0 milestone.
- **Risk:** the shared layer must be designed for all four from the start, not
  patched per solver.

## Validation / exit criteria

| solver | first gate |
|---|---|
| Burgers | viscous traveling wave and inviscid shock; observed order and conservation |
| Heat | 1D steady boundary layer and 2D/3D manufactured solution |
| Stokes | Poiseuille, Couette and cavity at \(Re\to0\); divergence norm |
| Incompressible | Ghia cavity at \(Re=100\), then \(Re=1000\); Taylor–Green vortex |

## References

- Burgers (1948); Hopf (1950); Cole (1951).
- Patankar (1980); Ferziger & Perić (2002).
- Ghia, Ghia & Shin (1982).
