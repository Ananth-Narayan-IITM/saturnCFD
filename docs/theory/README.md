# Theory pages

This directory is the derivation and validation record for v1. A solver is not
complete until its page here contains the equations, the discretisation, the
validation results and the observations.

## Pages

| page | contents |
|---|---|
| [`finiteVolume.md`](finiteVolume.md) | cell-centred finite-volume operators, face interpolation, Rhie–Chow, SIMPLE/PISO |
| [`burgers.md`](burgers.md) | scalar and vector Burgers, exact solutions, shocks, limiters |
| [`heat.md`](heat.md) | convection–diffusion, boundary layers, manufactured solutions |
| [`stokes.md`](stokes.md) | Stokes equations, SIMPLE, Poiseuille/Couette |
| [`incompressible.md`](incompressible.md) | incompressible Navier–Stokes, PISO, cavity |
| [`units.md`](units.md) | SI convention, dynamic pressure, dimensionless groups |

## Math rendering

All Markdown pages use the widely supported math syntax:

- inline: `$...$`
- display: `$$ ... $$` on separate lines

This renders in GitHub, VS Code Markdown preview (with math enabled), Obsidian,
Pandoc, Typora and most Markdown viewers with MathJax/KaTeX. Do not use the
`\(...\)` or `\[...\]` delimiters; they are not rendered by many viewers.

## Notation

| symbol | meaning | SI unit |
|---|---|---|
| $\mathbf{u}$ | velocity vector | m/s |
| $p$ | dynamic pressure | Pa |
| $\rho$ | density | kg/m³ |
| $\mu$ | dynamic viscosity | Pa·s |
| $\nu=\mu/\rho$ | kinematic viscosity | m²/s |
| $T$ | temperature | K |
| $\alpha$ | thermal diffusivity | m²/s |
| $\mathbf{f}$ | body force per unit volume | N/m³ |
| $V_P$ | cell volume | m³ |
| $A_f$ | face area | m² |
| $\mathbf{n}_f$ | outward face unit normal | — |
| $\phi$ | generic scalar | — |

## Equation policy

Each page gives the dimensional equation first. A dimensionless form is given
only when it is used for a benchmark, and the reference scales are stated
explicitly. See [`units.md`](units.md).

## Validation record

Each page has a **Validation** section with:

- case definition and reference;
- mesh, scheme, time step and tolerances;
- a table of computed versus reference values;
- observed order of accuracy;
- **Observations** — what the run actually showed, including artefacts and
  failures. This is deliberately a lab notebook, not a success-only summary.

## References

The bibliography is [`../references.bib`](../references.bib). In this directory,
citations use `[@key]`; in code, Doxygen uses `\cite{key}`.
