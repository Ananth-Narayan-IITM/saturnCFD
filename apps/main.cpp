#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "config/CommonSchema.hpp"
#include "config/Config.hpp"
#include "config/ConfigSchema.hpp"
#include "core/Error.hpp"
#include "core/Units.hpp"
#include "mesh/CheckMesh.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/VtuWriter.hpp"

namespace {

using namespace saturn;

struct Cli {
  std::string casePath;
  std::string checkMeshFile;
  std::string reportFile;
  std::string writeVTKPrefix;
  std::string checkConfigFile;
  std::string dumpSolver;
  std::string writeTemplateFile;
  std::string meshFile;
  std::string completionShell;
  int threads = 0;
  bool help = false;
  bool version = false;
  bool listSolvers = false;
  std::string helpTopic;
};

void writeTextFile(const std::string& path, const std::string& text) {
  const std::filesystem::path p(path);
  if (p.has_parent_path())
    std::filesystem::create_directories(p.parent_path());
  std::ofstream os(path);
  if (!os)
    throw SaturnError("cannot write '" + path + "'");
  os << text;
}

std::string defaultBoundarySkeleton(const Mesh& mesh) {
  std::ostringstream os;
  os << "\n# mesh patches: ";
  for (std::size_t i = 0; i < mesh.patches.size(); ++i) {
    if (i)
      os << ", ";
    os << mesh.patches[i].name;
  }
  os << "\n# a solver schema adds the boundary block and its BC types.\n";
  return os.str();
}

void printGeneralHelp() {
  std::cout
      << "saturnCFD 1.0 — structured finite-volume solver\n\n"
         "usage:\n"
         "  saturnCFD <case.yaml>                 run a case\n"
         "  saturnCFD -checkMesh <mesh.msh>       validate a Gmsh mesh\n"
         "  saturnCFD -checkConfig <case.yaml>    validate a case\n"
         "  saturnCFD -dumpConfig <solver|common> print a YAML template\n"
         "  saturnCFD -writeTemplate <file>       write the template\n"
         "  saturnCFD -listSolvers                list registered solvers\n"
         "  saturnCFD -help [topic]               help\n"
         "  saturnCFD -version                    version\n\n"
         "options:\n"
         "  -checkMesh <file>    mesh check; writes .vtu + .json by default\n"
         "  -report <path>       JSON report path; .json is appended if "
         "absent\n"
         "  -writeVTK <prefix>   also write per-patch .vtp and a .pvd\n"
         "  -writeTemplate <f>   write the template; .yaml is appended if "
         "absent\n"
         "  -mesh <file>         mesh patch names for -dumpConfig\n"
         "  -threads <N>         reserved for Kokkos thread count\n"
         "  -completion <shell>  print shell completion (bash|zsh|fish)\n\n"
         "notes:\n"
         "  - units are SI only in v1.0; non-SI input is rejected\n"
         "  - case templates are generated from the solver code\n"
         "  - by default -checkMesh writes only .vtu and .json\n"
         "  - -writeVTK adds .vtp patches and .pvd; open the .pvd in "
         "ParaView\n\n"
         "examples:\n"
         "  saturnCFD -checkMesh meshes/box2x2x2.msh\n"
         "  saturnCFD -checkMesh meshes/box2x2x2.msh -report results/mesh\n"
         "  saturnCFD -checkMesh meshes/box2x2x2.msh -writeVTK results/mesh\n"
         "  saturnCFD -dumpConfig common -writeTemplate common.yaml\n"
         "  saturnCFD -checkConfig common.yaml\n"
         "  saturnCFD -completion bash\n"
         "  saturnCFD -help checkMesh\n"
         "  saturnCFD -help mpi\n"
         "  saturnCFD -help build\n"
         "  saturnCFD -help gpu\n"
         "  saturnCFD -help clean\n"
         "  saturnCFD -help test\n";
}

void printCompletion(const std::string& shell) {
  if (shell == "bash") {
    std::cout << R"BASH(# saturnCFD bash completion
# install: source <(saturnCFD -completion bash)
_saturnCFD_paths() {
  local cur="$1"
  local IFS=$'\n'
  COMPREPLY=( $(compgen -f -- "${cur}") )
  local i
  for i in "${!COMPREPLY[@]}"; do
    if [[ -d "${COMPREPLY[$i]}" ]]; then
      COMPREPLY[$i]="${COMPREPLY[$i]}/"
    fi
  done
  compopt -o filenames 2>/dev/null
}

_saturnCFD_complete() {
  local cur prev
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  local opts="-help -version -listSolvers -checkMesh -report -writeVTK -checkConfig -dumpConfig -writeTemplate -mesh -threads -completion"

  # A new option: complete the option names themselves.
  if [[ "${cur}" == -* ]]; then
    COMPREPLY=( $(compgen -W "${opts}" -- "${cur}") )
    return 0
  fi

  # Complete the value of the option immediately before the cursor.
  case "${prev}" in
    -help)
      COMPREPLY=( $(compgen -W "checkMesh report writeVTK checkConfig dumpConfig writeTemplate mesh units config solvers completion mpi build gpu clean test" -- "${cur}") )
      ;;
    -completion)
      COMPREPLY=( $(compgen -W "bash zsh fish" -- "${cur}") )
      ;;
    -dumpConfig)
      COMPREPLY=( $(compgen -W "common" -- "${cur}") )
      ;;
    -checkMesh|-report|-writeVTK|-checkConfig|-writeTemplate|-mesh)
      _saturnCFD_paths "${cur}"
      ;;
    *)
      _saturnCFD_paths "${cur}"
      ;;
  esac
}
complete -o filenames -F _saturnCFD_complete saturnCFD
)BASH";
  } else if (shell == "zsh") {
    std::cout << R"ZSH(#compdef saturnCFD
# install: source <(saturnCFD -completion zsh)
_saturnCFD() {
  local -a opts
  opts=(-help -version -listSolvers -checkMesh -report -writeVTK -checkConfig -dumpConfig -writeTemplate -mesh -threads -completion)
  _arguments \
    '-help[show help]::topic:(checkMesh report writeVTK checkConfig dumpConfig writeTemplate mesh units config solvers completion mpi build gpu clean test)' \
    '-version[show version]' \
    '-listSolvers[list solvers]' \
    '-checkMesh[validate a Gmsh mesh]:mesh:_files' \
    '-report[write JSON report]:path:_files' \
    '-writeVTK[write patches and PVD]:prefix:_files' \
    '-checkConfig[validate a case]:case:_files' \
    '-dumpConfig[print a YAML template]:solver:(common burgers heat stokes incompressible)' \
    '-writeTemplate[write a template]:file:_files' \
    '-mesh[mesh for patch names]:mesh:_files' \
    '-threads[Kokkos thread count]:threads:' \
    '-completion[print shell completion]:shell:(bash zsh fish)'
}
_saturnCFD "$@"
)ZSH";
  } else if (shell == "fish") {
    std::cout << R"FISH(# saturnCFD fish completion
# install: saturnCFD -completion fish | source
complete -c saturnCFD -l help -r -a 'checkMesh report writeVTK checkConfig dumpConfig writeTemplate mesh units config solvers completion mpi build gpu clean test' -d 'Show help'
complete -c saturnCFD -l version -d 'Show version'
complete -c saturnCFD -l listSolvers -d 'List registered solvers'
complete -c saturnCFD -l checkMesh -r -F -d 'Validate a Gmsh mesh'
complete -c saturnCFD -l report -r -F -d 'Write JSON report'
complete -c saturnCFD -l writeVTK -r -F -d 'Write patches and PVD'
complete -c saturnCFD -l checkConfig -r -F -d 'Validate a case'
complete -c saturnCFD -l dumpConfig -r -a 'common burgers heat stokes incompressible' -d 'Print a YAML template'
complete -c saturnCFD -l writeTemplate -r -F -d 'Write a template'
complete -c saturnCFD -l mesh -r -F -d 'Mesh for patch names'
complete -c saturnCFD -l threads -r -d 'Kokkos thread count'
complete -c saturnCFD -l completion -r -a 'bash zsh fish' -d 'Print shell completion'
)FISH";
  } else {
    throw UsageError("unknown completion shell '" + shell +
                     "'; expected bash, zsh or fish");
  }
}

void printTopic(const std::string& topic) {
  if (topic == "checkMesh") {
    std::cout
        << "saturnCFD -checkMesh <mesh.msh>\n\n"
           "Validate a Gmsh 2.2 ASCII mesh. Only structured hexahedral meshes\n"
           "are accepted in v1.0. Tetrahedra, prisms, pyramids and\n"
           "non-contiguous hex meshes are rejected with a report.\n\n"
           "output:\n"
           "  by default <prefix>.vtu and <prefix>.json are written,\n"
           "  where <prefix> defaults to results/<mesh-stem>.\n\n"
           "optional:\n"
           "  -report <path>        JSON report path; .json is appended\n"
           "  -writeVTK <prefix>    also write .vtp patches and .pvd\n\n"
           "exit codes: 0 pass, 1 mesh error, 2 usage error\n\n"
           "example:\n"
           "  saturnCFD -checkMesh meshes/box2x2x2.msh -report results/mesh\n";
  } else if (topic == "report") {
    std::cout
        << "saturnCFD -report <path>\n\n"
           "Write the machine-readable report. The .json extension is added\n"
           "if absent: -report results/mesh writes results/mesh.json.\n"
           "Valid with -checkMesh and -checkConfig. The JSON contains counts,\n"
           "patches, volume and area ranges, quality metrics, structured "
           "flag,\n"
           "dimension and status.\n";
  } else if (topic == "writeVTK") {
    std::cout << "saturnCFD -writeVTK <prefix>\n\n"
                 "-checkMesh always writes <prefix>.vtu and <prefix>.json.\n"
                 "-writeVTK additionally writes one .vtp per boundary patch\n"
                 "and a .pvd collection. Open <prefix>.pvd in ParaView.\n\n"
                 "example:\n"
                 "  saturnCFD -checkMesh meshes/box2x2x2.msh -writeVTK "
                 "results/mesh\n";
  } else if (topic == "checkConfig") {
    std::cout
        << "saturnCFD -checkConfig <case.yaml>\n\n"
           "Parse and validate a case without running it. yaml-cpp reports\n"
           "syntax errors with line/column; the schema rejects unknown keys,\n"
           "wrong types and bad ranges; units are SI only in v1.0.\n\n"
           "If mesh.file is present, the mesh is read and checked.\n";
  } else if (topic == "dumpConfig") {
    std::cout
        << "saturnCFD -dumpConfig <solver|common>\n\n"
           "Print the YAML template generated from the registered options.\n"
           "In Phase 1 only 'common' is available; solver schemas arrive with\n"
           "the solvers. Use -writeTemplate to write the same content.\n\n"
           "optional:\n"
           "  -mesh <file>          append the mesh patch names\n"
           "  -writeTemplate <f>    write to a file\n";
  } else if (topic == "writeTemplate") {
    std::cout
        << "saturnCFD -writeTemplate <file>\n\n"
           "Write the template produced by -dumpConfig to a file. Valid only\n"
           "with -dumpConfig. If the argument has no extension, .yaml is\n"
           "appended. Parent directories are created.\n";
  } else if (topic == "mesh") {
    std::cout
        << "mesh\n\n"
           "v1.0 accepts Gmsh 2.2 ASCII, structured hexahedral meshes only.\n"
           "2D is a one-cell-thick 3D mesh with empty patches.\n"
           "Use: gmsh -format msh2 mesh.geo\n"
           "checkMesh reports structuredness, dimension, patches and "
           "quality.\n";
  } else if (topic == "units") {
    std::cout << "units\n\n"
                 "v1.0 accepts SI only.\n\n"
                 "accepted:\n"
                 "  units:\n"
                 "    system: SI\n"
                 "  # or no units block at all\n\n"
                 "rejected:\n"
                 "  units:\n"
                 "    system: CGS\n"
                 "  units:\n"
                 "    length: mm\n\n"
                 "Non-SI input is a validation error. There is no conversion "
                 "in v1.0.\n";
  } else if (topic == "config") {
    std::cout
        << "config\n\n"
           "YAML groups: solver, units, mesh, time, fields, boundary,\n"
           "numerics, linearSolver, output. yaml-cpp parses; the schema\n"
           "validates. Unknown keys are errors. -dumpConfig generates the\n"
           "template from the same registration that reads the case.\n";
  } else if (topic == "solvers") {
    std::cout
        << "solvers\n\n"
           "Phase 1 registers no solver yet. Phase 2 adds burgers, then heat,\n"
           "stokes and incompressible. A user solver is a copy under\n"
           "src/solvers/<name> plus one addSolver(<name>) line.\n";
  } else if (topic == "completion") {
    std::cout << "completion\n\n"
                 "Print a shell completion script.\n\n"
                 "  source <(saturnCFD -completion bash)   # bash\n"
                 "  source <(saturnCFD -completion zsh)    # zsh\n"
                 "  saturnCFD -completion fish | source    # fish\n\n"
                 "Then type: saturnCFD -dump<Tab>\n";
  } else if (topic == "mpi") {
    std::cout
        << "mpi\n\n"
           "Run the MPI test suite:\n\n"
           "  ctest --preset mpi\n"
           "  mpirun -np 2 build/saturn_mpi_tests\n"
           "  mpirun -np 4 build/saturn_mpi_tests\n\n"
           "This PC has enough cores for the current 1/2/4-rank tests; no\n"
           "--oversubscribe is needed. On a machine with fewer slots than\n"
           "requested ranks, configure with:\n"
           "  cmake -S . -B build -DSATURN_MPI_PREFLAGS=--oversubscribe\n\n"
           "Solver runs (Phase 2+):\n\n"
           "  mpirun -np 4 saturnCFD case.yaml\n"
           "  mpirun -np 1 saturnCFD case.yaml     # one GPU, one rank\n\n"
           "Current saturnCFD is a serial mesh/config tool; do not run it\n"
           "under mpirun yet. The MPI tests are the partitioned path today.\n"
           "One rank per GPU for performance. On the cluster use srun or\n"
           "mpirun with the allocated tasks.\n";
  } else if (topic == "build") {
    std::cout
        << "build\n\n"
           "Current serial/MPI build:\n\n"
           "  cmake --preset default\n"
           "  cmake --build --preset default -j\n"
           "  cmake --install build --prefix \"$HOME/.local\"\n\n"
           "CMake configure and build are separate steps. The first command\n"
           "writes build/; the second compiles it.\n\n"
           "  CMAKE_BUILD_TYPE=Release  optimised, no debug symbols\n"
           "  CMAKE_BUILD_TYPE=Debug    unoptimised, debug symbols\n"
           "  CMAKE_BUILD_TYPE=RelWithDebInfo  optimised with symbols\n\n"
           "SATURN_ENABLE_MPI=ON (default) links MPI, defines SATURN_USE_MPI\n"
           "and builds the MPI tests. It does not yet partition the solver.\n"
           "  cmake -S . -B build -DSATURN_ENABLE_MPI=OFF   # serial only\n\n"
           "Planned separate CPU/GPU builds (with the Kokkos FVM layer):\n\n"
           "  cmake --preset cpu        # build-cpu, OpenMP\n"
           "  cmake --build --preset cpu -j\n"
           "  cmake --preset gpu        # build-gpu, CUDA\n"
           "  cmake --build --preset gpu -j\n\n"
           "They are separate build directories because the compiler and\n"
           "backend are fixed at configure time.\n\n"
           "Clean:\n"
           "  cmake --build build --target clean   # remove compiled files\n"
           "  rm -rf build                         # remove the build tree\n\n"
           "Compilation database:\n"
           "  build/compile_commands.json          # CMake/clangd standard\n"
           "  build/compileCommands.json           # camelCase copy\n";
  } else if (topic == "gpu") {
    std::cout
        << "gpu\n\n"
           "GPU execution is selected at build/runtime, not in case.yaml.\n"
           "The case file describes physics; it stays CPU/GPU independent.\n\n"
           "Planned build:\n\n"
           "  cmake --preset gpu        # SATURN_BACKEND=cuda\n"
           "  cmake --build --preset gpu -j\n\n"
           "Runtime, one rank per GPU:\n\n"
           "  mpirun -np 1 saturnCFD case.yaml    # one GPU\n"
           "  mpirun -np 4 saturnCFD case.yaml    # four GPUs, one rank "
           "each\n\n"
           "mpirun is not CPU-only: it launches MPI ranks. Each rank's\n"
           "Kokkos backend decides whether that rank uses a CPU or a GPU.\n"
           "Rank-to-device mapping is by local rank; a rank beyond the\n"
           "visible device count clamps to device 0 and warns.\n\n"
           "Do not benchmark MPI+GPU scaling on one GPU. One GPU is enough\n"
           "for correctness and single-GPU performance; multi-GPU scaling is\n"
           "a cluster measurement. Kokkos is not wired into this slice yet.\n";
  } else if (topic == "clean") {
    std::cout
        << "clean\n\n"
           "  cmake --build build --target clean   # objects and binaries\n"
           "  rm -rf build                         # whole build tree\n"
           "  rm -rf build-cpu build-gpu           # both backend trees\n\n"
           "There is no wclean; the CMake clean target is the equivalent for\n"
           "one build directory. Deleting the build directory is the full\n"
           "clean and forces a fresh configure.\n";
  } else if (topic == "test") {
    std::cout << "test\n\n"
                 "  ctest --preset default       all tests\n"
                 "  ctest --preset regression    regression only\n"
                 "  ctest --preset unit\n"
                 "  ctest --preset mesh\n"
                 "  ctest --preset config\n"
                 "  ctest --preset cli\n"
                 "  ctest --preset mpi           MPI halo/partition tests\n"
                 "  ctest --list-presets         list them\n";
  } else {
    throw UsageError("unknown help topic '" + topic +
                     "'; topics: checkMesh, report, writeVTK, checkConfig, "
                     "dumpConfig, writeTemplate, mesh, units, config, solvers, "
                     "completion, mpi, build, gpu, clean, test");
  }
}

std::string closestOption(const std::string& bad) {
  const std::vector<std::string> known{
      "-help",          "-version",  "-listSolvers", "-checkMesh",
      "-report",        "-writeVTK", "-checkConfig", "-dumpConfig",
      "-writeTemplate", "-mesh",     "-threads",     "-completion"};
  auto distance = [](const std::string& a, const std::string& b) {
    std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j)
      prev[j] = static_cast<int>(j);
    for (std::size_t i = 1; i <= a.size(); ++i) {
      cur[0] = static_cast<int>(i);
      for (std::size_t j = 1; j <= b.size(); ++j) {
        const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
        cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
      }
      std::swap(prev, cur);
    }
    return prev[b.size()];
  };
  std::string best;
  int bestScore = 1000;
  for (const auto& k : known) {
    const int d = distance(bad, k);
    if (d < bestScore) {
      bestScore = d;
      best = k;
    }
  }
  return bestScore <= 3 ? best : "";
}

std::string requireValue(int& i, int argc, char** argv,
                         const std::string& option) {
  if (i + 1 >= argc)
    throw UsageError("option '" + option + "' needs a value");
  return argv[++i];
}

Cli parseCli(int argc, char** argv) {
  Cli cli;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-help" || a == "-h") {
      cli.help = true;
      if (i + 1 < argc && argv[i + 1][0] != '-')
        cli.helpTopic = argv[++i];
    } else if (a == "-version") {
      cli.version = true;
    } else if (a == "-listSolvers") {
      cli.listSolvers = true;
    } else if (a == "-checkMesh") {
      cli.checkMeshFile = requireValue(i, argc, argv, a);
    } else if (a == "-report") {
      cli.reportFile = requireValue(i, argc, argv, a);
    } else if (a == "-writeVTK") {
      cli.writeVTKPrefix = requireValue(i, argc, argv, a);
    } else if (a == "-checkConfig") {
      cli.checkConfigFile = requireValue(i, argc, argv, a);
    } else if (a == "-dumpConfig") {
      cli.dumpSolver = requireValue(i, argc, argv, a);
    } else if (a == "-writeTemplate") {
      cli.writeTemplateFile = requireValue(i, argc, argv, a);
    } else if (a == "-mesh") {
      cli.meshFile = requireValue(i, argc, argv, a);
    } else if (a == "-threads") {
      const std::string v = requireValue(i, argc, argv, a);
      cli.threads = std::atoi(v.c_str());
      if (cli.threads <= 0)
        throw UsageError("-threads needs a positive integer");
    } else if (a == "-completion") {
      cli.completionShell = requireValue(i, argc, argv, a);
    } else if (!a.empty() && a[0] == '-') {
      const std::string hint = closestOption(a);
      std::string msg = "unknown option '" + a + "'";
      if (!hint.empty())
        msg += "; did you mean '" + hint + "'?";
      msg += " see 'saturnCFD -help'";
      throw UsageError(msg);
    } else {
      if (!cli.casePath.empty())
        throw UsageError("more than one case file given");
      cli.casePath = a;
    }
  }
  return cli;
}

std::string ensureExtension(const std::string& path,
                            const std::string& extension) {
  if (std::filesystem::path(path).has_extension())
    return path;
  return path + extension;
}

std::string stripJsonExtension(const std::string& path) {
  if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".json") == 0)
    return path.substr(0, path.size() - 5);
  return path;
}

std::string meshOutputBase(const Cli& cli) {
  if (!cli.writeVTKPrefix.empty())
    return cli.writeVTKPrefix;
  if (!cli.reportFile.empty())
    return stripJsonExtension(cli.reportFile);
  const std::filesystem::path meshPath(cli.checkMeshFile);
  return "results/" + meshPath.stem().string();
}

void validateCombinations(const Cli& cli) {
  if (cli.help || cli.version || cli.listSolvers ||
      !cli.completionShell.empty())
    return;
  const int actions = static_cast<int>(!cli.checkMeshFile.empty()) +
                      static_cast<int>(!cli.checkConfigFile.empty()) +
                      static_cast<int>(!cli.dumpSolver.empty()) +
                      static_cast<int>(!cli.casePath.empty());
  if (actions == 0)
    throw UsageError("no action; see 'saturnCFD -help'");
  if (actions > 1)
    throw UsageError(
        "choose one action: run, -checkMesh, -checkConfig or "
        "-dumpConfig");
  if (!cli.reportFile.empty() && cli.checkMeshFile.empty() &&
      cli.checkConfigFile.empty())
    throw UsageError("-report requires -checkMesh or -checkConfig");
  if (!cli.writeVTKPrefix.empty() && cli.checkMeshFile.empty())
    throw UsageError("-writeVTK requires -checkMesh");
  if (!cli.writeTemplateFile.empty() && cli.dumpSolver.empty())
    throw UsageError("-writeTemplate requires -dumpConfig");
  if (!cli.meshFile.empty() && cli.dumpSolver.empty())
    throw UsageError("-mesh requires -dumpConfig");
}

int runCheckMesh(const Cli& cli) {
  Mesh mesh = readGmsh(cli.checkMeshFile);
  const MeshReport report = checkMesh(mesh, cli.checkMeshFile);
  std::cout << report.toText();

  const std::string base = meshOutputBase(cli);
  const std::string jsonPath = !cli.reportFile.empty()
                                   ? ensureExtension(cli.reportFile, ".json")
                                   : base + ".json";
  writeTextFile(jsonPath, report.toJson());
  writeMeshVtk(mesh, base, !cli.writeVTKPrefix.empty());
  return report.pass() ? 0 : 1;
}

int runCheckConfig(const Cli& cli) {
  const Config cfg = Config::fromFile(cli.checkConfigFile);
  const ConfigSchema schema = commonSchema();
  schema.validate(cfg);

  std::string meshSummary = "none";
  const std::string meshFile = cfg.getString("mesh.file", "");
  if (!meshFile.empty()) {
    Mesh mesh = readGmsh(meshFile);
    const MeshReport report = checkMesh(mesh, meshFile);
    if (!report.pass()) {
      std::cout << report.toText();
      throw ConfigError("mesh validation failed for '" + meshFile + "'");
    }
    meshSummary = meshFile + " (PASS)";
  }

  std::cout << "config PASS: " << cli.checkConfigFile << "\n";
  std::cout << "  solver: " << cfg.getString("solver", "") << "\n";
  std::cout << "  units : " << unitSystemName(cfg.unitSystem()) << "\n";
  std::cout << "  mesh  : " << meshSummary << "\n";

  if (!cli.reportFile.empty()) {
    std::ostringstream os;
    os << "{\n  \"case\": \"" << cli.checkConfigFile << "\",\n"
       << "  \"status\": \"PASS\",\n"
       << "  \"solver\": \"" << cfg.getString("solver", "") << "\",\n"
       << "  \"units\": \"" << unitSystemName(cfg.unitSystem()) << "\"\n}\n";
    writeTextFile(ensureExtension(cli.reportFile, ".json"), os.str());
  }
  return 0;
}

int runDumpConfig(const Cli& cli) {
  if (cli.dumpSolver != "common")
    throw UsageError(
        "solver '" + cli.dumpSolver +
        "' is not registered in Phase 1; use '-dumpConfig common'");

  const ConfigSchema schema = commonSchema();
  std::ostringstream os;
  schema.dump(os, cli.dumpSolver);
  if (!cli.meshFile.empty()) {
    const Mesh mesh = readGmsh(cli.meshFile);
    os << defaultBoundarySkeleton(mesh);
  }
  const std::string text = os.str();
  if (!cli.writeTemplateFile.empty())
    writeTextFile(ensureExtension(cli.writeTemplateFile, ".yaml"), text);
  else
    std::cout << text;
  return 0;
}

int runCase(const Cli& cli) {
  const Config cfg = Config::fromFile(cli.casePath);
  const ConfigSchema schema = commonSchema();
  schema.validate(cfg);
  throw SaturnError(
      "case '" + cli.casePath +
      "' is valid, but solver execution is not implemented in Phase 1");
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Cli cli = parseCli(argc, argv);
    if (cli.help) {
      if (cli.helpTopic.empty())
        printGeneralHelp();
      else
        printTopic(cli.helpTopic);
      return 0;
    }
    if (cli.version) {
      std::cout << "saturnCFD 1.0.0 (Phase 1: mesh and configuration)\n";
      return 0;
    }
    if (cli.listSolvers) {
      std::cout << "no solvers registered yet (Phase 1)\n";
      return 0;
    }
    if (!cli.completionShell.empty()) {
      printCompletion(cli.completionShell);
      return 0;
    }

    validateCombinations(cli);
    if (!cli.checkMeshFile.empty())
      return runCheckMesh(cli);
    if (!cli.checkConfigFile.empty())
      return runCheckConfig(cli);
    if (!cli.dumpSolver.empty())
      return runDumpConfig(cli);
    return runCase(cli);
  } catch (const SaturnError& e) {
    std::cerr << "error: " << e.what() << "\n";
    return e.exitCode();
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
