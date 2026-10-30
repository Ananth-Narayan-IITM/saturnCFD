# 0009 — Documentation, equations and citations

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/documentation.md`
- **Tests:** documentation build

## Context

The coursework is graded on the quality of the code and its presentation. The
user requires camelCase functions, full documentation, the equations used, and
reference citations. A numerical method without a derivation and a reference is
not considered complete.

## Decision

- **Style:** functions `camelCase`; types `PascalCase`; members `m` +
  `PascalCase`; constants `k` + `PascalCase`; `enum class`; `#pragma once`.
- **API docs:** Doxygen for every public class and function: `@brief`,
  `@param`, `@return`, `@pre`, `@post`, `@note`, `@warning`.
- **Equations:** Doxygen `\f$...\f$` and `\f[...\f]`, MathJax enabled. Theory
  pages in Markdown use `$...$` inline and `$$ ... $$` display, not `\(...\)`
  or `\[...\]`, so GitHub/VS Code/Obsidian render them.
- **Theory pages:** each solver has `docs/theory/<solver>.md` with governing
  equations, assumptions, non-dimensionalisation, discretisation, boundary
  conditions, time integration, stability, linearisation, verification,
  validation, observations and references.
- **Observations:** each validation page records measured order, error norms,
  anomalies and numerical artefacts. This is a lab notebook, not a marketing
  page.
- **Citations:** `docs/references.bib`, Doxygen `CITE_BIB_FILES`, `\cite{key}`
  in code, `[@key]` in Markdown. Every entry is verified before it is committed.
- **Reference-code policy:** NekRS, Nektar++, Basilisk and OpenFOAM are read for
  methodology and cited. **No GPL source code is copied.** Basilisk is GPL;
  saturnCFD is Apache-2.0.
- **ADRs:** every expensive decision gets a numbered record in
  `docs/decisions/`.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Comments only | no equation rendering, no bibliography, no cross-linking |
| A separate report written at the end | drifts from the code and misses the observations |
| Copying reference-code headers | licence violation and no understanding |
| No ADRs | decisions become folklore; hard to present or defend |

## Consequences

- **Code:** Doxygen comments are mandatory in review; a function without a
  theory link is incomplete.
- **Docs:** `docs/theory`, `docs/validation`, `docs/decisions`,
  `docs/references.bib`.
- **Presentation:** the coursework report can be assembled from verified theory
  and validation pages instead of being written from scratch.
- **Risk:** documentation takes time; the protocol makes it part of "done".

## Validation / exit criteria

- Doxygen builds without warnings for public API.
- Every theory page has at least one citation.
- Every solver has a validation page with a table of results.
- `clang-format` passes; `clang-tidy` is optional in v1.0.

## References

- Doxygen manual.
- BibTeX and `biblatex` documentation.
