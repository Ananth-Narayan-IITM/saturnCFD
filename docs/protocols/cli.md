# P0b — Command-line interface

**Status:** accepted (ADR `0010`)

## Grammar

```text
saturnCFD [options] [case.yaml]
```

Options are **single-dash long names in camelCase**. They are case-sensitive.
An option that takes an argument consumes the next token. There are no
subcommands: everything is `saturnCFD -option ...`.

## Options

| option | argument | description |
|---|---|---|
| `-help` | optional topic | general help, or detailed help for one option/topic |
| `-version` | — | version, git hash, backend and build type |
| `-listSolvers` | — | list registered solvers |
| `-checkMesh` | `mesh.msh` | validate a mesh and print the `checkMesh` report |
| `-report` | `report.json` | write the report as JSON; requires `-checkMesh` or `-checkConfig` |
| `-writeVtk` | `prefix` | write the mesh as VTK; requires `-checkMesh` |
| `-checkConfig` | `case.yaml` | validate the case and exit |
| `-dumpConfig` | `solver` | print the generated YAML template |
| `-writeTemplate` | `file.yaml` | write the generated template; requires `-dumpConfig` |
| `-mesh` | `mesh.msh` | give `-dumpConfig` the actual patch names |
| `-threads` | `N` | override the Kokkos thread count |

## Examples

```bash
# run a case
saturnCFD examples/burgers/traveling.yaml

# validate a mesh
saturnCFD -checkMesh meshes/channel.msh

# validate + JSON report
saturnCFD -checkMesh meshes/channel.msh -report results/channelMesh.json

# validate + ParaView output
saturnCFD -checkMesh meshes/channel.msh -writeVtk results/channelMesh

# validate a case without running it
saturnCFD -checkConfig examples/burgers/traveling.yaml

# print or write a solver template
saturnCFD -dumpConfig burgers
saturnCFD -dumpConfig burgers -writeTemplate burgersTemplate.yaml
saturnCFD -dumpConfig burgers -mesh meshes/channel.msh -writeTemplate burgers.yaml

# help
saturnCFD -help
saturnCFD -help checkMesh
saturnCFD -help dumpConfig
saturnCFD -help units
saturnCFD -help config
saturnCFD -help mesh
```

## Template generation

`-dumpConfig <solver>` prints a commented YAML template generated from the
solver's option registration. `-writeTemplate <file>` writes the same content to
a file. There is no hand-maintained template file.

```bash
saturnCFD -dumpConfig burgers -writeTemplate burgersTemplate.yaml
```

The template contains:

- the top-level `solver:` key;
- the `units:` block with `system: SI`;
- common groups: `mesh`, `time`, `fields`, `boundary`, `numerics`,
  `linearSolver`, `output`;
- solver-specific groups and every option the solver reads;
- defaults, types, units and one-line descriptions.

With `-mesh <file>`, the `boundary:` section is populated with the actual patch
names from the mesh and a valid BC skeleton.

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

## Units

v1.0 accepts SI only:

```yaml
units:
  system: SI
```

The block is optional; omitting it means SI. Any other system, and any
per-quantity unit such as `length: mm`, is a validation error. No conversion is
performed in v1.0. `-help units` prints the full rule.

## Help text

`saturnCFD -help` prints a general page with the synopsis, every option, file
formats, the SI-only rule and examples.

```text
saturnCFD — structured finite-volume solver

usage:
  saturnCFD <case.yaml>                 run a case
  saturnCFD -checkMesh <mesh.msh>       validate a Gmsh mesh
  saturnCFD -checkConfig <case.yaml>    validate a case
  saturnCFD -dumpConfig <solver>        print a YAML template
  saturnCFD -writeTemplate <file>       write the template (with -dumpConfig)
  saturnCFD -listSolvers                list solvers
  saturnCFD -help [topic]               help
  saturnCFD -version                    version and build

options:
  -checkMesh <file>    structured-mesh check; PASS/FAIL plus metrics
  -report <file.json>  JSON report; use with -checkMesh or -checkConfig
  -writeVtk <prefix>   .vtu/.vtp/.pvd for ParaView; use with -checkMesh
  -mesh <file>         patch names for -dumpConfig
  -threads <N>         Kokkos thread count

notes:
  - units are SI only in v1.0; non-SI input is rejected
  - case templates are generated from the solver code
  - open the .pvd, not the individual .vtu/.vtp, in ParaView

examples:
  saturnCFD examples/burgers/traveling.yaml
  saturnCFD -checkMesh meshes/channel.msh -writeVtk results/channelMesh
  saturnCFD -dumpConfig burgers -writeTemplate burgers.yaml
```

### Example: `-help checkMesh`

```text
saturnCFD -checkMesh <mesh.msh>

Validate a Gmsh mesh and print a checkMesh report. The mesh must be a
structured hexahedral mesh. Tetrahedra, prisms, pyramids and non-contiguous
hex meshes are rejected.

arguments:
  <mesh.msh>            Gmsh 2.2 or 4.1 ASCII file

optional:
  -report <file.json>   write the report as JSON
  -writeVtk <prefix>    write .vtu/.vtp/.pvd for ParaView

output:
  PASS/FAIL, cell/point/face counts, patches, volume/area ranges,
  non-orthogonality, skewness, aspect ratio, structured index range, dimension

exit:
  0  mesh passes
  1  mesh error
  2  usage error

example:
  saturnCFD -checkMesh meshes/channel.msh -report results/mesh.json
```

### Example: `-help dumpConfig`

```text
saturnCFD -dumpConfig <solver> [-mesh <mesh.msh>] [-writeTemplate <file.yaml>]

Print the YAML template generated from the solver's registered options.
With -mesh, the boundary section is populated with the actual patch names.
With -writeTemplate, the same content is written to a file.

example:
  saturnCFD -dumpConfig burgers
  saturnCFD -dumpConfig burgers -writeTemplate burgersTemplate.yaml
  saturnCFD -dumpConfig burgers -mesh meshes/channel.msh -writeTemplate burgers.yaml
```

### Example: `-help units`

```text
saturnCFD units

v1.0 accepts SI only.

accepted:
  units:
    system: SI
  # or no units block at all

rejected:
  units:
    system: CGS
  units:
    length: mm

Non-SI input is a validation error. There is no conversion in v1.0.
```

`saturnCFD -help <topic>` prints a detailed page. Recognised topics:

| topic | contents |
|---|---|
| `checkMesh` | mesh format, structured detection, report fields, error categories, VTK export |
| `report` | JSON schema and exit codes |
| `writeVtk` | `.vtu`/`.vtp`/`.pvd` layout and ParaView instructions |
| `checkConfig` | validation stages, units rule, BC/initial checks |
| `dumpConfig` | template generation, solver registration, patch names |
| `writeTemplate` | file output, overwrite rule, example |
| `mesh` | Gmsh version, hexahedral requirement, 2D rule, `empty` patches |
| `units` | SI-only rule, rejected forms, future extension |
| `config` | YAML groups, units, BCs, initial conditions |
| `solvers` | available solvers and their state variables |

Each detailed page states the argument, the preconditions, the files read and
written, the exit codes and a complete example. Unknown topics print the list of
topics and exit 2.

## Errors and exit codes

| code | meaning |
|---|---|
| 0 | success / PASS |
| 1 | mesh or case error, or runtime failure |
| 2 | usage error: unknown option/topic, missing argument, invalid combination |

An unknown option prints:

```text
error: unknown option '-checkmesh'
hint: did you mean '-checkMesh'? see 'saturnCFD -help'
```

Invalid combinations:

- `-report` without `-checkMesh` or `-checkConfig`;
- `-writeVtk` without `-checkMesh`;
- `-writeTemplate` without `-dumpConfig`;
- `-mesh` without `-dumpConfig`.

## Tests

- every option has a parser test;
- every help topic exits zero and contains its usage line;
- unknown option/topic suggests the closest match;
- invalid combinations return 2;
- `-checkMesh` on good/bad meshes returns 0/1;
- `-writeVtk` writes a `.pvd` that references the volume and all patches;
- `-dumpConfig -writeTemplate` writes a file that passes `-checkConfig`.
