# addSolver(name)
#
# Adds src/solvers/<name>, creates its static library and records it for
# saturnCFD. A user copies a solver directory and adds exactly one line:
#
#   addSolver(myBurgers)
#
function(addSolver name)
  set(dir "${SATURN_SOURCE_DIR}/src/solvers/${name}")
  if(NOT EXISTS "${dir}/CMakeLists.txt")
    message(FATAL_ERROR
      "addSolver(${name}): ${dir}/CMakeLists.txt does not exist")
  endif()

  add_subdirectory("${dir}" "${CMAKE_BINARY_DIR}/solvers/${name}")

  if(NOT TARGET saturn_solver_${name})
    message(FATAL_ERROR
      "addSolver(${name}): the solver CMakeLists must define "
      "saturn_solver_${name}")
  endif()

  target_link_libraries(saturn_solver_${name} PUBLIC saturn::core)
  set_property(GLOBAL APPEND PROPERTY SATURN_SOLVER_TARGETS
               saturn_solver_${name})
endfunction()
