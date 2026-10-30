# Documentation and formatting pipeline

**Status:** accepted (ADR `0013`).

## Goals

1. One source for API docs, theory pages, protocols, ADRs and validation
   records.
2. HTML for browsing and GitHub Pages hosting.
3. PDF for submission and offline reading.
4. Equations that render in both Markdown viewers and Doxygen.
5. clang-format enforced for every C++ file.

## Source of truth

`docs/` is the documentation source. Doxygen reads:

- `README.md`;
- `docs/**/*.md`;
- `src/**/*.hpp` and `src/**/*.cpp` once the source exists.

`docs/index.md` is the Doxygen main page.

## Markdown math

Markdown pages use `$...$` inline and `$$ ... $$` display so they render in
GitHub, VS Code, Obsidian and Pandoc.

Doxygen does not parse those delimiters, so the Doxygen build runs a small
input filter:

```text
tools/doxygenMathFilter.py
```

It converts, outside code blocks and inline code:

- `$...$`  -> `\f$...\f$`
- `$$...$$` -> `\f[...\f]`
- `[@key]` -> `\cite{key}`

The Markdown source stays GitHub-friendly; the filter adapts it for Doxygen.
The bibliography is `docs/references.bib`, wired through `CITE_BIB_FILES`.

## Doxygen

`docs/Doxyfile` sets:

```text
GENERATE_HTML   = YES
GENERATE_LATEX  = YES
USE_PDFLATEX    = YES
USE_MATHJAX     = YES
CITE_BIB_FILES  = docs/references.bib
INPUT           = README.md docs src
OUTPUT_DIRECTORY= build/doxygen
```

CMake targets (planned):

| target | output |
|---|---|
| `docHtml` | `build/doxygen/html/index.html` |
| `docPdf` | `build/doxygen/latex/refman.pdf` |
| `doc` | both |
| `format` | runs clang-format on tracked C++ files |

## GitHub Pages

`.github/workflows/docs.yml`:

- runs on push to `v1`/`main` and on `workflow_dispatch`;
- installs Doxygen and Graphviz;
- runs `mkdir -p build && doxygen docs/Doxyfile`;
- uploads `build/doxygen/html` with `actions/upload-pages-artifact`;
- deploys with `actions/deploy-pages`.

The Pages URL is:

```text
https://ananth-narayan-iitm.github.io/saturnCFD/
```

Manual repository setting (once): **Settings -> Pages -> Build and deployment ->
Source: GitHub Actions**.

The PDF is built on `workflow_dispatch` and uploaded as a workflow artifact.
It is not rebuilt on every push because the TeX installation is large.

## clang-format

- `.clang-format` is the single style file.
- Local command:
  ```bash
  clang-format -i $(git ls-files '*.hpp' '*.cpp')
  ```
- CMake target:
  ```bash
  cmake --build build --target format
  ```
- `.github/workflows/format.yml` runs:
  ```bash
  clang-format --dry-run --Werror $(git ls-files '*.hpp' '*.cpp')
  ```
- A change that fails the format check is not merged.

`clang-tidy` is optional in v1.0; the config file exists, but it is not a
blocking check until the source is stable.

## Doxygen comments in code

```cpp
/**
 * @brief Apply the negative Laplacian to a cell field.
 *
 * Computes \f$ A\phi = -\nabla^2\phi \f$ on every valid cell.
 *
 * @param[in]  phi  cell-centred scalar
 * @param[out] out  result on the same mesh
 * @pre  phi and out have the same mesh and component count
 * @post out is defined on every valid cell
 */
```

Use `\f$...\f$` in C++ comments, `$...$` in Markdown. The filter only touches
Markdown.

## Documentation layout

```text
docs/
  Doxyfile
  index.md
  Design-v1.md
  Reference-v0.md
  decisions/
  protocols/
  theory/
  guides/
  validation/
  plan/
  references.bib
```

Every page has a status line and links to the ADR or protocol it implements.

## Validation

- `mkdir -p build && doxygen docs/Doxyfile` builds HTML without missing-input
  errors.
- `make -C build/doxygen/latex` produces `refman.pdf`.
- The Pages workflow deploys and the URL loads.
- A `$...$` equation appears as an equation in HTML and PDF.
- `clang-format --dry-run --Werror` passes on tracked C++ files.
- The format workflow fails on a deliberately misformatted file.
