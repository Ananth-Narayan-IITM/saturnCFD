# 0013 — Documentation pipeline: Doxygen HTML+PDF, Pages, clang-format

- **Status:** accepted
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Protocol:** `docs/plan/documentationPipeline.md`
- **Tests:** `docs.yml`, `format.yml`

## Context

The project needs API documentation, theory pages with equations, ADRs and
validation records. The user wants Doxygen HTML and PDF, the HTML hosted on
GitHub, Markdown equations that render in normal viewers, and consistent C++
formatting.

Markdown viewers such as GitHub, VS Code and Obsidian render `$...$` and
`$$...$$`, but Doxygen expects `\f$...\f$` and `\f[...\f]`. A single source must
serve both without duplicating equations.

## Decision

1. **Doxygen is the documentation generator.**
   - `docs/Doxyfile` is the single configuration.
   - HTML uses MathJax.
   - PDF uses Doxygen's LaTeX output with `USE_PDFLATEX=YES`.
2. **Markdown is the source for prose and equations.**
   - `$...$` inline and `$$ ... $$` display in Markdown.
   - `tools/doxygenMathFilter.py` converts them to `\f$...\f$` / `\f[...\f]`
     for the Doxygen build only.
   - The filter also converts `[@key]` to `\cite{key}`.
3. **GitHub Pages hosts the HTML.**
   - `.github/workflows/docs.yml` builds on push to `v1`/`main` and on
     `workflow_dispatch`.
   - It uploads the HTML as the `saturnCFD-docs-html` artifact.
   - It publishes `buildCPU/doxygen/html` to the `gh-pages` branch with
     `peaceiris/actions-gh-pages`.
   - The one-time setting is **Settings -> Pages -> Deploy from a branch ->
     Branch: `gh-pages` / (root)**.
   - The workflow does **not** call the Pages create-site API. A workflow token
     cannot create a Pages site (`Resource not accessible by integration`).
   - Workflow permissions must allow branch writes: **Settings -> Actions ->
     General -> Workflow permissions -> Read and write permissions**.
   - The URL is `https://ananth-narayan-iitm.github.io/saturnCFD/`.
   - The PDF is built on `workflow_dispatch` and uploaded as an artifact.
4. **clang-format is mandatory.**
   - `.clang-format` is the source of truth.
   - `formatCode` CMake target rewrites tracked files.
   - `.github/workflows/format.yml` runs `--dry-run --Werror`.
5. **CMake owns the targets.**
   - `docHtml`, `docPdf`, `doc`, `formatCode`.
6. **Doxygen comments in C++ use `\f$...\f$`.** Markdown uses `$...$`.
   The filter only touches Markdown, never source code.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| MkDocs/Sphinx | excellent for prose, weak for C++ API without extra work; Doxygen is the natural fit |
| Doxygen only, no Markdown filter | `$` equations would not render in HTML/PDF |
| Markdown `\f$` everywhere | GitHub/VS Code would show raw delimiters |
| Manual PDF export | drifts from the source and is not reproducible |
| `actions/deploy-pages` from Actions artifacts | requires the Pages site to exist; a workflow token cannot create it (`Resource not accessible by integration`) |
| No format CI | style drift; the user explicitly asked for consistency |

## Consequences

- **Docs:** one source, two outputs (HTML and PDF).
- **CI:** `contents: write` to publish the `gh-pages` branch, and a manual
  Pages source setting once (`gh-pages` branch / root).
- **Math:** the filter must handle code fences and inline code correctly; it is
  tested on the theory pages.
- **PDF:** TeX dependencies are heavy; PDF is manual/release, not every push.
- **Formatting:** every C++ change runs clang-format; the CI check is blocking.
- **Risk:** Doxygen's Markdown support is weaker than a dedicated site
  generator; the filter is the small compatibility layer.

## Validation / exit criteria

- `doxygen docs/Doxyfile` builds HTML with no missing-input errors.
- `make -C buildCPU/doxygen/latex` produces `refman.pdf`.
- A page with `$...$` renders as an equation in HTML and PDF.
- The Pages workflow deploys and the URL loads.
- `clang-format --dry-run --Werror` passes on tracked C++ files.
- The format workflow fails on a deliberately misformatted file.

## References

- Doxygen manual: https://www.doxygen.nl/manual/
- GitHub Pages with Actions: https://docs.github.com/pages
- clang-format: https://clang.llvm.org/docs/ClangFormat.html
