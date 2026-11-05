/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "saturnCFD", "index.html", [
    [ "Where to start", "index.html#autotoc_md242", null ],
    [ "What v1 is", "index.html#autotoc_md243", null ],
    [ "Building the docs", "index.html#autotoc_md244", null ],
    [ "saturnCFD v1", "md_README.html", [
      [ "Host requirements", "md_README.html#autotoc_md1", null ],
      [ "Build and test", "md_README.html#autotoc_md2", null ],
      [ "CPU threads and GPUs", "md_README.html#autotoc_md3", null ],
      [ "Decisions", "md_README.html#autotoc_md4", null ],
      [ "Dependencies", "md_README.html#autotoc_md5", null ],
      [ "Documentation", "md_README.html#autotoc_md6", null ],
      [ "Equations and validation", "md_README.html#autotoc_md7", null ],
      [ "Command line", "md_README.html#autotoc_md8", null ],
      [ "Planned layout", "md_README.html#autotoc_md9", null ],
      [ "Writing a solver", "md_README.html#autotoc_md10", null ],
      [ "Inspecting v0", "md_README.html#autotoc_md11", null ],
      [ "License", "md_README.html#autotoc_md12", null ]
    ] ],
    [ "NNNN — Title", "md_docs_2decisions_20000-template.html", [
      [ "Context", "md_docs_2decisions_20000-template.html#autotoc_md14", null ],
      [ "Decision", "md_docs_2decisions_20000-template.html#autotoc_md15", null ],
      [ "Alternatives considered", "md_docs_2decisions_20000-template.html#autotoc_md16", null ],
      [ "Consequences", "md_docs_2decisions_20000-template.html#autotoc_md17", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20000-template.html#autotoc_md18", null ],
      [ "References", "md_docs_2decisions_20000-template.html#autotoc_md19", null ]
    ] ],
    [ "0001 — Physics roadmap: scalar to vector to incompressible", "md_docs_2decisions_20001-physics-roadmap.html", [
      [ "Context", "md_docs_2decisions_20001-physics-roadmap.html#autotoc_md21", null ],
      [ "Decision", "md_docs_2decisions_20001-physics-roadmap.html#autotoc_md22", null ],
      [ "Alternatives considered", "md_docs_2decisions_20001-physics-roadmap.html#autotoc_md23", null ],
      [ "Consequences", "md_docs_2decisions_20001-physics-roadmap.html#autotoc_md24", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20001-physics-roadmap.html#autotoc_md25", null ],
      [ "References", "md_docs_2decisions_20001-physics-roadmap.html#autotoc_md26", null ]
    ] ],
    [ "0002 — Mesh strategy: structured hexahedral <span class=\"tt\">.msh</span>, no AMR in v1.0", "md_docs_2decisions_20002-mesh-strategy.html", [
      [ "Context", "md_docs_2decisions_20002-mesh-strategy.html#autotoc_md28", null ],
      [ "Decision", "md_docs_2decisions_20002-mesh-strategy.html#autotoc_md29", null ],
      [ "Alternatives considered", "md_docs_2decisions_20002-mesh-strategy.html#autotoc_md30", null ],
      [ "Consequences", "md_docs_2decisions_20002-mesh-strategy.html#autotoc_md31", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20002-mesh-strategy.html#autotoc_md32", null ],
      [ "References", "md_docs_2decisions_20002-mesh-strategy.html#autotoc_md33", null ]
    ] ],
    [ "0003 — Linear algebra: PETSc, not a home-grown Krylov solver", "md_docs_2decisions_20003-linear-algebra-petsc.html", [
      [ "Context", "md_docs_2decisions_20003-linear-algebra-petsc.html#autotoc_md35", null ],
      [ "Decision", "md_docs_2decisions_20003-linear-algebra-petsc.html#autotoc_md36", null ],
      [ "Alternatives considered", "md_docs_2decisions_20003-linear-algebra-petsc.html#autotoc_md37", null ],
      [ "Consequences", "md_docs_2decisions_20003-linear-algebra-petsc.html#autotoc_md38", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20003-linear-algebra-petsc.html#autotoc_md39", null ],
      [ "Implementation status", "md_docs_2decisions_20003-linear-algebra-petsc.html#autotoc_md40", null ],
      [ "References", "md_docs_2decisions_20003-linear-algebra-petsc.html#autotoc_md41", null ]
    ] ],
    [ "0004 — Collocated state and pressure–velocity coupling", "md_docs_2decisions_20004-collocated-coupling.html", [
      [ "Context", "md_docs_2decisions_20004-collocated-coupling.html#autotoc_md43", null ],
      [ "Decision", "md_docs_2decisions_20004-collocated-coupling.html#autotoc_md44", null ],
      [ "Alternatives considered", "md_docs_2decisions_20004-collocated-coupling.html#autotoc_md45", null ],
      [ "Consequences", "md_docs_2decisions_20004-collocated-coupling.html#autotoc_md46", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20004-collocated-coupling.html#autotoc_md47", null ],
      [ "References", "md_docs_2decisions_20004-collocated-coupling.html#autotoc_md48", null ]
    ] ],
    [ "0005 — Solver lifecycle and runtime selection", "md_docs_2decisions_20005-solver-lifecycle.html", [
      [ "Context", "md_docs_2decisions_20005-solver-lifecycle.html#autotoc_md50", null ],
      [ "Decision", "md_docs_2decisions_20005-solver-lifecycle.html#autotoc_md51", null ],
      [ "Alternatives considered", "md_docs_2decisions_20005-solver-lifecycle.html#autotoc_md52", null ],
      [ "Consequences", "md_docs_2decisions_20005-solver-lifecycle.html#autotoc_md53", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20005-solver-lifecycle.html#autotoc_md54", null ],
      [ "References", "md_docs_2decisions_20005-solver-lifecycle.html#autotoc_md55", null ]
    ] ],
    [ "0006 — Configuration: typed schema, generated templates, strict validation", "md_docs_2decisions_20006-configuration.html", [
      [ "Context", "md_docs_2decisions_20006-configuration.html#autotoc_md57", null ],
      [ "Decision", "md_docs_2decisions_20006-configuration.html#autotoc_md58", null ],
      [ "Alternatives considered", "md_docs_2decisions_20006-configuration.html#autotoc_md59", null ],
      [ "Consequences", "md_docs_2decisions_20006-configuration.html#autotoc_md60", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20006-configuration.html#autotoc_md61", null ],
      [ "References", "md_docs_2decisions_20006-configuration.html#autotoc_md62", null ]
    ] ],
    [ "0007 — Mesh validation and the <span class=\"tt\">checkMesh</span> module", "md_docs_2decisions_20007-mesh-validation.html", [
      [ "Context", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md64", null ],
      [ "Decision", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md65", [
        [ "Structured detection", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md66", null ],
        [ "Report and visualisation", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md67", null ],
        [ "2D detection", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md68", null ]
      ] ],
      [ "Alternatives considered", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md69", null ],
      [ "Consequences", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md70", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md71", null ],
      [ "References", "md_docs_2decisions_20007-mesh-validation.html#autotoc_md72", null ]
    ] ],
    [ "0008 — SI units only in v1.0; non-SI is a validation error", "md_docs_2decisions_20008-units.html", [
      [ "Context", "md_docs_2decisions_20008-units.html#autotoc_md74", null ],
      [ "Decision", "md_docs_2decisions_20008-units.html#autotoc_md75", null ],
      [ "YAML form", "md_docs_2decisions_20008-units.html#autotoc_md76", null ],
      [ "Alternatives considered", "md_docs_2decisions_20008-units.html#autotoc_md77", null ],
      [ "Consequences", "md_docs_2decisions_20008-units.html#autotoc_md78", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20008-units.html#autotoc_md79", null ],
      [ "References", "md_docs_2decisions_20008-units.html#autotoc_md80", null ]
    ] ],
    [ "0009 — Documentation, equations and citations", "md_docs_2decisions_20009-documentation.html", [
      [ "Context", "md_docs_2decisions_20009-documentation.html#autotoc_md82", null ],
      [ "Decision", "md_docs_2decisions_20009-documentation.html#autotoc_md83", null ],
      [ "Alternatives considered", "md_docs_2decisions_20009-documentation.html#autotoc_md84", null ],
      [ "Consequences", "md_docs_2decisions_20009-documentation.html#autotoc_md85", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20009-documentation.html#autotoc_md86", null ],
      [ "References", "md_docs_2decisions_20009-documentation.html#autotoc_md87", null ]
    ] ],
    [ "0010 — Command-line interface and help", "md_docs_2decisions_20010-cli.html", [
      [ "Context", "md_docs_2decisions_20010-cli.html#autotoc_md89", null ],
      [ "Decision", "md_docs_2decisions_20010-cli.html#autotoc_md90", null ],
      [ "Solver template", "md_docs_2decisions_20010-cli.html#autotoc_md91", null ],
      [ "Mesh visualisation", "md_docs_2decisions_20010-cli.html#autotoc_md92", null ],
      [ "Completion", "md_docs_2decisions_20010-cli.html#autotoc_md93", null ],
      [ "Help", "md_docs_2decisions_20010-cli.html#autotoc_md94", null ],
      [ "Alternatives considered", "md_docs_2decisions_20010-cli.html#autotoc_md95", null ],
      [ "Consequences", "md_docs_2decisions_20010-cli.html#autotoc_md96", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20010-cli.html#autotoc_md97", null ],
      [ "References", "md_docs_2decisions_20010-cli.html#autotoc_md98", null ]
    ] ],
    [ "0011 — User-editable solvers, OpenFOAM-style", "md_docs_2decisions_20011-user-solvers.html", [
      [ "Context", "md_docs_2decisions_20011-user-solvers.html#autotoc_md100", null ],
      [ "Decision", "md_docs_2decisions_20011-user-solvers.html#autotoc_md101", null ],
      [ "Solver directory layout", "md_docs_2decisions_20011-user-solvers.html#autotoc_md102", null ],
      [ "User workflow", "md_docs_2decisions_20011-user-solvers.html#autotoc_md103", null ],
      [ "Alternatives considered", "md_docs_2decisions_20011-user-solvers.html#autotoc_md104", null ],
      [ "Consequences", "md_docs_2decisions_20011-user-solvers.html#autotoc_md105", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20011-user-solvers.html#autotoc_md106", null ],
      [ "References", "md_docs_2decisions_20011-user-solvers.html#autotoc_md107", null ]
    ] ],
    [ "0012 — Test framework: doctest", "md_docs_2decisions_20012-test-framework.html", [
      [ "Context", "md_docs_2decisions_20012-test-framework.html#autotoc_md109", null ],
      [ "What doctest is", "md_docs_2decisions_20012-test-framework.html#autotoc_md110", null ],
      [ "Decision", "md_docs_2decisions_20012-test-framework.html#autotoc_md111", null ],
      [ "Alternatives considered", "md_docs_2decisions_20012-test-framework.html#autotoc_md112", null ],
      [ "Consequences", "md_docs_2decisions_20012-test-framework.html#autotoc_md113", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20012-test-framework.html#autotoc_md114", null ],
      [ "References", "md_docs_2decisions_20012-test-framework.html#autotoc_md115", null ]
    ] ],
    [ "0013 — Documentation pipeline: Doxygen HTML+PDF, Pages, clang-format", "md_docs_2decisions_20013-documentation-pipeline.html", [
      [ "Context", "md_docs_2decisions_20013-documentation-pipeline.html#autotoc_md117", null ],
      [ "Decision", "md_docs_2decisions_20013-documentation-pipeline.html#autotoc_md118", null ],
      [ "Alternatives considered", "md_docs_2decisions_20013-documentation-pipeline.html#autotoc_md119", null ],
      [ "Consequences", "md_docs_2decisions_20013-documentation-pipeline.html#autotoc_md120", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20013-documentation-pipeline.html#autotoc_md121", null ],
      [ "References", "md_docs_2decisions_20013-documentation-pipeline.html#autotoc_md122", null ]
    ] ],
    [ "0014 — MPI decomposition and GPU execution model", "md_docs_2decisions_20014-mpi-gpu.html", [
      [ "Context", "md_docs_2decisions_20014-mpi-gpu.html#autotoc_md124", null ],
      [ "Decision", "md_docs_2decisions_20014-mpi-gpu.html#autotoc_md125", null ],
      [ "Alternatives considered", "md_docs_2decisions_20014-mpi-gpu.html#autotoc_md126", null ],
      [ "Consequences", "md_docs_2decisions_20014-mpi-gpu.html#autotoc_md127", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20014-mpi-gpu.html#autotoc_md128", null ],
      [ "References", "md_docs_2decisions_20014-mpi-gpu.html#autotoc_md129", null ]
    ] ],
    [ "0015 — Parallel testing, benchmarking and profiling", "md_docs_2decisions_20015-parallel-testing.html", [
      [ "Context", "md_docs_2decisions_20015-parallel-testing.html#autotoc_md131", null ],
      [ "Decision", "md_docs_2decisions_20015-parallel-testing.html#autotoc_md132", null ],
      [ "Alternatives considered", "md_docs_2decisions_20015-parallel-testing.html#autotoc_md133", null ],
      [ "Consequences", "md_docs_2decisions_20015-parallel-testing.html#autotoc_md134", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20015-parallel-testing.html#autotoc_md135", null ],
      [ "References", "md_docs_2decisions_20015-parallel-testing.html#autotoc_md136", null ]
    ] ],
    [ "0016 — Kokkos-backed field and FVM layer", "md_docs_2decisions_20016-kokkos-field-layer.html", [
      [ "Context", "md_docs_2decisions_20016-kokkos-field-layer.html#autotoc_md138", null ],
      [ "Decision", "md_docs_2decisions_20016-kokkos-field-layer.html#autotoc_md139", null ],
      [ "Alternatives considered", "md_docs_2decisions_20016-kokkos-field-layer.html#autotoc_md140", null ],
      [ "Consequences", "md_docs_2decisions_20016-kokkos-field-layer.html#autotoc_md141", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20016-kokkos-field-layer.html#autotoc_md142", null ],
      [ "References", "md_docs_2decisions_20016-kokkos-field-layer.html#autotoc_md143", null ]
    ] ],
    [ "0017 — PETSc discovery and <span class=\"tt\">saturnLinAlg</span>", "md_docs_2decisions_20017-petsc-linalg.html", [
      [ "Context", "md_docs_2decisions_20017-petsc-linalg.html#autotoc_md145", null ],
      [ "Decision", "md_docs_2decisions_20017-petsc-linalg.html#autotoc_md146", null ],
      [ "Alternatives considered", "md_docs_2decisions_20017-petsc-linalg.html#autotoc_md147", null ],
      [ "Consequences", "md_docs_2decisions_20017-petsc-linalg.html#autotoc_md148", null ],
      [ "Validation / exit criteria", "md_docs_2decisions_20017-petsc-linalg.html#autotoc_md149", null ],
      [ "References", "md_docs_2decisions_20017-petsc-linalg.html#autotoc_md150", null ]
    ] ],
    [ "saturnCFD v1 — design", "md_docs_2Design-v1.html", [
      [ "1. Mission", "md_docs_2Design-v1.html#autotoc_md158", null ],
      [ "2. Accepted architecture", "md_docs_2Design-v1.html#autotoc_md159", null ],
      [ "3. Why collocated", "md_docs_2Design-v1.html#autotoc_md160", null ],
      [ "4. Solver sequence", "md_docs_2Design-v1.html#autotoc_md161", null ],
      [ "4a. Command line and mesh visualisation", "md_docs_2Design-v1.html#autotoc_md162", null ],
      [ "5. Directory layout", "md_docs_2Design-v1.html#autotoc_md163", null ],
      [ "5a. Dependencies", "md_docs_2Design-v1.html#autotoc_md164", null ],
      [ "5b. User-editable solvers", "md_docs_2Design-v1.html#autotoc_md165", null ],
      [ "6. What is deliberately not in v1.0", "md_docs_2Design-v1.html#autotoc_md166", null ],
      [ "6a. Documentation and formatting", "md_docs_2Design-v1.html#autotoc_md167", null ],
      [ "7. Next actions", "md_docs_2Design-v1.html#autotoc_md168", null ]
    ] ],
    [ "Command cheat sheet", "md_docs_2guides_2commands.html", [
      [ "Host requirements", "md_docs_2guides_2commands.html#autotoc_md171", null ],
      [ "First configure", "md_docs_2guides_2commands.html#autotoc_md172", null ],
      [ "Build", "md_docs_2guides_2commands.html#autotoc_md173", [
        [ "What compilation does", "md_docs_2guides_2commands.html#autotoc_md174", null ],
        [ "KOKKOS_ROOT", "md_docs_2guides_2commands.html#autotoc_md175", null ],
        [ "Manual equivalent", "md_docs_2guides_2commands.html#autotoc_md176", null ]
      ] ],
      [ "Call saturnCFD from any directory", "md_docs_2guides_2commands.html#autotoc_md177", null ],
      [ "Clean", "md_docs_2guides_2commands.html#autotoc_md178", null ],
      [ "Test", "md_docs_2guides_2commands.html#autotoc_md179", null ],
      [ "Install only one backend", "md_docs_2guides_2commands.html#autotoc_md180", null ],
      [ "Run", "md_docs_2guides_2commands.html#autotoc_md181", [
        [ "Solver field output", "md_docs_2guides_2commands.html#autotoc_md182", null ],
        [ "Checkpoint and restart", "md_docs_2guides_2commands.html#autotoc_md183", null ]
      ] ],
      [ "Help and completion", "md_docs_2guides_2commands.html#autotoc_md184", null ],
      [ "Documentation", "md_docs_2guides_2commands.html#autotoc_md185", null ],
      [ "clangd", "md_docs_2guides_2commands.html#autotoc_md186", null ],
      [ "Format", "md_docs_2guides_2commands.html#autotoc_md187", null ],
      [ "MPI and GPU", "md_docs_2guides_2commands.html#autotoc_md188", null ],
      [ "What not to memorise", "md_docs_2guides_2commands.html#autotoc_md189", null ]
    ] ],
    [ "CPU and GPU builds, threads and benchmarking", "md_docs_2guides_2gpu.html", [
      [ "The two builds", "md_docs_2guides_2gpu.html#autotoc_md191", null ],
      [ "Building the GPU backend", "md_docs_2guides_2gpu.html#autotoc_md192", null ],
      [ "Running the same case", "md_docs_2guides_2gpu.html#autotoc_md193", null ],
      [ "CPU/GPU benchmark", "md_docs_2guides_2gpu.html#autotoc_md194", null ],
      [ "What <span class=\"tt\">OMP_NUM_THREADS</span> means", "md_docs_2guides_2gpu.html#autotoc_md195", [
        [ "Explicit control", "md_docs_2guides_2gpu.html#autotoc_md196", null ],
        [ "Automatic behavior", "md_docs_2guides_2gpu.html#autotoc_md197", null ]
      ] ]
    ] ],
    [ "PETSc setup and <span class=\"tt\">saturnLinAlg</span>", "md_docs_2guides_2petsc.html", [
      [ "Installed version", "md_docs_2guides_2petsc.html#autotoc_md199", null ],
      [ "CMake discovery", "md_docs_2guides_2petsc.html#autotoc_md200", null ],
      [ "Library", "md_docs_2guides_2petsc.html#autotoc_md201", null ],
      [ "Initialization order", "md_docs_2guides_2petsc.html#autotoc_md202", null ],
      [ "Tests", "md_docs_2guides_2petsc.html#autotoc_md203", null ],
      [ "KSP/PC choices", "md_docs_2guides_2petsc.html#autotoc_md204", null ],
      [ "Pitfalls", "md_docs_2guides_2petsc.html#autotoc_md205", null ],
      [ "Next", "md_docs_2guides_2petsc.html#autotoc_md206", null ]
    ] ],
    [ "Profiling and sanitizing", "md_docs_2guides_2profiling.html", [
      [ "1. Choose the question", "md_docs_2guides_2profiling.html#autotoc_md208", null ],
      [ "2. Build types", "md_docs_2guides_2profiling.html#autotoc_md209", null ],
      [ "3. Valgrind Memcheck", "md_docs_2guides_2profiling.html#autotoc_md210", null ],
      [ "4. AddressSanitizer / UBSan", "md_docs_2guides_2profiling.html#autotoc_md211", null ],
      [ "5. CPU profiling", "md_docs_2guides_2profiling.html#autotoc_md212", null ],
      [ "6. MPI profiling", "md_docs_2guides_2profiling.html#autotoc_md213", null ],
      [ "7. GPU profiling", "md_docs_2guides_2profiling.html#autotoc_md214", null ],
      [ "8. Kokkos Tools", "md_docs_2guides_2profiling.html#autotoc_md215", null ],
      [ "9. What not to do", "md_docs_2guides_2profiling.html#autotoc_md216", null ],
      [ "10. First profiling milestones", "md_docs_2guides_2profiling.html#autotoc_md217", null ]
    ] ],
    [ "Host requirements and environment check", "md_docs_2guides_2requirements.html", [
      [ "Why", "md_docs_2guides_2requirements.html#autotoc_md219", null ],
      [ "Check", "md_docs_2guides_2requirements.html#autotoc_md220", null ],
      [ "Install missing packages and Kokkos", "md_docs_2guides_2requirements.html#autotoc_md221", null ],
      [ "CMake target", "md_docs_2guides_2requirements.html#autotoc_md222", null ],
      [ "Requirements groups", "md_docs_2guides_2requirements.html#autotoc_md223", null ],
      [ "What the checker cannot replace", "md_docs_2guides_2requirements.html#autotoc_md224", null ],
      [ "clangd", "md_docs_2guides_2requirements.html#autotoc_md225", null ],
      [ "Doxygen timestamp", "md_docs_2guides_2requirements.html#autotoc_md226", null ],
      [ "Naming", "md_docs_2guides_2requirements.html#autotoc_md227", null ],
      [ "Related", "md_docs_2guides_2requirements.html#autotoc_md228", null ]
    ] ],
    [ "Writing a user solver", "md_docs_2guides_2writingASolver.html", [
      [ "1. Copy a solver", "md_docs_2guides_2writingASolver.html#autotoc_md230", null ],
      [ "2. Directory layout", "md_docs_2guides_2writingASolver.html#autotoc_md231", [
        [ "What each file is for", "md_docs_2guides_2writingASolver.html#autotoc_md232", null ]
      ] ],
      [ "3. Configuration and units", "md_docs_2guides_2writingASolver.html#autotoc_md233", null ],
      [ "4. State and boundary conditions", "md_docs_2guides_2writingASolver.html#autotoc_md234", null ],
      [ "5. Equations", "md_docs_2guides_2writingASolver.html#autotoc_md235", null ],
      [ "6. Build integration", "md_docs_2guides_2writingASolver.html#autotoc_md236", null ],
      [ "7. Solver API version", "md_docs_2guides_2writingASolver.html#autotoc_md237", null ],
      [ "8. Tests for a user solver", "md_docs_2guides_2writingASolver.html#autotoc_md238", null ],
      [ "9. GPU notes", "md_docs_2guides_2writingASolver.html#autotoc_md239", null ],
      [ "10. Do and do not", "md_docs_2guides_2writingASolver.html#autotoc_md240", null ],
      [ "11. Worked example", "md_docs_2guides_2writingASolver.html#autotoc_md241", null ]
    ] ],
    [ "saturnCFD", "md_docs_2master.html", [
      [ "0. One-sentence pitch", "md_docs_2master.html#autotoc_md252", null ],
      [ "1. Build, run, help", "md_docs_2master.html#autotoc_md254", [
        [ "1.1 Requirements", "md_docs_2master.html#autotoc_md255", null ],
        [ "1.2 CPU / GPU build and install", "md_docs_2master.html#autotoc_md256", null ],
        [ "1.3 Running", "md_docs_2master.html#autotoc_md257", null ],
        [ "1.4 Help, validation and templates", "md_docs_2master.html#autotoc_md258", null ],
        [ "1.5 Tests", "md_docs_2master.html#autotoc_md259", null ]
      ] ],
      [ "2. Architecture and why these choices", "md_docs_2master.html#autotoc_md261", [
        [ "2.1 Modules", "md_docs_2master.html#autotoc_md262", null ],
        [ "2.2 Why YAML", "md_docs_2master.html#autotoc_md263", null ],
        [ "2.3 Why Kokkos", "md_docs_2master.html#autotoc_md264", null ],
        [ "2.4 Why PETSc", "md_docs_2master.html#autotoc_md265", null ],
        [ "2.5 Why structured hex and collocated fields", "md_docs_2master.html#autotoc_md266", null ],
        [ "2.6 Why a solver registry and separate modules", "md_docs_2master.html#autotoc_md267", null ],
        [ "2.7 Units", "md_docs_2master.html#autotoc_md268", null ]
      ] ],
      [ "3. Burgers: convection, shocks and limiters", "md_docs_2master.html#autotoc_md270", [
        [ "3.1 Equation and exact solution", "md_docs_2master.html#autotoc_md271", null ],
        [ "3.2 Numerical method", "md_docs_2master.html#autotoc_md272", null ],
        [ "3.3 Validation", "md_docs_2master.html#autotoc_md273", null ]
      ] ],
      [ "4. Heat: scalar transport and diffusion", "md_docs_2master.html#autotoc_md275", [
        [ "4.1 Equation and exact solution", "md_docs_2master.html#autotoc_md276", null ],
        [ "4.2 Numerical method", "md_docs_2master.html#autotoc_md277", null ],
        [ "4.3 Validation", "md_docs_2master.html#autotoc_md278", null ]
      ] ],
      [ "5. Stokes: vector elliptic + pressure constraint", "md_docs_2master.html#autotoc_md280", [
        [ "5.1 Equations and cases", "md_docs_2master.html#autotoc_md281", null ],
        [ "5.2 Discretisation", "md_docs_2master.html#autotoc_md282", null ],
        [ "5.3 Direct solver", "md_docs_2master.html#autotoc_md283", null ],
        [ "5.4 Fieldsplit solver", "md_docs_2master.html#autotoc_md284", null ],
        [ "5.5 Why not SIMPLE/SIMPLEC for Stokes?", "md_docs_2master.html#autotoc_md285", null ],
        [ "5.6 Bugs found during tuning (worth mentioning)", "md_docs_2master.html#autotoc_md286", null ],
        [ "5.7 Validation", "md_docs_2master.html#autotoc_md287", null ]
      ] ],
      [ "6. MMS: verifying the discretisation, not just the solver", "md_docs_2master.html#autotoc_md289", [
        [ "6.1 Why MMS if Poiseuille/Couette already pass?", "md_docs_2master.html#autotoc_md290", null ],
        [ "6.2 Periodic MMS", "md_docs_2master.html#autotoc_md291", null ],
        [ "6.3 Wall MMS", "md_docs_2master.html#autotoc_md292", null ],
        [ "6.4 Sources", "md_docs_2master.html#autotoc_md293", null ],
        [ "6.5 Measured orders", "md_docs_2master.html#autotoc_md294", null ],
        [ "6.6 Order of accuracy: the simple version", "md_docs_2master.html#autotoc_md295", null ]
      ] ],
      [ "7. What to show the professor", "md_docs_2master.html#autotoc_md297", null ],
      [ "8. Parallel terminology and the scaling campaign", "md_docs_2master.html#autotoc_md299", [
        [ "8.1 Hardware, from small to large", "md_docs_2master.html#autotoc_md300", null ],
        [ "8.2 Software, from large to small", "md_docs_2master.html#autotoc_md301", null ],
        [ "8.3 Why one thread per rank", "md_docs_2master.html#autotoc_md302", null ],
        [ "8.4 The GPU side", "md_docs_2master.html#autotoc_md303", null ],
        [ "8.5 The configurations actually run", "md_docs_2master.html#autotoc_md304", null ],
        [ "8.6 Reading one result", "md_docs_2master.html#autotoc_md305", null ],
        [ "8.7 Speedup, efficiency, and what not to claim", "md_docs_2master.html#autotoc_md306", null ],
        [ "8.8 Measured on PARAM Shakthi (CPU only)", "md_docs_2master.html#autotoc_md307", null ]
      ] ],
      [ "9. Likely “why / why not” questions", "md_docs_2master.html#autotoc_md309", null ],
      [ "10. Known limitations (say them before the professor does)", "md_docs_2master.html#autotoc_md311", null ]
    ] ],
    [ "Documentation and formatting pipeline", "md_docs_2plan_2documentationPipeline.html", [
      [ "Goals", "md_docs_2plan_2documentationPipeline.html#autotoc_md313", null ],
      [ "Source of truth", "md_docs_2plan_2documentationPipeline.html#autotoc_md314", null ],
      [ "Markdown math", "md_docs_2plan_2documentationPipeline.html#autotoc_md315", null ],
      [ "Doxygen", "md_docs_2plan_2documentationPipeline.html#autotoc_md316", null ],
      [ "GitHub Pages", "md_docs_2plan_2documentationPipeline.html#autotoc_md317", null ],
      [ "clang-format", "md_docs_2plan_2documentationPipeline.html#autotoc_md318", null ],
      [ "Doxygen comments in code", "md_docs_2plan_2documentationPipeline.html#autotoc_md319", null ],
      [ "Documentation layout", "md_docs_2plan_2documentationPipeline.html#autotoc_md320", null ],
      [ "Validation", "md_docs_2plan_2documentationPipeline.html#autotoc_md321", null ]
    ] ],
    [ "FVM layer with Kokkos-backed fields", "md_docs_2plan_2fvmKokkosPlan.html", [
      [ "0. What is implemented now", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md323", null ],
      [ "1. Goal", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md324", null ],
      [ "2. Build model: CPU and GPU are separate builds", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md325", null ],
      [ "3. Field layout", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md326", null ],
      [ "4. Local geometry on device", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md327", null ],
      [ "5. Halo adapter", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md328", null ],
      [ "6. Kernels", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md329", null ],
      [ "7. First FVM operators", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md330", null ],
      [ "7a. Manufactured-solution methodology", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md331", null ],
      [ "8. Tests", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md332", null ],
      [ "9. Performance", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md333", null ],
      [ "10. Order of work", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md334", null ],
      [ "11. Open decisions", "md_docs_2plan_2fvmKokkosPlan.html#autotoc_md335", null ]
    ] ],
    [ "MPI and GPU design", "md_docs_2plan_2mpiGpuDesign.html", [
      [ "1. Goal", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md337", null ],
      [ "2. Current status and the earlier v0 plan", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md338", null ],
      [ "3. Decomposition", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md339", null ],
      [ "4. Shared cells, shared faces, ghost cells", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md340", null ],
      [ "5. Ghost width", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md341", null ],
      [ "6. Halo exchange", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md342", null ],
      [ "7. Mesh distribution", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md343", null ],
      [ "8. Field and operator semantics", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md344", null ],
      [ "9. MPI + Kokkos initialization", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md345", null ],
      [ "10. GPU halo exchange", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md346", null ],
      [ "11. PETSc", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md347", null ],
      [ "12. One-GPU benchmark plan", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md348", null ],
      [ "13. Tests", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md349", null ],
      [ "14. Open decisions", "md_docs_2plan_2mpiGpuDesign.html#autotoc_md350", null ]
    ] ],
    [ "Parallel testing plan", "md_docs_2plan_2parallelTesting.html", [
      [ "1. Test pyramid", "md_docs_2plan_2parallelTesting.html#autotoc_md352", null ],
      [ "2. MPI test binary", "md_docs_2plan_2parallelTesting.html#autotoc_md353", null ],
      [ "3. Core MPI tests", "md_docs_2plan_2parallelTesting.html#autotoc_md354", [
        [ "Partition invariants", "md_docs_2plan_2parallelTesting.html#autotoc_md355", null ],
        [ "Halo analytic test", "md_docs_2plan_2parallelTesting.html#autotoc_md356", null ],
        [ "Rank-equality test", "md_docs_2plan_2parallelTesting.html#autotoc_md357", null ],
        [ "Double-count test", "md_docs_2plan_2parallelTesting.html#autotoc_md358", null ]
      ] ],
      [ "4. GPU parity", "md_docs_2plan_2parallelTesting.html#autotoc_md359", null ],
      [ "5. CTest integration", "md_docs_2plan_2parallelTesting.html#autotoc_md360", null ],
      [ "6. Cluster testing (later)", "md_docs_2plan_2parallelTesting.html#autotoc_md361", null ],
      [ "7. Benchmark harness", "md_docs_2plan_2parallelTesting.html#autotoc_md362", null ],
      [ "8. What to compare", "md_docs_2plan_2parallelTesting.html#autotoc_md363", null ],
      [ "9. Exit criteria for the first parallel increment", "md_docs_2plan_2parallelTesting.html#autotoc_md364", null ]
    ] ],
    [ "Phase 1 status and remaining work", "md_docs_2plan_2phase1Status.html", [
      [ "Done", "md_docs_2plan_2phase1Status.html#autotoc_md366", null ],
      [ "Pending", "md_docs_2plan_2phase1Status.html#autotoc_md367", null ],
      [ "Recommended order", "md_docs_2plan_2phase1Status.html#autotoc_md368", null ],
      [ "Exit criteria for the next increment", "md_docs_2plan_2phase1Status.html#autotoc_md369", null ],
      [ "Commands", "md_docs_2plan_2phase1Status.html#autotoc_md370", null ]
    ] ],
    [ "Repository layout", "md_docs_2plan_2repositoryLayout.html", [
      [ "Tree", "md_docs_2plan_2repositoryLayout.html#autotoc_md372", null ],
      [ "Where a new file belongs", "md_docs_2plan_2repositoryLayout.html#autotoc_md373", null ],
      [ "Module READMEs", "md_docs_2plan_2repositoryLayout.html#autotoc_md374", null ],
      [ "Naming", "md_docs_2plan_2repositoryLayout.html#autotoc_md375", null ],
      [ "Solver directories", "md_docs_2plan_2repositoryLayout.html#autotoc_md376", null ],
      [ "Tests", "md_docs_2plan_2repositoryLayout.html#autotoc_md377", null ],
      [ "Docs", "md_docs_2plan_2repositoryLayout.html#autotoc_md378", null ],
      [ "Dependencies", "md_docs_2plan_2repositoryLayout.html#autotoc_md379", null ]
    ] ],
    [ "Solver implementation sequence", "md_docs_2plan_2solverSequence.html", [
      [ "Decision summary", "md_docs_2plan_2solverSequence.html#autotoc_md381", null ],
      [ "Why Burgers first", "md_docs_2plan_2solverSequence.html#autotoc_md382", null ],
      [ "Phase 0 — PETSc linear algebra", "md_docs_2plan_2solverSequence.html#autotoc_md383", null ],
      [ "Phase 1 — Solver lifecycle and time loop", "md_docs_2plan_2solverSequence.html#autotoc_md384", null ],
      [ "Phase 2 — Burgers", "md_docs_2plan_2solverSequence.html#autotoc_md385", null ],
      [ "Phase 3 — Heat equation", "md_docs_2plan_2solverSequence.html#autotoc_md386", null ],
      [ "Phase 4 — Stokes and pressure–velocity coupling", "md_docs_2plan_2solverSequence.html#autotoc_md387", null ],
      [ "Phase 5 — Incompressible Navier–Stokes", "md_docs_2plan_2solverSequence.html#autotoc_md388", null ],
      [ "Numerical choices", "md_docs_2plan_2solverSequence.html#autotoc_md389", null ],
      [ "Open decisions", "md_docs_2plan_2solverSequence.html#autotoc_md390", null ]
    ] ],
    [ "v1 roadmap", "md_docs_2plan_2v1-roadmap.html", [
      [ "Phase 0 — decisions and protocols", "md_docs_2plan_2v1-roadmap.html#autotoc_md392", null ],
      [ "Phase 1 — substrate", "md_docs_2plan_2v1-roadmap.html#autotoc_md393", null ],
      [ "Phase 2 — Burgers", "md_docs_2plan_2v1-roadmap.html#autotoc_md394", null ],
      [ "Phase 3 — heat", "md_docs_2plan_2v1-roadmap.html#autotoc_md395", null ],
      [ "Phase 4 — Stokes", "md_docs_2plan_2v1-roadmap.html#autotoc_md396", null ],
      [ "Phase 5 — incompressible Navier–Stokes", "md_docs_2plan_2v1-roadmap.html#autotoc_md397", null ],
      [ "Phase 6 — after v1.0", "md_docs_2plan_2v1-roadmap.html#autotoc_md398", null ],
      [ "Validation matrix", "md_docs_2plan_2v1-roadmap.html#autotoc_md399", null ],
      [ "Risks", "md_docs_2plan_2v1-roadmap.html#autotoc_md400", null ],
      [ "Definition of done per solver", "md_docs_2plan_2v1-roadmap.html#autotoc_md401", null ]
    ] ],
    [ "P0b — Command-line interface", "md_docs_2protocols_2cli.html", [
      [ "Grammar", "md_docs_2protocols_2cli.html#autotoc_md403", null ],
      [ "Options", "md_docs_2protocols_2cli.html#autotoc_md404", null ],
      [ "Examples", "md_docs_2protocols_2cli.html#autotoc_md405", null ],
      [ "Template generation", "md_docs_2protocols_2cli.html#autotoc_md406", null ],
      [ "Mesh output", "md_docs_2protocols_2cli.html#autotoc_md407", null ],
      [ "Completion", "md_docs_2protocols_2cli.html#autotoc_md408", null ],
      [ "Units", "md_docs_2protocols_2cli.html#autotoc_md409", null ],
      [ "Help text", "md_docs_2protocols_2cli.html#autotoc_md410", [
        [ "Example: <span class=\"tt\">-help checkMesh</span>", "md_docs_2protocols_2cli.html#autotoc_md411", null ],
        [ "Example: <span class=\"tt\">-help dumpConfig</span>", "md_docs_2protocols_2cli.html#autotoc_md412", null ],
        [ "Example: <span class=\"tt\">-help units</span>", "md_docs_2protocols_2cli.html#autotoc_md413", null ]
      ] ],
      [ "Errors and exit codes", "md_docs_2protocols_2cli.html#autotoc_md414", null ],
      [ "Tests", "md_docs_2protocols_2cli.html#autotoc_md415", null ]
    ] ],
    [ "P5 — Configuration protocol", "md_docs_2protocols_2config.html", [
      [ "Purpose", "md_docs_2protocols_2config.html#autotoc_md417", null ],
      [ "Vocabulary", "md_docs_2protocols_2config.html#autotoc_md418", null ],
      [ "Interface", "md_docs_2protocols_2config.html#autotoc_md419", null ],
      [ "Commands", "md_docs_2protocols_2config.html#autotoc_md420", null ],
      [ "Parser", "md_docs_2protocols_2config.html#autotoc_md421", null ],
      [ "Validation stages", "md_docs_2protocols_2config.html#autotoc_md422", null ],
      [ "Units block", "md_docs_2protocols_2config.html#autotoc_md423", null ],
      [ "Invariants", "md_docs_2protocols_2config.html#autotoc_md424", null ],
      [ "Host/device rules", "md_docs_2protocols_2config.html#autotoc_md425", null ],
      [ "Tests", "md_docs_2protocols_2config.html#autotoc_md426", null ],
      [ "References", "md_docs_2protocols_2config.html#autotoc_md427", null ]
    ] ],
    [ "P0 — Documentation and citation protocol", "md_docs_2protocols_2documentation.html", [
      [ "Style", "md_docs_2protocols_2documentation.html#autotoc_md429", null ],
      [ "API documentation", "md_docs_2protocols_2documentation.html#autotoc_md430", null ],
      [ "Math", "md_docs_2protocols_2documentation.html#autotoc_md431", null ],
      [ "Build and hosting", "md_docs_2protocols_2documentation.html#autotoc_md432", null ],
      [ "Theory pages", "md_docs_2protocols_2documentation.html#autotoc_md433", null ],
      [ "Citations", "md_docs_2protocols_2documentation.html#autotoc_md434", null ],
      [ "Reference-code policy", "md_docs_2protocols_2documentation.html#autotoc_md435", null ],
      [ "Definition of done", "md_docs_2protocols_2documentation.html#autotoc_md436", null ]
    ] ],
    [ "P3 — Finite-volume protocol", "md_docs_2protocols_2finiteVolume.html", [
      [ "Purpose", "md_docs_2protocols_2finiteVolume.html#autotoc_md438", null ],
      [ "Fields", "md_docs_2protocols_2finiteVolume.html#autotoc_md439", null ],
      [ "Integral form", "md_docs_2protocols_2finiteVolume.html#autotoc_md440", null ],
      [ "Operators", "md_docs_2protocols_2finiteVolume.html#autotoc_md441", null ],
      [ "Face interpolation", "md_docs_2protocols_2finiteVolume.html#autotoc_md442", null ],
      [ "Rhie–Chow face flux", "md_docs_2protocols_2finiteVolume.html#autotoc_md443", null ],
      [ "Boundary conditions", "md_docs_2protocols_2finiteVolume.html#autotoc_md444", null ],
      [ "Parallel invariants", "md_docs_2protocols_2finiteVolume.html#autotoc_md445", null ],
      [ "Invariants", "md_docs_2protocols_2finiteVolume.html#autotoc_md446", null ],
      [ "Host/device rules", "md_docs_2protocols_2finiteVolume.html#autotoc_md447", null ],
      [ "Tests", "md_docs_2protocols_2finiteVolume.html#autotoc_md448", null ],
      [ "References", "md_docs_2protocols_2finiteVolume.html#autotoc_md449", null ]
    ] ],
    [ "P7 — Linear algebra protocol (PETSc)", "md_docs_2protocols_2linearAlgebra.html", [
      [ "Purpose", "md_docs_2protocols_2linearAlgebra.html#autotoc_md451", null ],
      [ "Interface", "md_docs_2protocols_2linearAlgebra.html#autotoc_md452", null ],
      [ "Vector form", "md_docs_2protocols_2linearAlgebra.html#autotoc_md453", null ],
      [ "Coupled saddle-point systems", "md_docs_2protocols_2linearAlgebra.html#autotoc_md454", null ],
      [ "Recommended PETSc settings", "md_docs_2protocols_2linearAlgebra.html#autotoc_md455", null ],
      [ "Error handling", "md_docs_2protocols_2linearAlgebra.html#autotoc_md456", null ],
      [ "Invariants", "md_docs_2protocols_2linearAlgebra.html#autotoc_md457", null ],
      [ "Tests", "md_docs_2protocols_2linearAlgebra.html#autotoc_md458", null ],
      [ "References", "md_docs_2protocols_2linearAlgebra.html#autotoc_md459", null ]
    ] ],
    [ "P2 — Mesh protocol (structured hexahedral <span class=\"tt\">.msh</span>)", "md_docs_2protocols_2mesh.html", [
      [ "Purpose", "md_docs_2protocols_2mesh.html#autotoc_md461", null ],
      [ "Vocabulary", "md_docs_2protocols_2mesh.html#autotoc_md462", null ],
      [ "Input", "md_docs_2protocols_2mesh.html#autotoc_md463", null ],
      [ "Pipeline", "md_docs_2protocols_2mesh.html#autotoc_md464", null ],
      [ "Structured detection invariant", "md_docs_2protocols_2mesh.html#autotoc_md465", null ],
      [ "Dimension rule", "md_docs_2protocols_2mesh.html#autotoc_md466", null ],
      [ "<span class=\"tt\">checkMesh</span> report", "md_docs_2protocols_2mesh.html#autotoc_md467", null ],
      [ "Host/device rules", "md_docs_2protocols_2mesh.html#autotoc_md468", null ],
      [ "Error behaviour", "md_docs_2protocols_2mesh.html#autotoc_md469", null ],
      [ "Tests", "md_docs_2protocols_2mesh.html#autotoc_md470", null ],
      [ "References", "md_docs_2protocols_2mesh.html#autotoc_md471", null ]
    ] ],
    [ "P4 — Parallel protocol: MPI decomposition, ghosts and Kokkos", "md_docs_2protocols_2parallel.html", [
      [ "Purpose", "md_docs_2protocols_2parallel.html#autotoc_md473", null ],
      [ "Vocabulary", "md_docs_2protocols_2parallel.html#autotoc_md474", null ],
      [ "Decomposition contract", "md_docs_2protocols_2parallel.html#autotoc_md475", null ],
      [ "Ownership invariant", "md_docs_2protocols_2parallel.html#autotoc_md476", null ],
      [ "Halo contract", "md_docs_2protocols_2parallel.html#autotoc_md477", null ],
      [ "Field contract", "md_docs_2protocols_2parallel.html#autotoc_md478", null ],
      [ "Host/device contract", "md_docs_2protocols_2parallel.html#autotoc_md479", null ],
      [ "Initialization contract", "md_docs_2protocols_2parallel.html#autotoc_md480", null ],
      [ "PETSc contract", "md_docs_2protocols_2parallel.html#autotoc_md481", null ],
      [ "Tests", "md_docs_2protocols_2parallel.html#autotoc_md482", null ],
      [ "References", "md_docs_2protocols_2parallel.html#autotoc_md483", null ]
    ] ],
    [ "P6 — Solver lifecycle protocol", "md_docs_2protocols_2solver.html", [
      [ "Purpose", "md_docs_2protocols_2solver.html#autotoc_md489", null ],
      [ "Interface", "md_docs_2protocols_2solver.html#autotoc_md490", null ],
      [ "Ownership", "md_docs_2protocols_2solver.html#autotoc_md491", null ],
      [ "Registry", "md_docs_2protocols_2solver.html#autotoc_md492", null ],
      [ "User-editable solvers", "md_docs_2protocols_2solver.html#autotoc_md493", null ],
      [ "Time loop", "md_docs_2protocols_2solver.html#autotoc_md494", null ],
      [ "Diagnostics", "md_docs_2protocols_2solver.html#autotoc_md495", null ],
      [ "Checkpoint / restart", "md_docs_2protocols_2solver.html#autotoc_md496", null ],
      [ "Invariants", "md_docs_2protocols_2solver.html#autotoc_md497", null ],
      [ "Tests", "md_docs_2protocols_2solver.html#autotoc_md498", null ],
      [ "References", "md_docs_2protocols_2solver.html#autotoc_md499", null ]
    ] ],
    [ "P9 — Testing protocol", "md_docs_2protocols_2testing.html", [
      [ "Purpose", "md_docs_2protocols_2testing.html#autotoc_md501", null ],
      [ "Framework", "md_docs_2protocols_2testing.html#autotoc_md502", null ],
      [ "Test categories", "md_docs_2protocols_2testing.html#autotoc_md503", null ],
      [ "Layout", "md_docs_2protocols_2testing.html#autotoc_md504", null ],
      [ "Manufactured solutions", "md_docs_2protocols_2testing.html#autotoc_md505", null ],
      [ "Implemented FV tests", "md_docs_2protocols_2testing.html#autotoc_md506", null ],
      [ "Burgers solver tests", "md_docs_2protocols_2testing.html#autotoc_md507", null ],
      [ "Heat solver tests", "md_docs_2protocols_2testing.html#autotoc_md508", null ],
      [ "Stokes solver tests", "md_docs_2protocols_2testing.html#autotoc_md509", null ],
      [ "PETSc tests", "md_docs_2protocols_2testing.html#autotoc_md510", null ],
      [ "Time-loop tests", "md_docs_2protocols_2testing.html#autotoc_md511", null ],
      [ "Regression policy", "md_docs_2protocols_2testing.html#autotoc_md512", null ],
      [ "Presets", "md_docs_2protocols_2testing.html#autotoc_md513", null ],
      [ "Rules", "md_docs_2protocols_2testing.html#autotoc_md514", null ],
      [ "Unit test example", "md_docs_2protocols_2testing.html#autotoc_md515", null ],
      [ "Regression example", "md_docs_2protocols_2testing.html#autotoc_md516", null ],
      [ "CI", "md_docs_2protocols_2testing.html#autotoc_md517", null ],
      [ "Exit criteria", "md_docs_2protocols_2testing.html#autotoc_md518", null ]
    ] ],
    [ "P1b — Units protocol (SI only in v1.0)", "md_docs_2protocols_2units.html", [
      [ "Purpose", "md_docs_2protocols_2units.html#autotoc_md520", null ],
      [ "Policy", "md_docs_2protocols_2units.html#autotoc_md521", null ],
      [ "YAML", "md_docs_2protocols_2units.html#autotoc_md522", null ],
      [ "Dimensions", "md_docs_2protocols_2units.html#autotoc_md523", null ],
      [ "Solver contract", "md_docs_2protocols_2units.html#autotoc_md524", null ],
      [ "Output", "md_docs_2protocols_2units.html#autotoc_md525", null ],
      [ "Validation", "md_docs_2protocols_2units.html#autotoc_md526", null ],
      [ "Future extension", "md_docs_2protocols_2units.html#autotoc_md527", null ],
      [ "Tests", "md_docs_2protocols_2units.html#autotoc_md528", null ]
    ] ],
    [ "Reference: v0", "md_docs_2Reference-v0.html", [
      [ "How to inspect v0", "md_docs_2Reference-v0.html#autotoc_md530", null ],
      [ "Disposition of v0 modules", "md_docs_2Reference-v0.html#autotoc_md531", null ],
      [ "Known v0 issues — do not carry forward", "md_docs_2Reference-v0.html#autotoc_md532", null ],
      [ "What v1 should take from v0", "md_docs_2Reference-v0.html#autotoc_md533", null ],
      [ "What v1 must not take from v0", "md_docs_2Reference-v0.html#autotoc_md534", null ]
    ] ],
    [ "Burgers equation", "md_docs_2theory_2burgers.html", [
      [ "1. Governing equations", "md_docs_2theory_2burgers.html#autotoc_md536", [
        [ "Scalar Burgers", "md_docs_2theory_2burgers.html#autotoc_md537", null ],
        [ "Vector Burgers", "md_docs_2theory_2burgers.html#autotoc_md538", null ]
      ] ],
      [ "2. Exact solution used for validation", "md_docs_2theory_2burgers.html#autotoc_md539", null ],
      [ "3. Finite-volume discretisation", "md_docs_2theory_2burgers.html#autotoc_md540", null ],
      [ "4. Time integration", "md_docs_2theory_2burgers.html#autotoc_md541", null ],
      [ "5. Validation plan", "md_docs_2theory_2burgers.html#autotoc_md542", null ],
      [ "6. Implemented 1D/2D solver", "md_docs_2theory_2burgers.html#autotoc_md543", null ],
      [ "7. Observations from the validation runs", "md_docs_2theory_2burgers.html#autotoc_md544", null ],
      [ "8. Limitations", "md_docs_2theory_2burgers.html#autotoc_md545", null ],
      [ "References", "md_docs_2theory_2burgers.html#autotoc_md546", null ]
    ] ],
    [ "Coupled saddle-point solver", "md_docs_2theory_2coupledSaddlePoint.html", [
      [ "1. Why a coupled solve", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md548", null ],
      [ "2. Block system", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md549", null ],
      [ "3. Collocated finite-volume discretisation", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md550", null ],
      [ "4. PETSc linear solver", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md551", [
        [ "Matrix and vectors", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md552", null ],
        [ "Outer Krylov method", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md553", null ],
        [ "Fieldsplit / Schur preconditioner", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md554", null ],
        [ "Recommended PETSc settings", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md555", null ]
      ] ],
      [ "5. Navier–Stokes extension", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md556", null ],
      [ "6. Validation and test matrix", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md557", null ],
      [ "References", "md_docs_2theory_2coupledSaddlePoint.html#autotoc_md558", null ]
    ] ],
    [ "Finite-volume operators on a structured collocated mesh", "md_docs_2theory_2finiteVolume.html", [
      [ "1. Cell and face geometry", "md_docs_2theory_2finiteVolume.html#autotoc_md560", null ],
      [ "2. Generic conservation law", "md_docs_2theory_2finiteVolume.html#autotoc_md561", null ],
      [ "3. Face interpolation", "md_docs_2theory_2finiteVolume.html#autotoc_md562", null ],
      [ "4. Gradient", "md_docs_2theory_2finiteVolume.html#autotoc_md563", null ],
      [ "5. Divergence", "md_docs_2theory_2finiteVolume.html#autotoc_md564", null ],
      [ "6. Laplacian and non-orthogonal correction", "md_docs_2theory_2finiteVolume.html#autotoc_md565", null ],
      [ "7. Convection", "md_docs_2theory_2finiteVolume.html#autotoc_md566", null ],
      [ "8. Collocated pressure–velocity coupling", "md_docs_2theory_2finiteVolume.html#autotoc_md567", null ],
      [ "9. SIMPLE (optional steady incompressible path)", "md_docs_2theory_2finiteVolume.html#autotoc_md568", null ],
      [ "10. PISO (transient incompressible)", "md_docs_2theory_2finiteVolume.html#autotoc_md569", null ],
      [ "11. Boundary conditions", "md_docs_2theory_2finiteVolume.html#autotoc_md570", null ],
      [ "12. Parallel finite volume", "md_docs_2theory_2finiteVolume.html#autotoc_md571", null ],
      [ "References", "md_docs_2theory_2finiteVolume.html#autotoc_md572", null ]
    ] ],
    [ "Heat and convection–diffusion", "md_docs_2theory_2heat.html", [
      [ "1. Governing equation", "md_docs_2theory_2heat.html#autotoc_md574", null ],
      [ "2. Exact solutions", "md_docs_2theory_2heat.html#autotoc_md575", [
        [ "Steady 1D convection–diffusion", "md_docs_2theory_2heat.html#autotoc_md576", null ],
        [ "Gaussian pulse", "md_docs_2theory_2heat.html#autotoc_md577", null ],
        [ "Manufactured solution", "md_docs_2theory_2heat.html#autotoc_md578", null ]
      ] ],
      [ "3. Discretisation", "md_docs_2theory_2heat.html#autotoc_md579", null ],
      [ "4. Time integration", "md_docs_2theory_2heat.html#autotoc_md580", null ],
      [ "5. Validation plan", "md_docs_2theory_2heat.html#autotoc_md581", null ],
      [ "6. Implementation and measured results", "md_docs_2theory_2heat.html#autotoc_md582", [
        [ "Periodic Fourier convergence, \\(t=0.1\\) s", "md_docs_2theory_2heat.html#autotoc_md583", null ],
        [ "Periodic Fourier convergence, 2D, \\(t=0.2\\) s", "md_docs_2theory_2heat.html#autotoc_md584", null ],
        [ "Observations", "md_docs_2theory_2heat.html#autotoc_md585", null ]
      ] ],
      [ "References", "md_docs_2theory_2heat.html#autotoc_md586", null ]
    ] ],
    [ "Incompressible Navier–Stokes", "md_docs_2theory_2incompressible.html", [
      [ "1. Governing equations", "md_docs_2theory_2incompressible.html#autotoc_md588", null ],
      [ "2. Discretisation", "md_docs_2theory_2incompressible.html#autotoc_md589", null ],
      [ "3. Time integration: PISO", "md_docs_2theory_2incompressible.html#autotoc_md590", null ],
      [ "4. Boundary conditions", "md_docs_2theory_2incompressible.html#autotoc_md591", null ],
      [ "5. Validation", "md_docs_2theory_2incompressible.html#autotoc_md592", [
        [ "Lid-driven cavity", "md_docs_2theory_2incompressible.html#autotoc_md593", null ],
        [ "Taylor–Green vortex", "md_docs_2theory_2incompressible.html#autotoc_md594", null ],
        [ "Channel flow", "md_docs_2theory_2incompressible.html#autotoc_md595", null ]
      ] ],
      [ "6. Diagnostics", "md_docs_2theory_2incompressible.html#autotoc_md596", null ],
      [ "7. Observations (to be filled from runs)", "md_docs_2theory_2incompressible.html#autotoc_md597", null ],
      [ "References", "md_docs_2theory_2incompressible.html#autotoc_md598", null ]
    ] ],
    [ "Method of manufactured solutions", "md_docs_2theory_2manufacturedSolutions.html", [
      [ "1. What it is", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md600", null ],
      [ "2. Why we use it", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md601", null ],
      [ "3. Diffusion/Laplacian MMS", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md602", null ],
      [ "4. Advection–diffusion MMS", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md603", null ],
      [ "5. What to measure", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md604", [
        [ "First measured results", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md605", null ]
      ] ],
      [ "6. Test matrix", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md606", null ],
      [ "7. Why periodic first", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md607", null ],
      [ "8. MMS versus other tests", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md608", null ],
      [ "9. Common pitfalls", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md609", null ],
      [ "10. Acceptance criteria", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md610", null ],
      [ "References", "md_docs_2theory_2manufacturedSolutions.html#autotoc_md611", null ]
    ] ],
    [ "Poisson equation and the PETSc matrix", "md_docs_2theory_2poisson.html", [
      [ "1. Continuous problem", "md_docs_2theory_2poisson.html#autotoc_md613", null ],
      [ "2. Finite-volume matrix", "md_docs_2theory_2poisson.html#autotoc_md614", [
        [ "Dirichlet face", "md_docs_2theory_2poisson.html#autotoc_md615", null ],
        [ "Neumann face", "md_docs_2theory_2poisson.html#autotoc_md616", null ],
        [ "All-Neumann nullspace", "md_docs_2theory_2poisson.html#autotoc_md617", null ]
      ] ],
      [ "3. Non-orthogonal correction", "md_docs_2theory_2poisson.html#autotoc_md618", null ],
      [ "4. Solver", "md_docs_2theory_2poisson.html#autotoc_md619", null ],
      [ "5. Tests", "md_docs_2theory_2poisson.html#autotoc_md620", null ],
      [ "6. Production assembly plan", "md_docs_2theory_2poisson.html#autotoc_md621", null ],
      [ "References", "md_docs_2theory_2poisson.html#autotoc_md622", null ]
    ] ],
    [ "Stokes", "md_docs_2theory_2stokes.html", [
      [ "1. Governing equations", "md_docs_2theory_2stokes.html#autotoc_md631", null ],
      [ "2. Solution method", "md_docs_2theory_2stokes.html#autotoc_md632", null ],
      [ "3. Discretisation", "md_docs_2theory_2stokes.html#autotoc_md633", null ],
      [ "4. Boundary conditions", "md_docs_2theory_2stokes.html#autotoc_md634", null ],
      [ "5. Analytical validation", "md_docs_2theory_2stokes.html#autotoc_md635", [
        [ "Plane Poiseuille flow", "md_docs_2theory_2stokes.html#autotoc_md636", null ],
        [ "Plane Couette flow", "md_docs_2theory_2stokes.html#autotoc_md637", null ],
        [ "Manufactured solution", "md_docs_2theory_2stokes.html#autotoc_md638", null ],
        [ "Exit-boundary-condition cases", "md_docs_2theory_2stokes.html#autotoc_md639", null ]
      ] ],
      [ "6. Validation plan", "md_docs_2theory_2stokes.html#autotoc_md640", null ],
      [ "7. Implemented results", "md_docs_2theory_2stokes.html#autotoc_md641", null ],
      [ "8. MMS status", "md_docs_2theory_2stokes.html#autotoc_md642", null ],
      [ "9. Next steps", "md_docs_2theory_2stokes.html#autotoc_md643", null ],
      [ "10. Observations", "md_docs_2theory_2stokes.html#autotoc_md644", null ],
      [ "References", "md_docs_2theory_2stokes.html#autotoc_md645", null ]
    ] ],
    [ "Time integration", "md_docs_2theory_2timeIntegration.html", [
      [ "1. Semi-discrete form", "md_docs_2theory_2timeIntegration.html#autotoc_md647", null ],
      [ "2. Implemented explicit schemes", "md_docs_2theory_2timeIntegration.html#autotoc_md648", null ],
      [ "3. Stability limits", "md_docs_2theory_2timeIntegration.html#autotoc_md649", null ],
      [ "4. Planned implicit schemes", "md_docs_2theory_2timeIntegration.html#autotoc_md650", null ],
      [ "5. IMEX for Burgers and heat", "md_docs_2theory_2timeIntegration.html#autotoc_md651", null ],
      [ "6. Tests", "md_docs_2theory_2timeIntegration.html#autotoc_md652", null ],
      [ "References", "md_docs_2theory_2timeIntegration.html#autotoc_md653", null ]
    ] ],
    [ "Units and dimensional consistency", "md_docs_2theory_2units.html", [
      [ "1. Conventions", "md_docs_2theory_2units.html#autotoc_md655", null ],
      [ "1a. YAML units declaration", "md_docs_2theory_2units.html#autotoc_md656", null ],
      [ "2. Dynamic pressure, not kinematic pressure", "md_docs_2theory_2units.html#autotoc_md657", null ],
      [ "3. Benchmarks in SI", "md_docs_2theory_2units.html#autotoc_md658", null ],
      [ "4. Burgers and heat", "md_docs_2theory_2units.html#autotoc_md659", null ],
      [ "5. What not to do", "md_docs_2theory_2units.html#autotoc_md660", null ],
      [ "6. Future extensions", "md_docs_2theory_2units.html#autotoc_md661", null ],
      [ "References", "md_docs_2theory_2units.html#autotoc_md662", null ]
    ] ],
    [ "Burgers validation — 1D scalar <span class=\"tt\">saturnBurger</span>", "md_docs_2validation_2burgers.html", [
      [ "Measured convergence", "md_docs_2validation_2burgers.html#autotoc_md664", null ],
      [ "Rank independence", "md_docs_2validation_2burgers.html#autotoc_md665", null ],
      [ "Other checks", "md_docs_2validation_2burgers.html#autotoc_md666", null ],
      [ "Standalone example", "md_docs_2validation_2burgers.html#autotoc_md667", null ],
      [ "2D diagonal wave", "md_docs_2validation_2burgers.html#autotoc_md668", null ],
      [ "3D diagonal wave", "md_docs_2validation_2burgers.html#autotoc_md669", null ],
      [ "Checkpoint and restart", "md_docs_2validation_2burgers.html#autotoc_md670", null ],
      [ "Reproduce", "md_docs_2validation_2burgers.html#autotoc_md671", null ],
      [ "Verdict", "md_docs_2validation_2burgers.html#autotoc_md672", null ]
    ] ],
    [ "Fieldsplit versus direct LU", "md_docs_2validation_2fieldsplit.html", [
      [ "Block structure", "md_docs_2validation_2fieldsplit.html#autotoc_md674", null ],
      [ "Production configuration", "md_docs_2validation_2fieldsplit.html#autotoc_md675", null ],
      [ "Fully periodic domains", "md_docs_2validation_2fieldsplit.html#autotoc_md676", null ],
      [ "Known-good sub-solver configurations", "md_docs_2validation_2fieldsplit.html#autotoc_md677", null ],
      [ "Measured comparison", "md_docs_2validation_2fieldsplit.html#autotoc_md678", null ],
      [ "Reproduction", "md_docs_2validation_2fieldsplit.html#autotoc_md679", null ],
      [ "Limitations", "md_docs_2validation_2fieldsplit.html#autotoc_md680", null ]
    ] ],
    [ "Heat / convection–diffusion validation", "md_docs_2validation_2heat.html", [
      [ "1. Case", "md_docs_2validation_2heat.html#autotoc_md682", null ],
      [ "2. Numerics", "md_docs_2validation_2heat.html#autotoc_md683", null ],
      [ "3. Reference", "md_docs_2validation_2heat.html#autotoc_md684", null ],
      [ "4. Results", "md_docs_2validation_2heat.html#autotoc_md685", null ],
      [ "5. Observations", "md_docs_2validation_2heat.html#autotoc_md686", null ],
      [ "6. Reproduction", "md_docs_2validation_2heat.html#autotoc_md687", null ]
    ] ],
    [ "Regression tests", "md_docs_2validation_2regressionTests.html", [
      [ "What a regression test is", "md_docs_2validation_2regressionTests.html#autotoc_md695", null ],
      [ "Why we need them", "md_docs_2validation_2regressionTests.html#autotoc_md696", null ],
      [ "What is stored", "md_docs_2validation_2regressionTests.html#autotoc_md697", null ],
      [ "Rules", "md_docs_2validation_2regressionTests.html#autotoc_md698", null ],
      [ "What to compare", "md_docs_2validation_2regressionTests.html#autotoc_md699", null ],
      [ "Running", "md_docs_2validation_2regressionTests.html#autotoc_md700", null ],
      [ "Updating an expectation", "md_docs_2validation_2regressionTests.html#autotoc_md701", null ],
      [ "Phase 1 regression cases", "md_docs_2validation_2regressionTests.html#autotoc_md702", null ],
      [ "Why not only unit tests", "md_docs_2validation_2regressionTests.html#autotoc_md703", null ],
      [ "References", "md_docs_2validation_2regressionTests.html#autotoc_md704", null ]
    ] ],
    [ "Scaling of <span class=\"tt\">saturnHeat</span> on PARAM Shakthi", "md_docs_2validation_2scaling.html", [
      [ "Case", "md_docs_2validation_2scaling.html#autotoc_md706", null ],
      [ "Strong scaling, 128³", "md_docs_2validation_2scaling.html#autotoc_md707", null ],
      [ "Weak scaling, 32³ per rank", "md_docs_2validation_2scaling.html#autotoc_md708", null ],
      [ "What the numbers say", "md_docs_2validation_2scaling.html#autotoc_md709", null ],
      [ "Cause", "md_docs_2validation_2scaling.html#autotoc_md710", null ],
      [ "What to claim, and what not to", "md_docs_2validation_2scaling.html#autotoc_md711", null ],
      [ "What to run next", "md_docs_2validation_2scaling.html#autotoc_md712", null ],
      [ "Data", "md_docs_2validation_2scaling.html#autotoc_md713", null ]
    ] ],
    [ "Stokes validation", "md_docs_2validation_2stokes.html", [
      [ "1. Cases", "md_docs_2validation_2stokes.html#autotoc_md717", null ],
      [ "2. Numerics", "md_docs_2validation_2stokes.html#autotoc_md718", null ],
      [ "3. Error norms", "md_docs_2validation_2stokes.html#autotoc_md719", null ],
      [ "4. Acceptance targets", "md_docs_2validation_2stokes.html#autotoc_md720", null ],
      [ "4a. Measured results", "md_docs_2validation_2stokes.html#autotoc_md721", [
        [ "Fieldsplit default", "md_docs_2validation_2stokes.html#autotoc_md722", null ],
        [ "Source-consistent MMS", "md_docs_2validation_2stokes.html#autotoc_md723", null ]
      ] ],
      [ "4b. Direct versus fieldsplit", "md_docs_2validation_2stokes.html#autotoc_md724", null ],
      [ "5. Test/CTest plan", "md_docs_2validation_2stokes.html#autotoc_md725", null ],
      [ "6. Reproduction", "md_docs_2validation_2stokes.html#autotoc_md726", null ],
      [ "7. Observations", "md_docs_2validation_2stokes.html#autotoc_md727", null ],
      [ "References", "md_docs_2validation_2stokes.html#autotoc_md728", null ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"BoundaryFill_8cpp.html",
"classsaturn_1_1FaceField.html#ad9242cd050eae24ea58650c272aef3a6",
"classsaturn_1_1SaturnHeat.html#a7f92d3ee6946402d680bfeb45adf62d3",
"functions_vars_w.html",
"md_docs_2guides_2writingASolver.html#autotoc_md235",
"md_docs_2protocols_2testing.html#autotoc_md508",
"namespacesaturn.html#a802a7e04de7c6094075f90fe527a1936",
"structsaturn_1_1LocalMeshData.html#a23de9297fcd763c46bb6d9e0c69dc462",
"structsaturn_1_1StokesOptions.html#a4f6b1e4c0c6f9bc3089b750d2bd62af5"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';