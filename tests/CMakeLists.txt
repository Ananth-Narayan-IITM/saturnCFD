add_executable(saturn_tests
  TestMain.cpp
  TestUnits.cpp
  TestConfig.cpp
  TestMesh.cpp
  TestRegression.cpp
)
set_target_properties(saturn_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

target_link_libraries(saturn_tests PRIVATE saturn::core doctest::doctest)
target_compile_definitions(saturn_tests PRIVATE
  SATURN_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/meshes"
  SATURN_REGRESSION_DIR="${CMAKE_CURRENT_SOURCE_DIR}/regression"
)

add_test(NAME saturn_tests COMMAND saturn_tests)
set_tests_properties(saturn_tests PROPERTIES
  LABELS "unit;mesh;config;regression")

if(SATURN_ENABLE_MPI)
  add_executable(saturn_mpi_tests
    TestMpiMain.cpp
    TestParallel.cpp
  )
  set_target_properties(saturn_mpi_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_libraries(saturn_mpi_tests PRIVATE
    saturn::core doctest::doctest MPI::MPI_CXX)

  add_test(NAME mpi_np1
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 1
            ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:saturn_mpi_tests>)
  add_test(NAME mpi_np2
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 2
            ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:saturn_mpi_tests>)
  add_test(NAME mpi_np4
    COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
            ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:saturn_mpi_tests>)
  set_tests_properties(mpi_np1 mpi_np2 mpi_np4 PROPERTIES
    LABELS "mpi" RUN_SERIAL TRUE)
endif()

if(SATURN_ENABLE_KOKKOS)
  add_executable(saturn_fv_tests
    TestFvMain.cpp
    TestFv.cpp
  )
  set_target_properties(saturn_fv_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  target_link_libraries(saturn_fv_tests PRIVATE saturn::core doctest::doctest)

  set(SATURN_FV_LABELS "fv;regression;parity")
  if(SATURN_BACKEND STREQUAL "cuda")
    list(APPEND SATURN_FV_LABELS "gpu")
  endif()

  if(SATURN_ENABLE_MPI)
    foreach(ranks 1 2 4)
      add_test(NAME fv_np${ranks}
        COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${ranks}
                ${SATURN_MPI_PREFLAGS} $<TARGET_FILE:saturn_fv_tests>)
      set_tests_properties(fv_np${ranks} PROPERTIES
        LABELS "${SATURN_FV_LABELS};mpi" RUN_SERIAL TRUE)
    endforeach()
  else()
    add_test(NAME fv_serial COMMAND saturn_fv_tests)
    set_tests_properties(fv_serial PROPERTIES LABELS "${SATURN_FV_LABELS}")
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
  PROPERTIES LABELS "cli;regression")
