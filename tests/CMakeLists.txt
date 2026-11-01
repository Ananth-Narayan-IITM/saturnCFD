add_executable(testSaturn
  TestMain.cpp
  TestUnits.cpp
  TestConfig.cpp
  TestMesh.cpp
  TestRegression.cpp
)
set_target_properties(testSaturn PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_link_libraries(testSaturn PRIVATE saturn::Core doctest::doctest)
target_compile_definitions(testSaturn PRIVATE
  SATURN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/meshes"
  SATURN_REGRESSION_DIR="${CMAKE_CURRENT_SOURCE_DIR}/regression"
)

add_test(NAME testSaturn COMMAND testSaturn)
set_tests_properties(testSaturn PROPERTIES
  LABELS "unit;mesh;config;regression;saturnCFD")

if(SATURN_ENABLE_MPI)
  add_executable(testSaturnMPI
    TestMpiMain.cpp
    TestParallel.cpp
  )
  set_target_properties(testSaturnMPI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_libraries(testSaturnMPI PRIVATE
    saturn::Core doctest::doctest MPI::MPI_CXX)

  add_test(NAME mpi_np1
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 1
            ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:testSaturnMPI>)
  add_test(NAME mpi_np2
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 2
            ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:testSaturnMPI>)
  add_test(NAME mpi_np4
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
            ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:testSaturnMPI>)
  set_tests_properties(mpi_np1 mpi_np2 mpi_np4 PROPERTIES
    LABELS "mpi;saturnCFD" RUN_SERIAL TRUE
    ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")
endif()

if(SATURN_ENABLE_KOKKOS)
  add_executable(testSaturnFV
    TestFvMain.cpp
    TestFv.cpp
    TestLocalMesh.cpp
    TestSimulation.cpp
    TestSaturnBurger.cpp
    TestSaturnBurger2D.cpp
    TestSaturnBurger3D.cpp
    TestSaturnHeat.cpp
  )
  set_target_properties(testSaturnFV PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_libraries(testSaturnFV PRIVATE
    saturn::Core saturn::Simulation saturn::Burger saturn::Heat doctest::doctest)

  set(SATURN_FV_LABELS "solver;fv;regression;parity")
  if(SATURN_BACKEND STREQUAL "cuda")
    list(APPEND SATURN_FV_LABELS "gpu")
  endif()

  if(SATURN_ENABLE_MPI)
    foreach(ranks 1 2 4)
      add_test(NAME fv_np${ranks}
        COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${ranks}
                ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:testSaturnFV>)
      set_tests_properties(fv_np${ranks} PROPERTIES
        LABELS "${SATURN_FV_LABELS};mpi" RUN_SERIAL TRUE
        ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")
    endforeach()
  else()
    add_test(NAME fv_serial COMMAND testSaturnFV)
    set_tests_properties(fv_serial PROPERTIES LABELS "${SATURN_FV_LABELS}")
  endif()
endif()

if(SATURN_ENABLE_PETSC)
  add_executable(testSaturnPETSc
    TestPetscMain.cpp
    TestPetsc.cpp
  )
  set_target_properties(testSaturnPETSc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_libraries(testSaturnPETSc PRIVATE
    saturn::LinAlg doctest::doctest)


  add_executable(testSaturnStokes
    TestPetscMain.cpp
    TestSaturnStokes.cpp
  )
  set_target_properties(testSaturnStokes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_libraries(testSaturnStokes PRIVATE
    saturn::Stokes doctest::doctest)

  if(SATURN_ENABLE_MPI)
    foreach(ranks 1 2 4)
      add_test(NAME stokes_np${ranks}
        COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${ranks}
                ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:testSaturnStokes>)
      set_tests_properties(stokes_np${ranks} PROPERTIES
        LABELS "solver;stokes;regression;mpi" RUN_SERIAL TRUE
        ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")
    endforeach()
  else()
    add_test(NAME stokes_serial COMMAND testSaturnStokes)
    set_tests_properties(stokes_serial PROPERTIES
      LABELS "solver;stokes;regression")
  endif()
  if(SATURN_ENABLE_MPI)
    foreach(ranks 1 2 4)
      add_test(NAME petsc_np${ranks}
        COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${ranks}
                ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:testSaturnPETSc>)
      set_tests_properties(petsc_np${ranks} PROPERTIES
        LABELS "petsc;linalg;regression;mpi;saturnCFD" RUN_SERIAL TRUE
        ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")
    endforeach()
  else()
    add_test(NAME petsc_serial COMMAND testSaturnPETSc)
    set_tests_properties(petsc_serial PROPERTIES
      LABELS "petsc;linalg;regression;saturnCFD")
  endif()
endif()

# CLI regression checks: the path a user actually runs.
add_test(NAME cli_help COMMAND saturnCFD -help)
add_test(NAME cli_version COMMAND saturnCFD -version)
add_test(NAME cli_listSolvers COMMAND saturnCFD -listSolvers)
add_test(NAME cli_dumpConfig COMMAND saturnCFD -dumpConfig common)
add_test(NAME cli_checkConfig_si
  COMMAND saturnCFD -checkConfig
          ${CMAKE_CURRENT_SOURCE_DIR}/regression/config/siOnly.yaml)
add_test(NAME cli_checkConfig_cgs
  COMMAND saturnCFD -checkConfig
          ${CMAKE_CURRENT_SOURCE_DIR}/regression/config/cgsRejected.yaml)
set_tests_properties(cli_checkConfig_cgs PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_checkMesh_box
  COMMAND saturnCFD -checkMesh ${CMAKE_CURRENT_SOURCE_DIR}/data/meshes/box2x2x2.msh)
add_test(NAME cli_checkMesh_tet
  COMMAND saturnCFD -checkMesh ${CMAKE_CURRENT_SOURCE_DIR}/data/meshes/tet.msh)
set_tests_properties(cli_checkMesh_tet PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_defaultOutputs
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DMESH=${CMAKE_CURRENT_SOURCE_DIR}/data/meshes/box2x2x2.msh
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/cli_defaults/mesh
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkVtk.cmake)
add_test(NAME cli_writeVTK
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DMESH=${CMAKE_CURRENT_SOURCE_DIR}/data/meshes/box2x2x2.msh
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/cli_vtk/mesh
          -DWITH_PATCHES=ON
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkVtk.cmake)
add_test(NAME cli_completion
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkCompletion.cmake)
add_test(NAME cli_templateExtension
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/cli_template/template
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkTemplate.cmake)

set_tests_properties(
  cli_help cli_version cli_listSolvers cli_dumpConfig
  cli_checkConfig_si cli_checkConfig_cgs cli_checkMesh_box cli_checkMesh_tet
  cli_defaultOutputs cli_writeVTK cli_completion cli_templateExtension
  PROPERTIES LABELS "cli;regression;saturnCFD")

# YAML-driven Burgers validation: runs the real CLI, checks the analytic norms
# and the profile CSV. The MPI variants exercise the same case on 1/2/4 ranks.
set(SATURN_BURGERS_CASE
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/burgers/traveling64.yaml")

add_test(NAME cli_checkConfig_burgers
  COMMAND saturnCFD -checkConfig ${SATURN_BURGERS_CASE})
set_tests_properties(cli_checkConfig_burgers
  PROPERTIES LABELS "cli;config;burgers;regression;saturnCFD")

# The standalone user-facing example must stay valid and numerically clean.
set(SATURN_BURGERS_EXAMPLE
    "${SATURN_SOURCE_DIR}/examples/saturnBurger/saturnBurger1D/saturnBurger.yaml")
add_test(NAME cli_checkConfig_example_burgers
  COMMAND saturnCFD -checkConfig ${SATURN_BURGERS_EXAMPLE})
set_tests_properties(cli_checkConfig_example_burgers
  PROPERTIES LABELS "cli;config;burgers;regression;saturnCFD")

# Examples are user-editable demos: only validate their schema in CTest.
# Numerical regression uses the stable cases in tests/regression/.
set(SATURN_BURGERS2D_EXAMPLE
    "${SATURN_SOURCE_DIR}/examples/saturnBurger/saturnBurger2D/saturnBurger2D.yaml")
add_test(NAME cli_checkConfig_example_burgers2d
  COMMAND saturnCFD -checkConfig ${SATURN_BURGERS2D_EXAMPLE})
set_tests_properties(cli_checkConfig_example_burgers2d
  PROPERTIES LABELS "cli;config;burgers;burgers2d;regression;saturnCFD")

set(SATURN_BURGERS2D_CASE
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/burgers2d/traveling64.yaml")
add_test(NAME burgers_2d_traveling64
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DCASE=${SATURN_BURGERS2D_CASE}
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/burgers2d_traveling64
          -DPREFIX=results/burgers2d/traveling64
          -DROWS=4097
          -DL2_MIN=2.0e-3
          -DL2_MAX=3.0e-3
          -DLINF_MAX=3.0e-2
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkBurgers.cmake)
set_tests_properties(burgers_2d_traveling64 PROPERTIES
  LABELS "solver;burgers;burgers2d;regression" RUN_SERIAL TRUE
  ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")

set(SATURN_BURGERS_VTK_CASE
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/burgers/vtk.yaml")
add_test(NAME burgers_vtk
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DCASE=${SATURN_BURGERS_VTK_CASE}
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/burgers_vtk
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkBurgersVtk.cmake)
set_tests_properties(burgers_vtk PROPERTIES
  LABELS "burgers;vtk;regression;saturnCFD" RUN_SERIAL TRUE
  ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")

set(SATURN_BURGERS3D_EXAMPLE
    "${SATURN_SOURCE_DIR}/examples/saturnBurger/saturnBurger3D/saturnBurger3D.yaml")
add_test(NAME cli_checkConfig_example_burgers3d
  COMMAND saturnCFD -checkConfig ${SATURN_BURGERS3D_EXAMPLE})
set_tests_properties(cli_checkConfig_example_burgers3d
  PROPERTIES LABELS "cli;config;burgers;burgers3d;regression;saturnCFD")

set(SATURN_BURGERS3D_CASE
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/burgers3d/traveling16.yaml")
add_test(NAME burgers_3d_traveling16
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DCASE=${SATURN_BURGERS3D_CASE}
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/burgers3d_traveling16
          -DPREFIX=results/burgers3d/traveling16
          -DROWS=4097
          -DL2_MIN=1.5e-3
          -DL2_MAX=4.0e-3
          -DLINF_MAX=2.0e-2
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkBurgers.cmake)
set_tests_properties(burgers_3d_traveling16 PROPERTIES
  LABELS "solver;burgers;burgers3d;regression" RUN_SERIAL TRUE
  ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")

# YAML-driven heat (scalar convection–diffusion) validation. The exact data is
# the periodic Fourier mode T = A sin(k.x - (a.k)t) exp(-alpha |k|^2 t).
set(SATURN_HEAT2D_EXAMPLE
    "${SATURN_SOURCE_DIR}/examples/saturnHeat/saturnHeat2D/saturnHeat2D.yaml")
add_test(NAME cli_checkConfig_example_heat2d
  COMMAND saturnCFD -checkConfig ${SATURN_HEAT2D_EXAMPLE})
set_tests_properties(cli_checkConfig_example_heat2d
  PROPERTIES LABELS "cli;config;heat;heat2d;regression;saturnCFD")

set(SATURN_STOKES2D_EXAMPLE
    "${SATURN_SOURCE_DIR}/examples/saturnStokes/saturnStokes2D/poiseuille.yaml")
add_test(NAME cli_checkConfig_example_stokes2d
  COMMAND saturnCFD -checkConfig ${SATURN_STOKES2D_EXAMPLE})
set_tests_properties(cli_checkConfig_example_stokes2d
  PROPERTIES LABELS "cli;config;stokes;stokes2d;regression;saturnCFD")

set(SATURN_STOKES3D_EXAMPLE
    "${SATURN_SOURCE_DIR}/examples/saturnStokes/saturnStokes3D/poiseuille.yaml")
add_test(NAME cli_checkConfig_example_stokes3d
  COMMAND saturnCFD -checkConfig ${SATURN_STOKES3D_EXAMPLE})
set_tests_properties(cli_checkConfig_example_stokes3d
  PROPERTIES LABELS "cli;config;stokes;stokes3d;regression;saturnCFD")

set(SATURN_HEAT3D_EXAMPLE
    "${SATURN_SOURCE_DIR}/examples/saturnHeat/saturnHeat3D/saturnHeat3D.yaml")
add_test(NAME cli_checkConfig_example_heat3d
  COMMAND saturnCFD -checkConfig ${SATURN_HEAT3D_EXAMPLE})
set_tests_properties(cli_checkConfig_example_heat3d
  PROPERTIES LABELS "cli;config;heat;heat3d;regression;saturnCFD")

set(SATURN_HEAT2D_CASE
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/heat2d/fourier64.yaml")
add_test(NAME heat_2d_fourier64
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DCASE=${SATURN_HEAT2D_CASE}
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/heat2d_fourier64
          -DPREFIX=results/heat2d/fourier64
          -DROWS=4097
          -DL2_MIN=9.0e-4
          -DL2_MAX=1.3e-3
          -DLINF_MAX=3.0e-3
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkHeat.cmake)
set_tests_properties(heat_2d_fourier64 PROPERTIES
  LABELS "solver;heat;heat2d;regression" RUN_SERIAL TRUE
  ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")

set(SATURN_HEAT3D_CASE
    "${CMAKE_CURRENT_SOURCE_DIR}/regression/heat3d/fourier32.yaml")
add_test(NAME heat_3d_fourier32
  COMMAND ${CMAKE_COMMAND}
          -DCLI=$<TARGET_FILE:saturnCFD>
          -DCASE=${SATURN_HEAT3D_CASE}
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/heat3d_fourier32
          -DPREFIX=results/heat3d/fourier32
          -DROWS=32769
          -DL2_MIN=3.0e-3
          -DL2_MAX=5.0e-3
          -DLINF_MAX=1.5e-2
          -P ${CMAKE_CURRENT_SOURCE_DIR}/checkHeat.cmake)
set_tests_properties(heat_3d_fourier32 PROPERTIES
  LABELS "solver;heat;heat3d;regression" RUN_SERIAL TRUE
  ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME burgers_checkpoint
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/checkBurgersCheckpoint.py
            --cli $<TARGET_FILE:saturnCFD>
            --out ${CMAKE_CURRENT_BINARY_DIR}/burgers_checkpoint
            --full ${CMAKE_CURRENT_SOURCE_DIR}/regression/burgers/restartFull.yaml
            --checkpoint ${CMAKE_CURRENT_SOURCE_DIR}/regression/burgers/restartCheckpoint.yaml
            --template ${CMAKE_CURRENT_SOURCE_DIR}/regression/burgers/restartTemplate.yaml.in)
  set_tests_properties(burgers_checkpoint PROPERTIES
    LABELS "burgers;checkpoint;regression;saturnCFD" RUN_SERIAL TRUE
    ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")

  add_test(NAME heat_checkpoint
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/checkHeatCheckpoint.py
            --cli $<TARGET_FILE:saturnCFD>
            --out ${CMAKE_CURRENT_BINARY_DIR}/heat_checkpoint
            --full ${CMAKE_CURRENT_SOURCE_DIR}/regression/heat3d/restartFull.yaml
            --checkpoint ${CMAKE_CURRENT_SOURCE_DIR}/regression/heat3d/restartCheckpoint.yaml
            --template ${CMAKE_CURRENT_SOURCE_DIR}/regression/heat3d/restartTemplate.yaml.in)
  set_tests_properties(heat_checkpoint PROPERTIES
    LABELS "heat;checkpoint;regression;saturnCFD" RUN_SERIAL TRUE
    ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")
endif()

if(SATURN_ENABLE_MPI)
  foreach(ranks 1 2 4)
    add_test(NAME burgers_traveling64_np${ranks}
      COMMAND ${CMAKE_COMMAND}
              -DCLI=$<TARGET_FILE:saturnCFD>
              -DCASE=${SATURN_BURGERS_CASE}
              -DOUT=${CMAKE_CURRENT_BINARY_DIR}/burgers_np${ranks}
              -DMPIEXEC=${MPIEXEC_EXECUTABLE}
              -DRANKS=${ranks}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/checkBurgers.cmake)
    set_tests_properties(burgers_traveling64_np${ranks} PROPERTIES
      LABELS "solver;burgers;regression;mpi" RUN_SERIAL TRUE
      ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")
  endforeach()
else()
  add_test(NAME burgers_traveling64
    COMMAND ${CMAKE_COMMAND}
            -DCLI=$<TARGET_FILE:saturnCFD>
            -DCASE=${SATURN_BURGERS_CASE}
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/burgers_serial
            -P ${CMAKE_CURRENT_SOURCE_DIR}/checkBurgers.cmake)
  set_tests_properties(burgers_traveling64
    PROPERTIES LABELS "solver;burgers;regression")
endif()

# The heat case is periodic, so the MPI variants also exercise periodic halos
# and the rank-0 mesh distribution.
if(SATURN_ENABLE_MPI)
  foreach(ranks 1 2 4)
    add_test(NAME heat_fourier32_np${ranks}
      COMMAND ${CMAKE_COMMAND}
              -DCLI=$<TARGET_FILE:saturnCFD>
              -DCASE=${SATURN_HEAT3D_CASE}
              -DOUT=${CMAKE_CURRENT_BINARY_DIR}/heat_np${ranks}
              -DMPIEXEC=${MPIEXEC_EXECUTABLE}
              -DRANKS=${ranks}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/checkHeat.cmake)
    set_tests_properties(heat_fourier32_np${ranks} PROPERTIES
      LABELS "solver;heat;heat3d;regression;mpi" RUN_SERIAL TRUE
      ENVIRONMENT "OMP_NUM_THREADS=1;OMP_PROC_BIND=false")
  endforeach()
endif()
