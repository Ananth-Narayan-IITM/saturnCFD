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

set_tests_properties(
  cli_help cli_version cli_listSolvers cli_dumpConfig
  cli_checkConfig_si cli_checkConfig_cgs cli_checkMesh_box cli_checkMesh_tet
  cli_defaultOutputs cli_writeVTK cli_completion
  PROPERTIES LABELS "cli;regression")
