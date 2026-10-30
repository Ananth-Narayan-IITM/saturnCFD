# apps

The `saturnCFD` driver.

## Responsibilities

- Parse the single-dash CLI.
- Load and validate YAML before allocation.
- Select a solver from the registry.
- Run `Simulation`.
- Dispatch `-checkMesh`, `-checkConfig`, `-dumpConfig`, `-writeTemplate`,
  `-report`, `-writeVtk`, `-help`, `-version`, `-listSolvers`.

## Boundaries

- No physics.
- No mesh topology code beyond calling `mesh`.
- No YAML schema definitions; solvers and common modules register them.

## Tests

- `tests/cli/` — option parsing, help, exit codes.
- `tests/regression/cli/` — command output and files.

## Docs

- `docs/protocols/cli.md`
- ADR `0010`
