# P0b — Command-line interface

**Status:** accepted (ADR `0010`)

## Grammar

```text
saturnCFD [options] [case.yaml]
```

Options are **single-dash long names in camelCase**. They are case-sensitive.
An option that takes an argument consumes the next token.

## Options

| option | argument | description |
|---|---|---|
| `-help` | optional option name | general help, or help for one option |
| `-version` | — | version, git hash, backend and build type |
| `-listSolvers` | — | list registered solvers |
| `-checkMesh` | `mesh.msh` | validate a mesh and print the `checkMesh` report |
| `-report` | `report.json` | write the report as JSON; requires `-checkMesh` or `-checkConfig` |
| `-writeVtk` | `prefix` | write the mesh as VTK; requires `-checkMesh` |
| `-checkConfig` | `case.yaml` | validate the case and exit |
| `-dumpConfig` | `solver` | print the generated YAML template |
| `-mesh` | `mesh.msh` | give `-dumpConfig` the actual patch names |
| `-threads` | `N` | override the Kokkos thread count |

## Examples

```bash
# run a case
saturnCFD examples/burgers/traveling.yaml

# validate a mesh
saturnCFD -checkMesh meshes/channel.msh

# validate + JSON report
saturnCFD -checkMesh meshes/channel.msh -report results/channel-mesh.json

# validate + ParaView output
saturnCFD -checkMesh meshes/channel.msh -writeVtk results/channel-mesh

# validate a case without running it
saturnCFD -checkConfig examples/burgers/traveling.yaml

# generate a solver template
saturnCFD -dumpConfig burgers
saturnCFD -dumpConfig burgers -mesh meshes/channel.msh

# help
saturnCFD -help
saturnCFD -help checkMesh
```

## `-writeVtk` output

`-writeVtk output/mesh` writes:

| file | contents |
|---|---|
| `output/mesh.vtu` | volume mesh; cell data: `cellId`, `volume`, `nonOrthogonality`, `skewness`, `aspectRatio` |
| `output/mesh_<patch>.vtp` | one boundary surface per patch |
| `output/mesh.pvd` | ParaView collection referencing the volume and all patches |
| `output/mesh.json` | patch names, counts, units and quality metrics |

Open `output/mesh.pvd` in ParaView. The `.vtu` is the volume; each `.vtp` is a
named boundary patch; `patchId`/patch filenames identify the boundary.

## Help text

`saturnCFD -help` prints:

```text
saturnCFD — structured finite-volume solver

usage:
  saturnCFD <case.yaml>              run a case
  saturnCFD -checkMesh <mesh.msh>    validate a Gmsh mesh
  saturnCFD -checkConfig <case.yaml> validate a case
  saturnCFD -dumpConfig <solver>     print a YAML template
  saturnCFD -listSolvers             list solvers
  saturnCFD -help [option]           help
  saturnCFD -version                 version and build

options:
  -checkMesh <file>    structured-mesh check; prints PASS/FAIL and metrics
  -report <file.json>  JSON report; use with -checkMesh or -checkConfig
  -writeVtk <prefix>   write .vtu/.vtp/.pvd for ParaView; use with -checkMesh
  -mesh <file>         mesh patch names for -dumpConfig
  -threads <N>         Kokkos thread count
```

`saturnCFD -help checkMesh` adds the preconditions, the report fields, the
error categories and a complete example.

## Errors and exit codes

| code | meaning |
|---|---|
| 0 | success / PASS |
| 1 | mesh or case error, or runtime failure |
| 2 | usage error: unknown option, missing argument, invalid combination |

An unknown option prints:

```text
error: unknown option '-checkmesh'
hint: did you mean '-checkMesh'? see 'saturnCFD -help'
```

`-report` without `-checkMesh`/`-checkConfig` and `-writeVtk` without
`-checkMesh` are usage errors.

## Tests

- every option has a parser test;
- `-help` and `-help <option>` exit zero;
- unknown option suggests the closest known option;
- invalid combinations return 2;
- `-checkMesh` on good/bad meshes returns 0/1;
- `-writeVtk` writes a `.pvd` that references the volume and all patches.
