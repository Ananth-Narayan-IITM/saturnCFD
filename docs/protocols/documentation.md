# P0 — Documentation and citation protocol

**Status:** accepted (ADR `0009`)

## Style

- Functions: `camelCase`.
- Types and classes: `PascalCase`.
- Member variables: `m` + `PascalCase`.
- Constants: `k` + `PascalCase`.
- Enumerations: `enum class` with `PascalCase` enumerators.
- Headers: `#pragma once`; file name matches the primary type.
- Formatting: `.clang-format`; static checks with `clang-tidy`.

## API documentation

Every public class and function has a Doxygen comment:

```cpp
/**
 * @brief Apply the negative Laplacian to a cell field.
 *
 * Computes @f$ A\phi = -\nabla^2\phi @f$ with the boundary conditions
 * supplied by the operator. See docs/theory/finiteVolume.md.
 *
 * @param[in]  phi  cell-centred scalar field with ghost values filled
 * @param[out] out  cell-centred result
 * @pre  phi and out have the same mesh and component count
 * @post out is defined on every valid cell
 * @note The operator is matrix-free; assemble separately for PETSc.
 */
```

## Theory pages

Each solver and shared numerical module has a page in `docs/theory/` with:

1. governing equations in dimensional form;
2. assumptions and non-dimensionalisation;
3. finite-volume discretisation;
4. boundary and initial conditions;
5. time integration and stability;
6. linearisation and solution algorithm;
7. verification and validation;
8. observed behaviour and numerical artefacts;
9. references.

## Citations

- `docs/references.bib` is the single bibliography.
- Doxygen uses `\cite{key}`; Markdown uses `[@key]`.
- Every entry is verified (authors, title, journal, volume, pages, DOI) before
  it is committed.
- The theory page names the equation number or section it uses.

## Reference-code policy

NekRS, Nektar++, Basilisk, OpenFOAM and AMReX may be read for methodology and
cited. **No source code is copied from a GPL project.** Basilisk is GPL;
saturnCFD is Apache-2.0. Algorithms are reimplemented from papers and
documented derivations.

## Definition of done

A numerical feature is done only when:

- the API is documented;
- the theory page exists and has a citation;
- a unit or manufactured-solution test passes;
- a validation case is recorded in `docs/validation/`;
- the observed behaviour is written down, including anything that did not work.
