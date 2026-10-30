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
| `-checkMesh` | `mesh.msh` | validate a mesh; writes `.vtu` and `.json` by default |
| `-report` | `path` | JSON report path; `.json` is appended if absent |
| `-writeVTK` | `prefix` | also write per-patch `.vtp` and `.pvd`; requires `-checkMesh` |
| `-checkConfig` | `case.yaml` | validate the case and exit |
| `-dumpConfig` | `solver\|common` | print the generated YAML template |
| `-writeTemplate` | `file` | write the template; `.yaml` is appended if absent |
| `-mesh` | `mesh.msh` | give `-dumpConfig` the actual patch names |
| `-threads` | `N` | override the Kokkos thread count |
| `-completion` | `bash\|zsh\|fish` | print a shell completion script |

## Examples

```bash
# run a case
saturnCFD examples/burgers/traveling.yaml

# validate a mesh; writes results/channel.vtu and results/channel.json
saturnCFD -checkMesh meshes/channel.msh

# choose the JSON/VTU prefix; .json is appended
saturnCFD -checkMesh meshes/channel.msh -report results/channel

# add boundary patches and a PVD collection
saturnCFD -checkMesh meshes/channel.msh -writeVTK results/channel

# validate a case without running it
saturnCFD -checkConfig examples/burgers/traveling.yaml

# print or write the common template (solver templates arrive in Phase 2)
saturnCFD -dumpConfig common
saturnCFD -dumpConfig common -writeTemplate common

# help
saturnCFD -help
saturnCFD -help checkMesh
saturnCFD -help dumpConfig
saturnCFD -help units
saturnCFD -help config
saturnCFD -help mesh
saturnCFD -help completion
saturnCFD -help mpi
saturnCFD -help test

# shell completion
source <(saturnCFD -completion bash)
```

## Template generation

`-dumpConfig <solver|common>` prints a commented YAML template generated from
the registered options. `-writeTemplate <file>` writes the same content to a
file; if the argument has no extension, `.yaml` is appended. There is no
hand-maintained template file. Phase 1 registers `common`; solver templates
arrive with the solvers.

```bash
saturnCFD -dumpConfig common -writeTemplate common
# writes common.yaml
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

## Mesh output

By default, `-checkMesh` writes:

| file | contents |
|---|---|
| `<prefix>.vtu` | volume mesh; cell data: `cellId`, `volume`, `nonOrthogonality`, `skewness`, `aspectRatio` |
| `<prefix>.json` | full statistics: counts, patches, volume/area ranges, quality, structured flag, dimension, errors, warnings, status |

The default prefix is `results/<mesh-stem>`. `-report results/mesh` sets the
prefix to `results/mesh` and writes `results/mesh.json`; the `.json` extension
is optional in the argument.

`-writeVTK <prefix>` additionally writes:

| file | contents |
|---|---|
| `<prefix>_<patch>.vtp` | one boundary surface per patch |
| `<prefix>.pvd` | ParaView collection referencing the volume and all patches |

Open the `.pvd` in ParaView. The `.vtu` is the volume; each `.vtp` is a named
boundary patch.

## Completion

`-completion <shell>` prints a completion script. It is generated from the same
option list as the parser, so `-dump<Tab>` completes `-dumpConfig`. Path-valued
options (`-checkMesh`, `-report`, `-writeVTK`, `-checkConfig`,
`-writeTemplate`, `-mesh`) complete files and directories; directories get a
trailing slash, so `tests<Tab>` becomes `tests/` and the next `<Tab>` lists its
contents.

```bash
source <(saturnCFD -completion bash)
source <(saturnCFD -completion zsh)
saturnCFD -completion fish | source
```

Completion requires `saturnCFD` to be on `PATH`; install it with
`cmake --install build --prefix "$HOME/.local"`.

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
  saturnCFD -dumpConfig <solver|common> print a YAML template
  saturnCFD -writeTemplate <file>       write the template (with -dumpConfig)
  saturnCFD -listSolvers                list solvers
  saturnCFD -help [topic]               help
  saturnCFD -completion <shell>         print shell completion
  saturnCFD -version                    version and build

options:
  -checkMesh <file>    mesh check; writes .vtu + .json by default
  -report <path>       JSON report path; .json is appended if absent
  -writeVTK <prefix>   also write .vtp patches and .pvd
  -mesh <file>         patch names for -dumpConfig
  -threads <N>         Kokkos thread count
  -completion <shell>  bash, zsh or fish

notes:
  - units are SI only in v1.0; non-SI input is rejected
  - case templates are generated from the solver code
  - by default -checkMesh writes only .vtu and .json
  - -writeVTK adds .vtp patches and .pvd; open the .pvd in ParaView

examples:
  saturnCFD examples/burgers/traveling.yaml
  saturnCFD -checkMesh meshes/channel.msh -report results/channel
  saturnCFD -checkMesh meshes/channel.msh -writeVTK results/channel
  saturnCFD -dumpConfig common -writeTemplate common
  source <(saturnCFD -completion bash)
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
  -report <path>        JSON report path; .json is appended if absent
  -writeVTK <prefix>    also write .vtp patches and .pvd

output:
  by default <prefix>.vtu and <prefix>.json, prefix defaults to
  results/<mesh-stem>; PASS/FAIL, counts, patches, volume/area ranges,
  non-orthogonality, skewness, aspect ratio, structured index range, dimension

exit:
  0  mesh passes
  1  mesh error
  2  usage error

example:
  saturnCFD -checkMesh meshes/channel.msh -report results/channel
```

### Example: `-help dumpConfig`

```text
saturnCFD -dumpConfig <solver|common> [-mesh <mesh.msh>] [-writeTemplate <file>]

Print the YAML template generated from the registered options. In Phase 1
only 'common' is available; solver templates arrive with the solvers.
With -mesh, the actual patch names are appended as comments.
With -writeTemplate, the same content is written to a file.

example:
  saturnCFD -dumpConfig common
  saturnCFD -dumpConfig common -writeTemplate common
  saturnCFD -dumpConfig common -mesh meshes/channel.msh -writeTemplate channel
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
| `writeVTK` | default `.vtu`/`.json`, optional `.vtp`/`.pvd`, ParaView instructions |
| `checkConfig` | validation stages, units rule, BC/initial checks |
| `dumpConfig` | template generation, solver registration, patch names |
| `writeTemplate` | file output, overwrite rule, example |
| `mesh` | Gmsh version, hexahedral requirement, 2D rule, `empty` patches |
| `units` | SI-only rule, rejected forms, future extension |
| `config` | YAML groups, units, BCs, initial conditions |
| `solvers` | available solvers and their state variables |
| `completion` | bash, zsh and fish install lines |
| `mpi` | MPI test/run commands, one-rank-per-GPU policy |
| `test` | ctest presets and labels |

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
- `-writeVTK` without `-checkMesh`;
- `-writeTemplate` without `-dumpConfig`;
- `-mesh` without `-dumpConfig`.

## Tests

- every option has a parser test;
- every help topic exits zero and contains its usage line;
- unknown option/topic suggests the closest match;
- invalid combinations return 2;
- `-checkMesh` on good/bad meshes returns 0/1;
- `-writeVTK` writes a `.pvd` that references the volume and all patches;
- `-dumpConfig -writeTemplate results/file` writes `results/file.yaml`, which
  passes `-checkConfig`.
