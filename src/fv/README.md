# fv

Shared finite-volume operators. Every solver uses this layer; none of them
reimplements a gradient or a flux.

## Responsibilities

- `CellField`, `FaceField`, `VectorCellField`.
- Gradient, divergence, interpolation, non-orthogonal Laplacian.
- Convective fluxes: upwind, Rusanov, MUSCL/TVD.
- Rhie–Chow face flux for the collocated pressure coupling.
- Boundary-condition application.

## Boundaries

- No solver lifecycle, no YAML parsing, no file I/O.
- Operators take fields and workspaces; no hidden allocation in a time loop.
- No unit conversion: fields are SI.

## Tests

- `tests/fv/` — constant/linear fields, divergence theorem, manufactured
  Poisson, limiter behaviour.

## Docs

- `docs/protocols/finiteVolume.md`
- `docs/theory/finiteVolume.md`
- ADR `0004`
