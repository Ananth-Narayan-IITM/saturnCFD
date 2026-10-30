# Architecture decision records

Every decision that would be expensive to reverse gets a numbered record here
**before** the code that depends on it is written. The goal is control: a
decision is visible, argued, dated, and can be superseded without archaeology.

## Status values

| status | meaning |
|---|---|
| `proposed` | under discussion; do not build on it yet |
| `accepted` | frozen; code may depend on it |
| `rejected` | considered and declined; keep the reason |
| `superseded` | replaced by a later ADR; link both ways |

## Rules

1. One decision per record. If a record needs "and", split it.
2. Record the alternatives, not only the winner.
3. State the consequence for the code: which module, which interface, which
   test changes.
4. An accepted ADR is not edited. Change it by writing a new ADR that
   supersedes it.
5. Link the protocol spec and the tests that enforce the decision.

## Numbering

`0001`, `0002`, ... in creation order. The template is `0000-template.md`.

## Decision log

| ADR | decision | status |
|---|---|---|
| `0001` | Physics roadmap: Burgers → heat → Stokes → incompressible | accepted |
| `0002` | Structured hexahedral `.msh` only; 2D is one cell thick; no AMR in v1.0 | accepted |
| `0003` | PETSc as the linear-algebra backend; no home-grown Krylov | accepted |
| `0004` | Collocated cell-centred state; Rhie–Chow; SIMPLE/PISO | accepted |
| `0005` | `Solver` lifecycle and registry; `Simulation` owns the time loop | accepted |
| `0006` | Typed config schema, generated templates, strict validation | accepted |
| `0007` | `checkMesh` module and structured-mesh detection | accepted |
| `0008` | SI units from the start; units parsed at the config boundary | accepted |
| `0009` | Documentation, equations and citation protocol | accepted |
| `0010` | CLI: single-dash camelCase options, `-help`, mesh VTK export | accepted |

The remaining open items are the YAML parser choice (yaml-cpp recommended), the
test framework, and the exact GPU/MPI milestone. They do not block the mesh,
Burgers or heat work.
