# v1 protocols

A **protocol** is a stable contract between two parts of the code. It says
what is guaranteed, not how it is implemented. Protocols are written before
physics so that the same substrate can carry Stokes, incompressible and
compressible solvers without being rewritten for each one.

## What every protocol must contain

1. **Purpose** — what problem the protocol solves.
2. **Vocabulary** — the terms used by the contract, defined once.
3. **Interface** — the C++ signatures, concepts or abstract classes.
4. **Invariants** — what must always be true, checked by assertions/tests.
5. **Ownership and lifetime** — who allocates, who frees, what may be copied.
6. **Host/device rules** — what may be called from a Kokkos kernel.
7. **Error behaviour** — exceptions, aborts, or error codes; what is legal on
   the device.
8. **Parallel behaviour** — local vs global, rank ownership, reductions.
9. **Tests** — the unit, analytic or regression test that enforces it.
10. **References** — papers or reference codes that justify the design.

## Proposed protocol stack

| id | protocol | depends on | status |
|---|---|---|---|
| P0 | project, process, style and citation | — | proposed |
| P1 | core types, errors, logging, runtime | P0 | proposed |
| P2 | index space: `Box`, centering, ghosts, `Array4` | P1 | proposed |
| P3 | field: `MultiFab`, `State`, ownership, memory | P2 | proposed |
| P4 | parallel: distribution, halo, reductions | P3 | proposed |
| P5 | configuration: schema, defaults, validation | P1 | proposed |
| P6 | solver lifecycle, time step, diagnostics, restart | P3, P5 | proposed |
| P7 | linear operator and elliptic solver | P3, P4 | proposed |
| P8 | I/O: VTK, CSV, checkpoint, run manifest | P3 | proposed |
| P9 | verification: unit, MMS, regression, parity | all | proposed |
| P10 | physics models: Stokes, incompressible, compressible | P3, P6, P7 | proposed |

## Order of work

1. Decide P0–P5 as ADRs. No code beyond scaffolding.
2. Write the P1–P3 headers with documentation and invariants, but no physics.
3. Implement `Box`, `Array4`, `MultiFab` and their tests.
4. Implement the parallel and configuration protocols.
5. Implement the linear-solver protocol.
6. Implement Stokes as the first physics model.
7. Add incompressible, then compressible, behind the same solver protocol.

## Non-negotiable rules

- No physics in a protocol. A protocol must be meaningful for a solver that
  does not exist yet.
- No hidden allocation or host/device copy in a documented hot path.
- No `#ifdef` for physics. Backend and dimension switches are the only
  compile-time branches allowed in the substrate.
- Every public function is camelCase and documented; every numerical choice
  cites a reference.
- No code from GPL projects. Basilisk, NekRS and Nektar++ are read for
  methodology only; cite the paper or manual, never copy source.
