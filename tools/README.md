# tools

Cross-platform C++ and Python tools. No Bash-only workflow.

| tool | purpose |
|---|---|
| `saturnCFDSolver.cpp` | build target `saturnCFDSolver`; copies a solver and prints `addSolver(name)` |
| `doxygenMathFilter.py` | converts Markdown `$...$` / `$$...$$` to Doxygen `\f$...\f$` / `\f[...\f]` |

## Rules

- Prefer C++ or Python over shell scripts so the tool works on every machine.
- A tool that modifies the source tree prints exactly what it changed.
- No tool writes outside the repository unless the user asks.
- Tools are documented in `docs/guides/` or `docs/plan/`.

## Tests

- `tests/tools/` — scaffold copy/rename, math filter output.
