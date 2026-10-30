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
2. Record the alternatives, not only the winner. The rejected options are the
   most useful part six months later.
3. State the consequence for the code: which module, which interface, which
   test changes.
4. An accepted ADR is not edited. Change it by writing a new ADR that
   supersedes it.
5. Link the protocol spec and the tests that enforce the decision.

## Numbering

`0001`, `0002`, ... in creation order. The template is `0000-template.md`.

## Proposed first batch

The keystone decisions are the ones that are hard to reverse after the data
model exists:

- `0001` — target scope and physics roadmap
- `0002` — dimension, AMR and MPI strategy
- `0003` — precision, backends and dependency policy
- `0004` — state representation and field centering
- `0005` — solver lifecycle and runtime selection
- `0006` — configuration schema and validation
- `0007` — index-space, ghost and memory protocol
- `0008` — verification, documentation and citation protocol

The remaining decisions (I/O formats, logging, error handling, testing
framework, CI) follow once these are accepted.
