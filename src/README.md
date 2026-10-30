# src/

The shared substrate. Solvers live in `src/solvers/`; everything else here is
reused by every solver.

| directory | responsibility |
|---|---|
| `core/` | types, units, errors, logging, timers, physical properties |
| `config/` | yaml-cpp source, schema, binding, validation, template generation |
| `mesh/` | Gmsh reader, `Mesh`, `checkMesh`, `StructuredMesh`, VTK export |
| `comm/` | `MeshPartition`, MPI halo exchange, rank/device runtime |
| `fv/` | cell/face fields, finite-volume operators, fluxes, Rhie–Chow, BCs |
| `linalg/` | PETSc wrappers, matrix assembly, linear-solver interface |
| `io/` | VTU/PVD, CSV diagnostics, checkpoints, run manifest |
| `simulation/` | `Solver` interface, registry, time loop |
| `solvers/` | one self-contained directory per PDE |

Each directory has its own `README.md` with its contract and boundaries.
